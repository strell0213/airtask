#include "dbmanager.h"
#include "QFileInfo"

dbmanager::dbmanager(QString nameDB)
{
    m_nameDB = nameDB;
    connectDatabase();
}

void dbmanager::connectDatabase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");

    m_db.setDatabaseName(m_nameDB);

    if (!m_db.open()) {
        qDebug() << "Ошибка подключения: " << m_db.lastError().text();
        return;
    }

    qDebug() << "Путь к БД:" << QFileInfo(m_db.databaseName()).absoluteFilePath();

    qDebug() << "База данных успешно подключена!";
}

void dbmanager::UpdateTasks(QVector<task> &tasks)
{
    QSqlQuery query;
    if (!query.exec("SELECT * FROM tasks")) {
        qDebug() << "Ошибка запроса:" << query.lastError().text();
        return;
    }

    while(query.next())
    {
        task taskNew;
        taskNew.id = query.value("id").toInt();
        taskNew.project_id = query.value("project_id").toInt();
        taskNew.title = query.value("title").toString();
        taskNew.numpp = query.value("numpp").toInt();
        taskNew.is_completed = query.value("is_completed").toBool();
        taskNew.deadline = query.value("deadline").toDateTime();
        taskNew.tags = query.value("tags").toString();

        tasks.push_back(taskNew);
    }
}

void dbmanager::AddTaskToDB(task newTask)
{
    QSqlQuery query;

    query.prepare("INSERT INTO tasks (title, project_id, tags, deadline) "
                  "VALUES (:title, :project_id, :tags, :deadline)");

    query.bindValue(":title", newTask.title);
    query.bindValue(":project_id", newTask.project_id);
    query.bindValue(":tags", newTask.tags);
    query.bindValue(":deadline", newTask.deadline);

    if (!query.exec()) {
        qDebug() << "INSERT Error:" << query.lastError().text();
    } else {
        qDebug() << "Task added successfully!";
    }
}
