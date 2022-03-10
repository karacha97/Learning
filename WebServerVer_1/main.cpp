//#include "web_server.h"
#include "config.h"
int main(int argc, char *argv[])
{


    //命令行解析
    Config config;
    //config.parse_arg(argc, argv);

    WebServer server;

    //初始化
    server.init(config.PORT, config.OPT_LINGER,config.thread_num, config.TRIGMode, config.actor_model,config.LOGWrite,config.close_log);
    //
    server.log_write();

    //线程池
    server.thread_pool();

    //触发模式
    server.trig_mode();

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}