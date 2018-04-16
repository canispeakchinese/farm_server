#ifndef FARM_SERVER_H
#define FARM_SERVER_H

#include <muduo/base/Mutex.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/EventLoop.h>

#include <map>

#include <QList>
#include <QByteArray>
#include <QDataStream>
#include <QtSql/QSqlQuery>

using namespace muduo;
using namespace net;

struct ClientInfor {
    ClientInfor() {}
    ClientInfor(string username, QByteArray inBlock, int length, int messageType) :
        username(username), inBlock(inBlock), length(length), messageType(messageType) {

    }

    string username;
    QByteArray inBlock;
    qint64 length;
    int messageType;
};

class FarmServer
{
public:
    FarmServer(EventLoop* loop, const InetAddress& listenAddr, const string& name = "farm server", int threadNum = 5);
    ~FarmServer();

    void connectionCallback(const TcpConnectionPtr& conn);
    void messageCallback(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time);
    void writeCompleteCallback(const TcpConnectionPtr& conn);

    void newMessComing(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);
    void newParticipator(const TcpConnectionPtr &conn, QDataStream& in, ClientInfor& clientInfor);

    void start();

private:
    TcpServer server_;
    InetAddress listenAddr_;
    MutexLock mutex;
    std::map<TcpConnectionPtr, ClientInfor> clientMap;

    QList<QByteArray>list;

    QSqlQuery query;
};

#endif // FARM_SERVER_H
