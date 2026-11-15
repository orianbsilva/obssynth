#include "logger.hpp"
#include <iomanip>

namespace synth
{

    Logger::Logger(LogLevel min_level)
        : output_type_(OutputType::STDOUT), min_level_(min_level), timestamp_enabled_(true), stop_logging_(false)
    {
        initialize();
    }

    Logger::Logger(const std::string &filename, LogLevel min_level)
        : output_type_(OutputType::FILE), filename_(filename), min_level_(min_level), timestamp_enabled_(true), stop_logging_(false)
    {

        // Open file stream
        file_stream_ = std::make_unique<std::ofstream>(filename_, std::ios::app);
        if (!file_stream_->is_open())
        {
            throw std::runtime_error("Failed to open log file: " + filename_);
        }

        initialize();
    }

    Logger::~Logger()
    {
        if (logging_thread_.joinable())
        {
            stop_logging_ = true;
            queue_condition_.notify_all();
            logging_thread_.join();
        }

        // Close file stream if it exists
        if (file_stream_)
        {
            file_stream_->close();
        }
    }

    Logger::Logger(Logger &&other) noexcept
        : output_type_(OutputType::STDOUT), min_level_(LogLevel::INFO), timestamp_enabled_(true), stop_logging_(false)
    {
        move_from(std::move(other));
    }

    Logger &Logger::operator=(Logger &&other) noexcept
    {
        if (this != &other)
        {
            // Clean up current state
            if (logging_thread_.joinable())
            {
                stop_logging_ = true;
                queue_condition_.notify_all();
                logging_thread_.join();
            }

            if (file_stream_)
            {
                file_stream_->close();
                file_stream_.reset();
            }

            // Clear the queue
            std::queue<LogMessage> empty_queue;
            message_queue_.swap(empty_queue);

            // Move from other
            move_from(std::move(other));
        }
        return *this;
    }

    void Logger::debug(const std::string &message)
    {
        log(LogLevel::DEBUG, message);
    }

    void Logger::info(const std::string &message)
    {
        log(LogLevel::INFO, message);
    }

    void Logger::warning(const std::string &message)
    {
        log(LogLevel::WARNING, message);
    }

    void Logger::error(const std::string &message)
    {
        log(LogLevel::ERROR, message);
    }

    void Logger::critical(const std::string &message)
    {
        log(LogLevel::CRITICAL, message);
    }

    void Logger::log(LogLevel level, const std::string &message)
    {
        // Check if we should log this level
        if (level < min_level_)
        {
            return;
        }

        // Don't log if we're shutting down
        if (stop_logging_)
        {
            return;
        }

        // Add message to queue
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            message_queue_.emplace(level, message);
        }

        // Notify the logging thread
        queue_condition_.notify_one();
    }

    void Logger::set_min_level(LogLevel level)
    {
        min_level_ = level;
    }

    Logger::LogLevel Logger::get_min_level() const
    {
        return min_level_;
    }

    void Logger::set_timestamp_enabled(bool enable)
    {
        timestamp_enabled_ = enable;
    }

    bool Logger::is_timestamp_enabled() const
    {
        return timestamp_enabled_;
    }

    void Logger::flush()
    {
        // Don't flush if we're shutting down
        if (stop_logging_)
        {
            return;
        }

        // Wait for all messages to be processed
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_condition_.wait(lock, [this]
                              { return message_queue_.empty() || stop_logging_; });

        // Force flush the output stream
        if (output_type_ == OutputType::FILE && file_stream_)
        {
            file_stream_->flush();
        }
        else if (output_type_ == OutputType::STDOUT)
        {
            std::cout.flush();
        }
    }

    void Logger::initialize()
    {
        // Start the logging thread
        logging_thread_ = std::thread(&Logger::logging_worker, this);
    }

    void Logger::logging_worker()
    {
        while (!stop_logging_)
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            // Wait for messages or stop signal
            queue_condition_.wait(lock, [this]
                                  { return !message_queue_.empty() || stop_logging_; });

            // Process all available messages
            while (!message_queue_.empty())
            {
                LogMessage msg = std::move(message_queue_.front());
                message_queue_.pop();

                // Unlock while writing to avoid blocking the main thread
                lock.unlock();
                write_message(msg);
                lock.lock();
            }

            // Notify that queue is empty (for flush operations)
            queue_condition_.notify_all();
        }

        // Process any remaining messages before exiting
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!message_queue_.empty())
        {
            LogMessage msg = std::move(message_queue_.front());
            message_queue_.pop();
            write_message(msg);
        }

        // Final notification
        queue_condition_.notify_all();
    }

    void Logger::write_message(const LogMessage &msg)
    {
        std::ostringstream formatted_message;

        // Add timestamp if enabled
        if (timestamp_enabled_)
        {
            formatted_message << format_timestamp(msg.timestamp) << " ";
        }

        // Add log level
        formatted_message << "[" << level_to_string(msg.level) << "] ";

        // Add the actual message
        formatted_message << msg.message << std::endl;

        // Write to appropriate output
        if (output_type_ == OutputType::FILE && file_stream_)
        {
            *file_stream_ << formatted_message.str();
            file_stream_->flush(); // Ensure immediate write
        }
        else
        {
            std::cout << formatted_message.str();
            std::cout.flush(); // Ensure immediate write
        }
    }

    std::string Logger::level_to_string(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERROR:
            return "ERROR";
        case LogLevel::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
        }
    }

    std::string Logger::format_timestamp(const std::chrono::system_clock::time_point &timestamp)
    {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      timestamp.time_since_epoch()) %
                  1000;

        std::ostringstream oss;

#ifdef _WIN32
        // Use localtime_s on Windows
        struct tm time_info;
        if (localtime_s(&time_info, &time_t) == 0)
        {
            oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
        }
#else
// Use localtime_r on Unix-like systems, localtime on others
#ifdef _POSIX_C_SOURCE
        struct tm time_info;
        if (localtime_r(&time_t, &time_info) != nullptr)
        {
            oss << std::put_time(&time_info, "%Y-%m-%d %H:%M:%S");
        }
#else
        auto *time_info = std::localtime(&time_t);
        if (time_info != nullptr)
        {
            oss << std::put_time(time_info, "%Y-%m-%d %H:%M:%S");
        }
#endif
#endif

        oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

        return oss.str();
    }

    void Logger::move_from(Logger &&other) noexcept
    {
        output_type_ = other.output_type_;
        filename_ = std::move(other.filename_);
        min_level_ = other.min_level_;
        timestamp_enabled_ = other.timestamp_enabled_.load();
        stop_logging_ = false;

        // Move the file stream
        file_stream_ = std::move(other.file_stream_);

        // Move the message queue
        message_queue_ = std::move(other.message_queue_);

        // Stop the other logger's thread
        other.stop_logging_ = true;
        other.queue_condition_.notify_all();
        if (other.logging_thread_.joinable())
        {
            other.logging_thread_.join();
        }

        // Start our own thread
        initialize();
    }

} // namespace synth
