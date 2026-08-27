#include "traymanager.h"
#include "../src/ui/mainwindow.h" // <--- КРИТИЧЕСКИ ВАЖНО: теперь компилятор "увидит" MainWindow
#include <QMenu>         // Чтобы работал QMenu
#include <QAction>       // Чтобы работал QAction
#include <QApplication>  // Чтобы работал qApp


traymanager::traymanager() {}

void traymanager::createTrayIcon(MainWindow *mainWindow) {
    // 1. Устанавливаем родителя (mainWindow), чтобы иконка удалилась вместе с окном
    trayIcon = new QSystemTrayIcon(mainWindow);
    trayIcon->setIcon(QIcon(":/icon.ico"));
    trayIcon->setToolTip("AirTask");

    // 2. Создаем меню. Родителем ставим окно.
    QMenu *trayMenu = new QMenu(mainWindow);
    QAction *restoreAction = new QAction("Открыть AirTask", trayMenu);
    QAction *quitAction = new QAction("Выход", trayMenu);

    // 3. Соединяем сигналы. Используем mainWindow (указатель)
    QAction::connect(restoreAction, &QAction::triggered, mainWindow, [mainWindow]() {
        mainWindow->show();
        mainWindow->raise();
        mainWindow->activateWindow();
    });
    QAction::connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    trayMenu->addAction(restoreAction);
    trayMenu->addSeparator();
    trayMenu->addAction(quitAction);

    trayIcon->setContextMenu(trayMenu);

    // 4. Логика клика. Захватываем mainWindow в лямбду [mainWindow]
    QAction::connect(trayIcon, &QSystemTrayIcon::activated, [mainWindow](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            if (mainWindow->isVisible() && !mainWindow->isMinimized()) {
                mainWindow->hide();
            } else {
                mainWindow->show();
                mainWindow->raise();
                mainWindow->activateWindow();
            }
        }
    });

    trayIcon->show();
}

bool traymanager::checkIcon() {
    // Проверяем, активна ли иконка в трее
    return (this->trayIcon && this->trayIcon->isVisible());
}

void traymanager::ShowMessage(QString mes)
{
    trayIcon->showMessage(
        "AirTask",                                          // заголовок
        mes, // текст
        QSystemTrayIcon::Information,                       // иконка
        5000                                                // показывать 5 сек
        );
}