# Performance Optimization in FreeRTOS

Performance optimization in FreeRTOS involves fine-tuning various aspects of the RTOS to minimize overhead, reduce latency, and maximize efficiency. Let's explore each key area with practical examples.

## 1. Reducing Context Switch Overhead

Context switching is the process of saving the state of one task and restoring another. This overhead can significantly impact system performance, especially in systems with frequent task switches.

### Optimization Techniques:

**Minimize the number of tasks**: Each additional task increases scheduling overhead.

```c
// POOR: Too many tasks for simple operations
void ledTask1(void *pvParameters) {
    while(1) {
        GPIO_SetBits(GPIOA, GPIO_Pin_0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void ledTask2(void *pvParameters) {
    while(1) {
        GPIO_SetBits(GPIOA, GPIO_Pin_1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// BETTER: Combine related functionality
void ledControlTask(void *pvParameters) {
    while(1) {
        GPIO_SetBits(GPIOA, GPIO_Pin_0);
        GPIO_SetBits(GPIOA, GPIO_Pin_1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

**Use appropriate task priorities**: Avoid unnecessary preemptions.

```c
// Configure priorities to minimize context switches
#define CRITICAL_TASK_PRIORITY      (configMAX_PRIORITIES - 1)
#define PERIODIC_TASK_PRIORITY      (tskIDLE_PRIORITY + 2)
#define BACKGROUND_TASK_PRIORITY    (tskIDLE_PRIORITY + 1)

xTaskCreate(criticalTask, "Critical", 512, NULL, CRITICAL_TASK_PRIORITY, NULL);
xTaskCreate(periodicTask, "Periodic", 512, NULL, PERIODIC_TASK_PRIORITY, NULL);
xTaskCreate(backgroundTask, "Background", 512, NULL, BACKGROUND_TASK_PRIORITY, NULL);
```

**Reduce floating-point usage**: FPU context saving adds significant overhead.

```c
// Configure FPU lazy stacking in FreeRTOSConfig.h
#define configUSE_TASK_FPU_SUPPORT 2  // Lazy context switching for FPU

// Avoid unnecessary floating-point operations
// POOR: Unnecessary float conversion
float calculateAverage(int values[], int count) {
    float sum = 0.0f;
    for(int i = 0; i < count; i++) {
        sum += (float)values[i];
    }
    return sum / count;
}

// BETTER: Use integer arithmetic when possible
int calculateAverageInt(int values[], int count) {
    int sum = 0;
    for(int i = 0; i < count; i++) {
        sum += values[i];
    }
    return sum / count;
}
```

## 2. Optimizing Interrupt Latency

Interrupt latency is the time between an interrupt occurring and the ISR beginning execution. FreeRTOS adds some overhead to this.

### Optimization Techniques:

**Keep ISRs short and simple**:

```c
// POOR: Too much processing in ISR
void UART_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(UART_GetITStatus(UART1, UART_IT_RXNE)) {
        char data = UART_ReceiveData(UART1);
        
        // TOO MUCH WORK IN ISR!
        processData(data);
        updateDisplay(data);
        logToFlash(data);
        
        xTaskNotifyFromISR(taskHandle, data, eSetValueWithOverwrite, 
                          &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// BETTER: Defer processing to task
void UART_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(UART_GetITStatus(UART1, UART_IT_RXNE)) {
        char data = UART_ReceiveData(UART1);
        
        // Just queue the data and return quickly
        xQueueSendFromISR(uartQueue, &data, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

**Use direct-to-task notifications instead of queues**:

```c
// Slower: Using queue (more overhead)
void Timer_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t event = TIMER_EVENT;
    xQueueSendFromISR(eventQueue, &event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Faster: Using task notification (less overhead)
void Timer_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(timerTaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

**Configure interrupt priorities correctly**:

```c
// FreeRTOSConfig.h - Set the kernel interrupt priority
#define configKERNEL_INTERRUPT_PRIORITY         255  // Lowest priority
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191  // 5 levels for FreeRTOS

// In your initialization
void configureInterrupts(void) {
    // Critical, time-sensitive interrupt (ABOVE FreeRTOS priority)
    // Cannot use FreeRTOS API calls!
    NVIC_SetPriority(TIM1_IRQn, 1);  
    
    // Normal interrupt (BELOW FreeRTOS priority)
    // Can use FromISR API calls
    NVIC_SetPriority(UART1_IRQn, 5);  
    
    // Lower priority interrupt
    NVIC_SetPriority(DMA1_IRQn, 10);
}
```

## 3. Choosing Appropriate Tick Rates

The tick rate determines how often the scheduler runs. This is a critical trade-off between responsiveness and overhead.

### Configuration Examples:

```c
// FreeRTOSConfig.h

// OPTION 1: High-performance, lower time resolution
#define configTICK_RATE_HZ              ((TickType_t)100)   // 10ms tick
// Pros: Less overhead (1% CPU @ 100Hz vs 10% @ 1000Hz)
// Cons: Minimum delay is 10ms

// OPTION 2: Responsive, higher overhead
#define configTICK_RATE_HZ              ((TickType_t)1000)  // 1ms tick
// Pros: Fine-grained timing control
// Cons: More CPU used for tick interrupt

// OPTION 3: Balanced approach
#define configTICK_RATE_HZ              ((TickType_t)250)   // 4ms tick
// Good compromise for many applications
```

**Practical example showing tick rate impact**:

```c
// With 100Hz tick rate:
void lowPriorityTask(void *pvParameters) {
    while(1) {
        doWork();
        vTaskDelay(pdMS_TO_TICKS(15));  // Actual delay: 20ms (rounds up to 2 ticks)
    }
}

// With 1000Hz tick rate:
void lowPriorityTask(void *pvParameters) {
    while(1) {
        doWork();
        vTaskDelay(pdMS_TO_TICKS(15));  // Actual delay: 15ms (15 ticks)
    }
}
```

**Using tickless idle mode for power savings**:

```c
// FreeRTOSConfig.h
#define configUSE_TICKLESS_IDLE         1

// Implement these functions for your hardware
void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime) {
    // Stop tick interrupt
    // Enter low-power mode
    // Configure wakeup timer
    // Sleep
    // Compensate tick count on wake
}
```

## 4. Memory Optimization Techniques

Memory is often limited in embedded systems. Optimizing memory usage is crucial.

### Stack Size Optimization:

```c
// Use stack overflow detection during development
#define configCHECK_FOR_STACK_OVERFLOW  2

// Start with generous stack sizes, then optimize
xTaskCreate(dataProcessingTask, "DataProc", 
            2048,  // Start large during development
            NULL, 2, &taskHandle);

// Monitor actual stack usage
void monitorStackUsage(void) {
    UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(taskHandle);
    printf("Stack remaining: %u words\n", highWaterMark);
    
    // If highWaterMark is consistently > 500, reduce stack size
    // Always leave safety margin (20-30% headroom)
}

// After profiling, optimize:
xTaskCreate(dataProcessingTask, "DataProc", 
            512,   // Reduced after profiling showed 400 words max usage
            NULL, 2, &taskHandle);
```

### Heap Optimization:

```c
// FreeRTOSConfig.h - Choose appropriate heap scheme

// Heap_1: Simple, no free(), most deterministic
// Good for: Systems that create tasks once at startup
#define configSUPPORT_DYNAMIC_ALLOCATION 1

// Heap_2: Allows free(), fast but fragmentation
// Avoid for long-running systems

// Heap_3: Uses malloc/free wrapper
// Good for: Systems with existing malloc

// Heap_4: Best for most applications (coalescence)
// Good for: Dynamic task creation/deletion

// Heap_5: Supports multiple memory regions
// Good for: Systems with external RAM

#define configTOTAL_HEAP_SIZE           ((size_t)(20 * 1024))  // 20KB

// Monitor heap usage
void checkHeapUsage(void) {
    size_t freeHeap = xPortGetFreeHeapSize();
    size_t minEverFree = xPortGetMinimumEverFreeHeapSize();
    
    printf("Current free: %u bytes\n", freeHeap);
    printf("Minimum ever free: %u bytes\n", minEverFree);
    
    // If minEverFree is very low, increase configTOTAL_HEAP_SIZE
}
```

### Efficient Data Structures:

```c
// POOR: Allocating buffers for every message
typedef struct {
    char *data;
    uint16_t length;
} Message_t;

void sendMessage(const char *msg) {
    Message_t message;
    message.length = strlen(msg);
    message.data = pvPortMalloc(message.length);  // Heap allocation!
    strcpy(message.data, msg);
    xQueueSend(messageQueue, &message, portMAX_DELAY);
}

// BETTER: Fixed-size messages, no dynamic allocation
#define MAX_MESSAGE_LENGTH 64

typedef struct {
    char data[MAX_MESSAGE_LENGTH];
    uint16_t length;
} Message_t;

void sendMessage(const char *msg) {
    Message_t message;
    message.length = strlen(msg);
    strncpy(message.data, msg, MAX_MESSAGE_LENGTH - 1);
    message.data[MAX_MESSAGE_LENGTH - 1] = '\0';
    xQueueSend(messageQueue, &message, portMAX_DELAY);
}
```

### Static Allocation:

```c
// FreeRTOSConfig.h
#define configSUPPORT_STATIC_ALLOCATION 1

// Define buffers at compile time (no heap used)
static StaticTask_t xTaskBuffer;
static StackType_t xStack[512];

void createStaticTask(void) {
    TaskHandle_t xHandle = xTaskCreateStatic(
        vTaskCode,
        "StaticTask",
        512,
        NULL,
        2,
        xStack,
        &xTaskBuffer
    );
}

// Static queue (no heap fragmentation)
#define QUEUE_LENGTH 10
static StaticQueue_t xQueueBuffer;
static uint8_t ucQueueStorageArea[QUEUE_LENGTH * sizeof(uint32_t)];

QueueHandle_t createStaticQueue(void) {
    return xQueueCreateStatic(
        QUEUE_LENGTH,
        sizeof(uint32_t),
        ucQueueStorageArea,
        &xQueueBuffer
    );
}
```

## Comprehensive Optimization Example

Here's a complete example showing multiple optimization techniques:

```c
// Optimized sensor reading system
#define SENSOR_TASK_STACK_SIZE      256
#define PROCESSING_TASK_STACK_SIZE  512
#define SENSOR_QUEUE_LENGTH         5

// Static allocation for predictable memory usage
static StaticTask_t sensorTaskBuffer;
static StackType_t sensorStack[SENSOR_TASK_STACK_SIZE];
static StaticQueue_t sensorQueueBuffer;
static uint8_t queueStorage[SENSOR_QUEUE_LENGTH * sizeof(uint32_t)];

QueueHandle_t sensorQueue;
TaskHandle_t processingTaskHandle;

// Efficient ISR - minimal work
void SENSOR_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Just notify, no queue overhead
    vTaskNotifyGiveFromISR(processingTaskHandle, &xHigherPriorityTaskWoken);
    
    // Clear interrupt flag
    SENSOR_CLEAR_FLAG();
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Sensor reading task - uses integer math
void sensorTask(void *pvParameters) {
    uint32_t sensorValue;
    
    while(1) {
        // Read sensor (no floating point)
        sensorValue = ADC_Read();
        
        // Simple filtering using integer math
        static uint32_t average = 0;
        average = (average * 3 + sensorValue) >> 2;  // Bit shift instead of divide
        
        // Send to processing
        xQueueSend(sensorQueue, &average, 0);
        
        // Efficient delay
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Processing task
void processingTask(void *pvParameters) {
    uint32_t value;
    
    while(1) {
        // Wait for notification from ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Process queued data
        while(xQueueReceive(sensorQueue, &value, 0) == pdTRUE) {
            processValue(value);
        }
    }
}

void initOptimizedSystem(void) {
    // Create static queue
    sensorQueue = xQueueCreateStatic(
        SENSOR_QUEUE_LENGTH,
        sizeof(uint32_t),
        queueStorage,
        &sensorQueueBuffer
    );
    
    // Create static task
    xTaskCreateStatic(
        sensorTask,
        "Sensor",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        2,
        sensorStack,
        &sensorTaskBuffer
    );
    
    // Create processing task
    xTaskCreate(
        processingTask,
        "Processing",
        PROCESSING_TASK_STACK_SIZE,
        NULL,
        3,
        &processingTaskHandle
    );
}
```

## Key Performance Metrics to Monitor

```c
void printPerformanceStats(void) {
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    uint32_t ulTotalRunTime, ulStatsAsPercentage;
    
    // Get number of tasks
    uxArraySize = uxTaskGetNumberOfTasks();
    
    // Allocate array
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    
    if(pxTaskStatusArray != NULL) {
        // Get task stats
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, 
                                           uxArraySize, 
                                           &ulTotalRunTime);
        
        // Calculate percentage
        ulTotalRunTime /= 100UL;
        
        for(x = 0; x < uxArraySize; x++) {
            ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter 
                                  / ulTotalRunTime;
            
            printf("Task: %s\tCPU: %u%%\tStack HWM: %u\n",
                   pxTaskStatusArray[x].pcTaskName,
                   ulStatsAsPercentage,
                   pxTaskStatusArray[x].usStackHighWaterMark);
        }
        
        vPortFree(pxTaskStatusArray);
    }
    
    printf("Heap free: %u bytes\n", xPortGetFreeHeapSize());
    printf("Min heap: %u bytes\n", xPortGetMinimumEverFreeHeapSize());
}
```

These optimization techniques, when applied appropriately, can significantly improve FreeRTOS performance, reduce power consumption, and make better use of limited embedded system resources.