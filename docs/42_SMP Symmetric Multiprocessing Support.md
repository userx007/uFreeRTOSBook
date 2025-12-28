# FreeRTOS SMP (Symmetric Multiprocessing) Support

## Overview

FreeRTOS SMP is an extension of FreeRTOS that enables true symmetric multiprocessing on multi-core processors. Unlike the traditional single-core FreeRTOS kernel, FreeRTOS SMP allows multiple tasks to execute simultaneously on different cores, significantly improving performance for multi-threaded applications.

## Key Concepts

### 1. **SMP Scheduler**

The SMP scheduler extends the traditional FreeRTOS scheduler to manage task execution across multiple cores simultaneously. Each core can run a different task at the same priority level, and the scheduler ensures fair distribution of tasks.

**Key differences from single-core:**
- Multiple tasks can run simultaneously (one per core)
- The ready list is shared across all cores
- Each core maintains its own current task pointer
- Scheduler decisions consider which cores are available

### 2. **Core Affinity**

Core affinity allows you to pin specific tasks to specific cores or allow them to run on any available core. This is crucial for:
- Cache optimization
- Real-time determinism
- Hardware-specific operations (peripherals tied to specific cores)
- Load balancing

## Configuration

```c
/* FreeRTOSConfig.h - SMP Configuration */

#define configNUM_CORES                    2
#define configUSE_CORE_AFFINITY           1
#define configRUN_MULTIPLE_PRIORITIES     1
#define configUSE_PASSIVE_IDLE_HOOK       0

/* Optional: Use spinlocks for critical sections */
#define configUSE_MINIMAL_IDLE_HOOK       0
```

## Core Affinity Examples

### Example 1: Creating Tasks with Core Affinity

```c
#include "FreeRTOS.h"
#include "task.h"

/* Task handles */
TaskHandle_t xCore0Task, xCore1Task, xAnyTask;

/* Task pinned to Core 0 */
void vCore0SpecificTask(void *pvParameters)
{
    while(1)
    {
        printf("Running on Core 0\n");
        
        /* Perform Core 0 specific operations */
        /* e.g., Handle specific peripheral, optimized cache usage */
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Task pinned to Core 1 */
void vCore1SpecificTask(void *pvParameters)
{
    while(1)
    {
        printf("Running on Core 1\n");
        
        /* Perform Core 1 specific operations */
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Task that can run on any core */
void vFlexibleTask(void *pvParameters)
{
    while(1)
    {
        /* Get current core ID */
        BaseType_t xCoreID = xPortGetCoreID();
        printf("Flexible task running on Core %d\n", xCoreID);
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vCreateAffinityTasks(void)
{
    /* Create task pinned to Core 0 */
    xTaskCreateAffinitySet(
        vCore0SpecificTask,      /* Task function */
        "Core0Task",              /* Task name */
        1024,                     /* Stack size */
        NULL,                     /* Parameters */
        2,                        /* Priority */
        (1 << 0),                 /* Affinity mask - Core 0 only */
        &xCore0Task               /* Task handle */
    );
    
    /* Create task pinned to Core 1 */
    xTaskCreateAffinitySet(
        vCore1SpecificTask,
        "Core1Task",
        1024,
        NULL,
        2,
        (1 << 1),                 /* Affinity mask - Core 1 only */
        &xCore1Task
    );
    
    /* Create task that can run on any core */
    xTaskCreateAffinitySet(
        vFlexibleTask,
        "FlexTask",
        1024,
        NULL,
        1,
        tskNO_AFFINITY,          /* Can run on any core */
        &xAnyTask
    );
}
```

### Example 2: Dynamic Affinity Management

```c
/* Change task affinity at runtime */
void vManageTaskAffinity(void)
{
    /* Get current affinity */
    UBaseType_t uxCurrentAffinity = vTaskCoreAffinityGet(xAnyTask);
    
    printf("Current affinity mask: 0x%x\n", uxCurrentAffinity);
    
    /* Pin to Core 0 */
    vTaskCoreAffinitySet(xAnyTask, (1 << 0));
    
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    /* Pin to Core 1 */
    vTaskCoreAffinitySet(xAnyTask, (1 << 1));
    
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    /* Allow any core */
    vTaskCoreAffinitySet(xAnyTask, tskNO_AFFINITY);
}
```

## Spinlocks for Inter-Core Synchronization

Spinlocks are lightweight synchronization primitives used in SMP systems when critical sections are very short and blocking would be more expensive than spinning.

### Example 3: Using Spinlocks

```c
#include "FreeRTOS.h"
#include "task.h"

/* Declare spinlock */
static portMUX_TYPE xMySpinlock = portMUX_INITIALIZER_UNLOCKED;

/* Shared resource protected by spinlock */
static volatile uint32_t ulSharedCounter = 0;

void vTaskIncrementCounter(void *pvParameters)
{
    while(1)
    {
        /* Enter critical section using spinlock */
        portENTER_CRITICAL(&xMySpinlock);
        
        /* Critical section - very short operations only! */
        ulSharedCounter++;
        uint32_t ulLocalCopy = ulSharedCounter;
        
        portEXIT_CRITICAL(&xMySpinlock);
        
        /* Non-critical work */
        printf("Core %d: Counter = %lu\n", xPortGetCoreID(), ulLocalCopy);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vCreateSpinlockExample(void)
{
    /* Create multiple tasks that will run on different cores */
    for(int i = 0; i < configNUM_CORES; i++)
    {
        xTaskCreateAffinitySet(
            vTaskIncrementCounter,
            "Counter",
            1024,
            NULL,
            1,
            (1 << i),              /* Pin to specific core */
            NULL
        );
    }
}
```

### Example 4: Spinlock vs Mutex Comparison

```c
/* When to use spinlocks vs mutexes in SMP */

/* BAD: Long operation in spinlock (blocks other cores) */
void vBadSpinlockUsage(void)
{
    portENTER_CRITICAL(&xMySpinlock);
    
    vTaskDelay(pdMS_TO_TICKS(100));  /* NEVER DO THIS! */
    /* Expensive computation */
    
    portEXIT_CRITICAL(&xMySpinlock);
}

/* GOOD: Use mutex for longer operations */
SemaphoreHandle_t xMutex;

void vGoodMutexUsage(void)
{
    xMutex = xSemaphoreCreateMutex();
    
    if(xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
    {
        /* Longer critical section is OK with mutex */
        /* Other cores can run other tasks */
        vTaskDelay(pdMS_TO_TICKS(100));
        
        xSemaphoreGive(xMutex);
    }
}

/* GOOD: Use spinlock for very short operations */
void vGoodSpinlockUsage(void)
{
    portENTER_CRITICAL(&xMySpinlock);
    
    /* Only quick register access or variable updates */
    ulSharedCounter++;
    
    portEXIT_CRITICAL(&xMySpinlock);
}
```

## Inter-Core Synchronization Patterns

### Example 5: Producer-Consumer Across Cores

```c
/* Queue for inter-core communication */
QueueHandle_t xInterCoreQueue;

typedef struct
{
    uint32_t ulData;
    uint32_t ulProducerCore;
} DataPacket_t;

/* Producer task on Core 0 */
void vProducerTask(void *pvParameters)
{
    DataPacket_t xPacket;
    uint32_t ulCounter = 0;
    
    while(1)
    {
        xPacket.ulData = ulCounter++;
        xPacket.ulProducerCore = xPortGetCoreID();
        
        if(xQueueSend(xInterCoreQueue, &xPacket, pdMS_TO_TICKS(100)) == pdPASS)
        {
            printf("Core %d: Produced %lu\n", 
                   xPacket.ulProducerCore, xPacket.ulData);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* Consumer task on Core 1 */
void vConsumerTask(void *pvParameters)
{
    DataPacket_t xPacket;
    
    while(1)
    {
        if(xQueueReceive(xInterCoreQueue, &xPacket, portMAX_DELAY) == pdPASS)
        {
            printf("Core %d: Consumed %lu from Core %d\n",
                   xPortGetCoreID(), xPacket.ulData, xPacket.ulProducerCore);
            
            /* Process data */
        }
    }
}

void vSetupInterCoreComm(void)
{
    /* Create queue for inter-core communication */
    xInterCoreQueue = xQueueCreate(10, sizeof(DataPacket_t));
    
    /* Create producer on Core 0 */
    xTaskCreateAffinitySet(
        vProducerTask,
        "Producer",
        2048,
        NULL,
        2,
        (1 << 0),
        NULL
    );
    
    /* Create consumer on Core 1 */
    xTaskCreateAffinitySet(
        vConsumerTask,
        "Consumer",
        2048,
        NULL,
        2,
        (1 << 1),
        NULL
    );
}
```

### Example 6: Load Balancing with SMP

```c
/* Workload distribution across cores */
typedef struct
{
    uint32_t ulWorkID;
    uint32_t ulComplexity;
} WorkItem_t;

QueueHandle_t xWorkQueue;
static portMUX_TYPE xStatsLock = portMUX_INITIALIZER_UNLOCKED;
static uint32_t ulTasksPerCore[configNUM_CORES] = {0};

void vWorkerTask(void *pvParameters)
{
    WorkItem_t xWork;
    BaseType_t xCoreID;
    
    while(1)
    {
        if(xQueueReceive(xWorkQueue, &xWork, portMAX_DELAY) == pdPASS)
        {
            xCoreID = xPortGetCoreID();
            
            printf("Core %d: Processing work %lu (complexity: %lu)\n",
                   xCoreID, xWork.ulWorkID, xWork.ulComplexity);
            
            /* Simulate work proportional to complexity */
            vTaskDelay(pdMS_TO_TICKS(xWork.ulComplexity * 10));
            
            /* Update statistics */
            portENTER_CRITICAL(&xStatsLock);
            ulTasksPerCore[xCoreID]++;
            portEXIT_CRITICAL(&xStatsLock);
            
            printf("Core %d: Completed work %lu (total: %lu)\n",
                   xCoreID, xWork.ulWorkID, ulTasksPerCore[xCoreID]);
        }
    }
}

void vWorkDispatcherTask(void *pvParameters)
{
    WorkItem_t xWork;
    uint32_t ulWorkID = 0;
    
    while(1)
    {
        xWork.ulWorkID = ulWorkID++;
        xWork.ulComplexity = (ulWorkID % 10) + 1;
        
        xQueueSend(xWorkQueue, &xWork, portMAX_DELAY);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vCreateLoadBalancedSystem(void)
{
    xWorkQueue = xQueueCreate(20, sizeof(WorkItem_t));
    
    /* Create worker tasks that can run on any core */
    for(int i = 0; i < 4; i++)
    {
        xTaskCreateAffinitySet(
            vWorkerTask,
            "Worker",
            2048,
            NULL,
            2,
            tskNO_AFFINITY,  /* Scheduler distributes across cores */
            NULL
        );
    }
    
    /* Create dispatcher */
    xTaskCreate(vWorkDispatcherTask, "Dispatcher", 2048, NULL, 3, NULL);
}
```

## Critical Section Considerations in SMP

### Example 7: Avoiding Common SMP Pitfalls

```c
/* Shared data structure */
typedef struct
{
    uint32_t ulValue1;
    uint32_t ulValue2;
} SharedData_t;

static SharedData_t xSharedData;
static portMUX_TYPE xDataLock = portMUX_INITIALIZER_UNLOCKED;

/* WRONG: Inconsistent protection */
void vIncorrectAccess(void)
{
    /* CPU 0 might read while CPU 1 is updating */
    xSharedData.ulValue1 = 100;  /* NOT PROTECTED! */
    xSharedData.ulValue2 = 200;
}

/* CORRECT: Consistent spinlock protection */
void vCorrectAccess(void)
{
    portENTER_CRITICAL(&xDataLock);
    
    xSharedData.ulValue1 = 100;
    xSharedData.ulValue2 = 200;
    
    portEXIT_CRITICAL(&xDataLock);
}

/* CORRECT: Atomic operations for simple updates */
void vAtomicUpdate(void)
{
    /* For single variable updates, atomic operations are faster */
    __atomic_store_n(&xSharedData.ulValue1, 100, __ATOMIC_SEQ_CST);
}

/* Reading shared data */
void vReadSharedData(uint32_t *pulVal1, uint32_t *pulVal2)
{
    portENTER_CRITICAL(&xDataLock);
    
    /* Get consistent snapshot */
    *pulVal1 = xSharedData.ulValue1;
    *pulVal2 = xSharedData.ulValue2;
    
    portEXIT_CRITICAL(&xDataLock);
}
```

## Performance Optimization

### Example 8: Cache-Aware Task Allocation

```c
/* Allocate tasks to minimize cache coherency traffic */

/* Data processed exclusively by Core 0 */
static uint32_t ulCore0Data[1000] __attribute__((aligned(64)));

/* Data processed exclusively by Core 1 */
static uint32_t ulCore1Data[1000] __attribute__((aligned(64)));

void vCore0ProcessingTask(void *pvParameters)
{
    while(1)
    {
        /* Process local data - good cache locality */
        for(int i = 0; i < 1000; i++)
        {
            ulCore0Data[i] = ulCore0Data[i] * 2;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vCore1ProcessingTask(void *pvParameters)
{
    while(1)
    {
        /* Process local data - good cache locality */
        for(int i = 0; i < 1000; i++)
        {
            ulCore1Data[i] = ulCore1Data[i] + 1;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## Best Practices

1. **Use Core Affinity Wisely**: Pin latency-critical tasks to specific cores to avoid migration overhead
2. **Keep Spinlock Sections Short**: Never call blocking functions inside spinlock-protected sections
3. **Prefer Mutexes for Long Operations**: Use mutexes instead of spinlocks when critical sections take more than a few microseconds
4. **Minimize Shared Data**: Reduce cache coherency overhead by keeping data local to cores when possible
5. **Balance Load**: Let the scheduler distribute non-critical tasks across cores for optimal throughput
6. **Test on Target Hardware**: SMP behavior varies significantly across different multi-core architectures

## Common Use Cases

- **Dual-core ESP32**: Dedicate one core to WiFi/network stack, another to application logic
- **Multi-core ARM**: Separate real-time control tasks from data processing tasks
- **High-throughput systems**: Parallel processing pipelines with producer-consumer patterns
- **Safety-critical systems**: Isolate critical functions on dedicated cores with strict affinity

FreeRTOS SMP provides powerful tools for leveraging modern multi-core processors while maintaining the simplicity and reliability of the FreeRTOS programming model.