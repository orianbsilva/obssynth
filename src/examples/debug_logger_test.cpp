#include "utils/logger.hpp"
#include <iostream>

int main()
{
    try
    {
        std::cout << "=== Testing file logging only ===" << std::endl;

        {
            std::cout << "Creating file logger..." << std::endl;
            synth::Logger file_logger("debug_test.log", synth::Logger::LogLevel::INFO);

            std::cout << "Logging to file..." << std::endl;
            file_logger.info("Starting file logging test");
            file_logger.warning("This is a warning in the file");
            file_logger.error("This is an error in the file");

            std::cout << "Flushing..." << std::endl;
            file_logger.flush();

            std::cout << "File logging test completed successfully" << std::endl;
        }

        std::cout << "Logger destroyed, test completed" << std::endl;
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
}
