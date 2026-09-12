#!/usr/bin/env bash
# install.sh — сборка и установка sysflex в систему.
# Поддерживает установку через CMake (предпочтительно) или прямую сборку g++,
# если CMake недоступен. Тестировано на Debian/Ubuntu, но должно работать
# на любом Linux с g++ (или другим компилятором с поддержкой C++17).

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

PREFIX="${PREFIX:-/usr/local}"
BIN_DIR="$PREFIX/bin"
PLUGIN_DIR="$PREFIX/share/sysflex/plugins"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

info "Каталог установки: $PREFIX"

# --- Проверка компилятора ---
if ! command -v g++ >/dev/null 2>&1; then
    error "g++ не найден. Установите его командой: sudo apt install g++ build-essential"
    exit 1
fi

GCC_VERSION=$(g++ -dumpversion | cut -d. -f1)
if [ "$GCC_VERSION" -lt 8 ]; then
    warn "Обнаружена старая версия g++ ($GCC_VERSION). Рекомендуется g++ >= 8 для полной поддержки C++17."
fi

cd "$SCRIPT_DIR"

# --- Сборка через CMake, если доступен, иначе напрямую через g++ ---
if command -v cmake >/dev/null 2>&1; then
    info "Обнаружен CMake, собираем через него..."
    BUILD_DIR="$SCRIPT_DIR/build"
    mkdir -p "$BUILD_DIR"
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DSYSFLEX_BUILD_TESTS=OFF
    cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 2)"
    BINARY_PATH="$BUILD_DIR/sysflex"
else
    warn "CMake не найден, собираем напрямую через g++..."
    g++ -std=c++17 -O2 -Wall -Wextra -Iinclude -pthread \
        src/system_info.cpp src/display.cpp src/sysflex.cpp src/main.cpp \
        -o "$SCRIPT_DIR/sysflex"
    BINARY_PATH="$SCRIPT_DIR/sysflex"
fi

if [ ! -f "$BINARY_PATH" ]; then
    error "Сборка не удалась: бинарный файл не найден по пути $BINARY_PATH"
    exit 1
fi

# --- Установка бинарного файла ---
info "Устанавливаем бинарный файл в $BIN_DIR..."
if [ -w "$BIN_DIR" ] || [ "$(id -u)" -eq 0 ]; then
    mkdir -p "$BIN_DIR"
    cp "$BINARY_PATH" "$BIN_DIR/sysflex"
    chmod 755 "$BIN_DIR/sysflex"
else
    info "Требуются права root для записи в $BIN_DIR, используем sudo..."
    sudo mkdir -p "$BIN_DIR"
    sudo cp "$BINARY_PATH" "$BIN_DIR/sysflex"
    sudo chmod 755 "$BIN_DIR/sysflex"
fi

# --- Установка плагинов ---
info "Устанавливаем плагины в $PLUGIN_DIR..."
if [ -w "$PREFIX/share" ] 2>/dev/null || [ "$(id -u)" -eq 0 ]; then
    mkdir -p "$PLUGIN_DIR"
    cp -r "$SCRIPT_DIR"/plugins/*.sh "$PLUGIN_DIR/" 2>/dev/null || true
    chmod +x "$PLUGIN_DIR"/*.sh 2>/dev/null || true
else
    sudo mkdir -p "$PLUGIN_DIR"
    sudo cp -r "$SCRIPT_DIR"/plugins/*.sh "$PLUGIN_DIR/" 2>/dev/null || true
    sudo chmod +x "$PLUGIN_DIR"/*.sh 2>/dev/null || true
fi

info "Готово! sysflex установлен в $BIN_DIR/sysflex"
info "Запустите 'sysflex --help' чтобы увидеть доступные опции."

if ! echo "$PATH" | tr ':' '\n' | grep -qx "$BIN_DIR"; then
    warn "$BIN_DIR отсутствует в PATH. Добавьте его в ~/.bashrc или ~/.profile:"
    warn "  export PATH=\"$BIN_DIR:\$PATH\""
fi
