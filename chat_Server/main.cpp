#include "connect_mysql.h"
#include "farm_server.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

int main()
{
    if(!connect_mysql())
        LOG_FATAL << "Connect to mysql database failed";
    LOG_INFO << "Connect to mysql database successful";

    EventLoop loop;
    InetAddress serverAddr("0.0.0.0", 7777);

    FarmServer server(&loop, serverAddr);
    server.start();

    loop.loop();
    return 0;
}

