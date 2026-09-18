// config.hpp — конфигурация приложения и разбор аргументов командной строки
#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include "utils.hpp"
#include "themes.hpp"

namespace sysflex {

// Режим работы приложения
enum class Mode {
    Normal,     // Разовый полный вывод информации (флаг --once)
    Live,       // Живой режим обновления со спарклайнами (по умолчанию — работает,
                // пока пользователь сам не остановит его через Ctrl+C)
    Short,      // Компактный однострочный/короткий вывод
    Json,       // Вывод в формате JSON
    Game,       // Игровой режим (фокус на GPU/CPU)
    Bench,      // Режим бенчмарка (нагрузочный тест системы)
    ListThemes  // Показать список доступных тем и выйти (--list-themes)
};

// Структура конфигурации, заполняется из конфиг-файла и аргументов командной строки
struct Config {
    // По умолчанию sysflex запускается в живом режиме: экран обновляется
    // каждую секунду и работает бесконечно, пока пользователь сам не нажмёт
    // Ctrl+C. Для разового снимка (старое поведение по умолчанию до 2.0)
    // используйте флаг --once.
    Mode mode = Mode::Live;
    std::string theme = "default";   // Название темы оформления
    int interval = 1;                // Интервал обновления в секундах (для --live)
    int historyLength = 40;          // Длина истории для спарклайнов
    bool noColor = false;            // Отключить ANSI-цвета
    bool showHelp = false;
    bool showVersion = false;
    bool enablePlugins = true;       // Запускать ли пользовательские плагины
    int topProcessCount = 5;         // Сколько процессов показывать в топе
    bool noAscii = false;            // Не выводить ASCII-арт логотип дистрибутива
    int execTimeoutSec = 3;          // Таймаут (сек) для внешних команд (docker, bluetoothctl и т.п.)
    // Как часто (сек) перечитывать данные, требующие запуска внешних утилит
    // (nvidia-smi, docker, lsusb, bluetoothctl, playerctl, git, who).
    // Всё остальное читается из /proc и /sys и обновляется каждый кадр.
    double slowRefreshSec = 5.0;
    std::string configPath;          // Явный путь к конфиг-файлу (--config), пусто = автопоиск

    // Выводит справку по использованию программы
    static void printHelp() {
        std::cout <<
            "sysflex — гибкий системный монитор для Linux\n\n"
            "Использование: sysflex [ОПЦИИ]\n\n"
            "Режимы:\n"
            "  (без опций)         Живой режим: обновляется каждую секунду,\n"
            "                      пока вы сами не остановите его (Ctrl+C)\n"
            "  --once              Разовый вывод полной информации и выход\n"
            "  --live              То же самое, что и режим по умолчанию (явно)\n"
            "  --short             Компактный вывод (разовый)\n"
            "  --json              Вывод в формате JSON (разовый)\n"
            "  --game              Игровой режим (фокус на GPU/CPU/RAM, разовый)\n"
            "  --bench             Запустить встроенный бенчмарк системы\n"
            "  --list-themes       Показать доступные темы оформления и выйти\n\n"
            "Опции:\n"
            "  --theme NAME        Тема оформления (список: --list-themes), по умолчанию default\n"
            "  --interval N        Интервал обновления в секундах (для --live), по умолчанию 1\n"
            "  --history N         Длина истории спарклайнов, по умолчанию 40\n"
            "  --top N             Сколько процессов показывать в топе, по умолчанию 5\n"
            "  --timeout N         Таймаут (сек) для внешних команд (docker/bluetoothctl/…), по умолчанию 3\n"
            "  --slow-refresh N    Как часто (сек) перечитывать данные внешних утилит\n"
            "                      (GPU, docker, USB, Bluetooth, плеер, git), по умолчанию 5;\n"
            "                      0 — перечитывать каждый кадр\n"
            "  --config PATH       Прочитать настройки из указанного файла\n"
            "  --no-color          Отключить цветной вывод\n"
            "  --no-plugins        Не запускать плагины из каталога plugins/\n"
            "  --no-ascii          Не выводить ASCII-арт логотип дистрибутива\n"
            "  -h, --help          Показать эту справку\n"
            "  -v, --version       Показать версию\n\n"
            "Конфиг-файл:\n"
            "  Настройки по умолчанию можно задать в ~/.config/sysflex/config\n"
            "  (или в файле, указанном в $SYSFLEX_CONFIG / --config) в формате\n"
            "  key=value, например: theme=dracula. Аргументы командной строки\n"
            "  имеют приоритет.\n\n"
            "Примеры:\n"
            "  sysflex                          # живой дашборд, Ctrl+C для выхода\n"
            "  sysflex --theme dracula\n"
            "  sysflex --interval 1 --history 60\n"
            "  sysflex --once --json > status.json\n"
            "  sysflex --once --short\n"
            "  sysflex --top 10 --timeout 1\n";
    }

    // Применяет одну пару key=value из конфиг-файла к объекту Config.
    // Используется как из loadFromFile(), так и потенциально из будущих
    // источников конфигурации.
    static void applyKeyValue(Config& cfg, const std::string& rawKey, const std::string& rawValue) {
        std::string key = utils::toLowerStr(utils::trim(rawKey));
        std::string value = utils::trim(rawValue);
        auto asBool = [](const std::string& v) {
            std::string lv = utils::toLowerStr(v);
            return lv == "1" || lv == "true" || lv == "yes" || lv == "on";
        };

        if (key == "theme") cfg.theme = value;
        else if (key == "interval") cfg.interval = static_cast<int>(utils::toLong(value, cfg.interval));
        else if (key == "history") cfg.historyLength = static_cast<int>(utils::toLong(value, cfg.historyLength));
        else if (key == "top") cfg.topProcessCount = static_cast<int>(utils::toLong(value, cfg.topProcessCount));
        else if (key == "timeout") cfg.execTimeoutSec = static_cast<int>(utils::toLong(value, cfg.execTimeoutSec));
        else if (key == "slow_refresh" || key == "slow-refresh") cfg.slowRefreshSec = utils::toDouble(value, cfg.slowRefreshSec);
        else if (key == "no_color" || key == "no-color") cfg.noColor = asBool(value);
        else if (key == "no_plugins" || key == "no-plugins") cfg.enablePlugins = !asBool(value);
        else if (key == "no_ascii" || key == "no-ascii") cfg.noAscii = asBool(value);
        // Неизвестные ключи молча игнорируются, чтобы конфиг оставался
        // совместимым при добавлении новых версий sysflex.
    }

    // Загружает значения по умолчанию из конфиг-файла (если он существует).
    // Порядок выбора пути: --config FILE, затем $SYSFLEX_CONFIG,
    // затем $HOME/.config/sysflex/config.
    // Вызывается ДО основного разбора argv, так что аргументы командной
    // строки всегда имеют приоритет над конфиг-файлом.
    static void loadFromFile(Config& cfg) {
        std::string path = cfg.configPath;
        const bool explicitPath = !path.empty();
        if (!explicitPath) {
            if (const char* envPath = std::getenv("SYSFLEX_CONFIG")) {
                path = envPath;
            } else if (const char* home = std::getenv("HOME")) {
                path = std::string(home) + "/.config/sysflex/config";
            } else {
                return;
            }
        }
        if (!utils::fileExists(path)) {
            // Файл, указанный явно, обязан существовать — молча игнорировать
            // опечатку в пути значит запустить sysflex с чужими настройками.
            if (explicitPath) {
                std::cerr << "sysflex: конфиг-файл не найден: " << path << "\n";
            }
            return;
        }

        std::string content = utils::readFile(path);
        for (auto& rawLine : utils::split(content, '\n')) {
            std::string line = utils::trim(rawLine);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            applyKeyValue(cfg, line.substr(0, eq), line.substr(eq + 1));
        }
    }

    // Разбирает argc/argv в объект Config. Возвращает false, если нужно
    // немедленно завершить работу (например, была запрошена справка).
    static bool parse(int argc, char** argv, Config& cfg) {
        std::vector<std::string> args(argv + 1, argv + argc);

        // --config меняет то, откуда читаются настройки, поэтому argv
        // просматривается дважды: первый проход ищет только путь к файлу.
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "--config" && i + 1 < args.size()) {
                cfg.configPath = args[i + 1];
                break;
            }
            if (utils::startsWith(args[i], "--config=")) {
                cfg.configPath = args[i].substr(std::string("--config=").size());
                break;
            }
        }

        // Теперь применяем конфиг-файл (если есть) — это заполнит значения
        // по умолчанию, которые ниже могут быть переопределены аргументами.
        loadFromFile(cfg);

        for (size_t i = 0; i < args.size(); ++i) {
            const std::string& a = args[i];

            auto nextValue = [&](const std::string& def) -> std::string {
                if (i + 1 < args.size()) {
                    return args[++i];
                }
                return def;
            };

            if (a == "--once") {
                cfg.mode = Mode::Normal;
            } else if (a == "--live") {
                cfg.mode = Mode::Live;
            } else if (a == "--short") {
                cfg.mode = Mode::Short;
            } else if (a == "--json") {
                cfg.mode = Mode::Json;
            } else if (a == "--game") {
                cfg.mode = Mode::Game;
            } else if (a == "--bench") {
                cfg.mode = Mode::Bench;
            } else if (a == "--list-themes") {
                cfg.mode = Mode::ListThemes;
            } else if (a == "--theme") {
                cfg.theme = nextValue("default");
            } else if (utils::startsWith(a, "--theme=")) {
                cfg.theme = a.substr(std::string("--theme=").size());
            } else if (a == "--config") {
                nextValue(""); // значение уже обработано предварительным проходом
            } else if (utils::startsWith(a, "--config=")) {
                // то же: путь применён до loadFromFile()
            } else if (a == "--interval") {
                cfg.interval = static_cast<int>(utils::toLong(nextValue("1"), 1));
            } else if (utils::startsWith(a, "--interval=")) {
                cfg.interval = static_cast<int>(utils::toLong(a.substr(std::string("--interval=").size()), 1));
            } else if (a == "--slow-refresh") {
                cfg.slowRefreshSec = utils::toDouble(nextValue("5"), 5.0);
            } else if (utils::startsWith(a, "--slow-refresh=")) {
                cfg.slowRefreshSec = utils::toDouble(a.substr(std::string("--slow-refresh=").size()), 5.0);
            } else if (a == "--history") {
                cfg.historyLength = static_cast<int>(utils::toLong(nextValue("40"), 40));
            } else if (utils::startsWith(a, "--history=")) {
                cfg.historyLength = static_cast<int>(utils::toLong(a.substr(std::string("--history=").size()), 40));
            } else if (a == "--top") {
                cfg.topProcessCount = static_cast<int>(utils::toLong(nextValue("5"), 5));
            } else if (utils::startsWith(a, "--top=")) {
                cfg.topProcessCount = static_cast<int>(utils::toLong(a.substr(std::string("--top=").size()), 5));
            } else if (a == "--timeout") {
                cfg.execTimeoutSec = static_cast<int>(utils::toLong(nextValue("3"), 3));
            } else if (utils::startsWith(a, "--timeout=")) {
                cfg.execTimeoutSec = static_cast<int>(utils::toLong(a.substr(std::string("--timeout=").size()), 3));
            } else if (a == "--no-color") {
                cfg.noColor = true;
            } else if (a == "--no-plugins") {
                cfg.enablePlugins = false;
            } else if (a == "--no-ascii") {
                cfg.noAscii = true;
            } else if (a == "-h" || a == "--help") {
                cfg.showHelp = true;
            } else if (a == "-v" || a == "--version") {
                cfg.showVersion = true;
            } else {
                std::cerr << "Неизвестная опция: " << a << "\n";
                std::cerr << "Используйте --help для справки.\n";
                return false;
            }
        }

        if (cfg.interval < 1) cfg.interval = 1;
        if (cfg.interval > 3600) cfg.interval = 3600;
        if (cfg.historyLength < 5) cfg.historyLength = 5;
        if (cfg.historyLength > 10000) cfg.historyLength = 10000;
        if (cfg.topProcessCount < 1) cfg.topProcessCount = 1;
        if (cfg.topProcessCount > 50) cfg.topProcessCount = 50;
        if (cfg.execTimeoutSec < 0) cfg.execTimeoutSec = 0;
        if (cfg.slowRefreshSec < 0.0) cfg.slowRefreshSec = 0.0;

        // Неизвестная тема: предупреждаем и падаем на default вместо тихого
        // запуска «не с тем оформлением» — опечатку в --theme иначе не заметить.
        if (!isKnownTheme(cfg.theme)) {
            std::cerr << "sysflex: неизвестная тема '" << cfg.theme
                      << "', используется 'default'. Список тем: sysflex --list-themes\n";
            cfg.theme = "default";
        }

        return true;
    }
};

// Текущая версия приложения
inline const char* VERSION = "2.1.0";

} // namespace sysflex
