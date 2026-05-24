#ifndef TASKITEM_H
#define TASKITEM_H

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QBoxLayout>
#include <QPainter>
#include <QStyleOption>

#include "../src/core/models/task.h"
#include "../src/core/dbmanager.h"

class taskItem : public QWidget //композиция
{
    Q_OBJECT
public:
    explicit taskItem(task t, dbmanager *dbm, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
private:
    task m_Task;
    dbmanager *m_db;

    QCheckBox *m_checkBox;
    QLabel *m_titleLabel;
    QPushButton *m_deleteBtn;

    void onDeleteBtnClick();

signals:
    void deleteRequested();
};

#endif // TASKITEM_H
