cmake_minimum_required(VERSION 3.16)

project(AirTask VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Добавляем SQL, так как он понадобится для базы данных трекера
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Sql Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Sql Svg)

# 1. АВТОМАТИЧЕСКИЙ ПОИСК ФАЙЛОВ
# Ищет все файлы в src и её подпапках (ui, core и т.д.)
file(GLOB_RECURSE PROJECT_SOURCES 
    "src/*.cpp" 
    "src/*.h" 
    "src/*.ui" 
    "src/*.qrc"
    "src/*.qrc"  # <-- Важно!
    "src/*.qss"  # Можно добавить и qss для контроля
)

# 2. ДОБАВЛЯЕМ ПУТЬ К SRC В ИНКЛЮДЫ
# Это позволит писать #include "ui/mainwindow.h" вместо полных путей
include_directories(src)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AirTask
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(AirTask ${PROJECT_SOURCES})
endif()

# Не забудь добавить Qt${QT_VERSION_MAJOR}::Sql здесь для БД
target_link_libraries(AirTask PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Svg)

# Настройки бандла и исполняемого файла
set_target_properties(AirTask PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS AirTask
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AirTask)
endif()