// sysflex.hpp — главный класс приложения, объединяющий конфигурацию,
// сбор данных (SystemMonitor), отображение (Display) и систему плагинов.
#pragma once

#include "config.hpp"
#include "system_info.hpp"
#include "display.hpp"
#include "themes.hpp"
#include "sparkline.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace sysflex {

// Главный класс приложения sysflex. Инкапсулирует весь жизненный цикл:
// разбор конфигурации -> сбор данных -> отображение -> (опционально) плагины.
class App {
public:
    explicit App(const Config& config);

    // Точка входа: запускает приложение в режиме, указанном в конфигурации.
    // Возвращает код возврата процесса.
    int run();

private:
    Config config_;

    // Обрабатывает Ctrl+C в --live режиме для корректного завершения
    static volatile bool running_;
    static void signalHandler(int signum);

    void runNormal();
    void runLive();
    void runShort();
    void runJson();
    void runGame();
    void runBenchmark();

    // Находит и запускает все исполняемые плагины из каталога plugins/,
    // выводя их результат после основной информации.
    void runPlugins() const;

    // Ищет каталог plugins/ рядом с исполняемым файлом или в стандартных
    // местах установки (/usr/local/share/sysflex/plugins, ~/.sysflex/plugins)
    [[nodiscard]] std::vector<std::string> findPluginPaths() const;
};

} // namespace sysflex
