#include "utils/logger.hpp"
#include <iostream>

int main()
{
    try
    {
        std::cout << "=== Simple Logger Test ===" << std::endl;

        // Test stdout logging
        synth::Logger logger(synth::Logger::LogLevel::DEBUG);

        logger.info("Logger initialized successfully");
        logger.debug("Debug message test");
        logger.warning("Warning message test");
        logger.error("Error message test");

        // Test file logging
        synth::Logger file_logger("simple_test.log", synth::Logger::LogLevel::INFO);
        file_logger.info("File logging works!");
        file_logger.error("Error in file");

        logger.info("Test completed successfully");

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
