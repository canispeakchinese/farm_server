#ifndef CONNECT_MYSQL
#define CONNECT_MYSQL

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlDriver>

bool connect_mysql()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("127.0.0.1");
    db.setPort(3306);
    db.setUserName("root");
    db.setPassword("123");
    db.setDatabaseName("farm");

    return db.open();
}

#endif // CONNECT_MYSQL

