#ifndef LOG_MANAGER_HPP
#define LOG_MANAGER_HPP

#include "logger.hpp"
#include <memory>
#include <mutex>
#include <string>

namespace synth
{

    /**
     * @brief Singleton LogManager class that provides global access to logging functionality
     *
     * The LogManager class implements the singleton pattern to provide a single point
     * of access for logging throughout the application. It manages a Logger instance
     * and provides convenient static methods for logging.
     */
    class LogManager
    {
    public:
        /**
         * @brief Get the singleton instance of LogManager
         * @return Reference to the LogManager instance
         */
        static LogManager &getInstance();

        /**
         * @brief Initialize the LogManager with stdout logging
         * @param min_level Minimum log level to output
         */
        void initialize(Logger::LogLevel min_level = Logger::LogLevel::INFO);

        /**
         * @brief Initialize the LogManager with file logging
         * @param filename Path to the log file
         * @param min_level Minimum log level to output
         */
        void initialize(const std::string &filename, Logger::LogLevel min_level = Logger::LogLevel::INFO);

        /**
         * @brief Check if the LogManager has been initialized
         * @return True if initialized, false otherwise
         */
        bool isInitialized() const;

        /**
         * @brief Shutdown the LogManager and flush all pending messages
         */
        void shutdown();

        // Static convenience methods for logging
        static void Debug(const std::string &message);
        static void Info(const std::string &message);
        static void Warning(const std::string &message);
        static void Error(const std::string &message);
        static void Critical(const std::string &message);
        static void Log(Logger::LogLevel level, const std::string &message);

        // Configuration methods
        static void SetMinLevel(Logger::LogLevel level);
        static Logger::LogLevel GetMinLevel();
        static void SetTimestampEnabled(bool enable);
        static bool IsTimestampEnabled();
        static void Flush();

        // Delete copy constructor and assignment operator
        LogManager(const LogManager &) = delete;
        LogManager &operator=(const LogManager &) = delete;

        // Delete move constructor and assignment operator
        LogManager(LogManager &&) = delete;
        LogManager &operator=(LogManager &&) = delete;

    private:
        /**
         * @brief Private constructor for singleton pattern
         */
        LogManager() = default;

        /**
         * @brief Private destructor
         */
        ~LogManager();

        /**
         * @brief Get the logger instance (throws if not initialized)
         * @return Reference to the logger instance
         */
        Logger &getLogger();

        /**
         * @brief Get the logger instance (const version, throws if not initialized)
         * @return Const reference to the logger instance
         */
        const Logger &getLogger() const;

        // Private member variables (following project convention with underscore suffix)
        std::unique_ptr<Logger> logger_;
        mutable std::mutex mutex_;
        bool initialized_;
    };

} // namespace synth

#endif // LOG_MANAGER_HPP
