#include "pch.h"
#include <gtest/gtest.h>

#include <iostream>


int main(int argc, char** argv)
{
    std::cerr << "Starting Google Test runtime\n" << std::flush;
    ::testing::InitGoogleTest(&argc, argv);

    std::cerr << "Running the tests\n" << std::flush;
    return RUN_ALL_TESTS();
}