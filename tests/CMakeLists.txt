cmake_minimum_required(VERSION 3.16)

# Add Google Test to the project (via vcpkg config)
find_package(GTest CONFIG REQUIRED)

# Include test directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../cpp/include)

# Option to limit tests to Milestone 2 compatible ones
option(ECO_WATT_M2_TESTS_ONLY "Build only Milestone 2 compatible tests" ON)

# Define test source files (limit to Modbus frame tests for M2)
if(ECO_WATT_M2_TESTS_ONLY)
    set(TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_frame.cpp
    )
else()
    set(TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test_modbus_frame.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_protocol_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_api_integration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_error_scenarios.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_data_storage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test_acquisition_scheduler.cpp
    )
endif()

# Define project sources needed by tests (exclude main.cpp)
set(PROJECT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/modbus_frame.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/src/logger.cpp
)

# Create individual test executables
foreach(TEST_SOURCE ${TEST_SOURCES})
    # Get test name without extension and path
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)

    # Create test executable - link only what each test needs
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${PROJECT_SOURCES})

    # Link required libraries
    target_link_libraries(${TEST_NAME}
        GTest::gtest
        GTest::gmock
        spdlog::spdlog
        fmt::fmt
    )

    # Set C++17 standard
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)

    # Multi-config generators output into per-config folders; set runtime dir to current binary dir
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    # Add test to CTest
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})

    # Set test properties
    set_tests_properties(${TEST_NAME} PROPERTIES
        TIMEOUT 60
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
endforeach()

# Copy test configuration files (if needed by tests)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/config.json)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/../cpp/config.json
        ${CMAKE_CURRENT_BINARY_DIR}/config.json
        COPYONLY
    )
endif()

# Enable testing
enable_testing()

# Summary
message(STATUS "")
message(STATUS "Test Configuration Summary:")
message(STATUS "  Google Test Found: ${GTest_FOUND}")
message(STATUS "  Building M2-only tests: ${ECO_WATT_M2_TESTS_ONLY}")
message(STATUS "  Test Sources: ${TEST_SOURCES}")
message(STATUS "  Test Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Test Timeout: 60 seconds per test")
message(STATUS "")
