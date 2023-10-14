#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>
#include <functional>
#include <condition_variable>
#include "Connection.h"
#include "public.h"

using namespace std;
/*
 * 实现连接池功能模块
 *
 */

class ConnectionPool
{
public:
	static ConnectionPool* getConnectionPool();  // 获取连接池对象实例
	shared_ptr<Connection> getConnection(); // 给外部提供接口，从连接池中获取一个可用的空闲连接
private:
	ConnectionPool(); // 单例模式，构造函数私有化

	bool loadConfigFile(); // 从配置文件中加载配置项

	void produceConnectionTask(); // 运行在独立的线程中，专门负责生产新连接

	void scannerConnectionTask(); // 扫描超过 maxIdleTime 时间的空闲存活时间的线程 回收这些线程 

	std::string _ip; // mysql的ip地址
	unsigned short _port; // mysql的端口号 默认：3306
	string _username; // mysql的用户名
	string _password; // mysql的登录密码
	string _dbname; // 要写入的表
	int _initSize; // 连接池的初始连接量
	int _maxSize; // 连接池的最大连接量
	int _maxIdleTime; // 连接池最大空闲时间
	int _connectionTimeout; // 连接池获取连接的超时时间

	queue<Connection*> _connectionQue; // 存储mysql连接的队列
	mutex _queueMutex; // 维护连接队列的线程安全
	atomic_int _connectionCnt; //记录连接所创建的connection连接的总数量
	condition_variable cv; // 设置条件变量，用于连接生产线程和连接消费线程的通信

};