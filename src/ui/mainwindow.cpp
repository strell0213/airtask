#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QFile>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>


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
    UpdateListTask();

    m_traymanager = new traymanager();
    m_traymanager->createTrayIcon(this);
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
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground); // Прозрачный фон для закругленных углов

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget"); // Для QSS
    setCentralWidget(centralWidget);

    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(15, 15, 15, 15); // Отступы от края окна
    mainLayout->setSpacing(0);

    QWidget *headerWidget = new QWidget(this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 30); // Отступ снизу до списка

    QLabel *titleLabel = new QLabel("AirTask", this);
    titleLabel->setObjectName("appTitle");

    QPushButton *addButton = new QPushButton("+", this);
    addButton->setObjectName("addButton");
    addButton->setFixedSize(30, 30); // Квадратная кнопка

    connect(addButton, &QPushButton::clicked, this, &MainWindow::onAddTaskButtonOnClick);

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch(); // Сдвигает кнопку вправо
    headerLayout->addWidget(addButton);

    mainLayout->addWidget(headerWidget);

    // 4. Создаем область прокрутки (Scroll Area) для списка задач
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame); // Убираем рамку скролла
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // Убираем горизонтальный скролл
    scrollArea->setObjectName("taskScrollArea");

    // Виджет, который будет лежать внутри скролла
    QWidget *scrollContent = new QWidget();
    scrollLayout = new QVBoxLayout(scrollContent); // Сохраняем лейаут, чтобы добавлять задачи
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(5); // Отступ между элементами списка
    scrollLayout->addStretch(); // Чтобы элементы прижимались к верху

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);
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

    // Выбор категории (Work/Personal)
    categoryCombo = new QComboBox(this);
    categoryCombo->addItems({"Work", "Personal", "Shopping"});

    QPushButton *addbutton = new QPushButton("Add",this);
    addbutton->setObjectName("PBAddTaskToDB");
    addbutton->setFixedSize(70, 35); // Квадратная кнопка
    connect(addbutton, &QPushButton::clicked, this, &MainWindow::onAddTaskButtonToDBOnClick);

    // Добавляем всё в лайаут формы
    inputLayout->addWidget(taskInput);
    inputLayout->addWidget(labelInput);
    inputLayout->addWidget(categoryCombo);
    inputLayout->addWidget(deadlineInput);
    inputLayout->addWidget(addbutton);

    // Добавляем форму в основной лайаут (между заголовком и списком)
    mainLayout->insertWidget(1, inputContainer);
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

void MainWindow::onAddTaskButtonOnClick()
{
    if(inputContainer->isVisible())
      inputContainer->setVisible(false);
    else
      inputContainer->setVisible(true);
}

void MainWindow::onAddTaskButtonToDBOnClick()
{
    task addtask;
    addtask.title = taskInput->text();
    addtask.project_id = -1;
    addtask.tags = labelInput->text();
    addtask.deadline = deadlineInput->dateTime();

    m_dbmanager->AddTaskToDB(addtask);

    inputContainer->setVisible(false);
    UpdateListTask();
}

void MainWindow::UpdateListTask()
{
    m_dbmanager->UpdateTasks(m_task);

    while (scrollLayout->count() > 1) {
        QLayoutItem* item = scrollLayout->takeAt(0); // Берем самый верхний элемент
        if (item->widget()) {
            delete item->widget(); // Удаляем сам виджет
        }
        delete item; // Удаляем обертку (layout item)
    }

    // 3. Создаем новые виджеты на основе данных из m_task
    for (const task &t : m_task) {
        // Пока создаем простую строку с чекбоксом
        // В будущем здесь будет: TaskItem *item = new TaskItem(t);
        QCheckBox *taskCheck = new QCheckBox(t.title);
        taskCheck->setChecked(t.is_completed);
        taskCheck->setStyleSheet("color: white; font-size: 14px; padding: 5px;");

        // Вставляем виджет В НАЧАЛО (индекс 0), чтобы новые задачи были сверху
        scrollLayout->insertWidget(0, taskCheck);
    }
}


