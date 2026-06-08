#ifndef TASKITEM_H
#define TASKITEM_H

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include <QMouseEvent>
#include <QPoint>

#include "../src/core/models/task.h"
#include "../src/core/dbmanager.h"

class taskItem : public QWidget //композиция
{
    Q_OBJECT
public:
    explicit taskItem(task t, dbmanager *dbm, QWidget *parent = nullptr);
    int taskId() const { return m_Task.id; }
    int projectId() const { return m_Task.project_id; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
private:
    task m_Task;
    dbmanager *m_db;

    QCheckBox *m_checkBox;
    QLabel *m_titleLabel;
    QPushButton *m_deleteBtn;

    QLabel  *m_deadlineLabel;

    bool m_dragPending = false;
    bool m_dragActive = false;
    QPoint m_dragStartGlobal;

    bool isDragHandle(const QPoint &pos) const;
    void moveInList(const QPoint &globalPos);
    void finishDrag(bool saveOrder);

    void onDeleteBtnClick();

    //изменение данных
    void ShowDatePickerForEditDeadline();
    void ShowTextEditForTitle();
    void ShowComboBoxForColorTag();

signals:
    void updateRequested();
    void orderChanged();
};

#endif // TASKITEM_H
