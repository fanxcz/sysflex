#!/usr/bin/env bash
# weather.sh — плагин sysflex, показывающий текущую погоду.
# Использует бесплатный текстовый сервис wttr.in — не требует API-ключа.
# Если задана переменная окружения SYSFLEX_CITY, используется указанный
# город, иначе wttr.in определяет местоположение по IP-адресу.

set -euo pipefail

if ! command -v curl >/dev/null 2>&1; then
    exit 0
fi

CITY="${SYSFLEX_CITY:-}"
# Формат "%l: %c %t" -> "Город: облачно +21°C"
URL="https://wttr.in/${CITY}?format=%l:+%c+%t"

WEATHER=$(curl -s -m 2 "$URL" 2>/dev/null || true)

if [ -n "$WEATHER" ] && [[ "$WEATHER" != *"Unknown location"* ]]; then
    echo "$WEATHER"
fi
