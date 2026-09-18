// system_info.cpp — реализация SystemMonitor: чтение /proc, /sys и вызовы
// внешних утилит для сбора полной картины состояния Linux-системы.
#include "sysflex/system_info.hpp"
#include "sysflex/utils.hpp"

#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <map>
#include <unordered_set>
#include <limits.h>

namespace sysflex {

using namespace utils;

SystemMonitor::SystemMonitor(int topProcessCount)
    : topProcessCount_(topProcessCount > 0 ? topProcessCount : 5) {}

SystemInfo SystemMonitor::update() {
    SystemInfo info;
    info.timestamp = std::chrono::steady_clock::now();

    // --- Быстрые источники: /proc, /sys, statvfs. Ни одного fork/exec. ---
    collectCpu(info);
    collectMemory(info);
    collectSwap(info);
    collectDisk(info);
    collectNetwork(info);
    collectBattery(info);
    collectUptime(info);
    collectProcesses(info);
    collectShellTerminal(info);
    collectOsKernelHost(info);
    collectInodes(info);
    collectTime(info);
    collectExtras(info);
    collectExtras2(info);

    // --- Медленные источники: внешние утилиты (nvidia-smi, docker, lsusb,
    // bluetoothctl, playerctl, git, who). Обновляются не чаще одного раза в
    // slowRefreshSec_ секунд, между обновлениями отдаётся кэш. ---
    if (slowCacheStale(info)) {
        refreshSlowCache(info);
    }
    applySlowCache(info);

    computeHealthScore(info);

    prevTimestamp_ = info.timestamp;
    havePrevTimestamp_ = true;

    return info;
}

// ------------------------------------------------------------------
// Кэш «медленных» данных: что перечитывать, а что взять из кэша
// ------------------------------------------------------------------
bool SystemMonitor::slowCacheStale(const SystemInfo& info) const {
    if (!slow_.filled) return true;
    if (slowRefreshSec_ <= 0.0) return true; // кэш отключён
    const double age = std::chrono::duration<double>(info.timestamp - slow_.fetchedAt).count();
    return age >= slowRefreshSec_;
}

void SystemMonitor::refreshSlowCache(SystemInfo& info) {
    collectGpu(info);
    collectDocker(info);
    collectGit(info);
    collectMusic(info);
    collectUsb(info);
    collectBluetooth(info);
    collectUsers(info);

    slow_.gpuModel = info.gpuModel;
    slow_.gpuTemperatureC = info.gpuTemperatureC;
    slow_.gpuMemUsedMB = info.gpuMemUsedMB;
    slow_.gpuMemTotalMB = info.gpuMemTotalMB;
    slow_.dockerContainerCount = info.dockerContainerCount;
    slow_.gitBranch = info.gitBranch;
    slow_.musicStatus = info.musicStatus;
    slow_.usbDevices = info.usbDevices;
    slow_.bluetoothDevices = info.bluetoothDevices;
    slow_.loggedUsersCount = info.loggedUsersCount;
    slow_.fetchedAt = info.timestamp;
    slow_.filled = true;
}

void SystemMonitor::applySlowCache(SystemInfo& info) const {
    if (!slow_.filled) return;
    info.gpuModel = slow_.gpuModel;
    info.gpuTemperatureC = slow_.gpuTemperatureC;
    info.gpuMemUsedMB = slow_.gpuMemUsedMB;
    info.gpuMemTotalMB = slow_.gpuMemTotalMB;
    info.dockerContainerCount = slow_.dockerContainerCount;
    info.gitBranch = slow_.gitBranch;
    info.musicStatus = slow_.musicStatus;
    info.usbDevices = slow_.usbDevices;
    info.usbDeviceCount = static_cast<int>(slow_.usbDevices.size());
    info.bluetoothDevices = slow_.bluetoothDevices;
    info.bluetoothDeviceCount = static_cast<int>(slow_.bluetoothDevices.size());
    info.loggedUsersCount = slow_.loggedUsersCount;
}

// ------------------------------------------------------------------
// Фильтр записей /proc/diskstats: только «целые» устройства
// ------------------------------------------------------------------
namespace {

// Список «целых» блочных устройств из /sys/block: ядро держит там только
// сами устройства, а разделы лежат в их подкаталогах (sda/sda1, nvme0n1/
// nvme0n1p1). Это надёжнее угадывания по имени — прежняя эвристика «есть
// цифры => это раздел» целиком отбрасывала mmcblk0 (eMMC) и md0.
std::unordered_set<std::string> readWholeBlockDevices() {
    std::unordered_set<std::string> devices;
    static const char* skipPrefixes[] = {"loop", "ram", "zram", "dm-", "sr", "fd"};
    for (const std::string& name : utils::listDirectory("/sys/block")) {
        bool skip = false;
        for (const char* prefix : skipPrefixes) {
            if (utils::startsWith(name, prefix)) { skip = true; break; }
        }
        if (!skip) devices.insert(name);
    }
    return devices;
}

// Запасная эвристика на случай, если /sys/block недоступен (контейнер).
bool looksLikeWholeDevice(const std::string& devName) {
    if (devName.empty()) return false;
    if (utils::startsWith(devName, "loop") || utils::startsWith(devName, "ram") ||
        utils::startsWith(devName, "dm-")) {
        return false;
    }
    if (utils::startsWith(devName, "nvme") || utils::startsWith(devName, "mmcblk")) {
        return devName.find('p') == std::string::npos; // nvme0n1 — целое, nvme0n1p1 — раздел
    }
    return !std::isdigit(static_cast<unsigned char>(devName.back()));
}

bool isWholeDiskEntry(const std::unordered_set<std::string>& whole, const std::string& devName) {
    if (!whole.empty()) return whole.count(devName) > 0;
    return looksLikeWholeDevice(devName);
}

} // namespace

// ------------------------------------------------------------------
// 1. CPU: загрузка, частота, температура, load average, ядра
// ------------------------------------------------------------------
void SystemMonitor::collectCpu(SystemInfo& info) {
    // --- Загрузка CPU через /proc/stat (дельта между двумя замерами) ---
    std::ifstream statFile("/proc/stat");
    std::string line;
    CpuJiffies cur;
    if (statFile.is_open() && std::getline(statFile, line)) {
        std::istringstream iss(line);
        std::string cpuLabel;
        iss >> cpuLabel >> cur.user >> cur.nice >> cur.system >> cur.idle
            >> cur.iowait >> cur.irq >> cur.softirq >> cur.steal;
    }

    if (havePrevJiffies_) {
        long long totalDelta = cur.total() - prevJiffies_.total();
        long long idleDelta = cur.idleAll() - prevJiffies_.idleAll();
        if (totalDelta > 0) {
            double usage = 100.0 * static_cast<double>(totalDelta - idleDelta) / static_cast<double>(totalDelta);
            info.cpuUsagePercent = std::clamp(usage, 0.0, 100.0);
        }
    }
    prevJiffies_ = cur;
    havePrevJiffies_ = true;

    // --- Количество логических ядер ---
    info.cpuCoreCount = static_cast<int>(std::thread::hardware_concurrency());
    if (info.cpuCoreCount <= 0) info.cpuCoreCount = 1;

    // --- Частота CPU (берём среднее по всем ядрам из /proc/cpuinfo) ---
    std::ifstream cpuinfo("/proc/cpuinfo");
    double freqSum = 0.0;
    int freqCount = 0;
    while (std::getline(cpuinfo, line)) {
        if (startsWith(line, "cpu MHz")) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                freqSum += toDouble(trim(line.substr(pos + 1)));
                ++freqCount;
            }
        }
    }
    info.cpuFrequencyMHz = (freqCount > 0) ? (freqSum / freqCount) : 0.0;

    // --- Температура CPU: ищем в /sys/class/thermal/thermal_zone*/temp или hwmon ---
    double bestTemp = -1.0;
    for (int zone = 0; zone < 12; ++zone) {
        std::string typePath = "/sys/class/thermal/thermal_zone" + std::to_string(zone) + "/type";
        std::string tempPath = "/sys/class/thermal/thermal_zone" + std::to_string(zone) + "/temp";
        if (!fileExists(tempPath)) continue;
        std::string type = toLowerStr(readFirstLine(typePath));
        double raw = toDouble(readFirstLine(tempPath), -1.0);
        if (raw < 0) continue;
        double celsius = raw / 1000.0;
        // Предпочитаем зоны, относящиеся именно к процессору
        if (type.find("cpu") != std::string::npos || type.find("x86_pkg_temp") != std::string::npos
            || type.find("soc") != std::string::npos) {
            bestTemp = celsius;
            break;
        }
        if (bestTemp < 0) bestTemp = celsius; // запасной вариант — первая найденная зона
    }
    info.cpuTemperatureC = bestTemp;

    // --- Температуры по ядрам через hwmon (coretemp), если доступны ---
    DIR* hwmonDir = opendir("/sys/class/hwmon");
    if (hwmonDir) {
        struct dirent* entry;
        while ((entry = readdir(hwmonDir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            std::string base = "/sys/class/hwmon/" + name;
            std::string hwmonName = toLowerStr(readFirstLine(base + "/name"));
            if (hwmonName.find("coretemp") == std::string::npos &&
                hwmonName.find("k10temp") == std::string::npos) continue;
            for (int i = 1; i <= 32; ++i) {
                std::string inputPath = base + "/temp" + std::to_string(i) + "_input";
                std::string labelPath = base + "/temp" + std::to_string(i) + "_label";
                if (!fileExists(inputPath)) break;
                std::string lbl = toLowerStr(readFirstLine(labelPath));
                double raw = toDouble(readFirstLine(inputPath), -1.0);
                if (raw < 0) continue;
                // Интересуют только записи per-core ("Core N"), пропускаем общий "Package id"
                if (lbl.find("core") != std::string::npos) {
                    info.perCoreTempsC.push_back(raw / 1000.0);
                }
            }
        }
        closedir(hwmonDir);
    }

    // --- Load average через /proc/loadavg ---
    std::string loadLine = readFirstLine("/proc/loadavg");
    auto parts = splitWs(loadLine);
    if (parts.size() >= 3) {
        info.loadAverage = {toDouble(parts[0]), toDouble(parts[1]), toDouble(parts[2])};
    }
}

// ------------------------------------------------------------------
// 2. RAM
// ------------------------------------------------------------------
void SystemMonitor::collectMemory(SystemInfo& info) {
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    uint64_t memTotal = 0, memAvailable = 0, memFree = 0, buffers = 0, cached = 0;
    while (std::getline(meminfo, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value;
        iss >> key >> value;
        if (key == "MemTotal:") memTotal = value;
        else if (key == "MemAvailable:") memAvailable = value;
        else if (key == "MemFree:") memFree = value;
        else if (key == "Buffers:") buffers = value;
        else if (key == "Cached:") cached = value;
    }
    info.ramTotalKB = memTotal;
    // Если MemAvailable отсутствует (очень старые ядра) — считаем приблизительно
    uint64_t available = (memAvailable > 0) ? memAvailable : (memFree + buffers + cached);
    info.ramUsedKB = (memTotal > available) ? (memTotal - available) : 0;
    info.ramUsagePercent = (memTotal > 0) ? (100.0 * static_cast<double>(info.ramUsedKB) / static_cast<double>(memTotal)) : 0.0;
}

// ------------------------------------------------------------------
// 7. Swap
// ------------------------------------------------------------------
void SystemMonitor::collectSwap(SystemInfo& info) {
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    uint64_t swapTotal = 0, swapFree = 0;
    while (std::getline(meminfo, line)) {
        std::istringstream iss(line);
        std::string key;
        uint64_t value;
        iss >> key >> value;
        if (key == "SwapTotal:") swapTotal = value;
        else if (key == "SwapFree:") swapFree = value;
    }
    info.swapTotalKB = swapTotal;
    info.swapUsedKB = (swapTotal > swapFree) ? (swapTotal - swapFree) : 0;
    info.swapUsagePercent = (swapTotal > 0) ? (100.0 * static_cast<double>(info.swapUsedKB) / static_cast<double>(swapTotal)) : 0.0;
}

// ------------------------------------------------------------------
// 3. Disk: занятое место и I/O скорость
// ------------------------------------------------------------------
void SystemMonitor::collectDisk(SystemInfo& info) {
    // --- Использование места на корневом разделе ---
    struct statvfs vfs{};
    if (statvfs("/", &vfs) == 0) {
        uint64_t total = static_cast<uint64_t>(vfs.f_blocks) * vfs.f_frsize;
        uint64_t free = static_cast<uint64_t>(vfs.f_bfree) * vfs.f_frsize;
        info.diskTotalBytes = total;
        info.diskUsedBytes = (total > free) ? (total - free) : 0;
        info.diskUsagePercent = (total > 0) ? (100.0 * static_cast<double>(info.diskUsedBytes) / static_cast<double>(total)) : 0.0;
    }

    // --- I/O скорость через /proc/diskstats (суммируем по всем физическим дискам sdX/nvmeXnY/vdX) ---
    std::ifstream diskstats("/proc/diskstats");
    std::string line;
    uint64_t readSectors = 0, writeSectors = 0;
    const auto wholeDevices = readWholeBlockDevices();
    while (std::getline(diskstats, line)) {
        auto f = splitWs(line);
        if (f.size() < 14) continue;
        const std::string& devName = f[2];
        // Считаем только «целые» диски, а не их разделы, иначе байты и
        // операции учитываются дважды (устройство + его разделы).
        if (!isWholeDiskEntry(wholeDevices, devName)) continue;

        readSectors += toLong(f[5]);
        writeSectors += toLong(f[9]);
    }

    if (havePrevDisk_ && havePrevTimestamp_) {
        double dt = std::chrono::duration<double>(info.timestamp - prevTimestamp_).count();
        if (dt > 0) {
            // Стандартный размер сектора — 512 байт
            double readBytes = static_cast<double>(readSectors - prevDiskReadSectors_) * 512.0;
            double writeBytes = static_cast<double>(writeSectors - prevDiskWriteSectors_) * 512.0;
            info.diskReadBytesPerSec = std::max(0.0, readBytes / dt);
            info.diskWriteBytesPerSec = std::max(0.0, writeBytes / dt);
        }
    }
    prevDiskReadSectors_ = readSectors;
    prevDiskWriteSectors_ = writeSectors;
    havePrevDisk_ = true;
}

// ------------------------------------------------------------------
// 4. Network
// ------------------------------------------------------------------
void SystemMonitor::collectNetwork(SystemInfo& info) {
    std::ifstream netdev("/proc/net/dev");
    std::string line;
    uint64_t rxTotal = 0, txTotal = 0;
    std::string chosenIface;

    // Пропускаем два заголовочных ряда
    std::getline(netdev, line);
    std::getline(netdev, line);
    while (std::getline(netdev, line)) {
        auto colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;
        std::string iface = trim(line.substr(0, colonPos));
        if (iface == "lo") continue; // пропускаем loopback

        auto fields = splitWs(line.substr(colonPos + 1));
        if (fields.size() < 9) continue;
        uint64_t rx = static_cast<uint64_t>(toLong(fields[0]));
        uint64_t tx = static_cast<uint64_t>(toLong(fields[8]));
        rxTotal += rx;
        txTotal += tx;
        if (chosenIface.empty() && (rx > 0 || tx > 0)) chosenIface = iface;
    }
    if (chosenIface.empty()) chosenIface = "n/a";
    info.netInterface = chosenIface;

    if (havePrevNet_ && havePrevTimestamp_) {
        double dt = std::chrono::duration<double>(info.timestamp - prevTimestamp_).count();
        if (dt > 0) {
            info.netDownloadBytesPerSec = std::max(0.0, static_cast<double>(rxTotal - prevNetRxBytes_) / dt);
            info.netUploadBytesPerSec = std::max(0.0, static_cast<double>(txTotal - prevNetTxBytes_) / dt);
        }
    }
    prevNetRxBytes_ = rxTotal;
    prevNetTxBytes_ = txTotal;
    havePrevNet_ = true;
}

// ------------------------------------------------------------------
// 5. GPU
// ------------------------------------------------------------------
void SystemMonitor::collectGpu(SystemInfo& info) {
    // Пытаемся сначала NVIDIA (nvidia-smi), затем AMD (через lspci/sysfs)
    if (commandExists("nvidia-smi")) {
        std::string out = execCommand(
            "nvidia-smi --query-gpu=name,temperature.gpu,memory.used,memory.total "
            "--format=csv,noheader,nounits");
        out = trim(out);
        if (!out.empty()) {
            auto fields = split(out, ',');
            if (fields.size() >= 4) {
                info.gpuModel = trim(fields[0]);
                info.gpuTemperatureC = toDouble(trim(fields[1]), -1.0);
                info.gpuMemUsedMB = toDouble(trim(fields[2]), -1.0);
                info.gpuMemTotalMB = toDouble(trim(fields[3]), -1.0);
                return;
            }
        }
    }

    // AMD/Intel — определяем модель через lspci
    if (commandExists("lspci")) {
        std::string out = execCommand("lspci | grep -i 'vga\\|3d\\|display'");
        auto lines = split(out, '\n');
        if (!lines.empty() && !trim(lines[0]).empty()) {
            auto colonPos = lines[0].find(": ");
            info.gpuModel = (colonPos != std::string::npos) ? trim(lines[0].substr(colonPos + 2)) : trim(lines[0]);
        }
    }

    // Температура AMD через hwmon (amdgpu)
    DIR* hwmonDir = opendir("/sys/class/hwmon");
    if (hwmonDir) {
        struct dirent* entry;
        while ((entry = readdir(hwmonDir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            std::string base = "/sys/class/hwmon/" + name;
            std::string hwmonName = toLowerStr(readFirstLine(base + "/name"));
            if (hwmonName.find("amdgpu") == std::string::npos) continue;
            double raw = toDouble(readFirstLine(base + "/temp1_input"), -1.0);
            if (raw > 0) info.gpuTemperatureC = raw / 1000.0;
        }
        closedir(hwmonDir);
    }
}

// ------------------------------------------------------------------
// 6. Battery
// ------------------------------------------------------------------
void SystemMonitor::collectBattery(SystemInfo& info) {
    DIR* dir = opendir("/sys/class/power_supply");
    if (!dir) return;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string base = "/sys/class/power_supply/" + name;
        std::string type = readFirstLine(base + "/type");
        if (type != "Battery") continue;

        info.hasBattery = true;
        info.batteryPercent = static_cast<int>(toLong(readFirstLine(base + "/capacity"), -1));
        info.batteryStatus = readFirstLine(base + "/status");
        break; // берём первую найденную батарею
    }
    closedir(dir);
}

// ------------------------------------------------------------------
// 8. Uptime
// ------------------------------------------------------------------
void SystemMonitor::collectUptime(SystemInfo& info) {
    std::string line = readFirstLine("/proc/uptime");
    auto parts = splitWs(line);
    if (!parts.empty()) {
        info.uptimeSeconds = static_cast<long>(toDouble(parts[0]));
    }
}

// ------------------------------------------------------------------
// 9. Процессы: количество и top-5 по CPU
// ------------------------------------------------------------------
void SystemMonitor::collectProcesses(SystemInfo& info) {
    DIR* dir = opendir("/proc");
    if (!dir) return;

    struct dirent* entry;
    std::vector<ProcessInfo> all;
    int count = 0;

    const long clockTicks = sysconf(_SC_CLK_TCK);
    const double ticksPerSec = static_cast<double>(clockTicks > 0 ? clockTicks : 100);
    const long totalMemKB = static_cast<long>(info.ramTotalKB);
    const long pageSizeKB = sysconf(_SC_PAGESIZE) / 1024;

    // Интервал между этим и предыдущим кадром — по нему считаем мгновенную
    // загрузку процесса. На первом кадре дельты нет, поэтому там остаётся
    // запасной вариант (средняя загрузка за время жизни процесса).
    const double dt = havePrevTimestamp_
        ? std::chrono::duration<double>(info.timestamp - prevTimestamp_).count()
        : 0.0;
    // Многопоточный процесс может занимать больше 100% одного ядра.
    unsigned int hwCores = std::thread::hardware_concurrency();
    const double maxPercent = 100.0 * static_cast<double>(hwCores > 0 ? hwCores : 1);

    std::unordered_map<int, long long> currentTicks;

    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.empty() || !std::isdigit(static_cast<unsigned char>(name[0]))) continue;
        ++count;

        int pid = static_cast<int>(toLong(name));
        std::string statPath = "/proc/" + name + "/stat";
        std::string statContent = readFile(statPath);
        if (statContent.empty()) continue;

        // Формат: pid (comm) state ppid ... utime(14) stime(15) ...
        auto commStart = statContent.find('(');
        auto commEnd = statContent.rfind(')');
        if (commStart == std::string::npos || commEnd == std::string::npos) continue;
        std::string comm = statContent.substr(commStart + 1, commEnd - commStart - 1);
        std::string rest = statContent.substr(commEnd + 2);
        auto fields = splitWs(rest);
        // fields[0] = state (3-е поле stat), далее по порядку. utime — 14-е поле от начала,
        // т.е. fields[11] в этом смещённом массиве (14 - 3 = 11).
        if (fields.size() < 22) continue;
        long utime = toLong(fields[11]);
        long stime = toLong(fields[12]);
        long totalTimeTicks = utime + stime;
        double seconds = static_cast<double>(totalTimeTicks) / ticksPerSec;

        // Мгновенная загрузка: сколько процессного времени процесс потратил
        // между двумя кадрами. Раньше здесь считалось отношение к аптайму,
        // из-за чего в топе висели давно отработавшие процессы, а реально
        // нагруженный прямо сейчас мог вообще не попасть в список.
        double cpuPercent = 0.0;
        auto prev = prevProcTicks_.find(pid);
        if (prev != prevProcTicks_.end() && dt > 0.0) {
            double deltaTicks = static_cast<double>(totalTimeTicks - prev->second);
            if (deltaTicks < 0.0) deltaTicks = 0.0; // PID был переиспользован
            cpuPercent = std::clamp(100.0 * deltaTicks / ticksPerSec / dt, 0.0, maxPercent);
        } else if (info.uptimeSeconds > 0) {
            // Запасной вариант для первого кадра — средняя за время жизни.
            cpuPercent = std::clamp(100.0 * seconds / static_cast<double>(info.uptimeSeconds), 0.0, maxPercent);
        }

        // RSS в страницах памяти — 24-е поле от начала (fields[21])
        long rssPages = toLong(fields[21]);
        double memPercent = 0.0;
        if (totalMemKB > 0) {
            memPercent = 100.0 * static_cast<double>(rssPages * pageSizeKB) / static_cast<double>(totalMemKB);
        }

        ProcessInfo p;
        p.pid = pid;
        p.name = comm;
        p.cpuPercent = cpuPercent;
        p.memPercent = memPercent;
        all.push_back(p);

        currentTicks.emplace(pid, totalTimeTicks);
    }
    closedir(dir);

    prevProcTicks_ = std::move(currentTicks);
    info.processCount = count;

    std::sort(all.begin(), all.end(), [](const ProcessInfo& a, const ProcessInfo& b) {
        return a.cpuPercent > b.cpuPercent;
    });
    if (static_cast<int>(all.size()) > topProcessCount_) all.resize(static_cast<size_t>(topProcessCount_));
    info.topProcesses = all;
}

// ------------------------------------------------------------------
// 10. Shell и terminal
// ------------------------------------------------------------------
void SystemMonitor::collectShellTerminal(SystemInfo& info) {
    const char* shell = std::getenv("SHELL");
    if (shell) {
        std::string path = shell;
        auto slashPos = path.find_last_of('/');
        info.shellName = (slashPos != std::string::npos) ? path.substr(slashPos + 1) : path;
    } else {
        info.shellName = "unknown";
    }

    const char* term = std::getenv("TERM");
    const char* termProgram = std::getenv("TERM_PROGRAM");
    if (termProgram) {
        info.terminalName = termProgram;
    } else if (term) {
        info.terminalName = term;
    } else {
        info.terminalName = "unknown";
    }
}

// ------------------------------------------------------------------
// 11-13. OS / Kernel / Hostname
// ------------------------------------------------------------------
void SystemMonitor::collectOsKernelHost(SystemInfo& info) {
    // /etc/os-release содержит NAME, PRETTY_NAME, ID и т.д.
    std::ifstream osRelease("/etc/os-release");
    std::string line;
    while (std::getline(osRelease, line)) {
        if (startsWith(line, "PRETTY_NAME=")) {
            std::string val = line.substr(std::string("PRETTY_NAME=").size());
            val.erase(std::remove(val.begin(), val.end(), '"'), val.end());
            info.osPrettyName = val;
        } else if (startsWith(line, "NAME=")) {
            std::string val = line.substr(std::string("NAME=").size());
            val.erase(std::remove(val.begin(), val.end(), '"'), val.end());
            info.osName = val;
        }
    }
    if (info.osPrettyName.empty()) info.osPrettyName = info.osName.empty() ? "Unknown Linux" : info.osName;

    // Версия ядра и hostname через uname()
    struct utsname uts{};
    if (uname(&uts) == 0) {
        info.kernelVersion = uts.release;
        info.hostname = uts.nodename;
    } else {
        info.kernelVersion = "unknown";
        info.hostname = "unknown";
    }
}

// ------------------------------------------------------------------
// 14. Docker
// ------------------------------------------------------------------
void SystemMonitor::collectDocker(SystemInfo& info) {
    if (!commandExists("docker")) {
        info.dockerContainerCount = -1;
        return;
    }
    std::string out = execCommand("docker ps -q");
    if (out.empty()) {
        // Может быть 0 контейнеров, либо демон недоступен (нет прав) —
        // в обоих случаях показываем 0, т.к. отличить программно сложно
        // без анализа кода возврата, а падать не хотим.
        info.dockerContainerCount = 0;
        return;
    }
    auto lines = split(out, '\n');
    int count = 0;
    for (auto& l : lines) {
        if (!trim(l).empty()) ++count;
    }
    info.dockerContainerCount = count;
}

// ------------------------------------------------------------------
// 15. Git branch (в текущем рабочем каталоге)
// ------------------------------------------------------------------
void SystemMonitor::collectGit(SystemInfo& info) {
    if (!commandExists("git")) return;
    std::string out = trim(execCommand("git rev-parse --abbrev-ref HEAD"));
    if (out.empty() || out == "HEAD") return; // не репозиторий либо detached HEAD
    info.gitBranch = out;
}

// ------------------------------------------------------------------
// 16. Музыкальный плеер (playerctl)
// ------------------------------------------------------------------
void SystemMonitor::collectMusic(SystemInfo& info) {
    if (!commandExists("playerctl")) return;
    std::string status = trim(execCommand("playerctl status"));
    if (status.empty()) return;
    std::string artist = trim(execCommand("playerctl metadata artist"));
    std::string title = trim(execCommand("playerctl metadata title"));
    if (title.empty()) return;
    info.musicStatus = (artist.empty() ? title : (artist + " - " + title)) + " [" + status + "]";
}

// ------------------------------------------------------------------
// 17. USB устройства
// ------------------------------------------------------------------
void SystemMonitor::collectUsb(SystemInfo& info) {
    if (!commandExists("lsusb")) return;
    std::string out = execCommand("lsusb");
    auto lines = split(out, '\n');
    for (auto& l : lines) {
        std::string trimmed = trim(l);
        if (trimmed.empty()) continue;
        info.usbDevices.push_back(trimmed);
    }
    info.usbDeviceCount = static_cast<int>(info.usbDevices.size());
}

// ------------------------------------------------------------------
// 18. Bluetooth устройства
// ------------------------------------------------------------------
void SystemMonitor::collectBluetooth(SystemInfo& info) {
    if (!commandExists("bluetoothctl")) return;
    std::string out = execCommand("bluetoothctl devices");
    auto lines = split(out, '\n');
    for (auto& l : lines) {
        std::string trimmed = trim(l);
        if (trimmed.empty()) continue;
        info.bluetoothDevices.push_back(trimmed);
    }
    info.bluetoothDeviceCount = static_cast<int>(info.bluetoothDevices.size());
}

// ------------------------------------------------------------------
// 19. Использование inode на корневом разделе
// ------------------------------------------------------------------
void SystemMonitor::collectInodes(SystemInfo& info) {
    struct statvfs vfs{};
    if (statvfs("/", &vfs) == 0 && vfs.f_files > 0) {
        uint64_t total = vfs.f_files;
        uint64_t free = vfs.f_ffree;
        uint64_t used = (total > free) ? (total - free) : 0;
        info.inodeUsagePercent = 100.0 * static_cast<double>(used) / static_cast<double>(total);
    }
}

// ------------------------------------------------------------------
// 20. Количество залогиненных пользователей (через `who`)
// ------------------------------------------------------------------
void SystemMonitor::collectUsers(SystemInfo& info) {
    std::string out = execCommand("who");
    auto lines = split(out, '\n');
    int count = 0;
    for (auto& l : lines) {
        if (!trim(l).empty()) ++count;
    }
    info.loggedUsersCount = count;
}

// ------------------------------------------------------------------
// 21. Текущее время
// ------------------------------------------------------------------
void SystemMonitor::collectTime(SystemInfo& info) {
    info.currentTime = currentDateTimeString();
}

// ------------------------------------------------------------------
// 22. Оценка "здоровья" системы (0-100), эвристика на основе ключевых метрик
// ------------------------------------------------------------------
void SystemMonitor::computeHealthScore(SystemInfo& info) {
    double score = 100.0;

    // Штраф за высокую загрузку CPU
    if (info.cpuUsagePercent > 90) score -= 25;
    else if (info.cpuUsagePercent > 75) score -= 12;
    else if (info.cpuUsagePercent > 50) score -= 5;

    // Штраф за высокую загрузку RAM
    if (info.ramUsagePercent > 90) score -= 20;
    else if (info.ramUsagePercent > 75) score -= 10;
    else if (info.ramUsagePercent > 50) score -= 4;

    // Штраф за заполненный диск
    if (info.diskUsagePercent > 95) score -= 20;
    else if (info.diskUsagePercent > 85) score -= 10;
    else if (info.diskUsagePercent > 70) score -= 4;

    // Штраф за перегрев CPU
    if (info.cpuTemperatureC > 85) score -= 20;
    else if (info.cpuTemperatureC > 75) score -= 10;
    else if (info.cpuTemperatureC > 65) score -= 3;

    // Штраф за высокое использование swap (признак нехватки RAM)
    if (info.swapUsagePercent > 50) score -= 10;
    else if (info.swapUsagePercent > 10) score -= 3;

    // Штраф за высокий load average относительно числа ядер
    if (info.cpuCoreCount > 0) {
        double normalizedLoad = info.loadAverage[0] / info.cpuCoreCount;
        if (normalizedLoad > 2.0) score -= 15;
        else if (normalizedLoad > 1.0) score -= 6;
    }

    info.healthScore = static_cast<int>(std::clamp(score, 0.0, 100.0));
}

// ------------------------------------------------------------------
// Расширенные метрики: CPU, память, диск, сеть, датчики, окружение.
// Всё читается напрямую из /proc и /sys, поэтому в живом режиме эти
// числа реально пересчитываются на каждом кадре, а не берутся из кэша.
// ------------------------------------------------------------------
void SystemMonitor::collectExtras(SystemInfo& info) {
    double dt = 0.0;
    if (havePrevTimestamp_) {
        dt = std::chrono::duration<double>(info.timestamp - prevTimestamp_).count();
    }

    // --- 1-2. Модель и размер кэша CPU (/proc/cpuinfo) ---
    {
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (info.cpuModelName.empty() && startsWith(line, "model name")) {
                auto pos = line.find(':');
                if (pos != std::string::npos) info.cpuModelName = trim(line.substr(pos + 1));
            } else if (info.cpuCacheSizeKB == 0 && startsWith(line, "cache size")) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    // Формат обычно "1024 KB" — берём первое число
                    info.cpuCacheSizeKB = static_cast<uint64_t>(toLong(trim(line.substr(pos + 1))));
                }
            }
            if (!info.cpuModelName.empty() && info.cpuCacheSizeKB > 0) break;
        }
    }

    // --- 3. CPU governor ---
    info.cpuGovernor = readFirstLine("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");

    // --- 4-5, 6. ctxt / intr / processes (форки) из /proc/stat ---
    {
        std::ifstream statFile("/proc/stat");
        std::string line;
        long long ctxt = 0, intr = 0, processesCreated = 0;
        while (std::getline(statFile, line)) {
            if (startsWith(line, "ctxt ")) {
                ctxt = toLong(trim(line.substr(5)));
            } else if (startsWith(line, "intr ")) {
                auto fields = splitWs(line);
                if (fields.size() > 1) intr = toLong(fields[1]); // первое число после "intr" — суммарный счётчик
            } else if (startsWith(line, "processes ")) {
                processesCreated = toLong(trim(line.substr(10)));
            } else if (startsWith(line, "procs_running ")) {
                info.procsRunning = static_cast<int>(toLong(trim(line.substr(14))));
            } else if (startsWith(line, "procs_blocked ")) {
                info.procsBlocked = static_cast<int>(toLong(trim(line.substr(14))));
            }
        }
        if (havePrevExtras_ && dt > 0) {
            info.ctxSwitchesPerSec = std::max(0.0, static_cast<double>(ctxt - prevCtxt_) / dt);
            info.interruptsPerSec = std::max(0.0, static_cast<double>(intr - prevIntr_) / dt);
            info.forksPerSec = std::max(0.0, static_cast<double>(processesCreated - prevProcessesCreated_) / dt);
        }
        prevCtxt_ = ctxt;
        prevIntr_ = intr;
        prevProcessesCreated_ = processesCreated;
    }

    // --- 9. Зомби-процессы (лёгкий проход по /proc/*/stat, без полного парсинга) ---
    {
        DIR* dir = opendir("/proc");
        int zombies = 0;
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name.empty() || !std::isdigit(static_cast<unsigned char>(name[0]))) continue;
                std::string statContent = readFile("/proc/" + name + "/stat");
                auto commEnd = statContent.rfind(')');
                if (commEnd == std::string::npos || commEnd + 2 >= statContent.size()) continue;
                if (statContent[commEnd + 2] == 'Z') ++zombies;
            }
            closedir(dir);
        }
        info.zombieCount = zombies;
    }

    // --- 10. Энтропия ядра ---
    {
        std::string s = readFirstLine("/proc/sys/kernel/random/entropy_avail");
        info.entropyAvailable = s.empty() ? -1 : static_cast<int>(toLong(s, -1));
    }

    // --- 11-16. Расширенные данные о памяти ---
    {
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        uint64_t hpTotal = 0, hpFree = 0, hpSizeKB = 2048; // размер одной HugePage по умолчанию 2МБ
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            iss >> key >> value;
            if (key == "MemAvailable:") info.memAvailableKB = value;
            else if (key == "Buffers:") info.buffersKB = value;
            else if (key == "Cached:") info.cachedKB = value;
            else if (key == "Dirty:") info.dirtyKB = value;
            else if (key == "HugePages_Total:") hpTotal = value;
            else if (key == "HugePages_Free:") hpFree = value;
            else if (key == "Hugepagesize:") hpSizeKB = value;
        }
        (void)hpSizeKB;
        info.hugePagesTotal = hpTotal;
        info.hugePagesFree = hpFree;
    }

    // --- 17-18. IOPS диска (операций/сек) ---
    {
        std::ifstream diskstats("/proc/diskstats");
        std::string line;
        uint64_t readOps = 0, writeOps = 0;
        const auto wholeDevices = readWholeBlockDevices();
        while (std::getline(diskstats, line)) {
            auto f = splitWs(line);
            if (f.size() < 14) continue;
            const std::string& devName = f[2];
            if (!isWholeDiskEntry(wholeDevices, devName)) continue;
            readOps += static_cast<uint64_t>(toLong(f[3]));
            writeOps += static_cast<uint64_t>(toLong(f[7]));
        }
        if (havePrevExtras_ && dt > 0) {
            info.diskReadOpsPerSec = std::max(0.0, static_cast<double>(readOps - prevDiskReadOps_) / dt);
            info.diskWriteOpsPerSec = std::max(0.0, static_cast<double>(writeOps - prevDiskWriteOps_) / dt);
        }
        prevDiskReadOps_ = readOps;
        prevDiskWriteOps_ = writeOps;
    }

    // --- 19-20. Тип корневой ФС и число смонтированных ФС ---
    {
        std::ifstream mounts("/proc/mounts");
        std::string line;
        int mountCount = 0;
        while (std::getline(mounts, line)) {
            auto f = splitWs(line);
            if (f.size() < 3) continue;
            const std::string& device = f[0];
            const std::string& mountPoint = f[1];
            const std::string& fsType = f[2];
            // Пропускаем виртуальные/псевдо-ФС при подсчёте "реальных" точек монтирования
            static const std::vector<std::string> pseudo = {
                "proc", "sysfs", "tmpfs", "devtmpfs", "devpts", "cgroup", "cgroup2",
                "overlay", "squashfs", "debugfs", "tracefs", "securityfs", "pstore",
                "bpf", "mqueue", "hugetlbfs", "configfs", "fusectl", "autofs", "binfmt_misc"
            };
            if (std::find(pseudo.begin(), pseudo.end(), fsType) == pseudo.end()) ++mountCount;
            if (mountPoint == "/") info.rootFilesystemType = fsType;
            (void)device;
        }
        info.mountPointCount = mountCount;
    }

    // --- 21-22. TCP-соединения (ESTABLISHED / LISTEN) из /proc/net/tcp[6] ---
    {
        int established = 0, listening = 0;
        for (const char* path : {"/proc/net/tcp", "/proc/net/tcp6"}) {
            std::ifstream tcp(path);
            std::string line;
            std::getline(tcp, line); // заголовок
            while (std::getline(tcp, line)) {
                auto f = splitWs(line);
                if (f.size() < 4) continue;
                const std::string& state = f[3];
                if (state == "01") ++established;
                else if (state == "0A") ++listening;
            }
        }
        info.tcpEstablishedCount = established;
        info.tcpListenCount = listening;
    }

    // --- 23. Количество сетевых интерфейсов (кроме lo) ---
    {
        std::ifstream netdev("/proc/net/dev");
        std::string line;
        std::getline(netdev, line);
        std::getline(netdev, line);
        int ifaceCount = 0;
        while (std::getline(netdev, line)) {
            auto colonPos = line.find(':');
            if (colonPos == std::string::npos) continue;
            std::string iface = trim(line.substr(0, colonPos));
            if (iface != "lo" && !iface.empty()) ++ifaceCount;
        }
        info.networkInterfaceCount = ifaceCount;
    }

    // --- 24. DNS-серверы ---
    {
        std::ifstream resolv("/etc/resolv.conf");
        std::string line;
        std::vector<std::string> servers;
        while (std::getline(resolv, line)) {
            if (startsWith(trim(line), "nameserver")) {
                auto fields = splitWs(line);
                if (fields.size() >= 2) servers.push_back(fields[1]);
            }
        }
        std::ostringstream oss;
        for (size_t i = 0; i < servers.size(); ++i) {
            if (i) oss << ", ";
            oss << servers[i];
        }
        info.dnsServers = oss.str();
    }

    // --- 25-26. Вентиляторы и температурные датчики (/sys/class/hwmon) ---
    {
        int fanRpm = -1;
        double maxTemp = -1.0;
        DIR* hwmonDir = opendir("/sys/class/hwmon");
        if (hwmonDir) {
            struct dirent* entry;
            while ((entry = readdir(hwmonDir)) != nullptr) {
                std::string name = entry->d_name;
                if (name == "." || name == "..") continue;
                std::string base = "/sys/class/hwmon/" + name + "/";
                for (int i = 1; i <= 7; ++i) {
                    std::string fanPath = base + "fan" + std::to_string(i) + "_input";
                    if (fileExists(fanPath)) {
                        int rpm = static_cast<int>(toLong(readFirstLine(fanPath)));
                        if (rpm > 0) fanRpm = std::max(fanRpm, rpm);
                    }
                    std::string tempPath = base + "temp" + std::to_string(i) + "_input";
                    if (fileExists(tempPath)) {
                        double milliC = toDouble(readFirstLine(tempPath));
                        double celsius = milliC / 1000.0;
                        if (celsius > 0 && celsius < 200) maxTemp = std::max(maxTemp, celsius);
                    }
                }
            }
            closedir(hwmonDir);
        }
        info.fanSpeedRPM = fanRpm;
        info.maxSensorTempC = maxTemp;
    }

    // --- 27-28. Файловые дескрипторы (/proc/sys/fs/file-nr) ---
    {
        std::string line = readFirstLine("/proc/sys/fs/file-nr");
        auto fields = splitWs(line);
        if (fields.size() >= 3) {
            info.openFileDescriptors = static_cast<uint64_t>(toLong(fields[0]));
            info.maxFileDescriptors = static_cast<uint64_t>(toLong(fields[2]));
        }
    }

    // --- 29. Локаль ---
    {
        const char* lang = std::getenv("LANG");
        info.systemLocale = lang ? lang : "C";
    }

    // --- 30. Часовой пояс ---
    {
        std::string tz = readFile("/etc/timezone");
        tz = trim(tz);
        if (tz.empty()) {
            // Fallback: разбираем symlink /etc/localtime вида .../zoneinfo/Europe/Madrid.
            // Читаем ссылку системным вызовом readlink(2) — раньше здесь
            // запускался `readlink -f` через shell на каждом кадре.
            char buf[PATH_MAX] = {0};
            ssize_t len = readlink("/etc/localtime", buf, sizeof(buf) - 1);
            if (len > 0) {
                buf[len] = '\0';
                std::string link(buf);
                auto pos = link.find("zoneinfo/");
                if (pos != std::string::npos) tz = link.substr(pos + 9);
            }
        }
        info.systemTimezone = tz.empty() ? "UTC" : tz;
    }

    havePrevExtras_ = true;
}

// ------------------------------------------------------------------
// Диагностика ядра и подсистем. См. комментарий у полей в SystemInfo.
// ------------------------------------------------------------------
void SystemMonitor::collectExtras2(SystemInfo& info) {
    double dt = 0.0;
    if (havePrevTimestamp_) {
        dt = std::chrono::duration<double>(info.timestamp - prevTimestamp_).count();
    }
    (void)dt; // используется ниже только per-core, где нужна дельта

    // --- 31-33. Доли iowait/steal/softirq (из тех же jiffies, что и общая загрузка CPU) ---
    {
        std::ifstream statFile("/proc/stat");
        std::string line;
        if (std::getline(statFile, line)) {
            std::istringstream iss(line);
            std::string cpuLabel;
            CpuJiffies cur;
            iss >> cpuLabel >> cur.user >> cur.nice >> cur.system >> cur.idle
                >> cur.iowait >> cur.irq >> cur.softirq >> cur.steal;
            if (havePrevJiffies_) {
                long long totalDelta = cur.total() - prevJiffies_.total();
                if (totalDelta > 0) {
                    info.cpuIowaitPercent = std::clamp(100.0 * static_cast<double>(cur.iowait - prevJiffies_.iowait) / static_cast<double>(totalDelta), 0.0, 100.0);
                    info.cpuStealPercent = std::clamp(100.0 * static_cast<double>(cur.steal - prevJiffies_.steal) / static_cast<double>(totalDelta), 0.0, 100.0);
                    info.cpuSoftirqPercent = std::clamp(100.0 * static_cast<double>(cur.softirq - prevJiffies_.softirq) / static_cast<double>(totalDelta), 0.0, 100.0);
                }
            }
            // Примечание: prevJiffies_ уже обновляется и используется в collectCpu(),
            // поэтому здесь мы его только читаем, не перезаписываем повторно.
        }
    }

    // --- 34. Загрузка по каждому ядру отдельно (cpu0, cpu1, ... в /proc/stat) ---
    {
        std::ifstream statFile("/proc/stat");
        std::string line;
        std::vector<CpuJiffies> cores;
        while (std::getline(statFile, line)) {
            if (!startsWith(line, "cpu")) break; // строки cpuN идут в начале файла подряд
            bool isPerCoreLine = line.size() > 3 && std::isdigit(static_cast<unsigned char>(line[3]));
            if (!isPerCoreLine) continue; // это сводная строка "cpu " — пропускаем
            std::istringstream iss(line);
            std::string label;
            CpuJiffies c;
            iss >> label >> c.user >> c.nice >> c.system >> c.idle
                >> c.iowait >> c.irq >> c.softirq >> c.steal;
            cores.push_back(c);
        }
        info.perCoreUsagePercent.assign(cores.size(), 0.0);
        if (havePrevPerCore_ && prevPerCoreJiffies_.size() == cores.size()) {
            for (size_t i = 0; i < cores.size(); ++i) {
                long long totalDelta = cores[i].total() - prevPerCoreJiffies_[i].total();
                long long idleDelta = cores[i].idleAll() - prevPerCoreJiffies_[i].idleAll();
                if (totalDelta > 0) {
                    info.perCoreUsagePercent[i] = std::clamp(100.0 * static_cast<double>(totalDelta - idleDelta) / static_cast<double>(totalDelta), 0.0, 100.0);
                }
            }
        }
        prevPerCoreJiffies_ = cores;
        havePrevPerCore_ = true;
    }

    // --- 35-36. Архитектура и BogoMIPS ---
    {
        struct utsname uts{};
        if (uname(&uts) == 0) info.cpuArchitecture = uts.machine;
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (startsWith(line, "bogomips")) {
                auto pos = line.find(':');
                if (pos != std::string::npos) {
                    info.cpuBogoMips = toDouble(trim(line.substr(pos + 1)));
                    break;
                }
            }
        }
    }

    // --- 37-43. Расширенные поля /proc/meminfo ---
    {
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            uint64_t value;
            iss >> key >> value;
            if (key == "Slab:") info.slabKB = value;
            else if (key == "PageTables:") info.pageTablesKB = value;
            else if (key == "Committed_AS:") info.committedAsKB = value;
            else if (key == "Active(anon):") info.activeAnonKB = value;
            else if (key == "Shmem:") info.shmemKB = value;
            else if (key == "Mapped:") info.mappedKB = value;
            else if (key == "KernelStack:") info.kernelStackKB = value;
        }
    }

    // --- 44, 47, 48, 45-46. /proc/diskstats: очередь, счётчик устройств, суммарные байты/время ---
    {
        std::ifstream diskstats("/proc/diskstats");
        std::string line;
        int inFlight = 0, deviceCount = 0;
        uint64_t readSectorsTotal = 0, writeSectorsTotal = 0, ioTimeMsTotal = 0;
        const auto wholeDevices = readWholeBlockDevices();
        while (std::getline(diskstats, line)) {
            auto f = splitWs(line);
            if (f.size() < 14) continue;
            const std::string& devName = f[2];
            if (!isWholeDiskEntry(wholeDevices, devName)) continue;

            ++deviceCount;
            readSectorsTotal += static_cast<uint64_t>(toLong(f[5]));
            writeSectorsTotal += static_cast<uint64_t>(toLong(f[9]));
            inFlight += static_cast<int>(toLong(f[11]));
            ioTimeMsTotal += static_cast<uint64_t>(toLong(f[12]));
        }
        info.diskIoInFlight = inFlight;
        info.blockDeviceCount = deviceCount;
        info.diskTotalReadBytesAllTime = readSectorsTotal * 512ULL;
        info.diskTotalWriteBytesAllTime = writeSectorsTotal * 512ULL;
        info.diskIoTimeMsAllTime = ioTimeMsTotal;
    }

    // --- 49. UDP-сокеты ---
    {
        int udpCount = 0;
        for (const char* path : {"/proc/net/udp", "/proc/net/udp6"}) {
            std::ifstream udp(path);
            std::string line;
            std::getline(udp, line); // заголовок
            while (std::getline(udp, line)) {
                if (!trim(line).empty()) ++udpCount;
            }
        }
        info.udpSocketCount = udpCount;
    }

    // --- 50-53. Сеть: суммарные байты и ошибки с момента загрузки ---
    {
        std::ifstream netdev("/proc/net/dev");
        std::string line;
        std::getline(netdev, line);
        std::getline(netdev, line);
        uint64_t rxBytes = 0, txBytes = 0, rxErr = 0, txErr = 0;
        while (std::getline(netdev, line)) {
            auto colonPos = line.find(':');
            if (colonPos == std::string::npos) continue;
            std::string iface = trim(line.substr(0, colonPos));
            if (iface == "lo") continue;
            auto f = splitWs(line.substr(colonPos + 1));
            if (f.size() < 12) continue;
            rxBytes += static_cast<uint64_t>(toLong(f[0]));
            rxErr += static_cast<uint64_t>(toLong(f[2])) + static_cast<uint64_t>(toLong(f[3]));
            txBytes += static_cast<uint64_t>(toLong(f[8]));
            txErr += static_cast<uint64_t>(toLong(f[10])) + static_cast<uint64_t>(toLong(f[11]));
        }
        info.netRxBytesAllTime = rxBytes;
        info.netTxBytesAllTime = txBytes;
        info.netRxErrorsAllTime = rxErr;
        info.netTxErrorsAllTime = txErr;
    }

    // --- 54. Шлюз по умолчанию (/proc/net/route) ---
    {
        std::ifstream route("/proc/net/route");
        std::string line;
        std::getline(route, line); // заголовок
        std::string gateway;
        while (std::getline(route, line)) {
            auto f = splitWs(line);
            if (f.size() < 3) continue;
            if (f[1] == "00000000") { // Destination 0.0.0.0 = маршрут по умолчанию
                // Именно безопасный парсинг: std::stoul на нечисловом поле
                // бросил бы std::invalid_argument и аварийно завершил sysflex.
                unsigned long gwHex = toULong(f[2], 0, 16);
                // Байты в /proc/net/route хранятся в little-endian
                std::ostringstream oss;
                oss << ((gwHex >> 0) & 0xFF) << "." << ((gwHex >> 8) & 0xFF) << "."
                    << ((gwHex >> 16) & 0xFF) << "." << ((gwHex >> 24) & 0xFF);
                gateway = oss.str();
                break;
            }
        }
        info.defaultGateway = gateway.empty() ? "н/д" : gateway;
    }

    // --- 55-56. Суммарные потоки и уникальные пользователи процессов ---
    {
        DIR* dir = opendir("/proc");
        int threadTotal = 0;
        std::vector<std::string> uids;
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string name = entry->d_name;
                if (name.empty() || !std::isdigit(static_cast<unsigned char>(name[0]))) continue;
                std::string status = readFile("/proc/" + name + "/status");
                if (status.empty()) continue;
                std::istringstream iss(status);
                std::string line;
                while (std::getline(iss, line)) {
                    if (startsWith(line, "Threads:")) {
                        threadTotal += static_cast<int>(toLong(trim(line.substr(8))));
                    } else if (startsWith(line, "Uid:")) {
                        auto fields = splitWs(line);
                        if (fields.size() >= 2 && std::find(uids.begin(), uids.end(), fields[1]) == uids.end()) {
                            uids.push_back(fields[1]);
                        }
                    }
                }
            }
            closedir(dir);
        }
        info.totalThreadCount = threadTotal;
        info.uniqueProcessUsers = static_cast<int>(uids.size());
    }

    // --- 57. Загруженные модули ядра ---
    {
        std::ifstream modules("/proc/modules");
        std::string line;
        int count = 0;
        while (std::getline(modules, line)) {
            if (!trim(line).empty()) ++count;
        }
        info.kernelModuleCount = count;
    }

    // --- 58. Источник системных часов ---
    info.clockSource = readFirstLine("/sys/devices/system/clocksource/clocksource0/current_clocksource");
    if (info.clockSource.empty()) info.clockSource = "н/д";

    // --- 59. Init-система (имя процесса с PID 1) ---
    {
        std::string comm = trim(readFile("/proc/1/comm"));
        info.initSystemName = comm.empty() ? "н/д" : comm;
    }

    // --- 60. Параметры загрузки ядра (кратко) ---
    {
        std::string cmdline = trim(readFile("/proc/cmdline"));
        if (cmdline.size() > 80) cmdline = cmdline.substr(0, 77) + "...";
        info.kernelCmdline = cmdline.empty() ? "н/д" : cmdline;
    }
}

} // namespace sysflex
