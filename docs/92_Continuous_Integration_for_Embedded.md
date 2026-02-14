# Continuous Integration for Embedded Systems with FreeRTOS

## Overview

Continuous Integration (CI) for embedded systems extends traditional CI/CD practices to the unique challenges of firmware development. Unlike software running on servers or desktops, embedded systems require specialized testing approaches including cross-compilation, hardware simulation, and hardware-in-the-loop (HIL) testing. For FreeRTOS projects, CI pipelines automate building for multiple targets, run unit tests, perform static analysis, and validate firmware on actual or simulated hardware.

## Key Concepts

### CI/CD Pipeline Components

1. **Version Control Integration**: Triggers on commits, pull requests, or tags
2. **Cross-Compilation**: Building for ARM Cortex-M, RISC-V, and other embedded targets
3. **Automated Testing**: Unit tests, integration tests, and HIL tests
4. **Static Analysis**: Code quality checks, MISRA compliance, stack analysis
5. **Artifact Management**: Firmware binaries, test reports, coverage data
6. **Hardware Simulation**: QEMU, Renode, or vendor-specific simulators
7. **HIL Testing**: Running tests on actual hardware connected to CI runners

### Testing Strategies

- **Unit Testing**: Testing individual RTOS tasks and modules in isolation
- **Integration Testing**: Validating task interactions, queue communications, and semaphore behavior
- **Simulation Testing**: Running complete firmware in emulators
- **HIL Testing**: Deploying to real hardware and executing automated test suites

## C Implementation Examples

### 1. GitHub Actions CI Configuration

```yaml
# .github/workflows/freertos-ci.yml
name: FreeRTOS CI Pipeline

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [stm32f4, esp32, nrf52]
        build_type: [Debug, Release]
    
    steps:
    - uses: actions/checkout@v3
      with:
        submodules: recursive
    
    - name: Install ARM GCC
      run: |
        sudo apt-get update
        sudo apt-get install -y gcc-arm-none-eabi
    
    - name: Install CMake
      uses: jwlawson/actions-setup-cmake@v1.13
    
    - name: Configure Build
      run: |
        mkdir build
        cd build
        cmake -DTARGET=${{ matrix.target }} \
              -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} ..
    
    - name: Build Firmware
      run: cmake --build build --parallel
    
    - name: Upload Artifacts
      uses: actions/upload-artifact@v3
      with:
        name: firmware-${{ matrix.target }}-${{ matrix.build_type }}
        path: build/*.bin

  unit-test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Install Unity Test Framework
      run: |
        git clone https://github.com/ThrowTheSwitch/Unity.git
        cd Unity && mkdir build && cd build
        cmake .. && make && sudo make install
    
    - name: Run Unit Tests
      run: |
        mkdir build-test && cd build-test
        cmake -DBUILD_TESTS=ON ..
        make
        ctest --output-on-failure

  static-analysis:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Run Cppcheck
      run: |
        sudo apt-get install -y cppcheck
        cppcheck --enable=all --suppress=missingInclude \
                 --error-exitcode=1 src/
    
    - name: Check Coding Standards
      run: |
        pip install cpplint
        cpplint --recursive src/

  hardware-test:
    runs-on: [self-hosted, stm32]
    needs: build
    steps:
    - uses: actions/checkout@v3
    
    - name: Download Firmware
      uses: actions/download-artifact@v3
      with:
        name: firmware-stm32f4-Release
    
    - name: Flash Hardware
      run: |
        openocd -f interface/stlink.cfg \
                -f target/stm32f4x.cfg \
                -c "program firmware.bin 0x08000000 verify reset exit"
    
    - name: Run HIL Tests
      run: python3 scripts/hil_test.py --port /dev/ttyUSB0
```

### 2. FreeRTOS Task with Test Hooks

```c
// src/sensor_task.c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "sensor_task.h"

// Dependency injection for testing
static SensorReadFunc sensor_read_fn = NULL;
static QueueHandle_t sensor_queue = NULL;

void sensor_task_init(QueueHandle_t queue, SensorReadFunc read_fn) {
    sensor_queue = queue;
    sensor_read_fn = read_fn ? read_fn : sensor_read_hardware;
}

void sensor_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms
    
    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        SensorData data;
        if (sensor_read_fn && sensor_read_fn(&data)) {
            xQueueSend(sensor_queue, &data, portMAX_DELAY);
        }
    }
}

// Production implementation
bool sensor_read_hardware(SensorData *data) {
    // Read from actual I2C/SPI sensor
    data->temperature = read_temp_sensor();
    data->humidity = read_humidity_sensor();
    data->timestamp = xTaskGetTickCount();
    return true;
}
```

### 3. Unit Test Using Unity Framework

```c
// test/test_sensor_task.c
#include "unity.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "sensor_task.h"

static QueueHandle_t test_queue;
static int mock_read_count = 0;

// Mock sensor read function
bool mock_sensor_read(SensorData *data) {
    mock_read_count++;
    data->temperature = 25.0f;
    data->humidity = 60.0f;
    data->timestamp = mock_read_count;
    return true;
}

void setUp(void) {
    test_queue = xQueueCreate(10, sizeof(SensorData));
    mock_read_count = 0;
    sensor_task_init(test_queue, mock_sensor_read);
}

void tearDown(void) {
    if (test_queue) {
        vQueueDelete(test_queue);
    }
}

void test_sensor_task_sends_data_to_queue(void) {
    // Create and run task briefly
    TaskHandle_t task_handle;
    xTaskCreate(sensor_task, "Sensor", 512, NULL, 1, &task_handle);
    
    // Let task run for a short time
    vTaskDelay(pdMS_TO_TICKS(250));
    
    // Verify data was queued
    SensorData received;
    BaseType_t result = xQueueReceive(test_queue, &received, 0);
    
    TEST_ASSERT_EQUAL(pdTRUE, result);
    TEST_ASSERT_EQUAL_FLOAT(25.0f, received.temperature);
    TEST_ASSERT_EQUAL_FLOAT(60.0f, received.humidity);
    TEST_ASSERT_GREATER_THAN(0, mock_read_count);
    
    vTaskDelete(task_handle);
}

void test_sensor_queue_overflow_handling(void) {
    // Fill queue completely
    SensorData dummy = {0};
    for (int i = 0; i < 10; i++) {
        xQueueSend(test_queue, &dummy, 0);
    }
    
    // Verify queue is full
    TEST_ASSERT_EQUAL(0, uxQueueSpacesAvailable(test_queue));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_task_sends_data_to_queue);
    RUN_TEST(test_sensor_queue_overflow_handling);
    return UNITY_END();
}
```

### 4. CMake Build Configuration for Testing

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(FreeRTOS_Project C)

set(CMAKE_C_STANDARD 11)

# FreeRTOS sources
set(FREERTOS_DIR ${CMAKE_SOURCE_DIR}/FreeRTOS)
set(FREERTOS_SOURCES
    ${FREERTOS_DIR}/tasks.c
    ${FREERTOS_DIR}/queue.c
    ${FREERTOS_DIR}/list.c
    ${FREERTOS_DIR}/timers.c
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/port.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
)

# Production build
if(NOT BUILD_TESTS)
    add_executable(firmware
        src/main.c
        src/sensor_task.c
        ${FREERTOS_SOURCES}
    )
    
    target_include_directories(firmware PRIVATE
        ${FREERTOS_DIR}/include
        ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
        inc
    )
    
    # ARM Cortex-M4 specific flags
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m4 -mthumb -mfloat-abi=hard")
    
# Test build
else()
    find_package(Unity REQUIRED)
    
    # Use native compiler for host testing
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DTEST_BUILD")
    
    add_executable(test_sensor
        test/test_sensor_task.c
        src/sensor_task.c
        test/mocks/FreeRTOS_mock.c  # Mock FreeRTOS functions
    )
    
    target_link_libraries(test_sensor Unity::unity)
    target_include_directories(test_sensor PRIVATE
        inc
        test/mocks
    )
    
    enable_testing()
    add_test(NAME SensorTests COMMAND test_sensor)
endif()
```

### 5. Hardware-in-the-Loop Test Script

```c
// scripts/hil_test_firmware.c
#include "FreeRTOS.h"
#include "task.h"
#include "uart_test_interface.h"
#include <string.h>

typedef struct {
    const char *command;
    bool (*handler)(const char *args);
} TestCommand;

static bool cmd_led_test(const char *args) {
    int led_num = atoi(args);
    gpio_toggle(led_num);
    uart_send("LED_OK\n");
    return true;
}

static bool cmd_task_stats(const char *args) {
    TaskStatus_t *tasks;
    UBaseType_t num_tasks = uxTaskGetNumberOfTasks();
    
    tasks = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
    if (!tasks) return false;
    
    num_tasks = uxTaskGetSystemState(tasks, num_tasks, NULL);
    
    for (UBaseType_t i = 0; i < num_tasks; i++) {
        uart_printf("TASK:%s,STATE:%d,STACK:%u\n",
                    tasks[i].pcTaskName,
                    tasks[i].eCurrentState,
                    tasks[i].usStackHighWaterMark);
    }
    
    vPortFree(tasks);
    uart_send("STATS_OK\n");
    return true;
}

static bool cmd_queue_test(const char *args) {
    extern QueueHandle_t sensor_queue;
    UBaseType_t waiting = uxQueueMessagesWaiting(sensor_queue);
    UBaseType_t spaces = uxQueueSpacesAvailable(sensor_queue);
    
    uart_printf("QUEUE:waiting=%u,free=%u\n", waiting, spaces);
    uart_send("QUEUE_OK\n");
    return true;
}

static const TestCommand test_commands[] = {
    {"LED", cmd_led_test},
    {"STATS", cmd_task_stats},
    {"QUEUE", cmd_queue_test},
};

void test_interface_task(void *pvParameters) {
    char cmd_buffer[128];
    
    for (;;) {
        if (uart_read_line(cmd_buffer, sizeof(cmd_buffer))) {
            char *space = strchr(cmd_buffer, ' ');
            char *args = "";
            
            if (space) {
                *space = '\0';
                args = space + 1;
            }
            
            bool handled = false;
            for (size_t i = 0; i < sizeof(test_commands)/sizeof(test_commands[0]); i++) {
                if (strcmp(cmd_buffer, test_commands[i].command) == 0) {
                    test_commands[i].handler(args);
                    handled = true;
                    break;
                }
            }
            
            if (!handled) {
                uart_send("ERROR:UNKNOWN_CMD\n");
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## C++ Implementation Examples

### 6. GoogleTest Framework Integration

```cpp
// test/test_task_wrapper.cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
extern "C" {
    #include "FreeRTOS.h"
    #include "task.h"
    #include "queue.h"
}

class MockSensorHardware {
public:
    MOCK_METHOD(float, readTemperature, ());
    MOCK_METHOD(float, readHumidity, ());
};

class SensorTaskTest : public ::testing::Test {
protected:
    QueueHandle_t test_queue;
    MockSensorHardware mock_sensor;
    
    void SetUp() override {
        test_queue = xQueueCreate(10, sizeof(SensorData));
        ASSERT_NE(nullptr, test_queue);
    }
    
    void TearDown() override {
        if (test_queue) {
            vQueueDelete(test_queue);
        }
    }
};

TEST_F(SensorTaskTest, TaskCreationSucceeds) {
    TaskHandle_t task_handle = nullptr;
    
    BaseType_t result = xTaskCreate(
        sensor_task,
        "Sensor",
        512,
        nullptr,
        1,
        &task_handle
    );
    
    EXPECT_EQ(pdPASS, result);
    EXPECT_NE(nullptr, task_handle);
    
    if (task_handle) {
        vTaskDelete(task_handle);
    }
}

TEST_F(SensorTaskTest, SensorDataFlowsToQueue) {
    using ::testing::Return;
    
    EXPECT_CALL(mock_sensor, readTemperature())
        .WillOnce(Return(23.5f));
    EXPECT_CALL(mock_sensor, readHumidity())
        .WillOnce(Return(55.0f));
    
    // Inject mock and run task
    // ... task execution logic ...
    
    SensorData received;
    BaseType_t result = xQueueReceive(test_queue, &received, pdMS_TO_TICKS(500));
    
    EXPECT_EQ(pdTRUE, result);
    EXPECT_FLOAT_EQ(23.5f, received.temperature);
    EXPECT_FLOAT_EQ(55.0f, received.humidity);
}
```

### 7. RAII Wrapper for FreeRTOS Resources

```cpp
// inc/rtos_wrapper.hpp
#pragma once
#include <memory>
extern "C" {
    #include "FreeRTOS.h"
    #include "queue.h"
    #include "semphr.h"
}

namespace rtos {

class Queue {
    QueueHandle_t handle;
    
public:
    Queue(UBaseType_t length, UBaseType_t item_size) {
        handle = xQueueCreate(length, item_size);
        if (!handle) {
            throw std::runtime_error("Queue creation failed");
        }
    }
    
    ~Queue() {
        if (handle) {
            vQueueDelete(handle);
        }
    }
    
    // Prevent copying
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    
    bool send(const void *item, TickType_t wait = portMAX_DELAY) {
        return xQueueSend(handle, item, wait) == pdTRUE;
    }
    
    bool receive(void *buffer, TickType_t wait = portMAX_DELAY) {
        return xQueueReceive(handle, buffer, wait) == pdTRUE;
    }
    
    UBaseType_t messagesWaiting() const {
        return uxQueueMessagesWaiting(handle);
    }
    
    QueueHandle_t native() { return handle; }
};

} // namespace rtos
```

## Rust Implementation Examples

### 8. FreeRTOS Bindings with Rust

```rust
// src/freertos_bindings.rs
use std::ffi::c_void;
use std::ptr;

// FFI bindings to FreeRTOS C API
extern "C" {
    fn xQueueCreate(uxQueueLength: u32, uxItemSize: u32) -> *mut c_void;
    fn vQueueDelete(xQueue: *mut c_void);
    fn xQueueSend(xQueue: *mut c_void, pvItemToQueue: *const c_void, xTicksToWait: u32) -> i32;
    fn xQueueReceive(xQueue: *mut c_void, pvBuffer: *mut c_void, xTicksToWait: u32) -> i32;
    fn xTaskCreate(
        pxTaskCode: extern "C" fn(*mut c_void),
        pcName: *const i8,
        usStackDepth: u16,
        pvParameters: *mut c_void,
        uxPriority: u32,
        pxCreatedTask: *mut *mut c_void,
    ) -> i32;
}

pub struct Queue<T> {
    handle: *mut c_void,
    _phantom: std::marker::PhantomData<T>,
}

impl<T> Queue<T> {
    pub fn new(length: u32) -> Option<Self> {
        let handle = unsafe { xQueueCreate(length, std::mem::size_of::<T>() as u32) };
        
        if handle.is_null() {
            None
        } else {
            Some(Queue {
                handle,
                _phantom: std::marker::PhantomData,
            })
        }
    }
    
    pub fn send(&self, item: &T, ticks_to_wait: u32) -> bool {
        let result = unsafe {
            xQueueSend(
                self.handle,
                item as *const T as *const c_void,
                ticks_to_wait,
            )
        };
        result != 0
    }
    
    pub fn receive(&self, ticks_to_wait: u32) -> Option<T> {
        let mut item = std::mem::MaybeUninit::<T>::uninit();
        
        let result = unsafe {
            xQueueReceive(
                self.handle,
                item.as_mut_ptr() as *mut c_void,
                ticks_to_wait,
            )
        };
        
        if result != 0 {
            Some(unsafe { item.assume_init() })
        } else {
            None
        }
    }
}

impl<T> Drop for Queue<T> {
    fn drop(&mut self) {
        unsafe { vQueueDelete(self.handle) };
    }
}

unsafe impl<T: Send> Send for Queue<T> {}
unsafe impl<T: Send> Sync for Queue<T> {}
```

### 9. Rust Task with Testing Support

```rust
// src/sensor_task.rs
use crate::freertos_bindings::Queue;
use core::ffi::c_void;

#[repr(C)]
pub struct SensorData {
    pub temperature: f32,
    pub humidity: f32,
    pub timestamp: u32,
}

pub trait SensorReader {
    fn read(&self) -> Option<SensorData>;
}

pub struct HardwareSensor;

impl SensorReader for HardwareSensor {
    fn read(&self) -> Option<SensorData> {
        // Read from actual hardware
        Some(SensorData {
            temperature: read_i2c_temperature(),
            humidity: read_i2c_humidity(),
            timestamp: get_tick_count(),
        })
    }
}

pub struct SensorTask<R: SensorReader> {
    sensor: R,
    queue: Queue<SensorData>,
}

impl<R: SensorReader> SensorTask<R> {
    pub fn new(sensor: R, queue_length: u32) -> Option<Self> {
        Queue::new(queue_length).map(|queue| SensorTask { sensor, queue })
    }
    
    pub fn run(&self) {
        loop {
            if let Some(data) = self.sensor.read() {
                self.queue.send(&data, u32::MAX);
            }
            
            // Delay 100ms
            delay_ms(100);
        }
    }
}

// Mock sensor for testing
#[cfg(test)]
pub struct MockSensor {
    pub return_value: Option<SensorData>,
}

#[cfg(test)]
impl SensorReader for MockSensor {
    fn read(&self) -> Option<SensorData> {
        self.return_value.clone()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_sensor_task_sends_data() {
        let mock_sensor = MockSensor {
            return_value: Some(SensorData {
                temperature: 25.0,
                humidity: 60.0,
                timestamp: 100,
            }),
        };
        
        let task = SensorTask::new(mock_sensor, 10).unwrap();
        
        // Simulate one iteration
        if let Some(data) = task.sensor.read() {
            assert!(task.queue.send(&data, 0));
        }
        
        // Verify data in queue
        let received = task.queue.receive(0).unwrap();
        assert_eq!(received.temperature, 25.0);
        assert_eq!(received.humidity, 60.0);
    }
}

// External C functions (implemented elsewhere)
extern "C" {
    fn read_i2c_temperature() -> f32;
    fn read_i2c_humidity() -> f32;
    fn get_tick_count() -> u32;
    fn delay_ms(ms: u32);
}
```

### 10. Cargo Configuration for Embedded Rust

```toml
# Cargo.toml
[package]
name = "freertos-embedded"
version = "0.1.0"
edition = "2021"

[dependencies]
# No std for embedded
cortex-m = "0.7"
cortex-m-rt = "0.7"

[dev-dependencies]
# Testing dependencies (only for host)
mockall = "0.11"

[profile.release]
opt-level = "z"     # Optimize for size
lto = true          # Link-time optimization
codegen-units = 1   # Better optimization
debug = true        # Keep debug symbols

[profile.dev]
opt-level = 1       # Some optimization for embedded

# Build configuration
[build]
target = "thumbv7em-none-eabihf"  # ARM Cortex-M4F

[[bin]]
name = "firmware"
test = false
bench = false
```

### 11. Rust CI Configuration

```yaml
# .github/workflows/rust-ci.yml
name: Rust Embedded CI

on: [push, pull_request]

jobs:
  check:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install Rust
        uses: actions-rs/toolchain@v1
        with:
          profile: minimal
          toolchain: stable
          target: thumbv7em-none-eabihf
          components: rustfmt, clippy
      
      - name: Check formatting
        run: cargo fmt -- --check
      
      - name: Run clippy
        run: cargo clippy --target thumbv7em-none-eabihf -- -D warnings
      
      - name: Build firmware
        run: cargo build --release --target thumbv7em-none-eabihf
      
      - name: Run host tests
        run: cargo test --lib

  test-coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install tarpaulin
        run: cargo install cargo-tarpaulin
      
      - name: Generate coverage
        run: cargo tarpaulin --out Xml --output-dir coverage
      
      - name: Upload coverage
        uses: codecov/codecov-action@v3
        with:
          files: coverage/cobertura.xml
```

---

## Summary

**Continuous Integration for Embedded FreeRTOS Projects** brings modern DevOps practices to firmware development, addressing unique challenges like cross-compilation, hardware dependencies, and real-time constraints. Effective CI pipelines combine multiple testing strategies: unit tests with dependency injection and mocking frameworks (Unity for C, GoogleTest for C++, native test harness for Rust), simulation-based testing using QEMU or Renode, and hardware-in-the-loop testing with actual devices connected to CI runners.

Key implementation principles include designing code with testability in mind through dependency injection, using build systems (CMake, Cargo) that support both embedded and host targets, implementing automated flashing and test protocols for HIL testing, and maintaining comprehensive static analysis for code quality and safety compliance. Modern tooling supports building for multiple targets in parallel, generating test coverage reports, and detecting issues early in development.

The examples demonstrate complete CI workflows in C (with CMake and Unity), C++ (with GoogleTest), and Rust (with Cargo and native testing), showing how to structure projects for automated building, testing artifact management, and continuous quality assurance in resource-constrained embedded environments.