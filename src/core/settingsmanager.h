#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QString>
#include <QSet>
#include <QStringList>

class settingsmanager
{
public:
    settingsmanager();

    void SetAutostart(bool enable);
    bool GetAutostart();

    static QString GetSettingsDirPath();
    static QString GetSettingsFilePath();
    static void EnsureSettingsFileExists();
    void CheckSettings();

    QString GetDatabaseDir();
    void SetDatabaseDir(const QString &dirPath);

    int GetOpacityApp();
    void SetOpacityApp(int opacity);

    int GetPosWindowX();
    void SetPosWindowX(int x);

    int GetPosWindowY();
    void SetPosWindowY(int y);
    void SetWindowPos(int x, int y);

    int GetWindowWidth();
    void SetWindowWidth(int w);

    int GetWindowHeight();
    void SetWindowHeight(int h);
    void SetWindowSize(int w, int h);

    bool GetNotify();
    void SetNotify(bool enable);

    bool GetStartUp();
    void SetStartUp(bool enable);

    bool GetHideCompletedTasks();
    void SetHideCompletedTasks(bool enable);

    QSet<int> GetHiddenCategoryIds();
    void SetCategoryHidden(int categoryId, bool hidden);
    void SetHiddenCategoryIds(const QSet<int> &ids);
    bool IsCategoryHidden(int categoryId);

    QSet<int> GetHiddenFromAllCategoryIds();
    void SetCategoryHiddenFromAll(int categoryId, bool hidden);
    void SetHiddenFromAllCategoryIds(const QSet<int> &ids);
    bool IsCategoryHiddenFromAll(int categoryId);
};

#endif // SETTINGSMANAGER_H

