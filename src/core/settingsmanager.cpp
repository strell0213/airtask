#include "settingsmanager.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>
#include <QFile>

settingsmanager::settingsmanager()
{
    EnsureSettingsFileExists();
    CheckSettings();
}

void settingsmanager::CheckSettings()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    bool changed = false;

    if (!settings.contains("Database/Path")) {
        settings.setValue("Database/Path", "");
        changed = true;
    }
    if (!settings.contains("General/OpacityApp") && !settings.contains("OpacityApp")) {
        settings.setValue("General/OpacityApp", 100);
        changed = true;
    }
    if (!settings.contains("General/PosWindowX") && !settings.contains("PosWindowX")) {
        settings.setValue("General/PosWindowX", 0);
        changed = true;
    }
    if (!settings.contains("General/PosWindowY") && !settings.contains("PosWindowY")) {
        settings.setValue("General/PosWindowY", 0);
        changed = true;
    }
    if (!settings.contains("General/Notify") && !settings.contains("Notify")) {
        settings.setValue("General/Notify", true);
        changed = true;
    }
    if (!settings.contains("General/StartUp") && !settings.contains("StartUp")) {
        settings.setValue("General/StartUp", false);
        changed = true;
    }
    if (!settings.contains("General/HideCompletedTasks") && !settings.contains("HideCompletedTasks")) {
        settings.setValue("General/HideCompletedTasks", false);
        changed = true;
    }
    if (!settings.contains("General/WindowWidth") && !settings.contains("WindowWidth")) {
        settings.setValue("General/WindowWidth", 430);
        changed = true;
    }
    if (!settings.contains("General/WindowHeight") && !settings.contains("WindowHeight")) {
        settings.setValue("General/WindowHeight", 510);
        changed = true;
    }
    if (!settings.contains("Categories/HiddenCategories")) {
        settings.setValue("Categories/HiddenCategories", QStringList());
        changed = true;
    }
    if (!settings.contains("Categories/HiddenFromAll")) {
        settings.setValue("Categories/HiddenFromAll", QStringList());
        changed = true;
    }

    if (changed) {
        settings.sync();
    }
}

QString settingsmanager::GetSettingsDirPath()
{
    return "C:/Users/Public/Documents/AirTask";
}

QString settingsmanager::GetSettingsFilePath()
{
    return "C:/Users/Public/Documents/AirTask/Settings.ini";
}

void settingsmanager::EnsureSettingsFileExists()
{
    QString dirPath = GetSettingsDirPath();
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString filePath = GetSettingsFilePath();
    QFile file(filePath);
    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.close();
        }
    }
}

QString settingsmanager::GetDatabaseDir()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    QString dbDir = settings.value("Database/Path", "").toString().trimmed();
    if (dbDir.isEmpty()) {
        return QCoreApplication::applicationDirPath();
    }
    QDir dir(dbDir);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return QCoreApplication::applicationDirPath();
        }
    }
    return dbDir;
}

void settingsmanager::SetDatabaseDir(const QString &dirPath)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    settings.setValue("Database/Path", dirPath);
    settings.sync();
}

void settingsmanager::SetAutostart(bool enable)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    if (enable) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        settings.setValue("AirTask", QString("\"%1\"").arg(appPath));
    } else {
        settings.remove("AirTask");
    }
}

bool settingsmanager::GetAutostart()
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    return settings.contains("AirTask");
}

int settingsmanager::GetOpacityApp()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    int opacity = settings.value("General/OpacityApp", settings.value("OpacityApp", 100)).toInt();
    if (opacity < 20 || opacity > 100) {
        opacity = 100;
    }
    return opacity;
}

void settingsmanager::SetOpacityApp(int opacity)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    settings.setValue("General/OpacityApp", opacity);
    settings.sync();
}

int settingsmanager::GetPosWindowX()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    return settings.value("General/PosWindowX", settings.value("PosWindowX", 0)).toInt();
}

void settingsmanager::SetPosWindowX(int x)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    settings.setValue("General/PosWindowX", x);
    settings.sync();
}

int settingsmanager::GetPosWindowY()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    return settings.value("General/PosWindowY", settings.value("PosWindowY", 0)).toInt();
}

void settingsmanager::SetPosWindowY(int y)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    settings.setValue("General/PosWindowY", y);
    settings.sync();
}

void settingsmanager::SetWindowPos(int x, int y)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    settings.setValue("General/PosWindowX", x);
    settings.setValue("General/PosWindowY", y);
    settings.sync();
}

bool settingsmanager::GetNotify()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    return settings.value("General/Notify", settings.value("Notify", true)).toBool();
}

void settingsmanager::SetNotify(bool enable)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    settings.setValue("General/Notify", enable);
    settings.sync();
}

bool settingsmanager::GetStartUp()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    return settings.value("General/StartUp", settings.value("StartUp", false)).toBool();
}

void settingsmanager::SetStartUp(bool enable)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    settings.setValue("General/StartUp", enable);
    settings.sync();
    SetAutostart(enable);
}

int settingsmanager::GetWindowWidth()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    int w = settings.value("General/WindowWidth", settings.value("WindowWidth", 430)).toInt();
    return (w >= 300) ? w : 430;
}

void settingsmanager::SetWindowWidth(int w)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    if (w < 300) w = 300;
    settings.setValue("General/WindowWidth", w);
    settings.sync();
}

int settingsmanager::GetWindowHeight()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    int h = settings.value("General/WindowHeight", settings.value("WindowHeight", 510)).toInt();
    return (h >= 300) ? h : 510;
}

void settingsmanager::SetWindowHeight(int h)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    if (h < 300) h = 300;
    settings.setValue("General/WindowHeight", h);
    settings.sync();
}

void settingsmanager::SetWindowSize(int w, int h)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    if (w < 300) w = 300;
    if (h < 300) h = 300;
    settings.setValue("General/WindowWidth", w);
    settings.setValue("General/WindowHeight", h);
    settings.sync();
}

bool settingsmanager::GetHideCompletedTasks()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    return settings.value("General/HideCompletedTasks", settings.value("HideCompletedTasks", false)).toBool();
}

void settingsmanager::SetHideCompletedTasks(bool enable)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    settings.setValue("General/HideCompletedTasks", enable);
    settings.sync();
}

QSet<int> settingsmanager::GetHiddenCategoryIds()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    QVariant val = settings.value("Categories/HiddenCategories");
    QStringList list;
    if (val.userType() == QMetaType::QStringList) {
        list = val.toStringList();
    } else {
        QString str = val.toString().trimmed();
        if (!str.isEmpty()) {
            list = str.split(",", Qt::SkipEmptyParts);
        }
    }
    QSet<int> result;
    for (const QString &item : list) {
        bool ok = false;
        int id = item.trimmed().toInt(&ok);
        if (ok) result.insert(id);
    }
    return result;
}

void settingsmanager::SetCategoryHidden(int categoryId, bool hidden)
{
    QSet<int> current = GetHiddenCategoryIds();
    if (hidden) {
        current.insert(categoryId);
    } else {
        current.remove(categoryId);
    }
    SetHiddenCategoryIds(current);
}

void settingsmanager::SetHiddenCategoryIds(const QSet<int> &ids)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    QStringList list;
    for (int id : ids) {
        list.append(QString::number(id));
    }
    settings.setValue("Categories/HiddenCategories", list);
    settings.sync();
}

bool settingsmanager::IsCategoryHidden(int categoryId)
{
    return GetHiddenCategoryIds().contains(categoryId);
}

QSet<int> settingsmanager::GetHiddenFromAllCategoryIds()
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    QVariant val = settings.value("Categories/HiddenFromAll");
    QStringList list;
    if (val.userType() == QMetaType::QStringList) {
        list = val.toStringList();
    } else {
        QString str = val.toString().trimmed();
        if (!str.isEmpty()) {
            list = str.split(",", Qt::SkipEmptyParts);
        }
    }
    QSet<int> result;
    for (const QString &item : list) {
        bool ok = false;
        int id = item.trimmed().toInt(&ok);
        if (ok) result.insert(id);
    }
    return result;
}

void settingsmanager::SetCategoryHiddenFromAll(int categoryId, bool hidden)
{
    QSet<int> current = GetHiddenFromAllCategoryIds();
    if (hidden) {
        current.insert(categoryId);
    } else {
        current.remove(categoryId);
    }
    SetHiddenFromAllCategoryIds(current);
}

void settingsmanager::SetHiddenFromAllCategoryIds(const QSet<int> &ids)
{
    EnsureSettingsFileExists();
    QSettings settings(GetSettingsFilePath(), QSettings::IniFormat);
    QStringList list;
    for (int id : ids) {
        list.append(QString::number(id));
    }
    settings.setValue("Categories/HiddenFromAll", list);
    settings.sync();
}

bool settingsmanager::IsCategoryHiddenFromAll(int categoryId)
{
    return GetHiddenFromAllCategoryIds().contains(categoryId);
}


