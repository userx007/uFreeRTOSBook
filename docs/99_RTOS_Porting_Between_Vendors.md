# RTOS Porting Between Vendors

## Overview

Porting an embedded application from one RTOS to another is a common challenge in embedded systems development. Organizations may need to migrate due to licensing changes, vendor support issues, hardware platform changes, or strategic decisions. This guide focuses on migrating from popular RTOSes like ThreadX, µC/OS-III, and Zephyr to FreeRTOS.

The porting process involves understanding conceptual differences between RTOSes, mapping APIs, adapting synchronization primitives, handling timing differences, and refactoring architecture where necessary.

## Key Conceptual Differences

### Task/Thread Management

**FreeRTOS:**
- Tasks are created with `xTaskCreate()` or `xTaskCreateStatic()`
- Priority range: 0 (lowest) to configMAX_PRIORITIES-1 (highest)
- Tasks are functions that typically run in infinite loops
- Cooperative or preemptive scheduling based on configuration

**ThreadX:**
- Threads created with `tx_thread_create()`
- Priority range: 0 (highest) to 31 (lowest) - **inverted from FreeRTOS**
- Time-slicing can be enabled per thread
- Preemptive scheduling with optional time-slicing

**µC/OS-III:**
- Tasks created with `OSTaskCreate()`
- Priority range: 0 (highest) to OS_CFG_PRIO_MAX-1 (lowest)
- Round-robin scheduling at same priority level
- Highly deterministic, preemptive kernel

**Zephyr:**
- Threads created with `k_thread_create()`
- Priority can be negative (cooperative) or 0+ (preemptive)
- Supports both preemptive and cooperative scheduling
- More complex priority model

### Synchronization Primitives

| Primitive | FreeRTOS | ThreadX | µC/OS-III | Zephyr |
|-----------|----------|---------|-----------|---------|
| Mutex | `xSemaphoreCreateMutex()` | `tx_mutex_create()` | `OSMutexCreate()` | `k_mutex_init()` |
| Binary Semaphore | `xSemaphoreCreateBinary()` | `tx_semaphore_create()` | `OSSemCreate()` | `k_sem_init()` |
| Counting Semaphore | `xSemaphoreCreateCounting()` | `tx_semaphore_create()` | `OSSemCreate()` | `k_sem_init()` |
| Queue | `xQueueCreate()` | `tx_queue_create()` | `OSQCreate()` | `k_msgq_init()` |
| Event Flags | Event Groups | `tx_event_flags_create()` | `OSFlagCreate()` | `k_poll()` |

## API Mapping Guide

### Task Creation

**ThreadX to FreeRTOS:**

```c
// ThreadX
TX_THREAD my_thread;
CHAR thread_stack[1024];

tx_thread_create(&my_thread, 
                 "My Thread", 
                 my_thread_entry,
                 0x1234,              // Entry input
                 thread_stack, 
                 sizeof(thread_stack),
                 16,                  // Priority (0=highest)
                 16,                  // Preemption threshold
                 TX_NO_TIME_SLICE,
                 TX_AUTO_START);

void my_thread_entry(ULONG input) {
    while(1) {
        // Thread work
        tx_thread_sleep(100);
    }
}
```

```c
// FreeRTOS equivalent
TaskHandle_t my_task_handle;

xTaskCreate(my_task_function,
            "My Task",
            256,                    // Stack size in words
            (void*)0x1234,          // Parameters
            configMAX_PRIORITIES - 16, // Priority (inverted!)
            &my_task_handle);

void my_task_function(void *pvParameters) {
    uint32_t input = (uint32_t)pvParameters;
    while(1) {
        // Task work
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**µC/OS-III to FreeRTOS:**

```c
// µC/OS-III
OS_TCB MyTaskTCB;
CPU_STK MyTaskStack[128];

OSTaskCreate(&MyTaskTCB,
             "My Task",
             MyTask,
             (void *)0,
             10,                    // Priority (0=highest)
             MyTaskStack,
             128/10,                // Stack limit
             128,                   // Stack size
             0,                     // Messages
             10,                    // Time quanta
             (void *)0,
             OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR,
             &err);

void MyTask(void *p_arg) {
    OS_ERR err;
    while(1) {
        OSTimeDly(100, OS_OPT_TIME_DLY, &err);
    }
}
```

```c
// FreeRTOS equivalent
TaskHandle_t my_task_handle;

xTaskCreate(my_task,
            "My Task",
            128,                        // Stack in words
            NULL,
            configMAX_PRIORITIES - 10,  // Inverted priority
            &my_task_handle);

void my_task(void *pvParameters) {
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Zephyr to FreeRTOS:**

```c
// Zephyr
#define STACK_SIZE 1024
#define PRIORITY 7

K_THREAD_STACK_DEFINE(my_stack_area, STACK_SIZE);
struct k_thread my_thread_data;

k_thread_create(&my_thread_data, my_stack_area,
                K_THREAD_STACK_SIZEOF(my_stack_area),
                my_thread_entry,
                NULL, NULL, NULL,
                PRIORITY, 0, K_NO_WAIT);

void my_thread_entry(void *p1, void *p2, void *p3) {
    while(1) {
        k_sleep(K_MSEC(100));
    }
}
```

```c
// FreeRTOS equivalent
TaskHandle_t my_task;

xTaskCreate(my_task_entry,
            "MyTask",
            256,
            NULL,
            PRIORITY,
            &my_task);

void my_task_entry(void *pvParameters) {
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Mutex Operations

**ThreadX to FreeRTOS:**

```c
// ThreadX
TX_MUTEX my_mutex;
tx_mutex_create(&my_mutex, "My Mutex", TX_NO_INHERIT);

// Lock
tx_mutex_get(&my_mutex, TX_WAIT_FOREVER);
// Critical section
tx_mutex_put(&my_mutex);
```

```c
// FreeRTOS
SemaphoreHandle_t my_mutex;
my_mutex = xSemaphoreCreateMutex();

// Lock
xSemaphoreTake(my_mutex, portMAX_DELAY);
// Critical section
xSemaphoreGive(my_mutex);
```

### Queue Operations

**µC/OS-III to FreeRTOS:**

```c
// µC/OS-III
OS_Q MyQueue;
OS_MSG_QTY msg_count;

OSQCreate(&MyQueue, "My Queue", 10, &err);

// Send
void *msg = (void*)0x1234;
OSQPost(&MyQueue, msg, sizeof(msg), OS_OPT_POST_FIFO, &err);

// Receive
void *p_msg;
OS_MSG_SIZE msg_size;
p_msg = OSQPend(&MyQueue, 0, OS_OPT_PEND_BLOCKING, &msg_size, NULL, &err);
```

```c
// FreeRTOS
QueueHandle_t my_queue;
my_queue = xQueueCreate(10, sizeof(void*));

// Send
void *msg = (void*)0x1234;
xQueueSend(my_queue, &msg, portMAX_DELAY);

// Receive
void *p_msg;
xQueueReceive(my_queue, &p_msg, portMAX_DELAY);
```

## Complete Migration Example

### ThreadX Application

```c
// ThreadX original application
#include "tx_api.h"

#define STACK_SIZE 1024
#define QUEUE_SIZE 16

TX_THREAD producer_thread;
TX_THREAD consumer_thread;
TX_QUEUE data_queue;
TX_MUTEX resource_mutex;

UCHAR producer_stack[STACK_SIZE];
UCHAR consumer_stack[STACK_SIZE];
ULONG queue_storage[QUEUE_SIZE];

void producer_entry(ULONG input);
void consumer_entry(ULONG input);

void tx_application_define(void *first_unused_memory) {
    // Create mutex
    tx_mutex_create(&resource_mutex, "Resource Mutex", TX_NO_INHERIT);
    
    // Create queue
    tx_queue_create(&data_queue, "Data Queue", 
                    sizeof(ULONG), queue_storage, 
                    sizeof(queue_storage));
    
    // Create threads
    tx_thread_create(&producer_thread, "Producer", producer_entry,
                     0, producer_stack, STACK_SIZE, 
                     1, 1, TX_NO_TIME_SLICE, TX_AUTO_START);
    
    tx_thread_create(&consumer_thread, "Consumer", consumer_entry,
                     0, consumer_stack, STACK_SIZE,
                     2, 2, TX_NO_TIME_SLICE, TX_AUTO_START);
}

void producer_entry(ULONG input) {
    ULONG data = 0;
    while(1) {
        tx_mutex_get(&resource_mutex, TX_WAIT_FOREVER);
        data++;
        tx_mutex_put(&resource_mutex);
        
        tx_queue_send(&data_queue, &data, TX_WAIT_FOREVER);
        tx_thread_sleep(50);
    }
}

void consumer_entry(ULONG input) {
    ULONG received_data;
    while(1) {
        tx_queue_receive(&data_queue, &received_data, TX_WAIT_FOREVER);
        
        tx_mutex_get(&resource_mutex, TX_WAIT_FOREVER);
        // Process data
        tx_mutex_put(&resource_mutex);
    }
}
```

### Migrated FreeRTOS Application

```c
// FreeRTOS migrated version
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define STACK_SIZE 256  // In words, not bytes
#define QUEUE_LENGTH 16

TaskHandle_t producer_handle;
TaskHandle_t consumer_handle;
QueueHandle_t data_queue;
SemaphoreHandle_t resource_mutex;

void producer_task(void *pvParameters);
void consumer_task(void *pvParameters);

int main(void) {
    // Hardware initialization
    
    // Create mutex
    resource_mutex = xSemaphoreCreateMutex();
    
    // Create queue (stores uint32_t values)
    data_queue = xQueueCreate(QUEUE_LENGTH, sizeof(uint32_t));
    
    // Create tasks (note inverted priorities)
    xTaskCreate(producer_task, "Producer", STACK_SIZE, NULL,
                configMAX_PRIORITIES - 1,  // High priority
                &producer_handle);
    
    xTaskCreate(consumer_task, "Consumer", STACK_SIZE, NULL,
                configMAX_PRIORITIES - 2,  // Lower priority
                &consumer_handle);
    
    // Start scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    while(1);
}

void producer_task(void *pvParameters) {
    uint32_t data = 0;
    
    while(1) {
        xSemaphoreTake(resource_mutex, portMAX_DELAY);
        data++;
        xSemaphoreGive(resource_mutex);
        
        xQueueSend(data_queue, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void consumer_task(void *pvParameters) {
    uint32_t received_data;
    
    while(1) {
        xQueueReceive(data_queue, &received_data, portMAX_DELAY);
        
        xSemaphoreTake(resource_mutex, portMAX_DELAY);
        // Process data
        xSemaphoreGive(resource_mutex);
    }
}

// FreeRTOS hook functions
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    // Handle stack overflow
    while(1);
}

void vApplicationMallocFailedHook(void) {
    // Handle malloc failure
    while(1);
}
```

## Rust Implementation

FreeRTOS bindings exist for Rust, though they're less mature than C. Here's the equivalent in Rust using the `freertos-rust` crate:

```rust
// Cargo.toml dependencies:
// freertos-rust = "0.1"

use freertos_rust::*;
use std::sync::Arc;

const STACK_SIZE: u16 = 256;
const QUEUE_LENGTH: usize = 16;

fn main() {
    // Create synchronization primitives
    let resource_mutex = Arc::new(Mutex::new(()).unwrap());
    let data_queue = Arc::new(Queue::new(QUEUE_LENGTH).unwrap());
    
    // Clone for each task
    let mutex_producer = Arc::clone(&resource_mutex);
    let mutex_consumer = Arc::clone(&resource_mutex);
    let queue_producer = Arc::clone(&data_queue);
    let queue_consumer = Arc::clone(&data_queue);
    
    // Create producer task
    Task::new()
        .name("Producer")
        .stack_size(STACK_SIZE)
        .priority(TaskPriority(configMAX_PRIORITIES - 1))
        .start(move || {
            producer_task(mutex_producer, queue_producer);
        })
        .unwrap();
    
    // Create consumer task
    Task::new()
        .name("Consumer")
        .stack_size(STACK_SIZE)
        .priority(TaskPriority(configMAX_PRIORITIES - 2))
        .start(move || {
            consumer_task(mutex_consumer, queue_consumer);
        })
        .unwrap();
    
    // Start scheduler
    FreeRtosUtils::start_scheduler();
}

fn producer_task(mutex: Arc<Mutex<()>>, queue: Arc<Queue<u32>>) {
    let mut data: u32 = 0;
    
    loop {
        {
            let _lock = mutex.lock(Duration::infinite()).unwrap();
            data += 1;
        } // Mutex automatically released
        
        queue.send(data, Duration::infinite()).unwrap();
        CurrentTask::delay(Duration::ms(50));
    }
}

fn consumer_task(mutex: Arc<Mutex<()>>, queue: Arc<Queue<u32>>) {
    loop {
        let received_data = queue.receive(Duration::infinite()).unwrap();
        
        {
            let _lock = mutex.lock(Duration::infinite()).unwrap();
            // Process data
            println!("Received: {}", received_data);
        }
    }
}
```

## Timing Considerations

### Tick Rate Differences

Different RTOSes may have different default tick rates:
- **ThreadX**: Often 100 Hz (10ms tick)
- **µC/OS-III**: Typically 1000 Hz (1ms tick)
- **FreeRTOS**: Configurable via `configTICK_RATE_HZ` (commonly 1000 Hz)
- **Zephyr**: Configurable, often 100-1000 Hz

**Migration Strategy:**
```c
// Create macro for portable delays
#if defined(USING_THREADX)
    #define OS_DELAY_MS(ms) tx_thread_sleep((ms) / 10)  // 10ms ticks
#elif defined(USING_FREERTOS)
    #define OS_DELAY_MS(ms) vTaskDelay(pdMS_TO_TICKS(ms))
#elif defined(USING_ZEPHYR)
    #define OS_DELAY_MS(ms) k_sleep(K_MSEC(ms))
#endif
```

### Priority Inversion Handling

FreeRTOS handles priority inversion differently:
- **Mutual Exclusion**: Use mutexes (which support priority inheritance) instead of binary semaphores for resource protection
- **ThreadX** has priority inheritance via `TX_INHERIT` flag
- **FreeRTOS** mutexes automatically use priority inheritance

```c
// Wrong - binary semaphore for mutual exclusion
SemaphoreHandle_t sem = xSemaphoreCreateBinary();
xSemaphoreGive(sem);  // Initialize

// Correct - mutex with priority inheritance
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
```

## Refactoring Strategies

### 1. Abstraction Layer Approach

Create a hardware abstraction layer (HAL) for RTOS primitives:

```c
// rtos_abstraction.h
typedef void* RTOS_TaskHandle;
typedef void* RTOS_QueueHandle;
typedef void* RTOS_MutexHandle;

typedef void (*RTOS_TaskFunction)(void* params);

RTOS_TaskHandle RTOS_CreateTask(RTOS_TaskFunction func, 
                                 const char* name,
                                 uint32_t stack_size,
                                 void* params,
                                 uint32_t priority);

RTOS_QueueHandle RTOS_CreateQueue(uint32_t length, uint32_t item_size);
void RTOS_QueueSend(RTOS_QueueHandle queue, void* item, uint32_t timeout);
void RTOS_QueueReceive(RTOS_QueueHandle queue, void* item, uint32_t timeout);

RTOS_MutexHandle RTOS_CreateMutex(void);
void RTOS_MutexLock(RTOS_MutexHandle mutex, uint32_t timeout);
void RTOS_MutexUnlock(RTOS_MutexHandle mutex);

void RTOS_DelayMs(uint32_t ms);
```

```c
// rtos_abstraction_freertos.c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "rtos_abstraction.h"

RTOS_TaskHandle RTOS_CreateTask(RTOS_TaskFunction func,
                                 const char* name,
                                 uint32_t stack_size,
                                 void* params,
                                 uint32_t priority) {
    TaskHandle_t handle;
    xTaskCreate((TaskFunction_t)func, name, stack_size/4, 
                params, priority, &handle);
    return (RTOS_TaskHandle)handle;
}

RTOS_QueueHandle RTOS_CreateQueue(uint32_t length, uint32_t item_size) {
    return (RTOS_QueueHandle)xQueueCreate(length, item_size);
}

void RTOS_QueueSend(RTOS_QueueHandle queue, void* item, uint32_t timeout) {
    xQueueSend((QueueHandle_t)queue, item, 
               timeout == 0xFFFFFFFF ? portMAX_DELAY : pdMS_TO_TICKS(timeout));
}

RTOS_MutexHandle RTOS_CreateMutex(void) {
    return (RTOS_MutexHandle)xSemaphoreCreateMutex();
}

void RTOS_MutexLock(RTOS_MutexHandle mutex, uint32_t timeout) {
    xSemaphoreTake((SemaphoreHandle_t)mutex,
                   timeout == 0xFFFFFFFF ? portMAX_DELAY : pdMS_TO_TICKS(timeout));
}

void RTOS_MutexUnlock(RTOS_MutexHandle mutex) {
    xSemaphoreGive((SemaphoreHandle_t)mutex);
}

void RTOS_DelayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
```

### 2. Incremental Migration

Migrate one module at a time:

1. **Phase 1**: Create abstraction layer
2. **Phase 2**: Port non-critical tasks
3. **Phase 3**: Port interrupt handlers and critical sections
4. **Phase 4**: Optimize and remove abstraction layer if desired

### 3. Testing Strategy

```c
// Verification wrapper
void verify_queue_send(QueueHandle_t queue, void* data, const char* location) {
    BaseType_t result = xQueueSend(queue, data, portMAX_DELAY);
    if (result != pdPASS) {
        log_error("Queue send failed at %s", location);
        configASSERT(0);
    }
}

#define QUEUE_SEND_VERIFY(q, d) verify_queue_send(q, d, __FILE__ ":" STRINGIFY(__LINE__))
```

## Memory Management Differences

### Static vs Dynamic Allocation

**ThreadX**: Uses byte pools or block pools
```c
TX_BYTE_POOL my_pool;
CHAR pool_memory[10240];
tx_byte_pool_create(&my_pool, "My Pool", pool_memory, 10240);

VOID *memory_ptr;
tx_byte_allocate(&my_pool, &memory_ptr, 100, TX_NO_WAIT);
```

**FreeRTOS**: Uses heap schemes (heap_1 through heap_5)
```c
// Dynamic allocation
void *ptr = pvPortMalloc(100);
vPortFree(ptr);

// Or static allocation
StaticTask_t task_buffer;
StackType_t stack_buffer[128];
TaskHandle_t task = xTaskCreateStatic(task_func, "Task", 128, NULL, 1,
                                       stack_buffer, &task_buffer);
```

## Common Pitfalls and Solutions

### 1. Priority Inversion

**Problem**: Different priority numbering systems
**Solution**: Create priority mapping macros

```c
#define PRIORITY_HIGH    (configMAX_PRIORITIES - 1)
#define PRIORITY_MEDIUM  (configMAX_PRIORITIES / 2)
#define PRIORITY_LOW     1  // Don't use 0 (idle task priority)
```

### 2. Stack Size Confusion

**Problem**: Different units (bytes vs words)
**Solution**: Always calculate in bytes, convert for FreeRTOS

```c
#define TASK_STACK_BYTES 2048
#define TASK_STACK_WORDS (TASK_STACK_BYTES / sizeof(StackType_t))

xTaskCreate(task, "Task", TASK_STACK_WORDS, NULL, PRIORITY_MEDIUM, NULL);
```

### 3. Timing Precision

**Problem**: Different tick rates cause timing bugs
**Solution**: Use time-based APIs consistently

```c
// Always use millisecond-based delays
vTaskDelay(pdMS_TO_TICKS(100));  // Not vTaskDelay(100)
```

### 4. Interrupt Context

**Problem**: Different ISR APIs
**Solution**: Use FreeRTOS FromISR variants

```c
void UART_IRQHandler(void) {
    BaseType_t higher_priority_task_woken = pdFALSE;
    char data = UART_ReadByte();
    
    xQueueSendFromISR(uart_queue, &data, &higher_priority_task_woken);
    
    portYIELD_FROM_ISR(higher_priority_task_woken);
}
```

## Summary

Porting between RTOSes requires careful attention to:

1. **Priority Systems**: Understand inverted priorities (ThreadX/µC/OS high=0 vs FreeRTOS high=max)

2. **API Mapping**: Map tasks, queues, semaphores, and mutexes correctly with proper parameter conversions

3. **Timing**: Account for different tick rates and use portable time macros

4. **Memory Management**: Adapt from byte pools/block pools to FreeRTOS heap schemes

5. **Interrupt Handling**: Use FromISR variants for all queue/semaphore operations in interrupts

6. **Stack Sizing**: Convert between bytes and words appropriately for each platform

7. **Testing**: Validate behavior incrementally, especially timing-critical sections and synchronization

8. **Abstraction**: Consider an abstraction layer for easier future migrations or multi-RTOS support

The key to successful migration is methodical mapping of concepts, comprehensive testing, and understanding the subtle behavioral differences between RTOSes. Starting with non-critical components and migrating incrementally reduces risk while allowing validation at each step.