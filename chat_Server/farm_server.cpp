#include "farm_server.h"
#include <boost/bind.hpp>
#include <muduo/base/Logging.h>

#include <QDateTime>
#include <QVariant>

#define MAXINFORLEN 50
#define MAXMESSLEN 150
#define HEADLEN ((int)(sizeof(long long) + sizeof(int)))

#define newMessage 0
#define newParti 1
#define newChat 2

FarmServer::FarmServer(EventLoop* loop, const InetAddress& listenAddr, const string &name, int threadNum) :
    server_(loop, listenAddr, name), mutex() {
    server_.setThreadNum(threadNum);
    server_.setConnectionCallback(boost::bind(&FarmServer::connectionCallback, this, _1));
    server_.setMessageCallback(boost::bind(&FarmServer::messageCallback, this, _1, _2, _3));
    server_.setWriteCompleteCallback(boost::bind(&FarmServer::writeCompleteCallback, this, _1));
    LOG_INFO << "Farm server begin listening in inet address " << listenAddr.toIpPort();
}

FarmServer::~FarmServer() {
    MutexLockGuard mutexLockGuard(mutex);
    if(!clientMap.empty())
        LOG_WARN << "Close server while has client connection";
}

void FarmServer::connectionCallback(const TcpConnectionPtr &conn) {
    if(conn->disconnected()) {
        MutexLockGuard mutexLockGuard(mutex);
        assert(clientMap.find(conn)!=clientMap.end());
        LOG_INFO << "User " << clientMap[conn].username << " stoped game.";
        clientMap.erase(conn);
    } else {
        MutexLockGuard mutexLockGuard(mutex);
        assert(clientMap.find(conn)==clientMap.end());
        LOG_INFO << "New client connectioned with ip address: " << conn->peerAddress().toIpPort();
        clientMap[conn] = ClientInfor("Unknown", QByteArray(), -1, -1);
    }
}

void FarmServer::messageCallback(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time) {
    MutexLockGuard mutexLockGuard(mutex);
    assert(clientMap.find(conn) != clientMap.end());

    ClientInfor& clientInfor = clientMap[conn];
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
        switch (messageType) {
        case newMessage:
            newMessComing(conn, in, clientInfor);
            break;
        case newParti:
            newParticipator(conn, in, clientInfor);
            break;
        case newChat:
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

void FarmServer::newMessComing(const TcpConnectionPtr &conn, QDataStream &in, ClientInfor &clientInfor) {
    qint64 length = clientInfor.length;
    if(length > MAXMESSLEN) {
        LOG_WARN << "User " << clientInfor.username << "'s message is to long, force close this connection";
        clientMap.erase(conn);
        conn->forceClose();
        return;
    }

    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    QString username = clientInfor.username.c_str();
    QString message;
    in >> message;
    out << (qint64)(0) << newMessage << username << QDateTime::currentDateTime() << message;
    out.device()->seek(0);
    out << (qint64)(outBlock.size());

    LOG_INFO << "User " << clientInfor.username << " send a message: " << message.toStdString();

    for(std::map<TcpConnectionPtr, ClientInfor>::iterator it=clientMap.begin(); it!=clientMap.end(); it++) {
        it->first->send(outBlock.begin(), outBlock.size());
    }

    out.device()->seek(HEADLEN);
    list.append(out.device()->readAll());
    if(list.size()>5)
        list.pop_front();
}

void FarmServer::newParticipator(const TcpConnectionPtr &conn, QDataStream &in, ClientInfor &clientInfor) {
    qint64 length = clientInfor.length;

    if(length > MAXINFORLEN) {
        LOG_WARN << "New participator's infor is too long.";
        clientMap.erase(conn);
        conn->forceClose();
        return;
    }

    QString username, password;
    in >> username >> password;

    LOG_INFO << "Username: " << username.toStdString().c_str() << " Password: " << password.toStdString().c_str();

    query.prepare("select isonline from user where username = ? and password = ?");
    query.addBindValue(username);
    query.addBindValue(password);
    query.exec();

    QByteArray outBlock;
    QDataStream out(&outBlock, QIODevice::ReadWrite);
    out.setVersion(QDataStream::Qt_5_5);
    out << qint64(0) << newParti;

    if(query.next())
    {
        LOG_INFO << "User " << username.toStdString() << " login successful";
        clientInfor.username = username.toStdString().c_str();           //用户登录成功
        out << true;
        out << list.size();
        foreach (QByteArray message, list) {
            outBlock.append(message);
        }
    }
    else
        out << false;
    out.device()->seek(0);
    out << (qint64)outBlock.size();

    conn->send(outBlock.begin(), outBlock.size());
    length = -1;
}

void FarmServer::start() {
    server_.start();
}

