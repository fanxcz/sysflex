# sysflex

[![CI](https://github.com/fanxcz/sysflex/actions/workflows/ci.yml/badge.svg)](https://github.com/fanxcz/sysflex/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

Гибкий системный монитор для Linux, написанный на C++17 без внешних зависимостей.

По умолчанию sysflex работает как живой дашборд: показатели CPU, памяти,
диска и сети обновляются каждую секунду, а сам процесс работает до тех пор,
пока вы сами не остановите его сочетанием `Ctrl+C`. Для разового снимка
(например, в скриптах) используйте флаг `--once`.

Все базовые метрики читаются напрямую из `/proc` и `/sys`. Внешние утилиты
(`nvidia-smi`, `docker`, `lsusb`, `bluetoothctl`, `playerctl`, `git`) вызываются
не чаще одного раза в 5 секунд — интервал настраивается через `--slow-refresh`.

## Установка (Debian / Ubuntu, включая Debian 13)

```bash
sudo apt update
sudo apt install -y build-essential cmake git
git clone https://github.com/fanxcz/sysflex.git
cd sysflex
./install.sh
```

Скрипт соберёт проект и установит:

- бинарник — в `/usr/local/bin/sysflex`;
- плагины — в `/usr/local/share/sysflex/plugins`;
- справочную страницу — `man sysflex`.

Переменные окружения `install.sh`:

```bash
PREFIX="$HOME/.local" ./install.sh   # установить без прав root
RUN_TESTS=1 ./install.sh             # прогнать модульные тесты перед установкой
```

### Сборка без установки в систему

Через `make` (нужен только компилятор):

```bash
make            # бинарник ./sysflex
make test       # собрать и запустить модульные тесты
make debug      # сборка с AddressSanitizer/UBSan и тестами
make install PREFIX="$HOME/.local"
```

Через CMake:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
(cd build && ctest --output-on-failure)
./build/sysflex
```

Или одной командой компилятора — без `make` и CMake:

```bash
g++ -std=c++17 -O2 -Iinclude -pthread \
    src/system_info.cpp src/display.cpp src/sysflex.cpp src/main.cpp \
    -o sysflex
./sysflex
```

### Удаление

```bash
./uninstall.sh          # или: PREFIX="$HOME/.local" ./uninstall.sh
```

## Использование

```bash
sysflex                         # Живой дашборд, обновляется каждую секунду
                                 # (работает, пока вы сами не нажмёте Ctrl+C)
sysflex --once                  # Разовый вывод полной информации и выход
sysflex --once --short          # Компактный однострочный вывод
sysflex --once --json           # JSON для парсинга скриптами
sysflex --once --game           # Игровой режим (фокус на GPU/CPU)
sysflex --bench                 # Встроенный бенчмарк системы
sysflex --list-themes           # Список доступных тем
sysflex --interval 2 --history 60   # Реже обновлять, длинная история спарклайнов
sysflex --theme dracula         # Другая тема оформления
sysflex --top 10                # Показать топ-10 процессов вместо 5
sysflex --timeout 1             # Таймаут для внешних команд (docker/bluetoothctl/…), сек
sysflex --slow-refresh 10       # Реже опрашивать внешние утилиты (GPU, docker, …)
sysflex --config ./my.cfg       # Прочитать настройки из указанного файла
sysflex --no-color              # Без ANSI-цветов (для логов/пайпов)
sysflex --no-plugins            # Не запускать плагины
sysflex --no-ascii              # Без ASCII-арт логотипа (для узких терминалов/логов)
```

> Флаги `--short`, `--json`, `--game` и `--bench` всегда разовые — им не
> нужен `--once`. Флаг `--once` нужен, только если вы хотите разовый
> полный вывод (то, что раньше было поведением по умолчанию).

Полный список опций: `sysflex --help` или `man sysflex`.

### Живой режим и перенаправление вывода

В живом режиме sysflex проверяет, является ли stdout терминалом. При выводе
в файл или конвейер (`sysflex > log.txt`, `sysflex | tee log`) экран не
очищается и не используются escape-коды — кадры идут подряд, как обычный лог.
Курсор прячется только в интерактивном терминале и восстанавливается при выходе,
в том числе по `SIGTERM`.

### JSON-вывод

`sysflex --once --json` всегда печатает валидный документ:

- строки экранируются, некорректный UTF-8 заменяется на `U+FFFD`;
- значения, недопустимые в JSON (`NaN`, `Infinity`), заменяются на `0`;
- в документ добавлены поля `version` и `generated_at`.

```bash
sysflex --once --json | python3 -m json.tool     # проверить и распечатать
sysflex --once --json | jq '.cpu.usage_percent'  # конкретное поле
```

### Конфиг-файл

Настройки по умолчанию можно один раз задать в `~/.config/sysflex/config`
(или в файле из `$SYSFLEX_CONFIG`, или в файле из `--config`) в формате
`key=value`:

```ini
theme=dracula
top=10
timeout=2
slow_refresh=10
no_ascii=true
```

Поддерживаемые ключи: `theme`, `interval`, `history`, `top`, `timeout`,
`slow_refresh`, `no_color`, `no_plugins`, `no_ascii`.
Аргументы командной строки всегда имеют приоритет над конфиг-файлом.
Неизвестная тема не игнорируется молча: sysflex предупредит в stderr и
использует `default`.

## Темы оформления

`default`, `dracula`, `nord`, `catppuccin`, `gruvbox`, `solarized`, `tokyonight`

```bash
sysflex --list-themes      # посмотреть список
sysflex --theme catppuccin # применить
```

## Плагины

sysflex запускает все исполняемые скрипты из каталогов плагинов и выводит их
результат отдельным блоком (кроме `--no-plugins`). Каталоги просматриваются
в порядке приоритета:

1. `$SYSFLEX_PLUGINS_DIR`
2. `./plugins`
3. `/usr/local/share/sysflex/plugins`
4. `/usr/share/sysflex/plugins`
5. `~/.sysflex/plugins`
6. `~/.config/sysflex/plugins`

Плагины с одинаковыми именами запускаются один раз — из каталога с наивысшим
приоритетом. Каталог плагинов перебирается системными вызовами, без запуска
`find` через shell.

В комплекте: `uptime.sh`, `crypto.sh` (курс BTC/USD), `weather.sh` (погода через wttr.in).

Свой плагин — это просто исполняемый файл:

```bash
#!/usr/bin/env bash
echo "Привет из моего плагина!"
```

```bash
chmod +x plugins/my_plugin.sh
```

## Разработка

```bash
make test                                   # модульные тесты (150 проверок)
cmake -S . -B build -DSYSFLEX_WERROR=ON     # сборка с -Werror, как в CI
(cd build && ctest --output-on-failure)
```

CI (GitHub Actions) собирает проект матрицей **GCC/Clang × Release/Debug**
с `-Werror`, прогоняет ctest, дымовые тесты всех режимов, проверку валидности
JSON штатным парсером Python и `shellcheck` для скриптов и плагинов.

## Требования

- Linux
- Компилятор с поддержкой C++17 (GCC ≥ 8, Clang ≥ 7)
- `make` или CMake ≥ 3.16 (опционально — можно собрать напрямую через g++)

Опциональные утилиты для расширенного функционала (если их нет — соответствующие
поля просто не заполняются): `nvidia-smi`, `lspci`, `docker`, `git`, `playerctl`,
`lsusb`, `bluetoothctl`.

Все вызовы внешних команд ограничены таймаутом (по умолчанию 3 сек, настраивается
через `--timeout`), поэтому недоступный `docker`/`bluetoothctl` не вешает
запуск навсегда. Результат проверки наличия утилиты в `PATH` кэшируется,
а сами утилиты опрашиваются не чаще `--slow-refresh` секунд.

## Лицензия

[MIT](LICENSE)
