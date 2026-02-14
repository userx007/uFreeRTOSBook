# Unit Testing with CMock and Unity in FreeRTOS

Unit testing embedded systems, particularly those running FreeRTOS, presents unique challenges due to hardware dependencies, real-time constraints, and kernel interactions. CMock and Unity provide a powerful framework for testing FreeRTOS applications in a host environment without requiring target hardware.

## Understanding the Testing Framework

**Unity** is a lightweight unit testing framework written in C, designed specifically for embedded systems. It provides assertion macros and test runners with minimal overhead.

**CMock** is a mock generation tool that works with Unity, automatically creating mock functions from header files. This is crucial for isolating units of code and mocking FreeRTOS kernel functions like task creation, queue operations, and semaphores.

## Architecture and Setup

The typical testing architecture separates production code from test code:

```
project/
├── src/              # Production code
├── test/             # Test code
│   ├── mocks/        # Auto-generated mocks
│   └── tests/        # Test files
├── vendor/
│   ├── unity/
│   └── cmock/
└── CMakeLists.txt
```

## C/C++ Implementation

### Basic Unity Test Structure

```c
// test_task_manager.c
#include "unity.h"
#include "task_manager.h"
#include "mock_FreeRTOS.h"
#include "mock_queue.h"
#include "mock_semphr.h"

// Setup runs before each test
void setUp(void) {
    // Initialize test environment
}

// Teardown runs after each test
void tearDown(void) {
    // Clean up
}

// Example: Testing a sensor data processing task
void test_sensor_data_should_be_validated(void) {
    sensor_data_t test_data = {
        .temperature = 25.5f,
        .humidity = 60.0f,
        .timestamp = 1000
    };
    
    bool result = validate_sensor_data(&test_data);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.5f, test_data.temperature);
}

void test_invalid_sensor_data_should_fail(void) {
    sensor_data_t test_data = {
        .temperature = -999.0f,  // Invalid
        .humidity = 60.0f,
        .timestamp = 1000
    };
    
    bool result = validate_sensor_data(&test_data);
    
    TEST_ASSERT_FALSE(result);
}
```

### Mocking FreeRTOS Queue Operations

```c
// test_queue_handler.c
#include "unity.h"
#include "queue_handler.h"
#include "mock_queue.h"

void test_queue_send_should_succeed(void) {
    QueueHandle_t mock_queue = (QueueHandle_t)0x12345678;
    message_t test_msg = { .id = 1, .data = 42 };
    
    // Set expectation: xQueueSend will be called once and return pdTRUE
    xQueueSend_ExpectAndReturn(mock_queue, &test_msg, pdMS_TO_TICKS(100), pdTRUE);
    
    // Execute function under test
    bool result = send_message_to_queue(mock_queue, &test_msg);
    
    // Verify
    TEST_ASSERT_TRUE(result);
}

void test_queue_send_timeout_should_be_handled(void) {
    QueueHandle_t mock_queue = (QueueHandle_t)0x12345678;
    message_t test_msg = { .id = 1, .data = 42 };
    
    // Simulate timeout
    xQueueSend_ExpectAndReturn(mock_queue, &test_msg, pdMS_TO_TICKS(100), pdFALSE);
    
    bool result = send_message_to_queue(mock_queue, &test_msg);
    
    TEST_ASSERT_FALSE(result);
}

void test_queue_receive_should_process_message(void) {
    QueueHandle_t mock_queue = (QueueHandle_t)0x12345678;
    message_t expected_msg = { .id = 1, .data = 42 };
    message_t received_msg;
    
    // Mock will copy expected_msg to the buffer
    xQueueReceive_ExpectAndReturn(mock_queue, &received_msg, portMAX_DELAY, pdTRUE);
    xQueueReceive_ReturnThruPtr_pvBuffer(&expected_msg);
    
    bool result = receive_and_process_message(mock_queue, &received_msg);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, received_msg.id);
    TEST_ASSERT_EQUAL(42, received_msg.data);
}
```

### Mocking Task Creation and Management

```c
// test_task_creation.c
#include "unity.h"
#include "system_init.h"
#include "mock_task.h"
#include "mock_semphr.h"

void test_system_tasks_should_be_created(void) {
    TaskHandle_t sensor_task_handle = NULL;
    TaskHandle_t process_task_handle = NULL;
    
    // Expect sensor task creation
    xTaskCreate_ExpectAndReturn(
        sensor_task,
        "SensorTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        2,
        &sensor_task_handle,
        pdPASS
    );
    
    // Expect processing task creation
    xTaskCreate_ExpectAndReturn(
        processing_task,
        "ProcessTask",
        configMINIMAL_STACK_SIZE * 2,
        NULL,
        3,
        &process_task_handle,
        pdPASS
    );
    
    // Execute system initialization
    bool result = initialize_system_tasks();
    
    TEST_ASSERT_TRUE(result);
}

void test_task_creation_failure_should_be_handled(void) {
    TaskHandle_t task_handle = NULL;
    
    // Simulate task creation failure
    xTaskCreate_ExpectAndReturn(
        sensor_task,
        "SensorTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        2,
        &task_handle,
        pdFAIL
    );
    
    bool result = initialize_system_tasks();
    
    TEST_ASSERT_FALSE(result);
}
```

### Testing with Semaphores

```c
// test_resource_manager.c
#include "unity.h"
#include "resource_manager.h"
#include "mock_semphr.h"

void test_resource_acquisition_should_succeed(void) {
    SemaphoreHandle_t mock_semaphore = (SemaphoreHandle_t)0xABCDEF01;
    
    // Expect semaphore take
    xSemaphoreTake_ExpectAndReturn(mock_semaphore, pdMS_TO_TICKS(1000), pdTRUE);
    
    bool result = acquire_shared_resource(mock_semaphore);
    
    TEST_ASSERT_TRUE(result);
}

void test_resource_release_should_give_semaphore(void) {
    SemaphoreHandle_t mock_semaphore = (SemaphoreHandle_t)0xABCDEF01;
    
    xSemaphoreGive_ExpectAndReturn(mock_semaphore, pdTRUE);
    
    release_shared_resource(mock_semaphore);
    
    // Verify mock was called (CMock handles this automatically)
}

void test_critical_section_should_protect_data(void) {
    shared_data_t data = { .counter = 0 };
    SemaphoreHandle_t mutex = (SemaphoreHandle_t)0x12345678;
    
    xSemaphoreTake_ExpectAndReturn(mutex, portMAX_DELAY, pdTRUE);
    xSemaphoreGive_ExpectAndReturn(mutex, pdTRUE);
    
    increment_shared_counter(&data, mutex);
    
    TEST_ASSERT_EQUAL(1, data.counter);
}
```

### Complex Test: State Machine with FreeRTOS

```c
// test_state_machine.c
#include "unity.h"
#include "state_machine.h"
#include "mock_task.h"
#include "mock_queue.h"
#include "mock_timers.h"

void test_state_transition_from_idle_to_active(void) {
    state_machine_t sm;
    event_t event = { .type = EVENT_START, .data = 0 };
    QueueHandle_t event_queue = (QueueHandle_t)0x11111111;
    
    // Initialize state machine
    init_state_machine(&sm);
    TEST_ASSERT_EQUAL(STATE_IDLE, sm.current_state);
    
    // Mock queue receive
    xQueueReceive_ExpectAndReturn(event_queue, &event, portMAX_DELAY, pdTRUE);
    xQueueReceive_ReturnThruPtr_pvBuffer(&event);
    
    // Process event
    process_state_machine_event(&sm, event_queue);
    
    TEST_ASSERT_EQUAL(STATE_ACTIVE, sm.current_state);
}

void test_timeout_should_trigger_error_state(void) {
    state_machine_t sm = { .current_state = STATE_ACTIVE };
    event_t timeout_event = { .type = EVENT_TIMEOUT, .data = 0 };
    QueueHandle_t event_queue = (QueueHandle_t)0x11111111;
    
    xQueueReceive_ExpectAndReturn(event_queue, &timeout_event, portMAX_DELAY, pdTRUE);
    xQueueReceive_ReturnThruPtr_pvBuffer(&timeout_event);
    
    process_state_machine_event(&sm, event_queue);
    
    TEST_ASSERT_EQUAL(STATE_ERROR, sm.current_state);
}
```

### CMakeLists.txt Configuration

```cmake
cmake_minimum_required(VERSION 3.15)
project(FreeRTOS_Tests C)

# Unity framework
add_library(unity STATIC
    vendor/unity/src/unity.c
)
target_include_directories(unity PUBLIC vendor/unity/src)

# CMock
add_library(cmock STATIC
    vendor/cmock/src/cmock.c
)
target_include_directories(cmock PUBLIC 
    vendor/cmock/src
    vendor/unity/src
)

# Generate mocks
set(MOCK_DIR ${CMAKE_BINARY_DIR}/mocks)
file(MAKE_DIRECTORY ${MOCK_DIR})

# Mock generation for FreeRTOS headers
add_custom_command(
    OUTPUT ${MOCK_DIR}/mock_task.c ${MOCK_DIR}/mock_task.h
    COMMAND ruby vendor/cmock/lib/cmock.rb -o cmock_config.yml FreeRTOS/Source/include/task.h
    DEPENDS FreeRTOS/Source/include/task.h
)

# Test executable
add_executable(test_runner
    test/test_runner.c
    test/test_task_manager.c
    test/test_queue_handler.c
    ${MOCK_DIR}/mock_task.c
    ${MOCK_DIR}/mock_queue.c
    src/task_manager.c
    src/queue_handler.c
)

target_link_libraries(test_runner unity cmock)
target_include_directories(test_runner PRIVATE src ${MOCK_DIR})
```

## Rust Implementation

Testing FreeRTOS bindings in Rust requires a different approach, typically using trait-based abstraction for mocking.

### Abstracting FreeRTOS APIs

```rust
// freertos_traits.rs
use core::time::Duration;

pub trait QueueOps {
    type Item;
    
    fn send(&self, item: Self::Item, timeout: Duration) -> Result<(), QueueError>;
    fn receive(&self, timeout: Duration) -> Result<Self::Item, QueueError>;
}

pub trait TaskOps {
    fn create(
        name: &str,
        priority: u8,
        stack_size: usize,
        task_fn: fn()
    ) -> Result<TaskHandle, TaskError>;
    
    fn delay(duration: Duration);
    fn suspend(&self);
    fn resume(&self);
}

pub trait SemaphoreOps {
    fn take(&self, timeout: Duration) -> Result<(), SemaphoreError>;
    fn give(&self) -> Result<(), SemaphoreError>;
}

#[derive(Debug, PartialEq)]
pub enum QueueError {
    Timeout,
    Full,
    InvalidHandle,
}

#[derive(Debug)]
pub struct TaskHandle(usize);

#[derive(Debug, PartialEq)]
pub enum TaskError {
    CreationFailed,
    InvalidPriority,
}

#[derive(Debug, PartialEq)]
pub enum SemaphoreError {
    Timeout,
    NotAvailable,
}
```

### Mock Implementations for Testing

```rust
// mock_freertos.rs
use crate::freertos_traits::*;
use std::sync::{Arc, Mutex};
use std::collections::VecDeque;

pub struct MockQueue<T> {
    items: Arc<Mutex<VecDeque<T>>>,
    capacity: usize,
    send_calls: Arc<Mutex<Vec<T>>>,
    receive_calls: Arc<Mutex<usize>>,
}

impl<T: Clone> MockQueue<T> {
    pub fn new(capacity: usize) -> Self {
        Self {
            items: Arc::new(Mutex::new(VecDeque::new())),
            capacity,
            send_calls: Arc::new(Mutex::new(Vec::new())),
            receive_calls: Arc::new(Mutex::new(0)),
        }
    }
    
    pub fn verify_send_called_with(&self, expected: &T) -> bool 
    where T: PartialEq {
        let calls = self.send_calls.lock().unwrap();
        calls.iter().any(|item| item == expected)
    }
    
    pub fn verify_receive_called_times(&self, expected: usize) -> bool {
        *self.receive_calls.lock().unwrap() == expected
    }
}

impl<T: Clone> QueueOps for MockQueue<T> {
    type Item = T;
    
    fn send(&self, item: Self::Item, _timeout: Duration) -> Result<(), QueueError> {
        let mut items = self.items.lock().unwrap();
        let mut calls = self.send_calls.lock().unwrap();
        
        calls.push(item.clone());
        
        if items.len() >= self.capacity {
            return Err(QueueError::Full);
        }
        
        items.push_back(item);
        Ok(())
    }
    
    fn receive(&self, _timeout: Duration) -> Result<Self::Item, QueueError> {
        let mut items = self.items.lock().unwrap();
        let mut calls = self.receive_calls.lock().unwrap();
        
        *calls += 1;
        
        items.pop_front().ok_or(QueueError::Timeout)
    }
}

pub struct MockSemaphore {
    available: Arc<Mutex<bool>>,
    take_calls: Arc<Mutex<usize>>,
    give_calls: Arc<Mutex<usize>>,
}

impl MockSemaphore {
    pub fn new(initially_available: bool) -> Self {
        Self {
            available: Arc::new(Mutex::new(initially_available)),
            take_calls: Arc::new(Mutex::new(0)),
            give_calls: Arc::new(Mutex::new(0)),
        }
    }
    
    pub fn verify_take_called(&self, times: usize) -> bool {
        *self.take_calls.lock().unwrap() == times
    }
    
    pub fn verify_give_called(&self, times: usize) -> bool {
        *self.give_calls.lock().unwrap() == times
    }
}

impl SemaphoreOps for MockSemaphore {
    fn take(&self, _timeout: Duration) -> Result<(), SemaphoreError> {
        let mut available = self.available.lock().unwrap();
        let mut calls = self.take_calls.lock().unwrap();
        
        *calls += 1;
        
        if *available {
            *available = false;
            Ok(())
        } else {
            Err(SemaphoreError::NotAvailable)
        }
    }
    
    fn give(&self) -> Result<(), SemaphoreError> {
        let mut available = self.available.lock().unwrap();
        let mut calls = self.give_calls.lock().unwrap();
        
        *calls += 1;
        *available = true;
        Ok(())
    }
}
```

### Production Code Using Traits

```rust
// sensor_manager.rs
use crate::freertos_traits::*;
use core::time::Duration;

#[derive(Clone, Debug, PartialEq)]
pub struct SensorData {
    pub temperature: f32,
    pub humidity: f32,
    pub timestamp: u64,
}

pub struct SensorManager<Q: QueueOps<Item = SensorData>> {
    data_queue: Q,
}

impl<Q: QueueOps<Item = SensorData>> SensorManager<Q> {
    pub fn new(queue: Q) -> Self {
        Self { data_queue: queue }
    }
    
    pub fn publish_reading(&self, data: SensorData) -> Result<(), QueueError> {
        if !self.validate_data(&data) {
            return Err(QueueError::InvalidHandle);
        }
        
        self.data_queue.send(data, Duration::from_millis(100))
    }
    
    pub fn get_reading(&self) -> Result<SensorData, QueueError> {
        self.data_queue.receive(Duration::from_millis(1000))
    }
    
    fn validate_data(&self, data: &SensorData) -> bool {
        data.temperature >= -40.0 && data.temperature <= 125.0 &&
        data.humidity >= 0.0 && data.humidity <= 100.0
    }
}

pub struct ResourceGuard<'a, S: SemaphoreOps> {
    semaphore: &'a S,
}

impl<'a, S: SemaphoreOps> ResourceGuard<'a, S> {
    pub fn acquire(semaphore: &'a S, timeout: Duration) -> Result<Self, SemaphoreError> {
        semaphore.take(timeout)?;
        Ok(Self { semaphore })
    }
}

impl<'a, S: SemaphoreOps> Drop for ResourceGuard<'a, S> {
    fn drop(&mut self) {
        let _ = self.semaphore.give();
    }
}
```

### Unit Tests in Rust

```rust
// tests/sensor_manager_tests.rs
#[cfg(test)]
mod tests {
    use super::*;
    use crate::mock_freertos::*;
    use core::time::Duration;
    
    #[test]
    fn test_valid_sensor_data_should_be_published() {
        let queue = MockQueue::new(10);
        let manager = SensorManager::new(queue);
        
        let data = SensorData {
            temperature: 25.5,
            humidity: 60.0,
            timestamp: 1000,
        };
        
        let result = manager.publish_reading(data.clone());
        
        assert!(result.is_ok());
        assert!(manager.data_queue.verify_send_called_with(&data));
    }
    
    #[test]
    fn test_invalid_temperature_should_fail() {
        let queue = MockQueue::new(10);
        let manager = SensorManager::new(queue);
        
        let data = SensorData {
            temperature: -999.0,  // Invalid
            humidity: 60.0,
            timestamp: 1000,
        };
        
        let result = manager.publish_reading(data);
        
        assert!(result.is_err());
        assert_eq!(result.unwrap_err(), QueueError::InvalidHandle);
    }
    
    #[test]
    fn test_receive_should_return_queued_data() {
        let queue = MockQueue::new(10);
        let expected_data = SensorData {
            temperature: 22.0,
            humidity: 55.0,
            timestamp: 2000,
        };
        
        // Pre-populate queue
        queue.send(expected_data.clone(), Duration::from_millis(100)).unwrap();
        
        let manager = SensorManager::new(queue);
        let result = manager.get_reading();
        
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), expected_data);
    }
    
    #[test]
    fn test_queue_full_should_return_error() {
        let queue = MockQueue::new(1);  // Capacity of 1
        
        // Fill the queue
        let data1 = SensorData {
            temperature: 20.0,
            humidity: 50.0,
            timestamp: 1000,
        };
        queue.send(data1, Duration::from_millis(100)).unwrap();
        
        let manager = SensorManager::new(queue);
        
        let data2 = SensorData {
            temperature: 21.0,
            humidity: 51.0,
            timestamp: 2000,
        };
        
        let result = manager.publish_reading(data2);
        
        assert!(result.is_err());
        assert_eq!(result.unwrap_err(), QueueError::Full);
    }
    
    #[test]
    fn test_resource_guard_should_release_semaphore() {
        let semaphore = MockSemaphore::new(true);
        
        {
            let _guard = ResourceGuard::acquire(&semaphore, Duration::from_millis(100));
            assert!(semaphore.verify_take_called(1));
        } // Guard dropped here
        
        assert!(semaphore.verify_give_called(1));
    }
    
    #[test]
    fn test_semaphore_unavailable_should_fail() {
        let semaphore = MockSemaphore::new(false);
        
        let result = ResourceGuard::acquire(&semaphore, Duration::from_millis(100));
        
        assert!(result.is_err());
        assert_eq!(result.unwrap_err(), SemaphoreError::NotAvailable);
    }
}
```

### Integration Test with Multiple Components

```rust
// tests/integration_tests.rs
#[cfg(test)]
mod integration_tests {
    use super::*;
    
    #[test]
    fn test_sensor_pipeline_with_synchronization() {
        let data_queue = MockQueue::new(5);
        let mutex = MockSemaphore::new(true);
        
        let sensor_mgr = SensorManager::new(data_queue);
        
        // Simulate protected sensor read
        {
            let _guard = ResourceGuard::acquire(&mutex, Duration::from_millis(100)).unwrap();
            
            let data = SensorData {
                temperature: 23.5,
                humidity: 58.0,
                timestamp: 3000,
            };
            
            sensor_mgr.publish_reading(data).unwrap();
        }
        
        // Verify synchronization
        assert!(mutex.verify_take_called(1));
        assert!(mutex.verify_give_called(1));
        
        // Verify data was queued
        let received = sensor_mgr.get_reading().unwrap();
        assert_eq!(received.temperature, 23.5);
    }
}
```

## Summary

**Unity and CMock** provide comprehensive unit testing capabilities for FreeRTOS applications by:

1. **Isolation**: Mocking FreeRTOS kernel functions allows testing application logic without hardware or RTOS dependencies
2. **Automation**: CMock auto-generates mock implementations from header files, reducing manual effort
3. **Verification**: Tests verify both function returns and interaction patterns (calls, parameters, sequences)
4. **TDD Support**: Enables test-driven development even for embedded systems

**Key practices**:
- Mock all FreeRTOS APIs (tasks, queues, semaphores, timers) for true unit isolation
- Use `ExpectAndReturn` for setting expectations on mock calls
- Test both success and failure paths (timeouts, resource exhaustion, errors)
- Separate testable business logic from RTOS-specific code
- In Rust, use trait-based abstraction for dependency injection and mocking

**Benefits**:
- Tests run on host machines (fast feedback loop)
- No hardware required for most application logic testing
- Easier debugging than on-target testing
- Enables continuous integration/automated testing
- Improves code quality through forced modularity

This approach transforms embedded development by bringing modern software engineering practices to real-time systems, significantly improving reliability and maintainability.