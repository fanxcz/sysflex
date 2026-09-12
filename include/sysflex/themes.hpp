// themes.hpp — цветовые темы оформления для терминального вывода
// Каждая тема представляет собой набор ANSI escape-кодов для разных
// элементов интерфейса: заголовков, акцентов, предупреждений и т.д.
#pragma once

#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include "utils.hpp"

namespace sysflex {

// ANSI escape-код сброса цвета
inline const std::string RESET = "\033[0m";
inline const std::string BOLD = "\033[1m";
inline const std::string DIM = "\033[2m";

// Структура темы: содержит набор цветов для различных элементов вывода
struct Theme {
    std::string name;
    std::string primary;    // Основной акцентный цвет (заголовки, рамки)
    std::string secondary;  // Второстепенный акцентный цвет
    std::string success;    // Цвет для хороших значений (низкая нагрузка и т.п.)
    std::string warning;    // Цвет для предупреждений (средняя нагрузка)
    std::string danger;     // Цвет для критических значений (высокая нагрузка)
    std::string text;       // Обычный текст
    std::string muted;      // Приглушённый текст (подписи, второстепенная инфа)
    std::string reset;      // Код сброса стиля (пусто, если цвета отключены)
    std::string bold;       // Код жирного начертания (пусто, если цвета отключены)

    // Возвращает цвет по проценту нагрузки: зелёный < 60%, жёлтый < 85%, красный >= 85%
    [[nodiscard]] std::string colorForPercent(double percent) const {
        if (percent < 60.0) return success;
        if (percent < 85.0) return warning;
        return danger;
    }
};

// Возвращает таблицу всех доступных тем
inline const std::unordered_map<std::string, Theme>& allThemes() {
    static const std::unordered_map<std::string, Theme> themes = {
        {"default", Theme{
            "default",
            "\033[36m",   // primary — cyan
            "\033[35m",   // secondary — magenta
            "\033[32m",   // success — green
            "\033[33m",   // warning — yellow
            "\033[31m",   // danger — red
            "\033[97m",   // text — bright white
            "\033[90m",   // muted — grey
            "", ""        // reset/bold заполняются позже в getTheme()
        }},
        {"dracula", Theme{
            "dracula",
            "\033[38;5;141m", // фиолетовый (purple)
            "\033[38;5;212m", // розовый (pink)
            "\033[38;5;84m",  // зелёный (green)
            "\033[38;5;228m", // жёлтый (yellow)
            "\033[38;5;203m", // красный (red)
            "\033[38;5;253m", // светло-серый foreground
            "\033[38;5;61m",  // приглушённый comment-серый
            "", ""
        }},
        {"nord", Theme{
            "nord",
            "\033[38;5;110m", // nord8 — голубой
            "\033[38;5;109m", // nord7 — бирюзовый
            "\033[38;5;114m", // nord14 — зелёный
            "\033[38;5;222m", // nord13 — жёлтый
            "\033[38;5;167m", // nord11 — красный
            "\033[38;5;253m", // snow storm
            "\033[38;5;102m", // polar night приглушённый
            "", ""
        }},
        {"catppuccin", Theme{
            "catppuccin",
            "\033[38;5;183m", // mauve
            "\033[38;5;217m", // pink
            "\033[38;5;150m", // green
            "\033[38;5;223m", // yellow
            "\033[38;5;210m", // red
            "\033[38;5;189m", // text
            "\033[38;5;103m", // overlay/приглушённый
            "", ""
        }},
        {"gruvbox", Theme{
            "gruvbox",
            "\033[38;5;208m", // orange
            "\033[38;5;109m", // blue-ish aqua
            "\033[38;5;142m", // green
            "\033[38;5;214m", // yellow
            "\033[38;5;167m", // red
            "\033[38;5;223m", // fg
            "\033[38;5;245m", // grey
            "", ""
        }},
        {"solarized", Theme{
            "solarized",
            "\033[38;5;33m",  // blue
            "\033[38;5;37m",  // cyan
            "\033[38;5;64m",  // green
            "\033[38;5;136m", // yellow
            "\033[38;5;160m", // red
            "\033[38;5;230m", // base2/светлый текст
            "\033[38;5;244m", // base01 приглушённый
            "", ""
        }},
        {"tokyonight", Theme{
            "tokyonight",
            "\033[38;5;111m", // синий (blue)
            "\033[38;5;176m", // фиолетовый (magenta)
            "\033[38;5;114m", // зелёный (green)
            "\033[38;5;180m", // жёлтый (yellow)
            "\033[38;5;203m", // красный (red)
            "\033[38;5;189m", // светлый текст (fg)
            "\033[38;5;60m",  // приглушённый (comment)
            "", ""
        }},
    };
    return themes;
}

// Возвращает тему по имени; если тема не найдена — возвращает default с предупреждением
inline Theme getTheme(const std::string& name, bool noColor) {
    Theme t;
    auto& themes = allThemes();
    auto it = themes.find(utils::toLowerStr(name));
    if (it != themes.end()) {
        t = it->second;
    } else {
        t = themes.at("default");
    }
    if (noColor) {
        // Если цвета отключены — обнуляем все escape-коды, включая reset/bold
        t.primary = t.secondary = t.success = t.warning = t.danger = t.text = t.muted = "";
        t.reset = "";
        t.bold = "";
    } else {
        t.reset = RESET;
        t.bold = BOLD;
    }
    return t;
}

} // namespace sysflex
