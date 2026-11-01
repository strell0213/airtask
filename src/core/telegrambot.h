#ifndef TELEGRAMBOT_H
#define TELEGRAMBOT_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QTimer>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include <QHash>
#include "inotifier.h"
#include "dbmanager.h"
#include "settingsmanager.h"

enum class BotState {
    Stopped,
    Connecting,
    Running,
    Reconnecting
};

enum class TaskCreationStep {
    None,
    WaitingForTitle,
    WaitingForTags,
    WaitingForCategory,
    WaitingForDeadline
};

struct TaskCreationSession {
    TaskCreationStep step = TaskCreationStep::None;
    QString title;
    QString tags;
    int projectId = -1;
    QString projectName;
    QVector<projects> availableProjects;
};

class TelegramBot : public QObject, public INotifier
{
    Q_OBJECT

public:
    explicit TelegramBot(dbmanager *dbMgr, settingsmanager *settingsMgr, QObject *parent = nullptr);
    ~TelegramBot() override;

    // Реализация полиморфного интерфейса INotifier
    void ShowMessage(const QString &title, const QString &message) override;

    void StartBot();
    void StopBot();
    bool IsRunning() const;

    BotState GetState() const { return m_state; }
    QString GetStatusText() const { return m_statusText; }
    QString GetAuthCode() const { return m_authCode; }
    int GetRemainingCodeSeconds() const;
    QString GetBotUsername() const { return m_botUsername; }

    void SetToken(const QString &token);
    QString GetToken() const;

    void GenerateNewAuthCode();
    void UnbindUser(qint64 chatId);
    QVector<QString> GetLogHistory() const { return m_logHistory; }

signals:
    void stateChanged(BotState state, const QString &statusText);
    void authCodeChanged(const QString &newCode, int remainingSeconds);
    void userSubscribed(const QString &username, qint64 chatId);
    void logMessage(const QString &msg);
    void taskAdded();

private slots:
    void onPollFinished();
    void onPollTimeout();
    void onCodeTimerTick();

private:
    void pollUpdates();
    void scheduleRetry(int delayMs);
    void fetchBotInfo();
    void processUpdate(const QJsonObject &updateObj);
    void processMessage(const QJsonObject &msgObj);
    void sendMessage(qint64 chatId, const QString &text, const QJsonObject &customReplyMarkup = QJsonObject());
    void setState(BotState state, const QString &statusText);
    void log(const QString &text);

    QJsonObject getMainReplyKeyboard() const;
    QJsonObject getCancelReplyKeyboard() const;
    void handleTaskCreationStep(qint64 chatId, const QString &text);
    void cancelTaskCreation(qint64 chatId, bool notifyUser = true);

    dbmanager *m_dbManager = nullptr;
    settingsmanager *m_settingsManager = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    QNetworkReply *m_currentPollReply = nullptr;

    QString m_token;
    QString m_botUsername;
    QString m_authCode;
    QDateTime m_codeExpiresAt;

    QTimer *m_authCodeTimer = nullptr;
    QTimer *m_pollRetryTimer = nullptr;
    QTimer *m_watchdogTimer = nullptr;

    qint64 m_lastUpdateId = 0;
    bool m_isRunning = false;
    int m_consecutiveErrors = 0;

    BotState m_state = BotState::Stopped;
    QString m_statusText = "Выключен";

    QVector<QString> m_logHistory;
    QHash<qint64, TaskCreationSession> m_taskCreationSessions;
};

#endif // TELEGRAMBOT_H


