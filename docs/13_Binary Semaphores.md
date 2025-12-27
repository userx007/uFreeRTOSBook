# Binary Semaphores in FreeRTOS

## Overview

Binary semaphores are synchronization primitives in FreeRTOS that can have only two states: available (1) or unavailable (0). Unlike mutexes, which are designed for mutual exclusion and resource protection, binary semaphores are primarily used for **task synchronization** and **event signaling**, especially between tasks and interrupt service routines (ISRs).

## Core Concepts

### What is a Binary Semaphore?

A binary semaphore acts like a flag that can be "given" (set) and "taken" (cleared). When a task attempts to take a semaphore that's unavailable, it blocks until another task or ISR gives the semaphore. This makes them ideal for signaling that an event has occurred.

**Key characteristics:**
- Can only hold values 0 or 1
- No ownership concept (unlike mutexes)
- Can be given from ISRs safely
- Primarily used for synchronization, not mutual exclusion
- Does not implement priority inheritance

### Creating Binary Semaphores

```c
#include "FreeRTOS.h"
#include "semphr.h"

SemaphoreHandle_t xBinarySemaphore;

void setup() {
    // Create a binary semaphore
    xBinarySemaphore = xSemaphoreCreateBinary();
    
    if (xBinarySemaphore != NULL) {
        // Semaphore created successfully
        // Initially, binary semaphores are created "empty" (unavailable)
    }
}
```

## Common Use Cases

### 1. Task Synchronization

Binary semaphores synchronize the execution of multiple tasks, ensuring one task waits for another to complete an action.

```c
SemaphoreHandle_t xSyncSemaphore;

void vProducerTask(void *pvParameters) {
    for (;;) {
        // Perform some work (e.g., prepare data)
        printf("Producer: Data prepared\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Signal the consumer that data is ready
        xSemaphoreGive(xSyncSemaphore);
        printf("Producer: Semaphore given\n");
    }
}

void vConsumerTask(void *pvParameters) {
    for (;;) {
        // Wait for producer to signal
        if (xSemaphoreTake(xSyncSemaphore, portMAX_DELAY) == pdTRUE) {
            printf("Consumer: Semaphore received, processing data\n");
            // Process the data
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

void app_main() {
    xSyncSemaphore = xSemaphoreCreateBinary();
    
    xTaskCreate(vProducerTask, "Producer", 2048, NULL, 2, NULL);
    xTaskCreate(vConsumerTask, "Consumer", 2048, NULL, 2, NULL);
    
    vTaskStartScheduler();
}
```

### 2. Interrupt to Task Synchronization

This is one of the most common uses - an ISR signals a task that an event has occurred, and the task performs the processing.

```c
SemaphoreHandle_t xButtonSemaphore;

// Interrupt Service Routine
void IRAM_ATTR button_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Give the semaphore from ISR
    xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
    
    // Request context switch if higher priority task was woken
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void vButtonHandlerTask(void *pvParameters) {
    for (;;) {
        // Wait for button press (ISR to signal)
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE) {
            printf("Button pressed! Handling event...\n");
            // Perform time-consuming processing here
            // This keeps the ISR short and fast
            vTaskDelay(pdMS_TO_TICKS(100)); // Debounce
        }
    }
}

void setup_button_interrupt() {
    xButtonSemaphore = xSemaphoreCreateBinary();
    
    // Configure GPIO interrupt
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
    
    xTaskCreate(vButtonHandlerTask, "ButtonHandler", 2048, NULL, 5, NULL);
}
```

### 3. Periodic Task Synchronization with Timer

```c
SemaphoreHandle_t xTimerSemaphore;
TimerHandle_t xTimer;

void vTimerCallback(TimerHandle_t xTimer) {
    // Give semaphore every time timer expires
    xSemaphoreGive(xTimerSemaphore);
}

void vPeriodicTask(void *pvParameters) {
    for (;;) {
        // Wait for timer to signal
        if (xSemaphoreTake(xTimerSemaphore, portMAX_DELAY) == pdTRUE) {
            printf("Periodic task executing at %lu ms\n", 
                   xTaskGetTickCount() * portTICK_PERIOD_MS);
            // Perform periodic work
        }
    }
}

void app_main() {
    xTimerSemaphore = xSemaphoreCreateBinary();
    
    // Create a timer that fires every 2 seconds
    xTimer = xTimerCreate("Timer", pdMS_TO_TICKS(2000), pdTRUE, 
                          NULL, vTimerCallback);
    
    xTaskCreate(vPeriodicTask, "Periodic", 2048, NULL, 3, NULL);
    
    xTimerStart(xTimer, 0);
    vTaskStartScheduler();
}
```

## Binary Semaphores vs Mutexes

Understanding the differences is crucial for proper design:

| Feature | Binary Semaphore | Mutex |
|---------|-----------------|-------|
| **Primary Purpose** | Synchronization & signaling | Mutual exclusion (resource protection) |
| **Ownership** | No ownership concept | Has ownership - only the task that takes it can give it |
| **Priority Inheritance** | Not supported | Supported (prevents priority inversion) |
| **Use from ISR** | Can be given from ISR | Cannot be given from ISR |
| **Initial State** | Created empty (0) | Created available (1) |
| **Recursive Taking** | Not applicable | Supported with recursive mutexes |
| **Best for** | Event notification, sync | Protecting shared resources |

### Example Demonstrating the Difference

```c
// WRONG: Using binary semaphore for resource protection
SemaphoreHandle_t xResourceSemaphore;
int sharedResource = 0;

void vTask1(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(xResourceSemaphore, portMAX_DELAY) == pdTRUE) {
            sharedResource++;  // Access shared resource
            xSemaphoreGive(xResourceSemaphore);
        }
    }
}

// RIGHT: Using mutex for resource protection
SemaphoreHandle_t xResourceMutex;

void vTask2(void *pvParameters) {
    for (;;) {
        if (xSemaphoreTake(xResourceMutex, portMAX_DELAY) == pdTRUE) {
            sharedResource++;  // Access shared resource
            xSemaphoreGive(xResourceMutex);  // Only this task can give it back
        }
    }
}

void app_main() {
    // For resource protection, use mutex
    xResourceMutex = xSemaphoreCreateMutex();
    
    // For signaling, use binary semaphore
    xResourceSemaphore = xSemaphoreCreateBinary();
}
```

## Advanced Pattern: Multiple Event Signaling

```c
SemaphoreHandle_t xDataReadySemaphore;
SemaphoreHandle_t xProcessingCompleteSemaphore;

void vDataAcquisitionTask(void *pvParameters) {
    for (;;) {
        // Simulate data acquisition
        printf("Acquiring data...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Signal that data is ready
        xSemaphoreGive(xDataReadySemaphore);
        
        // Wait for processing to complete before acquiring more
        xSemaphoreTake(xProcessingCompleteSemaphore, portMAX_DELAY);
    }
}

void vDataProcessingTask(void *pvParameters) {
    for (;;) {
        // Wait for data to be ready
        if (xSemaphoreTake(xDataReadySemaphore, portMAX_DELAY) == pdTRUE) {
            printf("Processing data...\n");
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Signal completion
            xSemaphoreGive(xProcessingCompleteSemaphore);
        }
    }
}

void app_main() {
    xDataReadySemaphore = xSemaphoreCreateBinary();
    xProcessingCompleteSemaphore = xSemaphoreCreateBinary();
    
    // Initialize the completion semaphore as available
    xSemaphoreGive(xProcessingCompleteSemaphore);
    
    xTaskCreate(vDataAcquisitionTask, "Acquisition", 2048, NULL, 3, NULL);
    xTaskCreate(vDataProcessingTask, "Processing", 2048, NULL, 3, NULL);
    
    vTaskStartScheduler();
}
```

## Important API Functions

```c
// Creation
SemaphoreHandle_t xSemaphoreCreateBinary(void);

// Taking (blocking)
BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, 
                          TickType_t xBlockTime);

// Giving (from task)
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);

// Giving (from ISR) - MUST use this in interrupts
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t xSemaphore,
                                 BaseType_t *pxHigherPriorityTaskWoken);

// Deletion
void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);
```

## Best Practices

1. **Use for synchronization, not mutual exclusion** - Binary semaphores don't provide priority inheritance, making them unsuitable for protecting shared resources

2. **Keep ISRs short** - Use semaphores to defer processing from ISRs to tasks

3. **Always check return values** - xSemaphoreTake and xSemaphoreGive return pdTRUE/pdFALSE

4. **Use appropriate timeouts** - Avoid portMAX_DELAY if the event might never occur

5. **Initialize correctly** - Binary semaphores start empty; give them first if needed

6. **Handle context switches in ISRs** - Always use xSemaphoreGiveFromISR with the priority task woken parameter

Binary semaphores are essential tools in FreeRTOS for creating responsive, event-driven embedded systems where tasks need to coordinate their execution efficiently.