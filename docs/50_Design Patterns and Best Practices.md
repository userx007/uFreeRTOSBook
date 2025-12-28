# Design Patterns and Best Practices in FreeRTOS

Effective RTOS application design requires understanding common patterns, potential pitfalls, and architectural principles that promote maintainable, scalable embedded systems. Let me walk you through the essential design patterns and best practices for FreeRTOS development.

## Common RTOS Design Patterns

### 1. Producer-Consumer Pattern

The producer-consumer pattern decouples data generation from data processing, allowing tasks to operate at different rates while maintaining system stability.

**Implementation using Queues:**

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define QUEUE_LENGTH 10
#define ITEM_SIZE sizeof(SensorData_t)

typedef struct {
    uint16_t temperature;
    uint16_t humidity;
    uint32_t timestamp;
} SensorData_t;

QueueHandle_t xDataQueue;

// Producer Task: Reads sensor data
void vProducerTask(void *pvParameters) {
    SensorData_t xSensorData;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for (;;) {
        // Read sensor data
        xSensorData.temperature = ReadTemperatureSensor();
        xSensorData.humidity = ReadHumiditySensor();
        xSensorData.timestamp = xTaskGetTickCount();
        
        // Send to queue (non-blocking with timeout)
        if (xQueueSend(xDataQueue, &xSensorData, pdMS_TO_TICKS(100)) != pdPASS) {
            // Handle queue full condition
            LogError("Queue full - data dropped");
        }
        
        // Run every 1 second
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}

// Consumer Task: Processes and logs data
void vConsumerTask(void *pvParameters) {
    SensorData_t xReceivedData;
    
    for (;;) {
        // Wait for data (blocking)
        if (xQueueReceive(xDataQueue, &xReceivedData, portMAX_DELAY) == pdPASS) {
            // Process the data
            ProcessSensorData(&xReceivedData);
            
            // Log to storage
            LogToFlash(&xReceivedData);
            
            // Check thresholds
            if (xReceivedData.temperature > TEMP_THRESHOLD) {
                TriggerAlarm();
            }
        }
    }
}

void CreateProducerConsumer(void) {
    // Create queue
    xDataQueue = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
    
    if (xDataQueue != NULL) {
        xTaskCreate(vProducerTask, "Producer", 256, NULL, 2, NULL);
        xTaskCreate(vConsumerTask, "Consumer", 512, NULL, 1, NULL);
    }
}
```

**Multiple Producers, Single Consumer:**

```c
// Multiple sensor tasks feeding one processing task
void vTemperatureSensorTask(void *pvParameters) {
    for (;;) {
        SensorData_t data = {.temperature = ReadTemp(), .humidity = 0};
        xQueueSend(xDataQueue, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vHumiditySensorTask(void *pvParameters) {
    for (;;) {
        SensorData_t data = {.temperature = 0, .humidity = ReadHumidity()};
        xQueueSend(xDataQueue, &data, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(750));
    }
}
```

### 2. Event-Driven Pattern

Event-driven architectures respond to asynchronous events using event groups or task notifications, making systems more reactive and power-efficient.

**Using Event Groups:**

```c
#include "event_groups.h"

// Event bit definitions
#define EVENT_BUTTON_PRESSED    (1 << 0)
#define EVENT_DATA_READY        (1 << 1)
#define EVENT_TIMER_EXPIRED     (1 << 2)
#define EVENT_ERROR_OCCURRED    (1 << 3)

EventGroupHandle_t xSystemEvents;

// Event dispatcher/handler task
void vEventHandlerTask(void *pvParameters) {
    EventBits_t xEventBits;
    const EventBits_t xBitsToWaitFor = EVENT_BUTTON_PRESSED | 
                                       EVENT_DATA_READY | 
                                       EVENT_TIMER_EXPIRED | 
                                       EVENT_ERROR_OCCURRED;
    
    for (;;) {
        // Wait for any event (blocks until at least one bit is set)
        xEventBits = xEventGroupWaitBits(
            xSystemEvents,
            xBitsToWaitFor,
            pdTRUE,  // Clear bits on exit
            pdFALSE, // Wait for any bit (not all)
            portMAX_DELAY
        );
        
        // Handle events based on priority
        if (xEventBits & EVENT_ERROR_OCCURRED) {
            HandleSystemError();
        }
        
        if (xEventBits & EVENT_BUTTON_PRESSED) {
            HandleButtonPress();
        }
        
        if (xEventBits & EVENT_DATA_READY) {
            ProcessAvailableData();
        }
        
        if (xEventBits & EVENT_TIMER_EXPIRED) {
            PerformPeriodicMaintenance();
        }
    }
}

// Event generators (ISRs or other tasks)
void vButtonISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    xEventGroupSetBitsFromISR(xSystemEvents, EVENT_BUTTON_PRESSED, 
                              &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vDataAcquisitionTask(void *pvParameters) {
    for (;;) {
        AcquireData();
        xEventGroupSetBits(xSystemEvents, EVENT_DATA_READY);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**State Machine with Events:**

```c
typedef enum {
    STATE_IDLE,
    STATE_ACTIVE,
    STATE_PROCESSING,
    STATE_ERROR
} SystemState_t;

void vStateMachineTask(void *pvParameters) {
    SystemState_t eCurrentState = STATE_IDLE;
    EventBits_t xEvents;
    
    for (;;) {
        xEvents = xEventGroupWaitBits(xSystemEvents, 0xFF, pdTRUE, pdFALSE, 
                                       pdMS_TO_TICKS(100));
        
        switch (eCurrentState) {
            case STATE_IDLE:
                if (xEvents & EVENT_BUTTON_PRESSED) {
                    eCurrentState = STATE_ACTIVE;
                    StartOperation();
                }
                break;
                
            case STATE_ACTIVE:
                if (xEvents & EVENT_DATA_READY) {
                    eCurrentState = STATE_PROCESSING;
                } else if (xEvents & EVENT_ERROR_OCCURRED) {
                    eCurrentState = STATE_ERROR;
                }
                break;
                
            case STATE_PROCESSING:
                ProcessData();
                eCurrentState = STATE_IDLE;
                break;
                
            case STATE_ERROR:
                HandleError();
                eCurrentState = STATE_IDLE;
                break;
        }
    }
}
```

### 3. Pipeline Pattern

The pipeline pattern chains multiple processing stages together, with each stage handling a specific transformation of the data.

```c
#define STAGE1_QUEUE_LENGTH 5
#define STAGE2_QUEUE_LENGTH 5
#define STAGE3_QUEUE_LENGTH 5

typedef struct {
    uint8_t rawData[64];
    uint16_t length;
} RawData_t;

typedef struct {
    uint8_t filteredData[64];
    uint16_t length;
    bool isValid;
} FilteredData_t;

typedef struct {
    float processedValue;
    uint32_t confidence;
} ProcessedData_t;

QueueHandle_t xStage1ToStage2Queue;
QueueHandle_t xStage2ToStage3Queue;
QueueHandle_t xOutputQueue;

// Stage 1: Data Acquisition
void vAcquisitionStage(void *pvParameters) {
    RawData_t xRawData;
    
    for (;;) {
        // Acquire raw data from sensor/hardware
        ReadSensorData(xRawData.rawData, &xRawData.length);
        
        // Pass to next stage
        xQueueSend(xStage1ToStage2Queue, &xRawData, portMAX_DELAY);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Stage 2: Filtering
void vFilteringStage(void *pvParameters) {
    RawData_t xRawData;
    FilteredData_t xFilteredData;
    
    for (;;) {
        // Wait for data from previous stage
        if (xQueueReceive(xStage1ToStage2Queue, &xRawData, portMAX_DELAY) == pdPASS) {
            // Apply filtering algorithms
            xFilteredData.isValid = ApplyFilter(xRawData.rawData, 
                                                 xRawData.length,
                                                 xFilteredData.filteredData,
                                                 &xFilteredData.length);
            
            // Pass to next stage
            xQueueSend(xStage2ToStage3Queue, &xFilteredData, portMAX_DELAY);
        }
    }
}

// Stage 3: Processing
void vProcessingStage(void *pvParameters) {
    FilteredData_t xFilteredData;
    ProcessedData_t xProcessedData;
    
    for (;;) {
        if (xQueueReceive(xStage2ToStage3Queue, &xFilteredData, portMAX_DELAY) == pdPASS) {
            if (xFilteredData.isValid) {
                // Perform complex processing
                xProcessedData.processedValue = AnalyzeData(xFilteredData.filteredData,
                                                             xFilteredData.length);
                xProcessedData.confidence = CalculateConfidence();
                
                // Output results
                xQueueSend(xOutputQueue, &xProcessedData, portMAX_DELAY);
            }
        }
    }
}

void CreatePipeline(void) {
    xStage1ToStage2Queue = xQueueCreate(STAGE1_QUEUE_LENGTH, sizeof(RawData_t));
    xStage2ToStage3Queue = xQueueCreate(STAGE2_QUEUE_LENGTH, sizeof(FilteredData_t));
    xOutputQueue = xQueueCreate(STAGE3_QUEUE_LENGTH, sizeof(ProcessedData_t));
    
    xTaskCreate(vAcquisitionStage, "Stage1", 256, NULL, 3, NULL);
    xTaskCreate(vFilteringStage, "Stage2", 512, NULL, 2, NULL);
    xTaskCreate(vProcessingStage, "Stage3", 512, NULL, 1, NULL);
}
```

## Avoiding Deadlocks and Race Conditions

### Deadlock Prevention

**1. Lock Ordering - Always acquire mutexes in the same order:**

```c
SemaphoreHandle_t xMutexA, xMutexB;

// CORRECT: Consistent lock ordering
void vTask1(void *pvParameters) {
    for (;;) {
        xSemaphoreTake(xMutexA, portMAX_DELAY);  // Always A first
        xSemaphoreTake(xMutexB, portMAX_DELAY);  // Then B
        
        // Critical section accessing both resources
        AccessSharedResourceAB();
        
        xSemaphoreGive(xMutexB);
        xSemaphoreGive(xMutexA);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vTask2(void *pvParameters) {
    for (;;) {
        xSemaphoreTake(xMutexA, portMAX_DELAY);  // Same order: A first
        xSemaphoreTake(xMutexB, portMAX_DELAY);  // Then B
        
        AccessSharedResourceAB();
        
        xSemaphoreGive(xMutexB);
        xSemaphoreGive(xMutexA);
        
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}
```

**2. Timeout-based Locking:**

```c
void vSafeTask(void *pvParameters) {
    const TickType_t xTimeout = pdMS_TO_TICKS(1000);
    
    for (;;) {
        // Try to acquire with timeout
        if (xSemaphoreTake(xMutexA, xTimeout) == pdPASS) {
            if (xSemaphoreTake(xMutexB, xTimeout) == pdPASS) {
                // Successfully acquired both
                AccessSharedResourceAB();
                
                xSemaphoreGive(xMutexB);
                xSemaphoreGive(xMutexA);
            } else {
                // Failed to get second mutex, release first
                xSemaphoreGive(xMutexA);
                LogWarning("Could not acquire mutex B");
            }
        } else {
            LogWarning("Could not acquire mutex A");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**3. Use Recursive Mutexes When Needed:**

```c
SemaphoreHandle_t xRecursiveMutex;

void Function1(void) {
    xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
    // Do work
    Function2();  // Can take same mutex again
    xSemaphoreGiveRecursive(xRecursiveMutex);
}

void Function2(void) {
    xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY);
    // Do more work
    xSemaphoreGiveRecursive(xRecursiveMutex);
}
```

### Race Condition Prevention

**1. Atomic Operations for Simple Variables:**

```c
#include "atomic.h"

// Use atomic operations for counters
static uint32_t ulCounter = 0;

void vIncrementCounter(void) {
    // Atomic increment - no race condition
    Atomic_Increment_u32(&ulCounter);
}

// For read-modify-write operations
void vUpdateFlags(void) {
    uint32_t ulOldValue, ulNewValue;
    
    do {
        ulOldValue = Atomic_CompareAndSwap_u32(&ulFlags, ulOldValue, ulNewValue);
        ulNewValue = ulOldValue | FLAG_BIT;
    } while (ulOldValue != ulNewValue);
}
```

**2. Critical Sections for Short Operations:**

```c
volatile uint32_t ulSharedVariable = 0;

void vTaskA(void *pvParameters) {
    for (;;) {
        taskENTER_CRITICAL();
        ulSharedVariable += 10;  // Atomic operation
        taskEXIT_CRITICAL();
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vTaskB(void *pvParameters) {
    uint32_t ulLocalCopy;
    
    for (;;) {
        taskENTER_CRITICAL();
        ulLocalCopy = ulSharedVariable;  // Atomic read
        taskEXIT_CRITICAL();
        
        ProcessValue(ulLocalCopy);
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

**3. Proper Mutex Usage for Complex Data Structures:**

```c
typedef struct {
    uint32_t counter;
    uint8_t buffer[256];
    uint16_t head;
    uint16_t tail;
} SharedData_t;

SharedData_t xSharedData;
SemaphoreHandle_t xDataMutex;

void vSafeWrite(uint8_t *pucData, uint16_t usLength) {
    xSemaphoreTake(xDataMutex, portMAX_DELAY);
    
    // Safe to modify all fields
    memcpy(&xSharedData.buffer[xSharedData.tail], pucData, usLength);
    xSharedData.tail = (xSharedData.tail + usLength) % 256;
    xSharedData.counter++;
    
    xSemaphoreGive(xDataMutex);
}

void vSafeRead(uint8_t *pucBuffer, uint16_t usLength) {
    xSemaphoreTake(xDataMutex, portMAX_DELAY);
    
    memcpy(pucBuffer, &xSharedData.buffer[xSharedData.head], usLength);
    xSharedData.head = (xSharedData.head + usLength) % 256;
    
    xSemaphoreGive(xDataMutex);
}
```

## Structuring Applications for Maintainability

### 1. Modular Task Architecture

```c
// task_manager.h - Central task management
typedef struct {
    TaskHandle_t xHandle;
    const char *pcName;
    UBaseType_t uxPriority;
    uint32_t ulStackSize;
} TaskInfo_t;

typedef enum {
    TASK_SENSOR,
    TASK_PROCESSING,
    TASK_COMMUNICATION,
    TASK_UI,
    TASK_COUNT
} TaskID_t;

// Global task registry
TaskInfo_t xTaskRegistry[TASK_COUNT];

void TaskManager_Init(void);
void TaskManager_Start(TaskID_t eTaskID);
void TaskManager_Stop(TaskID_t eTaskID);
void TaskManager_GetStats(TaskID_t eTaskID, TaskStatus_t *pxStatus);

// sensor_task.c - Self-contained module
void SensorTask_Init(void) {
    // Initialize hardware and queues
    xSensorQueue = xQueueCreate(10, sizeof(SensorData_t));
    InitializeSensorHardware();
}

void SensorTask_Run(void *pvParameters) {
    SensorData_t xData;
    
    for (;;) {
        xData = ReadSensor();
        xQueueSend(xSensorQueue, &xData, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

QueueHandle_t SensorTask_GetQueue(void) {
    return xSensorQueue;
}
```

### 2. Configuration Management

```c
// app_config.h - Centralized configuration
#define SENSOR_TASK_PRIORITY        (tskIDLE_PRIORITY + 2)
#define SENSOR_TASK_STACK_SIZE      256
#define SENSOR_SAMPLING_RATE_MS     1000

#define PROCESS_TASK_PRIORITY       (tskIDLE_PRIORITY + 1)
#define PROCESS_TASK_STACK_SIZE     512

#define DATA_QUEUE_LENGTH           10
#define EVENT_QUEUE_LENGTH          5

// Hardware-specific configuration
#if defined(HARDWARE_V1)
    #define UART_BAUD_RATE          9600
    #define I2C_CLOCK_SPEED         100000
#elif defined(HARDWARE_V2)
    #define UART_BAUD_RATE          115200
    #define I2C_CLOCK_SPEED         400000
#endif

// Feature flags
#define ENABLE_LOGGING              1
#define ENABLE_WATCHDOG             1
#define ENABLE_POWER_MANAGEMENT     1
```

### 3. Error Handling Strategy

```c
typedef enum {
    ERR_NONE = 0,
    ERR_QUEUE_FULL,
    ERR_QUEUE_EMPTY,
    ERR_MUTEX_TIMEOUT,
    ERR_INVALID_PARAM,
    ERR_HARDWARE_FAULT,
    ERR_OUT_OF_MEMORY
} ErrorCode_t;

typedef struct {
    ErrorCode_t eErrorCode;
    const char *pcTaskName;
    uint32_t ulTimestamp;
    uint32_t ulLineNumber;
} ErrorInfo_t;

QueueHandle_t xErrorQueue;

void ErrorHandler_Init(void) {
    xErrorQueue = xQueueCreate(10, sizeof(ErrorInfo_t));
    xTaskCreate(vErrorHandlerTask, "ErrorHandler", 512, NULL, 
                configMAX_PRIORITIES - 1, NULL);
}

void ReportError(ErrorCode_t eError, uint32_t ulLine) {
    ErrorInfo_t xError = {
        .eErrorCode = eError,
        .pcTaskName = pcTaskGetName(NULL),
        .ulTimestamp = xTaskGetTickCount(),
        .ulLineNumber = ulLine
    };
    
    xQueueSend(xErrorQueue, &xError, 0);  // Non-blocking
}

void vErrorHandlerTask(void *pvParameters) {
    ErrorInfo_t xError;
    
    for (;;) {
        if (xQueueReceive(xErrorQueue, &xError, portMAX_DELAY) == pdPASS) {
            // Log error
            LogError("[%lu] %s: Error %d at line %lu",
                    xError.ulTimestamp,
                    xError.pcTaskName,
                    xError.eErrorCode,
                    xError.ulLineNumber);
            
            // Take corrective action
            switch (xError.eErrorCode) {
                case ERR_HARDWARE_FAULT:
                    ResetHardware();
                    break;
                case ERR_OUT_OF_MEMORY:
                    TriggerMemoryCleanup();
                    break;
                default:
                    break;
            }
        }
    }
}
```

## Scalability Best Practices

### 1. Resource Pools

```c
#define BUFFER_POOL_SIZE 10
#define BUFFER_SIZE 256

typedef struct {
    uint8_t data[BUFFER_SIZE];
    bool inUse;
} Buffer_t;

Buffer_t xBufferPool[BUFFER_POOL_SIZE];
SemaphoreHandle_t xPoolMutex;

void BufferPool_Init(void) {
    xPoolMutex = xSemaphoreCreateMutex();
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        xBufferPool[i].inUse = false;
    }
}

Buffer_t* BufferPool_Allocate(void) {
    Buffer_t *pxBuffer = NULL;
    
    xSemaphoreTake(xPoolMutex, portMAX_DELAY);
    
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!xBufferPool[i].inUse) {
            xBufferPool[i].inUse = true;
            pxBuffer = &xBufferPool[i];
            break;
        }
    }
    
    xSemaphoreGive(xPoolMutex);
    return pxBuffer;
}

void BufferPool_Free(Buffer_t *pxBuffer) {
    xSemaphoreTake(xPoolMutex, portMAX_DELAY);
    pxBuffer->inUse = false;
    xSemaphoreGive(xPoolMutex);
}
```

### 2. Dynamic Priority Adjustment

```c
void vAdaptivePriorityTask(void *pvParameters) {
    UBaseType_t uxCurrentPriority = uxTaskPriorityGet(NULL);
    uint32_t ulWorkload;
    
    for (;;) {
        ulWorkload = MeasureSystemLoad();
        
        if (ulWorkload > 80) {
            // System under heavy load, increase priority
            vTaskPrioritySet(NULL, uxCurrentPriority + 1);
        } else if (ulWorkload < 20) {
            // Light load, can reduce priority
            vTaskPrioritySet(NULL, uxCurrentPriority - 1);
        }
        
        PerformWork();
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

These patterns and practices form the foundation of robust FreeRTOS applications. The key is selecting the right pattern for your specific requirements, consistently applying synchronization mechanisms, and structuring code for long-term maintainability.