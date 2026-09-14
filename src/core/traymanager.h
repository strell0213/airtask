#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QSystemTrayIcon>

#include "inotifier.h"
#include "../src/core/models/task.h"

class MainWindow;

class traymanager : public INotifier
{
private:
    QSystemTrayIcon *trayIcon;
public:
    traymanager();

    void createTrayIcon(MainWindow *mainWindow);
    bool checkIcon();

    // Переопределение виртуального метода интерфейса INotifier (Полиморфизм)
    void ShowMessage(const QString &title, const QString &message) override;

    // Перегрузка для удобства вызова с одним параметром
    void ShowMessage(QString mes);
};

#endif // TRAYMANAGER_H
