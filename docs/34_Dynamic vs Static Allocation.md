# Dynamic vs Static Allocation in FreeRTOS

## Overview

FreeRTOS supports two fundamental approaches for allocating kernel objects like tasks, queues, semaphores, and timers: **dynamic allocation** (using heap memory) and **static allocation** (using compile-time allocated memory). This choice significantly impacts system predictability, memory usage, and certification requirements.

## Core Concepts

### Dynamic Allocation
Dynamic allocation uses `pvPortMalloc()` to allocate memory from the FreeRTOS heap at runtime. The kernel manages a heap (typically 5-200KB) configured by `configTOTAL_HEAP_SIZE`.

### Static Allocation
Static allocation uses memory provided by the application at compile time. Memory addresses are known before runtime, and no heap is required. Objects use buffers allocated in static or global memory space.

## Task Creation: xTaskCreate() vs xTaskCreateStatic()

### xTaskCreate() - Dynamic Allocation

```c
BaseType_t xTaskCreate(
    TaskFunction_t pvTaskCode,      // Task function pointer
    const char * const pcName,      // Descriptive name
    uint16_t usStackDepth,          // Stack size in words
    void *pvParameters,             // Task parameters
    UBaseType_t uxPriority,         // Task priority
    TaskHandle_t *pxCreatedTask     // Handle to created task
);
```

**Example:**
```c
void vTaskFunction(void *pvParameters)
{
    for(;;)
    {
        // Task work
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup_tasks(void)
{
    TaskHandle_t xHandle = NULL;
    
    // Create task dynamically - FreeRTOS allocates memory
    BaseType_t xResult = xTaskCreate(
        vTaskFunction,           // Task function
        "MyTask",               // Name for debugging
        200,                    // Stack size (200 words = 800 bytes on 32-bit)
        NULL,                   // Parameters
        2,                      // Priority
        &xHandle                // Task handle
    );
    
    if(xResult != pdPASS)
    {
        // Creation failed - likely out of heap memory
        error_handler();
    }
}
```

### xTaskCreateStatic() - Static Allocation

```c
TaskHandle_t xTaskCreateStatic(
    TaskFunction_t pvTaskCode,
    const char * const pcName,
    uint32_t ulStackDepth,
    void *pvParameters,
    UBaseType_t uxPriority,
    StackType_t * const puxStackBuffer,      // Pre-allocated stack
    StaticTask_t * const pxTaskBuffer        // Pre-allocated TCB
);
```

**Example:**
```c
// Pre-allocate memory at compile time
#define TASK_STACK_SIZE 200
static StackType_t xTaskStack[TASK_STACK_SIZE];
static StaticTask_t xTaskBuffer;

void vTaskFunction(void *pvParameters)
{
    for(;;)
    {
        // Task work
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup_tasks_static(void)
{
    TaskHandle_t xHandle;
    
    // Create task statically - application provides memory
    xHandle = xTaskCreateStatic(
        vTaskFunction,           // Task function
        "MyTask",               // Name
        TASK_STACK_SIZE,        // Stack size
        NULL,                   // Parameters
        2,                      // Priority
        xTaskStack,             // Stack buffer (pre-allocated)
        &xTaskBuffer            // TCB buffer (pre-allocated)
    );
    
    // Returns NULL only on parameter error, not memory issues
    configASSERT(xHandle != NULL);
}
```

## Configuration Requirements

### For Dynamic Allocation
```c
// FreeRTOSConfig.h
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configTOTAL_HEAP_SIZE               (20 * 1024)  // 20KB heap

// Choose heap implementation (heap_1.c through heap_5.c)
```

### For Static Allocation
```c
// FreeRTOSConfig.h
#define configSUPPORT_STATIC_ALLOCATION     1

// Required callbacks for IDLE and Timer tasks
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if (configUSE_TIMERS == 1)
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif
```

## Other Kernel Objects: Static vs Dynamic

### Queues

```c
// Dynamic queue creation
QueueHandle_t xQueue = xQueueCreate(10, sizeof(uint32_t));

// Static queue creation
#define QUEUE_LENGTH 10
#define ITEM_SIZE sizeof(uint32_t)
static uint8_t ucQueueStorageArea[QUEUE_LENGTH * ITEM_SIZE];
static StaticQueue_t xQueueBuffer;

QueueHandle_t xQueue = xQueueCreateStatic(
    QUEUE_LENGTH,
    ITEM_SIZE,
    ucQueueStorageArea,
    &xQueueBuffer
);
```

### Semaphores

```c
// Dynamic binary semaphore
SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();

// Static binary semaphore
static StaticSemaphore_t xSemaphoreBuffer;
SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinaryStatic(&xSemaphoreBuffer);

// Dynamic mutex
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

// Static mutex
static StaticSemaphore_t xMutexBuffer;
SemaphoreHandle_t xMutex = xSemaphoreCreateMutexStatic(&xMutexBuffer);
```

### Timers

```c
// Dynamic timer
TimerHandle_t xTimer = xTimerCreate(
    "Timer",                    // Name
    pdMS_TO_TICKS(1000),       // Period
    pdTRUE,                    // Auto-reload
    NULL,                      // Timer ID
    vTimerCallback             // Callback
);

// Static timer
static StaticTimer_t xTimerBuffer;
TimerHandle_t xTimer = xTimerCreateStatic(
    "Timer",
    pdMS_TO_TICKS(1000),
    pdTRUE,
    NULL,
    vTimerCallback,
    &xTimerBuffer
);
```

### Event Groups

```c
// Dynamic
EventGroupHandle_t xEventGroup = xEventGroupCreate();

// Static
static StaticEventGroup_t xEventGroupBuffer;
EventGroupHandle_t xEventGroup = xEventGroupCreateStatic(&xEventGroupBuffer);
```

## Trade-offs Analysis

### Dynamic Allocation

**Advantages:**
- Simpler API - no need to manage buffers
- Flexible - create/delete objects at runtime
- Less code to write and maintain
- Memory freed when objects deleted (with heap_4 or heap_5)

**Disadvantages:**
- Heap fragmentation risk over time
- Non-deterministic allocation time
- Requires heap memory overhead
- Can run out of memory at runtime
- Harder to analyze memory usage statically
- Not suitable for safety-critical certification (IEC 61508, ISO 26262)

**Memory overhead example:**
```c
// Dynamic task consumes:
// - Stack size: 200 words × 4 bytes = 800 bytes
// - TCB: ~96 bytes (architecture dependent)
// - Heap management overhead: ~8-16 bytes per allocation
// Total from heap: ~900-920 bytes
```

### Static Allocation

**Advantages:**
- Deterministic memory layout - known at compile time
- No heap fragmentation
- No runtime allocation failures
- Better for safety certification (DO-178C, IEC 62304)
- Easier worst-case memory analysis
- Slightly faster (no allocation overhead)
- Required for some embedded systems without dynamic memory

**Disadvantages:**
- More verbose code - must manage all buffers
- Less flexible - objects can't be deleted and memory reclaimed
- Larger RAM footprint if objects not always needed
- Must calculate exact memory requirements upfront

**Memory usage example:**
```c
// Static task consumes:
// - Stack: 800 bytes (in .bss or .data section)
// - TCB: 96 bytes (in .bss or .data section)
// Total: ~896 bytes of static RAM
// No heap needed
```

## Implementing a Fully Static System

Here's a complete example of a system using only static allocation:

```c
/* FreeRTOSConfig.h */
#define configSUPPORT_STATIC_ALLOCATION     1
#define configSUPPORT_DYNAMIC_ALLOCATION    0  // Disable dynamic allocation
#define configUSE_TIMERS                    1
#define configTIMER_TASK_STACK_DEPTH        200
#define configMINIMAL_STACK_SIZE            128

/* main.c */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Task 1: Sensor reader
#define SENSOR_TASK_STACK_SIZE 256
static StackType_t xSensorTaskStack[SENSOR_TASK_STACK_SIZE];
static StaticTask_t xSensorTaskBuffer;

void vSensorTask(void *pvParameters)
{
    uint32_t sensorValue;
    QueueHandle_t xQueue = (QueueHandle_t)pvParameters;
    
    for(;;)
    {
        sensorValue = read_sensor();  // Simulated
        xQueueSend(xQueue, &sensorValue, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task 2: Data processor
#define PROCESSOR_TASK_STACK_SIZE 512
static StackType_t xProcessorTaskStack[PROCESSOR_TASK_STACK_SIZE];
static StaticTask_t xProcessorTaskBuffer;

void vProcessorTask(void *pvParameters)
{
    uint32_t receivedValue;
    QueueHandle_t xQueue = (QueueHandle_t)pvParameters;
    
    for(;;)
    {
        if(xQueueReceive(xQueue, &receivedValue, portMAX_DELAY) == pdPASS)
        {
            process_data(receivedValue);  // Simulated
        }
    }
}

// Queue for inter-task communication
#define QUEUE_LENGTH 10
#define QUEUE_ITEM_SIZE sizeof(uint32_t)
static uint8_t ucQueueStorageArea[QUEUE_LENGTH * QUEUE_ITEM_SIZE];
static StaticQueue_t xQueueBuffer;

// Mutex for shared resource protection
static StaticSemaphore_t xMutexBuffer;

// Required: IDLE task memory
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

// Required: Timer task memory
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

int main(void)
{
    // Hardware initialization
    hardware_init();
    
    // Create queue statically
    QueueHandle_t xDataQueue = xQueueCreateStatic(
        QUEUE_LENGTH,
        QUEUE_ITEM_SIZE,
        ucQueueStorageArea,
        &xQueueBuffer
    );
    configASSERT(xDataQueue != NULL);
    
    // Create mutex statically
    SemaphoreHandle_t xMutex = xSemaphoreCreateMutexStatic(&xMutexBuffer);
    configASSERT(xMutex != NULL);
    
    // Create tasks statically
    TaskHandle_t xSensorHandle = xTaskCreateStatic(
        vSensorTask,
        "Sensor",
        SENSOR_TASK_STACK_SIZE,
        (void *)xDataQueue,
        2,
        xSensorTaskStack,
        &xSensorTaskBuffer
    );
    configASSERT(xSensorHandle != NULL);
    
    TaskHandle_t xProcessorHandle = xTaskCreateStatic(
        vProcessorTask,
        "Processor",
        PROCESSOR_TASK_STACK_SIZE,
        (void *)xDataQueue,
        2,
        xProcessorTaskStack,
        &xProcessorTaskBuffer
    );
    configASSERT(xProcessorHandle != NULL);
    
    // Start scheduler - never returns
    vTaskStartScheduler();
    
    // Should never reach here
    for(;;);
    return 0;
}

// Stack overflow hook (optional but recommended)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // Handle stack overflow error
    for(;;);
}
```

## Memory Calculation for Static Systems

Calculate total RAM needed:

```c
// For the example above:

// Tasks:
// - Sensor task TCB: 96 bytes
// - Sensor task stack: 256 × 4 = 1024 bytes
// - Processor task TCB: 96 bytes
// - Processor task stack: 512 × 4 = 2048 bytes
// - IDLE task TCB: 96 bytes
// - IDLE task stack: 128 × 4 = 512 bytes
// - Timer task TCB: 96 bytes
// - Timer task stack: 200 × 4 = 800 bytes

// Subtotal tasks: 4768 bytes

// Objects:
// - Queue storage: 10 × 4 = 40 bytes
// - Queue structure: ~72 bytes
// - Mutex structure: ~96 bytes

// Subtotal objects: 208 bytes

// TOTAL STATIC RAM: ~4976 bytes (~4.9 KB)
// No heap needed!
```

## Hybrid Approach

You can enable both allocation methods:

```c
#define configSUPPORT_STATIC_ALLOCATION     1
#define configSUPPORT_DYNAMIC_ALLOCATION    1

// Use static for critical tasks
TaskHandle_t xCriticalTask = xTaskCreateStatic(...);

// Use dynamic for non-critical or temporary tasks
xTaskCreate(vTemporaryTask, ...);
```

## Decision Guidelines

**Choose Static Allocation when:**
- Building safety-critical systems requiring certification
- Deterministic behavior is mandatory
- System must never fail due to memory exhaustion
- Memory footprint must be precisely known at compile time
- Target has no heap or dynamic allocation is prohibited
- Long-term reliability is critical (months/years uptime)

**Choose Dynamic Allocation when:**
- Rapid prototyping or development
- System complexity makes static allocation impractical
- Tasks/objects are created and destroyed at runtime
- Certification not required
- RAM is limited and objects not always needed simultaneously
- Development time is constrained

**Choose Hybrid when:**
- Critical tasks use static allocation for predictability
- Non-critical tasks use dynamic allocation for flexibility
- System has both deterministic and non-deterministic requirements

## Common Pitfalls

### Static Allocation Issues
```c
// WRONG: Using local variables (stack allocated)
void bad_create_task(void)
{
    StackType_t xStack[200];  // Lost when function returns!
    StaticTask_t xTCB;        // Lost when function returns!
    
    xTaskCreateStatic(..., xStack, &xTCB);  // DANGER!
}

// CORRECT: Use static or global storage
static StackType_t xStack[200];
static StaticTask_t xTCB;

void good_create_task(void)
{
    xTaskCreateStatic(..., xStack, &xTCB);  // Safe
}
```

### Stack Size Calculation
```c
// Underestimated stack - will overflow
#define TASK_STACK_SIZE 50  // Too small!

// Use stack high water mark to find actual usage
UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
// Then set stack size with safety margin: actual_usage × 1.5
```

This comprehensive understanding of dynamic versus static allocation enables you to make informed architectural decisions for your FreeRTOS-based embedded systems based on safety requirements, memory constraints, and system complexity.