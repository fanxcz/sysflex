// display.cpp — реализация вывода информации о системе в различных режимах
#include "sysflex/display.hpp"
#include "sysflex/utils.hpp"
#include "sysflex/ascii_art.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace sysflex {

using namespace utils;

Display::Display(const Theme& theme, bool noAscii) : theme_(theme), noAscii_(noAscii) {}

std::string Display::header(const std::string& title) const {
    std::ostringstream oss;
    oss << theme_.primary << theme_.bold << "── " << title << " " << theme_.reset;
    return oss.str();
}

std::string Display::label(const std::string& text) const {
    return theme_.muted + text + theme_.reset;
}

std::string Display::colorizeByPercent(const std::string& text, double percent) const {
    return theme_.colorForPercent(percent) + text + theme_.reset;
}

std::string Display::separator(int width) const {
    return theme_.muted + std::string(width, '-') + theme_.reset;
}

// ------------------------------------------------------------------
// Полный вывод
// ------------------------------------------------------------------
void Display::printFull(const SystemInfo& info) const {
    std::vector<std::string> art;
    if (!noAscii_) art = ascii::artForOs(toLowerStr(info.osPrettyName));

    std::cout << theme_.secondary << theme_.bold
              << "  sysflex" << theme_.reset << theme_.muted << "  —  гибкий системный монитор\n" << theme_.reset;
    std::cout << separator(60) << "\n";

    // Выводим ASCII-арт слева, а базовую инфу об ОС справа
    size_t maxArtWidth = 0;
    for (auto& l : art) maxArtWidth = std::max(maxArtWidth, l.size());

    std::vector<std::string> infoLines = {
        theme_.primary + theme_.bold + info.hostname + theme_.reset,
        label("OS:      ") + info.osPrettyName,
        label("Ядро:    ") + info.kernelVersion,
        label("Аптайм:  ") + formatTime(info.uptimeSeconds),
        label("Оболочка:") + " " + info.shellName + " (" + info.terminalName + ")",
        label("Время:   ") + info.currentTime,
    };

    size_t maxLines = std::max(art.size(), infoLines.size());
    for (size_t i = 0; i < maxLines; ++i) {
        std::string artLine = (i < art.size()) ? art[i] : std::string(maxArtWidth, ' ');
        std::string infoLine = (i < infoLines.size()) ? infoLines[i] : "";
        std::cout << "  " << theme_.secondary << std::left << std::setw(static_cast<int>(maxArtWidth) + 2)
                  << artLine << theme_.reset << "  " << infoLine << "\n";
    }

    std::cout << "\n" << header("Здоровье системы") << "\n";
    // Полоса заполнена на величину самого рейтинга (раньше рисовалась
    // «степень повреждения» — 100-health, из-за чего полностью здоровая
    // система показывала пустой бар, и это читалось как ошибка).
    std::cout << "  " << colorizeByPercent(progressBar(info.healthScore, 30), 100 - info.healthScore)
              << " " << colorizeByPercent(std::to_string(info.healthScore) + "/100", 100 - info.healthScore) << "\n";

    // --- CPU ---
    std::cout << "\n" << header("CPU") << "\n";
    std::cout << "  " << label("Загрузка:    ") << colorizeByPercent(progressBar(info.cpuUsagePercent, 25), info.cpuUsagePercent)
              << " " << colorizeByPercent(formatPercent(info.cpuUsagePercent), info.cpuUsagePercent) << "\n";
    std::cout << "  " << label("Частота:     ") << std::fixed << std::setprecision(0) << info.cpuFrequencyMHz << " MHz"
              << "  " << label("Ядра: ") << info.cpuCoreCount << "\n";
    if (info.cpuTemperatureC >= 0) {
        std::cout << "  " << label("Температура: ") << colorizeByPercent(
            (std::ostringstream() << std::fixed << std::setprecision(1) << info.cpuTemperatureC << " C").str(),
            info.cpuTemperatureC) << "\n";
    }
    std::cout << "  " << label("Load average:") << std::fixed << std::setprecision(2)
              << " " << info.loadAverage[0] << " " << info.loadAverage[1]
              << " " << info.loadAverage[2] << "\n";
    if (!info.perCoreTempsC.empty()) {
        std::cout << "  " << label("По ядрам:    ");
        for (size_t i = 0; i < info.perCoreTempsC.size(); ++i) {
            std::cout << "C" << i << ":" << std::fixed << std::setprecision(0) << info.perCoreTempsC[i] << "C ";
        }
        std::cout << "\n";
    }

    // --- RAM / Swap ---
    std::cout << "\n" << header("Память") << "\n";
    std::cout << "  " << label("RAM:  ") << colorizeByPercent(progressBar(info.ramUsagePercent, 25), info.ramUsagePercent)
              << " " << formatBytes(info.ramUsedKB * 1024.0) << " / " << formatBytes(info.ramTotalKB * 1024.0)
              << " (" << colorizeByPercent(formatPercent(info.ramUsagePercent), info.ramUsagePercent) << ")\n";
    if (info.swapTotalKB > 0) {
        std::cout << "  " << label("Swap: ") << colorizeByPercent(progressBar(info.swapUsagePercent, 25), info.swapUsagePercent)
                  << " " << formatBytes(info.swapUsedKB * 1024.0) << " / " << formatBytes(info.swapTotalKB * 1024.0)
                  << " (" << colorizeByPercent(formatPercent(info.swapUsagePercent), info.swapUsagePercent) << ")\n";
    } else {
        std::cout << "  " << label("Swap: ") << "не настроен\n";
    }

    // --- Диск ---
    std::cout << "\n" << header("Диск (/)") << "\n";
    std::cout << "  " << label("Место: ") << colorizeByPercent(progressBar(info.diskUsagePercent, 25), info.diskUsagePercent)
              << " " << formatBytes(static_cast<double>(info.diskUsedBytes)) << " / "
              << formatBytes(static_cast<double>(info.diskTotalBytes))
              << " (" << colorizeByPercent(formatPercent(info.diskUsagePercent), info.diskUsagePercent) << ")\n";
    std::cout << "  " << label("I/O:   ") << "чтение " << formatSpeed(info.diskReadBytesPerSec)
              << "  запись " << formatSpeed(info.diskWriteBytesPerSec) << "\n";
    std::cout << "  " << label("Inode: ") << formatPercent(info.inodeUsagePercent) << " занято\n";

    // --- Сеть ---
    std::cout << "\n" << header("Сеть (" + info.netInterface + ")") << "\n";
    std::cout << "  " << label("Загрузка:  ") << "\u2193 " << formatSpeed(info.netDownloadBytesPerSec) << "\n";
    std::cout << "  " << label("Отправка:  ") << "\u2191 " << formatSpeed(info.netUploadBytesPerSec) << "\n";

    // --- GPU ---
    std::cout << "\n" << header("GPU") << "\n";
    std::cout << "  " << label("Модель:      ") << info.gpuModel << "\n";
    if (info.gpuTemperatureC >= 0) {
        std::cout << "  " << label("Температура: ") << std::fixed << std::setprecision(1)
                  << info.gpuTemperatureC << " C\n";
    }
    if (info.gpuMemUsedMB >= 0 && info.gpuMemTotalMB >= 0) {
        std::cout << "  " << label("Память:      ") << std::fixed << std::setprecision(0)
                  << info.gpuMemUsedMB << " / " << info.gpuMemTotalMB << " MB\n";
    }

    // --- Батарея ---
    if (info.hasBattery) {
        std::cout << "\n" << header("Батарея") << "\n";
        std::cout << "  " << colorizeByPercent(progressBar(info.batteryPercent, 25), 100 - info.batteryPercent)
                  << " " << info.batteryPercent << "%  (" << info.batteryStatus << ")\n";
    }

    // --- Процессы ---
    std::cout << "\n" << header("Процессы (всего: " + std::to_string(info.processCount) + ")") << "\n";
    std::cout << "  " << label("PID     ИМЯ                  CPU%    MEM%") << "\n";
    for (auto& p : info.topProcesses) {
        std::cout << "  " << std::left << std::setw(8) << p.pid
                  << std::setw(22) << p.name.substr(0, 20)
                  << std::right << std::setw(6) << std::fixed << std::setprecision(1) << p.cpuPercent
                  << "%  " << std::setw(5) << p.memPercent << "%\n";
    }

    // --- Разное ---
    std::cout << "\n" << header("Прочее") << "\n";
    std::cout << "  " << label("Пользователей в системе: ") << info.loggedUsersCount << "\n";
    if (info.dockerContainerCount >= 0) {
        std::cout << "  " << label("Docker контейнеров:      ") << info.dockerContainerCount << "\n";
    }
    if (!info.gitBranch.empty()) {
        std::cout << "  " << label("Git ветка:               ") << info.gitBranch << "\n";
    }
    if (!info.musicStatus.empty()) {
        std::cout << "  " << label("Музыка:                  ") << info.musicStatus << "\n";
    }
    std::cout << "  " << label("USB устройств:           ") << info.usbDeviceCount << "\n";
    std::cout << "  " << label("Bluetooth устройств:     ") << info.bluetoothDeviceCount << "\n";

    // --- Расширенная диагностика: CPU, память, диск, сеть, датчики ---
    std::cout << "\n" << header("Расширенная диагностика") << "\n";
    std::cout << "  " << label("1  CPU модель:        ") << info.cpuModelName << "\n";
    std::cout << "  " << label("2  Кэш CPU:           ") << info.cpuCacheSizeKB << " KB\n";
    std::cout << "  " << label("3  CPU governor:      ") << (info.cpuGovernor.empty() ? "н/д" : info.cpuGovernor) << "\n";
    std::cout << "  " << label("4  Переключ. контекста:") << std::fixed << std::setprecision(0) << info.ctxSwitchesPerSec << "/с\n";
    std::cout << "  " << label("5  Прерываний:        ") << info.interruptsPerSec << "/с\n";
    std::cout << "  " << label("6  Форков процессов:  ") << info.forksPerSec << "/с\n";
    std::cout << "  " << label("7  Процессов running: ") << info.procsRunning << "\n";
    std::cout << "  " << label("8  Процессов blocked: ") << info.procsBlocked << "\n";
    std::cout << "  " << label("9  Зомби-процессов:   ") << info.zombieCount << "\n";
    std::cout << "  " << label("10 Энтропия ядра:     ") << (info.entropyAvailable >= 0 ? std::to_string(info.entropyAvailable) + " бит" : "н/д") << "\n";
    std::cout << "  " << label("11 MemAvailable:      ") << formatBytes(info.memAvailableKB * 1024.0) << "\n";
    std::cout << "  " << label("12 Buffers:           ") << formatBytes(info.buffersKB * 1024.0) << "\n";
    std::cout << "  " << label("13 Cached:            ") << formatBytes(info.cachedKB * 1024.0) << "\n";
    std::cout << "  " << label("14 Dirty pages:       ") << formatBytes(info.dirtyKB * 1024.0) << "\n";
    std::cout << "  " << label("15-16 HugePages:      ") << info.hugePagesFree << " своб. / " << info.hugePagesTotal << " всего\n";
    std::cout << "  " << label("17-18 Диск IOPS:      ") << "чтение " << std::setprecision(1) << info.diskReadOpsPerSec
              << "/с, запись " << info.diskWriteOpsPerSec << "/с\n";
    std::cout << "  " << label("19 Тип корневой ФС:   ") << (info.rootFilesystemType.empty() ? "н/д" : info.rootFilesystemType) << "\n";
    std::cout << "  " << label("20 Точек монтирования:") << info.mountPointCount << "\n";
    std::cout << "  " << label("21-22 TCP соединения: ") << info.tcpEstablishedCount << " активных, " << info.tcpListenCount << " слушающих\n";
    std::cout << "  " << label("23 Сетевых интерф.:   ") << info.networkInterfaceCount << "\n";
    std::cout << "  " << label("24 DNS-серверы:       ") << (info.dnsServers.empty() ? "н/д" : info.dnsServers) << "\n";
    std::cout << "  " << label("25 Вентилятор:        ") << (info.fanSpeedRPM >= 0 ? std::to_string(info.fanSpeedRPM) + " RPM" : "н/д") << "\n";
    if (info.maxSensorTempC >= 0) {
        std::cout << "  " << label("26 Макс. темп. датчик:") << std::fixed << std::setprecision(1) << info.maxSensorTempC << " C\n";
    } else {
        std::cout << "  " << label("26 Макс. темп. датчик:") << "н/д\n";
    }
    std::cout << "  " << label("27-28 Дескрипторы:    ") << info.openFileDescriptors << " / " << info.maxFileDescriptors << "\n";
    std::cout << "  " << label("29 Локаль:            ") << (info.systemLocale.empty() ? "н/д" : info.systemLocale) << "\n";
    std::cout << "  " << label("30 Часовой пояс:      ") << (info.systemTimezone.empty() ? "н/д" : info.systemTimezone) << "\n";

    // --- Расширенная диагностика: ядро, планировщик, файловые системы ---
    std::cout << "\n" << header("Диагностика ядра и подсистем") << "\n";
    std::cout << "  " << label("31 CPU iowait:        ") << formatPercent(info.cpuIowaitPercent) << "\n";
    std::cout << "  " << label("32 CPU steal:         ") << formatPercent(info.cpuStealPercent) << "\n";
    std::cout << "  " << label("33 CPU softirq:       ") << formatPercent(info.cpuSoftirqPercent) << "\n";
    std::cout << "  " << label("34 Загрузка по ядрам: ");
    for (size_t i = 0; i < info.perCoreUsagePercent.size(); ++i) {
        std::cout << "C" << i << ":" << std::fixed << std::setprecision(0) << info.perCoreUsagePercent[i] << "% ";
    }
    std::cout << "\n";
    std::cout << "  " << label("35 Архитектура:       ") << (info.cpuArchitecture.empty() ? "н/д" : info.cpuArchitecture) << "\n";
    std::cout << "  " << label("36 BogoMIPS:          ") << std::fixed << std::setprecision(1) << info.cpuBogoMips << "\n";
    std::cout << "  " << label("37 Slab:              ") << formatBytes(info.slabKB * 1024.0) << "\n";
    std::cout << "  " << label("38 PageTables:        ") << formatBytes(info.pageTablesKB * 1024.0) << "\n";
    std::cout << "  " << label("39 Committed_AS:      ") << formatBytes(info.committedAsKB * 1024.0) << "\n";
    std::cout << "  " << label("40 Active(anon):      ") << formatBytes(info.activeAnonKB * 1024.0) << "\n";
    std::cout << "  " << label("41 Shmem:             ") << formatBytes(info.shmemKB * 1024.0) << "\n";
    std::cout << "  " << label("42 Mapped:            ") << formatBytes(info.mappedKB * 1024.0) << "\n";
    std::cout << "  " << label("43 KernelStack:       ") << formatBytes(info.kernelStackKB * 1024.0) << "\n";
    std::cout << "  " << label("44 Диск в очереди:    ") << info.diskIoInFlight << " операций\n";
    std::cout << "  " << label("45-46 Прочитано/записано всего: ") << formatBytes(static_cast<double>(info.diskTotalReadBytesAllTime))
              << " / " << formatBytes(static_cast<double>(info.diskTotalWriteBytesAllTime)) << "\n";
    std::cout << "  " << label("47 Блочных устройств: ") << info.blockDeviceCount << "\n";
    std::cout << "  " << label("48 Диск занят суммарно:") << formatTime(static_cast<long>(info.diskIoTimeMsAllTime / 1000)) << "\n";
    std::cout << "  " << label("49 UDP-сокетов:       ") << info.udpSocketCount << "\n";
    std::cout << "  " << label("50-51 Трафик всего:   ") << "\u2193 " << formatBytes(static_cast<double>(info.netRxBytesAllTime))
              << "  \u2191 " << formatBytes(static_cast<double>(info.netTxBytesAllTime)) << "\n";
    std::cout << "  " << label("52-53 Ошибок сети:    ") << "rx " << info.netRxErrorsAllTime << ", tx " << info.netTxErrorsAllTime << "\n";
    std::cout << "  " << label("54 Шлюз по умолчанию: ") << info.defaultGateway << "\n";
    std::cout << "  " << label("55 Всего потоков:     ") << info.totalThreadCount << "\n";
    std::cout << "  " << label("56 Пользователей процессов:") << info.uniqueProcessUsers << "\n";
    std::cout << "  " << label("57 Модулей ядра:      ") << info.kernelModuleCount << "\n";
    std::cout << "  " << label("58 Clocksource:       ") << info.clockSource << "\n";
    std::cout << "  " << label("59 Init-система:      ") << info.initSystemName << "\n";
    std::cout << "  " << label("60 Kernel cmdline:    ") << info.kernelCmdline << "\n";

    std::cout << "\n" << separator(60) << "\n";
    std::cout.flush();
}

// ------------------------------------------------------------------
// Короткий вывод
// ------------------------------------------------------------------
void Display::printShort(const SystemInfo& info) const {
    std::cout << theme_.primary << theme_.bold << info.hostname << theme_.reset << "  "
              << label("|") << " CPU " << colorizeByPercent(formatPercent(info.cpuUsagePercent), info.cpuUsagePercent)
              << "  " << label("|") << " RAM " << colorizeByPercent(formatPercent(info.ramUsagePercent), info.ramUsagePercent)
              << "  " << label("|") << " Диск " << colorizeByPercent(formatPercent(info.diskUsagePercent), info.diskUsagePercent)
              << "  " << label("|") << " Аптайм " << formatTime(info.uptimeSeconds)
              << "  " << label("|") << " Здоровье " << colorizeByPercent(std::to_string(info.healthScore), 100 - info.healthScore)
              << "\n";
    std::cout.flush();
}

// ------------------------------------------------------------------
// JSON вывод
// ------------------------------------------------------------------
void Display::printJson(const SystemInfo& info) const {
    std::ostringstream oss;

    // Локальные помощники: строки всегда экранируются (включая проверку
    // UTF-8), а числа всегда печатаются как корректные JSON-числа — обычный
    // operator<< выдал бы "nan"/"inf", и такой документ не распарсился бы.
    auto str = [](const std::string& v) { return "\"" + jsonEscape(v) + "\""; };
    auto num = [](double v) { return jsonNumber(v); };
    auto intNum = [](long long v) { return std::to_string(v); };
    auto arr = [](const std::vector<double>& values) {
        std::string out = "[";
        for (size_t i = 0; i < values.size(); ++i) {
            if (i) out += ", ";
            out += jsonNumber(values[i]);
        }
        return out + "]";
    };
    const auto loadAvg = (info.loadAverage.size() >= 3)
                             ? std::vector<double>(info.loadAverage.begin(), info.loadAverage.begin() + 3)
                             : std::vector<double>{0.0, 0.0, 0.0};

    oss << "{\n";
    oss << "  \"version\": " << str(VERSION) << ",\n";
    oss << "  \"generated_at\": " << str(currentDateTimeString()) << ",\n";
    oss << "  \"hostname\": " << str(info.hostname) << ",\n";
    oss << "  \"os\": " << str(info.osPrettyName) << ",\n";
    oss << "  \"kernel\": " << str(info.kernelVersion) << ",\n";
    oss << "  \"uptime_seconds\": " << intNum(info.uptimeSeconds) << ",\n";
    oss << "  \"health_score\": " << intNum(info.healthScore) << ",\n";
    oss << "  \"cpu\": {\n";
    oss << "    \"usage_percent\": " << num(info.cpuUsagePercent) << ",\n";
    oss << "    \"frequency_mhz\": " << num(info.cpuFrequencyMHz) << ",\n";
    oss << "    \"temperature_c\": " << num(info.cpuTemperatureC) << ",\n";
    oss << "    \"cores\": " << intNum(info.cpuCoreCount) << ",\n";
    oss << "    \"load_average\": " << arr(loadAvg) << "\n";
    oss << "  },\n";
    oss << "  \"ram\": {\n";
    oss << "    \"total_kb\": " << intNum(static_cast<long long>(info.ramTotalKB)) << ",\n";
    oss << "    \"used_kb\": " << intNum(static_cast<long long>(info.ramUsedKB)) << ",\n";
    oss << "    \"usage_percent\": " << num(info.ramUsagePercent) << "\n";
    oss << "  },\n";
    oss << "  \"swap\": {\n";
    oss << "    \"total_kb\": " << intNum(static_cast<long long>(info.swapTotalKB)) << ",\n";
    oss << "    \"used_kb\": " << intNum(static_cast<long long>(info.swapUsedKB)) << ",\n";
    oss << "    \"usage_percent\": " << num(info.swapUsagePercent) << "\n";
    oss << "  },\n";
    oss << "  \"disk\": {\n";
    oss << "    \"total_bytes\": " << intNum(static_cast<long long>(info.diskTotalBytes)) << ",\n";
    oss << "    \"used_bytes\": " << intNum(static_cast<long long>(info.diskUsedBytes)) << ",\n";
    oss << "    \"usage_percent\": " << num(info.diskUsagePercent) << ",\n";
    oss << "    \"read_bytes_per_sec\": " << num(info.diskReadBytesPerSec) << ",\n";
    oss << "    \"write_bytes_per_sec\": " << num(info.diskWriteBytesPerSec) << ",\n";
    oss << "    \"inode_usage_percent\": " << num(info.inodeUsagePercent) << "\n";
    oss << "  },\n";
    oss << "  \"network\": {\n";
    oss << "    \"interface\": " << str(info.netInterface) << ",\n";
    oss << "    \"download_bytes_per_sec\": " << num(info.netDownloadBytesPerSec) << ",\n";
    oss << "    \"upload_bytes_per_sec\": " << num(info.netUploadBytesPerSec) << "\n";
    oss << "  },\n";
    oss << "  \"gpu\": {\n";
    oss << "    \"model\": " << str(info.gpuModel) << ",\n";
    oss << "    \"temperature_c\": " << num(info.gpuTemperatureC) << ",\n";
    oss << "    \"mem_used_mb\": " << num(info.gpuMemUsedMB) << ",\n";
    oss << "    \"mem_total_mb\": " << num(info.gpuMemTotalMB) << "\n";
    oss << "  },\n";
    oss << "  \"battery\": {\n";
    oss << "    \"present\": " << (info.hasBattery ? "true" : "false") << ",\n";
    oss << "    \"percent\": " << intNum(info.batteryPercent) << ",\n";
    oss << "    \"status\": " << str(info.batteryStatus) << "\n";
    oss << "  },\n";
    oss << "  \"processes\": {\n";
    oss << "    \"count\": " << intNum(info.processCount) << ",\n";
    oss << "    \"top\": [\n";
    for (size_t i = 0; i < info.topProcesses.size(); ++i) {
        const auto& p = info.topProcesses[i];
        oss << "      {\"pid\": " << intNum(p.pid) << ", \"name\": " << str(p.name)
            << ", \"cpu_percent\": " << num(p.cpuPercent) << ", \"mem_percent\": " << num(p.memPercent) << "}"
            << (i + 1 < info.topProcesses.size() ? "," : "") << "\n";
    }
    oss << "    ]\n";
    oss << "  },\n";
    oss << "  \"shell\": " << str(info.shellName) << ",\n";
    oss << "  \"terminal\": " << str(info.terminalName) << ",\n";
    oss << "  \"docker_containers\": " << intNum(info.dockerContainerCount) << ",\n";
    oss << "  \"git_branch\": " << str(info.gitBranch) << ",\n";
    oss << "  \"music\": " << str(info.musicStatus) << ",\n";
    oss << "  \"usb_device_count\": " << intNum(info.usbDeviceCount) << ",\n";
    oss << "  \"bluetooth_device_count\": " << intNum(info.bluetoothDeviceCount) << ",\n";
    oss << "  \"logged_users\": " << intNum(info.loggedUsersCount) << ",\n";
    oss << "  \"current_time\": " << str(info.currentTime) << ",\n";
    oss << "  \"extras\": {\n";
    oss << "    \"cpu_model\": " << str(info.cpuModelName) << ",\n";
    oss << "    \"cpu_cache_kb\": " << intNum(static_cast<long long>(info.cpuCacheSizeKB)) << ",\n";
    oss << "    \"cpu_governor\": " << str(info.cpuGovernor) << ",\n";
    oss << "    \"ctx_switches_per_sec\": " << num(info.ctxSwitchesPerSec) << ",\n";
    oss << "    \"interrupts_per_sec\": " << num(info.interruptsPerSec) << ",\n";
    oss << "    \"forks_per_sec\": " << num(info.forksPerSec) << ",\n";
    oss << "    \"procs_running\": " << intNum(info.procsRunning) << ",\n";
    oss << "    \"procs_blocked\": " << intNum(info.procsBlocked) << ",\n";
    oss << "    \"zombie_count\": " << intNum(info.zombieCount) << ",\n";
    oss << "    \"entropy_available\": " << intNum(info.entropyAvailable) << ",\n";
    oss << "    \"mem_available_kb\": " << intNum(static_cast<long long>(info.memAvailableKB)) << ",\n";
    oss << "    \"buffers_kb\": " << intNum(static_cast<long long>(info.buffersKB)) << ",\n";
    oss << "    \"cached_kb\": " << intNum(static_cast<long long>(info.cachedKB)) << ",\n";
    oss << "    \"dirty_kb\": " << intNum(static_cast<long long>(info.dirtyKB)) << ",\n";
    oss << "    \"hugepages_total\": " << intNum(static_cast<long long>(info.hugePagesTotal)) << ",\n";
    oss << "    \"hugepages_free\": " << intNum(static_cast<long long>(info.hugePagesFree)) << ",\n";
    oss << "    \"disk_read_ops_per_sec\": " << num(info.diskReadOpsPerSec) << ",\n";
    oss << "    \"disk_write_ops_per_sec\": " << num(info.diskWriteOpsPerSec) << ",\n";
    oss << "    \"root_fs_type\": " << str(info.rootFilesystemType) << ",\n";
    oss << "    \"mount_point_count\": " << intNum(info.mountPointCount) << ",\n";
    oss << "    \"tcp_established\": " << intNum(info.tcpEstablishedCount) << ",\n";
    oss << "    \"tcp_listen\": " << intNum(info.tcpListenCount) << ",\n";
    oss << "    \"network_interface_count\": " << intNum(info.networkInterfaceCount) << ",\n";
    oss << "    \"dns_servers\": " << str(info.dnsServers) << ",\n";
    oss << "    \"fan_speed_rpm\": " << intNum(info.fanSpeedRPM) << ",\n";
    oss << "    \"max_sensor_temp_c\": " << num(info.maxSensorTempC) << ",\n";
    oss << "    \"open_file_descriptors\": " << intNum(static_cast<long long>(info.openFileDescriptors)) << ",\n";
    oss << "    \"max_file_descriptors\": " << intNum(static_cast<long long>(info.maxFileDescriptors)) << ",\n";
    oss << "    \"system_locale\": " << str(info.systemLocale) << ",\n";
    oss << "    \"system_timezone\": " << str(info.systemTimezone) << "\n";
    oss << "  },\n";
    oss << "  \"extras2\": {\n";
    oss << "    \"cpu_iowait_percent\": " << num(info.cpuIowaitPercent) << ",\n";
    oss << "    \"cpu_steal_percent\": " << num(info.cpuStealPercent) << ",\n";
    oss << "    \"cpu_softirq_percent\": " << num(info.cpuSoftirqPercent) << ",\n";
    oss << "    \"per_core_usage_percent\": " << arr(info.perCoreUsagePercent) << ",\n";
    oss << "    \"cpu_architecture\": " << str(info.cpuArchitecture) << ",\n";
    oss << "    \"cpu_bogomips\": " << num(info.cpuBogoMips) << ",\n";
    oss << "    \"slab_kb\": " << intNum(static_cast<long long>(info.slabKB)) << ",\n";
    oss << "    \"page_tables_kb\": " << intNum(static_cast<long long>(info.pageTablesKB)) << ",\n";
    oss << "    \"committed_as_kb\": " << intNum(static_cast<long long>(info.committedAsKB)) << ",\n";
    oss << "    \"active_anon_kb\": " << intNum(static_cast<long long>(info.activeAnonKB)) << ",\n";
    oss << "    \"shmem_kb\": " << intNum(static_cast<long long>(info.shmemKB)) << ",\n";
    oss << "    \"mapped_kb\": " << intNum(static_cast<long long>(info.mappedKB)) << ",\n";
    oss << "    \"kernel_stack_kb\": " << intNum(static_cast<long long>(info.kernelStackKB)) << ",\n";
    oss << "    \"disk_io_in_flight\": " << intNum(info.diskIoInFlight) << ",\n";
    oss << "    \"disk_total_read_bytes\": " << intNum(static_cast<long long>(info.diskTotalReadBytesAllTime)) << ",\n";
    oss << "    \"disk_total_write_bytes\": " << intNum(static_cast<long long>(info.diskTotalWriteBytesAllTime)) << ",\n";
    oss << "    \"block_device_count\": " << intNum(info.blockDeviceCount) << ",\n";
    oss << "    \"disk_io_time_ms_total\": " << intNum(static_cast<long long>(info.diskIoTimeMsAllTime)) << ",\n";
    oss << "    \"udp_socket_count\": " << intNum(info.udpSocketCount) << ",\n";
    oss << "    \"net_rx_bytes_total\": " << intNum(static_cast<long long>(info.netRxBytesAllTime)) << ",\n";
    oss << "    \"net_tx_bytes_total\": " << intNum(static_cast<long long>(info.netTxBytesAllTime)) << ",\n";
    oss << "    \"net_rx_errors_total\": " << intNum(static_cast<long long>(info.netRxErrorsAllTime)) << ",\n";
    oss << "    \"net_tx_errors_total\": " << intNum(static_cast<long long>(info.netTxErrorsAllTime)) << ",\n";
    oss << "    \"default_gateway\": " << str(info.defaultGateway) << ",\n";
    oss << "    \"total_thread_count\": " << intNum(info.totalThreadCount) << ",\n";
    oss << "    \"unique_process_users\": " << intNum(info.uniqueProcessUsers) << ",\n";
    oss << "    \"kernel_module_count\": " << intNum(info.kernelModuleCount) << ",\n";
    oss << "    \"clock_source\": " << str(info.clockSource) << ",\n";
    oss << "    \"init_system\": " << str(info.initSystemName) << ",\n";
    oss << "    \"kernel_cmdline\": " << str(info.kernelCmdline) << "\n";
    oss << "  }\n";
    oss << "}\n";
    std::cout << oss.str();
    std::cout.flush();
}
// ------------------------------------------------------------------
// Игровой режим
// ------------------------------------------------------------------
void Display::printGame(const SystemInfo& info) const {
    std::cout << theme_.primary << theme_.bold << "  sysflex — GAME MODE" << theme_.reset << "\n";
    std::cout << separator(50) << "\n";
    std::cout << "  " << label("GPU:         ") << theme_.secondary << info.gpuModel << theme_.reset << "\n";
    if (info.gpuTemperatureC >= 0) {
        std::cout << "  " << label("GPU темп:    ") << colorizeByPercent(
            (std::ostringstream() << std::fixed << std::setprecision(1) << info.gpuTemperatureC << " C").str(),
            info.gpuTemperatureC) << "\n";
    }
    if (info.gpuMemUsedMB >= 0 && info.gpuMemTotalMB > 0) {
        double gpuMemPercent = 100.0 * info.gpuMemUsedMB / info.gpuMemTotalMB;
        std::cout << "  " << label("GPU память:  ") << colorizeByPercent(progressBar(gpuMemPercent, 25), gpuMemPercent)
                  << " " << std::fixed << std::setprecision(0)
                  << info.gpuMemUsedMB << "/" << info.gpuMemTotalMB << " MB\n";
    }
    std::cout << "\n";
    std::cout << "  " << label("CPU загрузка:") << colorizeByPercent(progressBar(info.cpuUsagePercent, 25), info.cpuUsagePercent)
              << " " << colorizeByPercent(formatPercent(info.cpuUsagePercent), info.cpuUsagePercent) << "\n";
    if (info.cpuTemperatureC >= 0) {
        std::cout << "  " << label("CPU темп:    ") << colorizeByPercent(
            (std::ostringstream() << std::fixed << std::setprecision(1) << info.cpuTemperatureC << " C").str(),
            info.cpuTemperatureC) << "\n";
    }
    std::cout << "  " << label("RAM:         ") << colorizeByPercent(progressBar(info.ramUsagePercent, 25), info.ramUsagePercent)
              << " " << formatBytes(info.ramUsedKB * 1024.0) << " / " << formatBytes(info.ramTotalKB * 1024.0) << "\n";
    std::cout << "  " << label("FPS-советы:  ") << (info.healthScore > 70 ? "Система готова к играм" : "Возможны просадки FPS") << "\n";
    std::cout << separator(50) << "\n";
    std::cout.flush();
}

// ------------------------------------------------------------------
// Живой режим (кадр)
// ------------------------------------------------------------------
void Display::printLiveFrame(const SystemInfo& info, std::unordered_map<std::string, Sparkline>& sparks,
                             bool clearScreen) const {
    sparks["cpu"].add(info.cpuUsagePercent);
    sparks["ram"].add(info.ramUsagePercent);
    sparks["disk"].add(info.diskUsagePercent);
    sparks["net_down"].add(info.netDownloadBytesPerSec / 1024.0); // в КБ/с для разумного масштаба
    sparks["net_up"].add(info.netUploadBytesPerSec / 1024.0);

    // Очищаем экран и переводим курсор в начало (стандартная ANSI-последовательность).
    // При выводе не в терминал пропускаем: иначе в файле окажутся escape-коды.
    if (clearScreen) std::cout << "\033[2J\033[H";

    std::cout << theme_.primary << theme_.bold << "  sysflex LIVE" << theme_.reset
              << label("  (Ctrl+C для выхода)  ") << info.currentTime << "\n";
    std::cout << separator(60) << "\n";

    std::cout << "  " << label("CPU  ") << colorizeByPercent(formatPercent(info.cpuUsagePercent), info.cpuUsagePercent)
              << "  " << theme_.secondary << sparks["cpu"].render(100.0) << theme_.reset << "\n";
    std::cout << "  " << label("RAM  ") << colorizeByPercent(formatPercent(info.ramUsagePercent), info.ramUsagePercent)
              << "  " << theme_.secondary << sparks["ram"].render(100.0) << theme_.reset << "\n";
    std::cout << "  " << label("Диск ") << colorizeByPercent(formatPercent(info.diskUsagePercent), info.diskUsagePercent)
              << "  " << theme_.secondary << sparks["disk"].render(100.0) << theme_.reset << "\n";
    std::cout << "  " << label("\u2193 Сеть") << " " << formatSpeed(info.netDownloadBytesPerSec)
              << "  " << theme_.secondary << sparks["net_down"].render(-1.0) << theme_.reset << "\n";
    std::cout << "  " << label("\u2191 Сеть") << " " << formatSpeed(info.netUploadBytesPerSec)
              << "  " << theme_.secondary << sparks["net_up"].render(-1.0) << theme_.reset << "\n";

    std::cout << "\n  " << label("Здоровье: ") << colorizeByPercent(std::to_string(info.healthScore) + "/100", 100 - info.healthScore)
              << "   " << label("Аптайм: ") << formatTime(info.uptimeSeconds)
              << "   " << label("Load: ") << std::fixed << std::setprecision(2) << info.loadAverage[0] << "\n";

    // Строка с показателями, которые реально пересчитываются на каждом
    // кадре (переключения контекста, прерывания, форки, IOPS — всё меняется
    // секунда от секунды, а не подставляется однократно при старте).
    std::cout << "  " << label("Ctx/с: ") << std::fixed << std::setprecision(0) << info.ctxSwitchesPerSec
              << "   " << label("Intr/с: ") << info.interruptsPerSec
              << "   " << label("Fork/с: ") << info.forksPerSec
              << "   " << label("IOPS: ") << std::setprecision(1) << (info.diskReadOpsPerSec + info.diskWriteOpsPerSec)
              << "   " << label("TCP: ") << info.tcpEstablishedCount
              << "   " << label("FD: ") << info.openFileDescriptors << "\n";

    // Пакет 2: iowait/steal, очередь диска и загрузка по ядрам — тоже
    // реально пересчитываются на каждом кадре.
    std::cout << "  " << label("iowait: ") << std::setprecision(1) << info.cpuIowaitPercent << "%"
              << "   " << label("steal: ") << info.cpuStealPercent << "%"
              << "   " << label("Диск в очереди: ") << info.diskIoInFlight
              << "   " << label("Потоков: ") << info.totalThreadCount
              << "   " << label("Ядра: ");
    for (size_t i = 0; i < info.perCoreUsagePercent.size() && i < 8; ++i) {
        std::cout << std::setprecision(0) << info.perCoreUsagePercent[i] << "% ";
    }
    std::cout << "\n";

    std::cout << "\n  " << header("Топ процессов") << "\n";
    std::cout << "    " << label("PID     ИМЯ                  CPU%    MEM%") << "\n";
    for (auto& p : info.topProcesses) {
        std::cout << "    " << std::left << std::setw(8) << p.pid
                  << std::setw(22) << p.name.substr(0, 20)
                  << std::right << std::setw(6) << std::fixed << std::setprecision(1) << p.cpuPercent
                  << "%  " << std::setw(5) << p.memPercent << "%\n";
    }
    std::cout << separator(60) << "\n";
    std::cout.flush();
}

// ------------------------------------------------------------------
// Отчёт бенчмарка
// ------------------------------------------------------------------
void Display::printBenchmarkResult(double cpuScore, double memScore, double diskScore,
                                    double totalScore, long durationMs) const {
    std::cout << "\n" << theme_.primary << theme_.bold << "  Результаты бенчмарка sysflex" << theme_.reset << "\n";
    std::cout << separator(50) << "\n";
    std::cout << "  " << label("CPU тест:   ") << std::fixed << std::setprecision(2) << cpuScore << " баллов\n";
    std::cout << "  " << label("Память:     ") << memScore << " баллов\n";
    std::cout << "  " << label("Диск (I/O): ") << diskScore << " баллов\n";
    std::cout << separator(50) << "\n";
    std::cout << "  " << theme_.success << theme_.bold << "  Итоговый балл: " << totalScore << theme_.reset << "\n";
    std::cout << "  " << label("Время выполнения: ") << durationMs << " мс\n";
    std::cout << separator(50) << "\n";
}

// ------------------------------------------------------------------
// Вывод результата плагина
// ------------------------------------------------------------------
void Display::printPluginOutput(const std::string& pluginName, const std::string& output) const {
    if (trim(output).empty()) return;
    std::cout << "\n" << header("Плагин: " + pluginName) << "\n";
    std::cout << "  " << trim(output) << "\n";
    std::cout.flush();
}

} // namespace sysflex
