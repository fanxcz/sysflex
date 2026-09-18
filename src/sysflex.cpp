// sysflex.cpp — реализация главного класса App: управление режимами работы,
// живым обновлением, бенчмарком и системой плагинов.
#include "sysflex/sysflex.hpp"
#include "sysflex/utils.hpp"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_set>
#include <sys/stat.h>
#include <unistd.h>

namespace sysflex {

using namespace utils;

std::atomic<bool> App::running_{true};

void App::signalHandler(int /*signum*/) {
    // В обработчике сигнала допустимы только async-signal-safe операции;
    // relaxed-запись в lock-free атомик к ним относится.
    running_.store(false, std::memory_order_relaxed);
}

App::App(const Config& config) : config_(config) {}

// Монитор с параметрами из конфигурации. Интервал обновления «медленных»
// данных (внешние утилиты) берётся из --slow-refresh / конфиг-файла.
SystemMonitor App::makeMonitor() const {
    SystemMonitor monitor(config_.topProcessCount);
    monitor.setSlowRefreshSeconds(config_.slowRefreshSec);
    return monitor;
}

// Два замера с паузой между ними: скорости и мгновенная загрузка процессов
// считаются как дельта между кадрами, поэтому одиночный update() всегда
// вернул бы нули.
SystemInfo App::warmUpSnapshot(SystemMonitor& monitor, int pauseMs) {
    monitor.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(pauseMs));
    return monitor.update();
}

int App::run() {
    if (config_.showHelp) {
        Config::printHelp();
        return 0;
    }
    if (config_.showVersion) {
        std::cout << "sysflex версии " << VERSION << "\n";
        return 0;
    }

    // Настраиваем глобальный таймаут для всех внешних команд (docker,
    // bluetoothctl, playerctl и т.п.), чтобы недоступный сервис/демон не
    // вешал sysflex навсегда. Делается один раз при старте.
    utils::setExecTimeoutSec(config_.execTimeoutSec);

    switch (config_.mode) {
        case Mode::Normal:     runNormal();     break;
        case Mode::Live:       runLive();       break;
        case Mode::Short:      runShort();      break;
        case Mode::Json:       runJson();       break;
        case Mode::Game:       runGame();       break;
        case Mode::Bench:      runBenchmark();  break;
        case Mode::ListThemes: runListThemes(); break;
    }
    return 0;
}

// ------------------------------------------------------------------
// Список доступных тем
// ------------------------------------------------------------------
void App::runListThemes() const {
    std::cout << "Доступные темы sysflex:\n";
    for (const std::string& name : themeNames()) {
        const Theme t = getTheme(name, config_.noColor);
        std::cout << "  " << t.primary << t.bold << name << t.reset
                  << (name == config_.theme ? "  (текущая)" : "") << "\n";
    }
    std::cout << "\nПрименить тему: sysflex --theme NAME\n";
}

// ------------------------------------------------------------------
// Обычный (полный) режим
// ------------------------------------------------------------------
void App::runNormal() {
    Theme theme = getTheme(config_.theme, config_.noColor);
    SystemMonitor monitor = makeMonitor();
    SystemInfo info = warmUpSnapshot(monitor, 300);

    Display display(theme, config_.noAscii);
    display.printFull(info);

    if (config_.enablePlugins) runPlugins();
}

// ------------------------------------------------------------------
// Короткий режим
// ------------------------------------------------------------------
void App::runShort() {
    Theme theme = getTheme(config_.theme, config_.noColor);
    SystemMonitor monitor = makeMonitor();
    SystemInfo info = warmUpSnapshot(monitor, 200);

    Display display(theme, config_.noAscii);
    display.printShort(info);
}

// ------------------------------------------------------------------
// JSON режим
// ------------------------------------------------------------------
void App::runJson() {
    Theme theme = getTheme(config_.theme, true); // JSON всегда без цветов
    SystemMonitor monitor = makeMonitor();
    SystemInfo info = warmUpSnapshot(monitor, 200);

    Display display(theme, config_.noAscii);
    display.printJson(info);
}

// ------------------------------------------------------------------
// Игровой режим
// ------------------------------------------------------------------
void App::runGame() {
    Theme theme = getTheme(config_.theme, config_.noColor);
    SystemMonitor monitor = makeMonitor();
    SystemInfo info = warmUpSnapshot(monitor, 300);

    Display display(theme, config_.noAscii);
    display.printGame(info);
}

// ------------------------------------------------------------------
// Живой режим со спарклайнами (обновляется в цикле до Ctrl+C)
// ------------------------------------------------------------------
void App::runLive() {
    // Ctrl+C, kill и закрытие терминала должны завершать sysflex аккуратно.
    std::signal(SIGINT, App::signalHandler);
    std::signal(SIGTERM, App::signalHandler);
    std::signal(SIGHUP, App::signalHandler);
    std::signal(SIGQUIT, App::signalHandler);
    // При выводе в pipe (sysflex | head) закрытие читателя не должно убивать
    // процесс сигналом — просто завершим цикл по ошибке записи.
    std::signal(SIGPIPE, SIG_IGN);

    // Экран перерисовывается и курсор прячется только в настоящем терминале:
    // при перенаправлении в файл или pipe escape-коды превратили бы лог в мусор.
    const bool interactive = isatty(STDOUT_FILENO) == 1;

    Theme theme = getTheme(config_.theme, config_.noColor);
    SystemMonitor monitor = makeMonitor();
    Display display(theme, config_.noAscii);

    std::unordered_map<std::string, Sparkline> sparks;
    for (const char* key : {"cpu", "ram", "disk", "net_down", "net_up"}) {
        sparks[key] = Sparkline(static_cast<size_t>(config_.historyLength));
    }

    monitor.update(); // инициализация счётчиков дельт

    if (interactive) std::cout << "\033[?25l"; // спрятать курсор
    std::cout.flush();

    while (running_.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        SystemInfo info = monitor.update();
        display.printLiveFrame(info, sparks, interactive);
        if (std::cout.fail()) break; // читатель закрыл pipe — дальше писать некуда

        // Ждём оставшуюся часть интервала, но проверяем флаг running_ почаще,
        // чтобы Ctrl+C сработал быстро, а не только в конце длинного sleep.
        int waited = 0;
        int totalMs = std::max(0, config_.interval * 1000 - 200);
        while (waited < totalMs && running_.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited += 100;
        }
    }

    // Возвращаем терминал в пригодное состояние: показываем курсор и
    // очищаем кадр, иначе последние цифры останутся «висеть» в консоли.
    if (interactive) std::cout << "\033[2J\033[H\033[?25h";
    std::cout << "sysflex: живой режим остановлен.\n";
    std::cout.flush();
}

// ------------------------------------------------------------------
// Бенчмарк системы: тестируем CPU, память и диск и выводим итоговый балл
// ------------------------------------------------------------------
void App::runBenchmark() {
    Theme theme = getTheme(config_.theme, config_.noColor);
    Display display(theme, config_.noAscii);

    std::cout << theme.primary << "Запуск бенчмарка sysflex... это может занять несколько секунд.\n" << theme.reset;

    auto benchStart = std::chrono::steady_clock::now();

    // --- CPU тест: считаем, сколько итераций плавающей арифметики удаётся
    // выполнить за фиксированное время (простая, но показательная нагрузка) ---
    std::cout << theme.muted << "  [1/3] Тестирование CPU..." << theme.reset << std::endl;
    double cpuAccumulator = 0.0;
    long cpuIterations = 0;
    auto cpuStart = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - cpuStart).count() < 1.0) {
        for (int i = 0; i < 100000; ++i) {
            cpuAccumulator += std::sin(static_cast<double>(i)) * std::cos(static_cast<double>(i));
            ++cpuIterations;
        }
    }
    // Нормируем к условным "баллам": миллион итераций/сек ~ 100 баллов
    double cpuScore = static_cast<double>(cpuIterations) / 1'000'000.0 * 100.0;
    // Запись в volatile не даёт оптимизатору выкинуть весь «бесполезный»
    // расчёт вместе с циклом (иначе бенчмарк измерял бы пустоту).
    volatile double benchmarkSink = cpuAccumulator;
    (void)benchmarkSink;

    // --- Тест памяти: аллоцируем буфер и многократно копируем его ---
    std::cout << theme.muted << "  [2/3] Тестирование памяти..." << theme.reset << std::endl;
    const size_t bufSize = 64 * 1024 * 1024; // 64 МБ
    std::vector<char> src(bufSize, 'a');
    std::vector<char> dst(bufSize, 0);
    long memCopies = 0;
    auto memStart = std::chrono::steady_clock::now();
    while (std::chrono::duration<double>(std::chrono::steady_clock::now() - memStart).count() < 1.0) {
        std::copy(src.begin(), src.end(), dst.begin());
        ++memCopies;
    }
    double memThroughputMBps = static_cast<double>(memCopies) * static_cast<double>(bufSize) / (1024.0 * 1024.0);
    double memScore = memThroughputMBps / 50.0; // нормировка: 5000 МБ/с ~ 100 баллов

    // --- Тест диска: пишем и читаем временный файл ---
    std::cout << theme.muted << "  [3/3] Тестирование диска..." << theme.reset << std::endl;
    double diskScore = 0.0;
    {
        const std::string tmpPath = "/tmp/sysflex_bench_" + std::to_string(getpid()) + ".tmp";
        const size_t chunkSize = 4 * 1024 * 1024; // 4 МБ
        const int chunks = 20; // итого 80 МБ
        std::vector<char> buffer(chunkSize, 'x');

        auto writeStart = std::chrono::steady_clock::now();
        {
            std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
            if (out.is_open()) {
                for (int i = 0; i < chunks; ++i) {
                    out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                }
                out.flush();
            }
        }
        double writeSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - writeStart).count();

        auto readStart = std::chrono::steady_clock::now();
        {
            std::ifstream in(tmpPath, std::ios::binary);
            std::vector<char> readBuf(chunkSize);
            while (in.read(readBuf.data(), static_cast<std::streamsize>(readBuf.size()))) {
                // просто читаем, ничего не делаем с данными
            }
        }
        double readSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - readStart).count();

        std::remove(tmpPath.c_str());

        double totalMB = static_cast<double>(chunkSize * chunks) / (1024.0 * 1024.0);
        double writeMBps = (writeSeconds > 0) ? totalMB / writeSeconds : 0.0;
        double readMBps = (readSeconds > 0) ? totalMB / readSeconds : 0.0;
        double avgMBps = (writeMBps + readMBps) / 2.0;
        diskScore = avgMBps / 5.0; // нормировка: 500 МБ/с ~ 100 баллов
    }

    double totalScore = (cpuScore + memScore + diskScore) / 3.0;
    auto benchEnd = std::chrono::steady_clock::now();
    long durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(benchEnd - benchStart).count();

    display.printBenchmarkResult(cpuScore, memScore, diskScore, totalScore, durationMs);
}

// ------------------------------------------------------------------
// Система плагинов
// ------------------------------------------------------------------
std::vector<std::string> App::findPluginPaths() const {
    std::vector<std::string> candidates;

    // Каталог, заданный пользователем, имеет наивысший приоритет: при
    // совпадении имён побеждает именно он (см. дедупликацию в runPlugins()).
    if (const char* envDir = std::getenv("SYSFLEX_PLUGINS_DIR")) {
        if (*envDir != '\0') candidates.push_back(envDir);
    }
    candidates.push_back("./plugins");
    candidates.push_back("/usr/local/share/sysflex/plugins");
    candidates.push_back("/usr/share/sysflex/plugins");

    const char* home = std::getenv("HOME");
    if (home) {
        candidates.push_back(std::string(home) + "/.sysflex/plugins");
        candidates.push_back(std::string(home) + "/.config/sysflex/plugins");
    }

    std::vector<std::string> found;
    for (auto& dir : candidates) {
        struct stat st{};
        if (stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            // Один и тот же каталог может попасть в список дважды
            // (например, через $SYSFLEX_PLUGINS_DIR и как ./plugins).
            if (std::find(found.begin(), found.end(), dir) == found.end()) {
                found.push_back(dir);
            }
        }
    }
    return found;
}

void App::runPlugins() const {
    Theme theme = getTheme(config_.theme, config_.noColor);
    Display display(theme, config_.noAscii);

    // Плагины ищутся сразу в нескольких каталогах (локальный ./plugins,
    // системный /usr/local/share/sysflex/plugins и т.д.) — это нужно, чтобы
    // sysflex работал как из папки с исходниками, так и после `install.sh`.
    // Но если один и тот же плагин лежит одновременно в нескольких из этих
    // каталогов (например, install.sh скопировал его, а исходная папка
    // plugins/ всё ещё рядом), раньше он запускался и выводился по разу на
    // каждый найденный каталог. Дедуплицируем по имени файла: побеждает
    // каталог с более высоким приоритетом (первый в списке findPluginPaths()).
    auto dirs = findPluginPaths();
    std::unordered_set<std::string> seenNames;
    for (auto& dir : dirs) {
        // Каталог перебирается системными вызовами, без запуска `find` через
        // shell: плагины и так являются внешними процессами, плодить лишние
        // не имеет смысла.
        std::vector<std::string> files = listDirectory(dir);
        std::sort(files.begin(), files.end()); // детерминированный порядок вывода
        for (const std::string& file : files) {
            const std::string path = dir + "/" + file;

            struct stat st{};
            if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) continue;
            if (access(path.c_str(), X_OK) != 0) continue; // не исполняемый — не плагин

            if (!seenNames.insert(file).second) {
                continue; // плагин с таким именем уже был запущен из другого каталога
            }

            std::string output = execCommand(path);
            display.printPluginOutput(file, output);
        }
    }
}

} // namespace sysflex
