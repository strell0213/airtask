#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QSystemTrayIcon>


class MainWindow;

class traymanager
{
private:
    QSystemTrayIcon *trayIcon;
public:
    traymanager();

    void createTrayIcon(MainWindow *mainWindow);
    bool checkIcon();
};

#endif // TRAYMANAGER_H
