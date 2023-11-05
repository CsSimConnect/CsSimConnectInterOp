# CMakeList.txt : CMake project for CsSimConnectInterOp, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()
SET(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

project (CsSimConnectInterOp)

enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src $ENV{MSFS_SDK}/SimConnect\ SDK/include)
link_directories($ENV{MSFS_SDK}/SimConnect\ SDK/lib)
link_libraries(SimConnect.lib)

add_subdirectory(src)
add_subdirectory(unittests)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CsSimConnectInterOp PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
