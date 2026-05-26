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
    tasks.clear();
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

QVector<QString> dbmanager::GetListNameProjects()
{
    QVector<QString> names;

    QSqlQuery query;
    if (!query.exec("SELECT * FROM projects")) {
        qDebug() << "Ошибка запроса:" << query.lastError().text();
        return {};
    }

    while(query.next())
    {
        QString name = query.value("name").toString();

        names.push_back(name);
    }

    return names;
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

void dbmanager::DeleteTaskFromDB(task t)
{
    QSqlQuery query;

    query.prepare("DELETE FROM tasks WHERE id = :id");

    query.bindValue(":id", t.id);

    if (!query.exec()) {
        qDebug() << "DELETE Error:" << query.lastError().text();
    } else {
        qDebug() << "Task deleted successfully!";
    }
}

void dbmanager::UpdateProjects(QVector<projects> &m_projects)
{
    m_projects.clear();
    QSqlQuery query;
    if (!query.exec("SELECT * FROM projects")) {
        qDebug() << "Ошибка запроса:" << query.lastError().text();
        return;
    }

    while(query.next())
    {
        projects p;
        p.id = query.value("id").toInt();
        p.name = query.value("name").toString();
        p.color = query.value("color").toString();

        m_projects.push_back(p);
    }
}

int dbmanager::GetFindProjectOrCreateID(QString name)
{
    int findId = GetFindProject(name);
    if(findId >= 0) return findId;

    int createId = CreateProjectByName(name);
    if(createId >= 0) return createId;

    return -1;
}

int dbmanager::GetFindProject(QString name)
{
    QSqlQuery query;

    query.prepare("SELECT id FROM projects WHERE name = :name");
    query.bindValue(":name", name);
    if (!query.exec()) {
        qDebug() << "Ошибка запроса:" << query.lastError().text();
        return -1;
    }

    if (!query.first()) return -1;

    int res = query.value("id").toInt();
    return res;
}

int dbmanager::CreateProjectByName(QString name)
{
    QSqlQuery query;

    query.prepare("INSERT INTO projects (name) VALUES (:name)");
    query.bindValue(":name", name);

    if (!query.exec()) {
        qDebug() << "INSERT Error:" << query.lastError().text();
        return -1;
    } else {
        return query.lastInsertId().toInt();
    }
}
