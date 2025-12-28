# FreeRTOS Debugging Techniques

Debugging real-time operating systems presents unique challenges compared to bare-metal or traditional application debugging. FreeRTOS provides several mechanisms and integrates with modern debugging tools to help developers identify issues in multitasking environments.

## Overview of FreeRTOS-Aware Debugging

Traditional debuggers show you the state of a single execution thread, but FreeRTOS applications involve multiple tasks executing concurrently. FreeRTOS-aware debugging tools extend standard JTAG/SWD debuggers to provide visibility into:

- All task states (running, ready, blocked, suspended)
- Task stack usage and potential overflows
- Queue contents and wait lists
- Semaphore and mutex states
- Timing information and execution traces
- Memory heap usage

## JTAG/SWD Debuggers with FreeRTOS Plugins

### Common Tool Chains

**SEGGER Ozone with SystemView**: Provides real-time visualization of task execution, with timeline views showing context switches, interrupts, and API calls.

**Eclipse with GDB and FreeRTOS awareness**: The GDB debugger can be extended with Python scripts to parse FreeRTOS data structures.

**IAR Embedded Workbench**: Built-in RTOS awareness with graphical views of tasks, queues, and timers.

**Keil MDK with RTOS-aware debugging**: Displays task information, stack usage, and kernel objects.

### Example: Configuring GDB with FreeRTOS Awareness

Here's a Python script for GDB that adds FreeRTOS awareness:

```python
# freertos_gdb.py - FreeRTOS awareness for GDB

import gdb

class ShowTasks(gdb.Command):
    """Display all FreeRTOS tasks and their states"""
    
    def __init__(self):
        super(ShowTasks, self).__init__("show tasks", gdb.COMMAND_USER)
    
    def invoke(self, arg, from_tty):
        # Parse the task list
        list_ptr = gdb.parse_and_eval("pxReadyTasksLists")
        current_task = gdb.parse_and_eval("pxCurrentTCB")
        
        print("Task Name          State      Priority  Stack High Water")
        print("-" * 60)
        
        # Iterate through ready lists
        for priority in range(int(gdb.parse_and_eval("configMAX_PRIORITIES"))):
            list_item = list_ptr[priority]['xListEnd']['pxNext']
            
            while list_item != list_ptr[priority]['xListEnd'].address:
                tcb = list_item['pvOwner']
                name = tcb['pcTaskName'].string()
                stack_hwm = tcb['uxTaskStackHighWaterMark']
                
                state = "Ready"
                if tcb == current_task:
                    state = "Running"
                
                print(f"{name:18s} {state:10s} {priority:8d}  {stack_hwm:8d}")
                list_item = list_item['pxNext']

ShowTasks()
```

Usage in GDB:
```
(gdb) source freertos_gdb.py
(gdb) show tasks
Task Name          State      Priority  Stack High Water
------------------------------------------------------------
IDLE               Ready             0       128
SensorTask         Running           2       256
DisplayTask        Blocked           3       512
```

## Runtime Task State Inspection

### Using FreeRTOS Task Statistics API

FreeRTOS provides built-in APIs for runtime inspection when `configUSE_TRACE_FACILITY` is enabled:

```c
// Example: Monitoring all tasks at runtime
#include "FreeRTOS.h"
#include "task.h"

void vTaskMonitor(void *pvParameters)
{
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    uint32_t ulTotalRunTime;
    
    while(1)
    {
        // Get number of tasks
        uxArraySize = uxTaskGetNumberOfTasks();
        
        // Allocate array to hold task info
        pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
        
        if(pxTaskStatusArray != NULL)
        {
            // Populate array with task information
            uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, 
                                               uxArraySize, 
                                               &ulTotalRunTime);
            
            printf("\n=== Task Status Report ===\n");
            printf("Task Name        State  Prio  Stack  CPU%%\n");
            printf("----------------------------------------\n");
            
            for(x = 0; x < uxArraySize; x++)
            {
                // Calculate CPU usage percentage
                uint32_t cpu_percent = 0;
                if(ulTotalRunTime > 0)
                {
                    cpu_percent = (pxTaskStatusArray[x].ulRunTimeCounter * 100) 
                                  / ulTotalRunTime;
                }
                
                const char *state_str;
                switch(pxTaskStatusArray[x].eCurrentState)
                {
                    case eReady:     state_str = "Ready"; break;
                    case eBlocked:   state_str = "Block"; break;
                    case eSuspended: state_str = "Susp "; break;
                    case eDeleted:   state_str = "Del  "; break;
                    default:         state_str = "Run  "; break;
                }
                
                printf("%-15s  %s  %2d    %4d   %3d%%\n",
                       pxTaskStatusArray[x].pcTaskName,
                       state_str,
                       pxTaskStatusArray[x].uxCurrentPriority,
                       pxTaskStatusArray[x].usStackHighWaterMark,
                       cpu_percent);
            }
            
            vPortFree(pxTaskStatusArray);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Report every 5 seconds
    }
}
```

Required configuration in `FreeRTOSConfig.h`:
```c
#define configUSE_TRACE_FACILITY              1
#define configGENERATE_RUN_TIME_STATS         1
#define configUSE_STATS_FORMATTING_FUNCTIONS  1

// Define runtime counter (use a hardware timer)
extern uint32_t ulGetRunTimeCounterValue(void);
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() 
#define portGET_RUN_TIME_COUNTER_VALUE() ulGetRunTimeCounterValue()
```

## Inspecting Queues at Runtime

### Examining Queue Contents and Wait Lists

```c
// Example: Queue debugging utility
#include "queue.h"

typedef struct
{
    uint32_t sensor_id;
    float temperature;
    uint32_t timestamp;
} SensorData_t;

QueueHandle_t xSensorQueue;

void vQueueDebugInfo(QueueHandle_t xQueue, const char *queue_name)
{
    UBaseType_t uxMessagesWaiting = uxQueueMessagesWaiting(xQueue);
    UBaseType_t uxSpacesAvailable = uxQueueSpacesAvailable(xQueue);
    
    printf("\n=== Queue: %s ===\n", queue_name);
    printf("Messages waiting: %d\n", uxMessagesWaiting);
    printf("Spaces available: %d\n", uxSpacesAvailable);
    printf("Queue full: %s\n", uxSpacesAvailable == 0 ? "YES" : "NO");
    printf("Queue empty: %s\n", uxMessagesWaiting == 0 ? "YES" : "NO");
    
    // Check if tasks are blocked on this queue
    // (requires registry and trace facilities)
    #if (configQUEUE_REGISTRY_SIZE > 0)
    const char *reg_name = pcQueueGetName(xQueue);
    if(reg_name != NULL)
    {
        printf("Registered name: %s\n", reg_name);
    }
    #endif
}

// Diagnostic task
void vDiagnosticTask(void *pvParameters)
{
    while(1)
    {
        vQueueDebugInfo(xSensorQueue, "Sensor Data Queue");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

// In application initialization
void vApplicationSetup(void)
{
    xSensorQueue = xQueueCreate(10, sizeof(SensorData_t));
    
    #if (configQUEUE_REGISTRY_SIZE > 0)
    // Register queue for debugging
    vQueueAddToRegistry(xSensorQueue, "SensorQueue");
    #endif
    
    xTaskCreate(vDiagnosticTask, "Diagnostic", 200, NULL, 1, NULL);
}
```

## Inspecting Semaphores and Mutexes

### Detecting Deadlocks and Priority Inversion

```c
// Example: Mutex state monitoring
#include "semphr.h"

SemaphoreHandle_t xMutex1;
SemaphoreHandle_t xMutex2;

// Wrapper function for mutex acquisition with timeout detection
BaseType_t xTakeMutexWithDebug(SemaphoreHandle_t xMutex, 
                                const char *mutex_name,
                                const char *task_name,
                                TickType_t xBlockTime)
{
    TickType_t xStartTime = xTaskGetTickCount();
    BaseType_t xResult;
    
    printf("[%s] Attempting to acquire %s\n", task_name, mutex_name);
    
    xResult = xSemaphoreTake(xMutex, xBlockTime);
    
    if(xResult == pdTRUE)
    {
        TickType_t xWaitTime = xTaskGetTickCount() - xStartTime;
        printf("[%s] Acquired %s after %d ticks\n", 
               task_name, mutex_name, xWaitTime);
        
        if(xWaitTime > pdMS_TO_TICKS(100))
        {
            printf("WARNING: Long wait time detected!\n");
        }
    }
    else
    {
        printf("ERROR: [%s] Failed to acquire %s (timeout)\n", 
               task_name, mutex_name);
    }
    
    return xResult;
}

// Example task that could create deadlock
void vTaskA(void *pvParameters)
{
    while(1)
    {
        if(xTakeMutexWithDebug(xMutex1, "Mutex1", "TaskA", 
                               pdMS_TO_TICKS(1000)) == pdTRUE)
        {
            vTaskDelay(pdMS_TO_TICKS(10)); // Simulate work
            
            if(xTakeMutexWithDebug(xMutex2, "Mutex2", "TaskA", 
                                   pdMS_TO_TICKS(1000)) == pdTRUE)
            {
                // Critical section with both mutexes
                xSemaphoreGive(xMutex2);
            }
            
            xSemaphoreGive(xMutex1);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Stack Overflow Detection

### Runtime Stack Monitoring

```c
// Configure in FreeRTOSConfig.h
#define configCHECK_FOR_STACK_OVERFLOW 2

// Stack overflow hook (called by FreeRTOS kernel)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // Disable interrupts
    taskDISABLE_INTERRUPTS();
    
    // Log the error (if logging is still functional)
    printf("STACK OVERFLOW in task: %s\n", pcTaskName);
    
    // Blink LED or trigger external watchdog
    while(1)
    {
        // Halt execution - inspect with debugger
    }
}

// Proactive stack checking task
void vStackCheckTask(void *pvParameters)
{
    TaskHandle_t xHandle;
    UBaseType_t uxHighWaterMark;
    
    while(1)
    {
        // Check idle task stack
        xHandle = xTaskGetIdleTaskHandle();
        uxHighWaterMark = uxTaskGetStackHighWaterMark(xHandle);
        
        if(uxHighWaterMark < 50) // Less than 50 words remaining
        {
            printf("WARNING: Idle task stack low: %d words\n", 
                   uxHighWaterMark);
        }
        
        // Check current task
        uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
        printf("Current task stack margin: %d words\n", uxHighWaterMark);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Using SEGGER SystemView for Advanced Tracing

### Integration Example

```c
// Include SystemView headers
#include "SEGGER_SYSVIEW.h"

// Configure in FreeRTOSConfig.h
#define INCLUDE_xTaskGetIdleTaskHandle    1
#define configUSE_TRACE_FACILITY          1

// In main.c
int main(void)
{
    // Initialize hardware
    SystemClock_Config();
    
    // Initialize SystemView
    SEGGER_SYSVIEW_Conf();
    
    // Create tasks
    xTaskCreate(vSensorTask, "Sensor", 256, NULL, 2, NULL);
    xTaskCreate(vDisplayTask, "Display", 512, NULL, 3, NULL);
    
    // Start scheduler (SystemView will automatically record events)
    vTaskStartScheduler();
    
    while(1);
}

// Custom instrumentation points
void vSensorTask(void *pvParameters)
{
    while(1)
    {
        SEGGER_SYSVIEW_PrintfHost("Reading sensor...");
        
        float temperature = fReadTemperature();
        
        SEGGER_SYSVIEW_PrintfHost("Temperature: %.2f C", temperature);
        
        xQueueSend(xTempQueue, &temperature, portMAX_DELAY);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Practical Debugging Scenario

Here's a complete example showing debugging a priority inversion issue:

```c
// Scenario: Low priority task holds mutex, high priority task blocked

SemaphoreHandle_t xResourceMutex;

// Low priority task (Priority 1)
void vLowPriorityTask(void *pvParameters)
{
    while(1)
    {
        printf("[LOW] Acquiring mutex\n");
        xSemaphoreTake(xResourceMutex, portMAX_DELAY);
        
        printf("[LOW] In critical section (long operation)\n");
        vTaskDelay(pdMS_TO_TICKS(500)); // Simulate long operation
        
        xSemaphoreGive(xResourceMutex);
        printf("[LOW] Released mutex\n");
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Medium priority task (Priority 2) - CPU intensive
void vMediumPriorityTask(void *pvParameters)
{
    while(1)
    {
        printf("[MED] Computing...\n");
        for(volatile int i = 0; i < 1000000; i++); // Busy loop
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// High priority task (Priority 3)
void vHighPriorityTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(100)); // Start slightly later
    
    while(1)
    {
        printf("[HIGH] Need mutex urgently!\n");
        
        TickType_t xStartTime = xTaskGetTickCount();
        xSemaphoreTake(xResourceMutex, portMAX_DELAY);
        TickType_t xWaitTime = xTaskGetTickCount() - xStartTime;
        
        printf("[HIGH] Got mutex after %d ticks (PROBLEM if > 500!)\n", 
               xWaitTime);
        
        // Use resource
        xSemaphoreGive(xResourceMutex);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Solution: Use mutex with priority inheritance
void setup_with_priority_inheritance(void)
{
    // Use mutex instead of binary semaphore
    xResourceMutex = xSemaphoreCreateMutex();
    
    // FreeRTOS will automatically handle priority inheritance
    // When high priority task blocks on mutex, low priority task
    // temporarily inherits high priority
}
```

## Key Configuration Options for Debugging

```c
// FreeRTOSConfig.h - Essential debugging options

// Enable trace facility
#define configUSE_TRACE_FACILITY              1

// Enable runtime statistics
#define configGENERATE_RUN_TIME_STATS         1

// Enable stack overflow detection (method 2 most thorough)
#define configCHECK_FOR_STACK_OVERFLOW        2

// Enable malloc failed hook
#define configUSE_MALLOC_FAILED_HOOK          1

// Enable queue registry for debugging tools
#define configQUEUE_REGISTRY_SIZE             10

// Include useful task functions
#define INCLUDE_uxTaskGetStackHighWaterMark   1
#define INCLUDE_pxTaskGetStackStart           1
#define INCLUDE_eTaskGetState                 1
#define INCLUDE_xTaskGetIdleTaskHandle        1
#define INCLUDE_xTaskGetHandle                1

// Use mutexes with priority inheritance to avoid priority inversion
#define configUSE_MUTEXES                     1
#define configUSE_RECURSIVE_MUTEXES           1

// Assert function for catching errors
#define configASSERT(x) if((x) == 0) vAssertCalled(__FILE__, __LINE__)
```

These debugging techniques and tools enable you to effectively troubleshoot complex multitasking issues, identify performance bottlenecks, detect resource conflicts, and ensure reliable operation of your FreeRTOS applications.