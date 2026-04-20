#include "taskitem.h"

taskItem::taskItem(const task &t, QWidget *parent) : QWidget(parent)
{
    this->setObjectName("taskItemWidget");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 5, 10, 5);

    m_checkBox = new QCheckBox(this);
    m_checkBox->setChecked(t.is_completed);

    m_titleLabel = new QLabel(t.title, this);
    m_titleLabel->setStyleSheet("color: white; font-size: 14px;");

    m_deleteBtn = new QPushButton("×", this); // Кнопка удаления
    m_deleteBtn->setFixedSize(24, 24);
    m_deleteBtn->setStyleSheet("QPushButton { color: #ff4d4d; border: none; font-weight: bold; }"
                               "QPushButton:hover { background-color: #331a1a; border-radius: 5px; }");

    layout->addWidget(m_checkBox);
    layout->addWidget(m_titleLabel);
    layout->addStretch(); // Распирает элементы, чтобы кнопка была справа
    layout->addWidget(m_deleteBtn);

    // Можно добавить эффект "зачеркивания" при нажатии на чекбокс позже через сигналы
}


void taskItem::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}