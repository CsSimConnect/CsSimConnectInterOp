# Here are the Unittests

add_executable(TestInterOp LogTests.cpp)

add_test(TestInterOp TestInterOp)

target_link_libraries(TestInterOp
 PRIVATE
  CsSimConnectInterOp
  SimConnect.lib
  GTest::gtest
  GTest::gtest_main)
