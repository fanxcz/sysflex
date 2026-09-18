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
#include <atomic>
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

    // Флаг «продолжать работу», который сбрасывает обработчик сигнала.
    // Именно std::atomic, а не volatile: обращение к нему происходит из
    // обработчика сигнала, и только lock-free атомик имеет там гарантированное
    // поведение (volatile не запрещает компилятору кэшировать значение).
    static std::atomic<bool> running_;
    static_assert(std::atomic<bool>::is_always_lock_free,
                  "std::atomic<bool> обязан быть lock-free для использования в обработчике сигнала");
    static void signalHandler(int signum);

    // Создаёт монитор с настройками из конфигурации
    [[nodiscard]] SystemMonitor makeMonitor() const;

    // Снимает два замера с небольшой паузой: первый инициализирует счётчики
    // дельт (CPU/сеть/диск/процессы), второй уже даёт осмысленные скорости.
    [[nodiscard]] static SystemInfo warmUpSnapshot(SystemMonitor& monitor, int pauseMs);

    void runNormal();
    void runLive();
    void runShort();
    void runJson();
    void runGame();
    void runBenchmark();
    void runListThemes() const;

    // Находит и запускает все исполняемые плагины из каталога plugins/,
    // выводя их результат после основной информации.
    void runPlugins() const;

    // Ищет каталог plugins/ рядом с исполняемым файлом или в стандартных
    // местах установки (/usr/local/share/sysflex/plugins, ~/.sysflex/plugins)
    [[nodiscard]] std::vector<std::string> findPluginPaths() const;
};

} // namespace sysflex
