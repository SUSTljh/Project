#include "httplib.h"
#include <boost/filesystem.hpp>
#include <iostream>

using namespace httplib;
namespace bf = boost::filesystem;

#define SERVER_BASE_DIR "www"
#define SERVER_PORT 9000
#define SERVER_BACKUP_DIR SERVER_BASE_DIR"/list"
#define SERVER_IP "0.0.0.0"

class CouldServer
{
  private:
    Server ser;
  public:
    CouldServer()
    {
      bf::path base_path(SERVER_BASE_DIR);
      if(!bf::exists(base_path))
      {
        bf::create_directory(base_path);
      }

      bf::path backup_path(SERVER_BACKUP_DIR);
      if(!bf::exists(backup_path))
      {
        bf::create_directory(backup_path);
      }
    }

    bool Start()
    {
      ser.set_base_dir(SERVER_BASE_DIR);
      ser.Get("/(list)?",GetFileList);
      ser.Get("/(.*)?",FileDownload);
      ser.Put("/(.*)?",PutFileBackup);
      ser.listen(SERVER_IP,SERVER_PORT);
      return true;
    }

  private:
    //获取文件列表
    static void GetFileList(const Request &rep,Response &rsp)
    {
      bf::path list_path(SERVER_BACKUP_DIR); 
      //用户迭代器访问文件
      bf::directory_iterator file_begin(list_path);
      bf::directory_iterator file_end;

      std::string body;
      std::fstream fs1("/home/ljh/Project/web1");
      std::fstream fs2("/home/ljh/Project/web2"); 
      std::stringstream w1,w2;
      w1 << fs1.rdbuf();
      w2 << fs2.rdbuf();
      std::string s1 = w1.str();
      std::string s2 = w2.str();
      //body += "<html><center>";
      body += s1;
      for(; file_begin != file_end; ++file_begin)
      {
        if(bf::is_directory(file_begin->status()))
        {
          continue;
        }

        std::string file = file_begin->path().filename().string();
        std::string uri = "/list/"+ file;
        std::cerr << "uri:" <<uri <<std::endl;
        body += "<h2><li><a href='";
        body += uri;
        body += "'>";
        body += file;
        body += "</a></li></h2>";
      }
      //body += "<center></html>";
      body += s2;
      rsp.set_content(&body[0],"text/html");
      return;
    }
    //文件下载
    static void FileDownload(const Request &rep,Response &rsp)
    {
      std::string file = SERVER_BASE_DIR + rep.path;
      std::cout<< "file:" << file <<std::endl;
      if(!bf::exists(file))
      {
        rsp.status = 404;
        return;
      }
      std::ifstream infile(file,std::ios::binary);
      std::string body;
      int64_t fsize = bf::file_size(file);
      body.resize(fsize);
      if(!infile.read(&body[0],fsize) )
      {
        std::cout << "read file error "<< std::endl;
      }

      rsp.set_content(body,"application/octet-stream");
      return;
    }

    static void PutFileBackup(const Request &req, Response &rsp)
    {
      if(!req.has_header("Ranger"))
      {
        std::cout << "have no Ranger" << std::endl;
        rsp.status = 400;
        return;
      }
      std::string range = req.get_header_value("Ranger");
      int64_t range_start;
      
      if(!RangeParse(range,range_start))
      {
        std::cout << "RangeParse false" << std::endl;
        rsp.status = 400;
        return;
      }
      std::string realpath = SERVER_BASE_DIR + req.path;
      std::ofstream file(realpath,std::ios::binary);
      std::cout << "file: " << realpath << std::endl;
      if(!file.is_open())
      {
        std::cout << "file:" << realpath << "open false" << std::endl;
        rsp.status = 500;
        return;
      }
      file.seekp(range_start,std::ios::beg);
      file.write(&req.body[0],req.body.size());
      if(!file.good())
      {
        std::cout << "file write body flase" << std::endl;
        rsp.status = 500;
        return;
      }
      file.close();
    }

    static bool RangeParse(std::string &range,int64_t &start)
    {
      //betes=start-end
      size_t pos1 = range.find("=");
      size_t pos2 = range.find("-");
      if(pos1 == std::string::npos || pos2 == std::string::npos)
      {
        std::cout << "range:" << range << "fomat error" << std::endl;
        return false;
      }
      std::stringstream ret;
      ret << range.substr(pos1+1, pos2-pos1+1);
      ret >> start;
      return true;
    }
};

int main()
{
  CouldServer ser;
  ser.Start();
  return 0;
}
