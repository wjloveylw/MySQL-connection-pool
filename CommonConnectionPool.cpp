#pragma once
#include "CommonConnectionPool.h"

/*
 * ʵ�����ӳع���ģ��
 */

// ���ӳصĹ��캯��
ConnectionPool::ConnectionPool()
{
	// ����������
	if (!loadConfigFile())
	{
		return;
	}

	// ������ʼ����������
	for(int i=0; i < _initSize; ++i)
	{
		Connection* p = new Connection();
		p->connect(_ip, _port, _username, _password, _dbname);
		p->refreshAliveTime(); // ˢ��һ�¿�ʼ���е���ʼʱ��
		_connectionQue.push(p);
		_connectionCnt++;
	}

	// ����һ���µ��̣߳���Ϊ���ӵ�������
	thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
	produce.detach();

	// ����һ���µ��߳� ɨ�賬�� maxIdleTime ʱ��Ŀ��д��ʱ����߳� ������Щ�߳�
	thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
	scanner.detach();
}


// �̰߳�ȫ����������ģʽ
ConnectionPool* ConnectionPool::getConnectionPool()
{
	static ConnectionPool pool;
	return &pool;
}

// ��ȡ����
shared_ptr<Connection> ConnectionPool::getConnection()
{
	unique_lock<mutex> lock(_queueMutex);
	while(_connectionQue.empty())
	{
		// �����ڵȴ���ʱ���ڲ��ϲ�ѯ�Ƿ��п��õ�����  sleep ����ֱ������ָ��ʱ��
		if(cv_status::timeout == cv.wait_for(lock, chrono::milliseconds(_connectionTimeout)))
		{
			if (_connectionQue.empty())
			{
				LOG("��ȡ�������ӳ�ʱ��...��ȡ����ʧ�ܣ�");
				return nullptr;
			}
		}
	}
	/*
	 * shared_ptr ����ָ������ʱ����� connection ��Դֱ�� delete �����൱�ڵ��� connection ������������ connection �ͱ� close ����
	 *
	 * ������Ҫ�Զ��� shared_ptr���ͷ���Դ�ķ�ʽ�� �� connection ֱ�ӹ黹�� queue ����
	 */
	shared_ptr<Connection> sp(_connectionQue.front(), [&](Connection *pcon)
	{
		// �������ڷ�����Ӧ���߳��е��õģ�����һ��Ҫ���Ƕ��е��̰߳�ȫ����
		unique_lock<mutex> lock(_queueMutex);
		pcon->refreshAliveTime(); // ˢ��һ�¿�ʼ���е���ʼʱ��
		_connectionQue.push(pcon);
	});
	_connectionQue.pop();
	cv.notify_all(); // ֪ͨ�����߳�����  ֪ͨ�������������ӣ������߻������ж��Ƿ�����������
	
	
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
		if(idx == -1)// ��Ч��������
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
			cv.wait(lock); // ���в��գ��˴������߳̽���ȴ�״̬  ��ʱ�Զ�����  ����ѭ�����Զ�����
		}

		// ��������û�е������ߣ����������µ�����
		if (_connectionCnt < _maxSize)
		{
			Connection* p = new Connection();
			p->connect(_ip, _port, _username, _password, _dbname);
			p->refreshAliveTime(); // ˢ��һ�¿�ʼ���е���ʼʱ��
			_connectionQue.push(p);
			_connectionCnt++;
		}

		// ֪ͨ�������߳� , ��������������    �������еȴ������߳� 
		cv.notify_all();
	}  // ������һ�� for ѭ�� ����
}

void ConnectionPool::scannerConnectionTask()
{
	for(;;)
	{
		// ͨ�� sleep ģ�ⶨʱЧ��
		this_thread::sleep_for(chrono::seconds(_maxIdleTime));

		// ɨ���������У� �ͷŶ��������
		unique_lock<mutex> lock(_queueMutex);
		while (_connectionCnt > _initSize)
		{
			Connection* p = _connectionQue.front();
			if(p->getAliveTime() > (_maxIdleTime*1000))
			{
				_connectionQue.pop();
				_connectionCnt--;
				delete p; // �����~Connection() �ͷ�����
			}
			else
			{
				break; // ��ͷ������û�г���_maxIdleTime, �������ӿ϶�û��
			}
		}
	}
}
