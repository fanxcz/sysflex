// test_main.cpp — простые модульные тесты для sysflex.
// Не используется внешний фреймворк тестирования (по требованию "без внешних
// зависимостей") — реализован минималистичный самодельный test-runner.
#include "sysflex/utils.hpp"
#include "sysflex/sparkline.hpp"
#include "sysflex/system_info.hpp"
#include "sysflex/config.hpp"
#include "sysflex/display.hpp"
#include "sysflex/themes.hpp"
#include "sysflex/ascii_art.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>

using namespace sysflex;

// Счётчики пройденных/провалившихся проверок
static int g_passed = 0;
static int g_failed = 0;

// Простой макрос проверки: печатает результат и увеличивает соответствующий счётчик
#define CHECK(cond, msg) \
    do { \
        if (cond) { \
            ++g_passed; \
            std::cout << "  [OK]   " << msg << "\n"; \
        } else { \
            ++g_failed; \
            std::cout << "  [FAIL] " << msg << "\n"; \
        } \
    } while (0)

// --- Тесты utils::trim ---
void testTrim() {
    std::cout << "Тест: utils::trim\n";
    CHECK(utils::trim("  hello  ") == "hello", "убирает пробелы по краям");
    CHECK(utils::trim("hello") == "hello", "не трогает строку без пробелов");
    CHECK(utils::trim("   ") == "", "строка из пробелов становится пустой");
    CHECK(utils::trim("\t\nhi\r\n") == "hi", "убирает табы и переводы строк");
}

// --- Тесты utils::formatBytes ---
void testFormatBytes() {
    std::cout << "Тест: utils::formatBytes\n";
    CHECK(utils::formatBytes(0) == "0.0 B", "ноль байт форматируется корректно");
    CHECK(utils::formatBytes(1024) == "1.0 KB", "1024 байта = 1.0 KB");
    CHECK(utils::formatBytes(1024.0 * 1024.0) == "1.0 MB", "1 МБ форматируется корректно");
    CHECK(utils::formatBytes(1024.0 * 1024.0 * 1024.0) == "1.0 GB", "1 ГБ форматируется корректно");
    CHECK(utils::formatBytes(500) == "500.0 B", "значения меньше килобайта остаются в байтах");
}

// --- Тесты utils::formatTime ---
void testFormatTime() {
    std::cout << "Тест: utils::formatTime\n";
    CHECK(utils::formatTime(0) == "0s", "ноль секунд форматируется как 0s");
    CHECK(utils::formatTime(59) == "59s", "59 секунд без минут");
    CHECK(utils::formatTime(60) == "1m 0s", "60 секунд равно 1 минуте");
    CHECK(utils::formatTime(3661) == "1h 1m 1s", "3661 секунда = 1ч 1м 1с");
    CHECK(utils::formatTime(90000) == "1d 1h 0m 0s", "90000 секунд включает дни");
}

// --- Тесты utils::split / trim edge cases ---
void testSplitAndMisc() {
    std::cout << "Тест: utils::split и вспомогательные функции\n";
    auto parts = utils::split("a,b,,c", ',');
    CHECK(parts.size() == 4, "split сохраняет пустые токены между разделителями");
    CHECK(parts[0] == "a" && parts[1] == "b" && parts[3] == "c", "split корректно разбивает строку");

    auto ws = utils::splitWs("  foo   bar baz ");
    CHECK(ws.size() == 3 && ws[0] == "foo" && ws[2] == "baz", "splitWs игнорирует лишние пробелы");

    CHECK(utils::startsWith("sysflex", "sys"), "startsWith находит существующий префикс");
    CHECK(!utils::startsWith("sysflex", "flex"), "startsWith не находит несуществующий префикс");

    CHECK(std::fabs(utils::toDouble("3.14") - 3.14) < 1e-9, "toDouble корректно парсит число");
    CHECK(utils::toDouble("не число", -1.0) == -1.0, "toDouble возвращает значение по умолчанию при ошибке");
}

// --- Тесты Sparkline ---
void testSparkline() {
    std::cout << "Тест: Sparkline\n";
    Sparkline s(5);
    CHECK(s.history().empty(), "новый спарклайн имеет пустую историю");

    for (double v : {10.0, 20.0, 30.0, 40.0, 50.0}) s.add(v);
    CHECK(s.history().size() == 5, "история хранит добавленные значения");

    s.add(60.0); // должно вытеснить самое старое значение (10.0)
    CHECK(s.history().size() == 5, "история не превышает maxHistory при переполнении");
    CHECK(s.history().front() == 20.0, "старое значение вытесняется при переполнении");

    std::string rendered = s.render(100.0);
    CHECK(rendered.size() > 0, "render() возвращает непустую строку");
    CHECK(s.history().back() == 60.0, "последнее добавленное значение сохраняется");

    Sparkline empty(5);
    CHECK(empty.render(100.0).empty(), "render() пустой истории возвращает пустую строку");
}

// --- Тест базовой работы SystemMonitor::update() ---
void testSystemInfoUpdate() {
    std::cout << "Тест: SystemMonitor::update()\n";
    SystemMonitor monitor;
    SystemInfo info1 = monitor.update();

    CHECK(!info1.hostname.empty(), "hostname определяется");
    CHECK(!info1.kernelVersion.empty(), "версия ядра определяется");
    CHECK(info1.ramTotalKB > 0, "общий объём RAM больше нуля");
    CHECK(info1.cpuCoreCount > 0, "количество ядер CPU больше нуля");
    CHECK(info1.diskTotalBytes > 0, "объём диска определяется");
    CHECK(info1.healthScore >= 0 && info1.healthScore <= 100, "healthScore в диапазоне [0, 100]");

    // Второй вызов update() должен корректно работать (расчёт дельт CPU/сети/диска)
    SystemInfo info2 = monitor.update();
    CHECK(info2.cpuUsagePercent >= 0.0 && info2.cpuUsagePercent <= 100.0, "cpuUsagePercent в допустимом диапазоне после второго замера");
    CHECK(info2.ramUsagePercent >= 0.0 && info2.ramUsagePercent <= 100.0, "ramUsagePercent в допустимом диапазоне");
}

// --- Тесты utils::formatSpeed / formatPercent / progressBar ---
void testFormattingHelpers() {
    std::cout << "Тест: utils::formatSpeed / formatPercent / progressBar\n";
    CHECK(utils::formatSpeed(1024) == "1.0 KB/s", "formatSpeed добавляет суффикс /s");
    CHECK(utils::formatSpeed(0) == "0.0 B/s", "нулевая скорость форматируется корректно");

    CHECK(utils::formatPercent(12.3456) == "12.3%", "formatPercent округляет до одного знака");
    CHECK(utils::formatPercent(0) == "0.0%", "нулевой процент форматируется корректно");

    std::string half = utils::progressBar(50.0, 10);
    CHECK(half == "[#####-----]", "progressBar рисует половину полосы");
    CHECK(utils::progressBar(0.0, 10) == "[----------]", "progressBar для 0% пустая");
    CHECK(utils::progressBar(100.0, 10) == "[##########]", "progressBar для 100% полностью заполнена");
    // Значения вне диапазона не должны приводить к отрицательной длине строки
    CHECK(utils::progressBar(-50.0, 10).size() == 12, "progressBar ограничивает значения < 0");
    CHECK(utils::progressBar(500.0, 10).size() == 12, "progressBar ограничивает значения > 100");
}

// --- Тесты utils::toULong (безопасный парсинг) ---
void testSafeParsing() {
    std::cout << "Тест: utils::toULong (безопасный парсинг)\n";
    CHECK(utils::toULong("42") == 42UL, "toULong парсит десятичное число");
    CHECK(utils::toULong("ff", 0, 16) == 255UL, "toULong поддерживает шестнадцатеричную базу");
    CHECK(utils::toULong("не число", 7) == 7UL, "toULong возвращает значение по умолчанию вместо исключения");
    CHECK(utils::toULong("", 7) == 7UL, "toULong переживает пустую строку");
    CHECK(utils::toULong("00000000", 9, 16) == 0UL, "toULong разбирает нули из /proc/net/route");

    CHECK(utils::endsWith("sda1", "a1"), "endsWith находит существующий суффикс");
    CHECK(!utils::endsWith("sda", "a1"), "endsWith не находит несуществующий суффикс");
    CHECK(utils::endsWith("abc", ""), "endsWith с пустым суффиксом возвращает true");
}

// --- Тесты utils::jsonEscape / jsonNumber ---
void testJsonHelpers() {
    std::cout << "Тест: utils::jsonEscape / jsonNumber\n";
    CHECK(utils::jsonEscape("plain") == "plain", "обычная строка не меняется");
    CHECK(utils::jsonEscape("a\"b") == "a\\\"b", "кавычки экранируются");
    CHECK(utils::jsonEscape("a\\b") == "a\\\\b", "обратный слэш экранируется");
    CHECK(utils::jsonEscape("a\nb") == "a\\nb", "перевод строки экранируется");
    CHECK(utils::jsonEscape(std::string("a\x01" "b")) == "a\\u0001b", "управляющие символы уходят в \\uXXXX");
    CHECK(utils::jsonEscape("тест") == "тест", "корректный UTF-8 сохраняется как есть");
    // Одинокий continuation-байт — невалидный UTF-8, обязан замениться на U+FFFD,
    // иначе любой JSON-парсер отвергнет документ.
    CHECK(utils::jsonEscape(std::string("\x80", 1)) == "\xEF\xBF\xBD",
          "невалидный UTF-8 заменяется на U+FFFD");

    CHECK(utils::jsonNumber(1.5) == "1.5000", "jsonNumber печатает число с фиксированной точностью");
    CHECK(utils::jsonNumber(std::nan("")) == "0", "NaN заменяется на 0 (недопустим в JSON)");
    CHECK(utils::jsonNumber(std::numeric_limits<double>::infinity()) == "0", "Infinity заменяется на 0");
    CHECK(utils::jsonNumber(-std::numeric_limits<double>::infinity()) == "0", "-Infinity заменяется на 0");
}

// --- Тесты тем оформления ---
void testThemes() {
    std::cout << "Тест: темы оформления\n";
    CHECK(isKnownTheme("default"), "тема default существует");
    CHECK(isKnownTheme("Dracula"), "поиск темы регистронезависим");
    CHECK(!isKnownTheme("не-тема"), "несуществующая тема не находится");

    auto names = themeNames();
    CHECK(names.size() == allThemes().size(), "themeNames() возвращает все темы");
    CHECK(std::is_sorted(names.begin(), names.end()), "themeNames() отсортирован по алфавиту");

    Theme colored = getTheme("dracula", false);
    CHECK(!colored.primary.empty() && !colored.reset.empty(), "цветная тема заполняет escape-коды");

    Theme plain = getTheme("dracula", true);
    CHECK(plain.primary.empty() && plain.reset.empty() && plain.bold.empty(),
          "noColor=true полностью убирает escape-коды");
    CHECK(plain.colorForPercent(99.0).empty(), "noColor: colorForPercent не возвращает кодов");

    // Неизвестная тема должна тихо деградировать в default, а не падать
    Theme fallback = getTheme("такой-темы-нет", false);
    CHECK(fallback.name == "default", "getTheme для неизвестного имени возвращает default");

    Theme t = getTheme("default", false);
    CHECK(t.colorForPercent(10) == t.success, "низкая нагрузка окрашивается в success");
    CHECK(t.colorForPercent(70) == t.warning, "средняя нагрузка окрашивается в warning");
    CHECK(t.colorForPercent(95) == t.danger, "высокая нагрузка окрашивается в danger");
}

// --- Тесты ASCII-арта ---
void testAsciiArt() {
    std::cout << "Тест: ascii::artForOs\n";
    CHECK(!ascii::artForOs("debian gnu/linux 12").empty(), "для Debian находится свой логотип");
    CHECK(ascii::artForOs("ubuntu 24.04") != ascii::artForOs("debian gnu/linux 12"),
          "логотипы Ubuntu и Debian различаются");
    CHECK(ascii::artForOs("полностью незнакомая ос") == ascii::genericArt(),
          "для неизвестной ОС возвращается generic-логотип");
    for (const char* os : {"arch linux", "fedora", "ubuntu", "debian"}) {
        auto art = ascii::artForOs(os);
        bool nonEmptyLines = !art.empty();
        for (const auto& line : art) {
            if (!line.empty()) { nonEmptyLines = true; break; }
        }
        CHECK(nonEmptyLines, std::string("логотип для '") + os + "' содержит непустые строки");
    }
}

// --- Тесты Config::applyKeyValue (конфиг-файл) ---
void testConfigKeyValue() {
    std::cout << "Тест: Config::applyKeyValue\n";
    Config cfg;
    Config::applyKeyValue(cfg, "theme", "nord");
    CHECK(cfg.theme == "nord", "ключ theme применяется");

    Config::applyKeyValue(cfg, "  TOP ", " 12 ");
    CHECK(cfg.topProcessCount == 12, "ключи и значения обрезаются от пробелов, регистр не важен");

    Config::applyKeyValue(cfg, "no_color", "true");
    CHECK(cfg.noColor, "no_color=true включает noColor");
    Config::applyKeyValue(cfg, "no_color", "false");
    CHECK(!cfg.noColor, "no_color=false выключает noColor");

    Config::applyKeyValue(cfg, "no-plugins", "yes");
    CHECK(!cfg.enablePlugins, "no-plugins=yes отключает плагины");
    Config::applyKeyValue(cfg, "no_ascii", "1");
    CHECK(cfg.noAscii, "no_ascii=1 включает noAscii");

    Config::applyKeyValue(cfg, "slow_refresh", "2.5");
    CHECK(std::fabs(cfg.slowRefreshSec - 2.5) < 1e-9, "slow_refresh поддерживает дробные секунды");

    Config::applyKeyValue(cfg, "interval", "не-число");
    CHECK(cfg.interval == 1, "нечисловое значение не портит настройку");

    Config before = cfg;
    Config::applyKeyValue(cfg, "совершенно_неизвестный_ключ", "x");
    CHECK(cfg.theme == before.theme && cfg.interval == before.interval,
          "неизвестный ключ игнорируется без побочных эффектов");
}

// Вспомогательная функция: собирает argv из списка строк и вызывает Config::parse.
// Возвращает результат parse, заполняя cfg.
static bool parseArgs(const std::vector<std::string>& args, Config& cfg) {
    std::vector<std::vector<char>> storage;
    std::vector<char*> argv;
    // Резервируем заранее: реаллокация storage обнулила бы уже сохранённые
    // в argv указатели.
    storage.reserve(args.size() + 1);

    std::string prog = "sysflex";
    storage.emplace_back(prog.begin(), prog.end());
    storage.back().push_back('\0');
    argv.push_back(storage.back().data());

    for (const auto& a : args) {
        storage.emplace_back(a.begin(), a.end());
        storage.back().push_back('\0');
        argv.push_back(storage.back().data());
    }
    return Config::parse(static_cast<int>(argv.size()), argv.data(), cfg);
}

// Перехватывает stderr на время вызова, чтобы проверки не засоряли вывод тестов
template <typename Fn>
static std::string captureStderr(Fn&& fn) {
    std::ostringstream capture;
    std::streambuf* saved = std::cerr.rdbuf(capture.rdbuf());
    fn();
    std::cerr.rdbuf(saved);
    return capture.str();
}

// --- Тесты разбора командной строки ---
void testConfigParse() {
    std::cout << "Тест: Config::parse\n";
    {
        Config cfg;
        bool ok = parseArgs({"--once", "--theme", "dracula", "--top", "10"}, cfg);
        CHECK(ok, "корректные аргументы разбираются успешно");
        CHECK(cfg.mode == Mode::Normal, "--once переключает режим в Normal");
        CHECK(cfg.theme == "dracula", "--theme NAME применяется");
        CHECK(cfg.topProcessCount == 10, "--top N применяется");
    }
    {
        Config cfg;
        CHECK(cfg.mode == Mode::Live, "режим по умолчанию — Live");
    }
    {
        Config cfg;
        parseArgs({"--theme=nord", "--interval=3", "--history=60", "--timeout=7"}, cfg);
        CHECK(cfg.theme == "nord", "поддерживается форма --theme=VALUE");
        CHECK(cfg.interval == 3, "поддерживается форма --interval=VALUE");
        CHECK(cfg.historyLength == 60, "поддерживается форма --history=VALUE");
        CHECK(cfg.execTimeoutSec == 7, "поддерживается форма --timeout=VALUE");
    }
    {
        Config cfg;
        parseArgs({"--interval"}, cfg);
        CHECK(cfg.interval == 1, "--interval без значения даёт значение по умолчанию 1");
    }
    {
        Config cfg;
        parseArgs({"--json"}, cfg);
        CHECK(cfg.mode == Mode::Json, "--json переключает режим в Json");
    }
    {
        Config cfg;
        parseArgs({"--list-themes"}, cfg);
        CHECK(cfg.mode == Mode::ListThemes, "--list-themes переключает режим в ListThemes");
    }
    {
        Config cfg;
        parseArgs({"--top", "-5", "--interval", "0", "--history", "1"}, cfg);
        CHECK(cfg.topProcessCount == 1, "отрицательное --top ограничивается снизу единицей");
        CHECK(cfg.interval == 1, "нулевой --interval ограничивается снизу единицей");
        CHECK(cfg.historyLength == 5, "слишком короткий --history ограничивается снизу");
    }
    {
        Config cfg;
        parseArgs({"--top", "10000"}, cfg);
        CHECK(cfg.topProcessCount == 50, "слишком большое --top ограничивается сверху");
    }
    {
        Config cfg;
        bool ok = parseArgs({"--help"}, cfg);
        CHECK(ok && cfg.showHelp, "--help выставляет флаг справки");
    }
    {
        Config cfg;
        parseArgs({"--version"}, cfg);
        CHECK(cfg.showVersion, "--version выставляет флаг версии");
    }
    {
        Config cfg;
        std::string err;
        bool ok = true;
        err = captureStderr([&] { ok = parseArgs({"--несуществующий-флаг"}, cfg); });
        CHECK(!ok, "неизвестная опция приводит к ошибке разбора");
        CHECK(err.find("Неизвестная опция") != std::string::npos, "неизвестная опция поясняется в stderr");
    }
    {
        Config cfg;
        std::string err = captureStderr([&] { parseArgs({"--theme", "такой-темы-нет"}, cfg); });
        CHECK(cfg.theme == "default", "неизвестная тема откатывается к default");
        CHECK(err.find("неизвестная тема") != std::string::npos, "неизвестная тема предупреждает в stderr");
    }
    {
        Config cfg;
        std::string err = captureStderr([&] { parseArgs({"--config", "/nonexistent/sysflex.cfg"}, cfg); });
        CHECK(err.find("конфиг-файл не найден") != std::string::npos,
              "отсутствующий файл из --config не игнорируется молча");
    }
}

// --- Тесты чтения конфиг-файла ---
void testConfigFile() {
    std::cout << "Тест: Config::loadFromFile\n";
    const std::string path = "/tmp/sysflex_test_config.cfg";
    {
        std::ofstream out(path);
        out << "# комментарий\n";
        out << "; ещё комментарий\n";
        out << "\n";
        out << "theme=gruvbox\n";
        out << "top=7\n";
        out << "slow_refresh=2\n";
        out << "строка без знака равно\n";
        out << "no_color=on\n";
    }
    Config cfg;
    cfg.configPath = path;
    Config::loadFromFile(cfg);
    CHECK(cfg.theme == "gruvbox", "тема читается из файла");
    CHECK(cfg.topProcessCount == 7, "top читается из файла");
    CHECK(std::fabs(cfg.slowRefreshSec - 2.0) < 1e-9, "slow_refresh читается из файла");
    CHECK(cfg.noColor, "no_color=on понимается как true");

    // Аргументы командной строки обязаны перекрывать файл
    Config cfg2;
    parseArgs({"--config", path, "--theme", "nord"}, cfg2);
    CHECK(cfg2.theme == "nord", "аргумент командной строки важнее значения из файла");
    CHECK(cfg2.topProcessCount == 7, "не переопределённые аргументом значения берутся из файла");

    std::remove(path.c_str());
}

// Мини-проверка структурной корректности JSON: баланс скобок вне строк,
// отсутствие запрещённых в JSON литералов nan/inf.
static bool looksLikeValidJson(const std::string& text, std::string& problem) {
    int braces = 0, brackets = 0;
    bool inString = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (inString) {
            if (c == '\\') { ++i; continue; }
            if (c == '"') inString = false;
            continue;
        }
        switch (c) {
            case '"': inString = true; break;
            case '{': ++braces; break;
            case '}': --braces; break;
            case '[': ++brackets; break;
            case ']': --brackets; break;
            default: break;
        }
        if (braces < 0 || brackets < 0) { problem = "незакрытая скобка"; return false; }
    }
    if (inString) { problem = "незакрытая строка"; return false; }
    if (braces != 0) { problem = "не сбалансированы фигурные скобки"; return false; }
    if (brackets != 0) { problem = "не сбалансированы квадратные скобки"; return false; }
    for (const char* bad : {"nan", "inf", "-inf", "NaN", "Infinity"}) {
        if (text.find(bad) != std::string::npos) { problem = std::string("найдено недопустимое значение: ") + bad; return false; }
    }
    return true;
}

// --- Тест вывода JSON: документ обязан оставаться валидным ---
void testJsonOutput() {
    std::cout << "Тест: Display::printJson\n";
    SystemMonitor monitor;
    monitor.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    SystemInfo info = monitor.update();

    std::ostringstream capture;
    std::streambuf* saved = std::cout.rdbuf(capture.rdbuf());
    Display display(getTheme("default", true), true);
    display.printJson(info);
    std::cout.rdbuf(saved);

    const std::string json = capture.str();
    std::string problem;
    bool valid = looksLikeValidJson(json, problem);
    CHECK(valid, std::string("вывод printJson структурно валиден") + (valid ? "" : " (" + problem + ")"));
    CHECK(utils::startsWith(utils::trim(json), "{"), "JSON начинается с {");
    CHECK(utils::endsWith(utils::trim(json), "}"), "JSON заканчивается на }");
    CHECK(json.find("\"hostname\"") != std::string::npos, "в JSON есть поле hostname");
    CHECK(json.find("\"health_score\"") != std::string::npos, "в JSON есть поле health_score");
    CHECK(json.find("\"extras2\"") != std::string::npos, "в JSON есть блок extras2");
    CHECK(json.find("\033[") == std::string::npos, "в JSON нет ANSI escape-кодов");

    // Отрицательные «недоступные» значения не должны ломать документ
    SystemInfo synthetic;
    synthetic.cpuTemperatureC = std::nan("");
    synthetic.gpuMemUsedMB = -std::numeric_limits<double>::infinity();
    std::ostringstream capture2;
    std::streambuf* saved2 = std::cout.rdbuf(capture2.rdbuf());
    display.printJson(synthetic);
    std::cout.rdbuf(saved2);
    std::string problem2;
    CHECK(looksLikeValidJson(capture2.str(), problem2),
          std::string("JSON остаётся валидным при NaN/Inf в данных") + (problem2.empty() ? "" : " (" + problem2 + ")"));
}

// --- Тесты SystemMonitor: кэш внешних данных и диапазоны значений ---
void testSystemMonitorExtras() {
    std::cout << "Тест: SystemMonitor (расширенные метрики и кэш)\n";
    SystemMonitor monitor(3);
    CHECK(std::fabs(monitor.slowRefreshSeconds() - 5.0) < 1e-9, "интервал обновления внешних данных по умолчанию 5 с");
    monitor.setSlowRefreshSeconds(12.5);
    CHECK(std::fabs(monitor.slowRefreshSeconds() - 12.5) < 1e-9, "setSlowRefreshSeconds меняет интервал");

    SystemInfo first = monitor.update();
    CHECK(first.processCount > 0, "процессы обнаруживаются");
    CHECK(static_cast<int>(first.topProcesses.size()) <= 3, "--top ограничивает размер списка процессов");
    CHECK(first.perCoreUsagePercent.size() == static_cast<size_t>(first.cpuCoreCount),
          "загрузка собирается по каждому логическому ядру");
    CHECK(!first.cpuArchitecture.empty(), "архитектура CPU определяется");
    CHECK(!first.systemTimezone.empty(), "часовой пояс определяется");
    CHECK(first.blockDeviceCount >= 0, "число блочных устройств неотрицательно");
    CHECK(!first.defaultGateway.empty(), "поле шлюза заполнено (или значением н/д)");
    CHECK(first.uptimeSeconds > 0, "аптайм больше нуля");

    const double maxPercent = 100.0 * static_cast<double>(first.cpuCoreCount > 0 ? first.cpuCoreCount : 1);
    for (const auto& p : first.topProcesses) {
        CHECK(p.cpuPercent >= 0.0 && p.cpuPercent <= maxPercent,
              "CPU% процесса в диапазоне [0, 100*ядра] для pid " + std::to_string(p.pid));
        CHECK(p.memPercent >= 0.0, "MEM% процесса неотрицателен");
    }

    // Второй снимок: дельты скоростей обязаны попасть в допустимый диапазон
    SystemInfo second = monitor.update();
    CHECK(second.netDownloadBytesPerSec >= 0.0, "скорость загрузки неотрицательна");
    CHECK(second.netUploadBytesPerSec >= 0.0, "скорость отдачи неотрицательна");
    CHECK(second.diskReadBytesPerSec >= 0.0, "скорость чтения с диска неотрицательна");
    CHECK(second.ctxSwitchesPerSec >= 0.0, "переключений контекста в секунду неотрицательно");
    CHECK(second.forksPerSec >= 0.0, "форков в секунду неотрицательно");
}


int main() {
    std::cout << "=== Запуск модульных тестов sysflex ===\n\n";

    // Отключаем влияние внешнего окружения: тесты не должны зависеть от
    // случайно существующего ~/.config/sysflex/config.
    setenv("SYSFLEX_CONFIG", "/tmp/sysflex_tests_nonexistent_config", 1);
    unsetenv("HOME");

    testTrim();
    testFormatBytes();
    testFormatTime();
    testSplitAndMisc();
    testFormattingHelpers();
    testSafeParsing();
    testJsonHelpers();
    testThemes();
    testAsciiArt();
    testConfigKeyValue();
    testConfigParse();
    testConfigFile();
    testSparkline();
    testSystemInfoUpdate();
    testSystemMonitorExtras();
    testJsonOutput();

    std::cout << "\n=== Итог: " << g_passed << " пройдено, " << g_failed << " провалено ===\n";
    return (g_failed == 0) ? 0 : 1;
}
