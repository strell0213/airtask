#include "taskitem.h"

#include <QVBoxLayout>
#include <QAbstractButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QCalendarWidget>
#include <QDialog>
#include <QTimeEdit>
#include <QLineEdit>

namespace {
constexpr int kDragThreshold = 6;
}

taskItem::taskItem(task t, dbmanager *dbm, QWidget *parent) : QWidget(parent)
{
    m_Task = t;
    m_db = dbm;

    this->setObjectName("taskItemWidget");
    this->setAttribute(Qt::WA_StyledBackground); // Чтобы QSS работал

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(12);

    m_checkBox = new QCheckBox(this);
    m_checkBox->setChecked(m_Task.is_completed);
    connect(m_checkBox, &QCheckBox::toggled, this, [this](bool value){
        m_Task.is_completed = value;
        m_db->CompleteTask(value, m_Task);

        if(value)
            m_titleLabel->setStyleSheet("color: grey; font-size: 16px; font-weight: 500; text-decoration: line-through;");
        else
            m_titleLabel->setStyleSheet("color: white; font-size: 16px; font-weight: 500;");

        emit updateRequested();
    });

    // --- СОЗДАЕМ ВЕРТИКАЛЬНЫЙ БЛОК ДЛЯ ТЕКСТА ---
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(4); // Расстояние между заголовком и тегом

    m_titleLabel = new QLabel(m_Task.title, this);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_titleLabel->setMinimumWidth(0);
    if(m_Task.is_completed)
        m_titleLabel->setStyleSheet("color: grey; font-size: 16px; font-weight: 500; text-decoration: line-through;");
    else
        m_titleLabel->setStyleSheet("color: white; font-size: 16px; font-weight: 500;");

    m_titleLabel->installEventFilter(this);

    // Создаем метку для тегов
    QHBoxLayout *textHLayout = new QHBoxLayout();
    textHLayout->setSpacing(4);

    QLabel *tagLabel = new QLabel(m_Task.tags, this);
    tagLabel->setObjectName("tagLabel"); // Даем имя для стилизации в QSS
    if (m_Task.tags.trimmed().isEmpty()) {
        tagLabel->setVisible(false);
    }

    QString deadlineText = m_Task.getDeadlineText();
    m_deadlineLabel = new QLabel(this);
    m_deadlineLabel->setText(deadlineText);
    m_deadlineLabel->setObjectName("deadlineLabel");
    m_deadlineLabel->installEventFilter(this);
    if (deadlineText.trimmed().isEmpty()) {
        m_deadlineLabel->setVisible(false);
    }

    textHLayout->addWidget(tagLabel);
    textHLayout->addWidget(m_deadlineLabel);
    textHLayout->addStretch();

    textLayout->addWidget(m_titleLabel);
    textLayout->addLayout(textHLayout);
    // --------------------------------------------

    m_deleteBtn = new QPushButton("×", this);
    m_deleteBtn->setFixedSize(24, 24);
    connect(m_deleteBtn, &QPushButton::clicked, this, &taskItem::onDeleteBtnClick);

    mainLayout->addWidget(m_checkBox, 0, Qt::AlignTop | Qt::AlignLeft);
    mainLayout->addLayout(textLayout, 1);
    mainLayout->addWidget(m_deleteBtn, 0, Qt::AlignTop | Qt::AlignRight);
}

bool taskItem::eventFilter(QObject *obj, QEvent *event)
{
    if (m_deadlineLabel != nullptr && obj == m_deadlineLabel && event->type() == QEvent::MouseButtonPress) {
        ShowDatePickerForEditDeadline();
        return true;
    }
    else if(m_titleLabel != nullptr && obj == m_titleLabel && event->type() == QEvent::MouseButtonPress) {
        ShowTextEditForTitle();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

QSize taskItem::minimumSizeHint() const
{
    return QSize(50, 30);
}

void taskItem::paintEvent(QPaintEvent * /*event*/)
{
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void taskItem::onDeleteBtnClick()
{
    m_db->DeleteTaskFromDB(m_Task);
    emit updateRequested();
}

bool taskItem::isDragHandle(const QPoint &pos) const
{
    QWidget *child = childAt(pos);
    while (child && child != this) {
        if (child == m_checkBox || child == m_deleteBtn)
            return false;
        if (qobject_cast<QAbstractButton*>(child))
            return false;
        child = child->parentWidget();
    }
    return true;
}

void taskItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isDragHandle(event->pos())) {
        m_dragPending = true;
        m_dragActive = false;
        m_dragStartGlobal = event->globalPosition().toPoint();
    }
    QWidget::mousePressEvent(event);
}

void taskItem::moveInList(const QPoint &globalPos)
{
    QWidget *container = parentWidget();
    if (!container) return;

    auto *layout = qobject_cast<QVBoxLayout*>(container->layout());
    if (!layout) return;

    const int count = layout->count();
    if (count <= 1) return;

    int currentIndex = -1;
    for (int i = 0; i < count - 1; ++i) {
        if (layout->itemAt(i)->widget() == this) {
            currentIndex = i;
            break;
        }
    }
    if (currentIndex < 0) return;

    int targetIndex = count - 1;
    for (int i = 0; i < count - 1; ++i) {
        QWidget *w = layout->itemAt(i)->widget();
        if (!w || w == this) continue;

        const int midY = w->mapToGlobal(w->rect().center()).y();
        if (globalPos.y() < midY) {
            targetIndex = i;
            break;
        }
    }

    if (targetIndex > currentIndex)
        --targetIndex;

    if (targetIndex != currentIndex)
        layout->insertWidget(targetIndex, this);

    // автопрокрутка при перетаскивании у краёв списка
    for (QWidget *p = container; p; p = p->parentWidget()) {
        auto *scroll = qobject_cast<QScrollArea*>(p);
        if (!scroll) continue;

        const int margin = 24;
        const QPoint viewPos = scroll->viewport()->mapFromGlobal(globalPos);
        const int viewH = scroll->viewport()->height();
        QScrollBar *bar = scroll->verticalScrollBar();
        bar->blockSignals(true);
        if (viewPos.y() < margin)
            bar->setValue(bar->value() - 8);
        else if (viewPos.y() > viewH - margin)
            bar->setValue(bar->value() + 8);
        bar->blockSignals(false);
        break;
    }
}

void taskItem::finishDrag(bool saveOrder)
{
    if (m_dragActive) {
        releaseMouse();
        if (saveOrder)
            emit orderChanged();
    }
    m_dragPending = false;
    m_dragActive = false;
}

void taskItem::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragPending && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragStartGlobal;
        if (!m_dragActive) {
            if (delta.manhattanLength() < kDragThreshold)
                return;
            m_dragActive = true;
            grabMouse();
            raise();
        }
        moveInList(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void taskItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragPending || m_dragActive) {
        const bool save = m_dragActive && event->button() == Qt::LeftButton;
        finishDrag(save);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}


//изменение данных
void taskItem::ShowDatePickerForEditDeadline()
{
    QDialog *popup = new QDialog(this);
    popup->setWindowFlags(Qt::Popup);
    popup->setFixedWidth(300);
    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(8, 8, 8, 8);

    // Календарь
    QCalendarWidget *calendar = new QCalendarWidget(popup);
    if (!m_Task.deadline.isNull()) {
        calendar->setSelectedDate(m_Task.deadline.date());
    }
    layout->addWidget(calendar);

    // Выбор времени
    QHBoxLayout *timeLayout = new QHBoxLayout();
    QLabel *timeLabel = new QLabel("Время:", popup);
    QTimeEdit *timeEdit = new QTimeEdit(popup);
    if (!m_Task.deadline.isNull()) {
        timeEdit->setTime(m_Task.deadline.time());
    }
    timeEdit->setDisplayFormat("HH:mm");
    timeLayout->addWidget(timeLabel);
    timeLayout->addWidget(timeEdit);
    layout->addLayout(timeLayout);

    // Кнопка подтверждения
    QPushButton *btnOk = new QPushButton("Готово", popup);
    layout->addWidget(btnOk);

    connect(btnOk, &QPushButton::clicked, [=]() {
        QDateTime selected;
        selected.setDate(calendar->selectedDate());
        selected.setTime(timeEdit->time());

        m_Task.deadline.setDate(selected.date());
        m_Task.deadline.setTime(selected.time());
        m_db->UpdateTaskChanged(m_Task);

        popup->close();
        emit updateRequested();
    });

    // Показываем под лейблом
    QPoint pos = m_deadlineLabel->mapToGlobal(QPoint(0, m_deadlineLabel->height()));
    popup->move(pos);
    popup->exec();
}

void taskItem::ShowTextEditForTitle()
{
    QDialog *popup = new QDialog(this);
    popup->setWindowFlags(Qt::Popup);
    popup->setFixedWidth(150);
    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(8,8,8,8);

    QLineEdit *lineEdit = new QLineEdit(popup);
    lineEdit->setObjectName("labelInput");
    if(!m_Task.title.isNull()) {
        lineEdit->setText(m_Task.title);
    }

    QPushButton *btnOk = new QPushButton("Готово", popup);

    layout->addWidget(lineEdit);
    layout->addWidget(btnOk);

    connect(btnOk, &QPushButton::clicked, [=]() {
        QString lineText;
        lineText = lineEdit->text();

        m_Task.title = lineText;
        m_db->UpdateTaskChanged(m_Task);

        popup->close();
        emit updateRequested();
    });

    // Показываем под лейблом
    QPoint pos = m_titleLabel->mapToGlobal(QPoint(0, m_titleLabel->height()));
    popup->move(pos);
    popup->exec();
}