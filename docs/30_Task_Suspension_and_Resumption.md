# Task Suspension and Resumption in FreeRTOS

## Overview

Task suspension is a mechanism in FreeRTOS that allows you to temporarily halt a task's execution without deleting it. A suspended task is completely removed from the scheduler's consideration and will not run until explicitly resumed. This differs from blocking, where a task waits for an event or timeout but remains in the scheduler's purview.

## Key Functions

### vTaskSuspend()
```c
void vTaskSuspend(TaskHandle_t xTaskToSuspend);
```
Suspends a task. If `NULL` is passed, the calling task suspends itself.

### vTaskResume()
```c
void vTaskResume(TaskHandle_t xTaskToResume);
```
Resumes a previously suspended task. Can only be called from a task context.

### xTaskResumeFromISR()
```c
BaseType_t xTaskResumeFromISR(TaskHandle_t xTaskToResume);
```
ISR-safe version of resume. Returns `pdTRUE` if resuming the task should result in a context switch.

## The Suspended State

A task in the suspended state:
- Is not available to the scheduler
- Does not consume CPU time
- Retains its stack and task control block
- Can be suspended multiple times (requires equal resume calls)
- Cannot be awakened by events, timeouts, or notifications
- Must be explicitly resumed to run again

## Practical Examples

### Example 1: Basic Suspension and Resumption

```c
#include "FreeRTOS.h"
#include "task.h"

TaskHandle_t xWorkerTaskHandle = NULL;

void vWorkerTask(void *pvParameters)
{
    uint32_t ulCounter = 0;
    
    while(1)
    {
        printf("Worker task running: %lu\n", ulCounter++);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vControlTask(void *pvParameters)
{
    while(1)
    {
        // Run worker for 5 seconds
        printf("Resuming worker task\n");
        vTaskResume(xWorkerTaskHandle);
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        // Suspend worker for 3 seconds
        printf("Suspending worker task\n");
        vTaskSuspend(xWorkerTaskHandle);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

int main(void)
{
    // Create worker task in suspended state
    xTaskCreate(vWorkerTask, "Worker", 200, NULL, 1, &xWorkerTaskHandle);
    vTaskSuspend(xWorkerTaskHandle); // Start suspended
    
    xTaskCreate(vControlTask, "Control", 200, NULL, 2, NULL);
    
    vTaskStartScheduler();
    return 0;
}
```

### Example 2: Power Management System

```c
#include "FreeRTOS.h"
#include "task.h"

// Task handles
TaskHandle_t xSensorTaskHandle = NULL;
TaskHandle_t xDisplayTaskHandle = NULL;
TaskHandle_t xLoggingTaskHandle = NULL;

typedef enum {
    POWER_MODE_FULL,
    POWER_MODE_LOW,
    POWER_MODE_SLEEP
} PowerMode_t;

void vSensorTask(void *pvParameters)
{
    while(1)
    {
        // Read sensors
        float temperature = readTemperatureSensor();
        float pressure = readPressureSensor();
        
        printf("Sensors: Temp=%.1f, Press=%.1f\n", temperature, pressure);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vDisplayTask(void *pvParameters)
{
    while(1)
    {
        // Update display
        updateDisplay();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vLoggingTask(void *pvParameters)
{
    while(1)
    {
        // Log data to SD card
        logDataToSD();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void vPowerManagerTask(void *pvParameters)
{
    PowerMode_t currentMode = POWER_MODE_FULL;
    
    while(1)
    {
        // Check battery level
        uint8_t batteryLevel = getBatteryLevel();
        
        if(batteryLevel > 50 && currentMode != POWER_MODE_FULL)
        {
            // Full power mode
            printf("Switching to FULL power mode\n");
            vTaskResume(xSensorTaskHandle);
            vTaskResume(xDisplayTaskHandle);
            vTaskResume(xLoggingTaskHandle);
            currentMode = POWER_MODE_FULL;
        }
        else if(batteryLevel > 20 && batteryLevel <= 50 && 
                currentMode != POWER_MODE_LOW)
        {
            // Low power mode - suspend logging
            printf("Switching to LOW power mode\n");
            vTaskResume(xSensorTaskHandle);
            vTaskResume(xDisplayTaskHandle);
            vTaskSuspend(xLoggingTaskHandle);
            currentMode = POWER_MODE_LOW;
        }
        else if(batteryLevel <= 20 && currentMode != POWER_MODE_SLEEP)
        {
            // Sleep mode - only sensors
            printf("Switching to SLEEP mode\n");
            vTaskResume(xSensorTaskHandle);
            vTaskSuspend(xDisplayTaskHandle);
            vTaskSuspend(xLoggingTaskHandle);
            currentMode = POWER_MODE_SLEEP;
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int main(void)
{
    xTaskCreate(vSensorTask, "Sensor", 200, NULL, 2, &xSensorTaskHandle);
    xTaskCreate(vDisplayTask, "Display", 200, NULL, 2, &xDisplayTaskHandle);
    xTaskCreate(vLoggingTask, "Logging", 200, NULL, 1, &xLoggingTaskHandle);
    xTaskCreate(vPowerManagerTask, "PowerMgr", 200, NULL, 3, NULL);
    
    vTaskStartScheduler();
    return 0;
}
```

### Example 3: Resume from ISR

```c
#include "FreeRTOS.h"
#include "task.h"

TaskHandle_t xDataProcessingTaskHandle = NULL;
volatile uint8_t dataBuffer[256];
volatile uint8_t dataReady = 0;

void vDataProcessingTask(void *pvParameters)
{
    while(1)
    {
        // Task starts, processes data, then suspends itself
        printf("Processing data...\n");
        processData(dataBuffer, 256);
        dataReady = 0;
        
        // Suspend until next interrupt
        vTaskSuspend(NULL); // Suspend self
    }
}

// UART receive complete interrupt
void UART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Copy received data
    readUARTData(dataBuffer, 256);
    dataReady = 1;
    
    // Resume the processing task
    xTaskResumeFromISR(xDataProcessingTaskHandle);
    
    // Request context switch if necessary
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int main(void)
{
    xTaskCreate(vDataProcessingTask, "DataProc", 300, NULL, 2, 
                &xDataProcessingTaskHandle);
    
    // Start with task suspended
    vTaskSuspend(xDataProcessingTaskHandle);
    
    enableUARTInterrupt();
    vTaskStartScheduler();
    return 0;
}
```

### Example 4: Diagnostic Mode

```c
#include "FreeRTOS.h"
#include "task.h"

TaskHandle_t xNormalTask1Handle = NULL;
TaskHandle_t xNormalTask2Handle = NULL;
TaskHandle_t xDiagnosticTaskHandle = NULL;

void vNormalTask1(void *pvParameters)
{
    while(1)
    {
        // Normal operation
        performNormalWork1();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vNormalTask2(void *pvParameters)
{
    while(1)
    {
        // Normal operation
        performNormalWork2();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void vDiagnosticTask(void *pvParameters)
{
    while(1)
    {
        // Run diagnostics
        printf("Running system diagnostics...\n");
        runMemoryTest();
        runPeripheralTest();
        runCommunicationTest();
        printf("Diagnostics complete\n");
        
        // Suspend self until needed again
        vTaskSuspend(NULL);
    }
}

void enterDiagnosticMode(void)
{
    printf("Entering diagnostic mode\n");
    
    // Suspend normal tasks
    vTaskSuspend(xNormalTask1Handle);
    vTaskSuspend(xNormalTask2Handle);
    
    // Resume diagnostic task
    vTaskResume(xDiagnosticTaskHandle);
}

void exitDiagnosticMode(void)
{
    printf("Exiting diagnostic mode\n");
    
    // Suspend diagnostic task
    vTaskSuspend(xDiagnosticTaskHandle);
    
    // Resume normal tasks
    vTaskResume(xNormalTask1Handle);
    vTaskResume(xNormalTask2Handle);
}

// Button interrupt handler
void BUTTON_IRQHandler(void)
{
    static uint8_t diagnosticMode = 0;
    
    if(diagnosticMode == 0)
    {
        enterDiagnosticMode();
        diagnosticMode = 1;
    }
    else
    {
        exitDiagnosticMode();
        diagnosticMode = 0;
    }
}
```

## Common Use Cases

### 1. **Power Management**
Suspend non-critical tasks during low-battery conditions to conserve power.

### 2. **Mode Switching**
Suspend tasks that aren't needed in the current operational mode (normal, diagnostic, calibration).

### 3. **Event-Driven Processing**
Tasks that only need to run when specific events occur can suspend themselves between events.

### 4. **Resource Sharing**
Temporarily suspend tasks to give another task exclusive access to a shared resource.

### 5. **Testing and Debugging**
Isolate specific tasks during development to observe system behavior.

## Important Considerations

**Avoid Overuse**: Suspension should be used sparingly. In most cases, blocking mechanisms like semaphores, queues, or notifications are more appropriate.

**No Automatic Wake**: Unlike blocked tasks, suspended tasks don't wake on timeouts or events. You must explicitly resume them.

**Nested Suspension**: If you suspend a task multiple times, you must resume it an equal number of times.

**Priority Inversion**: Suspending a task holding a mutex can lead to priority inversion problems.

**ISR Safety**: Always use `xTaskResumeFromISR()` from interrupt contexts, never `vTaskResume()`.

**State Loss**: If a task is suspended while waiting on a queue or semaphore, it loses that wait state.

## Suspension vs. Blocking

| Aspect | Suspension | Blocking |
|--------|-----------|----------|
| Control | External (another task/ISR) | Self-initiated |
| Wake condition | Explicit resume call | Event/timeout |
| Scheduler involvement | Removed from scheduler | Remains in scheduler |
| Use case | Power saving, mode switching | Waiting for resources |
| Flexibility | Manual management | Automatic |

Task suspension is a powerful tool for managing system behavior, but it requires careful design to avoid creating tasks that never resume or systems that become difficult to debug. Use it when you need explicit external control over task execution, and prefer standard blocking mechanisms for routine synchronization.