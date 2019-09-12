#pragma once
#ifndef __CLOUDCLIENT_H__
#define __CLOUDCLIENT_H__

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <boost/algorithm/string.hpp>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include "httplib.h"

#define CLIENT_BACKUP_DIR "backup"
#define CLIENT_BACKUP_INFO_FILE "backup.list"
#define BACKUP_URI "/list/"
#define RANGE_MAX_SIZE  (1<<20)
#define SERVER_IP "49.234.186.244"
#define POST 9000

namespace bf = boost::filesystem;

class ThrBackUp
{
private:
	std::string _file;
	int64_t _rang_start;
	int64_t _rang_len;
public:
	bool _res = true;
public:
	ThrBackUp(const std::string &file,int64_t start,int64_t len)
		:_file(file)
		,_rang_start(start)
		,_rang_len(len)
	{}
	void Start()
	{	
		//��ȡ�ļ��ķֿ�����
		std::ifstream path(_file,std::ios::binary);
		if (!path.is_open())
		{
			std::cout << "�ļ���ʧ��" << std::endl;
			_res = false;
			return;
		}
		//��ת���ļ��ֿ����ʼλ��
		path.seekg(_rang_start);
		std::string body;
		body.resize(_rang_len);
		path.read(&body[0],_rang_len);
		if (!path.good())
		{
			std::cout << "read file : " << _file << "rang data error " << std::endl;
			_res = false;
		}
		path.close();

		//�ϴ��ļ��ֿ�����
		bf::path filename(_file);
		std::cout << "�ϴ��ļ���" << filename ;
		std::string url = BACKUP_URI + filename.filename().string();
		//std::cout << " url : " << url << std::endl;
		httplib::Client cli(SERVER_IP,POST);
		httplib::Headers headers;
		headers.insert(std::make_pair("Content-Length:",std::to_string(_rang_len)));
		std::stringstream tt;
		tt << "bytes=" << _rang_start << "-" << (_rang_start + _rang_len - 1);
		headers.insert(std::make_pair("Ranger", tt.str().c_str()));
		auto rsp = cli.Put(&url[0], headers, body, "text/plain");
		if (rsp->status != 200)
		{
			std::cout << "rsp->status != 200" << std::endl;
			_res = false;
		}
		std::stringstream ss;
		ss << "backup file: [ " << _file << " ] range:[ " << _rang_start << " - " << _rang_len << " ]";
		std::cout << ss.str() << std::endl;
		return;
	}
};

class CloudClient
{
private:
	std::unordered_map<std::string, std::string> _backup_list;
public:
	CloudClient()
	{
		bf::path file(CLIENT_BACKUP_DIR);
		if (!bf::exists(file))
		{
			bf::create_directory(file);
		}
	}
private:
	bool GetBackupInfo()
	{
		//filename etag\n
		bf::path path(CLIENT_BACKUP_INFO_FILE);
		if (!bf::exists(path))
		{
			std::cout << "file: " << path.string() << " is not exists" << std::endl;
			return false;
		}
		int64_t fsize = bf::file_size(path);
		if (fsize == 0)
		{
			std::cout << " have no backup info" << std::endl;
			return false;
		}
		std::string body;
		body.resize(fsize);
		std::ifstream file(CLIENT_BACKUP_INFO_FILE, std::ios::binary);
		if (!file.is_open())
		{
			std::cout << "backup info file open false" << std::endl;
			return false;
		}
		file.read(&body[0],fsize);
		if (!file.good())
		{
			std::cout << "file read false" << std::endl;
			return false;
		}
		file.close();
		std::vector<std::string> list;
		boost::split(list,body,boost::is_any_of("\n"));
		for (auto i : list)
		{
			//filename etag\n
			size_t pos = i.find(" ");
			if (pos == std::string::npos)
			{
				continue;
			}
			std::string key = i.substr(0, pos);
			std::string val = i.substr(pos + 1);
			_backup_list[key] = val;
		}
		return true;
	}
	bool SetBackupInfo()
	{
		std::string body;
		std::cout << "���ñ�����Ϣ" << std::endl;
		for (auto i : _backup_list)
		{
			body += i.first + " " + i.second + "\n";
		}
		std::ofstream file(CLIENT_BACKUP_INFO_FILE,std::ios::binary);
		if (!file.is_open())
		{
			std::cout <<"backupfile open false" << std::endl;
			return false;
		}
		file.write(&body[0],body.size());
		if (!file.good())
		{
			std::cout << "set backupinfo false" << std::endl;
			return false;
		}
		file.close();
		std::cout << "���ñ�����Ϣ����" << std::endl;
		return true;
	}
	bool BackupDirListen(const std::string &path)
	{
		bf::directory_iterator item_begin(CLIENT_BACKUP_DIR);
		bf::directory_iterator item_end;
		for (; item_begin != item_end; item_begin++)
		{
			if (bf::is_directory(item_begin->path().string()))
			{
				BackupDirListen(item_begin->path().string());
				continue;
			}
			if (FileIsNeedBackup(item_begin->path().string()) == false)
			{
				continue;
			}
			if (PutFileData(item_begin->path().string()) == false)
			{
				continue;
			}
			AddBackupInfo(item_begin->path().string());
		}
		return true;
	}
	bool AddBackupInfo(const std::string &file)
	{
		//etag = "mtime-fsize"
		std::string etag;
		if (GetFileEtag(file, etag) == false)
		{
			std::cout << "��ȡetagʧ��" << std::endl;
			return false;
		}
		std::cout << "��ӱ�����Ϣ" << std::endl;
		_backup_list[file] = etag;
		return true;
	}

	bool PutFileData(const std::string &file)
	{
		//���ֿ鴫���С��10M�����ļ����зֿ鴫��
		//ͨ����ȡ�ֿ鴫���Ƿ�ɹ��ж������ļ��Ƿ��ϴ��ɹ�
		//ѡ����̴߳���

		//1����ȡ�ļ���С
		int64_t fsize = bf::file_size(file);
		std::cout << "fsize: " << fsize << std::endl;
		
		//2�������ܹ���Ҫ�ֶ��٣��õ�ÿ��Ĵ�С����ʼλ��
		int count = fsize / RANGE_MAX_SIZE;
		std::vector<ThrBackUp>  thr_res;
		std::vector<std::thread> thr_list;
		for (int i = 0; i <= count; i++)
		{
			int64_t range_start = i * RANGE_MAX_SIZE;
			int64_t range_end = (i + 1) * RANGE_MAX_SIZE - 1;
			if (i == count)
			{
				range_end = fsize - 1;
			}
			int64_t range_len = range_end - range_start + 1;
			ThrBackUp backup_info(file,range_start,range_end);
			thr_res.push_back(backup_info);
			thr_list.push_back(std::thread(thr_start,&thr_res[i]));
			std::cout << "count---i: " << i << std::endl;

		}																				
		//3��ѭ�������̣߳����߳����ϴ��ļ�����
		/*for (int i = 0; i < count; i++)
		{
			thr_list.push_back(std::thread(thr_start,&thr_res[i]));
		}*/
		//4���ȴ������߳��˳����ж��ļ��ϴ����
		bool ret = true;
		for (int i = 0; i <= count; i++)
		{
			thr_list[i].join();
			if (thr_res[i]._res == true)
			{
				continue;
			}
			ret = false;
		}
		//5���ϴ��ɹ���ӱ�����Ϣ
		if (ret == false)
		{
			std::cout << "file: " << file << " backup false" << std::endl;
			return false;
		}
		std::cout << "file: " << file << " backup success" << std::endl;
		return true;
	}

	static void thr_start(ThrBackUp *backup_info)
	{
		backup_info->Start();
		std::cout << "in thr_start" << std::endl;
		return;
	}
	bool GetFileEtag(const std::string &file, std::string &etag)
	{
		bf::path path(file);
		if (!bf::exists(path))
		{
			std::cout << "get file:" << file << "etag false"<< std::endl;
			return false;
		}
		int64_t fsize = bf::file_size(path);
		int64_t mtime = bf::last_write_time(path);
		std::stringstream tmp;
		tmp << std::hex << mtime << "-" << std::hex << fsize;
		etag = tmp.str();
		return true;
	}
	bool FileIsNeedBackup(const std::string &file)
	{
		std::string etag;
		if (GetFileEtag(file, etag) == false)
		{
			return false;
		}
		auto it = _backup_list.find(file);
		if (it != _backup_list.end() && it->second == etag)
		{
			return false;
		}
		return true;
	}
public:
	bool Start()
	{
		//��ȡ�ļ�������Ϣ
		GetBackupInfo();
		while (1)
		{
			BackupDirListen(CLIENT_BACKUP_DIR);
			SetBackupInfo();
			std::cout << std::endl;
			Sleep(3000);
		}
		return true;
	}
};

#endif // !__CLOUDCLIENT_H__



