#!/usr/bin/env bash
# crypto.sh — плагин sysflex, показывающий текущий курс BTC/USD.
# Использует публичный API CoinGecko (без ключа). Требует curl и, желательно,
# jq для аккуратного парсинга JSON (при отсутствии jq используется grep/sed).
#
# Внимание: этот плагин обращается к внешнему сервису через сеть, поэтому
# при отсутствии интернета или недоступности API он просто ничего не выведет
# (что для sysflex равносильно "плагин пропущен").

set -euo pipefail

API_URL="https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd"

if ! command -v curl >/dev/null 2>&1; then
    exit 0
fi

# Таймаут в 2 секунды, чтобы не тормозить общий вывод sysflex
RESPONSE=$(curl -s -m 2 "$API_URL" 2>/dev/null || true)

if [ -z "$RESPONSE" ]; then
    exit 0
fi

if command -v jq >/dev/null 2>&1; then
    PRICE=$(echo "$RESPONSE" | jq -r '.bitcoin.usd' 2>/dev/null || true)
else
    # Простой парсинг без jq: ищем число после "usd":
    PRICE=$(echo "$RESPONSE" | grep -o '"usd":[0-9.]*' | head -1 | cut -d':' -f2)
fi

if [ -n "${PRICE:-}" ] && [ "$PRICE" != "null" ]; then
    echo "BTC/USD: \$${PRICE}"
fi
