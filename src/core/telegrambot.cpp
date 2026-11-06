#include "telegrambot.h"
#include <QRandomGenerator>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>

TelegramBot::TelegramBot(dbmanager *dbMgr, settingsmanager *settingsMgr, QObject *parent)
    : QObject(parent)
    , m_dbManager(dbMgr)
    , m_settingsManager(settingsMgr)
{
    m_networkManager = new QNetworkAccessManager(this);

    // Таймер обратного отсчета кода авторизации (тикает каждую секунду)
    m_authCodeTimer = new QTimer(this);
    m_authCodeTimer->setInterval(1000);
    connect(m_authCodeTimer, &QTimer::timeout, this, &TelegramBot::onCodeTimerTick);

    // Таймер повтора опроса при ошибках
    m_pollRetryTimer = new QTimer(this);
    m_pollRetryTimer->setSingleShot(true);
    connect(m_pollRetryTimer, &QTimer::timeout, this, &TelegramBot::pollUpdates);

    // Сторожевой таймер (watchdog) для выявления зависших сетевых соединений
    m_watchdogTimer = new QTimer(this);
    m_watchdogTimer->setSingleShot(true);
    connect(m_watchdogTimer, &QTimer::timeout, this, &TelegramBot::onPollTimeout);

    // Загрузка сохраненного токена
    if (m_settingsManager) {
        m_token = m_settingsManager->GetTelegramToken();
    }

    // Первичная генерация кода авторизации
    GenerateNewAuthCode();
    m_authCodeTimer->start();
}

TelegramBot::~TelegramBot()
{
    StopBot();
}

void TelegramBot::SetToken(const QString &token)
{
    m_token = token.trimmed();
    if (m_settingsManager) {
        m_settingsManager->SetTelegramToken(m_token);
    }
}

QString TelegramBot::GetToken() const
{
    return m_token;
}

bool TelegramBot::IsRunning() const
{
    return m_isRunning;
}

int TelegramBot::GetRemainingCodeSeconds() const
{
    if (!m_codeExpiresAt.isValid()) return 0;
    int secs = QDateTime::currentDateTime().secsTo(m_codeExpiresAt);
    return qMax(0, secs);
}

void TelegramBot::GenerateNewAuthCode()
{
    static const char charset[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    const int charsetSize = sizeof(charset) - 1;

    QString code;
    code.reserve(10);
    for (int i = 0; i < 10; ++i) {
        quint32 index = QRandomGenerator::global()->bounded(charsetSize);
        code.append(charset[index]);
    }

    m_authCode = code;
    m_codeExpiresAt = QDateTime::currentDateTime().addSecs(300); // 5 минут

    emit authCodeChanged(m_authCode, 300);
}

void TelegramBot::onCodeTimerTick()
{
    int remaining = GetRemainingCodeSeconds();
    if (remaining <= 0) {
        GenerateNewAuthCode();
    } else {
        emit authCodeChanged(m_authCode, remaining);
    }
}

void TelegramBot::UnbindUser(qint64 chatId)
{
    if (m_dbManager) {
        m_dbManager->RemoveTelegramUser(chatId);
    }
    log(QString("Пользователь отвязан через интерфейс (chat_id: %1)").arg(chatId));
    QJsonObject rm;
    rm["remove_keyboard"] = true;
    sendMessage(chatId, "Ваш аккаунт был отвязан от AirTask через настройки приложения.", rm);
}

void TelegramBot::log(const QString &text)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString formatted = QString("[%1] %2").arg(timestamp, text);
    m_logHistory.append(formatted);
    if (m_logHistory.size() > 500) {
        m_logHistory.removeFirst();
    }
    qDebug().noquote() << "[TelegramBot]" << formatted;
    emit logMessage(formatted);
}

void TelegramBot::StartBot()
{
    if (m_isRunning) return;

    if (m_token.isEmpty()) {
        setState(BotState::Stopped, "Токен бота не указан");
        return;
    }

    m_isRunning = true;
    m_consecutiveErrors = 0;
    setState(BotState::Connecting, "Подключение к Telegram...");
    log("Запуск бота...");

    fetchBotInfo();
    pollUpdates();
}

void TelegramBot::StopBot()
{
    bool wasRunning = m_isRunning;
    m_isRunning = false;
    m_consecutiveErrors = 0;
    m_taskCreationSessions.clear();

    if (m_pollRetryTimer->isActive()) {
        m_pollRetryTimer->stop();
    }

    if (m_watchdogTimer->isActive()) {
        m_watchdogTimer->stop();
    }

    if (m_currentPollReply) {
        m_currentPollReply->abort();
        m_currentPollReply->deleteLater();
        m_currentPollReply = nullptr;
    }

    setState(BotState::Stopped, "Выключен");
    if (wasRunning) {
        log("Бот остановлен.");
    }
}

void TelegramBot::setState(BotState state, const QString &statusText)
{
    m_state = state;
    m_statusText = statusText;
    emit stateChanged(m_state, m_statusText);
}

void TelegramBot::fetchBotInfo()
{
    if (m_token.isEmpty()) return;

    QUrl url(QString("https://api.telegram.org/bot%1/getMe").arg(m_token));
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject root = doc.object();
            if (root.value("ok").toBool()) {
                QJsonObject user = root.value("result").toObject();
                m_botUsername = user.value("username").toString();
                qDebug() << "TelegramBot подключен к @" << m_botUsername;
                if (m_isRunning) {
                    setState(BotState::Running, "Включен (работает)");
                    log(QString("Бот @%1 успешно подключен к Telegram. Статус: Включен (работает).").arg(m_botUsername));
                }
            } else {
                QString desc = root.value("description").toString();
                log(QString("Ошибка getMe: %1").arg(desc));
            }
        } else {
            int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString errStr = reply->errorString();
            if (httpCode == 401) {
                log("Ошибка запуска: Неверный токен бота (401).");
                setState(BotState::Stopped, "Неверный токен бота (401)");
                m_isRunning = false;
            } else {
                log(QString("Сетевая ошибка при проверке бота: %1 (HTTP %2)").arg(errStr).arg(httpCode));
            }
        }
    });
}

void TelegramBot::pollUpdates()
{
    if (!m_isRunning || m_token.isEmpty()) return;

    if (m_currentPollReply) {
        return; // Уже выполняется запрос
    }

    // Long polling: запрашиваем обновления с таймаутом 25 секунд
    QUrl url(QString("https://api.telegram.org/bot%1/getUpdates").arg(m_token));
    QUrlQuery query;
    query.addQueryItem("offset", QString::number(m_lastUpdateId));
    query.addQueryItem("timeout", "25");
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setTransferTimeout(30000); // 30 секунд лимит Qt на соединение

    m_currentPollReply = m_networkManager->get(request);
    connect(m_currentPollReply, &QNetworkReply::finished, this, &TelegramBot::onPollFinished);

    // Запускаем watchdog-таймер на 35 секунд
    m_watchdogTimer->start(35000);
}

void TelegramBot::onPollFinished()
{
    m_watchdogTimer->stop();

    if (!m_currentPollReply) return;

    QNetworkReply *reply = m_currentPollReply;
    m_currentPollReply = nullptr;
    reply->deleteLater();

    if (!m_isRunning) return;

    if (reply->error() != QNetworkReply::NoError) {
        // Ошибка сети или соединения
        int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (httpCode == 401) {
            setState(BotState::Reconnecting, "Неверный токен бота (401). Переподключение...");
            log("Ошибка long polling: Неверный токен бота (401).");
            scheduleRetry(5000);
            return;
        }

        m_consecutiveErrors++;

        if (m_consecutiveErrors > MAX_RECONNECT_ATTEMPTS) {
            log(QString("Не удалось восстановить связь с Telegram после %1 попыток. Перезапуск программы...").arg(MAX_RECONNECT_ATTEMPTS));
            setState(BotState::Reconnecting, "Перезапуск программы...");
            restartApplication();
            return;
        }

        int delaySec = RECONNECT_DELAY_SEC;
        setState(BotState::Reconnecting, QString("Ошибка связи. Переподключение (%1/%2) через %3 с...")
                 .arg(m_consecutiveErrors).arg(MAX_RECONNECT_ATTEMPTS).arg(delaySec));
        log(QString("Ошибка связи long polling: %1 (HTTP %2). Переподключение (%3/%4) через %5 с...")
            .arg(reply->errorString()).arg(httpCode).arg(m_consecutiveErrors).arg(MAX_RECONNECT_ATTEMPTS).arg(delaySec));
        scheduleRetry(delaySec * 1000);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    if (!root.value("ok").toBool()) {
        QString desc = root.value("description").toString();
        m_consecutiveErrors++;

        if (m_consecutiveErrors > MAX_RECONNECT_ATTEMPTS) {
            log(QString("Не удалось восстановить связь с Telegram после %1 попыток (ошибка API: %2). Перезапуск программы...")
                .arg(MAX_RECONNECT_ATTEMPTS).arg(desc));
            setState(BotState::Reconnecting, "Перезапуск программы...");
            restartApplication();
            return;
        }

        int delaySec = RECONNECT_DELAY_SEC;
        setState(BotState::Reconnecting, QString("Ошибка Telegram: %1. Переподключение (%2/%3) через %4 с...")
                 .arg(desc).arg(m_consecutiveErrors).arg(MAX_RECONNECT_ATTEMPTS).arg(delaySec));
        log(QString("Ошибка Telegram API: %1. Переподключение (%2/%3) через %4 с...")
            .arg(desc).arg(m_consecutiveErrors).arg(MAX_RECONNECT_ATTEMPTS).arg(delaySec));
        scheduleRetry(delaySec * 1000);
        return;
    }

    // Успешный ответ
    m_consecutiveErrors = 0;

    if (m_state != BotState::Running) {
        setState(BotState::Running, "Включен (работает)");
    }

    QJsonArray updates = root.value("result").toArray();
    for (const QJsonValue &val : updates) {
        QJsonObject upd = val.toObject();
        processUpdate(upd);
    }

    // Сразу запрашиваем следующую пачку
    if (m_isRunning) {
        QTimer::singleShot(50, this, &TelegramBot::pollUpdates);
    }
}

void TelegramBot::onPollTimeout()
{
    // Watchdog сработал: запрос подвис
    if (m_currentPollReply) {
        log("Сработал watchdog: таймаут соединения long polling. Перезапуск сетевого запроса...");
        QNetworkReply *reply = m_currentPollReply;
        m_currentPollReply = nullptr;
        reply->abort();
        reply->deleteLater();
    }

    m_consecutiveErrors++;

    if (m_consecutiveErrors > MAX_RECONNECT_ATTEMPTS) {
        log(QString("Не удалось восстановить связь с Telegram после %1 попыток (таймаут). Перезапуск программы...")
            .arg(MAX_RECONNECT_ATTEMPTS));
        setState(BotState::Reconnecting, "Перезапуск программы...");
        restartApplication();
        return;
    }

    int delaySec = RECONNECT_DELAY_SEC;
    setState(BotState::Reconnecting, QString("Таймаут соединения. Переподключение (%1/%2) через %3 с...")
             .arg(m_consecutiveErrors).arg(MAX_RECONNECT_ATTEMPTS).arg(delaySec));
    scheduleRetry(delaySec * 1000);
}


void TelegramBot::scheduleRetry(int delayMs)
{
    if (!m_isRunning) return;
    m_pollRetryTimer->start(delayMs);
}

void TelegramBot::restartApplication()
{
    log("Выполняется перезапуск программы...");
    StopBot();

    QString appPath = QCoreApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();
    if (!args.isEmpty()) {
        args.removeFirst();
    }

    bool started = QProcess::startDetached(appPath, args, QCoreApplication::applicationDirPath());
    if (!started) {
        log("Критическая ошибка: не удалось запустить новый процесс приложения при перезапуске.");
        return;
    }

    QCoreApplication::quit();
}

void TelegramBot::processUpdate(const QJsonObject &updateObj)
{
    qint64 updateId = updateObj.value("update_id").toInteger();
    if (updateId >= m_lastUpdateId) {
        m_lastUpdateId = updateId + 1;
    }

    if (updateObj.contains("message")) {
        processMessage(updateObj.value("message").toObject());
    } else if (updateObj.contains("callback_query")) {
        processCallbackQuery(updateObj.value("callback_query").toObject());
    }
}

QJsonObject TelegramBot::getMainReplyKeyboard() const
{
    QJsonArray row1;
    QJsonObject b1; b1["text"] = "📋 Задачи";
    QJsonObject b2; b2["text"] = "➕ Добавить задачу";
    row1.append(b1);
    row1.append(b2);

    QJsonArray row2;
    QJsonObject b3; b3["text"] = "ℹ️ Статус";
    QJsonObject b4; b4["text"] = "🛑 Стоп";
    row2.append(b3);
    row2.append(b4);

    QJsonArray keyboard;
    keyboard.append(row1);
    keyboard.append(row2);

    QJsonObject markup;
    markup["keyboard"] = keyboard;
    markup["resize_keyboard"] = true;
    markup["is_persistent"] = true;
    return markup;
}

QJsonObject TelegramBot::getCancelReplyKeyboard() const
{
    QJsonArray row1;
    QJsonObject b1; b1["text"] = "❌ Отмена";
    row1.append(b1);

    QJsonArray keyboard;
    keyboard.append(row1);

    QJsonObject markup;
    markup["keyboard"] = keyboard;
    markup["resize_keyboard"] = true;
    markup["one_time_keyboard"] = false;
    return markup;
}

void TelegramBot::cancelTaskCreation(qint64 chatId, bool notifyUser)
{
    m_taskCreationSessions.remove(chatId);
    if (notifyUser) {
        log(QString("Создание задачи отменено (chat_id: %1)").arg(chatId));
        sendMessage(chatId, "❌ Создание задачи отменено.");
    }
}

void TelegramBot::handleTaskCreationStep(qint64 chatId, const QString &text)
{
    if (!m_taskCreationSessions.contains(chatId)) return;
    TaskCreationSession &session = m_taskCreationSessions[chatId];

    switch (session.step) {
    case TaskCreationStep::WaitingForTitle: {
        QString title = text.trimmed();
        if (title.isEmpty()) {
            sendMessage(chatId, "⚠️ Название задачи не может быть пустым. Пожалуйста, введите название задачи (или «Отмена»):");
            return;
        }
        session.title = title;
        session.step = TaskCreationStep::WaitingForTags;
        sendMessage(chatId,
                    "🏷️ <b>Шаг 2 из 4: Метки/теги</b>\n\n"
                    "Введите метки/теги через запятую (или отправьте <code>-</code>, если меток нет):");
        break;
    }

    case TaskCreationStep::WaitingForTags: {
        QString tags = text.trimmed();
        if (tags == "-" || tags == "—" || tags.compare("нет", Qt::CaseInsensitive) == 0) {
            session.tags = "";
        } else {
            session.tags = tags;
        }
        session.step = TaskCreationStep::WaitingForCategory;

        QVector<projects> projs;
        if (m_dbManager) {
            m_dbManager->UpdateProjects(projs);
        }
        session.availableProjects = projs;

        QString catMsg = "📂 <b>Шаг 3 из 4: Категория</b>\n\nВыберите категорию (введите номер или название):\n";
        catMsg += "0. Без категории\n";
        for (int i = 0; i < projs.size(); ++i) {
            catMsg += QString("%1. %2\n").arg(i + 1).arg(projs[i].name.toHtmlEscaped());
        }
        catMsg += "\n<i>Вы также можете ввести название новой категории, и она будет создана автоматически.</i>";

        sendMessage(chatId, catMsg);
        break;
    }

    case TaskCreationStep::WaitingForCategory: {
        QString catInput = text.trimmed();
        bool ok = false;
        int num = catInput.toInt(&ok);

        if (ok) {
            if (num == 0) {
                session.projectId = -1;
                session.projectName = "Без категории";
            } else if (num >= 1 && num <= session.availableProjects.size()) {
                session.projectId = session.availableProjects[num - 1].id;
                session.projectName = session.availableProjects[num - 1].name;
            } else {
                sendMessage(chatId, QString("⚠️ Номер категории должен быть от 0 до %1. Попробуйте еще раз:").arg(session.availableProjects.size()));
                return;
            }
        } else {
            if (catInput == "-" || catInput.compare("без категории", Qt::CaseInsensitive) == 0 || catInput.compare("все", Qt::CaseInsensitive) == 0) {
                session.projectId = -1;
                session.projectName = "Без категории";
            } else {
                if (m_dbManager) {
                    session.projectId = m_dbManager->GetFindProjectOrCreateID(catInput);
                    session.projectName = catInput;
                } else {
                    session.projectId = -1;
                    session.projectName = catInput;
                }
            }
        }

        session.step = TaskCreationStep::WaitingForDeadline;
        sendMessage(chatId,
                    "⏰ <b>Шаг 4 из 4: Дедлайн</b>\n\n"
                    "Введите дедлайн в формате <b>ДД.ММ.ГГГГ ЧЧ:ММ</b> (например: <code>05.11.2026 18:00</code>)\n"
                    "или отправьте <code>-</code>, если задача без дедлайна:");
        break;
    }

    case TaskCreationStep::WaitingForDeadline: {
        QString dlInput = text.trimmed();
        QDateTime deadline;

        if (dlInput != "-" && dlInput != "—" && dlInput.compare("нет", Qt::CaseInsensitive) != 0) {
            static const QStringList formats = {
                "dd.MM.yyyy HH:mm",
                "dd.MM.yyyy H:mm",
                "dd.MM.yyyy HH:mm:ss",
                "dd.MM.yyyy",
                "yyyy-MM-dd HH:mm",
                "yyyy-MM-dd"
            };

            for (const QString &fmt : formats) {
                QDateTime dt = QDateTime::fromString(dlInput, fmt);
                if (dt.isValid()) {
                    deadline = dt;
                    break;
                }
            }

            if (!deadline.isValid()) {
                sendMessage(chatId,
                            "⚠️ <b>Неверный формат даты!</b>\n\n"
                            "Пожалуйста, введите дату в формате <b>ДД.ММ.ГГГГ ЧЧ:ММ</b> (например: <code>05.11.2026 18:00</code>)\n"
                            "или отправьте <code>-</code>, если без дедлайна (или «Отмена» для отмены):");
                return;
            }
        }

        task newTask;
        newTask.title = session.title;
        newTask.tags = session.tags;
        newTask.project_id = session.projectId;
        newTask.deadline = deadline;
        newTask.is_completed = false;

        QString title = session.title;
        QString projectName = session.projectName.trimmed().isEmpty() ? "Без категории" : session.projectName;

        if (m_dbManager) {
            m_dbManager->AddTaskToDB(newTask);
        }

        QString confirmMsg = QString("✅ Задача <b>«%1»</b> успешно создана!").arg(title.toHtmlEscaped());
        log(QString("Создана задача «%1» (категория: «%2») через Telegram от chat_id %3").arg(title, projectName).arg(chatId));

        m_taskCreationSessions.remove(chatId);
        sendMessage(chatId, confirmMsg);
        NotifyTaskCreated(newTask, projectName);
        emit taskAdded();
        break;
    }
    default:
        m_taskCreationSessions.remove(chatId);
        break;
    }
}

void TelegramBot::processMessage(const QJsonObject &msgObj)
{
    QJsonObject chat = msgObj.value("chat").toObject();
    qint64 chatId = chat.value("id").toInteger();
    QString text = msgObj.value("text").toString().trimmed();

    QJsonObject from = msgObj.value("from").toObject();
    QString username = from.value("username").toString();
    QString firstName = from.value("first_name").toString();

    if (chatId == 0 || text.isEmpty()) return;

    QString userDisplay = firstName;
    if (!username.isEmpty()) {
        userDisplay += QString(" (@%1)").arg(username);
    }
    log(QString("Сообщение от %1, chat_id %2: \"%3\"").arg(userDisplay).arg(chatId).arg(text));

    bool isAuthorized = m_dbManager ? m_dbManager->IsTelegramUserAuthorized(chatId) : false;

    QString trimmedText = text.trimmed();
    QString lowerText = trimmedText.toLower();
    QString upperText = trimmedText.toUpper();
    QString cleanCode = m_authCode.toUpper();

    // Проверка отмены пошагового создания или редактирования задачи
    bool isCancel = (lowerText == "отмена" ||
                     trimmedText == "❌ Отмена" ||
                     lowerText == "/cancel");
    if (isCancel) {
        if (m_taskCreationSessions.contains(chatId)) {
            cancelTaskCreation(chatId, true);
            return;
        }
        if (m_taskEditSessions.contains(chatId)) {
            m_taskEditSessions.remove(chatId);
            sendMessage(chatId, "🚫 Редактирование задачи отменено.", getMainReplyKeyboard());
            return;
        }
    }

    // Если пользователь находится в процессе создания задачи
    if (m_taskCreationSessions.contains(chatId)) {
        // Если пришла стандартная команда или кнопка верхнего уровня, прерываем создание и выполняем команду
        bool isTopCommand = (trimmedText.startsWith("/") ||
                             trimmedText == "📋 Задачи" || lowerText == "задачи" ||
                             trimmedText == "➕ Добавить задачу" || lowerText == "добавить задачу" ||
                             trimmedText == "ℹ️ Статус" || lowerText == "статус" ||
                             trimmedText == "🛑 Стоп" || lowerText == "стоп");
        if (isTopCommand) {
            cancelTaskCreation(chatId, false);
        } else {
            handleTaskCreationStep(chatId, trimmedText);
            return;
        }
    }

    // Если пользователь находится в процессе редактирования задачи
    if (m_taskEditSessions.contains(chatId)) {
        bool isTopCommand = (trimmedText.startsWith("/") ||
                             trimmedText == "📋 Задачи" || lowerText == "задачи" ||
                             trimmedText == "➕ Добавить задачу" || lowerText == "добавить задачу" ||
                             trimmedText == "ℹ️ Статус" || lowerText == "статус" ||
                             trimmedText == "🛑 Стоп" || lowerText == "стоп");
        if (isTopCommand) {
            m_taskEditSessions.remove(chatId);
        } else {
            handleTaskEditStep(chatId, trimmedText);
            return;
        }
    }

    // Проверка совпадения кода (независимо от регистра)
    bool isCodeMatch = false;
    if (upperText == cleanCode) {
        isCodeMatch = true;
    } else if (upperText.startsWith("/START ") && upperText.mid(7).trimmed() == cleanCode) {
        isCodeMatch = true;
    } else if (upperText.startsWith("/AUTH ") && upperText.mid(6).trimmed() == cleanCode) {
        isCodeMatch = true;
    }

    if (isCodeMatch) {
        if (m_dbManager) {
            m_dbManager->AddTelegramUser(chatId, username, firstName);
        }
        log(QString("Пользователь успешно авторизован: %1 (chat_id: %2)").arg(userDisplay).arg(chatId));
        sendMessage(chatId,
                    "🎉 <b>Авторизация успешна!</b>\n\n"
                    "Вы успешно подключили AirTask к Telegram. Теперь вам будут приходить напоминания о дедлайнах задач.\n\n"
                    "Используйте кнопки меню внизу для быстрого управления задачами:\n"
                    "• <b>📋 Задачи</b> — список невыполненных задач\n"
                    "• <b>➕ Добавить задачу</b> — пошаговое создание задачи\n"
                    "• <b>ℹ️ Статус</b> — статус подключения\n"
                    "• <b>🛑 Стоп</b> — отключить уведомления");

        emit userSubscribed(username.isEmpty() ? firstName : "@" + username, chatId);
        return;
    }

    if (lowerText == "/start") {
        if (isAuthorized) {
            sendMessage(chatId,
                        "✅ <b>Вы уже подключены к AirTask!</b>\n\n"
                        "Уведомления о дедлайнах активны.\n\n"
                        "Используйте кнопки меню внизу экрана для управления задачами:\n"
                        "• <b>📋 Задачи</b> — список актуальных задач\n"
                        "• <b>➕ Добавить задачу</b> — создать новую задачу\n"
                        "• <b>ℹ️ Статус</b> — статус подключения\n"
                        "• <b>🛑 Стоп</b> — отключить уведомления");
        } else {
            sendMessage(chatId,
                        "👋 <b>Добро пожаловать в AirTask!</b>\n\n"
                        "Чтобы получать уведомления о дедлайнах и добавлять задачи, отправьте мне <b>10-значный код авторизации</b> из приложения AirTask.\n"
                        "<i>(Нажмите иконку Telegram в шапке приложения AirTask. Код обновляется каждые 5 минут).</i>");
        }
        return;
    }

    if (lowerText == "/status" || trimmedText == "ℹ️ Статус" || lowerText == "статус") {
        if (isAuthorized) {
            sendMessage(chatId, "✅ <b>Статус:</b> Вы авторизованы. Уведомления о дедлайнах включены.");
        } else {
            sendMessage(chatId, "⚠️ <b>Статус:</b> Вы не авторизованы.\nОтправьте 10-значный код из приложения AirTask.");
        }
        return;
    }

    if (lowerText == "/tasks" || trimmedText == "📋 Задачи" || lowerText == "задачи") {
        if (!isAuthorized) {
            sendMessage(chatId, "⚠️ Сначала авторизуйтесь с помощью кода из приложения AirTask.");
            return;
        }

        if (m_dbManager) {
            QVector<task> tasks;
            m_dbManager->UpdateTasks(tasks);
            QString taskList;
            int count = 0;
            QVector<int> displayedIds;
            for (const task &t : tasks) {
                if (!t.is_completed) {
                    count++;
                    displayedIds.append(t.id);
                    QString dlStr = t.deadline.isValid()
                        ? QString(" <i>(дедлайн: %1)</i>").arg(t.deadline.toString("dd.MM.yyyy HH:mm"))
                        : "";
                    QString tagStr = !t.tags.trimmed().isEmpty()
                        ? QString(" [<code>%1</code>]").arg(t.tags.trimmed().toHtmlEscaped())
                        : "";
                    taskList += QString("\n%1. %2%3%4").arg(count).arg(t.title.toHtmlEscaped(), tagStr, dlStr);
                }
            }

            if (count == 0) {
                m_userLastTaskList.remove(chatId);
                sendMessage(chatId, "👍 У вас нет невыполненных задач!");
            } else {
                m_userLastTaskList[chatId] = displayedIds;
                sendMessage(chatId, QString("📋 <b>Ваши невыполненные задачи (%1):</b>%2\n\n💡 <i>Напишите цифру задачи для просмотра более подробной информации о задаче.</i>")
                                        .arg(count).arg(taskList));
            }
        }
        return;
    }

    if (lowerText == "/add" || trimmedText == "➕ Добавить задачу" || lowerText == "добавить задачу") {
        if (!isAuthorized) {
            sendMessage(chatId, "⚠️ Сначала авторизуйтесь с помощью кода из приложения AirTask.");
            return;
        }

        TaskCreationSession session;
        session.step = TaskCreationStep::WaitingForTitle;
        m_taskCreationSessions[chatId] = session;
        log(QString("Пользователь %1 (chat_id: %2) начал создание задачи").arg(userDisplay).arg(chatId));

        sendMessage(chatId,
                    "📝 <b>Создание новой задачи (Шаг 1 из 4)</b>\n\n"
                    "Введите название задачи:\n"
                    "<i>(или нажмите «Отмена» для выхода)</i>");
        return;
    }

    if (lowerText == "/update") {
        if (!isAuthorized) {
            sendMessage(chatId, "⚠️ Сначала авторизуйтесь с помощью кода из приложения AirTask.");
            return;
        }

        log(QString("Пользователь %1 (chat_id: %2) запросил обновление меню кнопок").arg(userDisplay).arg(chatId));
        sendMessage(chatId, "🔄 <b>Меню кнопок успешно обновлено!</b>\nКнопки быстрого доступа активны внизу экрана.");
        return;
    }

    if (lowerText == "/stop" || trimmedText == "🛑 Стоп" || lowerText == "стоп") {
        if (m_dbManager) {
            m_dbManager->RemoveTelegramUser(chatId);
        }
        log(QString("Пользователь %1 (chat_id: %2) отключил уведомления (/stop)").arg(userDisplay).arg(chatId));
        QJsonObject removeMarkup;
        removeMarkup["remove_keyboard"] = true;
        sendMessage(chatId, "Уведомления AirTask отключены. Чтобы подключиться снова, введите актуальный код авторизации из приложения.", removeMarkup);
        return;
    }

    // Проверка выбора номера задачи из списка
    QString numStr = trimmedText;
    if (numStr.startsWith("#") || numStr.startsWith("№")) {
        numStr = numStr.mid(1).trimmed();
    }
    if (numStr.endsWith(".")) {
        numStr.chop(1);
        numStr = numStr.trimmed();
    }
    bool okNum = false;
    int taskIndex = numStr.toInt(&okNum);
    if (okNum && isAuthorized) {
        if (m_userLastTaskList.contains(chatId)) {
            const QVector<int> &ids = m_userLastTaskList[chatId];
            if (taskIndex >= 1 && taskIndex <= ids.size()) {
                int taskId = ids[taskIndex - 1];
                showTaskDetails(chatId, taskId, taskIndex);
                return;
            } else {
                sendMessage(chatId, QString("⚠️ Задачи с номером %1 нет в списке. Введите число от 1 до %2:").arg(taskIndex).arg(ids.size()));
                return;
            }
        } else {
            sendMessage(chatId, "ℹ️ Сначала откройте список задач с помощью кнопки <b>📋 Задачи</b>.");
            return;
        }
    }

    // Если код не подошел и это не известная команда
    if (!isAuthorized) {
        sendMessage(chatId,
                    "❌ <b>Неверный или устаревший код авторизации.</b>\n\n"
                    "Пожалуйста, посмотрите актуальный 10-значный код в приложении AirTask (он обновляется каждые 5 минут).");
    } else {
        sendMessage(chatId, "Неизвестная команда. Доступны: 📋 Задачи, ➕ Добавить задачу, ℹ️ Статус, 🛑 Стоп, /update");
    }
}

void TelegramBot::sendMessage(qint64 chatId, const QString &text, const QJsonObject &customReplyMarkup)
{
    if (m_token.isEmpty() || chatId == 0) return;

    QUrl url(QString("https://api.telegram.org/bot%1/sendMessage").arg(m_token));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload.insert("chat_id", chatId);
    payload.insert("text", text);
    payload.insert("parse_mode", "HTML");

    if (!customReplyMarkup.isEmpty()) {
        payload.insert("reply_markup", customReplyMarkup);
    } else if (m_taskCreationSessions.contains(chatId) || m_taskEditSessions.contains(chatId)) {
        payload.insert("reply_markup", getCancelReplyKeyboard());
    } else if (m_dbManager && m_dbManager->IsTelegramUserAuthorized(chatId)) {
        payload.insert("reply_markup", getMainReplyKeyboard());
    }

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

void TelegramBot::ShowMessage(const QString &title, const QString &message)
{
    if (!m_isRunning || m_token.isEmpty() || !m_dbManager) return;

    QVector<qint64> subscribers = m_dbManager->GetTelegramSubscribers();
    if (subscribers.isEmpty()) return;

    log(QString("Отправка напоминания о дедлайне: «%1» для %2 подписчик(ов)").arg(title).arg(subscribers.size()));

    QString formattedMessage = QString("⏰ <b>%1</b>\n%2").arg(title, message);

    for (qint64 chatId : subscribers) {
        sendMessage(chatId, formattedMessage);
    }
}

void TelegramBot::NotifyTaskCreated(const task &newTask, const QString &categoryName)
{
    if (!m_isRunning || m_token.isEmpty() || !m_dbManager) return;

    QVector<qint64> subscribers = m_dbManager->GetTelegramSubscribers();
    if (subscribers.isEmpty()) return;

    log(QString("Отправка уведомления о новой задаче: «%1» для %2 подписчик(ов)").arg(newTask.title).arg(subscribers.size()));

    QString safeCategory = categoryName.trimmed().isEmpty() ? "Без категории" : categoryName;

    QString text = "🆕 <b>Новая задача!</b>\n\n";
    text += QString("📌 <b>%1</b>\n").arg(newTask.title.toHtmlEscaped());
    text += QString("📂 Категория: <b>%1</b>\n").arg(safeCategory.toHtmlEscaped());
    if (!newTask.tags.trimmed().isEmpty()) {
        text += QString("🏷️ Метки: %1\n").arg(newTask.tags.toHtmlEscaped());
    }
    if (newTask.deadline.isValid()) {
        text += QString("⏰ Дедлайн: %1\n").arg(newTask.deadline.toString("dd.MM.yyyy HH:mm"));
    }

    for (qint64 chatId : subscribers) {
        sendMessage(chatId, text);
    }
}

void TelegramBot::answerCallbackQuery(const QString &callbackQueryId, const QString &text)
{
    if (m_token.isEmpty() || callbackQueryId.isEmpty()) return;

    QUrl url(QString("https://api.telegram.org/bot%1/answerCallbackQuery").arg(m_token));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload.insert("callback_query_id", callbackQueryId);
    if (!text.isEmpty()) {
        payload.insert("text", text);
    }

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}

QJsonObject TelegramBot::getTaskInlineKeyboard(int taskId, bool isCompleted) const
{
    QJsonArray inlineKeyboard;

    auto makeBtn = [](const QString &text, const QString &data) {
        QJsonObject btn;
        btn["text"] = text;
        btn["callback_data"] = data;
        return btn;
    };

    QJsonArray row1;
    row1.append(makeBtn("✏️ Редактировать название", QString("edit_title:%1").arg(taskId)));
    inlineKeyboard.append(row1);

    QJsonArray row2;
    row2.append(makeBtn("🏷️ Редактировать метку", QString("edit_tags:%1").arg(taskId)));
    inlineKeyboard.append(row2);

    QJsonArray row3;
    row3.append(makeBtn("⏰ Редактировать дедлайн", QString("edit_deadline:%1").arg(taskId)));
    inlineKeyboard.append(row3);

    QJsonArray row4;
    if (isCompleted) {
        row4.append(makeBtn("↩️ Вернуть в работу", QString("uncomplete_task:%1").arg(taskId)));
    } else {
        row4.append(makeBtn("✅ Выполнить задачу", QString("complete_task:%1").arg(taskId)));
    }
    row4.append(makeBtn("🗑️ Удалить задачу", QString("delete_task:%1").arg(taskId)));
    inlineKeyboard.append(row4);

    QJsonObject markup;
    markup["inline_keyboard"] = inlineKeyboard;
    return markup;
}

void TelegramBot::showTaskDetails(qint64 chatId, int taskId, int userIndex)
{
    if (!m_dbManager) return;

    task t;
    if (!m_dbManager->GetTaskById(taskId, t)) {
        sendMessage(chatId, "⚠️ Задача не найдена (возможно, она уже была удалена).");
        return;
    }

    QString categoryName = m_dbManager->GetProjectNameById(t.project_id);
    QString tagsDisplay = t.tags.trimmed().isEmpty() ? "<i>нет</i>" : QString("<code>%1</code>").arg(t.tags.trimmed().toHtmlEscaped());
    QString dlDisplay = t.deadline.isValid() ? t.deadline.toString("dd.MM.yyyy HH:mm") : "<i>нет</i>";
    QString statusDisplay = t.is_completed ? "✅ Выполнена" : "⏳ В работе";

    QString card = QString("📋 <b>Информация о задаче #%1</b>\n\n").arg(userIndex);
    card += QString("📌 <b>Название:</b> %1\n").arg(t.title.toHtmlEscaped());
    card += QString("📂 <b>Категория:</b> %1\n").arg(categoryName.toHtmlEscaped());
    card += QString("🏷️ <b>Метки:</b> %1\n").arg(tagsDisplay);
    card += QString("⏰ <b>Дедлайн:</b> %1\n").arg(dlDisplay);
    card += QString("📊 <b>Статус:</b> %1\n\n").arg(statusDisplay);
    card += "<i>Выберите действие с помощью кнопок ниже:</i>";

    sendMessage(chatId, card, getTaskInlineKeyboard(taskId, t.is_completed));
}

void TelegramBot::processCallbackQuery(const QJsonObject &cbObj)
{
    QString queryId = cbObj.value("id").toString();
    QJsonObject fromObj = cbObj.value("from").toObject();
    qint64 chatId = fromObj.value("id").toInteger();
    if (chatId == 0 && cbObj.contains("message")) {
        chatId = cbObj.value("message").toObject().value("chat").toObject().value("id").toInteger();
    }
    QString data = cbObj.value("data").toString();

    bool isAuthorized = m_dbManager ? m_dbManager->IsTelegramUserAuthorized(chatId) : false;
    if (!isAuthorized) {
        answerCallbackQuery(queryId, "⚠️ Вы не авторизованы.");
        return;
    }

    QStringList parts = data.split(':');
    if (parts.size() < 2) {
        answerCallbackQuery(queryId);
        return;
    }

    QString action = parts[0];
    int taskId = parts[1].toInt();

    // Отвечаем Telegram, чтобы убрать индикатор загрузки с кнопки
    answerCallbackQuery(queryId);

    if (action == "complete_task" || action == "uncomplete_task") {
        task t;
        if (!m_dbManager || !m_dbManager->GetTaskById(taskId, t)) {
            sendMessage(chatId, "⚠️ Задача не найдена (возможно, она уже была удалена).");
            return;
        }

        bool complete = (action == "complete_task");
        m_dbManager->CompleteTask(complete, t);
        emit taskAdded();

        if (m_taskEditSessions.contains(chatId) && m_taskEditSessions[chatId].taskId == taskId) {
            m_taskEditSessions.remove(chatId);
        }

        if (complete) {
            log(QString("Пользователь chat_id %1 отметил задачу #%2 («%3») выполненной через Telegram").arg(chatId).arg(taskId).arg(t.title));
            sendMessage(chatId, QString("✅ Задача <b>«%1»</b> успешно выполнена! 🎉").arg(t.title.toHtmlEscaped()));
        } else {
            log(QString("Пользователь chat_id %1 вернул задачу #%2 («%3») в работу через Telegram").arg(chatId).arg(taskId).arg(t.title));
            sendMessage(chatId, QString("⏳ Задача <b>«%1»</b> возвращена в работу.").arg(t.title.toHtmlEscaped()));
        }
        return;
    }

    if (action == "delete_task") {
        task t;
        if (!m_dbManager || !m_dbManager->GetTaskById(taskId, t)) {
            sendMessage(chatId, "⚠️ Задача уже была удалена.");
            return;
        }

        QString deletedTitle = t.title;
        m_dbManager->DeleteTaskFromDB(t);
        emit taskAdded();

        if (m_taskEditSessions.contains(chatId) && m_taskEditSessions[chatId].taskId == taskId) {
            m_taskEditSessions.remove(chatId);
        }

        log(QString("Пользователь chat_id %1 удалил задачу #%2 («%3») через Telegram").arg(chatId).arg(taskId).arg(deletedTitle));
        sendMessage(chatId, QString("🗑️ Задача <b>«%1»</b> успешно удалена!").arg(deletedTitle.toHtmlEscaped()));
        return;
    }

    if (action == "edit_title") {
        task t;
        if (!m_dbManager || !m_dbManager->GetTaskById(taskId, t)) {
            sendMessage(chatId, "⚠️ Задача не найдена (возможно, она удалена).");
            return;
        }

        TaskEditSession session;
        session.taskId = taskId;
        session.field = TaskEditField::Title;
        m_taskEditSessions[chatId] = session;

        sendMessage(chatId,
                    QString("✏️ <b>Редактирование названия</b>\n\n"
                            "Текущее название:\n<b>%1</b>\n\n"
                            "Введите новое название задачи:\n<i>(или нажмите «Отмена» для выхода)</i>")
                        .arg(t.title.toHtmlEscaped()),
                    getCancelReplyKeyboard());
        return;
    }

    if (action == "edit_tags") {
        task t;
        if (!m_dbManager || !m_dbManager->GetTaskById(taskId, t)) {
            sendMessage(chatId, "⚠️ Задача не найдена (возможно, она удалена).");
            return;
        }

        TaskEditSession session;
        session.taskId = taskId;
        session.field = TaskEditField::Tags;
        m_taskEditSessions[chatId] = session;

        QString tagsDisplay = t.tags.trimmed().isEmpty() ? "<i>(нет меток)</i>" : QString("<code>%1</code>").arg(t.tags.toHtmlEscaped());
        sendMessage(chatId,
                    QString("🏷️ <b>Редактирование меток</b>\n\n"
                            "Текущие метки: %1\n\n"
                            "Введите новые метки через запятую или пробел\n(или отправьте <code>-</code>, чтобы очистить метки, либо «Отмена» для выхода):")
                        .arg(tagsDisplay),
                    getCancelReplyKeyboard());
        return;
    }

    if (action == "edit_deadline") {
        task t;
        if (!m_dbManager || !m_dbManager->GetTaskById(taskId, t)) {
            sendMessage(chatId, "⚠️ Задача не найдена (возможно, она удалена).");
            return;
        }

        TaskEditSession session;
        session.taskId = taskId;
        session.field = TaskEditField::Deadline;
        m_taskEditSessions[chatId] = session;

        QString dlDisplay = t.deadline.isValid() ? t.deadline.toString("dd.MM.yyyy HH:mm") : "<i>(нет дедлайна)</i>";
        sendMessage(chatId,
                    QString("⏰ <b>Редактирование дедлайна</b>\n\n"
                            "Текущий дедлайн: <b>%1</b>\n\n"
                            "Введите новый дедлайн в формате <b>ДД.ММ.ГГГГ ЧЧ:ММ</b> (например: <code>05.11.2026 18:00</code>)\n"
                            "(или отправьте <code>-</code>, чтобы убрать дедлайн, либо «Отмена» для выхода):")
                        .arg(dlDisplay),
                    getCancelReplyKeyboard());
        return;
    }
}

void TelegramBot::handleTaskEditStep(qint64 chatId, const QString &text)
{
    QString trimmedText = text.trimmed();
    QString lowerText = trimmedText.toLower();

    if (lowerText == "/cancel" || lowerText == "отмена" || trimmedText == "❌ Отмена") {
        m_taskEditSessions.remove(chatId);
        sendMessage(chatId, "🚫 Редактирование задачи отменено.", getMainReplyKeyboard());
        return;
    }

    if (!m_taskEditSessions.contains(chatId)) return;

    TaskEditSession session = m_taskEditSessions.value(chatId);
    task t;
    if (!m_dbManager || !m_dbManager->GetTaskById(session.taskId, t)) {
        m_taskEditSessions.remove(chatId);
        sendMessage(chatId, "⚠️ Задача не найдена или была удалена.", getMainReplyKeyboard());
        return;
    }

    switch (session.field) {
    case TaskEditField::Title: {
        if (trimmedText.isEmpty()) {
            sendMessage(chatId, "⚠️ Название не может быть пустым. Введите новое название (или «Отмена»):");
            return;
        }
        t.title = trimmedText;
        m_dbManager->UpdateTaskChanged(t);
        m_taskEditSessions.remove(chatId);
        emit taskAdded();
        log(QString("Пользователь chat_id %1 изменил название задачи #%2 на «%3»").arg(chatId).arg(t.id).arg(t.title));
        sendMessage(chatId, QString("✅ Название задачи успешно обновлено на:\n<b>%1</b>").arg(t.title.toHtmlEscaped()), getMainReplyKeyboard());
        break;
    }

    case TaskEditField::Tags: {
        if (trimmedText == "-" || trimmedText == "—" || trimmedText.compare("нет", Qt::CaseInsensitive) == 0) {
            t.tags = "";
        } else {
            t.tags = trimmedText;
        }
        m_dbManager->UpdateTaskChanged(t);
        m_taskEditSessions.remove(chatId);
        emit taskAdded();
        log(QString("Пользователь chat_id %1 изменил метки задачи #%2 на «%3»").arg(chatId).arg(t.id).arg(t.tags));
        QString tagInfo = t.tags.isEmpty() ? "<i>(метки удалены)</i>" : QString("<code>%1</code>").arg(t.tags.toHtmlEscaped());
        sendMessage(chatId, QString("✅ Метки задачи успешно обновлены: %1").arg(tagInfo), getMainReplyKeyboard());
        break;
    }

    case TaskEditField::Deadline: {
        QDateTime deadline;
        if (trimmedText != "-" && trimmedText != "—" && trimmedText.compare("нет", Qt::CaseInsensitive) != 0) {
            static const QStringList formats = {
                "dd.MM.yyyy HH:mm",
                "dd.MM.yyyy H:mm",
                "dd.MM.yyyy HH:mm:ss",
                "dd.MM.yyyy",
                "yyyy-MM-dd HH:mm",
                "yyyy-MM-dd"
            };

            for (const QString &fmt : formats) {
                QDateTime dt = QDateTime::fromString(trimmedText, fmt);
                if (dt.isValid()) {
                    deadline = dt;
                    break;
                }
            }

            if (!deadline.isValid()) {
                sendMessage(chatId,
                            "⚠️ <b>Неверный формат даты!</b>\n\n"
                            "Пожалуйста, введите дату в формате <b>ДД.ММ.ГГГГ ЧЧ:ММ</b> (например: <code>05.11.2026 18:00</code>)\n"
                            "или отправьте <code>-</code>, чтобы убрать дедлайн (или «Отмена» для выхода):");
                return;
            }
        }

        t.deadline = deadline;
        m_dbManager->UpdateTaskChanged(t);
        m_taskEditSessions.remove(chatId);
        emit taskAdded();
        log(QString("Пользователь chat_id %1 изменил дедлайн задачи #%2").arg(chatId).arg(t.id));
        QString dlInfo = deadline.isValid() ? deadline.toString("dd.MM.yyyy HH:mm") : "<i>(дедлайн снят)</i>";
        sendMessage(chatId, QString("✅ Дедлайн задачи успешно обновлен: <b>%1</b>").arg(dlInfo), getMainReplyKeyboard());
        break;
    }

    default:
        m_taskEditSessions.remove(chatId);
        break;
    }
}


