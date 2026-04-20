#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include "../src/core/models/task.h"
#include "../src/core/models/projects.h"
#include "../src/core/models/tags.h"
#include "../src/core/dbmanager.h"
#include "../src/core/traymanager.h"
#include "../src/ui/taskitem.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
private:
    Ui::MainWindow *ui;
    QVBoxLayout *scrollLayout;
    QVBoxLayout *mainLayout;

    //форма добавления
    QWidget *inputContainer;
    QLineEdit *taskInput;
    QLineEdit *labelInput;
    QComboBox *categoryCombo;
    QDateTimeEdit *deadlineInput;

    //трей
    traymanager *m_traymanager;

    //перемещение окна
    bool m_dragging = false;
    QPoint m_dragPos;

    void initMainWindow();
    void initAddLayout();
    void loadStyle();

    void onAddTaskButtonOnClick();
    void onAddTaskButtonToDBOnClick();
    void onMoveMainLabel();

    QVector<task> m_task;   //Список задач
    QVector<projects> m_projects;  //Список проектов
    QVector<tags> m_tags;   //Список меток

    dbmanager *m_dbmanager; //Движок для работы с базой

    void UpdateListTask();
};
#endif // MAINWINDOW_H
