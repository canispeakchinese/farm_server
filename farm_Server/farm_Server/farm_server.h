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

enum GoodType {
    Seed, Fruit, Fertilize
};
enum Business {
    Buy, Sell, Use//买,卖,使用
};

enum ToolType {
    Spad, Pack, Water, Pyre, Weed, Harv, Alhar, Kit, Plant, Ferti, Empty
};

using namespace muduo;
using namespace net;

struct ClientInfor {
    ClientInfor() {}
    ClientInfor(int id, QString password, QByteArray inBlock, int length, int messageType) :
        id(id), password(password), inBlock(inBlock), length(length), messageType(messageType) {

    }

    int id;
    QString password;
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


    int yieldChange(const int id, int number, const QDateTime aimTime, QDateTime &water, QDateTime &pyre, QDateTime &weed);

    void checkLogin(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);

    void checkSign(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);

    void sendUpdateResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);
    QByteArray getSoilResult(ClientInfor& clientInfor);
    QByteArray getFriendResult(ClientInfor& clientInfor);
    QByteArray getGoodResult(ClientInfor& clientInfor, Business business);

    void sendPlantResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);

    void sendBusinessResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);

    void sendSpadResult(const TcpConnectionPtr &conn, QDataStream &in, ClientInfor &clientInfor);

    void sendHarvestResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);

    void sendStatusChangeResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);

    void sendReclaResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);

    void sendFertilizeResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor);

    void start();

private:
    TcpServer server_;
    InetAddress listenAddr_;

    MutexLock mutex;
    std::map<TcpConnectionPtr, ClientInfor> clientMap;

    QSqlQuery query;
};

#endif // FARM_SERVER_H
