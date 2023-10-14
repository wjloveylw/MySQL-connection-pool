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

//�̳߳�ģʽ
enum class PoolMode {
	Mode_FIXED,  //�̶��߳�����
	Mode_CACHED,  //�߳������ɶ�̬����
};

//�߳�����
class Thread {
public:
	using ThreadFunc = std::function<void(int)>;
	Thread(ThreadFunc func):func_(func),
		threadId(generateId_++) {
			
	}
	~Thread() {
		//std::cout << "thread" << std::this_thread::get_id() << "exit" << std::endl;
	}//�߳�����
	//�߳���������
	void start() {
		//����һ���߳�  ִ���̺߳���
		std::thread t1(func_, threadId);//cpp11 ��thread_���������������start()������

		//�̷߳���
		t1.detach();
	}

	//��ȡ�߳�id
	int getId() const {
		return threadId;
	}
private:
	ThreadFunc func_;
	static int generateId_;
	int threadId; //�����߳�id
};

int Thread::generateId_ = 0;

/*
*
	example:
	//task����
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
//�̳߳�����
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
		// �ȴ������̻߳���
		notEmpty_.notify_all();
		exit_.wait(lock, [&]()->bool {return threads_.size() == 0; });
	}

	//�����̳߳صĹ���ģʽ
	void setMode(PoolMode mode) {
		if (checkRunning())return;
		poolMode_ = mode;
	}

	//�����̳߳�
	void start(int size) {
		isStart = true;
		//��¼�̳߳�ʼ����
		initThreadSize_ = size;
		curThreadNum_ = size;
		//�����̶߳���
		for (size_t i = 0; i < initThreadSize_; i++) {
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int thread_id = ptr->getId();
			threads_.emplace(thread_id, std::move(ptr));
			freeThread++;
		}
		//�����߳�
		for (size_t i = 0; i < initThreadSize_; i++) {
			//std::cout << "�߳�����" << std::endl;
			threads_[i]->start();
			//�ȴ��̳߳��������е��̷߳���  ����//����ִ������
		}
	}

	//�̳߳���ֵ����
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


	//�ɱ��ģ����ʵ��
	template<typename Func,typename... Args>	//����ֵ���Զ��Ƶ�
	auto submitTask(Func&& func, Args&&... args)->std::future<decltype(func(args...))> {
		//�Ƶ�������ֵ������
		using RType = decltype(func(args...));

		auto task = std::make_shared<std::packaged_task<RType()>>(
			std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

		std::future<RType> result = task->get_future();
		//��ȡ��
		std::unique_lock<std::mutex> loc(taskQueMtx_);
		if (!(notFull_.wait_for(loc, std::chrono::seconds(1),
			[&]()->bool {return taskQue_.size() < (size_t)taskSizeThreshHold_; })))
		{
			//�ȴ�һ�룬���������Ȼ������
			std::cerr << "task queue is full, submit task fail" << std::endl;
			//�����ύʧ�ܷ��ض�Ӧ���͵�0ֵ
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType {return RType(); });
			(*task)();
			return task->get_future(); //return ����һ��Result����
		}
		//����п��࣬������ŵ����������
		taskQue_.emplace([task]() {(*task)();});
		//std::cout << taskSize_ << std::endl;
		taskSize_++;
		//���񲻿�����notEmptyqȥ����֪ͨ
		notEmpty_.notify_all();

		//�������������Ϳ����߳��������ж��Ƿ���Ҫ�����µ��߳�
		if (poolMode_ == PoolMode::Mode_CACHED
			&& taskSize_ > freeThread
			&& curThreadNum_ < threadSizeThreshHold_)
		{
			//�������߳�
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
	//�̺߳��� �Ӷ�������������
	void threadFunc(int threadId) {
		auto lastTime = std::chrono::high_resolution_clock().now();
		// �������ִ����ɣ��̳߳زſ��Ի���������Դ
		for (;;) {
			Task t;
			{//��ȡ���������������
				std::unique_lock<std::mutex> loc(taskQueMtx_);
				//std::cout << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;
				//���û������������ȴ�.Cache�����̵߳Ļ���
				//��ǰִ��ʱ��-��һ���߳�ִ��ʱ�䳬��60����߳̽��л���
				while (taskQue_.size() == 0) {
					//ÿһ���з���һ�Σ�������ֳ�ʱ���أ������������ִ�з���
					if (!isStart) {
						//ͨ���߳�id==���̶߳���==��ɾ���߳�
						threads_.erase(threadId);
						exit_.notify_all();
						return;//�̺߳����������߳̽���
					}
					if (poolMode_ == PoolMode::Mode_CACHED) {
						if (std::cv_status::timeout ==
							notEmpty_.wait_for(loc, std::chrono::seconds(1))) {

							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() >= Thread_MAX_IDLE_TIME && curThreadNum_ > initThreadSize_) {
								//�����߳�
								//��¼������ֵ�޸�
								curThreadNum_--;
								freeThread--;
								//ͨ���߳�id==���̶߳���==��ɾ���߳�
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
				//ȡ����
				t = taskQue_.front();
				taskQue_.pop();
				taskSize_--;

				//std::cout << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;
				//��������л������񣬼���֪ͨ�����߳�ִ��
				if (taskSize_ > 0) {
					notEmpty_.notify_all();
				}
				//����ȡ��
				notFull_.notify_all();
			}//task->run ʱ����Ҫ��ȡ��
			if (t != nullptr) {
				//std::cout << "ready_to run" << std::endl;
				//ִ������
				t();

			}
			//�߳�ִ�����ʱ��
			lastTime = std::chrono::high_resolution_clock().now();
			freeThread++;
		}
	}

	//����̳߳ص�����״̬
	bool checkRunning()const {
		return isStart;
	}
private:
	using Task = std::function<void()>;
	//�߳��б�
	//std::vector<std::unique_ptr<Thread>> threads_;
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;

	//��ʼ�̸߳���
	size_t initThreadSize_;
	//��¼�̳߳������߳�����
	std::atomic_int curThreadNum_;
	//��¼�����̵߳�����
	std::atomic_int freeThread;
	//��ʼ���������
	std::queue<Task> taskQue_;
	//�������
	std::atomic_int taskSize_;
	//����������ֵ,����
	int taskSizeThreshHold_;
	//�߳���������
	int threadSizeThreshHold_;

	std::mutex taskQueMtx_;//��֤��������̰߳�ȫ
	std::condition_variable notFull_;//��֤������в���
	std::condition_variable notEmpty_;//��֤������в���
	std::condition_variable exit_;


	PoolMode poolMode_;//����ģʽ

	std::atomic_bool isStart;//��ʾ��ǰ�̳߳ص�����״̬


};

#endif

