// utils.hpp — вспомогательные функции общего назначения для sysflex
// Здесь собраны функции форматирования строк, чисел, времени и байт,
// а также обёртки для выполнения shell-команд и чтения файлов /proc и /sys.
#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <memory>
#include <array>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cctype>
#include <ctime>
#include <unordered_map>
#include <dirent.h>

namespace sysflex::utils {

// Удаляет пробельные символы в начале и конце строки
inline std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(start, end - start + 1);
}

// Разбивает строку на токены по заданному разделителю
inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        tokens.push_back(item);
    }
    return tokens;
}

// Разбивает строку по любому количеству пробелов/табов подряд
inline std::vector<std::string> splitWs(const std::string& s) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string item;
    while (ss >> item) tokens.push_back(item);
    return tokens;
}

// Проверяет, начинается ли строка с заданного префикса
inline bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

// Проверяет, заканчивается ли строка заданным суффиксом
inline bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Возвращает имена записей в каталоге (без "." и "..").
// Единая точка работы с opendir/readdir: при ошибке возвращает пустой список,
// так что вызывающему коду не нужно проверять nullptr и закрывать каталог.
inline std::vector<std::string> listDirectory(const std::string& path) {
    std::vector<std::string> entries;
    DIR* dir = opendir(path.c_str());
    if (!dir) return entries;
    while (struct dirent* entry = readdir(dir)) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        entries.push_back(name);
    }
    closedir(dir);
    return entries;
}

// Форматирует количество байт в человекочитаемый вид (КБ, МБ, ГБ, ТБ)
inline std::string formatBytes(double bytes, int precision = 1) {
    static const std::array<const char*, 6> units = {"B", "KB", "MB", "GB", "TB", "PB"};
    size_t unitIndex = 0;
    double value = bytes;
    while (std::fabs(value) >= 1024.0 && unitIndex + 1 < units.size()) {
        value /= 1024.0;
        ++unitIndex;
    }
    std::ostringstream oss;
    oss.precision(precision);
    oss << std::fixed << value << " " << units[unitIndex];
    return oss.str();
}

// Форматирует скорость (байт/сек) в человекочитаемый вид с суффиксом /s
inline std::string formatSpeed(double bytesPerSec, int precision = 1) {
    return formatBytes(bytesPerSec, precision) + "/s";
}

// Форматирует время безотказной работы (в секундах) в строку вида "3d 4h 12m"
inline std::string formatTime(long totalSeconds) {
    if (totalSeconds < 0) totalSeconds = 0;
    long days = totalSeconds / 86400;
    long hours = (totalSeconds % 86400) / 3600;
    long minutes = (totalSeconds % 3600) / 60;
    long seconds = totalSeconds % 60;

    std::ostringstream oss;
    if (days > 0) oss << days << "d ";
    if (hours > 0 || days > 0) oss << hours << "h ";
    if (minutes > 0 || hours > 0 || days > 0) oss << minutes << "m ";
    oss << seconds << "s";
    return oss.str();
}

// Форматирует процент с одним знаком после запятой
inline std::string formatPercent(double value, int precision = 1) {
    std::ostringstream oss;
    oss.precision(precision);
    oss << std::fixed << value << "%";
    return oss.str();
}

// Глобальный таймаут (в секундах) для внешних команд, запускаемых через
// execCommand(). Настраивается один раз при старте приложения (--timeout /
// конфиг-файл) и применяется ко всем последующим вызовам, чтобы не тащить
// параметр через десятки мест вызова. 0 или отрицательное значение отключает
// таймаут (не рекомендуется — некоторые внешние команды, такие как
// bluetoothctl или docker с недоступным демоном, могут виснуть навсегда).
inline int& execTimeoutSecRef() {
    static int value = 3;
    return value;
}
inline void setExecTimeoutSec(int seconds) { execTimeoutSecRef() = seconds; }
inline int getExecTimeoutSec() { return execTimeoutSecRef(); }

// Низкоуровневый запуск команды через popen без какой-либо обёртки таймаутом.
// Используется только для быстрых, заведомо неблокирующих проверок (наличие
// команды в PATH), чтобы не создавать циклическую зависимость с execCommand().
inline std::string rawExec(const std::string& fullCmd) {
    std::array<char, 256> buffer{};
    std::string result;
    // Приводим pclose к простой сигнатуре через лямбду, чтобы избежать
    // предупреждения компилятора об атрибутах шаблонного аргумента.
    auto closer = [](FILE* f) { if (f) pclose(f); };
    std::unique_ptr<FILE, decltype(closer)> pipe(popen(fullCmd.c_str(), "r"), closer);
    if (!pipe) return "";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

// Проверяет доступность утилиты `timeout` (coreutils) один раз за всё время
// работы программы.
inline bool hasTimeoutUtility() {
    static bool checked = false;
    static bool available = false;
    if (!checked) {
        checked = true;
        available = !trim(rawExec("command -v timeout 2>/dev/null")).empty();
    }
    return available;
}

// Выполняет команду в shell и возвращает её вывод (stdout) в виде строки.
// Используется popen — это единственная "внешняя зависимость", но она
// является частью стандартной библиотеки POSIX, доступной на всех Linux.
//
// Команда оборачивается в `timeout N` (если утилита доступна), чтобы
// зависшая внешняя программа (например bluetoothctl или docker с недоступным
// демоном) не блокировала весь sysflex навсегда. timeoutSec == 0 отключает
// таймаут именно для этого вызова.
inline std::string execCommand(const std::string& cmd, int timeoutSec = -1) {
    int effectiveTimeout = (timeoutSec >= 0) ? timeoutSec : getExecTimeoutSec();
    std::string fullCmd;
    if (effectiveTimeout > 0 && hasTimeoutUtility()) {
        fullCmd = "timeout " + std::to_string(effectiveTimeout) + "s " + cmd + " 2>/dev/null";
    } else {
        fullCmd = cmd + " 2>/dev/null";
    }
    return rawExec(fullCmd);
}

// Проверяет наличие исполняемого файла в PATH (аналог `command -v`).
// Использует rawExec напрямую (без таймаута) — `command -v` является
// встроенной командой shell и не может зависнуть.
//
// Результат кэшируется на всё время работы процесса: набор утилит в PATH за
// время жизни sysflex практически не меняется, а каждый вызов `command -v` —
// это fork+exec /bin/sh (~1 мс). В живом режиме update() вызывается каждую
// секунду и раньше платил эту цену по 7 раз за кадр.
inline bool commandExists(const std::string& name) {
    static std::unordered_map<std::string, bool> cache;
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;

    std::string out = rawExec("command -v " + name + " 2>/dev/null");
    bool exists = !trim(out).empty();
    cache.emplace(name, exists);
    return exists;
}

// Читает содержимое файла целиком в строку. Возвращает пустую строку при ошибке.
inline std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Читает первую строку файла (удобно для однострочных файлов из /sys, /proc)
inline std::string readFirstLine(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string line;
    std::getline(file, line);
    return trim(line);
}

// Проверяет существование файла
inline bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

// Безопасное преобразование строки в double, с значением по умолчанию при ошибке
inline double toDouble(const std::string& s, double def = 0.0) {
    try {
        return std::stod(s);
    } catch (...) {
        return def;
    }
}

// Безопасное преобразование строки в long, с значением по умолчанию при ошибке
inline long toLong(const std::string& s, long def = 0) {
    try {
        return std::stol(s);
    } catch (...) {
        return def;
    }
}

// Безопасное преобразование строки в unsigned long.
// В отличие от std::stoul() не бросает исключение на мусорных данных: поля
// /proc/*/ и /sys/*/ читаются без доверия к их содержимому, и необработанное
// исключение std::invalid_argument завершило бы sysflex аварийно.
inline unsigned long toULong(const std::string& s, unsigned long def = 0, int base = 0) {
    try {
        size_t pos = 0;
        unsigned long value = std::stoul(s, &pos, base); // base 0: автоопределение 0x/0
        if (pos == 0) return def;                        // ни одного символа не разобрано
        return value;
    } catch (...) {
        return def;
    }
}

// Возвращает текущее время в виде отформатированной строки "ЧЧ:ММ:СС"
inline std::string currentTimeString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
    localtime_r(&t, &tmBuf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &tmBuf);
    return std::string(buf);
}

// Возвращает текущую дату и время в виде "ГГГГ-ММ-ДД ЧЧ:ММ:СС"
inline std::string currentDateTimeString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
    localtime_r(&t, &tmBuf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return std::string(buf);
}

// Создаёт горизонтальный прогресс-бар из символов ASCII, например [####----] 50%
inline std::string progressBar(double percent, int width = 20, char fill = '#', char empty = '-') {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    int filled = static_cast<int>(std::round(width * percent / 100.0));
    std::string bar = "[";
    bar += std::string(filled, fill);
    bar += std::string(width - filled, empty);
    bar += "]";
    return bar;
}

// Приводит строку к нижнему регистру (используется для регистронезависимых сравнений)
inline std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Экранирует спецсимволы для корректного вывода строки в JSON.
// Дополнительно проверяет корректность UTF-8: имена процессов и hostname
// приходят из ядра и могут содержать "сырые" байты, а невалидный UTF-8
// ломает любой JSON-парсер. Такие байты заменяются на U+FFFD.
inline std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    const size_t n = s.size();
    size_t i = 0;
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                default:
                    if (c < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += static_cast<char>(c);
                    }
            }
            ++i;
            continue;
        }

        // Стартовый байт многобайтовой последовательности: проверяем, что
        // заявленное число continuation-байтов действительно на месте.
        int extra = -1;
        if ((c & 0xE0) == 0xC0) extra = 1;
        else if ((c & 0xF0) == 0xE0) extra = 2;
        else if ((c & 0xF8) == 0xF0) extra = 3;

        bool valid = extra > 0 && i + static_cast<size_t>(extra) < n;
        for (int k = 1; valid && k <= extra; ++k) {
            if ((static_cast<unsigned char>(s[i + static_cast<size_t>(k)]) & 0xC0) != 0x80) valid = false;
        }
        if (valid) {
            out.append(s, i, static_cast<size_t>(extra) + 1);
            i += static_cast<size_t>(extra) + 1;
        } else {
            out += "\xEF\xBF\xBD"; // U+FFFD REPLACEMENT CHARACTER
            ++i;
        }
    }
    return out;
}

// Печатает число так, чтобы результат всегда был валидным JSON-числом.
// JSON не знает NaN и Infinity, а std::ostream по умолчанию выводит их как
// "nan"/"inf" — такой документ не распарсится ни одним инструментом.
// Такие значения заменяются на 0.
inline std::string jsonNumber(double value, int precision = 4) {
    if (!std::isfinite(value)) return "0";
    std::ostringstream oss;
    oss.precision(precision);
    oss << std::fixed << value;
    return oss.str();
}

} // namespace sysflex::utils
