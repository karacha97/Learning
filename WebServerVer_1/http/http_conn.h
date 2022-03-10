#ifndef HTTP_H
#define HTTP_H
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"
class http_conn
{
public:
    static const int FILENAME_LEN = 200;        //文件名长度
    static const int READ_BUFFER_SIZE = 2048;   //读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 1024;  //写缓冲区大小
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE    //主FSM状态
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,     //请求未读完
        GET_REQUEST,    //请求读取完成
        BAD_REQUEST,    //请求语法出错
        NO_RESOURCE,    //没有资源
        FORBIDDEN_REQUEST,
        FILE_REQUEST,   //文件请求
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,//读完了一行
        LINE_BAD,   //出错
        LINE_OPEN   //
    };
public:
    void init(int sockfd, const sockaddr_in &addr, char *, int,int);
    void close_conn(bool real_close = true);
    void process();         //处理请求
    bool read();            //读所有数据到缓冲区；
    bool write();           //写所有数据；
    sockaddr_in *get_address()//获取当前地址
    {
        return &m_address;
    }

private:
    void init();            //初始化
    HTTP_CODE process_read();           //解析读到的数据，确定请求内容
    bool process_write(HTTP_CODE ret);  //根据解析到的内容把数据放入写缓冲区
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();      //根据解析到的数据处理，这里是将请求的文件映射
    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
public:
    http_conn(){}
 
    ~http_conn(){}
//----------------------------------------------------------
public:
    static int m_epollfd;
    static int m_user_count;
    int m_state;  //读为0, 写为1,reactor模式下
    int timer_flag;
    int improv;
private:
    int m_sockfd;           //套接字文件描述符
    sockaddr_in m_address;  //套接字地址信息
    char m_read_buf[READ_BUFFER_SIZE];  //读缓冲相关
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE]; //写缓冲相关，需要写的数据放在这
    int m_write_idx;
    //------------------------------
    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    int m_content_length;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    char *m_string; //存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
    char *doc_root;
    int m_TRIGMode; //触发方式，LT or ET
    int m_close_log;
};



#endif