---
name: qt-desktop
description: >-
  Разработка desktop-приложений на Qt/C++ (Widgets, CMake, QSS, Qt SQL).
  Использовать при работе с Qt-проектами: MainWindow, custom widgets, SQLite,
  системный трей, frameless UI, resources.qrc, dbmanager/repository-слой.
---

# Qt Desktop (универсальный)

## Быстрый старт в новом проекте

1. Прочитать `CMakeLists.txt` — Qt-модули, C++ standard, layout `src/`
2. Найти точку входа (`main.cpp`) и главное окно
3. Найти data-слой (`*manager*`, `*repository*`, `*db*`)
4. Найти стили (`*.qss`, `*.qrc`)
5. **Следовать существующим соглашениям**, не навязывать свой стиль

## Типовая архитектура

```
main.cpp
  └── MainWindow / ApplicationWindow
        ├── DataLayer (dbmanager, settings)
        ├── Models (POD structs/classes)
        ├── Custom widgets (*Item, *Panel)
        └── QSS + qrc resources
```

**Поток данных:** UI → data layer → БД/файл → обновление кэша/модели → refresh UI.

## CMake + Qt

```cmake
cmake_minimum_required(VERSION 3.16)
project(MyApp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

qt_add_executable(MyApp ${SOURCES})
target_link_libraries(MyApp PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
```

Доп. модули по необходимости: `Sql`, `Svg`, `Network`, `Multimedia`.

## Добавление поля в модель (чеклист)

1. Поле в model struct/class
2. Колонка в БД (миграция/SQL вручную — если миграций нет в проекте)
3. SELECT / INSERT / UPDATE в data layer
4. UI: отображение + редактирование
5. Сборка и smoke-test сценария

## Custom widget

```cpp
class MyItem : public QWidget {
    Q_OBJECT
public:
    explicit MyItem(const Model &m, DataLayer *db, QWidget *parent = nullptr);
signals:
    void updateRequested();
protected:
    void paintEvent(QPaintEvent *) override;  // если нужен custom draw
};
```

- `paintEvent` + `QStyleOption` для корректного QSS
- DnD: порог движения, `mousePress/Move/Release`, сигнал после commit

## QSS

```css
QWidget#myPanel {
    background-color: #1e2229;
    border-radius: 6px;
}
QPushButton#primaryBtn:hover {
    background-color: #3d434f;
}
```

В C++: `widget->setObjectName("myPanel")`.

## Qt SQL

```cpp
QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
db.setDatabaseName("app.db");
if (!db.open()) { /* log error */ }

QSqlQuery q;
q.prepare("INSERT INTO items (title) VALUES (:t)");
q.bindValue(":t", title);
q.exec();
```

Отдельный порядок сортировки — часто выносится в join-таблицу (`item_order`), а не в основную таблицу.

## Системный трей

- `QSystemTrayIcon` с родителем = main window
- Меню: Restore / Quit
- `closeEvent` → hide + `event->ignore()`
- `activated(Trigger)` → toggle show/hide

## Сборка

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Qt в PATH или `-DCMAKE_PREFIX_PATH=<Qt>/lib/cmake/Qt6`.

## Чеклист перед завершением

- [ ] Сборка без ошибок
- [ ] SQL через prepare/bindValue
- [ ] Новые виджеты: objectName + QSS при необходимости
- [ ] Data layer обновлён, UI синхронизирован
- [ ] Минимальный diff

## Адаптация под проект

После копирования в новый репозиторий добавьте **тонкий** project-skill или rule с:

- Именем приложения и путями к ключевым файлам
- Схемой БД и списком настроек
- Особенностями и известными quirks

Универсальный skill не заменяет project-specific контекст — дополняет его.
