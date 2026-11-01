#ifndef TELEGRAMDIALOG_H
#define TELEGRAMDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QKeyEvent>

#include "../core/telegrambot.h"
#include "../core/settingsmanager.h"
#include "../core/dbmanager.h"

class TelegramDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TelegramDialog(TelegramBot *bot, settingsmanager *settingsMgr, dbmanager *dbMgr, QWidget *parent = nullptr);
    ~TelegramDialog() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onToggleBotClicked();
    void onSaveTokenClicked();
    void onAutoStartToggled(bool checked);
    void onCopyCodeClicked();
    void onOpenBotClicked();
    void onBotStateChanged(BotState state, const QString &statusText);
    void onBotAuthCodeChanged(const QString &newCode, int remainingSeconds);
    void onUserSubscribed(const QString &username, qint64 chatId);
    void onLogMessage(const QString &msg);
    void onRefreshClientsClicked();
    void onUnbindClientClicked();
    void onClearLogClicked();

private:
    void initUI();
    void updateStatusDisplay(BotState state, const QString &text);
    void updateSubscribersCount();
    void updateClientsTable();

    TelegramBot *m_bot = nullptr;
    settingsmanager *m_settingsManager = nullptr;
    dbmanager *m_dbManager = nullptr;

    // UI Widgets
    QPushButton *m_closeBtn = nullptr;
    QLineEdit *m_tokenInput = nullptr;
    QPushButton *m_saveTokenBtn = nullptr;
    QPushButton *m_toggleBotBtn = nullptr;
    QLabel *m_statusIndicator = nullptr;
    QLabel *m_statusTextLabel = nullptr;
    QCheckBox *m_autoStartCheckBox = nullptr;

    QLabel *m_authCodeLabel = nullptr;
    QLabel *m_timerLabel = nullptr;
    QPushButton *m_copyCodeBtn = nullptr;
    QPushButton *m_openBotBtn = nullptr;
    QLabel *m_subscribersCountLabel = nullptr;

    // Вкладки: Лог и Список клиентов
    QTabWidget *m_tabWidget = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    QPushButton *m_clearLogBtn = nullptr;
    QTableWidget *m_clientsTable = nullptr;
    QPushButton *m_refreshClientsBtn = nullptr;
    QPushButton *m_unbindClientBtn = nullptr;

    // Окно без рамок (drag support)
    bool m_dragging = false;
    QPoint m_dragPosition;
};

#endif // TELEGRAMDIALOG_H


