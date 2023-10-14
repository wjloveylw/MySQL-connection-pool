#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>
#include <future>
#include <thread>
#include <iostream>
#include <chrono>

const int TaskMaxThreadHold = 1024;
const int ThreadSizeThreshHold_ = 100;
const int Thread_MAX_IDLE_TIME = 3;

//线程池模式
enum class PoolMode {
	Mode_FIXED,  //固定线程数量
	Mode_CACHED,  //线程数量可动态增长
};

//线程类型
class Thread {
public:
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func):func_(func),
		threadId(generateId_++) {
			
	}
	~Thread() {
		//std::cout << "thread" << std::this_thread::get_id() << "exit" << std::endl;
	}//线程析构
	//线程启动函数
	void start() {
		//创建一个线程  执行线程函数
		std::thread t1(func_, threadId);//cpp11 中thread_对象的生命周期在start()函数中

		//线程分离
		t1.detach();
	}

	//获取线程id
	int getId() const {
		return threadId;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId; //保存线程id
};

int Thread::generateId_ = 0;

/*
*
	example:
	//task对象
	class Task1:public Task {
	public:
		virtual void run() {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::cout << std::this_thread::get_id() << std::endl;
		}
	};

	ThreadPool tpool;
	tpool.start(4);

	tpool.submitTask(std::shared_ptr<Task>(new Task1));
	tpool.submitTask(std::shared_ptr<Task>(new Task2(10,20)));

*/
//线程池类型
class ThreadPool {
public:
	ThreadPool(): initThreadSize_(0),
		taskSize_(0),
		taskSizeThreshHold_(TaskMaxThreadHold),
		threadSizeThreshHold_(ThreadSizeThreshHold_),
		poolMode_(PoolMode::Mode_FIXED),
		isStart(false),
		curThreadNum_(0),
		freeThread(0) {
		
	}
	~ThreadPool() {
		isStart = false;
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		// 等待所有线程回收
		notEmpty_.notify_all();
		exit_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	}

	//设置线程池的工作模式
	void setMode(PoolMode mode) {
		if (checkRunning())return;
		poolMode_ = mode;
	}

	//开启线程池
	void start(int size) {
		isStart = true;
		//记录线程初始数量
		initThreadSize_ = size;
		curThreadNum_ = size;
		//创建线程对象
		for (size_t i = 0; i < initThreadSize_; i++) {
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int thread_id = ptr->getId();
			threads_.emplace(thread_id, std::move(ptr));
			freeThread++;
		}
		//启动线程
		for (size_t i = 0; i < initThreadSize_; i++) {
			//std::cout << "线程启动" << std::endl;
			threads_[i]->start();
			//等待线程池里面所有的线程返回  阻塞//正在执行任务
		}
	}

	//线程池阈值设置
	void setTaskQueThreshHold(int threadhold)
	{
		taskSizeThreshHold_ = threadhold;
	}
	void setThreadSizeThreshHold(int threadhold) {
		if (checkRunning())return;
		if (poolMode_ == PoolMode::Mode_FIXED)
		{
			threadSizeThreshHold_ = threadhold;
		}
	}


	//可变参模板编程实现
	template<typename Func,typename... Args>	//返回值的自动推导
	auto submitTask(Func&& func, Args&&... args)->std::future<decltype(func(args...))> {
		//推导出返回值的类型
		using RType = decltype(func(args...));

		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

		std::future<RType> result = task->get_future();
		//获取锁
		std::unique_lock<std::mutex> loc(taskQueMtx_);
		if (!(notFull_.wait_for(loc, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < (size_t)taskSizeThreshHold_; })))
		{
			//等待一秒，任务队列依然是满的
			std::cerr << "task queue is full, submit task fail" << std::endl;
			//任务提交失败返回对应类型的0值
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); });
			(*task)();
			return task->get_future(); //return 的是一个Result对象
		}
		//如果有空余，把任务放到任务队列中
		taskQue_.emplace([task]() {(*task)();});
		//std::cout << taskSize_ << std::endl;
		taskSize_++;
		//任务不空则在notEmptyq去进行通知
		notEmpty_.notify_all();

		//根据任务数量和空闲线程数量，判断是否需要创建新的线程
		if (poolMode_ == PoolMode::Mode_CACHED
			&& taskSize_ > freeThread
			&& curThreadNum_ < threadSizeThreshHold_)
		{
			//创建新线程
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadId = ptr->getId();
			threads_.emplace(threadId, std::move(ptr));
			threads_[threadId]->start();
			curThreadNum_++;
			freeThread++;
			//std::cout << "create new thread" << std::endl;
		}

		return result;
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;



private:
	//线程函数 从队列中消费任务
	void threadFunc(int threadId) {
		auto lastTime = std::chrono::high_resolution_clock().now();
		// 任务必须执行完成，线程池才可以回收所有资源
		for (;;) {
			Task t;
			{//获取锁，访问任务队列
				std::unique_lock<std::mutex> loc(taskQueMtx_);
				//std::cout << std::this_thread::get_id() << "尝试获取任务" << std::endl;
				//如果没有任务来，则等待.Cache多余线程的回收
				//当前执行时间-上一次线程执行时间超过60秒对线程进行回收
				while (taskQue_.size() == 0) {
					//每一秒中返回一次，如何区分超时返回？还是有任务待执行返回
					if (!isStart) {
						//通过线程id==》线程对象==》删除线程
						threads_.erase(threadId);
						exit_.notify_all();
						return;//线程函数结束，线程结束
					}
					if (poolMode_ == PoolMode::Mode_CACHED) {
						if (std::cv_status::timeout ==
							notEmpty_.wait_for(loc, std::chrono::seconds(1))) {

							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= Thread_MAX_IDLE_TIME && curThreadNum_ > initThreadSize_) {
								//回收线程
								//记录数量的值修改
								curThreadNum_--;
								freeThread--;
								//通过线程id==》线程对象==》删除线程
								threads_.erase(threadId);

								//std::cout << "delete thread" << std::endl;

								return;
							}
						}
					}
					else {
						notEmpty_.wait(loc);
					}
				}

				freeThread--;
				//取任务
				t = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				//std::cout << std::this_thread::get_id() << "获取任务成功" << std::endl;
				//若任务队列还有任务，继续通知其余线程执行
				if (taskSize_ > 0) {
					notEmpty_.notify_all();
				}
				//任务取出
				notFull_.notify_all();
			}//task->run 时不需要获取锁
			if (t != nullptr) {
				//std::cout << "ready_to run" << std::endl;
				//执行任务
				t();

			}
			//线程执行完的时间
			lastTime = std::chrono::high_resolution_clock().now();
			freeThread++;
		}
	}

	//检查线程池的运行状态
	bool checkRunning()const {
		return isStart;
	}
private:
	using Task = std::function<void()>;
	//线程列表
	//std::vector<std::unique_ptr<Thread>> threads_;
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;

	//初始线程个数
	size_t initThreadSize_;
	//记录线程池中总线程数量
	std::atomic_int curThreadNum_;
	//记录空闲线程的数量
	std::atomic_int freeThread;
	//初始化任务队列
	std::queue<Task> taskQue_;
	//任务个数
	std::atomic_int taskSize_;
	//任务数量阈值,上限
	int taskSizeThreshHold_;
	//线程数量上限
	int threadSizeThreshHold_;

	std::mutex taskQueMtx_;//保证任务队列线程安全
	std::condition_variable notFull_;//保证任务队列不满
	std::condition_variable notEmpty_;//保证任务队列不空
	std::condition_variable exit_;


	PoolMode poolMode_;//工作模式

	std::atomic_bool isStart;//表示当前线程池的启动状态


};

#endif

