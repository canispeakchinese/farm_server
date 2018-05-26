#include "farm_server.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>

#include <QDateTime>
#include <QVariant>
#include <QVector>

#define MAXINFORLEN 50
#define MAXMESSLEN 150
#define HEADLEN ((int)(sizeof(long long) + sizeof(int)))

#define newMessage 0
#define newParti 1
#define newChat 2

const int duration = 3600;
const int interval = 3600 * 5;
const int randTime = 3600;
const int needExp[11] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};

FarmServer::FarmServer(EventLoop* loop, const InetAddress& listenAddr, const string &name, int threadNum) :
    server_(loop, listenAddr, name), mutex() {
    server_.setThreadNum(threadNum);
    server_.setConnectionCallback(boost::bind(&FarmServer::connectionCallback, this, _1));
    server_.setMessageCallback(boost::bind(&FarmServer::messageCallback, this, _1, _2, _3));
    server_.setWriteCompleteCallback(boost::bind(&FarmServer::writeCompleteCallback, this, _1));
    LOG_INFO << "Farm server begin listening in inet address " << listenAddr.toIpPort();

    srand(time(NULL));
}

FarmServer::~FarmServer() {
}

void FarmServer::connectionCallback(const TcpConnectionPtr &conn) {
    if(conn->disconnected()) {
        MutexLockGuard mutexLockGuard(mutex);
        assert(clientMap.find(conn)!=clientMap.end());
        if(clientMap[conn].id != -1) {
            LOG_INFO << "User " << clientMap[conn].id << " stoped game.";
            query.exec(QString("update user set isonline = 0 where id = %1").arg(clientMap[conn].id));
        }
        clientMap.erase(conn);
    } else {
        MutexLockGuard mutexLockGuard(mutex);
        assert(clientMap.find(conn)==clientMap.end());
        LOG_INFO << "New client connectioned with ip address: " << conn->peerAddress().toIpPort();
        clientMap[conn] = ClientInfor(-1, "unknown", QByteArray(), -1, -1);
    }
}

void FarmServer::messageCallback(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
    ClientInfor* tempInfor;

    {
        MutexLockGuard mutexLockGuard(mutex);
        Q_UNUSED(mutexLockGuard);
        assert(clientMap.find(conn) != clientMap.end());
        tempInfor = &clientMap[conn];
    }

    ClientInfor& clientInfor = *tempInfor;

    QByteArray& inBlock = clientInfor.inBlock;
    qint64& length = clientInfor.length;
    int& messageType = clientInfor.messageType;

    int messageLength = buf->readableBytes();
    inBlock.append(buf->retrieveAllAsString().c_str(), messageLength);

    QDataStream in(&inBlock, QIODevice::ReadWrite);
    in.setVersion(QDataStream::Qt_5_5);

    while(true) {
        if(length==-1 && inBlock.size()<HEADLEN) {
            LOG_INFO << "New message coming without a complete head.";
            break;
        } else if(length!=-1 && inBlock.size()<length) {
            LOG_INFO << "New message coming without a complete body.";
            break;
        }
        if(length==-1) {
            in >> length >> messageType;
            LOG_INFO << "Message length: " << length << " Message type:" << messageType;
            if(inBlock.size() < length) {
                LOG_INFO << "New message coming with a complete head and a uncomplete body.";
                break;
            }
        }

        LOG_INFO << "User " << clientInfor.id << " send a request, messageType " << messageType;

        switch (messageType) {
        case 1:
            checkLogin(conn, in, clientInfor);          //登录
            break;
        case 2:
            checkSign(conn, in, clientInfor);           //注册
            break;
        case 3:
            sendUpdateResult(conn, in, clientInfor);    //返回要求的信息
            break;
        case 4:
            sendPlantResult(conn, in, clientInfor);     //返回种植结果
            break;
        case 5:
            sendBusinessResult(conn, in, clientInfor);  //根据客户端发送的交易请求返回交易结果
            break;
        case 6:
            sendSpadResult(conn, in, clientInfor);
            break;
        case 7:
            sendHarvestResult(conn, in, clientInfor);
            break;
        case 8:
            sendStatusChangeResult(conn, in, clientInfor);
            break;
        case 9:
            sendReclaResult(conn, in, clientInfor);
            break;
        case 10:
            sendFertilizeResult(conn, in, clientInfor);
            break;
        default:
            break;
        }

        length = -1;
        inBlock = in.device()->readAll();
        LOG_INFO << "Now the last context size is " << inBlock.size();
    }
}

void FarmServer::writeCompleteCallback(const TcpConnectionPtr &conn) {
}

void FarmServer::start() {
    server_.start();
}

int FarmServer::yieldChange(const int id, int number, const QDateTime aimTime, QDateTime &water, QDateTime &pyre, QDateTime &weed)
{
    LOG_INFO << "用户" << id << "的第" << number << "块土地的产量以及出现害虫时间得到更新";
    QSqlQuery query;
    query.exec(QString("select yield from soil where id = %1 and number = %2").arg(id).arg(number));
    query.next();
    int yield = query.value(0).toInt();
    while(water != QDateTime(QDate(1900,1,1), QTime(0, 0)) && water.addSecs(duration) <= aimTime)
        water = water.addSecs(duration + interval + rand()%randTime);
    while(pyre != QDateTime(QDate(1900,1,1), QTime(0, 0)) && pyre.addSecs(duration) <= aimTime)
    {
        yield--;
        pyre = pyre.addSecs(duration + interval + rand()%randTime);
    }
    while(weed != QDateTime(QDate(1900,1,1), QTime(0, 0)) && weed.addSecs(duration) <= aimTime)
        weed = weed.addSecs(duration + interval +rand()%randTime);
    if(yield < 0)
        yield = 0;
    QString aim = QString("update soil set yield = %1").arg(yield);
    if(water != QDateTime(QDate(1900,1,1), QTime(0, 0)))
        aim += QString(", water = '%1'").arg(water.toString("yyyy-MM-dd hh:mm:ss"));
    if(pyre != QDateTime(QDate(1900,1,1), QTime(0, 0)))
        aim += QString(", pyre = '%1'").arg(pyre.toString("yyyy-MM-dd hh:mm:ss"));
    if(weed != QDateTime(QDate(1900,1,1), QTime(0, 0)))
        aim += QString(", weed = '%1'").arg(weed.toString("yyyy-MM-dd hh:mm:ss"));
    aim += QString(" where id = %1 and number = %2").arg(id).arg(number);
    query.exec(aim);
    return yield;
}

void FarmServer::checkLogin(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    assert(clientInfor.id == -1);
    QString username, password;
    in >> username >> password;
    query.prepare("select * from user where username=? and password=?");
    query.addBindValue(username);
    query.addBindValue(password);
    query.exec();
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << qint64(0) << 1;
    if(!query.next())
    {
        out << false << 0;
        out.device()->seek(0);
        out << (qint64)outBlock.size();
        LOG_INFO << "Once unsuccessful log in attemp with ip address " << conn->peerAddress().toIpPort();
    }
    else if(query.value(7).toBool() == true)
    {
        out << false << 1;
        out.device()->seek(0);
        out << (qint64)outBlock.size();
        LOG_INFO << "用户\"" << username.toStdString() << "\"试图从IP地址" << conn->peerAddress().toIpPort() << "重复登录";
    }
    else
    {
        clientInfor.id = query.value(0).toInt();
        clientInfor.password = password;
        out << true << clientInfor.id << query.value(3).toString() << query.value(4).toInt()
            << query.value(5).toInt() << query.value(6).toInt();
        out.device()->seek(0);
        out << (qint64)outBlock.size();
        query.prepare("update user set isonline=1 where username=?");
        query.addBindValue(username);
        query.exec();
        LOG_INFO << "User " << username.toStdString() << " log in successful with id = " << clientInfor.id;
    }
    conn->send(outBlock.begin(), outBlock.size());
}

void FarmServer::checkSign(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    assert(clientInfor.id == -1);
    QString username, password;
    in >> username >> password;
    query.prepare("select * from user where username=?");
    query.addBindValue(username);
    query.exec();
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << qint64(0) << 2;
    if(!query.next())
    {
        query.prepare("insert into user(username, password, faceaddress, level, exp, money, isonline) values(?, ?, 1, 1, 0, 30, 1)");
        query.addBindValue(username);
        query.addBindValue(password);
        query.exec();
        query.prepare("select id from user where username = ? and password = ?");
        query.addBindValue(username);
        query.addBindValue(password);
        query.exec();
        query.next();
        clientInfor.id = query.value(0).toInt();
        clientInfor.password = password;
        for(int i=1; i<=18; i++)
        {
            if(i <= 3)
                query.exec(QString("insert into soil(id, number, level, is_recla) values(%1, %2, 1, 1)").arg(clientInfor.id).arg(i));
            else
                query.exec(QString("insert into soil(id, number, level, is_recla) values(%1, %2, 1, 0)").arg(clientInfor.id).arg(i));
        }
        query.prepare("select * from user where username=? and password=?");
        query.addBindValue(username);
        query.addBindValue(password);
        query.exec();
        query.next();
        clientInfor.id = query.value(0).toInt();
        clientInfor.password = password;
        out << 0 << clientInfor.id << query.value(3).toString() << query.value(4).toInt()
            << query.value(5).toInt() << query.value(6).toInt();
        LOG_INFO << "新用户以用户名：\"" << username.toStdString() << "\"，密码：\"" << password.toStdString() << "\"注册成功";
    }
    else
    {
        out << 1;
        LOG_INFO << "新用户以用户名：\"" << username.toStdString() << "\"，密码：\"" << password.toStdString()
                 << "\"注册失败" << "(用户名已存在)";
    }
    out.device()->seek(0);
    out << (qint64)outBlock.size();
    conn->send(outBlock.begin(), outBlock.size());
}

void FarmServer::sendUpdateResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    assert(clientInfor.id != -1);
    int updateResult;
    in >> updateResult;
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << qint64(0) << 3 << updateResult;
    LOG_INFO << "User " << clientInfor.id << "'s update result is " << updateResult;
    if(updateResult&1) {
        outBlock.append(getSoilResult(clientInfor));
        LOG_INFO << "用户" << clientInfor.id << "更新土地信息";
    }
    if(updateResult&2) {
        outBlock.append(getFriendResult(clientInfor));
        LOG_INFO << "用户" << clientInfor.id << "更新好友信息";
    }
    if(updateResult&4) {
        outBlock.append(getGoodResult(clientInfor, Buy));
        LOG_INFO << "用户" << clientInfor.id << "更新购买信息";
    }
    if(updateResult&8) {
        outBlock.append(getGoodResult(clientInfor, Sell));
        LOG_INFO << "用户" << clientInfor.id << "更新出售信息";
    }
    if(updateResult&16) {
        outBlock.append(getGoodResult(clientInfor, Use));
        LOG_INFO << "用户" << clientInfor.id << "更新使用信息";
    }
    out.device()->seek(0);
    out << (qint64)outBlock.size();
    conn->send(outBlock.begin(), outBlock.size());
}

QByteArray FarmServer::getSoilResult(ClientInfor& clientInfor)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    query.prepare("select level, is_recla, kind, cal_time, water, pyre, weed, harvest from soil where id=? order by number");
    query.addBindValue(clientInfor.id);
    query.exec();
    int number = 0;
    int reclaNum = 0;
    out << reclaNum;
    while(query.next())
    {
        number++;
        out << query.value(0).toInt();
        bool is_recla = query.value(1).toBool();
        out << is_recla;
        if(is_recla)
        {
            LOG_INFO << "用户" << clientInfor.id << "的第" << number << "块土地已开垦";
            int kind = query.value(2).toInt();
            out << kind;
            if(kind)
            {
                LOG_INFO << "用户" << clientInfor.id << "的第" << number << "块土地已种植第" << kind << "种作物";
                if(query.value(7).toBool())         //已经收获
                {
                    LOG_INFO << "用户" << clientInfor.id << "的第" << number << "块土地已收获";
                    out << true;
                }
                else
                {
                    LOG_INFO << "用户" << clientInfor.id << "的第" << number << "块土地尚未收获";
                    out << false;
                    QSqlQuery query2;
                    query2.exec(QString("select growTime, allSta from plant where id=%1").arg(kind));
                    query2.next();
                    int growTime = query2.value(0).toInt();
                    int allSta = query2.value(1).toInt();
                    QDateTime cal_time = query.value(3).toDateTime();
                    QDateTime water = query.value(4).toDateTime();
                    QDateTime pyre = query.value(5).toDateTime();
                    QDateTime weed = query.value(6).toDateTime();
                    if(cal_time.addSecs(growTime * allSta * 3600) <= QDateTime::currentDateTime())      //已经成熟
                    {
                        LOG_INFO << "用户" << clientInfor.id << "的第" << number << "块土地作物已成熟";
                        out << true << allSta << yieldChange(clientInfor.id, number, cal_time.addSecs(growTime * allSta * 3600), water, pyre, weed);
                    }
                    else                                                                                //尚未成熟
                    {
                        LOG_INFO << "用户" << clientInfor.id << "的第" << number << "块土地作物尚未成熟";
                        out << false << growTime << allSta;
                        out << yieldChange(clientInfor.id, number, QDateTime::currentDateTime(), water, pyre, weed);
                        out << growTime * allSta * 3600 - (int)cal_time.secsTo(QDateTime::currentDateTime());
                        out << (water <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(water)) > 0 ?
                                   (int)QDateTime::currentDateTime().secsTo(water) : (duration + (int)QDateTime::currentDateTime().secsTo(water)));
                        out << (pyre <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(pyre)) > 0 ?
                                   (int)QDateTime::currentDateTime().secsTo(pyre) : (duration + (int)QDateTime::currentDateTime().secsTo(pyre)));
                        out << (water <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(weed)) > 0 ?
                                   (int)QDateTime::currentDateTime().secsTo(weed) : (duration + (int)QDateTime::currentDateTime().secsTo(weed)));
                    }
                }
            }
            else
            {
                LOG_INFO << "用户" << clientInfor.id << "的第" << number << "块土地尚未种植作物";
            }
        }
        else if(reclaNum == 0) {
            LOG_INFO << "用户" << clientInfor.id << "的第" << number << "块土地尚未开垦";
            reclaNum = number;
        }
    }
    out.device()->seek(0);
    out << reclaNum;
    return outBlock;
}

QByteArray FarmServer::getFriendResult(ClientInfor& clientInfor)
{
    LOG_INFO << "用户" << clientInfor.id << "正在更新好友信息";
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << 0;
    query.prepare("select id1 from goodfriend where id2=? union select id2 from goodfriend where id1=?");
    query.addBindValue(clientInfor.id);
    query.addBindValue(clientInfor.id);
    query.exec();
    QVector<int>temp;
    while(query.next())
        temp.append(query.value(0).toInt());
    for(int i=0; i<(int)temp.size(); i++)
    {
        int _id = temp[i];
        query.exec(QString("select username,faceaddress,level,exp,money from user where id=%1").arg(_id));
        query.next();
        out << query.value(0).toString() << query.value(1).toString() << query.value(2).toInt()
            << query.value(3).toInt() << query.value(4).toInt();
    }
    out.device()->seek(0);
    out << temp.size();
    return outBlock;
}

QByteArray FarmServer::getGoodResult(ClientInfor& clientInfor, Business business)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << 0;//条数
    int goodNum = 0;
    if(business == Buy)
    {
        LOG_INFO << "用户" << clientInfor.id << "正在更新商品信息";
        query.exec(QString("select money from user where id = %1").arg(clientInfor.id));
        query.next();
        out << query.value(0).toInt();
        query.exec("select id from plant");
        while(query.next())
        {
            out << (int)Seed << query.value(0).toInt();
            goodNum++;
        }
        query.exec("select id from fertilize");
        while(query.next())
        {
            out << (int)Fertilize << query.value(0).toInt();
            goodNum++;
        }
    }
    else if(business == Sell)
    {
        LOG_INFO << "用户" << clientInfor.id << "正在更新仓库物品信息";
        query.exec(QString("select type, kind, num from store where id=%1").arg(clientInfor.id));
        while(query.next())
        {
            out << query.value(0).toInt() << query.value(1).toInt() << query.value(2).toInt();
            goodNum++;
        }
    }
    else if(business == Use)
    {
        LOG_INFO << "用户" << clientInfor.id << "正在更新种子以及化肥信息";
        query.exec(QString("select type, kind, num from store where id=%1 and type in(%2, %3)").arg(clientInfor.id).arg(Seed).arg(Fertilize));
        while(query.next())
        {
            out << query.value(0).toInt() << query.value(1).toInt() << query.value(2).toInt();
            goodNum++;
        }
    }
    out.device()->seek(0);
    out << goodNum;
    return outBlock;
}

void FarmServer::sendBusinessResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    int business, type, kind, businessNum;
    in >> business >> type >> kind >> businessNum;
    out << qint64(0) << 5 << 0;//长度，类型，结果, business
    if(businessNum <= 0)
    {
        LOG_INFO << "用户" << clientInfor.id << "请求交易的数目不合法，为" << businessNum;
        out.device()->seek(0);
        out << (qint64)outBlock.size();
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    if(business == Buy)
    {
        if(type == Seed)
        {
            LOG_INFO << "用户" << clientInfor.id << "正在购买种子" << kind;
            query.prepare("select buyprice, buylevel from plant where id = ?");
            query.addBindValue(kind);
            query.exec();
        }
        else if(type == Fertilize)
        {
            LOG_INFO << "用户" << clientInfor.id << "正在购买化肥" << kind;
            query.prepare("select buyprice, buylevel from fertilize where id = ?");
            query.addBindValue(kind);
            query.exec();
        }
        if(!query.next())
        {
            LOG_INFO << "用户" << clientInfor.id << "购买失败，商品不存在";
            out.device()->seek(0);
            out << (qint64)outBlock.size() << 5 << 1;
            conn->send(outBlock.begin(), outBlock.size());
            return;
        }
        int buyprice = query.value(0).toInt();
        int buylevel = query.value(1).toInt();
        query.exec(QString("select money, level from user where id = %1").arg(clientInfor.id));
        query.next();
        int money = query.value(0).toInt();
        int level = query.value(1).toInt();
        if(money < buyprice * businessNum || level < buylevel)
        {
            LOG_INFO << "用户" << clientInfor.id << "购买失败，金币不足或等级不足";
            out.device()->seek(0);
            out << (qint64)outBlock.size() << 5 << 2;
            conn->send(outBlock.begin(), outBlock.size());
            return;
        }
        money -= buyprice * businessNum;
        query.exec(QString("update user set money = %1 where id =%2").arg(money).arg(clientInfor.id));
        query.prepare("select * from store where id = ? and type = ? and kind =?");
        query.addBindValue(clientInfor.id);
        query.addBindValue(type);
        query.addBindValue(kind);
        query.exec();
        if(query.next())
            query.prepare("update store set num = num + ? where id = ? and type = ? and kind = ?");
        else
            query.prepare("insert into store(num, id, type, kind) values(?, ?, ?, ?)");
        query.addBindValue(businessNum);
    }
    else if(business == Sell || business == Use)
    {
        query.prepare("select num from store where id = ? and type = ? and kind = ?");
        query.addBindValue(clientInfor.id);
        query.addBindValue(type);
        query.addBindValue(kind);
        query.exec();
        if(!query.next())
        {
            LOG_INFO << "用户" << clientInfor.id << "没有商品" << kind <<",出售失败";
            out.device()->seek(0);
            out << (qint64)outBlock.size() << 5 << 1;
            conn->send(outBlock.begin(), outBlock.size());
            return;
        }
        int currentNum = query.value(0).toInt();
        if(currentNum < businessNum)
        {
            LOG_INFO << "用户" << clientInfor.id << "出售商品" << kind <<"失败，持有数量不足";
            out.device()->seek(0);
            out << (qint64)outBlock.size() << 5 << 2;
            conn->send(outBlock.begin(), outBlock.size());
            return;
        }
        if(business == Sell)
        {
            int businessPrice = 0;
            if(type == Seed)
            {
                query.exec(QString("select buyprice from plant where id = %1").arg(kind));
                query.next();
                businessPrice = query.value(0).toInt() * 0.8;
            }
            else if(type == Fertilize)
            {
                query.exec(QString("select buyprice from fertilize where id = %1").arg(kind));
                query.next();
                businessPrice = query.value(0).toInt() * 0.8;
            }
            else if(type == Fruit)
            {
                query.exec(QString("select sellprice from plant where id = %1").arg(kind));
                query.next();
                businessPrice = query.value(0).toInt();
            }
            query.exec(QString("update user set money = money + %1 where id = %2").arg(businessPrice * businessNum).arg(clientInfor.id));
        }
        if(currentNum == businessNum)
            query.prepare("delete from store where id = ? and type = ? and kind = ?");
        else
        {
            query.prepare("update store set num = num - ? where id = ? and type = ? and kind = ?");
            query.addBindValue(businessNum);
        }
    }
    query.addBindValue(clientInfor.id);
    query.addBindValue(type);
    query.addBindValue(kind);
    query.exec();
    out.device()->seek(0);
    out << (qint64)outBlock.size() << 5 << 3;
    conn->send(outBlock.begin(), outBlock.size());
    LOG_INFO << "用户" << clientInfor.id << "交易成功";
}

void FarmServer::sendPlantResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << (qint64)0 << 4 << 0;
    out.device()->seek(0);
    out << (qint64)outBlock.size() << 4;
    int number, kind;
    in >> number >> kind;
    query.exec(QString("select num from store where id=%1 and kind=%2 and type=0").arg(clientInfor.id).arg(kind));
    if(!query.next())
    {
        LOG_INFO << "用户" << clientInfor.id << "种植作物" << kind << "失败，没有足够的种子";
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    int num = query.value(0).toInt();
    query.exec(QString("select is_recla, kind from soil where id = %1 and number = %2").arg(clientInfor.id).arg(number));
    if(!query.next() || query.value(0).toBool() == false || query.value(1).toInt())
    {
        LOG_INFO << "用户" << clientInfor.id << "种植作物" << kind << "失败，土地信息有误";
        out << 1;
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    query.exec(QString("select yield,growTime,allSta from plant where id=%1").arg(kind));
    query.next();
    int growTime = query.value(1).toInt();
    int allSta = query.value(2).toInt();
    int yield = query.value(0).toInt();
    if(num == 1)
        query.exec(QString("delete from store where id=%1 and kind=%2 and type=0").arg(clientInfor.id).arg(kind));
    else
        query.exec(QString("update store set num=num-1 where id=%1 and kind=%2 and type=0").arg(clientInfor.id).arg(kind));
    QDateTime cal_time = QDateTime::currentDateTime();
    int water = interval + rand()%randTime;
    int pyre = interval + rand()%randTime;
    int weed = interval + rand()%randTime;
    query.exec(QString("update soil set kind=%1,yield=%2,cal_time='%3',water='%4',pyre='%5',weed='%6',harvest=0 where id=%7 and"
        " number=%8").arg(kind).arg(yield).arg(cal_time.toString("yyyy-MM-dd hh:mm:ss")).arg(cal_time.addSecs(water).toString("yyyy-MM-dd hh:mm:ss")).arg(cal_time.addSecs(pyre).toString("yyyy-MM-dd hh:mm:ss")).arg(cal_time.addSecs(weed).toString("yyyy-MM-dd hh:mm:ss")).arg(clientInfor.id).arg(number));
    out << 2 << number << growTime << allSta << yield << water << pyre << weed;
    out.device()->seek(0);
    out << (qint64)outBlock.size();
    conn->send(outBlock.begin(), outBlock.size());
    LOG_INFO << "用户" << clientInfor.id << "种植作物" << kind << "成功";
}

void FarmServer::sendSpadResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << (qint64)0 << 6 << 0;
    out.device()->seek(0);
    out << (qint64)outBlock.size() << 6;
    int number;
    in >> number;
    query.exec(QString("select is_recla, kind from soil where id = %1 and number = %2").arg(clientInfor.id).arg(number));
    if(!query.next() || query.value(0).toBool() == false || query.value(1).toInt() == 0)
    {
        LOG_INFO << "用户" << clientInfor.id << "铲除土地" << number << "作物失败（土地信息有误）";
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    query.exec(QString("update soil set kind = 0, harvest = 0 where id = %1 and number = %2").arg(clientInfor.id).arg(number));
    out << 1 << number;
    out.device()->seek(0);
    out << (qint64)outBlock.size();
    conn->send(outBlock.begin(), outBlock.size());
    LOG_INFO << "用户" << clientInfor.id << "铲除土地" << number << "作物成功";
}

void FarmServer::sendHarvestResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << (qint64)0 << 7 << 0;
    out.device()->seek(0);
    out << (qint64)outBlock.size() << 7;
    int number;
    in >> number;
    query.exec(QString("select is_recla, kind, harvest, level, cal_time, yield from soil where id = %1 and number = %2").arg(clientInfor.id).arg(number));
    if(!query.next() || query.value(0).toBool() == false || query.value(1).toInt() == 0 || query.value(2).toBool() == true)
    {
        LOG_INFO << "用户" << clientInfor.id << "收获土地" << number << "作物失败（土地信息有误）";
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    int kind = query.value(1).toInt();
    //int level = query.value(3).toInt();
    QDateTime cal_time = query.value(4).toDateTime();
    int yield = query.value(5).toInt();
    query.exec(QString("select growTime, allSta, exp from plant where id = %1").arg(kind));
    query.next();
    int growTime = query.value(0).toInt();
    int allSta = query.value(1).toInt();
    int exp = query.value(2).toInt();
    if(cal_time.secsTo(QDateTime::currentDateTime()) < (qint64)growTime * allSta * 3600)
    {
        LOG_INFO << "用户" << clientInfor.id << "收获土地" << number << "作物失败（土地作物未成熟）";
        out << 1;
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    query.exec(QString("update soil set harvest = 1 where id = %1 and number = %2").arg(clientInfor.id).arg(number));
    query.exec(QString("select * from store where id = %1 and type = %2 and kind = %3").arg(clientInfor.id).arg(Fruit).arg(kind));
    if(query.next())
    {
        query.exec(QString("update store set num = num + %1 where id = %2 and type = %3 and kind = %4")
                   .arg(yield).arg(clientInfor.id).arg(Fruit).arg(kind));
    }
    else
        query.exec(QString("insert into store values(%1, %2, %3, %4)").arg(clientInfor.id).arg(kind).arg(yield).arg(Fruit));
    query.exec(QString("select level, exp from user where id = %1").arg(clientInfor.id));
    query.next();
    int currLevel = query.value(0).toInt();
    int currExp = query.value(1).toInt();
    currExp += exp;
    while(currExp >= needExp[currLevel])
    {
        currExp -= needExp[currLevel];
        currLevel++;
    }
    query.exec(QString("update user set level = %1, exp = %2 where id = %3").arg(currLevel).arg(currExp).arg(clientInfor.id));
    out << 2 << number << kind << yield << exp;
    out.device()->seek(0);
    out << (qint64)outBlock.size();
    conn->send(outBlock.begin(), outBlock.size());
    LOG_INFO << "用户" << clientInfor.id << "收获土地" << number << "作物成功";
}

void FarmServer::sendStatusChangeResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << (qint64)0 << 8 << 0;
    out.device()->seek(0);
    out << (qint64)outBlock.size() << 8;
    int number, _status;
    bool _auto;
    in >> number >> _status >> _auto;
    query.exec(QString("select kind, cal_time, water, pyre, weed from soil where id = %1 and number = %2").arg(clientInfor.id).arg(number));
    query.next();
    int kind = query.value(0).toInt();
    if(!kind)           //未种植
    {
        LOG_INFO << "用户" << clientInfor.id << "更新土地" << number << "状态失败，当前土地未种植";
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    else
    {
        QSqlQuery query2;
        query2.exec(QString("select growTime, allSta from plant where id = %1").arg(kind));
        query2.next();
        int growTime = query2.value(0).toInt();
        int allSta = query2.value(1).toInt();
        if(query.value(1).toDateTime().addSecs(growTime * allSta * 3600) <= QDateTime::currentDateTime())//已成熟
        {
            LOG_INFO << "用户" << clientInfor.id << "更新土地" << number << "状态失败，当前作物已成熟";
            conn->send(outBlock.begin(), outBlock.size());
            return;
        }
    }
    QDateTime water = QDateTime(QDate(1900,1,1), QTime(0, 0));
    QDateTime pyre = QDateTime(QDate(1900,1,1), QTime(0, 0));
    QDateTime weed = QDateTime(QDate(1900,1,1), QTime(0, 0));
    if(_status == Water)
    {
        water = query.value(2).toDateTime();
        if(_auto)//自动消失
        {
            LOG_INFO << "用户" << clientInfor.id << "土地" << number << "未及时浇水，产量下降";
            if(water.addSecs(duration) <= QDateTime::currentDateTime())
            {
                yieldChange(clientInfor.id, number, QDateTime::currentDateTime(), water, pyre, weed);
                out << 1 << number << _status << _auto << (water <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(water)) > 0 ?
                                                                       (int)QDateTime::currentDateTime().secsTo(water) : (duration + (int)QDateTime::currentDateTime().secsTo(water)));
                out.device()->seek(0);
                out << (qint64)outBlock.size();
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
            else
            {
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
        }
        else//人工浇水
        {
            LOG_INFO << "用户" << clientInfor.id << "及时为土地" << number << "浇水";
            if(water > QDateTime::currentDateTime() || water.addSecs(duration) <= QDateTime::currentDateTime())
            {
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
            else
            {
                water = QDateTime::currentDateTime().addSecs(interval + rand()%randTime);
                out << 1 << number << _status << _auto << (water <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(water)) > 0 ?
                                                                       (int)QDateTime::currentDateTime().secsTo(water) : (duration + (int)QDateTime::currentDateTime().secsTo(water)));
                query.exec(QString("update soil set yield = yield + 1, water = '%1' where id = %2 and number = %3")
                           .arg(water.toString("yyyy-MM-dd hh:mm:ss")).arg(clientInfor.id).arg(number));
                out.device()->seek(0);
                out << (qint64)outBlock.size();
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
        }
    }
    else if(_status == Pyre)
    {
        pyre = query.value(3).toDateTime();
        if(_auto)//自动消失
        {
            LOG_INFO << "用户" << clientInfor.id << "土地" << number << "未及时除虫，产量下降";
            if(pyre.addSecs(duration) <= QDateTime::currentDateTime())
            {
                yieldChange(clientInfor.id, number, QDateTime::currentDateTime(), water, pyre, weed);
                out << 1 << number << _status << _auto << (pyre <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(pyre)) > 0 ?
                                                                       (int)QDateTime::currentDateTime().secsTo(pyre) : (duration + (int)QDateTime::currentDateTime().secsTo(pyre)));
                out.device()->seek(0);
                out << (qint64)outBlock.size();
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
            else
            {
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
        }
        else//人工除虫
        {
            LOG_INFO << "用户" << clientInfor.id << "及时为土地" << number << "除虫";
            if(pyre > QDateTime::currentDateTime() || pyre.addSecs(duration) <= QDateTime::currentDateTime())
            {
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
            else
            {
                pyre = QDateTime::currentDateTime().addSecs(interval + rand()%randTime);
                out << 1 << number << _status << _auto << (pyre <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(pyre)) > 0 ?
                                                                       (int)QDateTime::currentDateTime().secsTo(pyre) : (duration + (int)QDateTime::currentDateTime().secsTo(pyre)));
                query.exec(QString("update soil set pyre = '%1' where id = %2 and number = %3")
                           .arg(pyre.toString("yyyy-MM-dd hh:mm:ss")).arg(clientInfor.id).arg(number));
                out.device()->seek(0);
                out << (qint64)outBlock.size();
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
        }
    }
    else if(_status == Weed)
    {
        weed = query.value(4).toDateTime();
        if(_auto)//自动消失
        {
            LOG_INFO << "用户" << clientInfor.id << "土地" << number << "未及时除草，产量下降";
            if(weed.addSecs(duration) <= QDateTime::currentDateTime())
            {
                yieldChange(clientInfor.id, number, QDateTime::currentDateTime(), water, pyre, weed);
                out << 1 << number << _status << _auto << (weed <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(weed)) > 0 ?
                                                                       (int)QDateTime::currentDateTime().secsTo(weed) : (duration + (int)QDateTime::currentDateTime().secsTo(weed)));
                out.device()->seek(0);
                out << (qint64)outBlock.size();
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
            else
            {
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
        }
        else//人工除草
        {
            LOG_INFO << "用户" << clientInfor.id << "及时为土地" << number << "除虫";
            if(weed > QDateTime::currentDateTime() || weed.addSecs(duration) <= QDateTime::currentDateTime())
            {
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
            else
            {
                weed = QDateTime::currentDateTime().addSecs(interval + rand()%randTime);
                out << 1 << number << _status << _auto << (weed <= QDateTime::currentDateTime()) << (((int)QDateTime::currentDateTime().secsTo(weed)) > 0 ?
                                                                       (int)QDateTime::currentDateTime().secsTo(weed) : (duration + (int)QDateTime::currentDateTime().secsTo(weed)));
                query.exec(QString("update soil set weed = '%1' where id = %2 and number = %3")
                           .arg(weed.toString("yyyy-MM-dd hh:mm:ss")).arg(clientInfor.id).arg(number));
                out.device()->seek(0);
                out << (qint64)outBlock.size();
                conn->send(outBlock.begin(), outBlock.size());
                return;
            }
        }
    }
}

void FarmServer::sendReclaResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << (qint64)0 << 9 << 0;
    out.device()->seek(0);
    out << (qint64)outBlock.size() << 9;
    int number;
    in >> number;
    query.exec(QString("select is_recla from soil where id = %1 and number in(%2, %3) order by number").arg(clientInfor.id).arg(number).arg(number+1));
    query.next();
    if(query.value(0).toBool() == false || !query.next() || query.value(0).toBool() == true)
    {
        LOG_INFO << "用户" << clientInfor.id << "开垦土地" << number << "失败，当前土地不可开垦";
        conn->send(outBlock.begin(), outBlock.size());
    }
    else
    {
        query.exec(QString("select money, level from user where id = %1").arg(clientInfor.id));
        query.next();
        if(query.value(0).toInt() < number || query.value(1).toInt() < number/3+1)
        {
            LOG_INFO << "用户" << clientInfor.id << "开垦土地" << number << "失败，等级不足或金币不足";
            out << 1;
            conn->send(outBlock.begin(), outBlock.size());
        }
        else
        {
            LOG_INFO << "用户" << clientInfor.id << "开垦土地" << number << "成功";
            int money = query.value(0).toInt() - number;
            query.exec(QString("update soil set money = %1 where id = %2").arg(money).arg(clientInfor.id));
            query.exec(QString("update soil set is_recla = 1 where id = %1 and number = %2").arg(clientInfor.id).arg(number+1));
            out << 2 << money;
            out.device()->seek(0);
            out << (qint64)outBlock.size();
            conn->send(outBlock.begin(), outBlock.size());
        }
    }
}

void FarmServer::sendFertilizeResult(const TcpConnectionPtr& conn, QDataStream& in, ClientInfor& clientInfor)
{
    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << (qint64)0 << 10 << 0;
    out.device()->seek(0);
    out << (qint64)outBlock.size() << 10;
    int number, kind;
    in >> number >> kind;
    query.exec(QString("select num from store where id=%1 and kind=%2 and type=2").arg(clientInfor.id).arg(kind));
    if(!query.next())
    {
        LOG_INFO << "用户" << clientInfor.id << "为土地" << number << "施化肥" << kind << "失败，化肥不足";
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    int num = query.value(0).toInt();
    query.exec(QString("select is_recla, kind, cal_time from soil where id = %1 and number = %2").arg(clientInfor.id).arg(number));
    if(!query.next() || query.value(0).toBool() == false || query.value(1).toInt() == 0)
    {
        LOG_INFO << "用户" << clientInfor.id << "为土地" << number << "施化肥" << kind << "失败，土地信息异常";
        out << 1;
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    int seedkind = query.value(1).toInt();
    QDateTime cal_time = query.value(2).toDateTime();
    query.exec(QString("select growTime,allSta from plant where id=%1").arg(seedkind));
    query.next();
    int growTime = query.value(0).toInt();
    int allSta = query.value(1).toInt();
    if(cal_time.addSecs(growTime * allSta * 3600) < QDateTime::currentDateTime())
    {
        LOG_INFO << "用户" << clientInfor.id << "为土地" << number << "施化肥" << kind << "失败，土地信息异常";
        out << 1;
        conn->send(outBlock.begin(), outBlock.size());
        return;
    }
    query.exec(QString("select reduTime from fertilize where id = %1").arg(kind));
    query.next();
    int reduTime = query.value(0).toInt();
    if(num == 1)
        query.exec(QString("delete from store where id=%1 and kind=%2 and type=2").arg(clientInfor.id).arg(kind));
    else
        query.exec(QString("update store set num=num-1 where id=%1 and kind=%2 and type=2").arg(clientInfor.id).arg(kind));
    cal_time = cal_time.addSecs(-reduTime);
    query.exec(QString("update soil set cal_time='%1' where id=%2 and number=%3")
               .arg(cal_time.toString("yyyy-MM-dd hh:mm:ss")).arg(clientInfor.id).arg(number));
    out << 2 << number << reduTime;
    out.device()->seek(0);
    out << (qint64)outBlock.size();
    conn->send(outBlock.begin(), outBlock.size());
    LOG_INFO << "用户" << clientInfor.id << "为土地" << number << "施化肥" << kind << "成功";
}

