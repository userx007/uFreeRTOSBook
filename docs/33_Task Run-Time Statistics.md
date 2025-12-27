# Task Run-Time Statistics in FreeRTOS

Task Run-Time Statistics is a powerful profiling feature in FreeRTOS that allows developers to measure and analyze how much CPU time each task consumes. This capability is essential for understanding system behavior, identifying performance bottlenecks, and optimizing resource utilization in real-time embedded systems.

## Overview

Run-time statistics provide visibility into:
- **CPU utilization per task** - How much processing time each task uses
- **Total system runtime** - Overall execution time tracking
- **Task execution patterns** - Understanding which tasks dominate CPU usage
- **Idle time** - How much time the system spends in the idle task

This information is invaluable for performance tuning, capacity planning, and ensuring real-time deadlines are met.

## Configuration Requirements

To enable run-time statistics collection, you need to configure several macros in `FreeRTOSConfig.h`:

```c
/* Enable run-time statistics collection */
#define configGENERATE_RUN_TIME_STATS           1

/* Enable task statistics retrieval API */
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/* Enable trace facility (required for statistics) */
#define configUSE_TRACE_FACILITY                1

/* Define macros for timer configuration */
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()  vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()          ulGetRunTimeCounterValue()
```

### High-Resolution Timer Setup

The statistics system requires a high-resolution timer (typically 10-20x faster than the system tick) for accurate measurements:

```c
/* Example: STM32 timer configuration for run-time statistics */
static uint32_t ulRunTimeCounter = 0;

void vConfigureTimerForRunTimeStats(void)
{
    /* Configure a hardware timer to increment at 10x the tick rate
     * For example, if tick rate is 1000 Hz, timer should run at 10 kHz
     */
    
    // Enable timer clock
    __HAL_RCC_TIM2_CLK_ENABLE();
    
    // Configure timer for precise timing
    TIM_HandleTypeDef htim2;
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = (SystemCoreClock / 10000) - 1; // 10 kHz
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFF; // Maximum period
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    
    HAL_TIM_Base_Init(&htim2);
    HAL_TIM_Base_Start(&htim2);
}

uint32_t ulGetRunTimeCounterValue(void)
{
    return __HAL_TIM_GET_COUNTER(&htim2);
}
```

### Alternative: Software-Based Counter

For systems without available hardware timers:

```c
volatile uint32_t ulHighFrequencyTimerTicks = 0;

void vConfigureTimerForRunTimeStats(void)
{
    // No hardware timer configuration needed
    ulHighFrequencyTimerTicks = 0;
}

uint32_t ulGetRunTimeCounterValue(void)
{
    return ulHighFrequencyTimerTicks;
}

/* Called from a high-frequency interrupt (e.g., 10x tick rate) */
void vIncrementRunTimeCounter(void)
{
    ulHighFrequencyTimerTicks++;
}
```

## Collecting Statistics

FreeRTOS provides two main APIs for retrieving run-time statistics:

### 1. Raw Statistics Data

```c
/* Structure to hold task statistics */
typedef struct
{
    TaskHandle_t xHandle;          // Task handle
    const char *pcTaskName;        // Task name
    uint32_t ulRunTimeCounter;     // Total runtime in timer ticks
    uint32_t ulPercentage;         // CPU usage percentage (if calculated)
} TaskStatus_t;

/* Get statistics for all tasks */
UBaseType_t uxTaskGetSystemState(TaskStatus_t *pxTaskStatusArray,
                                  const UBaseType_t uxArraySize,
                                  uint32_t *pulTotalRunTime);
```

### 2. Formatted Statistics String

```c
/* Generate human-readable statistics report */
void vTaskGetRunTimeStats(char *pcWriteBuffer);

/* Get list of tasks with their states */
void vTaskList(char *pcWriteBuffer);
```

## Practical Examples

### Example 1: Basic Statistics Monitoring

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* Task priorities */
#define SENSOR_TASK_PRIORITY    (tskIDLE_PRIORITY + 2)
#define PROCESS_TASK_PRIORITY   (tskIDLE_PRIORITY + 3)
#define DISPLAY_TASK_PRIORITY   (tskIDLE_PRIORITY + 1)
#define STATS_TASK_PRIORITY     (tskIDLE_PRIORITY + 4)

/* Simulated sensor task - periodic sampling */
void vSensorTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms period
    
    for (;;)
    {
        // Simulate sensor reading (takes time)
        for (volatile uint32_t i = 0; i < 50000; i++);
        
        // Process sensor data
        uint16_t sensorValue = readSensor();
        sendToQueue(sensorValue);
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* CPU-intensive processing task */
void vProcessTask(void *pvParameters)
{
    for (;;)
    {
        // Wait for data
        uint16_t data;
        if (receiveFromQueue(&data, portMAX_DELAY))
        {
            // Heavy computation
            for (volatile uint32_t i = 0; i < 100000; i++)
            {
                data = (data * 3 + 7) % 1000;
            }
            
            // Store result
            storeProcessedData(data);
        }
    }
}

/* Display update task - lower priority */
void vDisplayTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(500); // 500ms period
    
    for (;;)
    {
        // Update display (moderate CPU usage)
        for (volatile uint32_t i = 0; i < 30000; i++);
        updateDisplay();
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* Statistics monitoring task */
void vStatsTask(void *pvParameters)
{
    char statsBuffer[512];
    
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Report every 5 seconds
        
        printf("\n=== Task Run-Time Statistics ===\n");
        vTaskGetRunTimeStats(statsBuffer);
        printf("%s\n", statsBuffer);
        
        printf("\n=== Task State Information ===\n");
        vTaskList(statsBuffer);
        printf("%s\n", statsBuffer);
    }
}

int main(void)
{
    // Create tasks
    xTaskCreate(vSensorTask, "Sensor", 256, NULL, SENSOR_TASK_PRIORITY, NULL);
    xTaskCreate(vProcessTask, "Process", 512, NULL, PROCESS_TASK_PRIORITY, NULL);
    xTaskCreate(vDisplayTask, "Display", 256, NULL, DISPLAY_TASK_PRIORITY, NULL);
    xTaskCreate(vStatsTask, "Stats", 512, NULL, STATS_TASK_PRIORITY, NULL);
    
    // Start scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    for (;;);
}
```

**Example Output:**
```
=== Task Run-Time Statistics ===
Task            Abs Time        % Time
*************************************************
Stats           1234            1%
Process         45678           45%
Sensor          23456           23%
Display         12345           12%
IDLE            18987           19%

=== Task State Information ===
Name            State   Priority    Stack   Num
*************************************************
Stats           R       4           245     4
Process         B       3           378     2
Sensor          B       2           198     1
Display         B       1           234     3
IDLE            R       0           112     5
```

### Example 2: Performance Analysis and Optimization

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* Structure for detailed performance analysis */
typedef struct
{
    char taskName[configMAX_TASK_NAME_LEN];
    uint32_t runTime;
    float cpuPercentage;
    uint32_t stackHighWaterMark;
} PerformanceMetrics_t;

/* Analyze system performance */
void analyzeSystemPerformance(void)
{
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    uint32_t ulTotalRunTime, ulStatsAsPercentage;
    
    // Get number of tasks
    uxArraySize = uxTaskGetNumberOfTasks();
    
    // Allocate array for task status
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    
    if (pxTaskStatusArray != NULL)
    {
        // Get task statistics
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, 
                                           uxArraySize, 
                                           &ulTotalRunTime);
        
        // Calculate percentages
        printf("\n=== Detailed Performance Analysis ===\n");
        printf("%-15s %10s %8s %12s\n", 
               "Task", "Runtime", "CPU%", "Stack Free");
        printf("------------------------------------------------\n");
        
        PerformanceMetrics_t metrics[uxArraySize];
        
        for (x = 0; x < uxArraySize; x++)
        {
            // Calculate CPU percentage
            ulStatsAsPercentage = (pxTaskStatusArray[x].ulRunTimeCounter * 100) 
                                  / ulTotalRunTime;
            
            // Store metrics
            strncpy(metrics[x].taskName, 
                    pxTaskStatusArray[x].pcTaskName, 
                    configMAX_TASK_NAME_LEN);
            metrics[x].runTime = pxTaskStatusArray[x].ulRunTimeCounter;
            metrics[x].cpuPercentage = (float)ulStatsAsPercentage;
            metrics[x].stackHighWaterMark = 
                uxTaskGetStackHighWaterMark(pxTaskStatusArray[x].xHandle);
            
            // Print formatted output
            printf("%-15s %10lu %7.2f%% %9lu words\n",
                   metrics[x].taskName,
                   metrics[x].runTime,
                   metrics[x].cpuPercentage,
                   metrics[x].stackHighWaterMark);
        }
        
        // Identify performance issues
        printf("\n=== Performance Recommendations ===\n");
        
        for (x = 0; x < uxArraySize; x++)
        {
            // Check for CPU hogs
            if (metrics[x].cpuPercentage > 40.0 && 
                strcmp(metrics[x].taskName, "IDLE") != 0)
            {
                printf("WARNING: Task '%s' using %.1f%% CPU - Consider optimization\n",
                       metrics[x].taskName, metrics[x].cpuPercentage);
            }
            
            // Check for stack issues
            if (metrics[x].stackHighWaterMark < 50)
            {
                printf("WARNING: Task '%s' has only %lu words stack remaining\n",
                       metrics[x].taskName, metrics[x].stackHighWaterMark);
            }
        }
        
        // Calculate system load
        float idlePercentage = 0.0;
        for (x = 0; x < uxArraySize; x++)
        {
            if (strcmp(metrics[x].taskName, "IDLE") == 0)
            {
                idlePercentage = metrics[x].cpuPercentage;
                break;
            }
        }
        
        float systemLoad = 100.0 - idlePercentage;
        printf("\nSystem Load: %.1f%% (Idle: %.1f%%)\n", 
               systemLoad, idlePercentage);
        
        if (systemLoad > 90.0)
        {
            printf("CRITICAL: System load above 90%% - Risk of deadline misses!\n");
        }
        else if (systemLoad > 75.0)
        {
            printf("WARNING: System load above 75%% - Limited headroom\n");
        }
        else
        {
            printf("OK: System has adequate processing headroom\n");
        }
        
        vPortFree(pxTaskStatusArray);
    }
}

/* Periodic performance monitoring task */
void vPerformanceMonitorTask(void *pvParameters)
{
    const TickType_t xMonitorPeriod = pdMS_TO_TICKS(10000); // 10 seconds
    
    for (;;)
    {
        vTaskDelay(xMonitorPeriod);
        analyzeSystemPerformance();
    }
}
```

**Example Output:**
```
=== Detailed Performance Analysis ===
Task            Runtime      CPU%   Stack Free
------------------------------------------------
DataProcess     456789      45.67%    128 words
SensorRead      234567      23.46%    215 words
CommTask        123456      12.35%     98 words
IDLE            178901      17.89%    110 words
Monitor           6287       0.63%    445 words

=== Performance Recommendations ===
WARNING: Task 'DataProcess' using 45.7% CPU - Consider optimization
WARNING: Task 'CommTask' has only 98 words stack remaining

System Load: 82.1% (Idle: 17.9%)
WARNING: System load above 75% - Limited headroom
```

### Example 3: Real-Time Performance Optimization

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* Performance tracking structure */
typedef struct
{
    uint32_t lastRunTime;
    uint32_t peakRunTime;
    uint32_t averageRunTime;
    uint32_t samples;
} TaskPerformanceTracker_t;

static TaskPerformanceTracker_t taskTrackers[10];

/* Initialize performance tracking for a task */
void initTaskTracker(uint8_t taskIndex)
{
    taskTrackers[taskIndex].lastRunTime = 0;
    taskTrackers[taskIndex].peakRunTime = 0;
    taskTrackers[taskIndex].averageRunTime = 0;
    taskTrackers[taskIndex].samples = 0;
}

/* Update performance metrics */
void updateTaskMetrics(uint8_t taskIndex, uint32_t currentRunTime)
{
    TaskPerformanceTracker_t *tracker = &taskTrackers[taskIndex];
    
    // Calculate delta (time since last measurement)
    uint32_t delta = currentRunTime - tracker->lastRunTime;
    tracker->lastRunTime = currentRunTime;
    
    // Update peak
    if (delta > tracker->peakRunTime)
    {
        tracker->peakRunTime = delta;
    }
    
    // Update running average
    tracker->samples++;
    tracker->averageRunTime = 
        ((tracker->averageRunTime * (tracker->samples - 1)) + delta) 
        / tracker->samples;
}

/* Optimized task with performance monitoring */
void vOptimizedTask(void *pvParameters)
{
    uint8_t taskIndex = (uint8_t)(uintptr_t)pvParameters;
    TaskStatus_t taskStatus;
    
    initTaskTracker(taskIndex);
    
    for (;;)
    {
        // Get current run-time before work
        vTaskGetInfo(xTaskGetCurrentTaskHandle(), 
                     &taskStatus, 
                     pdTRUE, 
                     eInvalid);
        uint32_t startTime = taskStatus.ulRunTimeCounter;
        
        // Perform task work
        performTaskWork();
        
        // Get run-time after work
        vTaskGetInfo(xTaskGetCurrentTaskHandle(), 
                     &taskStatus, 
                     pdTRUE, 
                     eInvalid);
        uint32_t endTime = taskStatus.ulRunTimeCounter;
        
        // Update metrics
        updateTaskMetrics(taskIndex, endTime);
        
        // Self-optimization: adjust behavior based on metrics
        if (taskTrackers[taskIndex].peakRunTime > CRITICAL_THRESHOLD)
        {
            // Reduce work complexity or increase task period
            adjustTaskBehavior(taskIndex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Analysis and Optimization Strategies

### Identifying CPU Bottlenecks

1. **High CPU usage tasks**: Tasks consistently using more than 30-40% CPU may need optimization
2. **Low idle time**: Less than 10% idle time indicates system overload
3. **Unbalanced load**: One task dominating while others starve suggests priority or design issues

### Optimization Techniques Based on Statistics

**For CPU-bound tasks:**
- Break work into smaller chunks with yield points
- Lower task priority if real-time requirements allow
- Offload work to hardware peripherals (DMA, crypto engines)
- Use more efficient algorithms or data structures

**For I/O-bound tasks:**
- Increase blocking timeout to reduce polling overhead
- Use interrupt-driven I/O instead of polling
- Batch operations to reduce context switch overhead

**For periodic tasks:**
- Adjust period if deadlines are consistently met with time to spare
- Use event-driven approach instead of polling

### System Load Management

```c
/* Monitor and react to system load */
void adaptiveLoadManagement(void)
{
    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize;
    uint32_t ulTotalRunTime;
    float idlePercentage = 0.0;
    
    uxArraySize = uxTaskGetNumberOfTasks();
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    
    if (pxTaskStatusArray != NULL)
    {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, 
                                           uxArraySize, 
                                           &ulTotalRunTime);
        
        // Find idle task percentage
        for (UBaseType_t x = 0; x < uxArraySize; x++)
        {
            if (pxTaskStatusArray[x].eCurrentState == eRunning &&
                pxTaskStatusArray[x].uxCurrentPriority == tskIDLE_PRIORITY)
            {
                idlePercentage = 
                    (float)(pxTaskStatusArray[x].ulRunTimeCounter * 100) 
                    / ulTotalRunTime;
                break;
            }
        }
        
        // Adaptive behavior based on load
        if (idlePercentage < 5.0)
        {
            // Critical load - disable non-essential features
            disableNonCriticalTasks();
            reduceSamplingRates();
        }
        else if (idlePercentage < 15.0)
        {
            // High load - reduce update rates
            reduceUpdateFrequencies();
        }
        else if (idlePercentage > 50.0)
        {
            // Low load - can enable additional features
            enableOptionalFeatures();
        }
        
        vPortFree(pxTaskStatusArray);
    }
}
```

## Best Practices

1. **Timer resolution**: Use a timer at least 10x faster than the tick rate for accurate measurements
2. **Measurement overhead**: Statistics collection itself consumes CPU (typically 1-3%)
3. **Periodic analysis**: Don't poll statistics too frequently; every 5-10 seconds is usually sufficient
4. **Production systems**: Consider disabling in production builds to save resources, or enable only during development
5. **Buffer sizing**: Allocate adequate buffer space for formatted output (40-50 bytes per task)
6. **Overflow protection**: The run-time counter will eventually overflow; design for this or use 64-bit counters if available

Task Run-Time Statistics is an essential tool for developing efficient, reliable real-time systems. By understanding where CPU time is spent, developers can make informed decisions about task priorities, algorithm selection, and system architecture to ensure all real-time constraints are met with appropriate safety margins.