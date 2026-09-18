// system_info.hpp — структуры данных и класс SystemMonitor для сбора
// всей информации о состоянии системы (CPU, RAM, диск, сеть, GPU и т.д.)
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <unordered_map>

namespace sysflex {

// Информация об одном процессе (для списка top-5 процессов)
struct ProcessInfo {
    int pid = 0;
    std::string name;
    double cpuPercent = 0.0;
    double memPercent = 0.0;
};

// Полный снимок состояния системы на текущий момент времени.
// Заполняется классом SystemMonitor::update().
struct SystemInfo {
    // --- 1. CPU ---
    double cpuUsagePercent = 0.0;          // Общая загрузка CPU в процентах
    double cpuFrequencyMHz = 0.0;          // Текущая частота CPU (МГц)
    double cpuTemperatureC = -1.0;         // Температура CPU в градусах Цельсия (-1, если недоступна)
    std::vector<double> loadAverage{0, 0, 0}; // Load average за 1/5/15 минут
    std::vector<double> perCoreTempsC;     // Температуры по каждому ядру (если доступны)
    int cpuCoreCount = 0;                  // Количество логических ядер

    // --- 2. RAM ---
    uint64_t ramTotalKB = 0;
    uint64_t ramUsedKB = 0;
    double ramUsagePercent = 0.0;

    // --- 3. Disk ---
    uint64_t diskTotalBytes = 0;
    uint64_t diskUsedBytes = 0;
    double diskUsagePercent = 0.0;
    double diskReadBytesPerSec = 0.0;
    double diskWriteBytesPerSec = 0.0;

    // --- 4. Network ---
    double netUploadBytesPerSec = 0.0;
    double netDownloadBytesPerSec = 0.0;
    std::string netInterface;              // Имя основного сетевого интерфейса

    // --- 5. GPU ---
    std::string gpuModel = "Не обнаружено";
    double gpuTemperatureC = -1.0;
    double gpuMemUsedMB = -1.0;
    double gpuMemTotalMB = -1.0;

    // --- 6. Battery ---
    bool hasBattery = false;
    int batteryPercent = -1;
    std::string batteryStatus;             // Charging / Discharging / Full / Unknown

    // --- 7. Swap ---
    uint64_t swapTotalKB = 0;
    uint64_t swapUsedKB = 0;
    double swapUsagePercent = 0.0;

    // --- 8. Uptime ---
    long uptimeSeconds = 0;

    // --- 9. Processes ---
    int processCount = 0;
    std::vector<ProcessInfo> topProcesses;

    // --- 10. Shell/terminal ---
    std::string shellName;
    std::string terminalName;

    // --- 11-13. OS/Kernel/Hostname ---
    std::string osName;
    std::string osPrettyName;
    std::string kernelVersion;
    std::string hostname;

    // --- 14. Docker ---
    int dockerContainerCount = -1;         // -1 = docker не установлен/недоступен

    // --- 15. Git ---
    std::string gitBranch;                 // Пусто, если не в git-репозитории

    // --- 16. Music player ---
    std::string musicStatus;               // "Artist - Title" или пусто

    // --- 17. USB ---
    int usbDeviceCount = 0;
    std::vector<std::string> usbDevices;

    // --- 18. Bluetooth ---
    int bluetoothDeviceCount = 0;
    std::vector<std::string> bluetoothDevices;

    // --- 19. Inode usage ---
    double inodeUsagePercent = 0.0;

    // --- 20. Logged users ---
    int loggedUsersCount = 0;

    // --- 21. Current time ---
    std::string currentTime;

    // --- 22. Health score ---
    int healthScore = 100;

    // ================================================================
    // Расширенные метрики: CPU, память, диск, сеть, датчики, окружение.
    // Всё считается из реальных /proc и /sys на каждом снимке, поэтому
    // в живом режиме значения обновляются каждую секунду по-настоящему,
    // а не подставляются один раз при старте.
    // ================================================================

    // --- CPU extras (1-5) ---
    std::string cpuModelName;              // 1. Модель CPU (/proc/cpuinfo)
    uint64_t cpuCacheSizeKB = 0;            // 2. Размер кэша CPU (КБ)
    std::string cpuGovernor;                // 3. Governor текущего масштабирования частоты
    double ctxSwitchesPerSec = 0.0;         // 4. Переключений контекста в секунду
    double interruptsPerSec = 0.0;          // 5. Прерываний в секунду

    // --- Processes extras (6-9) ---
    double forksPerSec = 0.0;               // 6. Новых процессов (форков) в секунду
    int procsRunning = 0;                   // 7. Процессов в состоянии running
    int procsBlocked = 0;                   // 8. Процессов, заблокированных на I/O
    int zombieCount = 0;                    // 9. Процессов-зомби

    // --- System extras (10) ---
    int entropyAvailable = -1;              // 10. Доступная энтропия ядра (бит)

    // --- Memory extras (11-16) ---
    uint64_t memAvailableKB = 0;            // 11. Реально доступная память (MemAvailable)
    uint64_t buffersKB = 0;                 // 12. Буферы ядра
    uint64_t cachedKB = 0;                  // 13. Файловый кэш
    uint64_t dirtyKB = 0;                   // 14. "Грязные" страницы, ждущие записи на диск
    uint64_t hugePagesTotal = 0;            // 15. Всего HugePages
    uint64_t hugePagesFree = 0;             // 16. Свободных HugePages

    // --- Disk extras (17-20) ---
    double diskReadOpsPerSec = 0.0;         // 17. Операций чтения диска в секунду (IOPS)
    double diskWriteOpsPerSec = 0.0;        // 18. Операций записи диска в секунду (IOPS)
    std::string rootFilesystemType;         // 19. Тип ФС корневого раздела (ext4/btrfs/...)
    int mountPointCount = 0;                // 20. Количество смонтированных реальных ФС

    // --- Network extras (21-24) ---
    int tcpEstablishedCount = 0;            // 21. Активных TCP-соединений (ESTABLISHED)
    int tcpListenCount = 0;                 // 22. TCP-портов в состоянии LISTEN
    int networkInterfaceCount = 0;          // 23. Сетевых интерфейсов (кроме lo)
    std::string dnsServers;                 // 24. DNS-серверы из /etc/resolv.conf

    // --- Sensors extras (25-26) ---
    int fanSpeedRPM = -1;                   // 25. Скорость вентилятора (об/мин), -1 если нет
    double maxSensorTempC = -1.0;           // 26. Максимальная температура среди всех датчиков

    // --- Misc extras (27-30) ---
    uint64_t openFileDescriptors = 0;       // 27. Открытых файловых дескрипторов в системе
    uint64_t maxFileDescriptors = 0;        // 28. Системный лимит файловых дескрипторов
    std::string systemLocale;               // 29. Текущая локаль (LANG)
    std::string systemTimezone;             // 30. Часовой пояс системы

    // ================================================================
    // Диагностика ядра и подсистем: планировщик, файловые системы,
    // сеть на уровне сокетов, окружение выполнения. Тоже читается
    // напрямую из /proc и /sys на каждом снимке.
    // ================================================================

    // --- CPU (31-36) ---
    double cpuIowaitPercent = 0.0;          // 31. Доля времени CPU в ожидании I/O
    double cpuStealPercent = 0.0;           // 32. Доля времени, украденного гипервизором (steal)
    double cpuSoftirqPercent = 0.0;         // 33. Доля времени на softirq
    std::vector<double> perCoreUsagePercent; // 34. Загрузка по каждому ядру отдельно (%)
    std::string cpuArchitecture;            // 35. Архитектура (x86_64/aarch64/...)
    double cpuBogoMips = 0.0;               // 36. BogoMIPS первого ядра

    // --- Память (37-43) ---
    uint64_t slabKB = 0;                    // 37. Память ядра под slab-аллокатор
    uint64_t pageTablesKB = 0;              // 38. Память под таблицы страниц
    uint64_t committedAsKB = 0;             // 39. Committed_AS (общий "обещанный" объём памяти)
    uint64_t activeAnonKB = 0;              // 40. Активная анонимная память (не файловый кэш)
    uint64_t shmemKB = 0;                   // 41. Разделяемая память (tmpfs/shm)
    uint64_t mappedKB = 0;                  // 42. Замапленная память (mmap файлов/библиотек)
    uint64_t kernelStackKB = 0;             // 43. Память под стеки ядра

    // --- Диск (44-48) ---
    int diskIoInFlight = 0;                 // 44. Операций диска в очереди прямо сейчас
    uint64_t diskTotalReadBytesAllTime = 0; // 45. Всего прочитано с диска с момента загрузки
    uint64_t diskTotalWriteBytesAllTime = 0;// 46. Всего записано на диск с момента загрузки
    int blockDeviceCount = 0;               // 47. Количество обнаруженных блочных устройств
    uint64_t diskIoTimeMsAllTime = 0;       // 48. Суммарное время работы диска (мс) с загрузки

    // --- Сеть (49-54) ---
    int udpSocketCount = 0;                 // 49. Открытых UDP-сокетов
    uint64_t netRxBytesAllTime = 0;         // 50. Всего принято байт с момента загрузки
    uint64_t netTxBytesAllTime = 0;         // 51. Всего отправлено байт с момента загрузки
    uint64_t netRxErrorsAllTime = 0;        // 52. Ошибок/потерь при приёме
    uint64_t netTxErrorsAllTime = 0;        // 53. Ошибок/потерь при отправке
    std::string defaultGateway;             // 54. IP основного шлюза

    // --- Процессы / система (55-30 = 60) ---
    int totalThreadCount = 0;               // 55. Суммарное число потоков всех процессов
    int uniqueProcessUsers = 0;             // 56. Число разных пользователей, владеющих процессами
    int kernelModuleCount = 0;              // 57. Загруженных модулей ядра
    std::string clockSource;                // 58. Текущий источник системных часов
    std::string initSystemName;             // 59. Имя процесса с PID 1 (init/systemd/...)
    std::string kernelCmdline;              // 60. Параметры загрузки ядра (кратко)

    // Время, когда был сделан этот снимок (для расчёта дельт скоростей)
    std::chrono::steady_clock::time_point timestamp;
};

// Класс-монитор, инкапсулирующий логику чтения /proc, /sys и вызовов внешних
// утилит. Хранит предыдущие значения счётчиков (сеть, диск, CPU jiffies)
// для расчёта мгновенных скоростей между двумя вызовами update().
class SystemMonitor {
public:
    // topProcessCount задаёт, сколько процессов попадёт в topProcesses
    // (настраивается опцией --top / конфиг-файлом), по умолчанию 5.
    explicit SystemMonitor(int topProcessCount = 5);

    // Обновляет и возвращает актуальный снимок состояния системы.
    // При первом вызове скорости (сеть/диск/CPU%) будут равны 0, так как
    // для расчёта дельты требуется минимум два замера.
    SystemInfo update();

    // Задаёт, как часто (в секундах) перечитывать «медленные» данные,
    // требующие запуска внешних утилит: GPU (nvidia-smi/lspci), docker,
    // lsusb, bluetoothctl, playerctl, git и `who`. Всё остальное читается
    // из /proc и /sys и обновляется на каждом снимке. Значение <= 0
    // отключает кэш (данные перечитываются каждый кадр).
    void setSlowRefreshSeconds(double seconds) { slowRefreshSec_ = seconds; }
    [[nodiscard]] double slowRefreshSeconds() const { return slowRefreshSec_; }

private:
    // --- Внутренние методы сбора отдельных категорий данных ---
    void collectCpu(SystemInfo& info);
    void collectMemory(SystemInfo& info);
    void collectDisk(SystemInfo& info);
    void collectNetwork(SystemInfo& info);
    void collectGpu(SystemInfo& info);
    void collectBattery(SystemInfo& info);
    void collectSwap(SystemInfo& info);
    void collectUptime(SystemInfo& info);
    void collectProcesses(SystemInfo& info);
    void collectShellTerminal(SystemInfo& info);
    void collectOsKernelHost(SystemInfo& info);
    void collectDocker(SystemInfo& info);
    void collectGit(SystemInfo& info);
    void collectMusic(SystemInfo& info);
    void collectUsb(SystemInfo& info);
    void collectBluetooth(SystemInfo& info);
    void collectInodes(SystemInfo& info);
    void collectUsers(SystemInfo& info);
    void collectTime(SystemInfo& info);
    void computeHealthScore(SystemInfo& info);
    // Сбор 30 дополнительных метрик (v1.2) — см. комментарий в SystemInfo.
    void collectExtras(SystemInfo& info);
    // Сбор второго пакета из 30 метрик (v1.3) — см. комментарий в SystemInfo.
    void collectExtras2(SystemInfo& info);

    // --- Работа с кэшем «медленных» (внешних) данных ---
    // collectGpu/collectDocker/collectGit/collectMusic/collectUsb/
    // collectBluetooth/collectUsers вызывают внешние программы, поэтому
    // выполняются не чаще slowRefreshSec_ раз в секунду; в остальных кадрах
    // значения берутся из slow_ без единого fork/exec.
    [[nodiscard]] bool slowCacheStale(const SystemInfo& info) const;
    void refreshSlowCache(SystemInfo& info);
    void applySlowCache(SystemInfo& info) const;

    // --- Состояние между вызовами (для расчёта дельт) ---
    struct CpuJiffies {
        long long user = 0, nice = 0, system = 0, idle = 0,
                   iowait = 0, irq = 0, softirq = 0, steal = 0;
        [[nodiscard]] long long total() const {
            return user + nice + system + idle + iowait + irq + softirq + steal;
        }
        [[nodiscard]] long long idleAll() const { return idle + iowait; }
    };
    CpuJiffies prevJiffies_;
    bool havePrevJiffies_ = false;

    uint64_t prevNetRxBytes_ = 0;
    uint64_t prevNetTxBytes_ = 0;
    bool havePrevNet_ = false;

    uint64_t prevDiskReadSectors_ = 0;
    uint64_t prevDiskWriteSectors_ = 0;
    bool havePrevDisk_ = false;

    std::chrono::steady_clock::time_point prevTimestamp_;
    bool havePrevTimestamp_ = false;

    // --- Состояние для дельт новых метрик (v1.2) ---
    long long prevCtxt_ = 0;
    long long prevIntr_ = 0;
    long long prevProcessesCreated_ = 0;
    uint64_t prevDiskReadOps_ = 0;
    uint64_t prevDiskWriteOps_ = 0;
    bool havePrevExtras_ = false;

    // --- Состояние для дельт per-core загрузки (v1.3) ---
    std::vector<CpuJiffies> prevPerCoreJiffies_;
    bool havePrevPerCore_ = false;

    int topProcessCount_ = 5;

    // --- Кэш данных, требующих запуска внешних утилит ---
    struct SlowCache {
        bool filled = false;
        std::chrono::steady_clock::time_point fetchedAt{};
        std::string gpuModel;
        double gpuTemperatureC = -1.0;
        double gpuMemUsedMB = -1.0;
        double gpuMemTotalMB = -1.0;
        int dockerContainerCount = -1;
        std::string gitBranch;
        std::string musicStatus;
        std::vector<std::string> usbDevices;
        std::vector<std::string> bluetoothDevices;
        int loggedUsersCount = 0;
    } slow_;
    double slowRefreshSec_ = 5.0;

    // --- Предыдущие значения utime+stime по PID ---
    // Нужны, чтобы считать мгновенную загрузку процесса между двумя кадрами,
    // а не среднюю за всё время его жизни.
    std::unordered_map<int, long long> prevProcTicks_;
};

} // namespace sysflex
