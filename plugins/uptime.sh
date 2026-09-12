#!/usr/bin/env bash
# uptime.sh — простой плагин sysflex.
# Выводит аптайм системы в формате стандартной утилиты `uptime -p`.
# Это минимальный пример плагина: sysflex запускает любой исполняемый файл
# из каталога plugins/ и печатает его stdout в разделе "Плагин: <имя>".

set -euo pipefail

if command -v uptime >/dev/null 2>&1; then
    uptime -p 2>/dev/null || uptime
else
    echo "утилита uptime не найдена"
fi
