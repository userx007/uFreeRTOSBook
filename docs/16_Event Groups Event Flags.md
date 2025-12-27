# Event Groups (Event Flags) in FreeRTOS

## Overview

Event Groups are a powerful FreeRTOS synchronization primitive that allows tasks to wait for multiple conditions simultaneously. Unlike binary or counting semaphores that signal single events, event groups use individual bits (event flags) within a single variable to represent up to 24 different events (or conditions). This makes them ideal for complex synchronization patterns where tasks need to coordinate based on multiple, independent conditions.

## Core Concepts

**Event Bits**: Each bit in an event group represents a specific event or condition. When an event occurs, its corresponding bit is set to 1. Tasks can wait for any combination of bits to become set before proceeding.

**EventGroupHandle_t**: This is the handle type used to reference an event group after creation.

**Atomic Operations**: FreeRTOS ensures that operations on event groups are atomic, meaning they complete without interruption, preventing race conditions even when multiple tasks access the same event group.

## Key Functions

The primary functions for working with event groups are:

- `xEventGroupCreate()` - Creates a new event group
- `xEventGroupSetBits()` - Sets one or more event bits
- `xEventGroupClearBits()` - Clears one or more event bits
- `xEventGroupWaitBits()` - Waits for specific bits to become set
- `xEventGroupGetBits()` - Retrieves current bit values without blocking
- `xEventGroupSync()` - Synchronizes multiple tasks at a rendezvous point

## Practical Example 1: System Initialization

This example demonstrates waiting for multiple subsystems to complete initialization before starting the main application.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

// Define event bits for different initialization states
#define WIFI_READY_BIT      (1 << 0)  // Bit 0
#define SENSOR_READY_BIT    (1 << 1)  // Bit 1
#define DATABASE_READY_BIT  (1 << 2)  // Bit 2
#define ALL_READY_BITS      (WIFI_READY_BIT | SENSOR_READY_BIT | DATABASE_READY_BIT)

EventGroupHandle_t xSystemEventGroup;

void vWiFiInitTask(void *pvParameters) {
    // Simulate WiFi initialization
    printf("WiFi: Starting initialization...\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // WiFi is ready - set the bit
    xEventGroupSetBits(xSystemEventGroup, WIFI_READY_BIT);
    printf("WiFi: Ready!\n");
    
    vTaskDelete(NULL);
}

void vSensorInitTask(void *pvParameters) {
    // Simulate sensor initialization
    printf("Sensors: Starting initialization...\n");
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    // Sensors are ready
    xEventGroupSetBits(xSystemEventGroup, SENSOR_READY_BIT);
    printf("Sensors: Ready!\n");
    
    vTaskDelete(NULL);
}

void vDatabaseInitTask(void *pvParameters) {
    // Simulate database initialization
    printf("Database: Starting initialization...\n");
    vTaskDelay(pdMS_TO_TICKS(800));
    
    // Database is ready
    xEventGroupSetBits(xSystemEventGroup, DATABASE_READY_BIT);
    printf("Database: Ready!\n");
    
    vTaskDelete(NULL);
}

void vMainApplicationTask(void *pvParameters) {
    printf("Main App: Waiting for all subsystems...\n");
    
    // Wait for ALL three bits to be set
    EventBits_t uxBits = xEventGroupWaitBits(
        xSystemEventGroup,      // Event group handle
        ALL_READY_BITS,         // Bits to wait for
        pdFALSE,                // Don't clear bits on exit
        pdTRUE,                 // Wait for ALL bits (AND condition)
        portMAX_DELAY           // Wait indefinitely
    );
    
    if ((uxBits & ALL_READY_BITS) == ALL_READY_BITS) {
        printf("Main App: All systems ready - starting application!\n");
        
        while (1) {
            // Main application logic here
            printf("Main App: Running...\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

void app_main(void) {
    // Create the event group
    xSystemEventGroup = xEventGroupCreate();
    
    if (xSystemEventGroup != NULL) {
        // Create initialization tasks
        xTaskCreate(vWiFiInitTask, "WiFi", 2048, NULL, 2, NULL);
        xTaskCreate(vSensorInitTask, "Sensor", 2048, NULL, 2, NULL);
        xTaskCreate(vDatabaseInitTask, "Database", 2048, NULL, 2, NULL);
        xTaskCreate(vMainApplicationTask, "MainApp", 2048, NULL, 1, NULL);
    }
}
```

## Practical Example 2: Multi-Condition Data Processing

This example shows a data processing pipeline where a task waits for either of two input sources OR a timeout condition.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#define SENSOR_A_DATA_BIT   (1 << 0)
#define SENSOR_B_DATA_BIT   (1 << 1)
#define ERROR_CONDITION_BIT (1 << 2)
#define TIMEOUT_BIT         (1 << 3)

EventGroupHandle_t xDataEventGroup;

void vSensorATask(void *pvParameters) {
    while (1) {
        // Simulate reading sensor A
        vTaskDelay(pdMS_TO_TICKS(500));
        printf("Sensor A: New data available\n");
        xEventGroupSetBits(xDataEventGroup, SENSOR_A_DATA_BIT);
    }
}

void vSensorBTask(void *pvParameters) {
    while (1) {
        // Simulate reading sensor B (slower)
        vTaskDelay(pdMS_TO_TICKS(1200));
        printf("Sensor B: New data available\n");
        xEventGroupSetBits(xDataEventGroup, SENSOR_B_DATA_BIT);
    }
}

void vTimeoutTask(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        printf("Timeout: Setting timeout flag\n");
        xEventGroupSetBits(xDataEventGroup, TIMEOUT_BIT);
    }
}

void vDataProcessorTask(void *pvParameters) {
    const EventBits_t xBitsToWaitFor = SENSOR_A_DATA_BIT | 
                                        SENSOR_B_DATA_BIT | 
                                        TIMEOUT_BIT;
    
    while (1) {
        printf("Processor: Waiting for data or timeout...\n");
        
        // Wait for ANY of the specified bits (OR condition)
        EventBits_t uxBits = xEventGroupWaitBits(
            xDataEventGroup,
            xBitsToWaitFor,
            pdTRUE,              // Clear bits on exit
            pdFALSE,             // Wait for ANY bit (OR condition)
            portMAX_DELAY
        );
        
        // Check which event occurred
        if (uxBits & SENSOR_A_DATA_BIT) {
            printf("Processor: Processing Sensor A data\n");
        }
        
        if (uxBits & SENSOR_B_DATA_BIT) {
            printf("Processor: Processing Sensor B data\n");
        }
        
        if (uxBits & TIMEOUT_BIT) {
            printf("Processor: Timeout occurred - using default values\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Practical Example 3: Task Synchronization with xEventGroupSync()

This demonstrates using `xEventGroupSync()` to synchronize multiple tasks at a rendezvous point.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#define TASK_1_SYNC_BIT  (1 << 0)
#define TASK_2_SYNC_BIT  (1 << 1)
#define TASK_3_SYNC_BIT  (1 << 2)
#define ALL_SYNC_BITS    (TASK_1_SYNC_BIT | TASK_2_SYNC_BIT | TASK_3_SYNC_BIT)

EventGroupHandle_t xSyncEventGroup;

void vWorkerTask1(void *pvParameters) {
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("Task 1: Starting work cycle %d\n", cycle);
        
        // Do some work (variable duration)
        vTaskDelay(pdMS_TO_TICKS(100 + (cycle * 50)));
        
        printf("Task 1: Finished work, waiting at sync point\n");
        
        // Signal completion and wait for others
        xEventGroupSync(
            xSyncEventGroup,
            TASK_1_SYNC_BIT,     // Bit to set (my completion)
            ALL_SYNC_BITS,       // Bits to wait for (everyone's completion)
            portMAX_DELAY
        );
        
        printf("Task 1: All tasks synchronized, continuing...\n");
    }
    
    vTaskDelete(NULL);
}

void vWorkerTask2(void *pvParameters) {
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("Task 2: Starting work cycle %d\n", cycle);
        
        vTaskDelay(pdMS_TO_TICKS(200 + (cycle * 30)));
        
        printf("Task 2: Finished work, waiting at sync point\n");
        
        xEventGroupSync(
            xSyncEventGroup,
            TASK_2_SYNC_BIT,
            ALL_SYNC_BITS,
            portMAX_DELAY
        );
        
        printf("Task 2: All tasks synchronized, continuing...\n");
    }
    
    vTaskDelete(NULL);
}

void vWorkerTask3(void *pvParameters) {
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("Task 3: Starting work cycle %d\n", cycle);
        
        vTaskDelay(pdMS_TO_TICKS(150 + (cycle * 40)));
        
        printf("Task 3: Finished work, waiting at sync point\n");
        
        xEventGroupSync(
            xSyncEventGroup,
            TASK_3_SYNC_BIT,
            ALL_SYNC_BITS,
            portMAX_DELAY
        );
        
        printf("Task 3: All tasks synchronized, continuing...\n");
    }
    
    vTaskDelete(NULL);
}
```

## Understanding Atomic Operations

Event groups provide atomic operations, which is critical for preventing race conditions. Here's what happens internally:

**Atomicity Guarantees**: When you call `xEventGroupSetBits()`, the entire operation of reading the current bits, modifying them, and writing them back happens atomically. No other task can access the event group in the middle of this operation.

**Example of Why Atomicity Matters**:

```c
// Without atomicity (dangerous - this is NOT how FreeRTOS works):
// Task 1: Read bits = 0b0001
// Task 2: Read bits = 0b0001 (before Task 1 writes)
// Task 1: Set bit 1, write 0b0011
// Task 2: Set bit 2, write 0b0101 (overwrites Task 1's change!)
// Result: Bit 1 is lost!

// With atomicity (how FreeRTOS actually works):
// Task 1: Atomically sets bit 1: 0b0001 -> 0b0011
// Task 2: Atomically sets bit 2: 0b0011 -> 0b0111
// Result: Both bits are correctly set
```

## Advanced Pattern: State Machine Coordination

```c
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

typedef enum {
    STATE_IDLE = 0,
    STATE_ARMED,
    STATE_TRIGGERED,
    STATE_PROCESSING,
    STATE_COMPLETE
} SystemState_t;

#define ARMED_BIT       (1 << 0)
#define TRIGGER_BIT     (1 << 1)
#define PROCESS_BIT     (1 << 2)
#define COMPLETE_BIT    (1 << 3)
#define ERROR_BIT       (1 << 4)

EventGroupHandle_t xStateEventGroup;
SystemState_t currentState = STATE_IDLE;

void vStateMachineTask(void *pvParameters) {
    while (1) {
        switch (currentState) {
            case STATE_IDLE:
                printf("State: IDLE - Waiting to be armed\n");
                xEventGroupWaitBits(xStateEventGroup, ARMED_BIT, 
                                   pdTRUE, pdTRUE, portMAX_DELAY);
                currentState = STATE_ARMED;
                break;
                
            case STATE_ARMED:
                printf("State: ARMED - Waiting for trigger\n");
                EventBits_t bits = xEventGroupWaitBits(
                    xStateEventGroup, 
                    TRIGGER_BIT | ERROR_BIT,
                    pdTRUE, 
                    pdFALSE,  // Wait for ANY
                    pdMS_TO_TICKS(5000)
                );
                
                if (bits & TRIGGER_BIT) {
                    currentState = STATE_TRIGGERED;
                    xEventGroupSetBits(xStateEventGroup, PROCESS_BIT);
                } else if (bits & ERROR_BIT) {
                    printf("State: Error detected, returning to IDLE\n");
                    currentState = STATE_IDLE;
                } else {
                    printf("State: Timeout, disarming\n");
                    currentState = STATE_IDLE;
                }
                break;
                
            case STATE_TRIGGERED:
                printf("State: TRIGGERED - Processing\n");
                currentState = STATE_PROCESSING;
                // Fall through
                
            case STATE_PROCESSING:
                // Simulate processing
                vTaskDelay(pdMS_TO_TICKS(1000));
                xEventGroupSetBits(xStateEventGroup, COMPLETE_BIT);
                currentState = STATE_COMPLETE;
                break;
                
            case STATE_COMPLETE:
                printf("State: COMPLETE - Resetting to IDLE\n");
                xEventGroupClearBits(xStateEventGroup, 
                                    PROCESS_BIT | COMPLETE_BIT);
                currentState = STATE_IDLE;
                break;
        }
    }
}
```

## Important Considerations

**Bit Limitations**: Event groups typically provide 24 usable event bits (bits 0-23), with the upper 8 bits reserved for control purposes. Always check your FreeRTOS configuration.

**Clearing Bits**: The `xClearOnExit` parameter in `xEventGroupWaitBits()` determines whether bits are automatically cleared when the function returns. This is useful for single-use events but not for persistent states.

**Priority Inversion**: Like other synchronization primitives, event groups don't implement priority inheritance, so be mindful of priority inversion scenarios.

**ISR Variants**: Use `xEventGroupSetBitsFromISR()` when setting bits from interrupt service routines, as it uses a different mechanism that's safe for ISR context.

## When to Use Event Groups

Event groups are ideal when you need to coordinate tasks based on multiple independent conditions, synchronize tasks at specific points, implement complex state machines with multiple status flags, or wait for any combination of multiple events with a single blocking call. They provide an elegant solution for scenarios that would otherwise require multiple semaphores or complex signaling mechanisms.