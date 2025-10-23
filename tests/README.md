# EcoWatt Device - Comprehensive Test Suite

This directory contains a complete test suite for the EcoWatt Device project, providing thorough validation of all system components and API message handling.

## Test Files Overview

### 1. `test_modbus_frame.cpp` (1,000+ lines)
**Purpose**: Validates Modbus RTU frame creation, parsing, and protocol compliance
- Frame construction and serialization
- CRC-16 validation and error detection
- Hex encoding/decoding utilities
- Function code support (0x03, 0x06)
- Error frame handling
- Protocol specification compliance

### 2. `test_protocol_adapter.cpp` (800+ lines)
**Purpose**: Tests HTTP communication and protocol adapter functionality
- HTTP request/response handling
- Retry mechanisms and timeout handling
- Statistics tracking and error reporting
- Connection management
- JSON message encapsulation
- Integration with Inverter SIM API

### 3. `test_api_integration.cpp` (900+ lines)
**Purpose**: End-to-end API integration testing
- Complete read/write operation flows
- Server connectivity validation
- Register access patterns
- Response parsing verification
- Real-world scenario simulation
- Performance and reliability testing

### 4. `test_error_scenarios.cpp` (700+ lines)
**Purpose**: Comprehensive error handling and edge case testing
- Network failure scenarios
- Protocol error conditions
- Timeout and retry exhaustion
- Malformed message handling
- Recovery mechanism validation
- Graceful degradation testing

### 5. `test_data_storage.cpp` (1,200+ lines)
**Purpose**: Data storage and persistence testing
- Memory storage implementation
- SQLite database operations
- Hybrid storage scenarios
- Thread safety validation
- Performance benchmarking
- Data integrity verification

### 6. `test_acquisition_scheduler.cpp` (1,100+ lines)
**Purpose**: Automated polling and acquisition scheduler testing
- 5-second polling interval validation
- Multi-register acquisition
- Error recovery mechanisms
- Statistics and monitoring
- Configuration management
- Long-running stability

## Building and Running Tests

### Prerequisites
- CMake 3.20 or higher
- Google Test framework (gtest/gmock)
- C++17 compatible compiler
- vcpkg for dependency management

### Build Commands
```powershell
# Navigate to project directory
cd e:\EB P2\embedded-systems-engineering-m2

# Configure CMake with tests enabled
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DBUILD_TESTING=ON

# Build the project and tests
cmake --build build --config Release

# Run all tests
cd build
ctest --output-on-failure
```

### Running Individual Test Suites
```powershell
# Run specific test executable
.\build\Release\test_modbus_frame.exe
.\build\Release\test_protocol_adapter.exe
.\build\Release\test_api_integration.exe
.\build\Release\test_error_scenarios.exe
.\build\Release\test_data_storage.exe
.\build\Release\test_acquisition_scheduler.exe
```

## Test Coverage

### Protocol Validation ✅
- [x] Modbus RTU frame structure
- [x] CRC-16 checksum validation
- [x] Function code implementation
- [x] Register address mapping
- [x] Error code handling

### Communication Testing ✅
- [x] HTTP POST requests
- [x] JSON message format
- [x] Response parsing
- [x] Timeout handling
- [x] Retry mechanisms
- [x] Connection management

### API Compliance ✅
- [x] Read Holding Registers (0x03)
- [x] Write Single Register (0x06)
- [x] Multi-register operations
- [x] Error response handling
- [x] Status code validation

### Data Management ✅
- [x] Sample storage and retrieval
- [x] Time-series data handling
- [x] Memory management
- [x] Persistent storage
- [x] Data integrity
- [x] Performance optimization

### System Integration ✅
- [x] Automated polling (5-second intervals)
- [x] Real-time data acquisition
- [x] Error recovery
- [x] Statistics tracking
- [x] Configuration management
- [x] Long-running stability

### Error Scenarios ✅
- [x] Network connectivity issues
- [x] Server unavailability
- [x] Malformed responses
- [x] Protocol violations
- [x] Resource exhaustion
- [x] Graceful degradation

## Test Scenarios Covered

### Positive Test Cases
1. **Successful Read Operations**
   - Single register reads
   - Multi-register batch reads
   - All supported register types

2. **Successful Write Operations**
   - Single register writes
   - Value validation
   - Confirmation reads

3. **Normal Operation Flow**
   - Continuous polling
   - Data storage
   - Statistics collection

### Negative Test Cases
1. **Communication Failures**
   - Network timeouts
   - Connection refused
   - DNS resolution failures

2. **Protocol Errors**
   - Invalid CRC checksums
   - Malformed frames
   - Unsupported function codes

3. **Server Error Responses**
   - HTTP error codes
   - Modbus exception codes
   - Invalid JSON responses

### Edge Cases
1. **Boundary Conditions**
   - Maximum register values
   - Minimum register values
   - Large batch operations

2. **Resource Constraints**
   - Memory limitations
   - Storage capacity
   - Concurrent access

3. **Timing Scenarios**
   - Rapid polling intervals
   - Extended timeouts
   - Clock synchronization

## Performance Benchmarks

### Response Time Targets
- Single register read: < 100ms
- Batch register read: < 200ms
- Write operation: < 150ms
- Error recovery: < 500ms

### Throughput Expectations
- Polling rate: 5-second intervals (configurable)
- Concurrent operations: Up to 10 simultaneous
- Data storage: 1000+ samples/minute
- Memory usage: < 50MB steady state

### Reliability Metrics
- Uptime: 99.9% availability target
- Error rate: < 1% under normal conditions
- Recovery time: < 30 seconds from failures
- Data integrity: 100% accuracy requirement

## Debugging and Troubleshooting

### Log Analysis
Tests generate detailed logs for debugging:
- Communication traces
- Error condition details
- Performance metrics
- State transitions

### Common Issues
1. **Test Timeouts**: Increase timeout values in configuration
2. **Network Dependencies**: Ensure Inverter SIM server is running
3. **Port Conflicts**: Verify no other services using test ports
4. **Permission Issues**: Run tests with appropriate privileges

### Mock Server Setup
For isolated testing without external dependencies:
```cpp
// Use mock implementations provided in test files
MockProtocolAdapter mock_adapter;
MockDataStorage mock_storage;
// Configure expectations as needed
```

## Continuous Integration

### Automated Testing
Tests are designed for CI/CD pipelines:
- No external dependencies (when using mocks)
- Deterministic results
- Parallel execution support
- Machine-readable output formats

### Quality Gates
- All tests must pass before deployment
- Code coverage > 90%
- Performance benchmarks met
- Memory leak detection clean

## Contributing

### Adding New Tests
1. Follow existing naming conventions
2. Include comprehensive documentation
3. Cover both positive and negative cases
4. Add performance benchmarks where relevant
5. Update this README with new test descriptions

### Test Standards
- Use Google Test framework
- Include mock implementations
- Provide clear test descriptions
- Add timeout protections
- Validate all assertions

## Test Results Documentation

Test results include:
- Pass/fail status for each test case
- Performance metrics and benchmarks
- Coverage reports
- Error details and stack traces
- Resource usage statistics

For detailed test reports, run with verbose output:
```powershell
ctest --output-on-failure --verbose
```

This comprehensive test suite ensures the EcoWatt Device correctly handles all API messages and operates reliably in production environments.
