# CMakeLists.txt — сборочный скрипт проекта sysflex
# Требует CMake >= 3.16 и компилятор с поддержкой C++17.
cmake_minimum_required(VERSION 3.16)
project(sysflex VERSION 2.0.0 LANGUAGES CXX)

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

# --- Поиск pthread (используется std::thread внутри, но линкуем явно) ---
find_package(Threads REQUIRED)

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
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                          GROUP_READ GROUP_EXECUTE
                          WORLD_READ WORLD_EXECUTE)

# --- Тесты (опционально, включены по умолчанию) ---
option(SYSFLEX_BUILD_TESTS "Собирать модульные тесты sysflex" ON)
if(SYSFLEX_BUILD_TESTS)
    enable_testing()
    add_executable(sysflex_tests tests/test_main.cpp)
    target_link_libraries(sysflex_tests PRIVATE sysflex_core)
    add_test(NAME sysflex_unit_tests COMMAND sysflex_tests)
endif()

# --- Информационное сообщение по завершении конфигурации ---
message(STATUS "sysflex ${PROJECT_VERSION} — сборка настроена (C++${CMAKE_CXX_STANDARD}, ${CMAKE_BUILD_TYPE})")
