# Priority Inversion in FreeRTOS

Priority inversion is a problematic scheduling scenario where a high-priority task is blocked waiting for a resource held by a low-priority task, while a medium-priority task preempts the low-priority task and runs instead. This effectively inverts the intended priority scheme.

## How It Happens

Consider three tasks with priorities: High > Medium > Low

1. **Low-priority task** acquires a mutex/semaphore
2. **High-priority task** preempts and tries to acquire the same mutex - gets blocked
3. **Medium-priority task** becomes ready and preempts the low-priority task
4. The medium-priority task runs while the high-priority task waits, even though it should have higher priority

The high-priority task is now stuck waiting for the low-priority task to finish, but the low-priority task can't run because the medium-priority task keeps preempting it. This is priority inversion.

## Real-World Impact

The classic example is the Mars Pathfinder incident in 1997, where priority inversion caused system resets. A low-priority meteorological task held a shared resource needed by a high-priority communication task, while medium-priority tasks prevented the low-priority task from completing.

## Solutions in FreeRTOS

### 1. **Priority Inheritance**

FreeRTOS mutexes support priority inheritance by default when you use `xSemaphoreCreateMutex()` or `xSemaphoreCreateRecursiveMutex()`.

**How it works:** When a high-priority task blocks on a mutex held by a low-priority task, the low-priority task temporarily inherits the high-priority task's priority. This allows it to complete quickly and release the mutex.

```c
// Create a mutex (has priority inheritance built-in)
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();

// Low priority task
void vLowPriorityTask(void *pvParameters) {
    xSemaphoreTake(xMutex, portMAX_DELAY);
    // Critical section - if high priority task blocks here,
    // this task inherits its priority
    // ... work ...
    xSemaphoreGive(xMutex);
}
```

**Important:** Binary semaphores and counting semaphores do NOT support priority inheritance - only mutexes do.

### 2. **Priority Ceiling Protocol**

Set the mutex holder's priority to a predetermined ceiling priority (higher than all tasks that might use it). FreeRTOS doesn't directly implement this, but you can manually adjust task priorities.

### 3. **Avoid Long Critical Sections**

Keep mutex-protected sections as short as possible to minimize blocking time:

```c
xSemaphoreTake(xMutex, portMAX_DELAY);
// Only protect the minimal critical data access
sharedVariable++;
xSemaphoreGive(xMutex);
// Do other work outside the mutex
```

### 4. **Disable Preemption for Critical Sections**

For very short critical sections, use task scheduler suspension:

```c
vTaskSuspendAll();
// Critical section - no task switching occurs
sharedVariable++;
xTaskResumeAll();
```

This prevents priority inversion but stops all task switching, so use sparingly and only for very brief operations.

### 5. **Design Considerations**

- Minimize resource sharing between tasks of different priorities
- Use message queues instead of shared memory when possible
- Carefully consider task priority assignments
- Avoid having low-priority tasks hold resources that high-priority tasks need

## Key Takeaways

Priority inversion is insidious because it only manifests under specific timing conditions, making it hard to debug. In FreeRTOS, always use mutexes (not binary semaphores) for shared resource protection when tasks of different priorities are involved, as they automatically provide priority inheritance. Keep critical sections short and consider your system's priority architecture carefully during design.