#include "log_manager.hpp"
#include <stdexcept>

namespace synth
{

    LogManager &LogManager::getInstance()
    {
        static LogManager instance;
        return instance;
    }

    void LogManager::initialize(Logger::LogLevel min_level)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_)
        {
            throw std::runtime_error("LogManager is already initialized");
        }

        logger_ = std::make_unique<Logger>(min_level);
        initialized_ = true;
    }

    void LogManager::initialize(const std::string &filename, Logger::LogLevel min_level)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_)
        {
            throw std::runtime_error("LogManager is already initialized");
        }

        logger_ = std::make_unique<Logger>(filename, min_level);
        initialized_ = true;
    }

    bool LogManager::isInitialized() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return initialized_;
    }

    void LogManager::shutdown()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_)
        {
            return; // Already shutdown or never initialized
        }

        if (logger_)
        {
            logger_->flush();
            logger_.reset();
        }

        initialized_ = false;
    }

    LogManager::~LogManager()
    {
        // No need to lock here as destructor is called only once
        if (initialized_ && logger_)
        {
            logger_->flush();
        }
    }

    Logger &LogManager::getLogger()
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_ || !logger_)
        {
            throw std::runtime_error("LogManager is not initialized. Call initialize() first.");
        }

        return *logger_;
    }

    const Logger &LogManager::getLogger() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_ || !logger_)
        {
            throw std::runtime_error("LogManager is not initialized. Call initialize() first.");
        }

        return *logger_;
    }

    // Static convenience methods
    void LogManager::Debug(const std::string &message)
    {
        getInstance().getLogger().debug(message);
    }

    void LogManager::Info(const std::string &message)
    {
        getInstance().getLogger().info(message);
    }

    void LogManager::Warning(const std::string &message)
    {
        getInstance().getLogger().warning(message);
    }

    void LogManager::Error(const std::string &message)
    {
        getInstance().getLogger().error(message);
    }

    void LogManager::Critical(const std::string &message)
    {
        getInstance().getLogger().critical(message);
    }

    void LogManager::Log(Logger::LogLevel level, const std::string &message)
    {
        getInstance().getLogger().log(level, message);
    }

    void LogManager::SetMinLevel(Logger::LogLevel level)
    {
        getInstance().getLogger().set_min_level(level);
    }

    Logger::LogLevel LogManager::GetMinLevel()
    {
        return getInstance().getLogger().get_min_level();
    }

    void LogManager::SetTimestampEnabled(bool enable)
    {
        getInstance().getLogger().set_timestamp_enabled(enable);
    }

    bool LogManager::IsTimestampEnabled()
    {
        return getInstance().getLogger().is_timestamp_enabled();
    }

    void LogManager::Flush()
    {
        getInstance().getLogger().flush();
    }

} // namespace synth
