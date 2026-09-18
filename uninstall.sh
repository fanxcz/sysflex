#!/usr/bin/env bash
# uninstall.sh — удаление sysflex из системы, установленного через install.sh
set -euo pipefail

if [ -t 1 ]; then
    C_GREEN='\033[32m'; C_RED='\033[31m'; C_RESET='\033[0m'
else
    C_GREEN=''; C_RED=''; C_RESET=''
fi

info()  { echo -e "${C_GREEN}[uninstall]${C_RESET} $1"; }
error() { echo -e "${C_RED}[uninstall]${C_RESET} $1" >&2; }

PREFIX="${PREFIX:-/usr/local}"
BIN_PATH="$PREFIX/bin/sysflex"
PLUGIN_DIR="$PREFIX/share/sysflex"
MAN_PATH="$PREFIX/share/man/man1/sysflex.1"

remove_path() {
    local path="$1"
    if [ -e "$path" ]; then
        if [ -w "$(dirname "$path")" ] || [ "$(id -u)" -eq 0 ]; then
            rm -rf "$path"
        else
            sudo rm -rf "$path"
        fi
        info "Удалено: $path"
    fi
}

if [ ! -e "$BIN_PATH" ] && [ ! -e "$PLUGIN_DIR" ] && [ ! -e "$MAN_PATH" ]; then
    error "sysflex не найден в $PREFIX. Возможно, он установлен в другой каталог (укажите PREFIX=... uninstall.sh)."
    exit 1
fi

remove_path "$BIN_PATH"
remove_path "$PLUGIN_DIR"
remove_path "$MAN_PATH"

info "sysflex успешно удалён из системы."
