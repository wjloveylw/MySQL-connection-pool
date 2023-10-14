#include <iostream>
using namespace std;
#include "Connection.h"
#include "CommonConnectionPool.h"
#include "public.h"
#include "threadpool.h"
#include <functional>
#include <future>

ConnectionPool* pool = ConnectionPool::getConnectionPool();

int submit() {

	shared_ptr<Connection> sp = pool->getConnection();
	char sql[1024] = { 0 };
	sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
	// �������ݿ�������ڴ�������ʱ�����
	sp->update(sql);
	return 0;
};

int main()
{
	
	clock_t begin = clock();
	ThreadPool thread_pool;
	thread_pool.setTaskQueThreshHold(1024);
	thread_pool.setThreadSizeThreshHold(100);
	thread_pool.setMode(PoolMode::Mode_CACHED);
	thread_pool.start(4);

	future<int> res;

	for(int i = 0; i < 10000; ++i)
	{
		res = thread_pool.submitTask(submit);
	}
	res.get();

	clock_t end = clock();

	std::cout << "used time:" << end - begin << "ms" << std::endl;

	/*Connection conn;
	char sql[1024] = { 0 };
	sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
	conn.connect("127.0.0.1", 3306, "root", "password(Ҫ���)", "chat");
	conn.update(sql);*/

	/*clock_t begin = clock();

	Connection conn;
	conn.connect("127.0.0.1", 3306, "root", "wang1999@jie", "chat");

	thread t1([]() {for (int i = 0; i < 2500; ++i)
	{
		Connection conn;
		char sql[1024] = { 0 };
		sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
		conn.connect("127.0.0.1", 3306, "root", "wang1999@jie", "chat");
		conn.update(sql);
	}});
	thread t2([]() {for (int i = 0; i < 2500; ++i)
	{
		Connection conn;
		char sql[1024] = { 0 };
		sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
		conn.connect("127.0.0.1", 3306, "root", "wang1999@jie", "chat");
		conn.update(sql);
	}});
	thread t3([]() {for (int i = 0; i < 2500; ++i)
	{
		Connection conn;
		char sql[1024] = { 0 };
		sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
		conn.connect("127.0.0.1", 3306, "root", "wang1999@jie", "chat");
		conn.update(sql);
	}});
	thread t4([]() {for (int i = 0; i < 2500; ++i)
	{
		Connection conn;
		char sql[1024] = { 0 };
		sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
		conn.connect("127.0.0.1", 3306, "root", "wang1999@jie", "chat");
		conn.update(sql);
	}});

	t1.join();
	t2.join();
	t3.join();
	t4.join();

	clock_t end = clock();

	cout << "used time:" << end - begin << "ms" << endl;*/

	//clock_t begin = clock();

	//thread t1([]() {ConnectionPool* pool = ConnectionPool::getConnectionPool();

	//for (int i = 0; i < 2500; i++)
	//{
	//	shared_ptr<Connection> sp = pool->getConnection();
	//	char sql[1024] = { 0 };
	//	sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
	//	// �������ݿ�������ڴ�������ʱ�����
	//	sp->update(sql);
	//}});
	//thread t2([]() {ConnectionPool* pool = ConnectionPool::getConnectionPool();

	//for (int i = 0; i < 2500; i++)
	//{
	//	shared_ptr<Connection> sp = pool->getConnection();
	//	char sql[1024] = { 0 };
	//	sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
	//	// �������ݿ�������ڴ�������ʱ�����
	//	sp->update(sql);
	//}});
	//thread t3([]() {ConnectionPool* pool = ConnectionPool::getConnectionPool();

	//for (int i = 0; i < 2500; i++)
	//{
	//	shared_ptr<Connection> sp = pool->getConnection();
	//	char sql[1024] = { 0 };
	//	sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
	//	// �������ݿ�������ڴ�������ʱ�����
	//	sp->update(sql);
	//}});
	//thread t4([]() {ConnectionPool* pool = ConnectionPool::getConnectionPool();

	//for (int i = 0; i < 2500; i++)
	//{
	//	shared_ptr<Connection> sp = pool->getConnection();
	//	char sql[1024] = { 0 };
	//	sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
	//	// �������ݿ�������ڴ�������ʱ�����
	//	sp->update(sql);
	//}});

	//t1.join();
	//t2.join();
	//t3.join();
	//t4.join();

	//clock_t end = clock();

	//cout << "used time:" << end - begin << "ms" << endl;

#if 0

	// ����ֱ���������ݿ�
	/*for(int i = 0; i < 5000; ++i)
	{
		Connection conn;
		char sql[1024] = { 0 };
		sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
		conn.connect("127.0.0.1", 3306, "root", "wang1999@jie", "chat");
		conn.update(sql);
	}*/

	// ����ʹ�����ӳ��������ݿ�
	ConnectionPool* pool = ConnectionPool::getConnectionPool();

	for (int i = 0; i < 10000; i++)
	{
		shared_ptr<Connection> sp = pool->getConnection();
		char sql[1024] = { 0 };
		sprintf_s(sql, "insert into user(name,age,sex) values('%s',%d,'%s')", "zhang san", 20, "male");
		// �������ݿ�������ڴ�������ʱ�����
		sp->update(sql);
	}

#endif


	/*ConnectionPool *pool  = ConnectionPool::getConnectionPool();
	pool->loadConfigFile();*/

	return 0;
}