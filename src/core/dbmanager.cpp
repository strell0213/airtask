#include "dbmanager.h"
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>

dbmanager::dbmanager(QString nameDB)
{
    m_nameDB = nameDB;
    connectDatabase();
}

void dbmanager::connectDatabase()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");

    QString dbPath = m_nameDB;
    if (QFileInfo(dbPath).isRelative()) {
        dbPath = QDir(QCoreApplication::applicationDirPath()).filePath(m_nameDB);
    }
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qDebug() << "Ошибка подключения: " << m_db.lastError().text();
        return;
    }

    qDebug() << "Путь к БД:" << QFileInfo(m_db.databaseName()).absoluteFilePath();

    qDebug() << "База данных успешно подключена!";
}

void dbmanager::UpdateTasks(QVector<task> &tasks, int orderProjectId)
{
    tasks.clear();
    QSqlQuery query;
    query.prepare(
        "SELECT "
        "  t.id, "
        "  t.project_id, "
        "  t.title, "
        "  COALESCE(o.numpp, -1) AS numpp, "
        "  t.is_completed, "
        "  t.deadline, "
        "  t.tags "
        "FROM tasks t "
        "LEFT JOIN task_order o "
        "  ON o.task_id = t.id AND o.project_id = :order_pid "
        "ORDER BY "
        "  (o.numpp IS NULL) ASC, "
        "  o.numpp ASC, "
        "  t.id ASC");
    query.bindValue(":order_pid", orderProjectId);

    if (!query.exec()) {
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
    if (!query.exec("SELECT DISTINCT name FROM projects WHERE name IS NOT NULL AND TRIM(name) != '' ORDER BY id ASC")) {
        qDebug() << "Ошибка запроса:" << query.lastError().text();
        return {};
    }

    while(query.next())
    {
        QString name = query.value("name").toString().trimmed();
        if (!name.isEmpty()) {
            names.push_back(name);
        }
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

        int taskId = query.lastInsertId().toInt();
        int next = GetNextNumpp(newTask.project_id);
        AddTaskOrder(taskId, newTask.project_id, next);
        AddTaskOrder(taskId, OrderProjectAll, GetNextNumpp(OrderProjectAll));
    }
}

void dbmanager::UpdateTaskChanged(task t)
{
    QSqlQuery query;

    query.prepare("UPDATE tasks SET "
                  "title = :title,"
                  "tags = :tags,"
                  "deadline = :deadline"
                  " WHERE id = :id");

    query.bindValue(":title", t.title);
    query.bindValue(":tags", t.tags);
    query.bindValue(":deadline", t.deadline);
    query.bindValue(":id", t.id);

    if(!query.exec())
    {
        qDebug() << "UPDATE Error:" << query.lastError().text();
    }
    else return;
}

void dbmanager::DeleteTaskFromDB(task t)
{
    QSqlQuery query;

    DeleteTaskOrder(t.id, t.project_id);
    DeleteTaskOrder(t.id, OrderProjectAll);

    query.prepare("DELETE FROM tasks WHERE id = :id");

    query.bindValue(":id", t.id);

    if (!query.exec()) {
        qDebug() << "DELETE Error:" << query.lastError().text();
    } else {
        qDebug() << "Task deleted successfully!";
    }

    NormalizeTaskOrder(t.project_id);
    NormalizeTaskOrder(OrderProjectAll);
}

void dbmanager::CompleteTask(bool complete, task &ctask)
{
    QSqlQuery query;
    query.prepare("UPDATE tasks "
                  "SET is_completed = :is_completed "
                  "WHERE id = :id");
    query.bindValue(":is_completed", complete);
    query.bindValue(":id", ctask.id);


    if (!query.exec()) {
        qDebug() << "UPDATE tasks Error:" << query.lastError().text();
    }
    else
    {
        ctask.is_completed = complete;
        return;
    }
}

void dbmanager::SetTaskOrder(int taskId, int orderProjectId, int numpp)
{
    QSqlQuery check;
    check.prepare(
        "SELECT 1 FROM task_order "
        "WHERE task_id = :tid AND project_id = :pid LIMIT 1");
    check.bindValue(":tid", taskId);
    check.bindValue(":pid", orderProjectId);

    if (!check.exec()) {
        qDebug() << "SELECT task_order Error:" << check.lastError().text();
        return;
    }

    if (check.first())
        UpdateTaskOrder(taskId, orderProjectId, numpp);
    else
        AddTaskOrder(taskId, orderProjectId, numpp);
}

void dbmanager::AddTaskOrder(int taskId, int projectId, int numpp)
{
    QSqlQuery query;
    query.prepare("INSERT INTO task_order (task_id, project_id, numpp) "
                  "VALUES (:tid, :pid, :numpp)");
    query.bindValue(":tid", taskId);
    query.bindValue(":pid", projectId);
    query.bindValue(":numpp", numpp);

    if (!query.exec()) {
        qDebug() << "INSERT task_order Error:" << query.lastError().text();
    }
}

void dbmanager::DeleteTaskOrder(int taskId, int projectId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM task_order WHERE task_id = :tid AND project_id = :pid");
    query.bindValue(":tid", taskId);
    query.bindValue(":pid", projectId);

    if (!query.exec()) {
        qDebug() << "DELETE task_order Error:" << query.lastError().text();
    }
}

void dbmanager::UpdateTaskOrder(int taskId, int projectId, int order)
{
    QSqlQuery query;
    query.prepare("UPDATE task_order "
                  "SET numpp = :numpp "
                  "WHERE task_id = :tid AND project_id = :pid");
    query.bindValue(":numpp", order);
    query.bindValue(":tid", taskId);
    query.bindValue(":pid", projectId);

    if (!query.exec()) {
        qDebug() << "UPDATE task_order Error:" << query.lastError().text();
    }
}

int dbmanager::GetNextNumpp(int projectId)
{
    QSqlQuery query;
    query.prepare("SELECT MAX(numpp) AS max_numpp FROM task_order WHERE project_id = :pid");
    query.bindValue(":pid", projectId);

    if (!query.exec()) {
        qDebug() << "SELECT MAX(numpp) Error:" << query.lastError().text();
        return 0;
    }

    if (!query.first()) return 0;
    if (query.isNull("max_numpp")) return 0;

    return query.value("max_numpp").toInt() + 1;
}

void dbmanager::NormalizeTaskOrder(int orderProjectId)
{
    {
        QSqlQuery cleanup;
        if (orderProjectId == OrderProjectAll) {
            cleanup.prepare(
                "DELETE FROM task_order "
                "WHERE project_id = :pid "
                "  AND task_id NOT IN (SELECT id FROM tasks)");
        } else {
            cleanup.prepare(
                "DELETE FROM task_order "
                "WHERE project_id = :pid "
                "  AND task_id NOT IN (SELECT id FROM tasks WHERE project_id = :pid)");
        }
        cleanup.bindValue(":pid", orderProjectId);
        if (!cleanup.exec()) {
            qDebug() << "CLEANUP task_order Error:" << cleanup.lastError().text();
        }
    }

    QSqlQuery query;
    if (orderProjectId == OrderProjectAll) {
        query.prepare(
            "SELECT "
            "  t.id AS task_id, "
            "  o.numpp AS numpp "
            "FROM tasks t "
            "LEFT JOIN task_order o "
            "  ON o.task_id = t.id AND o.project_id = :pid "
            "ORDER BY "
            "  (o.numpp IS NULL) ASC, "
            "  o.numpp ASC, "
            "  t.id ASC");
    } else {
        query.prepare(
            "SELECT "
            "  t.id AS task_id, "
            "  o.numpp AS numpp "
            "FROM tasks t "
            "LEFT JOIN task_order o "
            "  ON o.task_id = t.id AND o.project_id = :pid "
            "WHERE t.project_id = :pid "
            "ORDER BY "
            "  (o.numpp IS NULL) ASC, "
            "  o.numpp ASC, "
            "  t.id ASC");
    }
    query.bindValue(":pid", orderProjectId);

    if (!query.exec()) {
        qDebug() << "NormalizeTaskOrder SELECT Error:" << query.lastError().text();
        return;
    }

    int idx = 0;
    while (query.next()) {
        int tid = query.value("task_id").toInt();
        SetTaskOrder(tid, orderProjectId, idx);
        idx++;
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
    QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) return -1;

    int findId = GetFindProject(trimmedName);
    if(findId >= 0) return findId;

    int createId = CreateProjectByName(trimmedName);
    if(createId >= 0) return createId;

    return -1;
}

int dbmanager::GetFindProject(QString name)
{
    QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) return -1;

    QSqlQuery query;
    query.prepare("SELECT id FROM projects WHERE TRIM(name) = :name COLLATE NOCASE LIMIT 1");
    query.bindValue(":name", trimmedName);
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
    QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) return -1;

    QSqlQuery query;
    query.prepare("INSERT INTO projects (name) VALUES (:name)");
    query.bindValue(":name", trimmedName);

    if (!query.exec()) {
        qDebug() << "INSERT Error:" << query.lastError().text();
        return -1;
    } else {
        return query.lastInsertId().toInt();
    }
}

void dbmanager::DeleteProjectFromDB(int pId)
{
    QSqlQuery query;
    query.prepare("DELETE FROM projects WHERE id = :id");
    query.bindValue(":id", pId);

    if (!query.exec()) {
        qDebug() << "DELETE Error:" << query.lastError().text();
        return;
    }
}

void dbmanager::UpdateSettings(QVector<setting> &settings)
{
    settings.clear();

    QSqlQuery query;
    if (!query.exec("SELECT * FROM settings")) {
        qDebug() << "Ошибка запроса:" << query.lastError().text();
        return;
    }

    while(query.next())
    {
        setting s;
        s.ID = query.value("ID").toInt();
        s.SKey = query.value("SKey").toString();
        s.SValue = query.value("SValue").toString();

        settings.push_back(s);
    }

    CheckSettings(settings);
}

void dbmanager::CheckSettings(QVector<setting> &settings)
{
    bool update = false;

    bool opacity = false;
    bool posWindowX = false;
    bool posWindowY = false;
    bool notify = false;
    bool startUp = false;

    for (const setting s : settings)
    {
        if(s.SKey == "OpacityApp") opacity = true;
        if(s.SKey == "PosWindowX") posWindowX = true;
        if(s.SKey == "PosWindowY") posWindowY = true;
        if(s.SKey == "Notify") notify = true;
        if(s.SKey == "StartUp") startUp = true;
    }

    if(!opacity)
    {
        update=true;
        AddSetting("OpacityApp", "100");
    }

    if(!posWindowX)
    {
        update=true;
        AddSetting("PosWindowX", "0");
    }

    if(!posWindowY)
    {
        update=true;
        AddSetting("PosWindowY", "0");
    }

    if(!notify)
    {
        update=true;
        AddSetting("Notify", "1");
    }

    if (!startUp)
    {
        update=true;
        AddSetting("StartUp", "0");
    }

    if (update) UpdateSettings(settings);
    else return;
}

void dbmanager::AddSetting(QString name, QString value)
{
    QSqlQuery query;

    query.prepare("INSERT INTO settings (SKey, SValue) VALUES (:key, :value)");
    query.bindValue(":key", name);
    query.bindValue(":value", value);

    if (!query.exec()) {
        qDebug() << "INSERT Error:" << query.lastError().text();
        return;
    } else {
        return;
    }
}

void dbmanager::UpdateSetting(setting s)
{
    QSqlQuery query;

    query.prepare("UPDATE settings SET SValue = :value WHERE SKey = :key");
    query.bindValue(":key", s.SKey);
    query.bindValue(":value", s.SValue);

    if (!query.exec()) {
        qDebug() << "UPDATE Error:" << query.lastError().text();
        return;
    } else {
        return;
    }
}