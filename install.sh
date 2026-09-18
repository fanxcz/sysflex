#!/usr/bin/env bash
# install.sh — сборка и установка sysflex в систему.
# Порядок выбора инструмента сборки: CMake -> make -> g++ напрямую.
# Тестировано на Debian/Ubuntu, но должно работать на любом Linux
# с компилятором, поддерживающим C++17.
#
# Переменные окружения:
#   PREFIX=/usr/local   каталог установки (по умолчанию /usr/local)
#   RUN_TESTS=1         прогнать модульные тесты перед установкой

set -euo pipefail

# --- Цвета для вывода (отключаются, если вывод не в терминал) ---
if [ -t 1 ]; then
    C_GREEN='\033[32m'; C_YELLOW='\033[33m'; C_RED='\033[31m'; C_RESET='\033[0m'
else
    C_GREEN=''; C_YELLOW=''; C_RED=''; C_RESET=''
fi

info()  { echo -e "${C_GREEN}[install]${C_RESET} $1"; }
warn()  { echo -e "${C_YELLOW}[install]${C_RESET} $1"; }
error() { echo -e "${C_RED}[install]${C_RESET} $1" >&2; }

# Выполняет команду с правами root только тогда, когда это действительно нужно
as_root() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        error "нужны права root, но sudo не найден. Запустите скрипт от root."
        exit 1
    fi
}

PREFIX="${PREFIX:-/usr/local}"
RUN_TESTS="${RUN_TESTS:-0}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
JOBS="$(nproc 2>/dev/null || echo 2)"

info "Каталог установки: $PREFIX"
cd "$SCRIPT_DIR"

# --- Проверка компилятора ---
if ! command -v g++ >/dev/null 2>&1 && ! command -v c++ >/dev/null 2>&1; then
    error "g++ не найден. Установите его командой: sudo apt install g++ build-essential"
    exit 1
fi

CXX_BIN="$(command -v g++ || command -v c++)"
GCC_VERSION="$("$CXX_BIN" -dumpversion 2>/dev/null | cut -d. -f1 || echo 0)"
if [ "$GCC_VERSION" -lt 8 ]; then
    warn "Обнаружена старая версия компилятора ($GCC_VERSION). Рекомендуется >= 8 для полной поддержки C++17."
fi

# --- Опциональные тесты перед установкой ---
if [ "$RUN_TESTS" = "1" ]; then
    info "Запускаем модульные тесты..."
    if command -v cmake >/dev/null 2>&1; then
        # Отдельный каталог сборки: основной build/ ниже конфигурируется с
        # -DSYSFLEX_BUILD_TESTS=OFF, и цель sysflex_tests в нём отсутствует.
        TEST_BUILD_DIR="$SCRIPT_DIR/build-tests"
        cmake -S "$SCRIPT_DIR" -B "$TEST_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
              -DSYSFLEX_BUILD_TESTS=ON >/dev/null
        cmake --build "$TEST_BUILD_DIR" -j"$JOBS" --target sysflex_tests >/dev/null
        (cd "$TEST_BUILD_DIR" && ctest --output-on-failure)
    else
        make test
    fi
fi

# --- Сборка: CMake -> make -> g++ ---
if command -v cmake >/dev/null 2>&1; then
    info "Обнаружен CMake, собираем через него..."
    BUILD_DIR="$SCRIPT_DIR/build"
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DSYSFLEX_BUILD_TESTS=OFF
    cmake --build "$BUILD_DIR" -j"$JOBS"
    BINARY_PATH="$BUILD_DIR/sysflex"
elif command -v make >/dev/null 2>&1; then
    info "CMake не найден, собираем через make..."
    make
    BINARY_PATH="$SCRIPT_DIR/sysflex"
else
    warn "Ни CMake, ни make не найдены, собираем напрямую компилятором..."
    "$CXX_BIN" -std=c++17 -O2 -Wall -Wextra -Iinclude -pthread \
        src/system_info.cpp src/display.cpp src/sysflex.cpp src/main.cpp \
        -o "$SCRIPT_DIR/sysflex"
    BINARY_PATH="$SCRIPT_DIR/sysflex"
fi

if [ ! -f "$BINARY_PATH" ]; then
    error "Сборка не удалась: бинарный файл не найден по пути $BINARY_PATH"
    exit 1
fi

# --- Установка бинарного файла, плагинов и справочной страницы ---
info "Устанавливаем sysflex в $PREFIX/bin..."
as_root mkdir -p "$PREFIX/bin"
as_root install -m 0755 "$BINARY_PATH" "$PREFIX/bin/sysflex"

info "Устанавливаем плагины в $PREFIX/share/sysflex/plugins..."
as_root mkdir -p "$PREFIX/share/sysflex/plugins"
for plugin in "$SCRIPT_DIR"/plugins/*.sh; do
    [ -e "$plugin" ] || continue
    as_root install -m 0755 "$plugin" "$PREFIX/share/sysflex/plugins/"
done

if [ -f "$SCRIPT_DIR/man/sysflex.1" ]; then
    info "Устанавливаем справочную страницу в $PREFIX/share/man/man1..."
    as_root mkdir -p "$PREFIX/share/man/man1"
    as_root install -m 0644 "$SCRIPT_DIR/man/sysflex.1" "$PREFIX/share/man/man1/sysflex.1"
fi

info "Готово! sysflex установлен в $PREFIX/bin/sysflex"
info "Запустите 'sysflex --help' чтобы увидеть доступные опции."

if ! echo "$PATH" | tr ':' '\n' | grep -qx "$PREFIX/bin"; then
    warn "$PREFIX/bin отсутствует в PATH. Добавьте его в ~/.bashrc или ~/.profile:"
    warn "  export PATH=\"$PREFIX/bin:\$PATH\""
fi
