// display.hpp — вывод информации о системе в терминал в различных режимах:
// полный, короткий, JSON, игровой, живой (со спарклайнами), бенчмарк.
#pragma once

#include <string>
#include <unordered_map>
#include "system_info.hpp"
#include "themes.hpp"
#include "sparkline.hpp"
#include "config.hpp"

namespace sysflex {

// Класс, отвечающий за визуализацию SystemInfo в терминале.
class Display {
public:
    // noAscii отключает вывод ASCII-арт логотипа дистрибутива в printFull()
    // (полезно для узких терминалов или запуска в скриптах/логах).
    explicit Display(const Theme& theme, bool noAscii = false);

    // Полный вывод (режим по умолчанию), с ASCII-артом дистрибутива
    void printFull(const SystemInfo& info) const;

    // Компактный однострочный/короткий вывод
    void printShort(const SystemInfo& info) const;

    // Вывод в формате JSON (для парсинга сторонними инструментами)
    void printJson(const SystemInfo& info) const;

    // Игровой режим: фокус на GPU/CPU/RAM, минимум лишнего
    void printGame(const SystemInfo& info) const;

    // Один "кадр" живого режима: полный дашборд + спарклайны истории
    void printLiveFrame(const SystemInfo& info,
                         std::unordered_map<std::string, Sparkline>& sparks) const;

    // Отчёт по результатам бенчмарка
    void printBenchmarkResult(double cpuScore, double memScore, double diskScore,
                               double totalScore, long durationMs) const;

    // Выводит вывод плагина в едином стиле оформления
    void printPluginOutput(const std::string& pluginName, const std::string& output) const;

private:
    Theme theme_;
    bool noAscii_ = false;

    // Вспомогательные функции форматирования строк с учётом темы
    [[nodiscard]] std::string header(const std::string& title) const;
    [[nodiscard]] std::string label(const std::string& text) const;
    [[nodiscard]] std::string colorizeByPercent(const std::string& text, double percent) const;
    [[nodiscard]] std::string separator(int width = 50) const;
};

} // namespace sysflex
