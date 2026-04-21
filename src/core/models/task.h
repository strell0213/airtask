#ifndef TASK_H
#define TASK_H

#include <QString>
#include <QDateTime>

class task
{
private:

public:
    int id;
    int project_id;
    QString title;
    int numpp;
    bool is_completed;
    QDateTime deadline;
    QString tags;

    task();

    QString getDeadlineText() const;
};

#endif // TASK_H
