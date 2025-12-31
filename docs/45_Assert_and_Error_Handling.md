# FreeRTOS Assert and Error Handling

## Overview

Robust error handling is critical for embedded systems running FreeRTOS. The RTOS provides several mechanisms to detect, report, and recover from errors during development and production. The primary tools are `configASSERT()` for catching programming errors, proper handling of allocation failures, and implementing recovery strategies.

## 1. configASSERT() - The Debug Safety Net

### What is configASSERT()?

`configASSERT()` is a macro similar to the standard C `assert()`, but specifically designed for FreeRTOS. It's used throughout the kernel to validate assumptions and catch programming errors early.

### Configuring configASSERT()

In `FreeRTOSConfig.h`:

```c
// Basic implementation - halts execution
#define configASSERT(x) if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }

// Enhanced implementation with debug information
#define configASSERT(x) if((x) == 0) vAssertCalled(__FILE__, __LINE__)

void vAssertCalled(const char *pcFile, uint32_t ulLine)
{
    taskDISABLE_INTERRUPTS();
    
    // Log error information
    printf("ASSERT failed: %s:%lu\r\n", pcFile, ulLine);
    
    // Optional: Store in non-volatile memory for post-mortem analysis
    // saveAssertInfo(pcFile, ulLine);
    
    // Halt system
    for(;;);
}
```

### Practical Example: Queue Validation

```c
void vProducerTask(void *pvParameters)
{
    QueueHandle_t xQueue = (QueueHandle_t)pvParameters;
    uint32_t ulValue = 0;
    BaseType_t xStatus;
    
    // Validate queue handle
    configASSERT(xQueue != NULL);
    
    for(;;)
    {
        ulValue++;
        xStatus = xQueueSend(xQueue, &ulValue, pdMS_TO_TICKS(100));
        
        // This should never fail with proper design
        configASSERT(xStatus == pdPASS);
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### Common configASSERT() Use Cases

```c
// Parameter validation
void vTaskFunction(void *pvParameters)
{
    TaskParams_t *pxParams = (TaskParams_t *)pvParameters;
    configASSERT(pxParams != NULL);
    configASSERT(pxParams->xQueue != NULL);
    configASSERT(pxParams->ulMaxValue > 0);
    
    // Task logic...
}

// Stack overflow detection (already built into FreeRTOS)
// FreeRTOS internally uses configASSERT() to check stack usage

// Mutex validation
void vCriticalFunction(SemaphoreHandle_t xMutex)
{
    configASSERT(xMutex != NULL);
    
    BaseType_t xResult = xSemaphoreTake(xMutex, portMAX_DELAY);
    configASSERT(xResult == pdTRUE);
    
    // Critical section
    
    xResult = xSemaphoreGive(xMutex);
    configASSERT(xResult == pdTRUE);
}
```

## 2. Handling Allocation Failures

### Memory Allocation in FreeRTOS

FreeRTOS allocates memory for tasks, queues, semaphores, and other objects. Allocation can fail when heap memory is exhausted.

### Checking for Allocation Failures

```c
TaskHandle_t xTaskHandle = NULL;
BaseType_t xStatus;

// Always check task creation result
xStatus = xTaskCreate(
    vTaskFunction,
    "Task",
    256,
    NULL,
    2,
    &xTaskHandle
);

if(xStatus != pdPASS)
{
    // Task creation failed - handle the error
    printf("ERROR: Failed to create task\r\n");
    // Take corrective action
}

// Validate handle
configASSERT(xTaskHandle != NULL);
```

### Queue and Semaphore Allocation

```c
QueueHandle_t xQueue;
SemaphoreHandle_t xSemaphore;

// Queue creation can return NULL on failure
xQueue = xQueueCreate(10, sizeof(uint32_t));
if(xQueue == NULL)
{
    // Not enough heap memory
    printf("ERROR: Queue creation failed\r\n");
    // Recovery action needed
}

// Semaphore creation
xSemaphore = xSemaphoreCreateBinary();
if(xSemaphore == NULL)
{
    printf("ERROR: Semaphore creation failed\r\n");
}
```

### Monitoring Heap Usage

```c
void vHeapMonitorTask(void *pvParameters)
{
    size_t xFreeHeap;
    size_t xMinEverFreeHeap;
    
    for(;;)
    {
        xFreeHeap = xPortGetFreeHeapSize();
        xMinEverFreeHeap = xPortGetMinimumEverFreeHeapSize();
        
        printf("Free heap: %u bytes\r\n", xFreeHeap);
        printf("Min ever free: %u bytes\r\n", xMinEverFreeHeap);
        
        // Warning threshold
        if(xFreeHeap < 1024)
        {
            printf("WARNING: Low heap memory!\r\n");
            // Take preventive action
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## 3. Robust Error Recovery Mechanisms

### System-Wide Error Handler

```c
typedef enum
{
    ERROR_NONE = 0,
    ERROR_HEAP_EXHAUSTED,
    ERROR_TASK_CREATION_FAILED,
    ERROR_QUEUE_FULL,
    ERROR_SENSOR_FAILURE,
    ERROR_COMMUNICATION_TIMEOUT,
    ERROR_WATCHDOG_TRIGGERED
} SystemError_t;

typedef struct
{
    SystemError_t eErrorCode;
    const char *pcErrorMessage;
    uint32_t ulTimestamp;
    TaskHandle_t xTaskHandle;
} ErrorInfo_t;

// Error queue for centralized handling
QueueHandle_t xErrorQueue;

void vErrorHandlerTask(void *pvParameters)
{
    ErrorInfo_t xError;
    
    for(;;)
    {
        if(xQueueReceive(xErrorQueue, &xError, portMAX_DELAY) == pdPASS)
        {
            // Log error
            printf("[%lu] ERROR in task %p: %s (code: %d)\r\n",
                   xError.ulTimestamp,
                   xError.xTaskHandle,
                   xError.pcErrorMessage,
                   xError.eErrorCode);
            
            // Take recovery action based on error type
            switch(xError.eErrorCode)
            {
                case ERROR_HEAP_EXHAUSTED:
                    vHandleHeapError();
                    break;
                    
                case ERROR_SENSOR_FAILURE:
                    vResetSensor();
                    break;
                    
                case ERROR_COMMUNICATION_TIMEOUT:
                    vReinitializeCommunication();
                    break;
                    
                default:
                    // Generic error handling
                    break;
            }
        }
    }
}

void vReportError(SystemError_t eError, const char *pcMessage)
{
    ErrorInfo_t xError;
    
    xError.eErrorCode = eError;
    xError.pcErrorMessage = pcMessage;
    xError.ulTimestamp = xTaskGetTickCount();
    xError.xTaskHandle = xTaskGetCurrentTaskHandle();
    
    // Non-blocking send to avoid deadlock
    xQueueSend(xErrorQueue, &xError, 0);
}
```

### Watchdog Integration

```c
// Hardware watchdog management
#define WATCHDOG_TIMEOUT_MS 5000

TaskHandle_t xCriticalTaskHandle;
uint32_t ulLastWatchdogKick = 0;

void vWatchdogTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        // Check if critical task is still running
        if(eTaskGetState(xCriticalTaskHandle) == eRunning ||
           eTaskGetState(xCriticalTaskHandle) == eReady)
        {
            // Kick the watchdog
            HAL_IWDG_Refresh(&hiwdg);
            ulLastWatchdogKick = xTaskGetTickCount();
        }
        else
        {
            // Critical task hung - log and reset
            vReportError(ERROR_WATCHDOG_TRIGGERED, "Critical task hung");
            // System will reset when watchdog expires
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}
```

### Graceful Degradation Example

```c
typedef enum
{
    MODE_NORMAL,
    MODE_DEGRADED,
    MODE_SAFE
} SystemMode_t;

SystemMode_t eCurrentMode = MODE_NORMAL;

void vSensorTask(void *pvParameters)
{
    SensorData_t xData;
    BaseType_t xReadResult;
    uint32_t ulFailureCount = 0;
    
    for(;;)
    {
        xReadResult = xReadSensor(&xData);
        
        if(xReadResult != pdPASS)
        {
            ulFailureCount++;
            
            if(ulFailureCount > 3)
            {
                // Switch to degraded mode - use cached values
                eCurrentMode = MODE_DEGRADED;
                vReportError(ERROR_SENSOR_FAILURE, "Sensor unreliable");
                
                if(ulFailureCount > 10)
                {
                    // Switch to safe mode - stop non-essential tasks
                    eCurrentMode = MODE_SAFE;
                    vSuspendNonEssentialTasks();
                }
            }
        }
        else
        {
            // Sensor recovered
            if(ulFailureCount > 0)
            {
                ulFailureCount = 0;
                eCurrentMode = MODE_NORMAL;
                vResumeNonEssentialTasks();
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Malloc Failed Hook

```c
// Called when pvPortMalloc fails
void vApplicationMallocFailedHook(void)
{
    // Disable interrupts to prevent further allocations
    taskDISABLE_INTERRUPTS();
    
    // Log the failure
    printf("FATAL: Heap allocation failed!\r\n");
    printf("Free heap: %u bytes\r\n", xPortGetFreeHeapSize());
    
    // Attempt to save diagnostic information
    vSaveDiagnostics();
    
    // System cannot continue reliably
    for(;;);
}
```

### Stack Overflow Hook

```c
// Called when stack overflow is detected
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    taskDISABLE_INTERRUPTS();
    
    printf("FATAL: Stack overflow in task: %s\r\n", pcTaskName);
    
    // Log task information
    UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(xTask);
    printf("Stack high water mark: %u\r\n", uxHighWaterMark);
    
    // Save crash information
    vSaveCrashInfo(xTask, pcTaskName);
    
    for(;;);
}
```

## Best Practices

1. **Enable configASSERT() during development** - Disable in production if needed for performance, but keep comprehensive error handling
2. **Always check return values** - Never assume operations succeed
3. **Monitor heap usage** - Track minimum free heap size to prevent allocation failures
4. **Implement centralized error handling** - Use a dedicated error handler task
5. **Design for graceful degradation** - System should degrade functionality rather than crash completely
6. **Use hardware watchdogs** - Protect against complete system hangs
7. **Log error information** - Store error data for post-mortem analysis
8. **Test error paths** - Deliberately inject faults to verify error handling works
9. **Reserve emergency heap** - Keep a small memory reserve for error handling
10. **Implement recovery strategies** - Restart tasks, reinitialize hardware, or reset system as appropriate

These mechanisms work together to create resilient FreeRTOS applications that can detect, report, and recover from errors effectively.