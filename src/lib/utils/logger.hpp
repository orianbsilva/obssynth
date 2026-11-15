#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <chrono>
#include <sstream>

namespace synth
{

    /**
     * @brief Thread-safe Logger class that logs messages in a separate thread
     *
     * The Logger class provides asynchronous logging capabilities to either stdout
     * or a file. Messages are queued and processed in a dedicated logging thread
     * to avoid blocking the main application thread.
     */
    class Logger
    {
    public:
        /**
         * @brief Log levels for filtering messages
         */
        enum class LogLevel
        {
            DEBUG = 0,
            INFO = 1,
            WARNING = 2,
            ERROR = 3,
            CRITICAL = 4
        };

        /**
         * @brief Output destination for log messages
         */
        enum class OutputType
        {
            STDOUT,
            FILE
        };

        /**
         * @brief Constructor for stdout logging
         * @param min_level Minimum log level to output
         */
        explicit Logger(LogLevel min_level = LogLevel::INFO);

        /**
         * @brief Constructor for file logging
         * @param filename Path to the log file
         * @param min_level Minimum log level to output
         */
        Logger(const std::string &filename, LogLevel min_level = LogLevel::INFO);

        /**
         * @brief Destructor - ensures all messages are written before destruction
         */
        ~Logger();

        // Delete copy constructor and assignment operator
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

        // Move constructor and assignment operator
        Logger(Logger &&other) noexcept;
        Logger &operator=(Logger &&other) noexcept;

        /**
         * @brief Log a debug message
         * @param message The message to log
         */
        void debug(const std::string &message);

        /**
         * @brief Log an info message
         * @param message The message to log
         */
        void info(const std::string &message);

        /**
         * @brief Log a warning message
         * @param message The message to log
         */
        void warning(const std::string &message);

        /**
         * @brief Log an error message
         * @param message The message to log
         */
        void error(const std::string &message);

        /**
         * @brief Log a critical message
         * @param message The message to log
         */
        void critical(const std::string &message);

        /**
         * @brief Log a message with specified level
         * @param level The log level
         * @param message The message to log
         */
        void log(LogLevel level, const std::string &message);

        /**
         * @brief Set the minimum log level
         * @param level New minimum log level
         */
        void set_min_level(LogLevel level);

        /**
         * @brief Get the current minimum log level
         * @return Current minimum log level
         */
        LogLevel get_min_level() const;

        /**
         * @brief Enable or disable timestamp prefixes
         * @param enable Whether to include timestamps
         */
        void set_timestamp_enabled(bool enable);

        /**
         * @brief Check if timestamp prefixes are enabled
         * @return True if timestamps are enabled
         */
        bool is_timestamp_enabled() const;

        /**
         * @brief Flush all pending log messages
         * This method blocks until all queued messages are written
         */
        void flush();

    private:
        struct LogMessage
        {
            LogLevel level;
            std::string message;
            std::chrono::system_clock::time_point timestamp;

            LogMessage(LogLevel l, std::string msg)
                : level(l), message(std::move(msg)), timestamp(std::chrono::system_clock::now()) {}
        };

        // Private member variables (following project convention with underscore suffix)
        OutputType output_type_;
        std::string filename_;
        LogLevel min_level_;
        std::atomic<bool> timestamp_enabled_;

        std::queue<LogMessage> message_queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_condition_;
        std::atomic<bool> stop_logging_;
        std::thread logging_thread_;

        std::unique_ptr<std::ofstream> file_stream_;

        /**
         * @brief Initialize the logger (common initialization for all constructors)
         */
        void initialize();

        /**
         * @brief Main logging thread function
         */
        void logging_worker();

        /**
         * @brief Write a message to the output destination
         * @param msg The log message to write
         */
        void write_message(const LogMessage &msg);

        /**
         * @brief Convert log level to string representation
         * @param level The log level to convert
         * @return String representation of the log level
         */
        static std::string level_to_string(LogLevel level);

        /**
         * @brief Format timestamp for log output
         * @param timestamp The timestamp to format
         * @return Formatted timestamp string
         */
        static std::string format_timestamp(const std::chrono::system_clock::time_point &timestamp);

        /**
         * @brief Move implementation helper
         * @param other Logger to move from
         */
        void move_from(Logger &&other) noexcept;
    };

} // namespace synth

#endif // LOGGER_HPP
