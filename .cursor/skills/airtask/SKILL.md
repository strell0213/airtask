---
name: airtask
description: >-
  Специфика репозитория AirTask: схема airtask.db, порядок task_order,
  настройки, ключевые файлы и quirks. Использовать вместе с skill qt-desktop
  при работе именно с этим проектом.
---

# AirTask (project-specific)

> Универсальные Qt-паттерны — skill `qt-desktop`. Здесь только специфика этого репо.

## Ключевые файлы

| Файл | Роль |
|------|------|
| `src/ui/mainwindow.cpp` | UI, проекты, настройки, уведомления |
| `src/ui/taskitem.cpp` | Строка задачи, DnD, inline-edit |
| `src/core/dbmanager.cpp` | Весь SQL |
| `src/ui/style.qss` | Тёмная тема |

## SQLite

```sql
tasks(id, title, project_id, tags, deadline, is_completed)
projects(id, name, color)
task_order(task_id, project_id, numpp)   -- -1 = вкладка «Все»
settings(ID, SKey, SValue)
```

Миграций в коде нет.

## Порядок задач

1. `AddTaskToDB` → order для project + `OrderProjectAll (-1)`
2. `UpdateTasks(tasks, orderProjectId)` — JOIN по `project_id`
3. DnD → `ReorderTasks` → `NormalizeTaskOrder`

## Настройки

| SKey | Назначение |
|------|------------|
| OpacityApp | 20–100 |
| PosWindowX/Y | Позиция окна |
| Notify | Дедлайн-уведомления |
| StartUp | Автозапуск (реестр) |

Новая настройка: `CheckSettings` → `AddSetting` → `initSettings` → `UpdateSettings`.

## Quirks (не трогать без задачи)

- Include-пути `"../src/..."` в headers
- Ресурс `settigns.svg` (опечатка в имени)
- `CheckSettings`: проверка Y через `posWindowX` — известный баг
- `tags.h` не связан с БД; тег = `task.tags`

## Сборка

```powershell
cmake -S . -B build && cmake --build build --config Release
.\build\AirTask.exe
```

БД `airtask.db` — CWD процесса.
