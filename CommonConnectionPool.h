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
 * ʵ�����ӳع���ģ��
 *
 */

class ConnectionPool
{
public:
	static ConnectionPool* getConnectionPool();  // ��ȡ���ӳض���ʵ��
	shared_ptr<Connection> getConnection(); // ���ⲿ�ṩ�ӿڣ������ӳ��л�ȡһ�����õĿ�������
private:
	ConnectionPool(); // ����ģʽ�����캯��˽�л�

	bool loadConfigFile(); // �������ļ��м���������

	void produceConnectionTask(); // �����ڶ������߳��У�ר�Ÿ�������������

	void scannerConnectionTask(); // ɨ�賬�� maxIdleTime ʱ��Ŀ��д��ʱ����߳� ������Щ�߳� 

	std::string _ip; // mysql��ip��ַ
	unsigned short _port; // mysql�Ķ˿ں� Ĭ�ϣ�3306
	string _username; // mysql���û���
	string _password; // mysql�ĵ�¼����
	string _dbname; // Ҫд��ı�
	int _initSize; // ���ӳصĳ�ʼ������
	int _maxSize; // ���ӳص����������
	int _maxIdleTime; // ���ӳ�������ʱ��
	int _connectionTimeout; // ���ӳػ�ȡ���ӵĳ�ʱʱ��

	queue<Connection*> _connectionQue; // �洢mysql���ӵĶ���
	mutex _queueMutex; // ά�����Ӷ��е��̰߳�ȫ
	atomic_int _connectionCnt; //��¼������������connection���ӵ�������
	condition_variable cv; // ���������������������������̺߳����������̵߳�ͨ��

};