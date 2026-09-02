#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

#include "../src/core/models/task.h"
#include "../src/core/models/projects.h"
#include "../src/core/models/setting.h"

class dbmanager
{
public:
    // Порядок для вкладки «Все» (отдельно от порядка внутри проекта)
    static constexpr int OrderProjectAll = -1;

private:
    QSqlDatabase m_db;
    void connectDatabase();
    QString m_nameDB;

public:
    dbmanager(QString nameDB);
    ~dbmanager();

    QString GetDatabaseFilePath() const;
    QString GetDatabaseDirPath() const;
    bool ChangeDatabasePath(const QString &newDir);

    void UpdateTasks(QVector<task> &m_tasks, int orderProjectId = OrderProjectAll);
    void AddTaskToDB(task newTask);
    void CompleteTask(bool complete, task &cTask);
    void UpdateTaskChanged(task t);
    void DeleteTaskFromDB(task t);

    void AddTaskOrder(int taskId, int projectId, int numpp);
    void DeleteTaskOrder(int taskId, int projectId);
    void UpdateTaskOrder(int taskId, int orderProjectId, int order);
    void SetTaskOrder(int taskId, int orderProjectId, int numpp);
    int GetNextNumpp(int projectId);
    void NormalizeTaskOrder(int projectId);

    void UpdateProjects(QVector<projects> &projects);
    QVector<QString> GetListNameProjects();
    int GetFindProjectOrCreateID(QString name);
    int GetFindProject(QString name);
    int CreateProjectByName(QString name);
    void DeleteProjectFromDB(int pId);

    void UpdateSettings(QVector<setting> &settings);
    void CheckSettings(QVector<setting> &settings);
    void AddSetting(QString name, QString value);
    void UpdateSetting(setting s);
};

#endif // DBMANAGER_H
