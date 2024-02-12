#include "pch.h"
/*
 * Copyright (c) 2023-2024. Bert Laverman
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>

#include <iostream>

#include "Log.h"

using namespace nl::rakis::logging;

TEST(LogTests, TestLogging)
{
    std::cerr << "Getting a logger\n" << std::flush;
    Logger log{ Logger::getLogger("test") };

    std::cerr << "Log something.\n" << std::flush;
    log.info("test");

    std::cerr << "Do something silly\n" << std::flush;
    const auto expected = 1;
    const auto actual = 1;
    ASSERT_EQ(expected, actual) << "Do something silly\n";
}
