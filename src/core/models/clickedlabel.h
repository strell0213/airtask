#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QMouseEvent>

class ClickedLabel : public QLabel
{
    Q_OBJECT

public:
    explicit ClickedLabel(QWidget* parent = nullptr) : QLabel(parent) {}

signals:
    void clicked(); // Наш собственный сигнал

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            qDebug() << "1. Физический клик по лейблу зафиксирован!";
            emit clicked(); // Генерируем сигнал при клике левой кнопкой
        }
        QLabel::mousePressEvent(event); // Передаем событие дальше базовому классу
    }
};

#endif // CLICKABLELABEL_H