#ifndef CONSOLENOTIFIER_H
#define CONSOLENOTIFIER_H

#include "inotifier.h"
#include <QDebug>

/**
 * @brief Альтернативная реализация INotifier для вывода сообщений в консоль/отладчик.
 * Демонстрирует полиморфизм: объект другого типа, но с тем же интерфейсом.
 */
class ConsoleNotifier : public INotifier
{
public:
    void ShowMessage(const QString &title, const QString &message) override
    {
        qDebug() << "[ConsoleNotifier]" << title << ":" << message;
    }
};

#endif // CONSOLENOTIFIER_H
