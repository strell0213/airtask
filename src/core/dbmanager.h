#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

#include "../src/core/models/task.h"

class dbmanager
{
private:
    QSqlDatabase m_db;
    void connectDatabase();
    QString m_nameDB;

public:
    dbmanager(QString nameDB);

    void UpdateTasks(QVector<task> &m_tasks); //Передаем адресс переменной в стеке
    void AddTaskToDB(task newTask);

};

#endif // DBMANAGER_H
