# Integration Testing Strategies for FreeRTOS

## Overview

Integration testing in FreeRTOS focuses on verifying that multiple tasks, queues, semaphores, and other RTOS components work correctly together as a system. Unlike unit testing which isolates individual functions, integration testing validates the interactions between components, timing behavior, and real-world multi-tasking scenarios.

## Key Concepts

### 1. Task Interaction Testing
Testing how tasks communicate through queues, semaphores, mutexes, and event groups to ensure proper synchronization and data flow.

### 2. Timing Constraint Verification
Validating that tasks meet their deadlines, that priority inversion is handled correctly, and that the system responds within acceptable time bounds.

### 3. Test Harnesses
Creating specialized environments that can inject faults, monitor task behavior, and verify system state without disrupting normal operation.

### 4. Mock Tasks and Stubs
Replacing hardware dependencies or complex subsystems with controllable test components.

---

## C/C++ Implementation

### Basic Test Harness Framework

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdio.h>
#include <stdbool.h>

// Test result tracking
typedef struct {
    const char* testName;
    bool passed;
    uint32_t timestamp;
    const char* message;
} TestResult;

#define MAX_TEST_RESULTS 50
static TestResult testResults[MAX_TEST_RESULTS];
static uint32_t testCount = 0;
static SemaphoreHandle_t testMutex;

// Test assertion macro
#define TEST_ASSERT(condition, message) \
    do { \
        xSemaphoreTake(testMutex, portMAX_DELAY); \
        if (testCount < MAX_TEST_RESULTS) { \
            testResults[testCount].testName = __func__; \
            testResults[testCount].passed = (condition); \
            testResults[testCount].timestamp = xTaskGetTickCount(); \
            testResults[testCount].message = message; \
            testCount++; \
        } \
        xSemaphoreGive(testMutex); \
    } while(0)

// Initialize test framework
void initTestFramework(void) {
    testMutex = xSemaphoreCreateMutex();
    testCount = 0;
}

// Print test results
void printTestResults(void) {
    printf("\n=== Test Results ===\n");
    uint32_t passed = 0;
    for (uint32_t i = 0; i < testCount; i++) {
        printf("[%s] %s @ %lu: %s\n",
               testResults[i].passed ? "PASS" : "FAIL",
               testResults[i].testName,
               testResults[i].timestamp,
               testResults[i].message);
        if (testResults[i].passed) passed++;
    }
    printf("\nTotal: %lu/%lu passed\n", passed, testCount);
}
```

### Testing Queue Communication Between Tasks

```c
// Shared test data
static QueueHandle_t testQueue;
static volatile uint32_t producerCount = 0;
static volatile uint32_t consumerCount = 0;

// Producer task
void testProducerTask(void* params) {
    uint32_t value = 0;
    TickType_t startTime = xTaskGetTickCount();
    
    for (int i = 0; i < 100; i++) {
        value = i;
        if (xQueueSend(testQueue, &value, pdMS_TO_TICKS(100)) == pdPASS) {
            producerCount++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    TickType_t duration = xTaskGetTickCount() - startTime;
    TEST_ASSERT(producerCount == 100, "Producer sent all messages");
    TEST_ASSERT(duration < pdMS_TO_TICKS(1500), "Producer completed in time");
    
    vTaskDelete(NULL);
}

// Consumer task
void testConsumerTask(void* params) {
    uint32_t value;
    uint32_t expectedValue = 0;
    TickType_t startTime = xTaskGetTickCount();
    
    while (consumerCount < 100) {
        if (xQueueReceive(testQueue, &value, pdMS_TO_TICKS(200)) == pdPASS) {
            TEST_ASSERT(value == expectedValue, "Received correct value");
            consumerCount++;
            expectedValue++;
        }
    }
    
    TickType_t duration = xTaskGetTickCount() - startTime;
    TEST_ASSERT(consumerCount == 100, "Consumer received all messages");
    TEST_ASSERT(duration < pdMS_TO_TICKS(2000), "Consumer completed in time");
    
    vTaskDelete(NULL);
}

// Test runner
void testQueueCommunication(void) {
    testQueue = xQueueCreate(10, sizeof(uint32_t));
    producerCount = 0;
    consumerCount = 0;
    
    xTaskCreate(testProducerTask, "Producer", 256, NULL, 2, NULL);
    xTaskCreate(testConsumerTask, "Consumer", 256, NULL, 2, NULL);
    
    // Monitor task will verify completion
}
```

### Testing Mutex and Priority Inversion

```c
static SemaphoreHandle_t testMutex2;
static volatile bool highPriorityRan = false;
static volatile bool mediumPriorityRan = false;
static volatile bool lowPriorityRan = false;
static volatile TickType_t highPriorityStartTime = 0;
static volatile TickType_t highPriorityEndTime = 0;

void lowPriorityTask(void* params) {
    printf("Low priority task started\n");
    
    // Acquire mutex
    xSemaphoreTake(testMutex2, portMAX_DELAY);
    printf("Low priority task holding mutex\n");
    lowPriorityRan = true;
    
    // Simulate work while holding mutex
    vTaskDelay(pdMS_TO_TICKS(100));
    
    xSemaphoreGive(testMutex2);
    printf("Low priority task released mutex\n");
    
    vTaskDelete(NULL);
}

void mediumPriorityTask(void* params) {
    vTaskDelay(pdMS_TO_TICKS(20)); // Let low priority start
    
    printf("Medium priority task running\n");
    mediumPriorityRan = true;
    
    // Busy work (doesn't need mutex)
    vTaskDelay(pdMS_TO_TICKS(50));
    
    vTaskDelete(NULL);
}

void highPriorityTask(void* params) {
    vTaskDelay(pdMS_TO_TICKS(30)); // Let low priority acquire mutex
    
    highPriorityStartTime = xTaskGetTickCount();
    printf("High priority task trying to get mutex\n");
    highPriorityRan = true;
    
    // This will block until low priority releases
    xSemaphoreTake(testMutex2, portMAX_DELAY);
    highPriorityEndTime = xTaskGetTickCount();
    
    TickType_t waitTime = highPriorityEndTime - highPriorityStartTime;
    
    // With priority inheritance, high priority should not be blocked
    // excessively by medium priority task
    TEST_ASSERT(waitTime < pdMS_TO_TICKS(150), 
                "Priority inheritance prevented excessive blocking");
    
    xSemaphoreGive(testMutex2);
    
    vTaskDelete(NULL);
}

void testPriorityInheritance(void) {
    // Use mutex with priority inheritance
    testMutex2 = xSemaphoreCreateMutex();
    
    xTaskCreate(lowPriorityTask, "LowPri", 256, NULL, 1, NULL);
    xTaskCreate(mediumPriorityTask, "MedPri", 256, NULL, 2, NULL);
    xTaskCreate(highPriorityTask, "HighPri", 256, NULL, 3, NULL);
}
```

### Timing Constraint Verification

```c
typedef struct {
    const char* taskName;
    TickType_t expectedPeriod;
    TickType_t tolerance;
    TickType_t lastRun;
    uint32_t runCount;
    uint32_t violations;
} TimingMonitor;

#define MAX_MONITORED_TASKS 10
static TimingMonitor monitors[MAX_MONITORED_TASKS];
static uint32_t monitorCount = 0;

// Register a task for timing monitoring
void registerTimingMonitor(const char* name, TickType_t period, TickType_t tolerance) {
    if (monitorCount < MAX_MONITORED_TASKS) {
        monitors[monitorCount].taskName = name;
        monitors[monitorCount].expectedPeriod = period;
        monitors[monitorCount].tolerance = tolerance;
        monitors[monitorCount].lastRun = 0;
        monitors[monitorCount].runCount = 0;
        monitors[monitorCount].violations = 0;
        monitorCount++;
    }
}

// Call this at the start of each monitored task iteration
void recordTaskExecution(const char* taskName) {
    TickType_t now = xTaskGetTickCount();
    
    for (uint32_t i = 0; i < monitorCount; i++) {
        if (strcmp(monitors[i].taskName, taskName) == 0) {
            if (monitors[i].lastRun != 0) {
                TickType_t actualPeriod = now - monitors[i].lastRun;
                TickType_t deviation = (actualPeriod > monitors[i].expectedPeriod) ?
                    (actualPeriod - monitors[i].expectedPeriod) :
                    (monitors[i].expectedPeriod - actualPeriod);
                
                if (deviation > monitors[i].tolerance) {
                    monitors[i].violations++;
                    printf("Timing violation in %s: expected %lu, got %lu\n",
                           taskName, monitors[i].expectedPeriod, actualPeriod);
                }
            }
            monitors[i].lastRun = now;
            monitors[i].runCount++;
            break;
        }
    }
}

// Example periodic task under test
void periodicTask(void* params) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(100);
    
    for (int i = 0; i < 50; i++) {
        recordTaskExecution("PeriodicTask");
        
        // Do work
        vTaskDelay(pdMS_TO_TICKS(20));
        
        vTaskDelayUntil(&lastWakeTime, period);
    }
    
    vTaskDelete(NULL);
}

void testPeriodicTiming(void) {
    registerTimingMonitor("PeriodicTask", pdMS_TO_TICKS(100), pdMS_TO_TICKS(5));
    xTaskCreate(periodicTask, "PeriodicTask", 256, NULL, 2, NULL);
}
```

---

## Rust Implementation

### Test Framework in Rust

```rust
use freertos_rust::*;
use std::sync::{Arc, Mutex};
use std::time::Duration;

#[derive(Clone)]
struct TestResult {
    test_name: &'static str,
    passed: bool,
    timestamp: u32,
    message: &'static str,
}

struct TestFramework {
    results: Arc<Mutex<Vec<TestResult>>>,
}

impl TestFramework {
    fn new() -> Self {
        TestFramework {
            results: Arc::new(Mutex::new(Vec::new())),
        }
    }
    
    fn assert(&self, test_name: &'static str, condition: bool, message: &'static str) {
        let mut results = self.results.lock().unwrap();
        results.push(TestResult {
            test_name,
            passed: condition,
            timestamp: FreeRtosUtils::get_tick_count(),
            message,
        });
    }
    
    fn print_results(&self) {
        let results = self.results.lock().unwrap();
        println!("\n=== Test Results ===");
        
        let passed = results.iter().filter(|r| r.passed).count();
        
        for result in results.iter() {
            println!(
                "[{}] {} @ {}: {}",
                if result.passed { "PASS" } else { "FAIL" },
                result.test_name,
                result.timestamp,
                result.message
            );
        }
        
        println!("\nTotal: {}/{} passed", passed, results.len());
    }
}
```

### Queue Communication Test in Rust

```rust
fn test_queue_communication(framework: Arc<TestFramework>) {
    let queue: Arc<Queue<u32>> = Arc::new(Queue::new(10).unwrap());
    let queue_producer = queue.clone();
    let queue_consumer = queue.clone();
    
    let framework_producer = framework.clone();
    let framework_consumer = framework.clone();
    
    // Producer task
    Task::new()
        .name("Producer")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || {
            let start_time = FreeRtosUtils::get_tick_count();
            let mut sent_count = 0u32;
            
            for i in 0..100u32 {
                if queue_producer.send(i, Duration::from_millis(100)).is_ok() {
                    sent_count += 1;
                }
                CurrentTask::delay(Duration::from_millis(10));
            }
            
            let duration = FreeRtosUtils::get_tick_count() - start_time;
            
            framework_producer.assert(
                "Producer",
                sent_count == 100,
                "Sent all messages"
            );
            
            framework_producer.assert(
                "Producer",
                duration < FreeRtosUtils::ms_to_ticks(1500),
                "Completed in time"
            );
        })
        .unwrap();
    
    // Consumer task
    Task::new()
        .name("Consumer")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || {
            let start_time = FreeRtosUtils::get_tick_count();
            let mut received_count = 0u32;
            let mut expected_value = 0u32;
            
            while received_count < 100 {
                if let Ok(value) = queue_consumer.receive(Duration::from_millis(200)) {
                    framework_consumer.assert(
                        "Consumer",
                        value == expected_value,
                        "Received correct value"
                    );
                    received_count += 1;
                    expected_value += 1;
                }
            }
            
            let duration = FreeRtosUtils::get_tick_count() - start_time;
            
            framework_consumer.assert(
                "Consumer",
                received_count == 100,
                "Received all messages"
            );
            
            framework_consumer.assert(
                "Consumer",
                duration < FreeRtosUtils::ms_to_ticks(2000),
                "Completed in time"
            );
        })
        .unwrap();
}
```

### Mutex Priority Testing in Rust

```rust
fn test_priority_inheritance(framework: Arc<TestFramework>) {
    let mutex: Arc<Mutex<()>> = Arc::new(Mutex::new(()));
    let mutex_low = mutex.clone();
    let mutex_med = mutex.clone();
    let mutex_high = mutex.clone();
    
    let framework_high = framework.clone();
    
    let high_start = Arc::new(Mutex::new(0u32));
    let high_end = Arc::new(Mutex::new(0u32));
    let high_start_clone = high_start.clone();
    let high_end_clone = high_end.clone();
    
    // Low priority task
    Task::new()
        .name("LowPri")
        .stack_size(2048)
        .priority(TaskPriority(1))
        .start(move || {
            println!("Low priority task started");
            let _guard = mutex_low.lock().unwrap();
            println!("Low priority holding mutex");
            
            CurrentTask::delay(Duration::from_millis(100));
            
            println!("Low priority releasing mutex");
        })
        .unwrap();
    
    // Medium priority task
    Task::new()
        .name("MedPri")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || {
            CurrentTask::delay(Duration::from_millis(20));
            println!("Medium priority running");
            CurrentTask::delay(Duration::from_millis(50));
        })
        .unwrap();
    
    // High priority task
    Task::new()
        .name("HighPri")
        .stack_size(2048)
        .priority(TaskPriority(3))
        .start(move || {
            CurrentTask::delay(Duration::from_millis(30));
            
            *high_start_clone.lock().unwrap() = FreeRtosUtils::get_tick_count();
            println!("High priority trying to get mutex");
            
            let _guard = mutex_high.lock().unwrap();
            
            *high_end_clone.lock().unwrap() = FreeRtosUtils::get_tick_count();
            
            let wait_time = *high_end.lock().unwrap() - *high_start.lock().unwrap();
            
            framework_high.assert(
                "PriorityInheritance",
                wait_time < FreeRtosUtils::ms_to_ticks(150),
                "Priority inheritance prevented excessive blocking"
            );
        })
        .unwrap();
}
```

### Timing Monitor in Rust

```rust
struct TimingMonitor {
    task_name: &'static str,
    expected_period_ms: u32,
    tolerance_ms: u32,
    last_run: Mutex<Option<u32>>,
    run_count: Mutex<u32>,
    violations: Mutex<u32>,
}

impl TimingMonitor {
    fn new(task_name: &'static str, expected_period_ms: u32, tolerance_ms: u32) -> Self {
        TimingMonitor {
            task_name,
            expected_period_ms,
            tolerance_ms,
            last_run: Mutex::new(None),
            run_count: Mutex::new(0),
            violations: Mutex::new(0),
        }
    }
    
    fn record_execution(&self) {
        let now = FreeRtosUtils::get_tick_count();
        let mut last_run = self.last_run.lock().unwrap();
        
        if let Some(last) = *last_run {
            let actual_period = now - last;
            let expected = FreeRtosUtils::ms_to_ticks(self.expected_period_ms);
            
            let deviation = if actual_period > expected {
                actual_period - expected
            } else {
                expected - actual_period
            };
            
            if deviation > FreeRtosUtils::ms_to_ticks(self.tolerance_ms) {
                *self.violations.lock().unwrap() += 1;
                println!(
                    "Timing violation in {}: expected {}, got {}",
                    self.task_name, expected, actual_period
                );
            }
        }
        
        *last_run = Some(now);
        *self.run_count.lock().unwrap() += 1;
    }
    
    fn get_statistics(&self) -> (u32, u32) {
        (*self.run_count.lock().unwrap(), *self.violations.lock().unwrap())
    }
}

fn test_periodic_timing() {
    let monitor = Arc::new(TimingMonitor::new("PeriodicTask", 100, 5));
    let monitor_clone = monitor.clone();
    
    Task::new()
        .name("PeriodicTask")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || {
            for _ in 0..50 {
                monitor_clone.record_execution();
                
                // Do work
                CurrentTask::delay(Duration::from_millis(20));
                
                // Wait for next period
                CurrentTask::delay(Duration::from_millis(100));
            }
        })
        .unwrap();
}
```

---

## Summary

**Integration Testing Strategies for FreeRTOS** involve verifying that multiple RTOS components work together correctly in real-world multi-tasking scenarios. Key elements include:

1. **Task Interaction Testing**: Validating communication through queues, semaphores, and synchronization primitives to ensure proper data flow and coordination between tasks.

2. **Timing Constraint Verification**: Monitoring periodic tasks, measuring response times, and detecting timing violations to ensure the system meets real-time requirements.

3. **Priority Testing**: Verifying that priority inheritance mechanisms work correctly to prevent priority inversion and ensure high-priority tasks aren't blocked excessively.

4. **Test Harnesses**: Creating frameworks that track test results, inject controlled conditions, and monitor system behavior without disrupting normal operation.

5. **Best Practices**:
   - Use assertion macros to automatically log test results
   - Monitor tasks with timing constraints to detect deadline violations
   - Test both normal operation and edge cases (full queues, mutex contention, high load)
   - Create reproducible test scenarios that can run automatically
   - Separate test infrastructure from production code

The examples demonstrate practical approaches in both C/C++ and Rust for building robust integration tests that verify multi-tasking behavior, timing guarantees, and inter-task communication in FreeRTOS applications.