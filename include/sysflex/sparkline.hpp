// sparkline.hpp — мини-графики (спарклайны) для отображения истории значений
// прямо в терминале с помощью блочных символов Юникода.
#pragma once

#include <deque>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

namespace sysflex {

// Класс для хранения истории числовых значений и рендеринга их в виде
// компактного текстового графика (спарклайна), например: ▂▃▅▇█▆▄▃▂
class Sparkline {
public:
    explicit Sparkline(size_t maxHistory = 40) : maxHistory_(maxHistory) {}

    // Добавляет новое значение в историю, отбрасывая старые при переполнении
    void add(double value) {
        history_.push_back(value);
        while (history_.size() > maxHistory_) {
            history_.pop_front();
        }
    }

    // Изменяет максимальный размер истории (используется при --history N)
    void setMaxHistory(size_t maxHistory) {
        maxHistory_ = maxHistory;
        while (history_.size() > maxHistory_) {
            history_.pop_front();
        }
    }

    // Возвращает текущую историю значений
    [[nodiscard]] const std::deque<double>& history() const { return history_; }

    // Рендерит спарклайн в виде строки Юникод-символов блоков разной высоты.
    // По умолчанию шкала — от 0 до 100 (для процентов), но можно передать
    // fixedMax = -1, чтобы шкала строилась динамически по максимуму истории.
    [[nodiscard]] std::string render(double fixedMax = 100.0) const {
        static const std::vector<std::string> blocks = {
            " ", "\u2581", "\u2582", "\u2583", "\u2584", "\u2585", "\u2586", "\u2587", "\u2588"
        };
        if (history_.empty()) return "";

        double maxVal = fixedMax;
        if (maxVal <= 0.0) {
            maxVal = *std::max_element(history_.begin(), history_.end());
            if (maxVal <= 0.0) maxVal = 1.0;
        }

        std::string result;
        result.reserve(history_.size() * 3);
        for (double v : history_) {
            double clamped = std::min(std::max(v, 0.0), maxVal);
            size_t idx = static_cast<size_t>(std::round((clamped / maxVal) * (blocks.size() - 1)));
            idx = std::min(idx, blocks.size() - 1);
            result += blocks[idx];
        }
        return result;
    }

private:
    std::deque<double> history_;
    size_t maxHistory_;
};

} // namespace sysflex
