# sysflex

Гибкий системный монитор для Linux, написанный на C++17 без внешних зависимостей.

По умолчанию sysflex работает как живой дашборд: показатели CPU, памяти,
диска и сети обновляются каждую секунду, а сам процесс работает до тех пор,
пока вы сами не остановите его сочетанием `Ctrl+C`. Для разового снимка
(например, в скриптах) используйте флаг `--once`.

## Установка (Debian / Ubuntu, включая Debian 13)

```bash
sudo apt update
sudo apt install -y build-essential cmake git
git clone https://github.com/fanxcz/sysflex.git
cd sysflex
./install.sh
```

Скрипт соберёт проект и установит бинарник в `/usr/local/bin/sysflex`.

### Сборка без установки в систему

```bash
g++ -std=c++17 -O2 -Iinclude -pthread \
    src/system_info.cpp src/display.cpp src/sysflex.cpp src/main.cpp \
    -o sysflex
./sysflex
```

### Удаление

```bash
./uninstall.sh
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
sysflex --interval 2 --history 60   # Реже обновлять, длинная история спарклайнов
sysflex --theme dracula         # Другая тема оформления
sysflex --top 10                # Показать топ-10 процессов вместо 5
sysflex --timeout 1             # Таймаут для внешних команд (docker/bluetoothctl/…), сек
sysflex --no-color              # Без ANSI-цветов (для логов/пайпов)
sysflex --no-plugins            # Не запускать плагины
sysflex --no-ascii              # Без ASCII-арт логотипа (для узких терминалов/логов)
```

> Флаги `--short`, `--json`, `--game` и `--bench` всегда разовые — им не
> нужен `--once`. Флаг `--once` нужен, только если вы хотите разовый
> полный вывод (то, что раньше было поведением по умолчанию).

Полный список опций: `sysflex --help`

### Конфиг-файл

Настройки по умолчанию можно один раз задать в `~/.config/sysflex/config`
(или в файле по пути из переменной `$SYSFLEX_CONFIG`) в формате `key=value`:

```ini
theme=dracula
top=10
timeout=2
no_ascii=true
```

Аргументы командной строки всегда имеют приоритет над конфиг-файлом.

## Темы оформления

`default`, `dracula`, `nord`, `catppuccin`, `gruvbox`, `solarized`, `tokyonight`

```bash
sysflex --theme catppuccin
```

## Плагины

sysflex запускает все исполняемые скрипты из каталога `plugins/` и выводит их
результат отдельным блоком (кроме `--no-plugins`).

В комплекте: `uptime.sh`, `crypto.sh` (курс BTC/USD), `weather.sh` (погода через wttr.in).

Свой плагин — это просто исполняемый файл:

```bash
#!/usr/bin/env bash
echo "Привет из моего плагина!"
```

```bash
chmod +x plugins/my_plugin.sh
```

## Требования

- Linux
- Компилятор с поддержкой C++17 (GCC ≥ 8, Clang ≥ 7)
- CMake ≥ 3.16 (опционально — можно собрать напрямую через g++)

Опциональные утилиты для расширенного функционала (если их нет — соответствующие
поля просто не заполняются): `nvidia-smi`, `lspci`, `docker`, `git`, `playerctl`,
`lsusb`, `bluetoothctl`.

Все вызовы внешних команд ограничены таймаутом (по умолчанию 3 сек, настраивается
через `--timeout`), поэтому недоступный `docker`/`bluetoothctl` больше не вешает
запуск навсегда.

## Лицензия

[MIT](LICENSE)
