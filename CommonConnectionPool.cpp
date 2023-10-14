#pragma once
#include "CommonConnectionPool.h"

/*
 * 实现连接池功能模块
 */

// 连接池的构造函数
ConnectionPool::ConnectionPool()
{
	// 加载配置项
	if (!loadConfigFile())
	{
		return;
	}

	// 创建初始数量的连接
	for(int i=0; i < _initSize; ++i)
	{
		Connection* p = new Connection();
		p->connect(_ip, _port, _username, _password, _dbname);
		p->refreshAliveTime(); // 刷新一下开始空闲的起始时间
		_connectionQue.push(p);
		_connectionCnt++;
	}

	// 启动一个新的线程，作为连接的生产者
	thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
	produce.detach();

	// 启动一个新的线程 扫描超过 maxIdleTime 时间的空闲存活时间的线程 回收这些线程
	thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
	scanner.detach();
}


// 线程安全的懒汉单例模式
ConnectionPool* ConnectionPool::getConnectionPool()
{
	static ConnectionPool pool;
	return &pool;
}

// 获取连接
shared_ptr<Connection> ConnectionPool::getConnection()
{
	unique_lock<mutex> lock(_queueMutex);
	while(_connectionQue.empty())
	{
		// 可以在等待的时间内不断查询是否有可用的连接  sleep 则是直接休眠指定时间
		if(cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeout)))
		{
			if (_connectionQue.empty())
			{
				LOG("获取空闲连接超时了...获取连接失败！");
				return nullptr;
			}
		}
	}
	/*
	 * shared_ptr 智能指针析构时，会把 connection 资源直接 delete 掉，相当于调用 connection 的析构函数， connection 就被 close 掉了
	 *
	 * 这里需要自定义 shared_ptr的释放资源的方式， 把 connection 直接归还到 queue 当中
	 */
	shared_ptr<Connection> sp(_connectionQue.front(), [&](Connection *pcon)
	{
		// 这里是在服务器应用线程中调用的，所以一定要考虑队列的线程安全操作
		unique_lock<mutex> lock(_queueMutex);
		pcon->refreshAliveTime(); // 刷新一下开始空闲的起始时间
		_connectionQue.push(pcon);
	});
	_connectionQue.pop();
	cv.notify_all(); // 通知其它线程消费  通知生产者生产连接（生产者会自行判断是否满足条件）
	
	
	return sp;
}

bool ConnectionPool::loadConfigFile()
{
	FILE* pf;
	errno_t err;
	err = fopen_s(&pf, "mysql.ini", "r");

	if(pf==nullptr)
	{
		LOG("mysql.ini file is not exist!");
		return false;
	}
	while (!feof(pf))
	{
		char line[1024] = { 0 };
		fgets(line, 1024, pf);
		string str = line;
		int idx = str.find('=', 0);
		if(idx == -1)// 无效的配置项
		{
			continue;
		}
		//username=root\n
		int endidx = str.find('\n', idx);
		string key = str.substr(0, idx);
		string value = str.substr(idx + 1, endidx - idx - 1);

		if (key == "ip")
		{
			this->_ip = value;
		}
		else if (key == "port")
		{
			this->_port = atoi(value.c_str());
		}
		else if (key == "username")
		{
			this->_username = value;
		}
		else if (key == "password")
		{
			this->_password = value;
		}
		else if (key == "dbname")
		{
			this->_dbname = value;
		}
		else if (key == "initSize")
		{
			this->_initSize = atoi(value.c_str());
		}
		else if (key == "maxSize")
		{
			this->_maxSize = atoi(value.c_str());
		}
		else if (key == "maxIdleTime")
		{
			this->_maxIdleTime = atoi(value.c_str());
		}
		else if (key == "connectionTimeout")
		{
			this->_connectionTimeout = atoi(value.c_str());
		}
	}
	
	return true;
	
}

void ConnectionPool::produceConnectionTask()
{
	for(;;)
	{
		unique_lock<mutex> lock(_queueMutex);
		while(!_connectionQue.empty())
		{
			cv.wait(lock); // 队列不空，此处生产线程进入等待状态  此时自动解锁  跳出循环后自动上锁
		}

		// 连接数量没有到达上线，继续创建新的连接
		if (_connectionCnt < _maxSize)
		{
			Connection* p = new Connection();
			p->connect(_ip, _port, _username, _password, _dbname);
			p->refreshAliveTime(); // 刷新一下开始空闲的起始时间
			_connectionQue.push(p);
			_connectionCnt++;
		}

		// 通知消费者线程 , 可以消费连接了    唤醒所有等待锁的线程 
		cv.notify_all();
	}  // 进入下一次 for 循环 解锁
}

void ConnectionPool::scannerConnectionTask()
{
	for(;;)
	{
		// 通过 sleep 模拟定时效果
		this_thread::sleep_for(chrono::seconds(_maxIdleTime));

		// 扫描整个队列， 释放多余的连接
		unique_lock<mutex> lock(_queueMutex);
		while (_connectionCnt > _initSize)
		{
			Connection* p = _connectionQue.front();
			if(p->getAliveTime() > (_maxIdleTime*1000))
			{
				_connectionQue.pop();
				_connectionCnt--;
				delete p; // 会调用~Connection() 释放连接
			}
			else
			{
				break; // 队头的连接没有超过_maxIdleTime, 其它连接肯定没有
			}
		}
	}
}
