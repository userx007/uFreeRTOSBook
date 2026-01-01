# Comprehensive reference guide of FreeRTOS APIs 

**Main Categories:**
1. **Task Management** - Creating, deleting, and controlling tasks
2. **Queue Management** - Inter-task communication with FIFO queues
3. **Semaphores & Mutexes** - Synchronization and resource protection
4. **Event Groups** - Multi-bit synchronization flags
5. **Software Timers** - Callback-based timing operations
6. **Memory Management** - Dynamic heap allocation
7. **Task Notifications** - Lightweight task-to-task signaling
8. **Stream Buffers** - Continuous byte stream communication
9. **Message Buffers** - Discrete message passing
10. **Critical Sections & Interrupts** - Interrupt management
11. **Co-routines** - (Deprecated but included)
12. **Utility Macros** - Helper functions and constants

Each API includes:
- Function signature/name
- Brief description of what it does
- Practical usage example with context
- ISR-safe variants where applicable

The guide also notes that many features require specific configuration in `FreeRTOSConfig.h`. This should serve as a quick reference for embedded development with FreeRTOS!

# FreeRTOS API Reference Guide

## 1. Task Management APIs

### xTaskCreate()
Creates a new task and adds it to the list of tasks ready to run.

```c
TaskHandle_t xHandle = NULL;

void vTaskCode(void *pvParameters) {
    for(;;) {
        // Task code
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

xTaskCreate(vTaskCode, "TaskName", 128, NULL, 1, &xHandle);
```

### xTaskCreateStatic()
Creates a task using statically allocated memory (no heap required).

```c
StaticTask_t xTaskBuffer;
StackType_t xStack[128];

xTaskCreateStatic(vTaskCode, "StaticTask", 128, NULL, 1, xStack, &xTaskBuffer);
```

### vTaskDelete()
Deletes a task, removing it from the kernel's management.

```c
vTaskDelete(xHandle);  // Delete specific task
vTaskDelete(NULL);     // Delete calling task
```

### vTaskDelay()
Delays a task for a specified number of ticks.

```c
vTaskDelay(pdMS_TO_TICKS(500));  // Delay for 500ms
```

### vTaskDelayUntil()
Delays a task until a specific time, useful for periodic tasks.

```c
TickType_t xLastWakeTime = xTaskGetTickCount();
const TickType_t xFrequency = pdMS_TO_TICKS(100);

for(;;) {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    // Execute every 100ms
}
```

### uxTaskPriorityGet()
Returns the priority of a task.

```c
UBaseType_t priority = uxTaskPriorityGet(xHandle);
```

### vTaskPrioritySet()
Sets the priority of a task.

```c
vTaskPrioritySet(xHandle, 3);  // Set priority to 3
```

### vTaskSuspend()
Suspends a task (stops it from executing).

```c
vTaskSuspend(xHandle);  // Suspend specific task
vTaskSuspend(NULL);     // Suspend calling task
```

### vTaskResume()
Resumes a suspended task.

```c
vTaskResume(xHandle);
```

### xTaskResumeFromISR()
Resumes a task from an ISR, returns pdTRUE if context switch needed.

```c
BaseType_t xYieldRequired = xTaskResumeFromISR(xHandle);
portYIELD_FROM_ISR(xYieldRequired);
```

### taskYIELD()
Forces a context switch to allow other tasks to run.

```c
taskYIELD();
```

### xTaskGetTickCount()
Returns the current tick count.

```c
TickType_t xTicks = xTaskGetTickCount();
```

### xTaskGetTickCountFromISR()
Returns the current tick count from an ISR.

```c
TickType_t xTicks = xTaskGetTickCountFromISR();
```

### pcTaskGetName()
Returns the name of a task.

```c
const char *pcName = pcTaskGetName(xHandle);
```

### xTaskGetHandle()
Returns the handle of a task given its name.

```c
TaskHandle_t xHandle = xTaskGetHandle("TaskName");
```

### uxTaskGetStackHighWaterMark()
Returns the minimum amount of stack space that has remained for a task.

```c
UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(xHandle);
```

---

## 2. Queue Management APIs

### xQueueCreate()
Creates a queue with specified length and item size.

```c
QueueHandle_t xQueue = xQueueCreate(10, sizeof(uint32_t));
```

### xQueueCreateStatic()
Creates a queue using statically allocated memory.

```c
StaticQueue_t xQueueBuffer;
uint8_t ucQueueStorage[10 * sizeof(uint32_t)];

QueueHandle_t xQueue = xQueueCreateStatic(10, sizeof(uint32_t), 
                                          ucQueueStorage, &xQueueBuffer);
```

### xQueueSend()
Sends an item to the back of a queue.

```c
uint32_t ulValue = 100;
if(xQueueSend(xQueue, &ulValue, pdMS_TO_TICKS(100)) == pdPASS) {
    // Item sent successfully
}
```

### xQueueSendToFront()
Sends an item to the front of a queue.

```c
xQueueSendToFront(xQueue, &ulValue, pdMS_TO_TICKS(100));
```

### xQueueSendToBack()
Sends an item to the back of a queue (same as xQueueSend).

```c
xQueueSendToBack(xQueue, &ulValue, pdMS_TO_TICKS(100));
```

### xQueueSendFromISR()
Sends an item to a queue from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xQueueSendFromISR(xQueue, &ulValue, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### xQueueReceive()
Receives an item from a queue, removing it.

```c
uint32_t ulReceivedValue;
if(xQueueReceive(xQueue, &ulReceivedValue, pdMS_TO_TICKS(100)) == pdPASS) {
    // Item received successfully
}
```

### xQueuePeek()
Receives an item from a queue without removing it.

```c
uint32_t ulPeekedValue;
xQueuePeek(xQueue, &ulPeekedValue, pdMS_TO_TICKS(100));
```

### xQueueReceiveFromISR()
Receives an item from a queue from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xQueueReceiveFromISR(xQueue, &ulReceivedValue, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### uxQueueMessagesWaiting()
Returns the number of messages in a queue.

```c
UBaseType_t uxCount = uxQueueMessagesWaiting(xQueue);
```

### uxQueueSpacesAvailable()
Returns the number of free spaces in a queue.

```c
UBaseType_t uxSpaces = uxQueueSpacesAvailable(xQueue);
```

### xQueueReset()
Resets a queue to its empty state.

```c
xQueueReset(xQueue);
```

### vQueueDelete()
Deletes a queue.

```c
vQueueDelete(xQueue);
```

---

## 3. Semaphore & Mutex APIs

### xSemaphoreCreateBinary()
Creates a binary semaphore (initially empty).

```c
SemaphoreHandle_t xSemaphore = xSemaphoreCreateBinary();
```

### xSemaphoreCreateCounting()
Creates a counting semaphore with max and initial count.

```c
SemaphoreHandle_t xSemaphore = xSemaphoreCreateCounting(10, 0);
```

### xSemaphoreCreateMutex()
Creates a mutex semaphore for mutual exclusion.

```c
SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();
```

### xSemaphoreCreateRecursiveMutex()
Creates a recursive mutex (can be taken multiple times by same task).

```c
SemaphoreHandle_t xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
```

### xSemaphoreTake()
Takes (acquires) a semaphore or mutex.

```c
if(xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Semaphore acquired
    // Critical section
    xSemaphoreGive(xSemaphore);
}
```

### xSemaphoreTakeRecursive()
Takes a recursive mutex.

```c
xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
// Can be called multiple times
xSemaphoreGiveRecursive(xRecursiveMutex);
```

### xSemaphoreGive()
Gives (releases) a semaphore or mutex.

```c
xSemaphoreGive(xSemaphore);
```

### xSemaphoreGiveRecursive()
Gives a recursive mutex.

```c
xSemaphoreGiveRecursive(xRecursiveMutex);
```

### xSemaphoreGiveFromISR()
Gives a semaphore from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### xSemaphoreTakeFromISR()
Takes a semaphore from an ISR (binary and counting only).

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xSemaphoreTakeFromISR(xSemaphore, &xHigherPriorityTaskWoken);
```

### uxSemaphoreGetCount()
Returns the count value of a semaphore.

```c
UBaseType_t uxCount = uxSemaphoreGetCount(xSemaphore);
```

### vSemaphoreDelete()
Deletes a semaphore or mutex.

```c
vSemaphoreDelete(xSemaphore);
```

---

## 4. Event Group APIs

### xEventGroupCreate()
Creates an event group.

```c
EventGroupHandle_t xEventGroup = xEventGroupCreate();
```

### xEventGroupSetBits()
Sets bits in an event group.

```c
#define BIT_0 (1 << 0)
#define BIT_4 (1 << 4)

xEventGroupSetBits(xEventGroup, BIT_0 | BIT_4);
```

### xEventGroupSetBitsFromISR()
Sets bits in an event group from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xEventGroupSetBitsFromISR(xEventGroup, BIT_0, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### xEventGroupClearBits()
Clears bits in an event group.

```c
xEventGroupClearBits(xEventGroup, BIT_0 | BIT_4);
```

### xEventGroupClearBitsFromISR()
Clears bits in an event group from an ISR.

```c
xEventGroupClearBitsFromISR(xEventGroup, BIT_0);
```

### xEventGroupWaitBits()
Waits for bits to be set in an event group.

```c
EventBits_t uxBits = xEventGroupWaitBits(
    xEventGroup,
    BIT_0 | BIT_4,      // Bits to wait for
    pdTRUE,             // Clear bits on exit
    pdTRUE,             // Wait for all bits
    pdMS_TO_TICKS(100)  // Timeout
);
```

### xEventGroupGetBits()
Returns the current value of the event group.

```c
EventBits_t uxBits = xEventGroupGetBits(xEventGroup);
```

### xEventGroupGetBitsFromISR()
Returns the event group value from an ISR.

```c
EventBits_t uxBits = xEventGroupGetBitsFromISR(xEventGroup);
```

### xEventGroupSync()
Synchronizes tasks using event bits.

```c
xEventGroupSync(
    xEventGroup,
    BIT_0,              // Bits to set
    BIT_0 | BIT_4,      // Bits to wait for
    portMAX_DELAY
);
```

### vEventGroupDelete()
Deletes an event group.

```c
vEventGroupDelete(xEventGroup);
```

---

## 5. Software Timer APIs

### xTimerCreate()
Creates a software timer.

```c
void vTimerCallback(TimerHandle_t xTimer) {
    // Timer expired
}

TimerHandle_t xTimer = xTimerCreate(
    "Timer",            // Name
    pdMS_TO_TICKS(500), // Period
    pdTRUE,             // Auto-reload
    (void*)0,           // Timer ID
    vTimerCallback      // Callback
);
```

### xTimerCreateStatic()
Creates a timer using statically allocated memory.

```c
StaticTimer_t xTimerBuffer;
TimerHandle_t xTimer = xTimerCreateStatic(
    "Timer",
    pdMS_TO_TICKS(500),
    pdTRUE,
    (void*)0,
    vTimerCallback,
    &xTimerBuffer
);
```

### xTimerStart()
Starts a timer.

```c
xTimerStart(xTimer, 0);
```

### xTimerStartFromISR()
Starts a timer from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xTimerStartFromISR(xTimer, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### xTimerStop()
Stops a timer.

```c
xTimerStop(xTimer, 0);
```

### xTimerStopFromISR()
Stops a timer from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xTimerStopFromISR(xTimer, &xHigherPriorityTaskWoken);
```

### xTimerReset()
Resets a timer (restarts it from the beginning).

```c
xTimerReset(xTimer, 0);
```

### xTimerResetFromISR()
Resets a timer from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xTimerResetFromISR(xTimer, &xHigherPriorityTaskWoken);
```

### xTimerChangePeriod()
Changes the period of a timer.

```c
xTimerChangePeriod(xTimer, pdMS_TO_TICKS(1000), 0);
```

### xTimerChangePeriodFromISR()
Changes the period of a timer from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xTimerChangePeriodFromISR(xTimer, pdMS_TO_TICKS(1000), &xHigherPriorityTaskWoken);
```

### xTimerIsTimerActive()
Checks if a timer is active.

```c
if(xTimerIsTimerActive(xTimer) != pdFALSE) {
    // Timer is active
}
```

### xTimerGetPeriod()
Returns the period of a timer.

```c
TickType_t xPeriod = xTimerGetPeriod(xTimer);
```

### xTimerGetExpiryTime()
Returns the time at which the timer will expire.

```c
TickType_t xExpiryTime = xTimerGetExpiryTime(xTimer);
```

### pcTimerGetName()
Returns the name of a timer.

```c
const char *pcName = pcTimerGetName(xTimer);
```

### pvTimerGetTimerID()
Returns the timer ID.

```c
void *pvID = pvTimerGetTimerID(xTimer);
```

### vTimerSetTimerID()
Sets the timer ID.

```c
vTimerSetTimerID(xTimer, (void*)123);
```

### xTimerDelete()
Deletes a timer.

```c
xTimerDelete(xTimer, 0);
```

---

## 6. Memory Management APIs

### pvPortMalloc()
Allocates memory from the FreeRTOS heap.

```c
uint8_t *pucBuffer = (uint8_t*)pvPortMalloc(100);
if(pucBuffer != NULL) {
    // Use buffer
    vPortFree(pucBuffer);
}
```

### vPortFree()
Frees memory allocated by pvPortMalloc().

```c
vPortFree(pucBuffer);
```

### xPortGetFreeHeapSize()
Returns the amount of free heap space.

```c
size_t xFreeHeap = xPortGetFreeHeapSize();
```

### xPortGetMinimumEverFreeHeapSize()
Returns the minimum amount of free heap since boot.

```c
size_t xMinimumEverFreeHeap = xPortGetMinimumEverFreeHeapSize();
```

---

## 7. Task Notification APIs

### xTaskNotify()
Sends a notification to a task, updating its notification value.

```c
xTaskNotify(xHandle, 0x01, eSetBits);
```

### xTaskNotifyGive()
Sends a notification incrementing the notification value (lightweight semaphore).

```c
xTaskNotifyGive(xHandle);
```

### xTaskNotifyGiveFromISR()
Sends a notification from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
vTaskNotifyGiveFromISR(xHandle, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### ulTaskNotifyTake()
Waits for a notification (lightweight semaphore).

```c
uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
```

### xTaskNotifyWait()
Waits for a notification with bitwise operations.

```c
uint32_t ulNotificationValue;
xTaskNotifyWait(
    0x00,           // Don't clear bits on entry
    0xFFFFFFFF,     // Clear all bits on exit
    &ulNotificationValue,
    portMAX_DELAY
);
```

### xTaskNotifyFromISR()
Sends a notification from an ISR with action.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xTaskNotifyFromISR(xHandle, 0x01, eSetBits, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### xTaskNotifyStateClear()
Clears the notification state of a task.

```c
xTaskNotifyStateClear(xHandle);
```

---

## 8. Stream Buffer APIs

### xStreamBufferCreate()
Creates a stream buffer.

```c
StreamBufferHandle_t xStreamBuffer = xStreamBufferCreate(1000, 1);
```

### xStreamBufferSend()
Sends data to a stream buffer.

```c
uint8_t ucData[20];
size_t xBytesSent = xStreamBufferSend(xStreamBuffer, ucData, 20, pdMS_TO_TICKS(100));
```

### xStreamBufferReceive()
Receives data from a stream buffer.

```c
uint8_t ucRxData[20];
size_t xBytesReceived = xStreamBufferReceive(xStreamBuffer, ucRxData, 20, pdMS_TO_TICKS(100));
```

### xStreamBufferSendFromISR()
Sends data to a stream buffer from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xStreamBufferSendFromISR(xStreamBuffer, ucData, 20, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### xStreamBufferReceiveFromISR()
Receives data from a stream buffer from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xStreamBufferReceiveFromISR(xStreamBuffer, ucRxData, 20, &xHigherPriorityTaskWoken);
```

### xStreamBufferReset()
Resets a stream buffer to empty.

```c
xStreamBufferReset(xStreamBuffer);
```

### xStreamBufferIsEmpty()
Checks if a stream buffer is empty.

```c
if(xStreamBufferIsEmpty(xStreamBuffer) == pdTRUE) {
    // Buffer is empty
}
```

### xStreamBufferIsFull()
Checks if a stream buffer is full.

```c
if(xStreamBufferIsFull(xStreamBuffer) == pdTRUE) {
    // Buffer is full
}
```

### xStreamBufferBytesAvailable()
Returns the number of bytes in the stream buffer.

```c
size_t xBytes = xStreamBufferBytesAvailable(xStreamBuffer);
```

### xStreamBufferSpacesAvailable()
Returns the free space in the stream buffer.

```c
size_t xSpace = xStreamBufferSpacesAvailable(xStreamBuffer);
```

### vStreamBufferDelete()
Deletes a stream buffer.

```c
vStreamBufferDelete(xStreamBuffer);
```

---

## 9. Message Buffer APIs

### xMessageBufferCreate()
Creates a message buffer (stream buffer for discrete messages).

```c
MessageBufferHandle_t xMessageBuffer = xMessageBufferCreate(1000);
```

### xMessageBufferSend()
Sends a discrete message to a message buffer.

```c
uint8_t ucMessage[20];
xMessageBufferSend(xMessageBuffer, ucMessage, 20, pdMS_TO_TICKS(100));
```

### xMessageBufferReceive()
Receives a discrete message from a message buffer.

```c
uint8_t ucRxMessage[50];
size_t xReceivedBytes = xMessageBufferReceive(xMessageBuffer, ucRxMessage, 50, pdMS_TO_TICKS(100));
```

### xMessageBufferSendFromISR()
Sends a message from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xMessageBufferSendFromISR(xMessageBuffer, ucMessage, 20, &xHigherPriorityTaskWoken);
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

### xMessageBufferReceiveFromISR()
Receives a message from an ISR.

```c
BaseType_t xHigherPriorityTaskWoken = pdFALSE;
xMessageBufferReceiveFromISR(xMessageBuffer, ucRxMessage, 50, &xHigherPriorityTaskWoken);
```

### xMessageBufferReset()
Resets a message buffer.

```c
xMessageBufferReset(xMessageBuffer);
```

### vMessageBufferDelete()
Deletes a message buffer.

```c
vMessageBufferDelete(xMessageBuffer);
```

---

## 10. Critical Section & Interrupt APIs

### taskENTER_CRITICAL()
Enters a critical section (disables interrupts).

```c
taskENTER_CRITICAL();
// Critical code
taskEXIT_CRITICAL();
```

### taskEXIT_CRITICAL()
Exits a critical section (re-enables interrupts).

```c
taskEXIT_CRITICAL();
```

### taskENTER_CRITICAL_FROM_ISR()
Enters a critical section from an ISR.

```c
UBaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
// Critical code
taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
```

### taskEXIT_CRITICAL_FROM_ISR()
Exits a critical section from an ISR.

```c
taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
```

### taskDISABLE_INTERRUPTS()
Disables interrupts (macro).

```c
taskDISABLE_INTERRUPTS();
// Critical code
taskENABLE_INTERRUPTS();
```

### taskENABLE_INTERRUPTS()
Enables interrupts (macro).

```c
taskENABLE_INTERRUPTS();
```

### portYIELD_FROM_ISR()
Performs a context switch from an ISR if needed.

```c
portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
```

---

## 11. Co-routine APIs (Deprecated)

Co-routines are a lighter weight alternative to tasks but are now deprecated in favor of tasks. They are included here for completeness.

### xCoRoutineCreate()
Creates a co-routine.

```c
xCoRoutineCreate(vCoRoutineCode, 0, 0);
```

### vCoRoutineSchedule()
Schedules co-routines to run.

```c
vCoRoutineSchedule();
```

---

## 12. Utility Macros

### pdMS_TO_TICKS()
Converts milliseconds to ticks.

```c
TickType_t xDelay = pdMS_TO_TICKS(100);  // 100ms
```

### pdTICKS_TO_MS()
Converts ticks to milliseconds.

```c
TickType_t xMS = pdTICKS_TO_MS(xTicks);
```

### portMAX_DELAY
Maximum delay value (wait forever).

```c
xSemaphoreTake(xSemaphore, portMAX_DELAY);
```

### pdTRUE / pdFALSE
Boolean values in FreeRTOS.

```c
if(xSemaphoreTake(xSemaphore, 0) == pdTRUE) {
    // Semaphore acquired
}
```

### pdPASS / pdFAIL
Return values for many FreeRTOS APIs.

```c
if(xQueueSend(xQueue, &data, 0) == pdPASS) {
    // Success
}
```

---

## Configuration Notes

Many FreeRTOS features require specific configuration in `FreeRTOSConfig.h`:

- `configUSE_MUTEXES` - Enable mutex support
- `configUSE_RECURSIVE_MUTEXES` - Enable recursive mutexes
- `configUSE_COUNTING_SEMAPHORES` - Enable counting semaphores
- `configUSE_TIMERS` - Enable software timers
- `configUSE_TASK_NOTIFICATIONS` - Enable task notifications
- `configSUPPORT_DYNAMIC_ALLOCATION` - Enable dynamic memory allocation
- `configSUPPORT_STATIC_ALLOCATION` - Enable static memory allocation
- `configUSE_16_BIT_TICKS` - Use 16-bit tick counter (default is 32-bit)

Always check your `FreeRTOSConfig.h` to ensure required features are enabled.