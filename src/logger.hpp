// copyright google gemini 14sep2026

#pragma once

#include <iostream>
#include <mutex>
#include <string>
#include <chrono>
#include <format> // Требует C++20

namespace logger {

    // Уровни логирования
    enum class Level {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    // Вспомогательная функция для получения текущего времени в формате HH:MM:SS.ooo
    inline std::string get_current_time_string() {
        auto now = std::chrono::system_clock::now();
        auto time_t_zone = std::chrono::current_zone()->to_local(now);
        
        // Форматируем время с точностью до миллисекунд
        return std::format("{:%H:%M:%S}", std::chrono::floor<std::chrono::milliseconds>(time_t_zone));
    }

    // Превращаем энум в строку с цветовыми кодами ANSI для консоли Linux/macOS
    inline std::string level_to_string(Level level) {
        switch (level) {
            case Level::Debug:      return "\033[36m[DEBUG]\033[0m"; // Циановый
            case Level::Info:       return "\033[32m[INFO] \033[0m"; // Зеленый
            case Level::Warning:    return "\033[33m[WARN] \033[0m"; // Желтый
            case Level::Error:      return "\033[31m[ERROR]\033[0m"; // Красный
            case Level::Fatal:      return "\033[31;1;4m[FATAL]\033[0m"; // 
        }
        return "[LOG]";
    }

    // Основная функция логирования (потокобезопасная через std::mutex)
    template <typename... Args>
    void log(Level level, std::format_string<Args...> fmt, Args&&... args) {
        // Мьютекс защищает от перемешивания строк, если лог идет из разных потоков (например, от сети Box2D и логики)
        static std::mutex log_mutex;
        std::lock_guard<std::mutex> lock(log_mutex);

        std::string time_str = get_current_time_string();
        std::string level_str = level_to_string(level);
        
        // Форматируем пользовательское сообщение
        std::string message = std::format(fmt, std::forward<Args>(args)...);

        // Выводим в стандартный поток (Error и Warn лучше слать в cerr)
        if (level == Level::Error || level == Level::Warning) {
            std::cerr << std::format("{} {} {}\n", time_str, level_str, message);
        } else {
            std::cout << std::format("{} {} {}\n", time_str, level_str, message);
        }
    }

    // Удобные обертки (short-cuts), чтобы не писать каждый раз Level::...
    template <typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::Debug, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::Info, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::Warning, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::Error, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void fatal(std::format_string<Args...> fmt, Args&&... args) {
        log(Level::Fatal, fmt, std::forward<Args>(args)...);
    }   
}