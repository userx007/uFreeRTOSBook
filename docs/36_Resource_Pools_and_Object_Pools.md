# Resource Pools and Object Pools in FreeRTOS

## Overview

Resource pools and object pools are design patterns that pre-allocate a fixed number of reusable objects at initialization time, eliminating the need for dynamic memory allocation during runtime. This approach is crucial in real-time embedded systems where dynamic allocation can lead to heap fragmentation, unpredictable timing, and potential memory exhaustion.

## Why Use Resource Pools in FreeRTOS?

**Problems with Dynamic Allocation:**
- Heap fragmentation over time leads to allocation failures
- Non-deterministic execution time (malloc/free are unpredictable)
- Memory leaks if resources aren't properly freed
- Difficult to guarantee memory availability in safety-critical systems

**Benefits of Resource Pools:**
- Deterministic memory behavior and timing
- Prevents heap fragmentation
- Guaranteed resource availability (if pool size is adequate)
- Simplified memory management and debugging
- Better suited for certification (DO-178C, IEC 61508, etc.)

## Implementation Patterns

### 1. Basic Object Pool Structure

Here's a fundamental object pool implementation:

```c
#include "FreeRTOS.h"
#include "semphr.h"
#include "queue.h"

// Object pool configuration
#define POOL_SIZE 10

// Example: Message buffer pool
typedef struct {
    uint8_t data[256];
    uint16_t length;
    uint32_t timestamp;
} MessageBuffer_t;

// Pool structure
typedef struct {
    MessageBuffer_t buffers[POOL_SIZE];
    QueueHandle_t freeQueue;
    SemaphoreHandle_t mutex;
    uint32_t allocCount;
    uint32_t freeCount;
} MessagePool_t;

static MessagePool_t messagePool;

// Initialize the pool
BaseType_t MessagePool_Init(void)
{
    // Create queue to track free buffers
    messagePool.freeQueue = xQueueCreate(POOL_SIZE, sizeof(MessageBuffer_t*));
    if (messagePool.freeQueue == NULL) {
        return pdFAIL;
    }
    
    // Create mutex for statistics
    messagePool.mutex = xSemaphoreCreateMutex();
    if (messagePool.mutex == NULL) {
        vQueueDelete(messagePool.freeQueue);
        return pdFAIL;
    }
    
    // Initialize all buffers as free
    for (int i = 0; i < POOL_SIZE; i++) {
        MessageBuffer_t* pBuffer = &messagePool.buffers[i];
        xQueueSend(messagePool.freeQueue, &pBuffer, 0);
    }
    
    messagePool.allocCount = 0;
    messagePool.freeCount = 0;
    
    return pdPASS;
}

// Allocate a buffer from the pool
MessageBuffer_t* MessagePool_Alloc(TickType_t timeout)
{
    MessageBuffer_t* pBuffer = NULL;
    
    if (xQueueReceive(messagePool.freeQueue, &pBuffer, timeout) == pdPASS) {
        xSemaphoreTake(messagePool.mutex, portMAX_DELAY);
        messagePool.allocCount++;
        xSemaphoreGive(messagePool.mutex);
        
        // Clear the buffer
        memset(pBuffer, 0, sizeof(MessageBuffer_t));
    }
    
    return pBuffer;
}

// Return a buffer to the pool
BaseType_t MessagePool_Free(MessageBuffer_t* pBuffer)
{
    if (pBuffer == NULL) {
        return pdFAIL;
    }
    
    // Validate buffer belongs to pool
    if (pBuffer < &messagePool.buffers[0] || 
        pBuffer > &messagePool.buffers[POOL_SIZE - 1]) {
        return pdFAIL;
    }
    
    xSemaphoreTake(messagePool.mutex, portMAX_DELAY);
    messagePool.freeCount++;
    xSemaphoreGive(messagePool.mutex);
    
    return xQueueSend(messagePool.freeQueue, &pBuffer, 0);
}

// Get pool statistics
void MessagePool_GetStats(uint32_t* allocated, uint32_t* free, uint32_t* available)
{
    xSemaphoreTake(messagePool.mutex, portMAX_DELAY);
    *allocated = messagePool.allocCount;
    *free = messagePool.freeCount;
    xSemaphoreGive(messagePool.mutex);
    
    *available = uxQueueMessagesWaiting(messagePool.freeQueue);
}
```

### 2. TCP/IP Packet Buffer Pool Example

A more sophisticated example for network packet management:

```c
#define PACKET_POOL_SIZE 20
#define PACKET_DATA_SIZE 1500  // MTU size

typedef enum {
    PACKET_STATE_FREE,
    PACKET_STATE_ALLOCATED,
    PACKET_STATE_IN_USE
} PacketState_t;

typedef struct PacketBuffer {
    uint8_t data[PACKET_DATA_SIZE];
    uint16_t dataLength;
    uint16_t dataOffset;
    PacketState_t state;
    uint32_t allocTime;
    struct PacketBuffer* next;  // For chaining
} PacketBuffer_t;

typedef struct {
    PacketBuffer_t packets[PACKET_POOL_SIZE];
    SemaphoreHandle_t mutex;
    PacketBuffer_t* freeList;
    uint32_t totalAllocs;
    uint32_t totalFrees;
    uint32_t peakUsage;
    uint32_t currentUsage;
} PacketPool_t;

static PacketPool_t packetPool;

BaseType_t PacketPool_Init(void)
{
    packetPool.mutex = xSemaphoreCreateMutex();
    if (packetPool.mutex == NULL) {
        return pdFAIL;
    }
    
    // Build free list
    packetPool.freeList = NULL;
    for (int i = 0; i < PACKET_POOL_SIZE; i++) {
        packetPool.packets[i].state = PACKET_STATE_FREE;
        packetPool.packets[i].next = packetPool.freeList;
        packetPool.freeList = &packetPool.packets[i];
    }
    
    packetPool.totalAllocs = 0;
    packetPool.totalFrees = 0;
    packetPool.peakUsage = 0;
    packetPool.currentUsage = 0;
    
    return pdPASS;
}

PacketBuffer_t* PacketPool_Alloc(void)
{
    PacketBuffer_t* pPacket = NULL;
    
    xSemaphoreTake(packetPool.mutex, portMAX_DELAY);
    
    if (packetPool.freeList != NULL) {
        pPacket = packetPool.freeList;
        packetPool.freeList = pPacket->next;
        
        pPacket->state = PACKET_STATE_ALLOCATED;
        pPacket->dataLength = 0;
        pPacket->dataOffset = 0;
        pPacket->allocTime = xTaskGetTickCount();
        pPacket->next = NULL;
        
        packetPool.totalAllocs++;
        packetPool.currentUsage++;
        
        if (packetPool.currentUsage > packetPool.peakUsage) {
            packetPool.peakUsage = packetPool.currentUsage;
        }
    }
    
    xSemaphoreGive(packetPool.mutex);
    
    return pPacket;
}

void PacketPool_Free(PacketBuffer_t* pPacket)
{
    if (pPacket == NULL) {
        return;
    }
    
    xSemaphoreTake(packetPool.mutex, portMAX_DELAY);
    
    if (pPacket->state != PACKET_STATE_FREE) {
        pPacket->state = PACKET_STATE_FREE;
        pPacket->next = packetPool.freeList;
        packetPool.freeList = pPacket;
        
        packetPool.totalFrees++;
        packetPool.currentUsage--;
    }
    
    xSemaphoreGive(packetPool.mutex);
}

// Chain multiple packets for large transfers
PacketBuffer_t* PacketPool_AllocChain(uint16_t numPackets)
{
    PacketBuffer_t* head = NULL;
    PacketBuffer_t* tail = NULL;
    
    for (uint16_t i = 0; i < numPackets; i++) {
        PacketBuffer_t* pPacket = PacketPool_Alloc();
        if (pPacket == NULL) {
            // Allocation failed, free chain and return NULL
            PacketPool_FreeChain(head);
            return NULL;
        }
        
        if (head == NULL) {
            head = pPacket;
            tail = pPacket;
        } else {
            tail->next = pPacket;
            tail = pPacket;
        }
    }
    
    return head;
}

void PacketPool_FreeChain(PacketBuffer_t* pHead)
{
    while (pHead != NULL) {
        PacketBuffer_t* pNext = pHead->next;
        PacketPool_Free(pHead);
        pHead = pNext;
    }
}
```

### 3. Thread-Safe Task Context Pool

Managing task-specific contexts efficiently:

```c
#define MAX_TASKS 8

typedef struct {
    uint32_t taskId;
    void* userData;
    uint8_t workBuffer[512];
    SemaphoreHandle_t completionSemaphore;
    TaskHandle_t ownerTask;
    bool inUse;
} TaskContext_t;

typedef struct {
    TaskContext_t contexts[MAX_TASKS];
    SemaphoreHandle_t poolMutex;
} ContextPool_t;

static ContextPool_t contextPool;

BaseType_t ContextPool_Init(void)
{
    contextPool.poolMutex = xSemaphoreCreateMutex();
    if (contextPool.poolMutex == NULL) {
        return pdFAIL;
    }
    
    for (int i = 0; i < MAX_TASKS; i++) {
        contextPool.contexts[i].taskId = i;
        contextPool.contexts[i].completionSemaphore = xSemaphoreCreateBinary();
        contextPool.contexts[i].inUse = false;
        contextPool.contexts[i].ownerTask = NULL;
    }
    
    return pdPASS;
}

TaskContext_t* ContextPool_Acquire(TickType_t timeout)
{
    TickType_t startTime = xTaskGetTickCount();
    
    while (1) {
        xSemaphoreTake(contextPool.poolMutex, portMAX_DELAY);
        
        // Find first free context
        for (int i = 0; i < MAX_TASKS; i++) {
            if (!contextPool.contexts[i].inUse) {
                contextPool.contexts[i].inUse = true;
                contextPool.contexts[i].ownerTask = xTaskGetCurrentTaskHandle();
                
                xSemaphoreGive(contextPool.poolMutex);
                return &contextPool.contexts[i];
            }
        }
        
        xSemaphoreGive(contextPool.poolMutex);
        
        // Check timeout
        if (timeout != portMAX_DELAY) {
            TickType_t elapsed = xTaskGetTickCount() - startTime;
            if (elapsed >= timeout) {
                return NULL;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void ContextPool_Release(TaskContext_t* pContext)
{
    if (pContext == NULL) {
        return;
    }
    
    xSemaphoreTake(contextPool.poolMutex, portMAX_DELAY);
    
    if (pContext->ownerTask == xTaskGetCurrentTaskHandle()) {
        memset(pContext->workBuffer, 0, sizeof(pContext->workBuffer));
        pContext->userData = NULL;
        pContext->ownerTask = NULL;
        pContext->inUse = false;
    }
    
    xSemaphoreGive(contextPool.poolMutex);
}
```

### 4. Complete Application Example

Here's a practical example showing a data acquisition system using resource pools:

```c
// Sensor data acquisition system with resource pooling

#define SENSOR_POOL_SIZE 15
#define QUEUE_LENGTH 10

typedef struct {
    uint16_t sensorId;
    float temperature;
    float humidity;
    float pressure;
    uint32_t timestamp;
    bool valid;
} SensorData_t;

typedef struct {
    SensorData_t samples[SENSOR_POOL_SIZE];
    QueueHandle_t freeQueue;
    QueueHandle_t dataQueue;
    SemaphoreHandle_t mutex;
    uint32_t overruns;
} SensorPool_t;

static SensorPool_t sensorPool;

// Initialize the sensor data pool
void SensorPool_Init(void)
{
    sensorPool.freeQueue = xQueueCreate(SENSOR_POOL_SIZE, sizeof(SensorData_t*));
    sensorPool.dataQueue = xQueueCreate(QUEUE_LENGTH, sizeof(SensorData_t*));
    sensorPool.mutex = xSemaphoreCreateMutex();
    sensorPool.overruns = 0;
    
    // Initialize free pool
    for (int i = 0; i < SENSOR_POOL_SIZE; i++) {
        SensorData_t* pSample = &sensorPool.samples[i];
        xQueueSend(sensorPool.freeQueue, &pSample, 0);
    }
}

// Sensor acquisition task
void vSensorAcquisitionTask(void* pvParameters)
{
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 100ms sampling
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Get a free sample buffer
        SensorData_t* pSample = NULL;
        if (xQueueReceive(sensorPool.freeQueue, &pSample, 0) == pdPASS) {
            // Read sensors (simulated)
            pSample->sensorId = 1;
            pSample->temperature = 25.5f + (rand() % 100) / 100.0f;
            pSample->humidity = 60.0f + (rand() % 200) / 100.0f;
            pSample->pressure = 1013.25f + (rand() % 50) / 10.0f;
            pSample->timestamp = xTaskGetTickCount();
            pSample->valid = true;
            
            // Send to processing queue
            if (xQueueSend(sensorPool.dataQueue, &pSample, 0) != pdPASS) {
                // Queue full, return buffer to pool
                xQueueSend(sensorPool.freeQueue, &pSample, 0);
                
                xSemaphoreTake(sensorPool.mutex, portMAX_DELAY);
                sensorPool.overruns++;
                xSemaphoreGive(sensorPool.mutex);
            }
        } else {
            // Pool exhausted
            xSemaphoreTake(sensorPool.mutex, portMAX_DELAY);
            sensorPool.overruns++;
            xSemaphoreGive(sensorPool.mutex);
        }
    }
}

// Data processing task
void vDataProcessingTask(void* pvParameters)
{
    SensorData_t* pSample;
    
    while (1) {
        // Wait for data
        if (xQueueReceive(sensorPool.dataQueue, &pSample, portMAX_DELAY) == pdPASS) {
            // Process the data
            if (pSample->valid) {
                printf("Sensor %d: T=%.2f H=%.2f P=%.2f @ %lu\n",
                       pSample->sensorId,
                       pSample->temperature,
                       pSample->humidity,
                       pSample->pressure,
                       pSample->timestamp);
                
                // Perform calculations, filtering, etc.
                // ...
            }
            
            // Return buffer to free pool
            xQueueSend(sensorPool.freeQueue, &pSample, portMAX_DELAY);
        }
    }
}

// Monitoring task
void vMonitoringTask(void* pvParameters)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        uint32_t available = uxQueueMessagesWaiting(sensorPool.freeQueue);
        uint32_t pending = uxQueueMessagesWaiting(sensorPool.dataQueue);
        
        xSemaphoreTake(sensorPool.mutex, portMAX_DELAY);
        uint32_t overruns = sensorPool.overruns;
        xSemaphoreGive(sensorPool.mutex);
        
        printf("Pool Status: Free=%lu Pending=%lu Overruns=%lu\n",
               available, pending, overruns);
    }
}

// Application entry point
void app_main(void)
{
    SensorPool_Init();
    
    xTaskCreate(vSensorAcquisitionTask, "SensorAcq", 2048, NULL, 3, NULL);
    xTaskCreate(vDataProcessingTask, "DataProc", 2048, NULL, 2, NULL);
    xTaskCreate(vMonitoringTask, "Monitor", 2048, NULL, 1, NULL);
    
    vTaskStartScheduler();
}
```

## Best Practices

**Pool Sizing:** Determine pool size through profiling and worst-case analysis. Consider peak load scenarios and add margin for safety.

**Validation:** Always validate that returned objects actually belong to the pool to catch double-free errors.

**Statistics:** Track allocation/free counts, peak usage, and failures to optimize pool sizing and detect leaks.

**Initialization:** Pre-allocate pools during system initialization before the scheduler starts to ensure deterministic behavior.

**Error Handling:** Decide on timeout policies—should tasks block indefinitely, fail fast, or implement exponential backoff when pools are exhausted?

**Safety:** In safety-critical systems, consider implementing watchdog mechanisms that detect objects held too long and automatic recovery strategies.

Resource pools are fundamental to building robust, deterministic FreeRTOS applications that meet real-time requirements while avoiding the pitfalls of dynamic memory allocation.