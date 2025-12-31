# Mutexes and Priority Inheritance in FreeRTOS

## Overview

Mutexes (Mutual Exclusion semaphores) are specialized synchronization primitives in FreeRTOS designed specifically for protecting shared resources from simultaneous access by multiple tasks. Unlike binary semaphores, mutexes include a critical feature called **priority inheritance** that prevents a common real-time system problem known as **priority inversion**.

## The Priority Inversion Problem

Priority inversion occurs when a high-priority task is blocked waiting for a resource held by a low-priority task, while a medium-priority task preempts the low-priority task, effectively causing the high-priority task to wait for the medium-priority task to complete.

### Classic Priority Inversion Scenario

```
Time ──────────────────────────────────>

Low Priority Task (L):    [Acquires Resource]────[blocked by M]──[Releases]
Medium Priority Task (M):                    [──── Running ────]
High Priority Task (H):                [Waits for Resource──────────][Runs]
```

In this scenario:
1. Task L acquires a mutex-protected resource
2. Task H becomes ready and preempts L (higher priority)
3. Task H tries to acquire the same resource and blocks
4. Task M becomes ready and preempts L (M > L priority)
5. Task H is indirectly waiting for Task M to complete!

This violates the priority-based scheduling principle and can cause deadline misses in real-time systems.

## Priority Inheritance Solution

When using mutexes with priority inheritance, if a high-priority task blocks on a mutex held by a low-priority task, the low-priority task **temporarily inherits** the priority of the high-priority task until it releases the mutex.

### How Priority Inheritance Works

```c
// Low priority task temporarily elevated to high priority
Time ──────────────────────────────────>

Low Priority Task (L):    [Acquires]──[Inherits H's priority]──[Releases]
Medium Priority Task (M):                    [Blocked - cannot preempt]
High Priority Task (H):                [Waits]──────────────────[Runs]
```

Now Task M cannot preempt Task L because L has inherited H's priority, ensuring Task H waits only for Task L to finish its critical section.

## Creating and Using Mutexes

### Basic Mutex Creation

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

SemaphoreHandle_t xMutex;

void setup(void)
{
    // Create a mutex
    xMutex = xSemaphoreCreateMutex();
    
    if (xMutex != NULL)
    {
        // Mutex created successfully
        // Create tasks that will use this mutex
        xTaskCreate(vLowPriorityTask, "LowTask", 200, NULL, 1, NULL);
        xTaskCreate(vMediumPriorityTask, "MedTask", 200, NULL, 2, NULL);
        xTaskCreate(vHighPriorityTask, "HighTask", 200, NULL, 3, NULL);
    }
}
```

### Acquiring and Releasing Mutexes

```c
void vTaskFunction(void *pvParameters)
{
    for (;;)
    {
        // Try to take the mutex (wait indefinitely)
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            // Successfully acquired the mutex
            // Access shared resource safely
            shared_resource++;
            
            // Always release the mutex when done
            xSemaphoreGive(xMutex);
        }
        
        // Other task work
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Complete Priority Inheritance Example

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

SemaphoreHandle_t xMutex;
volatile uint32_t shared_counter = 0;

// Low priority task
void vLowPriorityTask(void *pvParameters)
{
    for (;;)
    {
        printf("Low: Attempting to take mutex\n");
        
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            printf("Low: Acquired mutex, working...\n");
            
            // Simulate long critical section
            for (uint32_t i = 0; i < 1000000; i++)
            {
                shared_counter++;
            }
            
            printf("Low: Releasing mutex\n");
            xSemaphoreGive(xMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Medium priority task (doesn't use mutex)
void vMediumPriorityTask(void *pvParameters)
{
    for (;;)
    {
        printf("Medium: Running (no mutex needed)\n");
        
        // Simulate work
        for (uint32_t i = 0; i < 500000; i++)
        {
            // Busy work
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// High priority task
void vHighPriorityTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(100)); // Let low priority task run first
    
    for (;;)
    {
        printf("High: Attempting to take mutex\n");
        
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            printf("High: Acquired mutex, working...\n");
            shared_counter += 10;
            
            printf("High: Releasing mutex\n");
            xSemaphoreGive(xMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    // Create mutex with priority inheritance
    xMutex = xSemaphoreCreateMutex();
    
    if (xMutex != NULL)
    {
        xTaskCreate(vLowPriorityTask, "Low", 1000, NULL, 1, NULL);
        xTaskCreate(vMediumPriorityTask, "Medium", 1000, NULL, 2, NULL);
        xTaskCreate(vHighPriorityTask, "High", 1000, NULL, 3, NULL);
        
        vTaskStartScheduler();
    }
    
    // Should never reach here
    for (;;);
    return 0;
}
```

## Recursive Mutexes

A recursive mutex allows the same task to take the mutex multiple times without deadlocking itself. The mutex must be released the same number of times it was taken.

### When to Use Recursive Mutexes

Recursive mutexes are useful when:
- Functions call other functions that also need the same mutex
- Implementing nested critical sections
- Recursive algorithms need resource protection

### Creating Recursive Mutexes

```c
SemaphoreHandle_t xRecursiveMutex;

void setup(void)
{
    xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
    
    if (xRecursiveMutex != NULL)
    {
        // Successfully created
    }
}
```

### Using Recursive Mutexes

```c
// Use special recursive mutex functions
void vRecursiveFunction(uint32_t level)
{
    if (xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY) == pdTRUE)
    {
        printf("Acquired mutex at level %u\n", level);
        
        // Access shared resource
        shared_resource++;
        
        if (level > 0)
        {
            // Recursive call - will acquire mutex again
            vRecursiveFunction(level - 1);
        }
        
        // Must release the same number of times as taken
        xSemaphoreGiveRecursive(xRecursiveMutex);
        printf("Released mutex at level %u\n", level);
    }
}
```

### Complete Recursive Mutex Example

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

SemaphoreHandle_t xRecursiveMutex;
uint32_t shared_data = 0;

// Function that calls itself recursively
void vProcessData(uint32_t depth)
{
    if (xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY) == pdTRUE)
    {
        printf("  Level %u: Processing data = %u\n", depth, shared_data);
        shared_data++;
        
        if (depth > 0)
        {
            // Recursive call - will take the mutex again
            vProcessData(depth - 1);
        }
        
        // Must release for each take
        xSemaphoreGiveRecursive(xRecursiveMutex);
    }
}

// High-level function that also needs the mutex
void vUpdateData(uint32_t value)
{
    if (xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY) == pdTRUE)
    {
        printf("Updating data with value %u\n", value);
        shared_data = value;
        
        // Call another function that needs the same mutex
        vProcessData(3);
        
        printf("Update complete. Final data = %u\n", shared_data);
        
        xSemaphoreGiveRecursive(xRecursiveMutex);
    }
}

void vTask(void *pvParameters)
{
    for (;;)
    {
        vUpdateData(100);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
    
    if (xRecursiveMutex != NULL)
    {
        xTaskCreate(vTask, "Task", 1000, NULL, 1, NULL);
        vTaskStartScheduler();
    }
    
    for (;;);
    return 0;
}
```

## Key Differences: Mutex vs Binary Semaphore

| Feature | Mutex | Binary Semaphore |
|---------|-------|------------------|
| Priority Inheritance | Yes | No |
| Ownership | Task that takes it must give it | Any task can give |
| Recursive Taking | Supported (recursive mutex) | Not supported |
| Use Case | Resource protection | Task synchronization |
| ISR Usage | Cannot be used | Can be given from ISR |

## Best Practices

1. **Always release mutexes**: Ensure every `xSemaphoreTake()` has a corresponding `xSemaphoreGive()`

2. **Keep critical sections short**: Minimize the time a mutex is held to reduce blocking time

3. **Avoid nested mutexes**: Taking multiple mutexes can lead to deadlock

```c
// Deadlock risk
Task1: Take(MutexA) -> Take(MutexB)
Task2: Take(MutexB) -> Take(MutexA)
```

4. **Use timeouts when appropriate**: Instead of `portMAX_DELAY`, consider a reasonable timeout

```c
if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(100)) == pdTRUE)
{
    // Got mutex
}
else
{
    // Handle timeout
}
```

5. **Choose the right tool**: Use mutexes for resource protection, binary semaphores for synchronization

6. **Never use from ISR**: Mutexes cannot be taken or given from interrupt service routines

## Common Pitfalls

1. **Forgetting to release**: Always use structured code to ensure release
2. **Using in ISRs**: Use binary semaphores instead for ISR-to-task signaling
3. **Priority inversion awareness**: Even with priority inheritance, minimize critical section time
4. **Recursive mutex overhead**: Only use when necessary; regular mutexes are more efficient

Understanding mutexes and priority inheritance is crucial for building robust, predictable real-time systems with FreeRTOS. They provide the foundation for safe multi-task resource sharing while maintaining the integrity of priority-based scheduling.