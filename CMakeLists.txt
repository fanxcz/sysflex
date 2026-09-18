# CMakeLists.txt — сборочный скрипт проекта sysflex
# Требует CMake >= 3.16 и компилятор с поддержкой C++17.
cmake_minimum_required(VERSION 3.16)
project(sysflex VERSION 2.1.0 LANGUAGES CXX)

# --- Глобальные настройки стандарта C++ ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# По умолчанию собираем в Release с оптимизациями, если тип сборки не указан
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Единые флаги предупреждений компилятора для более чистого и надёжного кода
add_compile_options(-Wall -Wextra)

# В CI собираем с -Werror, чтобы предупреждение не могло доехать до main
option(SYSFLEX_WERROR "Трактовать предупреждения компилятора как ошибки" OFF)
if(SYSFLEX_WERROR)
    add_compile_options(-Werror)
endif()

# --- Поиск pthread (используется std::thread внутри, но линкуем явно) ---
find_package(Threads REQUIRED)

# Версия объявляется в одном месте — константа VERSION в include/sysflex/config.hpp.
# Здесь лишь проверяем, что она совпадает с версией проекта, иначе два числа
# неизбежно разъедутся при следующем релизе.
file(STRINGS "${CMAKE_SOURCE_DIR}/include/sysflex/config.hpp" _sysflex_version_line
     REGEX "^inline const char\\* VERSION = ")
if(_sysflex_version_line)
    string(REGEX REPLACE ".*VERSION = \"([^\"]+)\".*" "\\1" SYSFLEX_HEADER_VERSION "${_sysflex_version_line}")
    if(NOT SYSFLEX_HEADER_VERSION STREQUAL PROJECT_VERSION)
        message(FATAL_ERROR
            "Версия в include/sysflex/config.hpp (${SYSFLEX_HEADER_VERSION}) "
            "не совпадает с версией проекта (${PROJECT_VERSION})")
    endif()
endif()

# --- Библиотека с основной логикой sysflex (без main.cpp), чтобы её можно
# было переиспользовать и в тестах, и в основном исполняемом файле ---
add_library(sysflex_core
    src/system_info.cpp
    src/display.cpp
    src/sysflex.cpp
)
target_include_directories(sysflex_core PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(sysflex_core PUBLIC Threads::Threads)

# --- Основной исполняемый файл ---
add_executable(sysflex src/main.cpp)
target_link_libraries(sysflex PRIVATE sysflex_core)

install(TARGETS sysflex RUNTIME DESTINATION bin)
install(DIRECTORY plugins/ DESTINATION share/sysflex/plugins
        USE_SOURCE_PERMISSIONS
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                          GROUP_READ GROUP_EXECUTE
                          WORLD_READ WORLD_EXECUTE)
install(FILES man/sysflex.1 DESTINATION share/man/man1)

# --- Тесты (опционально, включены по умолчанию) ---
option(SYSFLEX_BUILD_TESTS "Собирать модульные тесты sysflex" ON)
if(SYSFLEX_BUILD_TESTS)
    enable_testing()
    add_executable(sysflex_tests tests/test_main.cpp)
    target_link_libraries(sysflex_tests PRIVATE sysflex_core)
    # Тесты не должны зависеть от локального ~/.config/sysflex/config
    add_test(NAME sysflex_unit_tests
             COMMAND ${CMAKE_COMMAND} -E env
                     SYSFLEX_CONFIG=${CMAKE_BINARY_DIR}/nonexistent-sysflex-config
                     $<TARGET_FILE:sysflex_tests>)
endif()

# --- Дымовые тесты самого бинарного файла (режимы и валидность JSON) ---
option(SYSFLEX_BUILD_SMOKE_TESTS "Добавить дымовые тесты бинарного файла" OFF)
if(SYSFLEX_BUILD_SMOKE_TESTS)
    enable_testing()
    add_test(NAME smoke_once COMMAND sysflex --once --no-plugins --no-color --no-ascii)
    add_test(NAME smoke_short COMMAND sysflex --once --short --no-plugins --no-color)
    add_test(NAME smoke_game COMMAND sysflex --once --game --no-plugins --no-color)
    add_test(NAME smoke_list_themes COMMAND sysflex --list-themes)
    add_test(NAME smoke_version COMMAND sysflex --version)
endif()

# --- Информационное сообщение по завершении конфигурации ---
message(STATUS "sysflex ${PROJECT_VERSION} — сборка настроена (C++${CMAKE_CXX_STANDARD}, ${CMAKE_BUILD_TYPE})")
