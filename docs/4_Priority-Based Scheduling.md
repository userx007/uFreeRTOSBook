# Priority-Based Scheduling in FreeRTOS

## Overview

FreeRTOS implements a **fixed-priority preemptive scheduling** algorithm, which is the foundation of its real-time behavior. This means that at any given time, the scheduler ensures the highest-priority ready task is executing. Tasks are assigned static priorities that don't change during runtime (unless explicitly modified), and a higher-priority task can preempt a lower-priority task immediately when it becomes ready.

## Fixed-Priority Preemptive Scheduling

### Core Principles

**Priority Levels**: FreeRTOS supports configurable priority levels from 0 to (configMAX_PRIORITIES - 1), where:
- **Priority 0** is the lowest priority (idle task)
- **(configMAX_PRIORITIES - 1)** is the highest priority
- Typically, configMAX_PRIORITIES is set between 5 and 32 depending on application needs

**Preemption**: When a higher-priority task becomes ready (unblocked from a delay, waiting for a queue, etc.), it immediately preempts any lower-priority running task. The context switch happens without waiting for the lower-priority task to yield voluntarily.

**Determinism**: The scheduler guarantees that the highest-priority ready task always runs, providing predictable, deterministic behavior crucial for real-time systems.

### How the Scheduler Works

The scheduler maintains separate ready lists for each priority level. When selecting the next task to run:

1. It identifies the highest priority level with ready tasks
2. Selects a task from that priority's ready list
3. Performs a context switch if necessary

The time to make a scheduling decision is constant and independent of the number of tasks, making it highly efficient.

## Priority Assignment Strategies

### Rate Monotonic Scheduling (RMS)

A common approach where tasks with shorter periods receive higher priorities. This is mathematically proven to be optimal for periodic tasks under certain conditions.

```c
// Example: Three periodic tasks with RMS priority assignment

// Task 1: Fastest rate (10ms period) - Highest priority
void vHighSpeedSensorTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10);
    
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Read critical sensor data
        uint16_t sensorValue = readCriticalSensor();
        processUrgentData(sensorValue);
    }
}

// Task 2: Medium rate (50ms period) - Medium priority
void vControlLoopTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50);
    
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Execute control algorithm
        calculatePIDControl();
        updateActuators();
    }
}

// Task 3: Slowest rate (100ms period) - Lower priority
void vLoggingTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100);
    
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Log system status
        logSystemStatus();
    }
}

// In main():
xTaskCreate(vHighSpeedSensorTask, "FastSensor", 128, NULL, 5, NULL);  // Highest
xTaskCreate(vControlLoopTask, "Control", 128, NULL, 3, NULL);         // Medium
xTaskCreate(vLoggingTask, "Logging", 128, NULL, 2, NULL);            // Lower
```

### Deadline Monotonic Scheduling

Tasks with shorter deadlines get higher priorities, useful when task periods and deadlines differ.

### Application-Specific Priority Assignment

Priorities based on criticality, responsiveness requirements, or system functionality:

```c
// Example: Mixed criticality system
#define PRIORITY_CRITICAL_SAFETY    6  // Emergency stop, critical sensors
#define PRIORITY_HIGH_REALTIME      5  // Motor control, fast feedback loops
#define PRIORITY_NORMAL_REALTIME    4  // User interface updates
#define PRIORITY_BACKGROUND         3  // Data processing
#define PRIORITY_LOW                2  // Logging, diagnostics
#define PRIORITY_IDLE_HOOK          1  // Optional background tasks

// Safety-critical task
xTaskCreate(vEmergencyStopTask, "EmergencyStop", 128, NULL, 
            PRIORITY_CRITICAL_SAFETY, NULL);

// Real-time motor control
xTaskCreate(vMotorControlTask, "MotorCtrl", 256, NULL, 
            PRIORITY_HIGH_REALTIME, NULL);

// User interface
xTaskCreate(vDisplayUpdateTask, "Display", 256, NULL, 
            PRIORITY_NORMAL_REALTIME, NULL);
```

## Same-Priority Task Behavior (Time Slicing)

When multiple tasks share the same priority, FreeRTOS uses **round-robin time slicing** if configUSE_TIME_SLICING is enabled (default is enabled).

### Time Slice Behavior

Each task at the same priority level gets to run for one **tick period** before the scheduler switches to the next ready task at that priority. The tick period is defined by configTICK_RATE_HZ (typically 1000Hz = 1ms tick).

```c
// Example: Three tasks at the same priority demonstrating time slicing

void vWorkerTask1(void *pvParameters) {
    for(;;) {
        // This task runs for approximately 1 tick before switching
        printf("Task 1 executing\n");
        performWork1();
        
        // No explicit delay - relies on time slicing
    }
}

void vWorkerTask2(void *pvParameters) {
    for(;;) {
        printf("Task 2 executing\n");
        performWork2();
    }
}

void vWorkerTask3(void *pvParameters) {
    for(;;) {
        printf("Task 3 executing\n");
        performWork3();
    }
}

// All created with same priority
xTaskCreate(vWorkerTask1, "Worker1", 128, NULL, 3, NULL);
xTaskCreate(vWorkerTask2, "Worker2", 128, NULL, 3, NULL);
xTaskCreate(vWorkerTask3, "Worker3", 128, NULL, 3, NULL);

// Execution pattern (approximate):
// Tick 0: Task1 runs
// Tick 1: Task2 runs  
// Tick 2: Task3 runs
// Tick 3: Task1 runs again
// ... and so on
```

### Cooperative Scheduling at Same Priority

Tasks can voluntarily yield the processor using `taskYIELD()`, allowing finer control:

```c
void vCooperativeTask1(void *pvParameters) {
    for(;;) {
        performShortOperation();
        
        // Voluntarily yield to other same-priority tasks
        taskYIELD();
    }
}

void vCooperativeTask2(void *pvParameters) {
    for(;;) {
        performAnotherOperation();
        taskYIELD();
    }
}
```

### Disabling Time Slicing

If configUSE_TIME_SLICING is set to 0, same-priority tasks only switch when:
- The running task blocks (vTaskDelay, queue wait, etc.)
- The running task explicitly yields with taskYIELD()
- A higher-priority task preempts

```c
// In FreeRTOSConfig.h
#define configUSE_TIME_SLICING  0

// With time slicing disabled, these tasks won't automatically round-robin
void vTask1(void *pvParameters) {
    for(;;) {
        doWork();
        // Must explicitly yield or block to allow other same-priority tasks
        vTaskDelay(pdMS_TO_TICKS(10));  // Block to allow task switching
    }
}
```

## Practical Example: Complete System

Here's a comprehensive example showing priority-based scheduling in action:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Priority definitions
#define PRIORITY_ISR_HANDLER    5
#define PRIORITY_PROCESSOR      4
#define PRIORITY_DISPLAY        3
#define PRIORITY_LOGGER         2

// Shared queue
QueueHandle_t xDataQueue;

// High-priority: Simulates ISR handler task
void vUrgentDataHandler(void *pvParameters) {
    uint32_t urgentData;
    
    for(;;) {
        // Simulate receiving urgent data (e.g., from interrupt)
        urgentData = getUrgentData();
        
        // Send to queue for processing
        xQueueSend(xDataQueue, &urgentData, 0);
        
        // This high-priority task preempts all lower-priority tasks
        vTaskDelay(pdMS_TO_TICKS(20));  // Runs every 20ms
    }
}

// Medium-high priority: Process data quickly
void vDataProcessor(void *pvParameters) {
    uint32_t receivedData;
    
    for(;;) {
        // Wait for data from high-priority task
        if(xQueueReceive(xDataQueue, &receivedData, portMAX_DELAY)) {
            // Process the data
            processData(receivedData);
            
            // When done, this task blocks and lower-priority tasks can run
        }
    }
}

// Medium priority: Update display
void vDisplayTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
        
        // Update display - can be preempted by higher priority tasks
        updateDisplay();
    }
}

// Low priority: Background logging
void vLoggerTask(void *pvParameters) {
    for(;;) {
        // Runs when no higher-priority tasks are ready
        logToSD();
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Two tasks at same priority - demonstrate time slicing
void vBackgroundTask1(void *pvParameters) {
    for(;;) {
        performBackgroundWork1();
        // Time slicing causes automatic switching to vBackgroundTask2
    }
}

void vBackgroundTask2(void *pvParameters) {
    for(;;) {
        performBackgroundWork2();
        // Time slicing causes automatic switching back to vBackgroundTask1
    }
}

int main(void) {
    // Create queue
    xDataQueue = xQueueCreate(10, sizeof(uint32_t));
    
    // Create tasks with different priorities
    xTaskCreate(vUrgentDataHandler, "UrgentData", 128, NULL, 
                PRIORITY_ISR_HANDLER, NULL);
    
    xTaskCreate(vDataProcessor, "Processor", 256, NULL, 
                PRIORITY_PROCESSOR, NULL);
    
    xTaskCreate(vDisplayTask, "Display", 256, NULL, 
                PRIORITY_DISPLAY, NULL);
    
    xTaskCreate(vLoggerTask, "Logger", 256, NULL, 
                PRIORITY_LOGGER, NULL);
    
    // Two same-priority background tasks
    xTaskCreate(vBackgroundTask1, "Background1", 128, NULL, 1, NULL);
    xTaskCreate(vBackgroundTask2, "Background2", 128, NULL, 1, NULL);
    
    // Start scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    for(;;);
}
```

## Key Considerations and Best Practices

**Priority Inversion**: Be aware that a low-priority task holding a resource can block a high-priority task. Use priority inheritance mutexes to mitigate this.

**Starvation**: Very high-priority tasks that never block can starve lower-priority tasks. Ensure all tasks periodically block or delay.

**Priority Levels**: Use only as many priority levels as needed. Too many can make the system harder to analyze and debug.

**Testing**: Thoroughly test priority assignments under worst-case scenarios to ensure deadlines are met.

**Dynamic Priority Changes**: While FreeRTOS allows changing task priorities at runtime with `vTaskPrioritySet()`, this should be used sparingly as it can make system behavior less predictable.

This priority-based scheduling mechanism is what gives FreeRTOS its real-time capabilities, ensuring that critical tasks always get CPU time when they need it, while less critical tasks run when resources are available.