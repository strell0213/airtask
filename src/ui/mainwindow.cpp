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
#include <QFileDialog>
#include <QMessageBox>
#include <QResizeEvent>
#include <algorithm>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <windowsx.h>
#endif

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

    m_settingManager = new settingsmanager();

    QString dbDir = m_settingManager->GetDatabaseDir();
    QString dbPath = QDir(dbDir).filePath("airtask.db");
    m_dbmanager = new dbmanager(dbPath);

    initMainWindow();
    initAddLayout();

    loadStyle();

    UpdateAllList();

    m_traymanager = new traymanager();
    m_traymanager->createTrayIcon(this);

    settingsScreen = new QWidget();
    settingsScreen->setObjectName("settingsScreen");
    QVBoxLayout *screenLayout = new QVBoxLayout(settingsScreen);
    screenLayout->setContentsMargins(0, 0, 0, 0);

    settingsScrollArea = new QScrollArea(settingsScreen);
    settingsScrollArea->setWidgetResizable(true);
    settingsScrollArea->setFrameShape(QFrame::NoFrame);
    settingsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settingsScrollArea->setObjectName("settingsScrollArea");

    settingsContent = new QWidget();
    settingsContent->setObjectName("settingsContent");
    settingsScrollArea->setWidget(settingsContent);

    screenLayout->addWidget(settingsScrollArea);

    initSettings();
    UpdateSettings();
    stackedWidget->addWidget(settingsScreen);

    m_notifyTimer = new QTimer(this);
    connect(m_notifyTimer, &QTimer::timeout, this, &MainWindow::CheckDeadlines);
    m_notifyTimer->start(10000); // каждые 10 секунд


    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::UpdateAllTimer);
    m_updateTimer->start(10000);
}

MainWindow::~MainWindow()
{
    delete m_settingManager;
    delete m_dbmanager;
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
    setMinimumSize(300, 300);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    int savedW = m_settingManager->GetWindowWidth();
    int savedH = m_settingManager->GetWindowHeight();
    resize(savedW, savedH);

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

    QPushButton *telegramButton = new QPushButton(this);
    telegramButton->setObjectName("telegramButton");
    telegramButton->setIcon(QIcon(":/telegram.svg"));
    telegramButton->setFixedSize(30, 30); // Квадратная кнопка

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
    headerLayout->addWidget(telegramButton);
    headerLayout->addWidget(settingsButton);
    headerLayout->addWidget(addButton);

    mainLayout->addWidget(headerWidget);

    QWidget *tabsWidget = new QWidget(this);
    tabsWidget->setMinimumWidth(0);
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
    taskListScreen->setMinimumSize(0, 0);

    m_scrollContent = new QWidget();
    m_scrollContent->setMinimumWidth(0);
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
    taskInput->setMinimumWidth(0);

    // Поле для метки/лейбла
    labelInput = new QLineEdit(this);
    labelInput->setPlaceholderText("Label");
    labelInput->setObjectName("labelInput");
    labelInput->setMinimumWidth(0);

    deadlineInput = new QDateTimeEdit(this);
    deadlineInput->setObjectName("deadlineInput");
    deadlineInput->setReadOnly(true);
    deadlineInput->setButtonSymbols(QAbstractSpinBox::NoButtons);
    deadlineInput->setDisplayFormat("dd-MMM-yy HH:mm");
    deadlineInput->setDate(QDate::currentDate());
    deadlineInput->setMinimumWidth(0);

    btnCalendar = new QPushButton("📅", this);
    btnCalendar->setFixedWidth(30);

    connect(btnCalendar, &QPushButton::clicked, this, &MainWindow::ShowDatePickerPopup);

    QHBoxLayout *deadlineLayout = new QHBoxLayout();
    deadlineLayout->addWidget(deadlineInput);
    deadlineLayout->addWidget(btnCalendar);

    // Выбор категории (Work/Personal)
    categoryCombo = new QComboBox(this);
    categoryCombo->setEditable(true);
    categoryCombo->setMinimumWidth(0);
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
    QVBoxLayout *layout = new QVBoxLayout(settingsContent);
    layout->setContentsMargins(10, 10, 10, 30);
    layout->setSpacing(12);

    QLabel *label = new QLabel("Настройки программы", settingsContent);
    label->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");

    // 1. База данных
    QWidget *dbWidget = new QWidget(settingsContent);
    QVBoxLayout *dbLayout = new QVBoxLayout(dbWidget);
    dbLayout->setContentsMargins(0, 5, 0, 5);

    QLabel *dbLabel = new QLabel("Путь к базе данных", dbWidget);
    dbLabel->setStyleSheet("color: white; font-size: 14px; font-weight: 500;");

    QHBoxLayout *dbPathLayout = new QHBoxLayout();
    dbPathLayout->setSpacing(6);

    QLineEdit *dbPathEdit = new QLineEdit(dbWidget);
    dbPathEdit->setObjectName("dbPathEdit");
    dbPathEdit->setReadOnly(true);
    dbPathEdit->setText(m_settingManager->GetDatabaseDir());
    dbPathEdit->setToolTip(m_settingManager->GetDatabaseDir());

    QPushButton *btnSelectDbDir = new QPushButton("Обзор...", dbWidget);
    btnSelectDbDir->setObjectName("btnSelectDbDir");
    btnSelectDbDir->setCursor(Qt::PointingHandCursor);

    connect(btnSelectDbDir, &QPushButton::clicked, this, [this, dbPathEdit]() {
        QString currentDir = m_settingManager->GetDatabaseDir();
        QString selectedDir = QFileDialog::getExistingDirectory(
            this,
            "Выберите папку для базы данных",
            currentDir,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

        if (!selectedDir.isEmpty()) {
            selectedDir = QDir::toNativeSeparators(selectedDir);
            QString currentNormalized = QDir::toNativeSeparators(currentDir);
            if (QString::compare(selectedDir, currentNormalized, Qt::CaseInsensitive) != 0) {
                if (m_dbmanager->ChangeDatabasePath(selectedDir)) {
                    m_settingManager->SetDatabaseDir(selectedDir);
                    dbPathEdit->setText(selectedDir);
                    dbPathEdit->setToolTip(selectedDir);

                    UpdateAllList();
                    UpdateProjectTabs();
                    loadProjectsAddComboBox();
                    UpdateSettings();
                } else {
                    QMessageBox::warning(this, "Ошибка", "Не удалось переместить базу данных в выбранную папку.");
                }
            }
        }
    });

    dbPathLayout->addWidget(dbPathEdit);
    dbPathLayout->addWidget(btnSelectDbDir);
    dbLayout->addWidget(dbLabel);
    dbLayout->addLayout(dbPathLayout);

    // 2. Прозрачность и автозапуск
    int currentOpacity = m_settingManager->GetOpacityApp();
    bool currentStartUp = m_settingManager->GetStartUp();

    QWidget *opacityWidget = new QWidget(settingsContent);
    QVBoxLayout *opacityLayout = new QVBoxLayout(opacityWidget);
    opacityLayout->setContentsMargins(0, 5, 0, 5);

    QLabel *opacityLabel = new QLabel("Прозрачность", opacityWidget);
    opacityLabel->setStyleSheet("color: white; font-size: 14px; font-weight: 500;");

    QSlider *slider = new QSlider(Qt::Horizontal, opacityWidget);
    slider->setMinimum(20);
    slider->setMaximum(100);
    slider->setValue(currentOpacity);
    setWindowOpacity(currentOpacity / 100.0);

    connect(slider, &QSlider::valueChanged, this, [this](int value){
        setWindowOpacity(value / 100.0);
    });

    connect(slider, &QSlider::sliderReleased, this, [this, slider](){
        setWindowOpacity(slider->value() / 100.0);
        m_settingManager->SetOpacityApp(slider->value());
    });

    QCheckBox *startUpCheck = new QCheckBox(opacityWidget);
    startUpCheck->setText("Запускать программу при запуске системы");
    startUpCheck->setChecked(currentStartUp);
    connect(startUpCheck, &QCheckBox::toggled, this, [this](bool value){
        m_settingManager->SetStartUp(value);
    });

    opacityLayout->addWidget(opacityLabel);
    opacityLayout->addWidget(slider);
    opacityLayout->addWidget(startUpCheck);

    // 3. Уведомления
    bool currentNotify = m_settingManager->GetNotify();
    QWidget *notifyWidget = new QWidget(settingsContent);
    QVBoxLayout *notifyLayout = new QVBoxLayout(notifyWidget);
    notifyLayout->setContentsMargins(0, 0, 0, 5);

    QLabel *notifyLabel = new QLabel("Уведомления", notifyWidget);
    notifyLabel->setStyleSheet("color: white; font-size: 14px; font-weight: 500;");

    QCheckBox *notifyCheck = new QCheckBox(notifyWidget);
    notifyCheck->setText("Показывать уведомления по дедлайнам");
    notifyCheck->setChecked(currentNotify);
    connect(notifyCheck, &QCheckBox::toggled, this, [this](bool value){
        m_settingManager->SetNotify(value);
        m_notify = value;
    });

    notifyLayout->addWidget(notifyLabel);
    notifyLayout->addWidget(notifyCheck);

    // 4. Задачи (скрытие выполненных)
    bool hideCompleted = m_settingManager->GetHideCompletedTasks();
    QWidget *tasksSettingsWidget = new QWidget(settingsContent);
    QVBoxLayout *tasksSettingsLayout = new QVBoxLayout(tasksSettingsWidget);
    tasksSettingsLayout->setContentsMargins(0, 0, 0, 5);

    QLabel *tasksSettingsLabel = new QLabel("Задачи", tasksSettingsWidget);
    tasksSettingsLabel->setStyleSheet("color: white; font-size: 14px; font-weight: 500;");

    QCheckBox *hideCompletedCheck = new QCheckBox(tasksSettingsWidget);
    hideCompletedCheck->setText("Скрывать выполненные задачи");
    hideCompletedCheck->setChecked(hideCompleted);
    connect(hideCompletedCheck, &QCheckBox::toggled, this, [this](bool value){
        m_settingManager->SetHideCompletedTasks(value);
        UpdateListTask();
    });

    tasksSettingsLayout->addWidget(tasksSettingsLabel);
    tasksSettingsLayout->addWidget(hideCompletedCheck);

    // 5. Скрытие категорий (панель вкладок)
    QWidget *catTabsWidget = new QWidget(settingsContent);
    QVBoxLayout *catTabsMainLayout = new QVBoxLayout(catTabsWidget);
    catTabsMainLayout->setContentsMargins(0, 0, 0, 5);

    QLabel *catTabsLabel = new QLabel("Отображение категорий (вкладки)", catTabsWidget);
    catTabsLabel->setStyleSheet("color: white; font-size: 14px; font-weight: 500;");

    QLabel *catTabsHint = new QLabel("Снимите галочку, чтобы скрыть вкладку категории:", catTabsWidget);
    catTabsHint->setStyleSheet("color: #aaaaaa; font-size: 12px;");

    QWidget *catTabsContainer = new QWidget(catTabsWidget);
    m_categoryTabsLayout = new QVBoxLayout(catTabsContainer);
    m_categoryTabsLayout->setContentsMargins(5, 2, 0, 2);
    m_categoryTabsLayout->setSpacing(4);

    catTabsMainLayout->addWidget(catTabsLabel);
    catTabsMainLayout->addWidget(catTabsHint);
    catTabsMainLayout->addWidget(catTabsContainer);

    // 6. Скрытие категорий из вкладки "Все"
    QWidget *catAllWidget = new QWidget(settingsContent);
    QVBoxLayout *catAllMainLayout = new QVBoxLayout(catAllWidget);
    catAllMainLayout->setContentsMargins(0, 0, 0, 5);

    QLabel *catAllLabel = new QLabel("Отображение в категории «Все»", catAllWidget);
    catAllLabel->setStyleSheet("color: white; font-size: 14px; font-weight: 500;");

    QLabel *catAllHint = new QLabel("Снимите галочку, чтобы скрыть задачи категории из «Все»:", catAllWidget);
    catAllHint->setStyleSheet("color: #aaaaaa; font-size: 12px;");

    QWidget *catAllContainer = new QWidget(catAllWidget);
    m_categoryAllLayout = new QVBoxLayout(catAllContainer);
    m_categoryAllLayout->setContentsMargins(5, 2, 0, 2);
    m_categoryAllLayout->setSpacing(4);

    catAllMainLayout->addWidget(catAllLabel);
    catAllMainLayout->addWidget(catAllHint);
    catAllMainLayout->addWidget(catAllContainer);

    layout->addWidget(label);
    layout->addWidget(dbWidget);
    layout->addWidget(opacityWidget);
    layout->addWidget(notifyWidget);
    layout->addWidget(tasksSettingsWidget);
    layout->addWidget(catTabsWidget);
    layout->addWidget(catAllWidget);
    layout->addStretch();
}

void MainWindow::UpdateCategorySettings()
{
    if (!m_categoryTabsLayout || !m_categoryAllLayout) return;

    // Очистить текущие чекбоксы
    QLayoutItem *item;
    while ((item = m_categoryTabsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }
    while ((item = m_categoryAllLayout->takeAt(0)) != nullptr) {
        if (item->widget()) delete item->widget();
        delete item;
    }

    QSet<int> hiddenTabs = m_settingManager->GetHiddenCategoryIds();
    QSet<int> hiddenAll = m_settingManager->GetHiddenFromAllCategoryIds();

    if (m_projects.isEmpty()) {
        QLabel *emptyTabs = new QLabel("Нет созданных категорий", settingsContent);
        emptyTabs->setStyleSheet("color: #888888; font-size: 12px;");
        m_categoryTabsLayout->addWidget(emptyTabs);

        QLabel *emptyAll = new QLabel("Нет созданных категорий", settingsContent);
        emptyAll->setStyleSheet("color: #888888; font-size: 12px;");
        m_categoryAllLayout->addWidget(emptyAll);
        return;
    }

    for (const projects &p : m_projects) {
        // Чекбокс для показа вкладки
        QCheckBox *cbTab = new QCheckBox(p.name, settingsContent);
        cbTab->setChecked(!hiddenTabs.contains(p.id));
        connect(cbTab, &QCheckBox::toggled, this, [this, projectId = p.id](bool checked) {
            m_settingManager->SetCategoryHidden(projectId, !checked);
            UpdateProjectTabs();
            UpdateListTask();
        });
        m_categoryTabsLayout->addWidget(cbTab);

        // Чекбокс для отображения в "Все"
        QCheckBox *cbAll = new QCheckBox(p.name, settingsContent);
        cbAll->setChecked(!hiddenAll.contains(p.id));
        connect(cbAll, &QCheckBox::toggled, this, [this, projectId = p.id](bool checked) {
            m_settingManager->SetCategoryHiddenFromAll(projectId, !checked);
            UpdateListTask();
        });
        m_categoryAllLayout->addWidget(cbAll);
    }
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

    int x = m_settingManager->GetPosWindowX();
    int y = m_settingManager->GetPosWindowY();
    int w = m_settingManager->GetWindowWidth();
    int h = m_settingManager->GetWindowHeight();

    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen ? screen->availableGeometry() : QRect(0, 0, 800, 600);

    if (x <= 0 && y <= 0) {
        // Центр экрана по умолчанию
        x = screenGeometry.x() + (screenGeometry.width() - w) / 2;
        y = screenGeometry.y() + (screenGeometry.height() - h) / 2;
    }
    move(x, y);
    resize(w, h);

    int opacity = m_settingManager->GetOpacityApp();
    setWindowOpacity(opacity / 100.0);

    m_notify = m_settingManager->GetNotify();

    UpdateCategorySettings();
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
    UpdateCategorySettings();
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
    btnAll->setObjectName("projectTab");
    m_filterGroup->addButton(btnAll, 0);
    tabsLayout->addWidget(btnAll);

    m_dbmanager->UpdateProjects(m_projects);

    QSet<int> hiddenTabs = m_settingManager->GetHiddenCategoryIds();
    bool currentSelectedVisible = (m_currentProjectId == -1);

    for (int i = 0; i < m_projects.size(); i++) {
        if (hiddenTabs.contains(m_projects[i].id))
            continue;

        QPushButton *btn = new QPushButton(m_projects[i].name, this);
        btn->setCheckable(true);
        btn->setObjectName("projectTab");
        m_filterGroup->addButton(btn, i + 1);
        tabsLayout->addWidget(btn);

        if (m_currentProjectId == m_projects[i].id) {
            btn->setChecked(true);
            currentSelectedVisible = true;
        }
    }

    if (!currentSelectedVisible || m_currentProjectId == -1) {
        m_currentProjectId = -1;
        btnAll->setChecked(true);
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

    bool hideCompleted = m_settingManager->GetHideCompletedTasks();
    QSet<int> hiddenFromAll = m_settingManager->GetHiddenFromAllCategoryIds();

    // 3. Создаем новые виджеты на основе данных из m_task
    for (const task &t : m_task) {
        if (hideCompleted && t.is_completed)
            continue;

        if (m_currentProjectId == -1) {
            // Во вкладке "Все" пропускаем задачи скрытых категорий
            if (t.project_id != -1 && hiddenFromAll.contains(t.project_id))
                continue;
        } else {
            // В конкретной вкладке отображаем только задачи этой категории
            if (t.project_id != m_currentProjectId)
                continue;
        }

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
            if (mouseEvent && mouseEvent->button() == Qt::RightButton) {
                ShowAirTaskContextMenu(mouseEvent->globalPosition().toPoint());
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

            m_settingManager->SetWindowPos(pos.x(), pos.y());

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

void MainWindow::ShowAirTaskContextMenu(const QPoint &pos)
{
    QMenu contextMenu;

    contextMenu.addAction("Свернуть", [this]() {
        this->hide();
    });

    contextMenu.addAction("Выход", [this]() {
        QCoreApplication::quit();
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

void MainWindow::UpdateAllTimer()
{
    UpdateAllList();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_settingManager && !isMinimized() && !isMaximized()) {
        m_settingManager->SetWindowSize(width(), height());
    }
}

#if defined(Q_OS_WIN)
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            const int borderMargin = 8;
            POINT pt = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
            RECT rc;
            GetWindowRect(reinterpret_cast<HWND>(winId()), &rc);

            bool left = (pt.x >= rc.left && pt.x < rc.left + borderMargin);
            bool right = (pt.x < rc.right && pt.x >= rc.right - borderMargin);
            bool top = (pt.y >= rc.top && pt.y < rc.top + borderMargin);
            bool bottom = (pt.y < rc.bottom && pt.y >= rc.bottom - borderMargin);

            if (top && left) { *result = HTTOPLEFT; return true; }
            if (top && right) { *result = HTTOPRIGHT; return true; }
            if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
            if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
            if (left) { *result = HTLEFT; return true; }
            if (right) { *result = HTRIGHT; return true; }
            if (top) { *result = HTTOP; return true; }
            if (bottom) { *result = HTBOTTOM; return true; }
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif


