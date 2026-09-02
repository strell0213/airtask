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
main.cpp → MainWindow
             ├── DataLayer (db, settings)
             ├── Models
             ├── Custom widgets
             └── QSS + qrc
```

UI → data layer → БД → refresh UI.

## Добавление поля в модель

1. Поле в model
2. Колонка в БД (если нет миграций — SQL вручную)
3. SELECT/INSERT/UPDATE в data layer
4. UI отображение/редактирование

## Custom widget + DnD

`Q_OBJECT`, signals `updateRequested` / `orderChanged`, порог drag, commit порядка в data layer.

## QSS

`setObjectName` → селекторы в `.qss` → `.qrc`.

## Qt SQL

```cpp
query.prepare("... WHERE id = :id");
query.bindValue(":id", id);
```

Отдельная таблица порядка (`*_order`) — если сортировка разная в разных «вкладках».

## Сборка

```powershell
cmake -S . -B build && cmake --build build --config Release
```

## Чеклист

- [ ] Сборка OK
- [ ] SQL: prepare + bindValue
- [ ] objectName + QSS для новых виджетов
- [ ] Минимальный diff

Полная версия для копирования: `.cursor/portable/skills/qt-desktop/SKILL.md`
