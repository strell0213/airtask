#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QSystemTrayIcon>

#include "../src/core/models/task.h"


class MainWindow;

class traymanager
{
private:
    QSystemTrayIcon *trayIcon;
public:
    traymanager();

    void createTrayIcon(MainWindow *mainWindow);
    bool checkIcon();
    void ShowMessage(QString mes);
};

#endif // TRAYMANAGER_H
