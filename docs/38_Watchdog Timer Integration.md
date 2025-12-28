# Watchdog Timer Integration in FreeRTOS

## Overview

A **watchdog timer** is a hardware timer that resets the system if software fails to periodically "feed" (reset) it. This safety mechanism protects against system hangs, infinite loops, and task failures. In FreeRTOS systems, proper watchdog integration ensures that critical tasks are running correctly and the system remains responsive.

## Core Concepts

### Watchdog Timer Fundamentals

The watchdog timer operates on a simple principle:
- **Initialize**: Configure the watchdog with a timeout period
- **Feed/Refresh**: Periodically reset the timer before it expires
- **Reset**: If not fed in time, the watchdog triggers a system reset

### Integration Challenges in RTOS

In a multitasking environment like FreeRTOS, watchdog integration becomes more complex because:
- Multiple tasks must coordinate to feed the watchdog
- A single stuck task shouldn't prevent other tasks from feeding the watchdog
- The system needs to verify that *all* critical tasks are healthy, not just one

## Watchdog Feeding Strategies

### Strategy 1: Single Task Feeding (Simple but Limited)

The simplest approach where one dedicated task feeds the watchdog periodically.

```c
#include "FreeRTOS.h"
#include "task.h"

// Hardware-specific watchdog functions (example for STM32)
extern void HAL_IWDG_Refresh(void);
extern void HAL_IWDG_Init(uint32_t timeout_ms);

void vWatchdogTask(void *pvParameters)
{
    const TickType_t xFeedInterval = pdMS_TO_TICKS(500); // Feed every 500ms
    
    // Initialize watchdog with 1000ms timeout
    HAL_IWDG_Init(1000);
    
    for(;;)
    {
        // Feed the watchdog
        HAL_IWDG_Refresh();
        
        // Wait before next feed
        vTaskDelay(xFeedInterval);
    }
}

void app_main(void)
{
    xTaskCreate(vWatchdogTask, "WDT", 256, NULL, 
                configMAX_PRIORITIES - 1, NULL); // Highest priority
    
    // Create other application tasks...
    
    vTaskStartScheduler();
}
```

**Limitations**: This only verifies the scheduler is running, not that application tasks are healthy.

### Strategy 2: Multi-Task Health Monitoring (Recommended)

A more robust approach where multiple tasks must check in before feeding the watchdog.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define NUM_MONITORED_TASKS 3

// Bit flags for each monitored task
#define TASK_SENSOR_BIT     (1 << 0)
#define TASK_COMM_BIT       (1 << 1)
#define TASK_CONTROL_BIT    (1 << 2)
#define ALL_TASKS_OK        ((1 << NUM_MONITORED_TASKS) - 1)

static volatile uint32_t ulTaskHealthStatus = 0;
static SemaphoreHandle_t xHealthMutex;

// Task health check-in function
void vTaskCheckIn(uint32_t ulTaskBit)
{
    xSemaphoreTake(xHealthMutex, portMAX_DELAY);
    ulTaskHealthStatus |= ulTaskBit;
    xSemaphoreGive(xHealthMutex);
}

void vWatchdogMonitorTask(void *pvParameters)
{
    const TickType_t xMonitorInterval = pdMS_TO_TICKS(800);
    uint32_t ulHealthSnapshot;
    
    HAL_IWDG_Init(1000); // 1 second timeout
    
    for(;;)
    {
        vTaskDelay(xMonitorInterval);
        
        // Check if all tasks have checked in
        xSemaphoreTake(xHealthMutex, portMAX_DELAY);
        ulHealthSnapshot = ulTaskHealthStatus;
        ulTaskHealthStatus = 0; // Reset for next cycle
        xSemaphoreGive(xHealthMutex);
        
        if(ulHealthSnapshot == ALL_TASKS_OK)
        {
            // All tasks are healthy - feed the watchdog
            HAL_IWDG_Refresh();
        }
        else
        {
            // One or more tasks failed to check in
            // Log the failure (if time permits before reset)
            printf("Watchdog: Task health failure 0x%lx\n", 
                   ulHealthSnapshot);
            
            // Option 1: Don't feed watchdog - let it reset the system
            // Option 2: Perform emergency actions before reset
            
            // System will reset due to watchdog timeout
        }
    }
}

// Example monitored task
void vSensorTask(void *pvParameters)
{
    for(;;)
    {
        // Do sensor reading work
        read_sensors();
        process_sensor_data();
        
        // Check in to indicate task is healthy
        vTaskCheckIn(TASK_SENSOR_BIT);
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vCommunicationTask(void *pvParameters)
{
    for(;;)
    {
        // Handle communication
        process_messages();
        send_data();
        
        vTaskCheckIn(TASK_COMM_BIT);
        
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void vControlTask(void *pvParameters)
{
    for(;;)
    {
        // Control logic
        update_control_outputs();
        
        vTaskCheckIn(TASK_CONTROL_BIT);
        
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

void app_main(void)
{
    xHealthMutex = xSemaphoreCreateMutex();
    
    xTaskCreate(vWatchdogMonitorTask, "WDT_MON", 512, NULL, 
                configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(vSensorTask, "SENSOR", 512, NULL, 2, NULL);
    xTaskCreate(vCommunicationTask, "COMM", 512, NULL, 2, NULL);
    xTaskCreate(vControlTask, "CTRL", 512, NULL, 2, NULL);
    
    vTaskStartScheduler();
}
```

### Strategy 3: Software Watchdog with Task Monitoring

Implementing a software watchdog layer before the hardware watchdog provides more control.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#define MAX_MONITORED_TASKS 5

typedef struct {
    const char *pcTaskName;
    uint32_t ulMaxCheckInTime_ms;
    TickType_t xLastCheckIn;
    BaseType_t xActive;
} TaskMonitor_t;

static TaskMonitor_t xTaskMonitors[MAX_MONITORED_TASKS];
static uint8_t ucNumRegisteredTasks = 0;
static TimerHandle_t xWatchdogTimer;

// Register a task for monitoring
BaseType_t xRegisterTaskForMonitoring(const char *pcName, 
                                      uint32_t ulMaxPeriod_ms)
{
    if(ucNumRegisteredTasks >= MAX_MONITORED_TASKS)
        return pdFAIL;
    
    TaskMonitor_t *pxMonitor = &xTaskMonitors[ucNumRegisteredTasks];
    pxMonitor->pcTaskName = pcName;
    pxMonitor->ulMaxCheckInTime_ms = ulMaxPeriod_ms;
    pxMonitor->xLastCheckIn = xTaskGetTickCount();
    pxMonitor->xActive = pdTRUE;
    
    ucNumRegisteredTasks++;
    return pdPASS;
}

// Task check-in function
void vTaskAlive(const char *pcTaskName)
{
    TickType_t xCurrentTime = xTaskGetTickCount();
    
    for(uint8_t i = 0; i < ucNumRegisteredTasks; i++)
    {
        if(strcmp(xTaskMonitors[i].pcTaskName, pcTaskName) == 0)
        {
            xTaskMonitors[i].xLastCheckIn = xCurrentTime;
            return;
        }
    }
}

// Software watchdog timer callback
void vSoftwareWatchdogCallback(TimerHandle_t xTimer)
{
    TickType_t xCurrentTime = xTaskGetTickCount();
    BaseType_t xAllTasksHealthy = pdTRUE;
    
    // Check each registered task
    for(uint8_t i = 0; i < ucNumRegisteredTasks; i++)
    {
        if(!xTaskMonitors[i].xActive)
            continue;
            
        TickType_t xTimeSinceCheckIn = xCurrentTime - 
                                       xTaskMonitors[i].xLastCheckIn;
        uint32_t ulTimeSinceCheckIn_ms = 
            (xTimeSinceCheckIn * 1000) / configTICK_RATE_HZ;
        
        if(ulTimeSinceCheckIn_ms > xTaskMonitors[i].ulMaxCheckInTime_ms)
        {
            // Task has exceeded its check-in period
            printf("ERROR: Task '%s' has not checked in for %lu ms\n",
                   xTaskMonitors[i].pcTaskName, ulTimeSinceCheckIn_ms);
            xAllTasksHealthy = pdFALSE;
        }
    }
    
    if(xAllTasksHealthy)
    {
        // All tasks healthy - feed hardware watchdog
        HAL_IWDG_Refresh();
    }
    else
    {
        // Don't feed watchdog - system will reset
        printf("CRITICAL: Software watchdog triggered - system will reset\n");
        
        // Optional: Save diagnostic info to non-volatile memory
        save_crash_info();
    }
}

void vInitializeSoftwareWatchdog(void)
{
    // Create software watchdog timer (runs every 200ms)
    xWatchdogTimer = xTimerCreate("SW_WDT", 
                                  pdMS_TO_TICKS(200),
                                  pdTRUE,  // Auto-reload
                                  NULL,
                                  vSoftwareWatchdogCallback);
    
    // Initialize hardware watchdog with 1 second timeout
    HAL_IWDG_Init(1000);
    
    // Start the software watchdog timer
    xTimerStart(xWatchdogTimer, 0);
}

// Example usage
void vCriticalTask(void *pvParameters)
{
    xRegisterTaskForMonitoring("CRITICAL", 500); // Must check in every 500ms
    
    for(;;)
    {
        // Perform critical operations
        perform_critical_work();
        
        // Indicate task is alive
        vTaskAlive("CRITICAL");
        
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}
```

## Handling Watchdog Failures

### Pre-Reset Diagnostics

When a watchdog reset is imminent, save diagnostic information:

```c
#include "FreeRTOS.h"
#include "task.h"

typedef struct {
    uint32_t ulResetCount;
    uint32_t ulLastResetReason;
    uint32_t ulTaskStates[10];
    TickType_t xLastResetTime;
    char pcFailedTask[16];
} WatchdogDiagnostics_t;

// In non-volatile memory or special RAM section
__attribute__((section(".noinit"))) WatchdogDiagnostics_t xWatchdogDiag;

void vSaveWatchdogDiagnostics(const char *pcFailedTaskName)
{
    xWatchdogDiag.ulResetCount++;
    xWatchdogDiag.ulLastResetReason = RESET_REASON_WATCHDOG;
    xWatchdogDiag.xLastResetTime = xTaskGetTickCount();
    
    if(pcFailedTaskName)
    {
        strncpy(xWatchdogDiag.pcFailedTask, pcFailedTaskName, 15);
        xWatchdogDiag.pcFailedTask[15] = '\0';
    }
    
    // Save task states
    TaskStatus_t xTaskStatusArray[10];
    UBaseType_t uxTaskCount = uxTaskGetSystemState(xTaskStatusArray, 
                                                    10, NULL);
    
    for(UBaseType_t i = 0; i < uxTaskCount && i < 10; i++)
    {
        xWatchdogDiag.ulTaskStates[i] = xTaskStatusArray[i].eCurrentState;
    }
}

void vCheckAndReportWatchdogReset(void)
{
    // Check if last reset was due to watchdog
    if(__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
    {
        printf("=== WATCHDOG RESET DETECTED ===\n");
        printf("Reset count: %lu\n", xWatchdogDiag.ulResetCount);
        printf("Failed task: %s\n", xWatchdogDiag.pcFailedTask);
        printf("Time of reset: %lu ticks\n", xWatchdogDiag.xLastResetTime);
        
        // Send diagnostics to logging system
        log_watchdog_event(&xWatchdogDiag);
        
        // Clear the watchdog reset flag
        __HAL_RCC_CLEAR_RESET_FLAGS();
    }
}
```

### Watchdog Suspension During Debug

During development, you may want to disable the watchdog:

```c
void vWatchdogTask(void *pvParameters)
{
    #ifndef DEBUG_BUILD
    HAL_IWDG_Init(1000);
    #endif
    
    for(;;)
    {
        #ifndef DEBUG_BUILD
        if(xAllTasksHealthy())
        {
            HAL_IWDG_Refresh();
        }
        #endif
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

## Advanced Techniques

### Adaptive Watchdog Timeout

Adjust watchdog timeout based on system mode:

```c
typedef enum {
    SYSTEM_MODE_NORMAL,
    SYSTEM_MODE_HIGH_LOAD,
    SYSTEM_MODE_LOW_POWER
} SystemMode_t;

void vAdjustWatchdogTimeout(SystemMode_t eMode)
{
    switch(eMode)
    {
        case SYSTEM_MODE_NORMAL:
            HAL_IWDG_Init(1000);  // 1 second
            break;
            
        case SYSTEM_MODE_HIGH_LOAD:
            HAL_IWDG_Init(2000);  // 2 seconds - more tolerance
            break;
            
        case SYSTEM_MODE_LOW_POWER:
            HAL_IWDG_Init(5000);  // 5 seconds - CPU may be slower
            break;
    }
}
```

### Window Watchdog

Some microcontrollers provide window watchdogs that must be fed within a specific time window:

```c
// Must feed between 500ms and 1000ms
void vWindowWatchdogTask(void *pvParameters)
{
    const TickType_t xMinDelay = pdMS_TO_TICKS(500);
    const TickType_t xMaxDelay = pdMS_TO_TICKS(1000);
    
    HAL_WWDG_Init(500, 1000); // Min: 500ms, Max: 1000ms
    
    TickType_t xLastFeed = xTaskGetTickCount();
    
    for(;;)
    {
        TickType_t xNow = xTaskGetTickCount();
        TickType_t xElapsed = xNow - xLastFeed;
        
        if(xElapsed >= xMinDelay && xElapsed < xMaxDelay)
        {
            if(xAllTasksHealthy())
            {
                HAL_WWDG_Refresh();
                xLastFeed = xNow;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Best Practices

1. **Set appropriate timeouts**: Watchdog timeout should be 2-3x the longest expected task delay
2. **High priority watchdog task**: The watchdog feeding task should have high priority to ensure it runs
3. **Monitor critical tasks only**: Don't monitor every task - focus on safety-critical operations
4. **Test watchdog behavior**: Regularly test that watchdog actually resets the system when tasks fail
5. **Log watchdog events**: Always log watchdog resets for post-mortem analysis
6. **Graceful degradation**: Consider if partial functionality is better than a full reset
7. **Consider startup time**: Ensure watchdog timeout accounts for system initialization time

Proper watchdog integration provides a crucial safety net for embedded systems, ensuring they recover from unexpected failures and maintain operational reliability.