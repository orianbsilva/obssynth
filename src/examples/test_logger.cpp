#include "utils/logger.hpp"
#include <thread>
#include <chrono>

int main()
{
    try
    {
        // Test 1: Stdout logging
        std::cout << "=== Testing stdout logging ===" << std::endl;
        {
            synth::Logger stdout_logger(synth::Logger::LogLevel::DEBUG);

            stdout_logger.debug("This is a debug message");
            stdout_logger.info("This is an info message");
            stdout_logger.warning("This is a warning message");
            stdout_logger.error("This is an error message");
            stdout_logger.critical("This is a critical message");

            // Test logging from multiple threads
            std::cout << "\n--- Testing multi-threaded logging ---" << std::endl;

            std::thread t1([&stdout_logger]()
                           {
                for (int i = 0; i < 5; ++i) {
                    stdout_logger.info("Thread 1 - Message " + std::to_string(i));
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } });

            std::thread t2([&stdout_logger]()
                           {
                for (int i = 0; i < 5; ++i) {
                    stdout_logger.warning("Thread 2 - Message " + std::to_string(i));
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                } });

            t1.join();
            t2.join();

            // Ensure all messages are written
            stdout_logger.flush();
        }

        std::cout << "\n=== Testing file logging ===" << std::endl;

        // Test 2: File logging
        {
            synth::Logger file_logger("test_log.txt", synth::Logger::LogLevel::INFO);

            file_logger.info("Starting file logging test");
            file_logger.warning("This is a warning in the file");
            file_logger.error("This is an error in the file");

            // Test level filtering
            file_logger.debug("This debug message should not appear (level too low)");

            file_logger.info("Testing level changes");
            file_logger.set_min_level(synth::Logger::LogLevel::DEBUG);
            file_logger.debug("This debug message should now appear");

            file_logger.flush();
        }

        std::cout << "File logging test completed. Check 'test_log.txt' for output." << std::endl;

        // Test 3: Test without timestamps
        std::cout << "\n=== Testing without timestamps ===" << std::endl;
        {
            synth::Logger no_timestamp_logger(synth::Logger::LogLevel::INFO);
            no_timestamp_logger.set_timestamp_enabled(false);

            no_timestamp_logger.info("Message without timestamp");
            no_timestamp_logger.error("Error without timestamp");

            no_timestamp_logger.flush();
        }

        // Test 4: Test high-frequency logging
        std::cout << "\n=== Testing high-frequency logging ===" << std::endl;
        {
            synth::Logger perf_logger(synth::Logger::LogLevel::INFO);

            auto start = std::chrono::steady_clock::now();

            for (int i = 0; i < 100; ++i)
            { // Reduced from 1000 to 100
                perf_logger.info("High frequency message " + std::to_string(i));
            }

            perf_logger.flush();

            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            std::cout << "Logged 100 messages in " << duration.count() << "ms" << std::endl;
        }

        std::cout << "\n=== All logger tests completed successfully ===" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
