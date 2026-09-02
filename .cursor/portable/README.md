# Переносимые Rules и Skill для Qt/C++ проектов

Скопируйте содержимое в другой репозиторий:

```
portable/rules/*.mdc     →  <другой-проект>/.cursor/rules/
portable/skills/qt-desktop/  →  <другой-проект>/.cursor/skills/qt-desktop/
```

## Что внутри

| Файл | Назначение |
|------|------------|
| `rules/cpp-core.mdc` | Общие правила C++ (always apply) |
| `rules/qt-desktop.mdc` | Qt Widgets, QSS, SQL, CMake (для `*.{cpp,h,ui,qss,qrc}`) |
| `skills/qt-desktop/SKILL.md` | Workflow и чеклисты для Qt desktop |

## Рекомендуемая схема в целевом проекте

```
.cursor/
  rules/
    cpp-core.mdc          ← из portable
    qt-desktop.mdc        ← из portable
    project.mdc           ← создать вручную: имя app, пути, схема БД
  skills/
    qt-desktop/SKILL.md   ← из portable
    my-app/SKILL.md       ← опционально: только специфика проекта
```

**Универсальный слой** — переиспользуется везде.  
**project.mdc / my-app/SKILL.md** — 20–40 строк под конкретный репозиторий.

## Личное использование (все проекты)

Альтернатива: скопировать skill в `~/.cursor/skills/qt-desktop/` — будет доступен глобально.  
Rules из `.mdc` работают только внутри репозитория, где лежат в `.cursor/rules/`.
