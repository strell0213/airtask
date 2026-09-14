#ifndef INOTIFIER_H
#define INOTIFIER_H

#include <QString>

/**
 * @brief Абстрактный интерфейс уведомителя (Базовый класс).
 * Демонстрирует полиморфизм: определяет общий контракт для всех способов отправки уведомлений.
 */
class INotifier
{
public:
    // Виртуальный деструктор обязателен для базовых классов с полиморфизмом,
    // чтобы гарантировать корректное удаление объектов-наследников через указатель базового типа (INotifier*)
    virtual ~INotifier() = default;

    /**
     * @brief Чисто виртуальный метод (pure virtual function)
     * Спецификатор '= 0' делает метод чисто виртуальным, а сам класс INotifier — абстрактным.
     * Каждый класс-наследник ОБЯЗАН переопределить этот метод по-своему.
     */
    virtual void ShowMessage(const QString &title, const QString &message) = 0;
};

#endif // INOTIFIER_H
