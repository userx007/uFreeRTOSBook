# Task Parameters and Return Values in FreeRTOS

## Overview

In FreeRTOS, tasks are independent execution threads that often need to receive configuration data at creation time and share information with other tasks during runtime. Understanding how to pass parameters, share data safely, and manage task-local storage is essential for building robust multitasking applications.

## 1. Passing Parameters to Tasks During Creation

When you create a task using `xTaskCreate()`, you can pass a pointer to any data structure as a parameter. This parameter becomes available to the task through its function signature.

### Basic Syntax

```c
BaseType_t xTaskCreate(
    TaskFunction_t pvTaskCode,      // Pointer to task function
    const char * const pcName,      // Task name
    uint16_t usStackDepth,          // Stack size
    void *pvParameters,             // Parameter passed to task
    UBaseType_t uxPriority,         // Task priority
    TaskHandle_t *pxCreatedTask     // Handle to created task
);
```

### Example 1: Passing a Simple Integer Parameter

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

// Task function that receives an integer parameter
void vBlinkTask(void *pvParameters)
{
    // Cast the void pointer back to the expected type
    uint32_t blinkDelay = (uint32_t)pvParameters;
    
    while(1)
    {
        printf("LED toggling with delay: %lu ms\n", blinkDelay);
        // Toggle LED here
        vTaskDelay(pdMS_TO_TICKS(blinkDelay));
    }
}

void app_main(void)
{
    // Create two tasks with different blink rates
    xTaskCreate(vBlinkTask, "Blink1", 2048, (void*)500, 1, NULL);
    xTaskCreate(vBlinkTask, "Blink2", 2048, (void*)1000, 1, NULL);
    
    vTaskStartScheduler();
}
```

### Example 2: Passing a Structure Parameter

This is the most flexible and commonly used approach for passing multiple parameters:

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

// Structure to hold task parameters
typedef struct {
    uint8_t sensorId;
    uint16_t samplingRate;
    const char *sensorName;
    QueueHandle_t dataQueue;
} SensorTaskParams_t;

void vSensorTask(void *pvParameters)
{
    // Cast parameter to structure pointer
    SensorTaskParams_t *params = (SensorTaskParams_t *)pvParameters;
    
    printf("Sensor Task Started:\n");
    printf("  ID: %d\n", params->sensorId);
    printf("  Name: %s\n", params->sensorName);
    printf("  Rate: %d Hz\n", params->samplingRate);
    
    while(1)
    {
        // Simulate sensor reading
        uint32_t sensorValue = params->sensorId * 100; // Dummy value
        
        // Send data to queue
        xQueueSend(params->dataQueue, &sensorValue, portMAX_DELAY);
        
        vTaskDelay(pdMS_TO_TICKS(1000 / params->samplingRate));
    }
}

void app_main(void)
{
    QueueHandle_t dataQueue = xQueueCreate(10, sizeof(uint32_t));
    
    // IMPORTANT: Parameters must remain valid for the task's lifetime
    // Use static or dynamically allocated memory
    static SensorTaskParams_t tempSensorParams = {
        .sensorId = 1,
        .samplingRate = 10,
        .sensorName = "Temperature",
        .dataQueue = dataQueue
    };
    
    static SensorTaskParams_t humiditySensorParams = {
        .sensorId = 2,
        .samplingRate = 5,
        .sensorName = "Humidity",
        .dataQueue = dataQueue
    };
    
    xTaskCreate(vSensorTask, "TempTask", 2048, &tempSensorParams, 2, NULL);
    xTaskCreate(vSensorTask, "HumidTask", 2048, &humiditySensorParams, 2, NULL);
    
    vTaskStartScheduler();
}
```

### Critical Warning: Parameter Lifetime

```c
// WRONG - Parameter goes out of scope!
void createTaskIncorrectly(void)
{
    int localParam = 42;
    // BAD: localParam will be destroyed when function returns
    xTaskCreate(vMyTask, "Task", 2048, &localParam, 1, NULL);
}

// CORRECT - Using static storage
void createTaskCorrectly(void)
{
    static int staticParam = 42;
    // GOOD: staticParam persists for program lifetime
    xTaskCreate(vMyTask, "Task", 2048, &staticParam, 1, NULL);
}

// CORRECT - Using dynamic allocation
void createTaskWithDynamic(void)
{
    int *heapParam = (int*)pvPortMalloc(sizeof(int));
    *heapParam = 42;
    // GOOD: Memory persists until explicitly freed
    xTaskCreate(vMyTask, "Task", 2048, heapParam, 1, NULL);
}
```

## 2. Sharing Data Between Tasks Safely

FreeRTOS provides several mechanisms for safe inter-task communication:

### Method 1: Queues (Recommended for Most Cases)

Queues provide thread-safe FIFO communication with built-in synchronization:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

typedef struct {
    uint32_t timestamp;
    float temperature;
    uint8_t sensorId;
} SensorData_t;

QueueHandle_t xDataQueue;

// Producer task
void vProducerTask(void *pvParameters)
{
    SensorData_t data;
    uint32_t counter = 0;
    
    while(1)
    {
        // Prepare data
        data.timestamp = xTaskGetTickCount();
        data.temperature = 20.0f + (counter % 10);
        data.sensorId = 1;
        
        // Send to queue (blocks if queue is full)
        if(xQueueSend(xDataQueue, &data, pdMS_TO_TICKS(100)) == pdPASS)
        {
            printf("Produced: Temp=%.1f at %lu\n", 
                   data.temperature, data.timestamp);
        }
        else
        {
            printf("Queue full!\n");
        }
        
        counter++;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Consumer task
void vConsumerTask(void *pvParameters)
{
    SensorData_t receivedData;
    
    while(1)
    {
        // Wait for data (blocks until available)
        if(xQueueReceive(xDataQueue, &receivedData, portMAX_DELAY) == pdPASS)
        {
            printf("Consumed: Temp=%.1f from sensor %d at %lu\n",
                   receivedData.temperature,
                   receivedData.sensorId,
                   receivedData.timestamp);
        }
    }
}

void app_main(void)
{
    // Create queue for 5 sensor data items
    xDataQueue = xQueueCreate(5, sizeof(SensorData_t));
    
    xTaskCreate(vProducerTask, "Producer", 2048, NULL, 2, NULL);
    xTaskCreate(vConsumerTask, "Consumer", 2048, NULL, 2, NULL);
    
    vTaskStartScheduler();
}
```

### Method 2: Mutexes (For Protecting Shared Resources)

Mutexes prevent race conditions when multiple tasks access shared data:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

// Shared resource
typedef struct {
    uint32_t totalCount;
    float averageValue;
    uint32_t lastUpdate;
} SharedStatistics_t;

SharedStatistics_t gStatistics = {0};
SemaphoreHandle_t xStatsMutex;

void vUpdateTask(void *pvParameters)
{
    uint32_t taskId = (uint32_t)pvParameters;
    
    while(1)
    {
        // Acquire mutex before accessing shared data
        if(xSemaphoreTake(xStatsMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            // Critical section - protected by mutex
            gStatistics.totalCount++;
            gStatistics.averageValue = 
                (gStatistics.averageValue * (gStatistics.totalCount - 1) + 
                 taskId * 10.0f) / gStatistics.totalCount;
            gStatistics.lastUpdate = xTaskGetTickCount();
            
            printf("Task %lu updated: count=%lu, avg=%.2f\n",
                   taskId, gStatistics.totalCount, gStatistics.averageValue);
            
            // Release mutex
            xSemaphoreGive(xStatsMutex);
        }
        else
        {
            printf("Task %lu: Mutex timeout!\n", taskId);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500 + taskId * 100));
    }
}

void vReadTask(void *pvParameters)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        if(xSemaphoreTake(xStatsMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            printf("=== Statistics ===\n");
            printf("Total: %lu, Average: %.2f, Last: %lu\n",
                   gStatistics.totalCount,
                   gStatistics.averageValue,
                   gStatistics.lastUpdate);
            
            xSemaphoreGive(xStatsMutex);
        }
    }
}

void app_main(void)
{
    // Create mutex
    xStatsMutex = xSemaphoreCreateMutex();
    
    xTaskCreate(vUpdateTask, "Update1", 2048, (void*)1, 2, NULL);
    xTaskCreate(vUpdateTask, "Update2", 2048, (void*)2, 2, NULL);
    xTaskCreate(vReadTask, "Reader", 2048, NULL, 1, NULL);
    
    vTaskStartScheduler();
}
```

### Method 3: Task Notifications (Lightweight Signaling)

Task notifications are the fastest and most memory-efficient way to signal between tasks:

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

TaskHandle_t xProcessingTaskHandle = NULL;

void vSensorISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Send notification from ISR
    vTaskNotifyGiveFromISR(xProcessingTaskHandle, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vProcessingTask(void *pvParameters)
{
    uint32_t notificationValue;
    
    while(1)
    {
        // Wait for notification (blocks indefinitely)
        notificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        printf("Received %lu notifications\n", notificationValue);
        
        // Process sensor data
        printf("Processing sensor data...\n");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vTriggerTask(void *pvParameters)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Simulate ISR trigger
        printf("Triggering sensor event...\n");
        xTaskNotifyGive(xProcessingTaskHandle);
    }
}

void app_main(void)
{
    xTaskCreate(vProcessingTask, "Processing", 2048, NULL, 3, 
                &xProcessingTaskHandle);
    xTaskCreate(vTriggerTask, "Trigger", 2048, NULL, 2, NULL);
    
    vTaskStartScheduler();
}
```

## 3. Task-Local Storage (TLS)

Task-Local Storage allows each task to have its own private copy of data, similar to thread-local storage in other systems.

### Example: Using Task-Local Storage

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>

// TLS index
#if (configNUM_THREAD_LOCAL_STORAGE_POINTERS > 0)

typedef struct {
    uint32_t callCount;
    char taskName[16];
    uint32_t errorCount;
} TaskLocalData_t;

void vCleanupTLS(int xIndex, void *pvValue)
{
    // Cleanup function called when task is deleted
    if(pvValue != NULL)
    {
        printf("Cleaning up TLS for task\n");
        vPortFree(pvValue);
    }
}

void vWorkerTask(void *pvParameters)
{
    uint32_t taskId = (uint32_t)pvParameters;
    
    // Allocate task-local storage
    TaskLocalData_t *pTaskData = (TaskLocalData_t*)pvPortMalloc(sizeof(TaskLocalData_t));
    pTaskData->callCount = 0;
    pTaskData->errorCount = 0;
    snprintf(pTaskData->taskName, sizeof(pTaskData->taskName), "Worker%lu", taskId);
    
    // Store in TLS (index 0)
    vTaskSetThreadLocalStoragePointer(NULL, 0, pTaskData);
    vTaskSetThreadLocalStoragePointerAndDelCallback(NULL, 0, pTaskData, vCleanupTLS);
    
    for(int i = 0; i < 10; i++)
    {
        // Retrieve from TLS
        TaskLocalData_t *pData = (TaskLocalData_t*)pvTaskGetThreadLocalStoragePointer(NULL, 0);
        
        pData->callCount++;
        
        printf("%s: Call count = %lu\n", pData->taskName, pData->callCount);
        
        vTaskDelay(pdMS_TO_TICKS(500 + taskId * 200));
    }
    
    // Task will be deleted, TLS cleanup function will be called
    vTaskDelete(NULL);
}

void app_main(void)
{
    xTaskCreate(vWorkerTask, "Worker1", 2048, (void*)1, 2, NULL);
    xTaskCreate(vWorkerTask, "Worker2", 2048, (void*)2, 2, NULL);
    
    vTaskStartScheduler();
}

#endif // configNUM_THREAD_LOCAL_STORAGE_POINTERS
```

## Best Practices Summary

1. **Parameter Passing:**
   - Use static or heap-allocated memory for task parameters
   - Prefer structures for passing multiple values
   - Document parameter ownership and lifetime

2. **Data Sharing:**
   - Use queues for producer-consumer patterns
   - Use mutexes for protecting shared resources
   - Use task notifications for simple signaling
   - Avoid global variables without protection

3. **Task-Local Storage:**
   - Use for task-specific state that shouldn't be shared
   - Always provide cleanup callbacks
   - Enable `configNUM_THREAD_LOCAL_STORAGE_POINTERS` in FreeRTOSConfig.h

4. **General Guidelines:**
   - Minimize shared data to reduce complexity
   - Always handle timeout cases when waiting for resources
   - Prefer message passing over shared memory when possible
   - Document synchronization requirements clearly

These mechanisms form the foundation of safe and efficient inter-task communication in FreeRTOS applications.