# FreeRTOS Version Migration Guide

## Overview

The FreeRTOS Version Migration Guide addresses the critical task of upgrading FreeRTOS applications across major version boundaries, particularly from V9.x to V10.x and beyond. This guide helps developers navigate API changes, configuration updates, and behavioral differences that arise when moving between versions, ensuring smooth transitions while maintaining system stability and functionality.

## Key Concepts

### Major Version Transitions

**V9.x to V10.x Migration**
The transition from FreeRTOS V9.x to V10.x introduced significant changes:
- Static allocation support became standard
- New tickless idle mode enhancements
- Stream buffer and message buffer additions
- MPU (Memory Protection Unit) support improvements
- Standardized naming conventions

**V10.x to V11.x Considerations**
Later versions introduced:
- SMP (Symmetric Multiprocessing) support
- Enhanced kernel structures
- Improved synchronization primitives
- Updated configuration options

### Configuration Changes

Migration typically requires updating `FreeRTOSConfig.h` with new configuration parameters and removing deprecated options. Understanding which settings have changed behavior or default values is crucial.

## C/C++ Programming Examples

### Example 1: Static vs Dynamic Allocation Migration

**Old V9.x Code (Dynamic Only):**
```c
#include "FreeRTOS.h"
#include "task.h"

void vOldTask(void *pvParameters) {
    while(1) {
        // Task work
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void createTask(void) {
    // V9.x - only dynamic allocation
    xTaskCreate(vOldTask, "OldTask", 128, NULL, 1, NULL);
}
```

**New V10.x Code (Supporting Both):**
```c
#include "FreeRTOS.h"
#include "task.h"

void vNewTask(void *pvParameters) {
    while(1) {
        // Task work
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Method 1: Dynamic allocation (backward compatible)
void createTaskDynamic(void) {
    xTaskCreate(vNewTask, "NewTask", 128, NULL, 1, NULL);
}

// Method 2: Static allocation (V10.x feature)
void createTaskStatic(void) {
    static StaticTask_t xTaskBuffer;
    static StackType_t xStack[128];
    
    xTaskCreateStatic(
        vNewTask,
        "NewTask",
        128,
        NULL,
        1,
        xStack,
        &xTaskBuffer
    );
}
```

### Example 2: Queue Creation Migration

**V9.x Approach:**
```c
#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    uint32_t data;
    uint8_t status;
} QueueItem_t;

QueueHandle_t xQueue;

void initializeQueue(void) {
    // V9.x - only dynamic
    xQueue = xQueueCreate(10, sizeof(QueueItem_t));
    
    if(xQueue == NULL) {
        // Handle error
    }
}
```

**V10.x Enhanced Approach:**
```c
#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    uint32_t data;
    uint8_t status;
} QueueItem_t;

QueueHandle_t xQueue;

// Dynamic allocation (compatible)
void initializeQueueDynamic(void) {
    xQueue = xQueueCreate(10, sizeof(QueueItem_t));
    configASSERT(xQueue != NULL);
}

// Static allocation (V10.x)
void initializeQueueStatic(void) {
    static StaticQueue_t xQueueBuffer;
    static uint8_t ucQueueStorage[10 * sizeof(QueueItem_t)];
    
    xQueue = xQueueCreateStatic(
        10,
        sizeof(QueueItem_t),
        ucQueueStorage,
        &xQueueBuffer
    );
    
    configASSERT(xQueue != NULL);
}
```

### Example 3: Stream Buffer (New in V10.x)

```c
#include "FreeRTOS.h"
#include "stream_buffer.h"

// New feature in V10.x - not available in V9.x
StreamBufferHandle_t xStreamBuffer;

void initializeStreamBuffer(void) {
    // Dynamic allocation
    xStreamBuffer = xStreamBufferCreate(
        1000,  // Buffer size in bytes
        10     // Trigger level
    );
    
    configASSERT(xStreamBuffer != NULL);
}

void streamBufferExample(void) {
    uint8_t txData[100] = "Hello from stream buffer";
    uint8_t rxData[100];
    size_t xBytesSent, xBytesReceived;
    
    // Send data
    xBytesSent = xStreamBufferSend(
        xStreamBuffer,
        txData,
        sizeof(txData),
        pdMS_TO_TICKS(100)
    );
    
    // Receive data
    xBytesReceived = xStreamBufferReceive(
        xStreamBuffer,
        rxData,
        sizeof(rxData),
        pdMS_TO_TICKS(100)
    );
}
```

### Example 4: Configuration Migration

**Old FreeRTOSConfig.h (V9.x):**
```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ( 80000000UL )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                    ( 5 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 20 * 1024 ) )
#define configMAX_TASK_NAME_LEN                 ( 16 )

// V9.x didn't require these
// #define configSUPPORT_STATIC_ALLOCATION      0
// #define configSUPPORT_DYNAMIC_ALLOCATION     1

#endif /* FREERTOS_CONFIG_H */
```

**Updated FreeRTOSConfig.h (V10.x):**
```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ( 80000000UL )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                    ( 5 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 128 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 20 * 1024 ) )
#define configMAX_TASK_NAME_LEN                 ( 16 )

// New in V10.x - must be explicitly defined
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1

// Enhanced features in V10.x
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_STREAM_BUFFERS                1
#define configUSE_MESSAGE_BUFFERS               1

// Memory allocation scheme
#define configAPPLICATION_ALLOCATED_HEAP        0

// Tickless idle mode (enhanced in V10.x)
#define configUSE_TICKLESS_IDLE                 2

#endif /* FREERTOS_CONFIG_H */
```

### Example 5: Task Notification Migration

**V9.x Limited Support:**
```c
// Task notifications existed but were more limited
void vTaskNotificationV9(void) {
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue = 0x01;
    
    // Basic notification
    xTaskNotifyGive(xTaskToNotify);
    
    // Wait for notification
    ulNotificationValue = ulTaskNotifyTake(
        pdTRUE,  // Clear on exit
        portMAX_DELAY
    );
}
```

**V10.x Enhanced Support:**
```c
// More sophisticated notification features
void vTaskNotificationV10(void) {
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue = 0;
    BaseType_t xResult;
    
    // Enhanced notification with action
    xResult = xTaskNotify(
        xTaskToNotify,
        0x01,
        eSetBits  // Action: set bits
    );
    
    // Wait with previous value retrieval
    xResult = xTaskNotifyWait(
        0x00,           // Don't clear bits on entry
        0xFFFFFFFF,     // Clear all bits on exit
        &ulNotificationValue,
        portMAX_DELAY
    );
    
    // Indexed notifications (V10.4.0+)
    xResult = xTaskNotifyIndexed(
        xTaskToNotify,
        0,      // Notification index
        0x01,
        eSetBits
    );
}
```

## Rust Programming Examples

### Example 1: Basic Task Creation with Version Compatibility

```rust
// Using freertos-rust crate
use freertos_rust::*;
use core::time::Duration;

// V10.x compatible task creation
fn create_task_v10() -> Result<(), FreeRtosError> {
    // Dynamic allocation (works in both V9.x and V10.x)
    Task::new()
        .name("RustTask")
        .stack_size(2048)
        .priority(TaskPriority(1))
        .start(move |_| {
            loop {
                // Task work
                CurrentTask::delay(Duration::from_millis(1000));
            }
        })?;
    
    Ok(())
}

// Static allocation wrapper for V10.x
fn create_static_task_v10() {
    // Note: Static allocation in Rust requires unsafe blocks
    // and careful lifetime management
    unsafe {
        // This is a simplified example
        // Actual implementation requires more FFI work
        static mut TASK_STACK: [u8; 2048] = [0; 2048];
        static mut TASK_TCB: freertos_sys::StaticTask_t = 
            core::mem::zeroed();
        
        // Create static task
        // Implementation would call xTaskCreateStatic
    }
}
```

### Example 2: Queue Migration Pattern

```rust
use freertos_rust::*;

#[derive(Copy, Clone)]
struct QueueItem {
    data: u32,
    status: u8,
}

// V10.x compatible queue implementation
struct QueueManager {
    queue: Queue<QueueItem>,
}

impl QueueManager {
    fn new_dynamic() -> Result<Self, FreeRtosError> {
        Ok(QueueManager {
            queue: Queue::new(10)?,
        })
    }
    
    fn send_item(&self, item: QueueItem, timeout_ms: u32) 
        -> Result<(), FreeRtosError> {
        self.queue.send(
            item, 
            Duration::from_millis(timeout_ms as u64)
        )
    }
    
    fn receive_item(&self, timeout_ms: u32) 
        -> Result<QueueItem, FreeRtosError> {
        self.queue.receive(
            Duration::from_millis(timeout_ms as u64)
        )
    }
}
```

### Example 3: Mutex with Recursive Support (V10.x)

```rust
use freertos_rust::*;

struct SharedResource {
    mutex: Mutex<u32>,
}

impl SharedResource {
    fn new() -> Result<Self, FreeRtosError> {
        Ok(SharedResource {
            mutex: Mutex::new(0)?,
        })
    }
    
    fn increment(&self) -> Result<(), FreeRtosError> {
        let mut data = self.mutex.lock(Duration::from_secs(1))?;
        *data += 1;
        Ok(())
    }
    
    fn get_value(&self) -> Result<u32, FreeRtosError> {
        let data = self.mutex.lock(Duration::from_secs(1))?;
        Ok(*data)
    }
}

// Recursive mutex (V10.x feature)
struct RecursiveResource {
    rec_mutex: freertos_rust::Mutex<u32>, // Would use recursive variant
}

impl RecursiveResource {
    fn recursive_operation(&self, depth: u32) 
        -> Result<(), FreeRtosError> {
        if depth == 0 {
            return Ok(());
        }
        
        let mut data = self.rec_mutex.lock(Duration::from_secs(1))?;
        *data += 1;
        
        // Recursive call - requires recursive mutex
        drop(data); // Release lock before recursion in this example
        self.recursive_operation(depth - 1)?;
        
        Ok(())
    }
}
```

### Example 4: Task Notifications in Rust

```rust
use freertos_rust::*;

struct NotificationExample {
    task_handle: TaskHandle,
}

impl NotificationExample {
    fn send_notification(&self, value: u32) -> Result<(), FreeRtosError> {
        // Send notification to task
        self.task_handle.notify(value, NotifyAction::SetBits)?;
        Ok(())
    }
    
    fn wait_for_notification() -> u32 {
        // Wait for notification (in the task itself)
        CurrentTask::notify_wait(
            0,          // Clear bits on entry
            0xFFFFFFFF, // Clear bits on exit
            Duration::max_value()
        ).unwrap_or(0)
    }
}

fn notification_task() {
    Task::new()
        .name("NotifyTask")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move |_| {
            loop {
                let notification_value = 
                    NotificationExample::wait_for_notification();
                
                // Process notification
                if notification_value & 0x01 != 0 {
                    // Handle bit 0
                }
                
                if notification_value & 0x02 != 0 {
                    // Handle bit 1
                }
            }
        })
        .unwrap();
}
```

### Example 5: Feature-Gated Code for Version Compatibility

```rust
// Cargo.toml would have feature flags for different versions
// [features]
// freertos_v9 = []
// freertos_v10 = ["stream_buffers"]

use freertos_rust::*;

#[cfg(feature = "freertos_v10")]
mod v10_features {
    use super::*;
    
    pub struct StreamBufferManager {
        // Stream buffer implementation for V10.x
    }
    
    impl StreamBufferManager {
        pub fn new(size: usize) -> Result<Self, FreeRtosError> {
            // Create stream buffer
            Ok(StreamBufferManager {})
        }
        
        pub fn send(&self, data: &[u8]) -> Result<usize, FreeRtosError> {
            // Send data to stream buffer
            Ok(data.len())
        }
        
        pub fn receive(&self, buffer: &mut [u8]) 
            -> Result<usize, FreeRtosError> {
            // Receive data from stream buffer
            Ok(0)
        }
    }
}

#[cfg(not(feature = "freertos_v10"))]
mod v10_features {
    // Provide stub or alternative implementation for V9.x
    pub struct StreamBufferManager;
    
    impl StreamBufferManager {
        pub fn new(_size: usize) -> Result<Self, ()> {
            Err(()) // Not supported in V9.x
        }
    }
}
```

## Migration Checklist

### Pre-Migration Steps
1. **Backup existing code** - Create version control checkpoint
2. **Review release notes** - Understand all changes between versions
3. **Identify deprecated APIs** - List all APIs that need updates
4. **Check hardware dependencies** - Ensure port compatibility

### During Migration
1. **Update FreeRTOSConfig.h** - Add new required definitions
2. **Enable static allocation** - Set `configSUPPORT_STATIC_ALLOCATION`
3. **Update API calls** - Replace deprecated functions
4. **Add error checking** - Use `configASSERT` more extensively
5. **Test incrementally** - Migrate and test module by module

### Post-Migration Validation
1. **Run all existing tests** - Ensure functionality preserved
2. **Monitor resource usage** - Check stack and heap consumption
3. **Verify timing behavior** - Confirm task scheduling unchanged
4. **Check interrupt handlers** - Validate ISR compatibility

## Summary

The FreeRTOS Version Migration Guide is essential for maintaining and upgrading embedded systems that rely on FreeRTOS. The transition from V9.x to V10.x introduced significant enhancements including mandatory static allocation support, stream buffers, message buffers, and improved task notification mechanisms. Successful migration requires careful attention to configuration changes in `FreeRTOSConfig.h`, API updates, and adoption of new features while maintaining backward compatibility where needed.

Key takeaways include:
- **Static allocation** became a first-class citizen requiring explicit configuration
- **New primitives** like stream buffers and message buffers offer efficient data transfer
- **Enhanced notifications** provide more flexible inter-task communication
- **Configuration management** is critical with new mandatory defines
- **Incremental testing** ensures stable migration without introducing regressions

Both C/C++ and Rust implementations can leverage these improvements, with Rust benefiting from additional type safety and memory guarantees. Proper version migration ensures systems can take advantage of performance improvements, new features, and ongoing security updates while maintaining stability in production environments.