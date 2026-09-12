// test_main.cpp — простые модульные тесты для sysflex.
// Не используется внешний фреймворк тестирования (по требованию "без внешних
// зависимостей") — реализован минималистичный самодельный test-runner.
#include "sysflex/utils.hpp"
#include "sysflex/sparkline.hpp"
#include "sysflex/system_info.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <string>

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

int main() {
    std::cout << "=== Запуск модульных тестов sysflex ===\n\n";

    testTrim();
    testFormatBytes();
    testFormatTime();
    testSplitAndMisc();
    testSparkline();
    testSystemInfoUpdate();

    std::cout << "\n=== Итог: " << g_passed << " пройдено, " << g_failed << " провалено ===\n";
    return (g_failed == 0) ? 0 : 1;
}
