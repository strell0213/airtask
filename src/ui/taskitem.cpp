#include "taskitem.h"

taskItem::taskItem(const task &t, QWidget *parent) : QWidget(parent)
{
    this->setObjectName("taskItemWidget");
    this->setAttribute(Qt::WA_StyledBackground); // Чтобы QSS работал

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(12);

    m_checkBox = new QCheckBox(this);
    m_checkBox->setChecked(t.is_completed);

    // --- СОЗДАЕМ ВЕРТИКАЛЬНЫЙ БЛОК ДЛЯ ТЕКСТА ---
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(4); // Расстояние между заголовком и тегом

    m_titleLabel = new QLabel(t.title, this);
    m_titleLabel->setStyleSheet("color: white; font-size: 16px; font-weight: 500;");

    // Создаем метку для тегов
    QHBoxLayout *textHLayout = new QHBoxLayout();
    textHLayout->setSpacing(4);

    QLabel *tagLabel = new QLabel(t.tags, this);
    tagLabel->setObjectName("tagLabel"); // Даем имя для стилизации в QSS

    QString deadlineText = t.getDeadlineText();
    QLabel *deadlineLabel = new QLabel(deadlineText, this);
    deadlineLabel->setObjectName("deadlineLabel");
    textHLayout->addWidget(tagLabel);
    textHLayout->addWidget(deadlineLabel);

    textLayout->addWidget(m_titleLabel);
    textLayout->addLayout(textHLayout);
    // --------------------------------------------

    m_deleteBtn = new QPushButton("×", this);
    m_deleteBtn->setFixedSize(24, 24);
    // Стили кнопок лучше оставить в QSS файле, но для примера оставим тут

    mainLayout->addWidget(m_checkBox);
    mainLayout->addLayout(textLayout); // Добавляем наш текстовый блок
    mainLayout->addStretch();
    mainLayout->addWidget(m_deleteBtn);
}


void taskItem::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
