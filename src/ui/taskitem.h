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

class taskItem : public QWidget //композиция
{
    Q_OBJECT
public:
    explicit taskItem(const task &t, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QCheckBox *m_checkBox;
    QLabel *m_titleLabel;
    QPushButton *m_deleteBtn;
};

#endif // TASKITEM_H
