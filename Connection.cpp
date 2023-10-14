#pragma once
#include <mysql.h>
#include "Connection.h"
#include "public.h"

/*
 * ʵ��MySQL���ݿ�Ĳ���
 */

#include <mysql.h>
#include <string>
using namespace std;
#include "public.h"
// ���ݿ������

Connection::Connection()
{
	_conn = mysql_init(nullptr);
}

// �ͷ����ݿ�������Դ
Connection::~Connection()
{
	if (_conn != nullptr)
		mysql_close(_conn);
}

// �������ݿ�
bool Connection::connect(string ip, unsigned short port, string user, string password,
	string dbname)
{
	MYSQL* p = mysql_real_connect(_conn, ip.c_str(), user.c_str(),
		password.c_str(), dbname.c_str(), port, nullptr, 0);
	return p != nullptr;
}

// ���²��� insert��delete��update
bool Connection::update(string sql)
{
	if (mysql_query(_conn, sql.c_str()))
	{
		LOG("����ʧ��:" + sql);
		return false;
	}
	return true;
}

// ��ѯ���� select
MYSQL_RES* Connection::query(string sql)  // ����Ҫ�ǲ��������ƣ��������� ���� ɾ�� �� ָ��Ҳִ��
{
	if (mysql_query(_conn, sql.c_str()))
	{
		LOG("��ѯʧ��:" + sql);
		return nullptr;
	}
	return mysql_use_result(_conn);
}
