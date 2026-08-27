#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFile>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QTimeEdit>
#include <QCalendarWidget>
#include <QButtonGroup>
#include <algorithm>

static bool taskItemYLessThan(taskItem* a, taskItem* b)
{
    if (!a || !b) return a != nullptr;
    return a->mapToParent(QPoint(0, 0)).y() < b->mapToParent(QPoint(0, 0)).y();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icon.ico"));

    initMainWindow();
    initAddLayout();

    loadStyle();

    m_dbmanager = new dbmanager("airtask.db");
    UpdateAllList();

    m_traymanager = new traymanager();
    m_traymanager->createTrayIcon(this);

    m_settingManager = new settingsmanager();

    settingsScreen = new QWidget();
    settingsScreen->setObjectName("settingsScreen");
    UpdateSettings();
    initSettings();
    stackedWidget->addWidget(settingsScreen);

    m_notifyTimer = new QTimer(this);
    connect(m_notifyTimer, &QTimer::timeout, this, &MainWindow::CheckDeadlines);
    m_notifyTimer->start(10000); // каждые 10 секунд
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Проверяем, активна ли иконка в трее
    if (m_traymanager->checkIcon()) {

        // Скрываем окно с экрана
        this->hide();

        // Важный момент: говорим системе "игнорируй это событие".
        // Окно не будет уничтожено, оно просто станет невидимым.
        event->ignore();
    } else {
        // Если трея нет (по какой-то причине), просто закрываемся штатно
        event->accept();
    }
}

void MainWindow::initMainWindow()
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground); // Прозрачный фон для закругленных углов

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget"); // Для QSS
    setCentralWidget(centralWidget);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15); // Отступы от края окна
    mainLayout->setSpacing(0);

    QWidget *headerWidget = new QWidget(this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(5, 0, 0, 15); // Отступ снизу до списка

    QLabel *titleLabel = new QLabel("AirTask", this);
    titleLabel->setObjectName("appTitle");
    titleLabel->installEventFilter(this);
    titleLabel->setCursor(Qt::OpenHandCursor);

    QPushButton *addButton = new QPushButton("+", this);
    addButton->setObjectName("addButton");
    addButton->setFixedSize(30, 30); // Квадратная кнопка

    QPushButton *settingsButton = new QPushButton(this);
    settingsButton->setObjectName("settingsButton");
    settingsButton->setIcon(QIcon(":/settigns.svg"));
    settingsButton->setFixedSize(30, 30); // Квадратная кнопка

    connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddTaskButtonOnClick);
    connect(settingsButton, &QPushButton::clicked, this, [this](){
        if(stackedWidget->currentIndex() == 0) {
            stackedWidget->setCurrentIndex(1); // Показать настройки
        } else {
            stackedWidget->setCurrentIndex(0); // Вернуться к задачам
        }
    });

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(); // Сдвигает кнопку вправо
    headerLayout->addWidget(settingsButton);
    headerLayout->addWidget(addButton);

    mainLayout->addWidget(headerWidget);

    QWidget *tabsWidget = new QWidget(this);
    tabsLayout = new QHBoxLayout(tabsWidget);
    tabsLayout->setContentsMargins(5, 0, 5, 8);
    tabsLayout->setSpacing(6);
    tabsLayout->addStretch();                        // кнопки прижаты влево, stretch справа
    mainLayout->addWidget(tabsWidget);

    m_filterGroup = new QButtonGroup(this);
    m_filterGroup->setExclusive(true);

    connect(m_filterGroup, &QButtonGroup::idClicked, this, [this](int btnIndex){
        if (btnIndex == 0) {
            m_currentProjectId = -1;  // показать все
        } else {
            m_currentProjectId = m_projects[btnIndex - 1].id; // индекс проекта в БД
        }
        UpdateListTask();
    });

    stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(stackedWidget);

    taskListScreen = new QScrollArea(this);
    taskListScreen->setWidgetResizable(true);
    taskListScreen->setFrameShape(QFrame::NoFrame); // Убираем рамку скролла
    taskListScreen->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Убираем горизонтальный скролл
    taskListScreen->setObjectName("taskScrollArea");

    m_scrollContent = new QWidget();
    scrollLayout = new QVBoxLayout(m_scrollContent); // Сохраняем лейаут, чтобы добавлять задачи
    scrollLayout->setSpacing(10);
    scrollLayout->setContentsMargins(10, 10, 10, 10);
    scrollLayout->addStretch(); // Чтобы элементы прижимались к верху

    taskListScreen->setWidget(m_scrollContent);
    stackedWidget->addWidget(taskListScreen);
}

void MainWindow::initAddLayout()
{
    // Контейнер для формы ввода
    inputContainer = new QWidget(this);
    inputContainer->setObjectName("inputContainer");
    inputContainer->setVisible(false); // Скрыт при запуске

    QVBoxLayout *inputLayout = new QVBoxLayout(inputContainer);
    inputLayout->setContentsMargins(0, 0, 0, 10);

    // Поле ввода текста задачи
    taskInput = new QLineEdit(this);
    taskInput->setPlaceholderText("Add a new task...");
    taskInput->setObjectName("taskInput");

    // Поле для метки/лейбла
    labelInput = new QLineEdit(this);
    labelInput->setPlaceholderText("Label");
    labelInput->setObjectName("labelInput");

    deadlineInput = new QDateTimeEdit(this);
    deadlineInput->setObjectName("deadlineInput");
    deadlineInput->setReadOnly(true);
    deadlineInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    deadlineInput->setDisplayFormat("dd-MMM-yy HH:mm");
    deadlineInput->setDate(QDate::currentDate());

    btnCalendar = new QPushButton("📅", this);
    btnCalendar->setFixedWidth(30);

    connect(btnCalendar, &QPushButton::clicked, this, &MainWindow::ShowDatePickerPopup);

    QHBoxLayout *deadlineLayout = new QHBoxLayout();
    deadlineLayout->addWidget(deadlineInput);
    deadlineLayout->addWidget(btnCalendar);

    // Выбор категории (Work/Personal)
    categoryCombo = new QComboBox(this);
    categoryCombo->setEditable(true);
    loadProjectsAddComboBox();

    QPushButton *addbutton = new QPushButton("Add",this);
    addbutton->setObjectName("PBAddTaskToDB");
    addbutton->setFixedSize(70, 35); // Квадратная кнопка
    connect(addbutton, &QPushButton::clicked, this, &MainWindow::onAddTaskButtonToDBOnClick);

    // Добавляем всё в лайаут формы
    inputLayout->addWidget(taskInput);
    inputLayout->addWidget(labelInput);
    inputLayout->addWidget(categoryCombo);
    inputLayout->addLayout(deadlineLayout);
    inputLayout->addWidget(addbutton);

    // Добавляем форму в основной лайаут (между заголовком и списком)
    mainLayout->insertWidget(1, inputContainer);
}

void MainWindow::initSettings()
{
    QVBoxLayout *layout = new QVBoxLayout(settingsScreen);
    layout->setContentsMargins(10,10,10,30);

    QLabel *label = new QLabel("Настройки программы", settingsScreen);
    label->setStyleSheet("color: white; font-size: 18px;");

    //прозрачность
    setting opacityS = GetSettingByKey("OpacityApp");
    setting startUp = GetSettingByKey("StartUp");

    QWidget *opacityWidget = new QWidget(settingsScreen);
    QVBoxLayout *opacityLayout = new QVBoxLayout(opacityWidget);
    opacityLayout->setContentsMargins(0,5,0,10);

    QLabel *opacityLabel = new QLabel("Прозрачность", opacityWidget);
    opacityLabel->setStyleSheet("color: white; font-size: 14px");

    QSlider *slider = new QSlider(Qt::Horizontal, opacityWidget);
    slider->setMinimum(20);
    slider->setMaximum(100);
    slider->setValue(opacityS.SValue.toInt());
    setWindowOpacity(opacityS.SValue.toInt() / 100.0);

    connect(slider, &QSlider::valueChanged, this, [this](int value){
        setWindowOpacity(value / 100.0);
    });

    connect(slider, &QSlider::sliderReleased, this, [this, slider](){
        setting opacityS = GetSettingByKey("OpacityApp");
        setWindowOpacity(slider->value() / 100.0);
        opacityS.SValue = QString::number(slider->value());
        m_dbmanager->UpdateSetting(opacityS);
    });

    QCheckBox *startUpCheck = new QCheckBox(opacityWidget);
    startUpCheck->setText("Запускать программу при запуске системы");
    startUpCheck->setChecked((startUp.SValue == "1") ? true : false);
    connect(startUpCheck, &QCheckBox::toggled, this, [this](bool value){
        m_settingManager->SetAutostart(value);

        setting startUp = GetSettingByKey("StartUp");
        startUp.SValue =(value) ? "1" : "0";
        m_dbmanager->UpdateSetting(startUp);
    });

    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(slider);
    opacityLayout->addWidget(startUpCheck);
    //прозрачность

    //уведомления
    setting sNotify = GetSettingByKey("Notify");
    QWidget *notifyWidget = new QWidget(settingsScreen);
    QVBoxLayout *notifyLayout = new QVBoxLayout(notifyWidget);
    notifyLayout->setContentsMargins(0,0,0,10);

    QLabel *notifyLabel = new QLabel("Уведомления", notifyWidget);
    notifyLabel->setStyleSheet("color: white; font-size: 14px");

    QCheckBox *notifyCheck = new QCheckBox(notifyWidget);
    notifyCheck->setText("Показывать уведомления по дедлайнам");
    notifyCheck->setChecked((sNotify.SValue == "1") ? true : false);
    connect(notifyCheck, &QCheckBox::toggled, this, [this](bool value){
        setting sNotify = GetSettingByKey("Notify");
        sNotify.SValue = (value) ? "1" : "0";
        m_dbmanager->UpdateSetting(sNotify);
        m_notify = value;
    });

    notifyLayout->addWidget(notifyLabel);
    notifyLayout->addWidget(notifyCheck);
    //уведомления


    layout->addWidget(label);
    layout->addWidget(opacityWidget);
    layout->addWidget(notifyWidget);
    layout->addStretch();
}

void MainWindow::loadStyle()
{
    QFile file(":/style.qss");
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        this->setStyleSheet(styleSheet);
        file.close();
    }
}

void MainWindow::UpdateSettings()
{
    m_dbmanager->UpdateSettings(m_settings);

    setting posWindowX = GetSettingByKey("PosWindowX");
    setting posWindowY = GetSettingByKey("PosWindowY");
    int x = posWindowX.SValue.toInt();
    int y = posWindowY.SValue.toInt();

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen ? screen->availableGeometry() : QRect(0, 0, 800, 600);

    if (x <= 0 && y <= 0) {
        // Центр экрана по умолчанию
        x = screenGeometry.x() + (screenGeometry.width() - width()) / 2;
        y = screenGeometry.y() + (screenGeometry.height() - height()) / 2;
    } else {
        // Проверяем, чтобы окно не оказалось за пределами видимого экрана
        if (x < screenGeometry.left() || x > screenGeometry.right() - 50) {
            x = screenGeometry.x() + (screenGeometry.width() - width()) / 2;
        }
        if (y < screenGeometry.top() || y > screenGeometry.bottom() - 50) {
            y = screenGeometry.y() + (screenGeometry.height() - height()) / 2;
        }
    }
    move(x, y);

    setting sNotify = GetSettingByKey("Notify");
    m_notify = (sNotify.SValue == "1") ? true : false;
}

void MainWindow::onAddTaskButtonOnClick()
{
    if(inputContainer->isVisible()) {
        inputContainer->setVisible(false);
    } else {
        loadProjectsAddComboBox();
        taskInput->clear();
        labelInput->clear();
        deadlineInput->setDateTime(QDateTime::currentDateTime());
        inputContainer->setVisible(true);
        taskInput->setFocus();
    }
}

void MainWindow::onAddTaskButtonToDBOnClick()
{
    QString catText = categoryCombo->currentText().trimmed();
    int projectId = -1;
    if (!catText.isEmpty()) {
        projectId = m_dbmanager->GetFindProjectOrCreateID(catText);
    }

    task addtask;
    addtask.title = taskInput->text().trimmed();
    addtask.project_id = projectId;
    addtask.tags = labelInput->text().trimmed();
    addtask.deadline = deadlineInput->dateTime();

    if (!addtask.title.isEmpty()) {
        m_dbmanager->AddTaskToDB(addtask);
    }

    taskInput->clear();
    labelInput->clear();
    categoryCombo->setCurrentIndex(-1);
    categoryCombo->setEditText("");
    inputContainer->setVisible(false);
    UpdateAllList();
}

void MainWindow::UpdateAllList()
{
    UpdateProjectTabs();
    UpdateListTask();
    loadProjectsAddComboBox();
}

void MainWindow::UpdateProjectTabs()
{
    // Очищаем старые кнопки
    QLayoutItem *item;
    while ((item = tabsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    m_filterGroup->buttons().clear();

    // Кнопка "Все" — btnIndex = 0
    QPushButton *btnAll = new QPushButton("Все", this);
    btnAll->setCheckable(true);
    btnAll->setChecked(true);
    btnAll->setObjectName("projectTab");
    m_filterGroup->addButton(btnAll, 0);
    tabsLayout->addWidget(btnAll);

    m_dbmanager->UpdateProjects(m_projects);

    for (int i = 0; i < m_projects.size(); i++) {
        QPushButton *btn = new QPushButton(m_projects[i].name, this);
        btn->setCheckable(true);
        btn->setObjectName("projectTab");
        m_filterGroup->addButton(btn, i + 1);
        tabsLayout->addWidget(btn);
    }

    if (m_filterGroup) {
        const QList<QAbstractButton*> buttons = m_filterGroup->buttons();
        for (QAbstractButton *btn : buttons) {
            if (btn) {  // Проверка на nullptr
                btn->installEventFilter(this);
            }
        }
    }

    tabsLayout->addStretch();
}

void MainWindow::UpdateListTask(bool updateDb)
{
    if (updateDb)
        m_dbmanager->UpdateTasks(m_task, m_currentProjectId);

    while (scrollLayout->count() > 1) {
        QLayoutItem* item = scrollLayout->takeAt(0); // Берем самый верхний элемент
        if (item->widget()) {
            delete item->widget(); // Удаляем сам виджет
        }
        delete item; // Удаляем обертку (layout item)
    }

    // 3. Создаем новые виджеты на основе данных из m_task
    for (const task &t : m_task) {
        if (m_currentProjectId != -1 && t.project_id != m_currentProjectId)
            continue;

        // Создаем наш кастомный виджет
        taskItem *item = new taskItem(t, m_dbmanager, m_scrollContent);

        connect(item, &taskItem::updateRequested, this, &MainWindow::UpdateAllList);
        connect(item, &taskItem::orderChanged, this, &MainWindow::onTaskOrderChanged);

        // Вставляем его в Layout
        scrollLayout->insertWidget(scrollLayout->count() - 1, item); // перед stretch
    }
}

void MainWindow::loadProjectsAddComboBox()
{
    QString prevText = categoryCombo->currentText().trimmed();
    categoryCombo->clear();

    QVector<QString> names = m_dbmanager->GetListNameProjects();
    for (const QString &n : names)
    {
        if (!n.trimmed().isEmpty())
            categoryCombo->addItem(n.trimmed());
    }

    if (!prevText.isEmpty()) {
        int idx = categoryCombo->findText(prevText, Qt::MatchFixedString);
        if (idx >= 0) {
            categoryCombo->setCurrentIndex(idx);
        } else {
            categoryCombo->setEditText(prevText);
        }
    } else {
        categoryCombo->setCurrentIndex(-1);
        categoryCombo->setEditText("");
    }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!obj || !event) {
        return QMainWindow::eventFilter(obj, event);
    }

    // ===== ОБРАБОТКА ПЕРЕМЕЩЕНИЯ ОКНА =====
    if (obj->objectName() == "appTitle") {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
            if (mouseEvent && mouseEvent->button() == Qt::LeftButton) {
                m_dragging = true;
                m_dragPos = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove && m_dragging) {
            QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
            if (mouseEvent) {
                move(mouseEvent->globalPosition().toPoint() - m_dragPos);
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            m_dragging = false;
            QPoint pos = frameGeometry().topLeft();

            setting posWindowX = GetSettingByKey("PosWindowX");
            setting posWindowY = GetSettingByKey("PosWindowY");

            posWindowX.SValue = QString::number(pos.x());
            posWindowY.SValue = QString::number(pos.y());
            m_dbmanager->UpdateSetting(posWindowX);
            m_dbmanager->UpdateSetting(posWindowY);

            return true;
        }
    }

    // ===== ОБРАБОТКА ПРАВОГО КЛИКА НА КНОПКАХ ФИЛЬТРА =====
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);

        if (mouseEvent && mouseEvent->button() == Qt::RightButton) {
            QAbstractButton *btn = qobject_cast<QAbstractButton*>(obj);

            if (btn && m_filterGroup) {
                if (m_filterGroup->buttons().contains(btn)) {
                    int btnId = m_filterGroup->id(btn);
                    ShowProjectContextMenu(btnId, mouseEvent->globalPosition().toPoint());
                    return true;
                }
            }
        }
    }

    // Для всех остальных событий вызываем стандартную обработку
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::ShowDatePickerPopup()
{
    QDialog *popup = new QDialog(this);
    popup->setWindowFlags(Qt::Popup);
    popup->setFixedWidth(300);

    QVBoxLayout *layout = new QVBoxLayout(popup);
    layout->setContentsMargins(8, 8, 8, 8);

    // Календарь
    QCalendarWidget *calendar = new QCalendarWidget(popup);
    calendar->setSelectedDate(deadlineInput->date());
    layout->addWidget(calendar);

    // Выбор времени
    QHBoxLayout *timeLayout = new QHBoxLayout();
    QLabel *timeLabel = new QLabel("Время:", popup);
    QTimeEdit *timeEdit = new QTimeEdit(popup);
    timeEdit->setTime(deadlineInput->time());
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
        deadlineInput->setDateTime(selected);
        popup->close();
    });

    // Показываем под кнопкой-календарём
    QPoint pos = btnCalendar->mapToGlobal(QPoint(0, btnCalendar->height()));
    popup->move(pos);
    popup->exec();
}

void MainWindow::ShowProjectContextMenu(int btnId, const QPoint &pos)
{
    QMenu contextMenu;

    contextMenu.addAction("Удалить", [this, btnId]() {
        // Удаление проекта
        int projectId = (btnId == 0) ? -1 : m_projects[btnId - 1].id;

        m_dbmanager->DeleteProjectFromDB(projectId);
        UpdateAllList();
    });

    contextMenu.exec(pos);
}

QList<taskItem*> MainWindow::collectVisibleTaskItems() const
{
    QList<taskItem*> items;
    if (!scrollLayout) return items;

    for (int i = 0; i < scrollLayout->count() - 1; ++i) {
        QLayoutItem *li = scrollLayout->itemAt(i);
        if (!li || !li->widget()) continue;
        if (auto *w = qobject_cast<taskItem*>(li->widget()))
            items.append(w);
    }
    return items;
}

void MainWindow::onTaskOrderChanged()
{
    ReorderTasks(collectVisibleTaskItems(), m_currentProjectId);
}

void MainWindow::ReorderTasks(QList<taskItem*> items, int orderProjectId)
{
    std::sort(items.begin(), items.end(), taskItemYLessThan);

    for (int i = 0; i < items.size(); i++) {
        taskItem *w = items[i];
        if (!w) continue;
        m_dbmanager->SetTaskOrder(w->taskId(), orderProjectId, i);
    }

    m_dbmanager->NormalizeTaskOrder(orderProjectId);
    UpdateListTask();
}

setting MainWindow::GetSettingByKey(QString key)
{
    for(setting s : m_settings)
    {
        if(s.SKey == key) return s;
    }

    return {};
}

void MainWindow::CheckDeadlines()
{
    if (!m_notify)
    {
        UpdateListTask(false);
        return;
    }

    QDateTime now = QDateTime::currentDateTime();

    QList<int> minutes = {60, 30, 0};

    for(const task t : m_task)
    {
        if (!t.deadline.isValid()) continue;
        if (t.is_completed) continue;

        int minuteLeft = now.secsTo(t.deadline) / 60;

        for (int minute : minutes)
        {
            if (m_notifiedTasks[t.id].contains(minute)) continue;

            if(minuteLeft <= minute && minuteLeft > minute - 10)
            {
                QString mes;

                if (minute == 0)
                    mes = QString("«%1» — дедлайн сейчас!").arg(t.title);
                else
                    mes = QString("«%1» через %2 мин").arg(t.title).arg(minute);

                m_traymanager->ShowMessage(mes);

                m_notifiedTasks[t.id].insert(minute);
            }
        }
    }
    UpdateListTask(false);
}

