#include "connect_mysql.h"
#include "farm_server.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

int init() {
    if(!connect_mysql()) {
        LOG_FATAL << "Connect to mysql database failed";
    }
    LOG_INFO << "Connect to mysql database successful";
}

int main()
{
    init();

    EventLoop loop;
    InetAddress serverAddr("0.0.0.0", 6666);

    FarmServer server(&loop, serverAddr);
    server.start();

    loop.loop();
    return 0;
}

