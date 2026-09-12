// ascii_art.hpp — ASCII-арт логотипы для популярных дистрибутивов Linux
// Используется для украшения вывода рядом с общей информацией о системе.
#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include "themes.hpp"

namespace sysflex::ascii {

using Art = std::vector<std::string>;

// Логотип Debian (упрощённый, символьный)
inline Art debianArt() {
    return {
        "   _,met$$$$$gg.   ",
        " ,g$$$$$$$$$$$$$$$P. ",
        ",$$$P'     `$$$Y$$.\\.",
        "',$$P       ,ggs.`$b:",
        "`d$$'     ,$P\"'   .$$$",
        " $$P      d$'     ,$$P",
        " $$:      $$.   -,d$$'",
        " $$;      Y$b._   _,d$P",
        " Y$$.    `.`\"Y$$$$P\"'",
        " `$$b      \"-.__",
        "  `Y$$",
        "   `Y$$.",
        "     `$$b.",
        "       `Y$$b.",
        "          `\"Y$b._",
        "              `\"\"\""
    };
}

// Логотип Ubuntu (стилизованный)
inline Art ubuntuArt() {
    return {
        "         _",
        "     ---(_)",
        " _/  ---  \\",
        "(_) |   |",
        "  \\  --- _/",
        "     ---(_)"
    };
}

// Логотип Arch Linux
inline Art archArt() {
    return {
        "      /\\      ",
        "     /  \\     ",
        "    /\\   \\    ",
        "   /      \\   ",
        "  /   ,,   \\  ",
        " /   |  |  -\\ ",
        "/_-''    ''-_\\"
    };
}

// Логотип Fedora
inline Art fedoraArt() {
    return {
        "      _____   ",
        "     /   __)\\ ",
        "     |  /  \\ \\",
        "  ___|  |__/ /",
        " / (_    _)_/ ",
        "/ /  |  |     ",
        "\\ \\__/  |     ",
        " \\(_____/     "
    };
}

// Обобщённый логотип пингвина Tux для неопознанных / прочих дистрибутивов
inline Art genericArt() {
    return {
        "    .--.    ",
        "   |o_o |   ",
        "   |:_/ |   ",
        "  //   \\ \\  ",
        " (|     | ) ",
        "/'\\_   _/`\\ ",
        "\\___)=(___/ "
    };
}

// Определяет и возвращает подходящий ASCII-арт по названию ОС (уже приведённому
// к нижнему регистру строке /etc/os-release или аналогичной)
inline Art artForOs(const std::string& osNameLower) {
    if (osNameLower.find("debian") != std::string::npos) return debianArt();
    if (osNameLower.find("ubuntu") != std::string::npos) return ubuntuArt();
    if (osNameLower.find("arch") != std::string::npos) return archArt();
    if (osNameLower.find("fedora") != std::string::npos) return fedoraArt();
    return genericArt();
}

} // namespace sysflex::ascii
