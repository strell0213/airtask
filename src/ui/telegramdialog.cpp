#include "telegramdialog.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QMouseEvent>
#include <QMessageBox>
#include <QFrame>
#include <QHeaderView>
#include <QTabBar>

TelegramDialog::TelegramDialog(TelegramBot *bot, settingsmanager *settingsMgr, dbmanager *dbMgr, QWidget *parent)
    : QDialog(parent)
    , m_bot(bot)
    , m_settingsManager(settingsMgr)
    , m_dbManager(dbMgr)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(540, 700);

    initUI();

    // Подключение сигналов от TelegramBot
    if (m_bot) {
        connect(m_bot, &TelegramBot::stateChanged, this, &TelegramDialog::onBotStateChanged);
        connect(m_bot, &TelegramBot::authCodeChanged, this, &TelegramDialog::onBotAuthCodeChanged);
        connect(m_bot, &TelegramBot::userSubscribed, this, &TelegramDialog::onUserSubscribed);
        connect(m_bot, &TelegramBot::logMessage, this, &TelegramDialog::onLogMessage);

        // Инициализация текущего состояния
        updateStatusDisplay(m_bot->GetState(), m_bot->GetStatusText());
        onBotAuthCodeChanged(m_bot->GetAuthCode(), m_bot->GetRemainingCodeSeconds());

        // Загрузка существующей истории логов
        if (m_logEdit) {
            for (const QString &line : m_bot->GetLogHistory()) {
                m_logEdit->appendPlainText(line);
            }
        }
    }

    updateSubscribersCount();
    updateClientsTable();
}

TelegramDialog::~TelegramDialog()
{
}

void TelegramDialog::initUI()
{
    setObjectName("TelegramDialog");
    setStyleSheet(
        "QDialog#TelegramDialog { "
        "  background-color: #1e2229; "
        "  border: 1px solid #3d434f; "
        "  border-radius: 10px; "
        "} "
        "QLabel { color: #ffffff; font-family: 'Segoe UI', sans-serif; font-size: 13px; } "
        "QLineEdit { "
        "  background-color: #252a33; "
        "  border: 1px solid #3d434f; "
        "  border-radius: 6px; "
        "  padding: 8px; "
        "  color: #ffffff; "
        "  font-size: 13px; "
        "} "
        "QLineEdit:focus { border: 1px solid #4da6ff; } "
        "QPushButton { "
        "  background-color: #2d323b; "
        "  color: #ffffff; "
        "  border: 1px solid #3d434f; "
        "  border-radius: 6px; "
        "  padding: 7px 14px; "
        "  font-size: 13px; "
        "  font-weight: 500; "
        "} "
        "QPushButton:hover { background-color: #3d434f; } "
        "QPushButton:pressed { background-color: #252a33; } "
        "QPushButton#closeDialogBtn { "
        "  background-color: #2d323b; "
        "  border: 1px solid #3d434f; "
        "  border-radius: 6px; "
        "  font-size: 15px; "
        "  font-weight: bold; "
        "  color: #e0e0e0; "
        "} "
        "QPushButton#closeDialogBtn:hover { "
        "  background-color: #e74c3c; "
        "  border-color: #e74c3c; "
        "  color: #ffffff; "
        "} "
        "QPushButton#closeDialogBtn:pressed { "
        "  background-color: #c0392b; "
        "} "
        "QCheckBox { color: #ffffff; font-size: 13px; spacing: 8px; } "
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1px solid #3d434f; background-color: #252a33; } "
        "QCheckBox::indicator:checked { background-color: #3498db; border-color: #3498db; } "
        "QTabWidget::pane { "
        "  border: 1px solid #2d323b; "
        "  background-color: #171a20; "
        "  border-radius: 6px; "
        "} "
        "QTabBar::tab { "
        "  background-color: #252a33; "
        "  color: #a0a0a0; "
        "  border: 1px solid #3d434f; "
        "  border-bottom: none; "
        "  border-top-left-radius: 6px; "
        "  border-top-right-radius: 6px; "
        "  padding: 6px 16px; "
        "  margin-right: 4px; "
        "  font-size: 12px; "
        "  font-weight: 600; "
        "} "
        "QTabBar::tab:selected { "
        "  background-color: #171a20; "
        "  color: #4da6ff; "
        "  border-bottom: 2px solid #4da6ff; "
        "} "
        "QTabBar::tab:hover:!selected { "
        "  background-color: #2d323b; "
        "  color: #ffffff; "
        "} "
        "QPlainTextEdit { "
        "  background-color: #14171c; "
        "  border: 1px solid #2d323b; "
        "  border-radius: 6px; "
        "  color: #d1d5db; "
        "  font-family: 'Consolas', 'Courier New', monospace; "
        "  font-size: 11px; "
        "  padding: 6px; "
        "} "
        "QTableWidget { "
        "  background-color: #14171c; "
        "  border: 1px solid #2d323b; "
        "  border-radius: 6px; "
        "  gridline-color: #252a33; "
        "  color: #ffffff; "
        "  font-size: 12px; "
        "  selection-background-color: #1d3c5a; "
        "  selection-color: #ffffff; "
        "} "
        "QHeaderView::section { "
        "  background-color: #20242c; "
        "  color: #a0a0a0; "
        "  padding: 6px; "
        "  border: 1px solid #2d323b; "
        "  font-size: 11px; "
        "  font-weight: bold; "
        "} "
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 14, 18, 16);
    mainLayout->setSpacing(12);

    // --- Хедер с заголовком и кнопкой закрытия ---
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("Telegram Бот AirTask", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");

    m_closeBtn = new QPushButton("✕", this);
    m_closeBtn->setObjectName("closeDialogBtn");
    m_closeBtn->setFixedSize(30, 30);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setToolTip("Закрыть окно (Esc)");
    connect(m_closeBtn, &QPushButton::clicked, this, [this]() {
        this->close();
    });

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(headerLayout);

    // --- Разделитель ---
    QFrame *sep1 = new QFrame(this);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setStyleSheet("color: #2d323b; background-color: #2d323b; max-height: 1px;");
    mainLayout->addWidget(sep1);

    // --- Секция 1: Токен бота ---
    QVBoxLayout *tokenLayout = new QVBoxLayout();
    tokenLayout->setSpacing(5);
    QLabel *tokenLabel = new QLabel("Токен бота (от @BotFather):", this);
    tokenLabel->setStyleSheet("color: #a0a0a0; font-size: 12px; font-weight: bold;");

    QHBoxLayout *tokenInputLayout = new QHBoxLayout();
    tokenInputLayout->setSpacing(8);
    m_tokenInput = new QLineEdit(this);
    m_tokenInput->setPlaceholderText("123456789:ABCdefGhIJKlmNoPQRsTUVwxyZ");
    m_tokenInput->setEchoMode(QLineEdit::PasswordEchoOnEdit);

    if (m_settingsManager) {
        m_tokenInput->setText(m_settingsManager->GetTelegramToken());
    }

    m_saveTokenBtn = new QPushButton("Сохранить", this);
    connect(m_saveTokenBtn, &QPushButton::clicked, this, &TelegramDialog::onSaveTokenClicked);

    tokenInputLayout->addWidget(m_tokenInput);
    tokenInputLayout->addWidget(m_saveTokenBtn);

    tokenLayout->addWidget(tokenLabel);
    tokenLayout->addLayout(tokenInputLayout);
    mainLayout->addLayout(tokenLayout);

    // --- Секция 2: Статус и управление включением ---
    QHBoxLayout *statusControlLayout = new QHBoxLayout();
    statusControlLayout->setSpacing(10);

    QHBoxLayout *statusIndicatorLayout = new QHBoxLayout();
    statusIndicatorLayout->setSpacing(6);
    m_statusIndicator = new QLabel("●", this);
    m_statusIndicator->setStyleSheet("color: #e74c3c; font-size: 16px;");

    m_statusTextLabel = new QLabel("Статус: Выключен", this);
    m_statusTextLabel->setStyleSheet("color: #ffffff; font-weight: 500;");

    statusIndicatorLayout->addWidget(m_statusIndicator);
    statusIndicatorLayout->addWidget(m_statusTextLabel);
    statusIndicatorLayout->addStretch();

    m_toggleBotBtn = new QPushButton("Включить бота", this);
    m_toggleBotBtn->setMinimumWidth(130);
    connect(m_toggleBotBtn, &QPushButton::clicked, this, &TelegramDialog::onToggleBotClicked);

    statusControlLayout->addLayout(statusIndicatorLayout);
    statusControlLayout->addWidget(m_toggleBotBtn);
    mainLayout->addLayout(statusControlLayout);

    // --- Секция 3: Автозапуск ---
    m_autoStartCheckBox = new QCheckBox("Включать бота при запуске программы", this);
    if (m_settingsManager) {
        m_autoStartCheckBox->setChecked(m_settingsManager->GetTelegramAutoStart());
    }
    connect(m_autoStartCheckBox, &QCheckBox::toggled, this, &TelegramDialog::onAutoStartToggled);
    mainLayout->addWidget(m_autoStartCheckBox);

    // --- Секция 4: Код авторизации ---
    QFrame *codeContainer = new QFrame(this);
    codeContainer->setObjectName("codeContainer");
    codeContainer->setStyleSheet(
        "QFrame#codeContainer { "
        "  background-color: #171a20; "
        "  border: 1px solid #2d323b; "
        "  border-radius: 8px; "
        "  padding: 8px; "
        "}"
    );
    QVBoxLayout *codeLayout = new QVBoxLayout(codeContainer);
    codeLayout->setContentsMargins(12, 8, 12, 8);
    codeLayout->setSpacing(6);

    QLabel *codeDescLabel = new QLabel("Код для авторизации в боте:", codeContainer);
    codeDescLabel->setStyleSheet("color: #a0a0a0; font-size: 12px;");

    m_authCodeLabel = new QLabel("----------", codeContainer);
    m_authCodeLabel->setAlignment(Qt::AlignCenter);
    m_authCodeLabel->setStyleSheet(
        "font-size: 20px; font-weight: bold; letter-spacing: 4px; color: #4da6ff; "
        "background-color: #20242c; border: 1px dashed #3d434f; border-radius: 6px; padding: 4px;"
    );

    m_timerLabel = new QLabel("Обновление через: 05:00", codeContainer);
    m_timerLabel->setAlignment(Qt::AlignCenter);
    m_timerLabel->setStyleSheet("color: #7f8c8d; font-size: 11px;");

    QHBoxLayout *codeActionsLayout = new QHBoxLayout();
    codeActionsLayout->setSpacing(8);

    m_copyCodeBtn = new QPushButton("Скопировать код", codeContainer);
    connect(m_copyCodeBtn, &QPushButton::clicked, this, &TelegramDialog::onCopyCodeClicked);

    m_openBotBtn = new QPushButton("Открыть бота", codeContainer);
    connect(m_openBotBtn, &QPushButton::clicked, this, &TelegramDialog::onOpenBotClicked);

    codeActionsLayout->addWidget(m_copyCodeBtn);
    codeActionsLayout->addWidget(m_openBotBtn);

    codeLayout->addWidget(codeDescLabel);
    codeLayout->addWidget(m_authCodeLabel);
    codeLayout->addWidget(m_timerLabel);
    codeLayout->addLayout(codeActionsLayout);

    mainLayout->addWidget(codeContainer);

    // --- Секция 5: Подключенные пользователи ---
    m_subscribersCountLabel = new QLabel("Подключенных пользователей: 0", this);
    m_subscribersCountLabel->setStyleSheet("color: #7f8c8d; font-size: 12px;");
    mainLayout->addWidget(m_subscribersCountLabel);

    // --- Секция 6: Вкладки внизу (Лог и Список клиентов) ---
    m_tabWidget = new QTabWidget(this);

    // Вкладка «Лог»
    QWidget *logTab = new QWidget();
    QVBoxLayout *logTabLayout = new QVBoxLayout(logTab);
    logTabLayout->setContentsMargins(8, 8, 8, 8);
    logTabLayout->setSpacing(6);

    m_logEdit = new QPlainTextEdit(logTab);
    m_logEdit->setReadOnly(true);
    m_logEdit->setPlaceholderText("Здесь будут отображаться события бота...");

    QHBoxLayout *logBottomLayout = new QHBoxLayout();
    logBottomLayout->addStretch();
    m_clearLogBtn = new QPushButton("Очистить лог", logTab);
    m_clearLogBtn->setStyleSheet("font-size: 11px; padding: 4px 10px;");
    connect(m_clearLogBtn, &QPushButton::clicked, this, &TelegramDialog::onClearLogClicked);
    logBottomLayout->addWidget(m_clearLogBtn);

    logTabLayout->addWidget(m_logEdit);
    logTabLayout->addLayout(logBottomLayout);
    m_tabWidget->addTab(logTab, "Лог событий");

    // Вкладка «Список клиентов»
    QWidget *clientsTab = new QWidget();
    QVBoxLayout *clientsTabLayout = new QVBoxLayout(clientsTab);
    clientsTabLayout->setContentsMargins(8, 8, 8, 8);
    clientsTabLayout->setSpacing(6);

    m_clientsTable = new QTableWidget(0, 4, clientsTab);
    m_clientsTable->setHorizontalHeaderLabels(QStringList() << "Chat ID" << "Имя" << "Username" << "Дата авторизации");
    m_clientsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_clientsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_clientsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_clientsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_clientsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_clientsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_clientsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_clientsTable->verticalHeader()->setVisible(false);

    QHBoxLayout *clientActionsLayout = new QHBoxLayout();
    m_refreshClientsBtn = new QPushButton("🔄 Обновить", clientsTab);
    m_refreshClientsBtn->setStyleSheet("font-size: 11px; padding: 4px 10px;");
    connect(m_refreshClientsBtn, &QPushButton::clicked, this, &TelegramDialog::onRefreshClientsClicked);

    m_unbindClientBtn = new QPushButton("🗑️ Отвязать пользователя", clientsTab);
    m_unbindClientBtn->setStyleSheet(
        "QPushButton { font-size: 11px; padding: 4px 10px; background-color: #7f1d1d; border-color: #991b1b; } "
        "QPushButton:hover { background-color: #991b1b; }"
    );
    connect(m_unbindClientBtn, &QPushButton::clicked, this, &TelegramDialog::onUnbindClientClicked);

    clientActionsLayout->addWidget(m_refreshClientsBtn);
    clientActionsLayout->addStretch();
    clientActionsLayout->addWidget(m_unbindClientBtn);

    clientsTabLayout->addWidget(m_clientsTable);
    clientsTabLayout->addLayout(clientActionsLayout);
    m_tabWidget->addTab(clientsTab, "Список клиентов");

    mainLayout->addWidget(m_tabWidget, 1);
}

void TelegramDialog::updateStatusDisplay(BotState state, const QString &text)
{
    m_statusTextLabel->setText("Статус: " + text);

    switch (state) {
    case BotState::Running:
        m_statusIndicator->setStyleSheet("color: #2ecc71; font-size: 16px;"); // зеленый
        m_toggleBotBtn->setText("Выключить бота");
        m_toggleBotBtn->setStyleSheet(
            "QPushButton { background-color: #c0392b; color: white; border: 1px solid #e74c3c; border-radius: 6px; padding: 7px 14px; } "
            "QPushButton:hover { background-color: #e74c3c; }"
        );
        break;
    case BotState::Connecting:
    case BotState::Reconnecting:
        m_statusIndicator->setStyleSheet("color: #f39c12; font-size: 16px;"); // оранжевый / желтый
        m_toggleBotBtn->setText("Выключить бота");
        m_toggleBotBtn->setStyleSheet(
            "QPushButton { background-color: #d35400; color: white; border: 1px solid #e67e22; border-radius: 6px; padding: 7px 14px; } "
            "QPushButton:hover { background-color: #e67e22; }"
        );
        break;
    case BotState::Stopped:
    default:
        m_statusIndicator->setStyleSheet("color: #e74c3c; font-size: 16px;"); // красный
        m_toggleBotBtn->setText("Включить бота");
        m_toggleBotBtn->setStyleSheet(
            "QPushButton { background-color: #27ae60; color: white; border: 1px solid #2ecc71; border-radius: 6px; padding: 7px 14px; } "
            "QPushButton:hover { background-color: #2ecc71; }"
        );
        break;
    }
}

void TelegramDialog::updateSubscribersCount()
{
    if (m_dbManager) {
        int count = m_dbManager->GetTelegramSubscribers().size();
        m_subscribersCountLabel->setText(QString("Подключенных пользователей: %1").arg(count));
    }
}

void TelegramDialog::onToggleBotClicked()
{
    if (!m_bot) return;

    if (m_bot->IsRunning()) {
        m_bot->StopBot();
    } else {
        QString token = m_tokenInput->text().trimmed();
        if (token.isEmpty()) {
            QMessageBox::warning(this, "Telegram Бот", "Пожалуйста, введите токен бота.");
            return;
        }
        m_bot->SetToken(token);
        m_bot->StartBot();
    }
}

void TelegramDialog::onSaveTokenClicked()
{
    QString token = m_tokenInput->text().trimmed();
    if (m_bot) {
        m_bot->SetToken(token);
    }
    if (m_settingsManager) {
        m_settingsManager->SetTelegramToken(token);
    }

    m_saveTokenBtn->setText("✓ Сохранено");
    QTimer::singleShot(1500, this, [this]() {
        if (m_saveTokenBtn) {
            m_saveTokenBtn->setText("Сохранить");
        }
    });
}

void TelegramDialog::onAutoStartToggled(bool checked)
{
    if (m_settingsManager) {
        m_settingsManager->SetTelegramAutoStart(checked);
    }
}

void TelegramDialog::onCopyCodeClicked()
{
    if (m_bot) {
        QClipboard *clipboard = QGuiApplication::clipboard();
        clipboard->setText(m_bot->GetAuthCode());

        m_copyCodeBtn->setText("✓ Скопировано!");
        QTimer::singleShot(1500, this, [this]() {
            if (m_copyCodeBtn) {
                m_copyCodeBtn->setText("Скопировать код");
            }
        });
    }
}

void TelegramDialog::onOpenBotClicked()
{
    if (m_bot) {
        QString username = m_bot->GetBotUsername();
        if (!username.isEmpty()) {
            QDesktopServices::openUrl(QUrl(QString("https://t.me/%1").arg(username)));
        } else {
            // Если имя бота еще не определилось (бот выключен), предлагаем открыть t.me
            QDesktopServices::openUrl(QUrl("https://t.me/"));
        }
    }
}

void TelegramDialog::onBotStateChanged(BotState state, const QString &statusText)
{
    updateStatusDisplay(state, statusText);
}

void TelegramDialog::onBotAuthCodeChanged(const QString &newCode, int remainingSeconds)
{
    m_authCodeLabel->setText(newCode);

    int mins = remainingSeconds / 60;
    int secs = remainingSeconds % 60;
    m_timerLabel->setText(QString("Обновление через: %1:%2")
                              .arg(mins, 2, 10, QChar('0'))
                              .arg(secs, 2, 10, QChar('0')));
}

void TelegramDialog::updateClientsTable()
{
    if (!m_dbManager || !m_clientsTable) return;

    QVector<dbmanager::TelegramUserInfo> users = m_dbManager->GetTelegramUsers();
    m_clientsTable->setRowCount(0);

    for (int i = 0; i < users.size(); ++i) {
        const auto &u = users[i];
        m_clientsTable->insertRow(i);

        QTableWidgetItem *itemChatId = new QTableWidgetItem(QString::number(u.chatId));
        QTableWidgetItem *itemName = new QTableWidgetItem(u.firstName);
        QTableWidgetItem *itemUser = new QTableWidgetItem(u.username.isEmpty() ? "—" : "@" + u.username);
        QTableWidgetItem *itemDate = new QTableWidgetItem(u.authDate.isEmpty() ? "—" : u.authDate);

        itemChatId->setTextAlignment(Qt::AlignCenter);
        itemUser->setTextAlignment(Qt::AlignCenter);
        itemDate->setTextAlignment(Qt::AlignCenter);

        m_clientsTable->setItem(i, 0, itemChatId);
        m_clientsTable->setItem(i, 1, itemName);
        m_clientsTable->setItem(i, 2, itemUser);
        m_clientsTable->setItem(i, 3, itemDate);
    }

    m_subscribersCountLabel->setText(QString("Подключенных пользователей: %1").arg(users.size()));
}

void TelegramDialog::onRefreshClientsClicked()
{
    updateClientsTable();
}

void TelegramDialog::onUnbindClientClicked()
{
    if (!m_clientsTable) return;
    int row = m_clientsTable->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "Отвязка пользователя", "Пожалуйста, выберите пользователя в таблице.");
        return;
    }

    QTableWidgetItem *chatIdItem = m_clientsTable->item(row, 0);
    QTableWidgetItem *nameItem = m_clientsTable->item(row, 1);
    if (!chatIdItem) return;

    qint64 chatId = chatIdItem->text().toLongLong();
    QString name = nameItem ? nameItem->text() : QString::number(chatId);

    int reply = QMessageBox::question(this, "Отвязка пользователя",
                                      QString("Вы уверены, что хотите отвязать пользователя %1 (chat_id: %2)?\nОн перестанет получать уведомления о дедлайнах.").arg(name).arg(chatId),
                                      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        if (m_bot) {
            m_bot->UnbindUser(chatId);
        } else if (m_dbManager) {
            m_dbManager->RemoveTelegramUser(chatId);
        }
        updateClientsTable();
        updateSubscribersCount();
    }
}

void TelegramDialog::onClearLogClicked()
{
    if (m_logEdit) {
        m_logEdit->clear();
    }
}

void TelegramDialog::onLogMessage(const QString &msg)
{
    if (m_logEdit) {
        m_logEdit->appendPlainText(msg);
    }
}

void TelegramDialog::onUserSubscribed(const QString &username, qint64 chatId)
{
    Q_UNUSED(username);
    Q_UNUSED(chatId);
    updateSubscribersCount();
    updateClientsTable();
}

void TelegramDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        this->close();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void TelegramDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QWidget *child = childAt(event->position().toPoint());
        if (child && (qobject_cast<QPushButton*>(child) ||
                      qobject_cast<QLineEdit*>(child) ||
                      qobject_cast<QTabBar*>(child) ||
                      qobject_cast<QPlainTextEdit*>(child) ||
                      qobject_cast<QTableWidget*>(child))) {
            QDialog::mousePressEvent(event);
            return;
        }
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void TelegramDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void TelegramDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    event->accept();
}


