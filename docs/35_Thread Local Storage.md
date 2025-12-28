# Thread Local Storage (TLS) in FreeRTOS

## Overview

Thread Local Storage (TLS) in FreeRTOS provides a mechanism for tasks to maintain their own private data that persists across context switches but remains isolated from other tasks. Each task can have an array of pointers that point to task-specific data structures, allowing different tasks to access what appears to be the same "global" variable but actually retrieve their own unique copy.

## Understanding TLS Architecture

FreeRTOS implements TLS through an array of pointers stored in each task's Task Control Block (TCB). The size of this array is configured at compile time using `configNUM_THREAD_LOCAL_STORAGE_POINTERS` in FreeRTOSConfig.h.

### Key Characteristics

- **Per-task isolation**: Each task has its own independent TLS array
- **Persistent across context switches**: TLS data survives when the scheduler switches between tasks
- **Fixed size**: The number of TLS pointers is determined at compile time
- **Index-based access**: TLS slots are accessed using integer indices (0 to N-1)
- **Optional callbacks**: Deletion callbacks can be registered to clean up TLS data when a task is deleted

## Core TLS Functions

### Setting TLS Pointers

```c
void vTaskSetThreadLocalStoragePointer(
    TaskHandle_t xTaskToSet,
    BaseType_t xIndex,
    void *pvValue
);
```

### Retrieving TLS Pointers

```c
void *pvTaskGetThreadLocalStoragePointer(
    TaskHandle_t xTaskToQuery,
    BaseType_t xIndex
);
```

### Setting Deletion Callbacks (if enabled)

```c
#if (configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS == 1)
void vTaskSetThreadLocalStoragePointerAndDelCallback(
    TaskHandle_t xTaskToSet,
    BaseType_t xIndex,
    void *pvValue,
    TlsDeleteCallbackFunction_t pvDelCallback
);
#endif
```

## Configuration

In FreeRTOSConfig.h:

```c
// Define the number of TLS pointers per task
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS 3

// Enable deletion callbacks (optional)
#define configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS 1
```

## Practical Examples

### Example 1: Per-Task Error Counters

Different tasks tracking their own error statistics:

```c
#include "FreeRTOS.h"
#include "task.h"

#define TLS_INDEX_ERROR_COUNTER 0

typedef struct {
    uint32_t criticalErrors;
    uint32_t warnings;
    uint32_t totalErrors;
} ErrorStats_t;

// Initialize error tracking for a task
void initTaskErrorTracking(TaskHandle_t task) {
    ErrorStats_t *stats = pvPortMalloc(sizeof(ErrorStats_t));
    
    if (stats != NULL) {
        stats->criticalErrors = 0;
        stats->warnings = 0;
        stats->totalErrors = 0;
        
        vTaskSetThreadLocalStoragePointer(task, TLS_INDEX_ERROR_COUNTER, stats);
    }
}

// Log an error for the current task
void logTaskError(uint8_t severity) {
    ErrorStats_t *stats = (ErrorStats_t *)pvTaskGetThreadLocalStoragePointer(
        NULL,  // NULL means current task
        TLS_INDEX_ERROR_COUNTER
    );
    
    if (stats != NULL) {
        stats->totalErrors++;
        if (severity > 5) {
            stats->criticalErrors++;
        } else {
            stats->warnings++;
        }
    }
}

// Worker task that tracks its own errors
void vWorkerTask(void *pvParameters) {
    // Initialize TLS for this task
    initTaskErrorTracking(NULL);  // NULL = current task
    
    for (;;) {
        // Simulate some work that might generate errors
        if (performOperation() == false) {
            logTaskError(8);  // Critical error
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Monitoring task that checks error stats
void vMonitorTask(void *pvParameters) {
    TaskHandle_t workerHandle = (TaskHandle_t)pvParameters;
    
    for (;;) {
        ErrorStats_t *stats = (ErrorStats_t *)pvTaskGetThreadLocalStoragePointer(
            workerHandle,
            TLS_INDEX_ERROR_COUNTER
        );
        
        if (stats != NULL) {
            printf("Worker errors: Critical=%lu, Warnings=%lu, Total=%lu\n",
                   stats->criticalErrors, stats->warnings, stats->totalErrors);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

### Example 2: Per-Task Random Number Generator State

Each task maintaining its own PRNG state for thread-safe random number generation:

```c
#define TLS_INDEX_RNG_STATE 1

typedef struct {
    uint32_t seed;
    uint32_t callCount;
} RngState_t;

// Cleanup callback for RNG state
void rngDeleteCallback(int xIndex, void *pvData) {
    if (pvData != NULL) {
        vPortFree(pvData);
    }
}

// Initialize RNG for a task
void initTaskRNG(uint32_t initialSeed) {
    RngState_t *rng = pvPortMalloc(sizeof(RngState_t));
    
    if (rng != NULL) {
        rng->seed = initialSeed;
        rng->callCount = 0;
        
        #if (configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS == 1)
        vTaskSetThreadLocalStoragePointerAndDelCallback(
            NULL,  // Current task
            TLS_INDEX_RNG_STATE,
            rng,
            rngDeleteCallback
        );
        #else
        vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_RNG_STATE, rng);
        #endif
    }
}

// Thread-safe random number generator (simple LCG)
uint32_t taskLocalRandom(void) {
    RngState_t *rng = (RngState_t *)pvTaskGetThreadLocalStoragePointer(
        NULL,
        TLS_INDEX_RNG_STATE
    );
    
    if (rng == NULL) {
        return 0;  // Not initialized
    }
    
    // Linear Congruential Generator
    rng->seed = (1103515245 * rng->seed + 12345) & 0x7FFFFFFF;
    rng->callCount++;
    
    return rng->seed;
}

void vGameTask(void *pvParameters) {
    // Each game task gets its own RNG with unique seed
    uint32_t taskSeed = (uint32_t)pvParameters;
    initTaskRNG(taskSeed);
    
    for (;;) {
        uint32_t randomValue = taskLocalRandom();
        // Use random value for game logic
        processGameEvent(randomValue % 100);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

### Example 3: Per-Task Logging Context

Implementing task-specific logging with automatic task identification:

```c
#define TLS_INDEX_LOG_CONTEXT 2

typedef struct {
    char taskName[16];
    uint32_t messageCount;
    TickType_t lastLogTime;
    uint8_t logLevel;
} LogContext_t;

void logContextDeleteCallback(int xIndex, void *pvData) {
    if (pvData != NULL) {
        LogContext_t *ctx = (LogContext_t *)pvData;
        printf("Task %s logged %lu messages total\n", 
               ctx->taskName, ctx->messageCount);
        vPortFree(pvData);
    }
}

// Initialize logging context for current task
void initTaskLogging(const char *name, uint8_t level) {
    LogContext_t *ctx = pvPortMalloc(sizeof(LogContext_t));
    
    if (ctx != NULL) {
        strncpy(ctx->taskName, name, sizeof(ctx->taskName) - 1);
        ctx->taskName[sizeof(ctx->taskName) - 1] = '\0';
        ctx->messageCount = 0;
        ctx->lastLogTime = 0;
        ctx->logLevel = level;
        
        #if (configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS == 1)
        vTaskSetThreadLocalStoragePointerAndDelCallback(
            NULL,
            TLS_INDEX_LOG_CONTEXT,
            ctx,
            logContextDeleteCallback
        );
        #else
        vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_LOG_CONTEXT, ctx);
        #endif
    }
}

// Task-aware logging function
void taskLog(uint8_t level, const char *format, ...) {
    LogContext_t *ctx = (LogContext_t *)pvTaskGetThreadLocalStoragePointer(
        NULL,
        TLS_INDEX_LOG_CONTEXT
    );
    
    if (ctx == NULL || level < ctx->logLevel) {
        return;  // Not initialized or level too low
    }
    
    TickType_t now = xTaskGetTickCount();
    ctx->messageCount++;
    ctx->lastLogTime = now;
    
    // Print log with task context
    printf("[%lu][%s] ", now, ctx->taskName);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
}

void vSensorTask(void *pvParameters) {
    initTaskLogging("SENSOR", 2);  // Log level 2 and above
    
    for (;;) {
        int sensorValue = readSensor();
        taskLog(3, "Sensor reading: %d", sensorValue);
        
        if (sensorValue > THRESHOLD) {
            taskLog(5, "WARNING: Sensor threshold exceeded!");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vControlTask(void *pvParameters) {
    initTaskLogging("CONTROL", 1);  // Log everything
    
    for (;;) {
        taskLog(2, "Control loop iteration");
        performControlAlgorithm();
        taskLog(1, "Control update complete");
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Example 4: Complete Multi-Index TLS System

Managing multiple TLS indices with a centralized system:

```c
// TLS Index allocation
#define TLS_INDEX_STATS      0
#define TLS_INDEX_RNG        1
#define TLS_INDEX_LOG        2

// Statistics structure
typedef struct {
    uint32_t cyclesExecuted;
    uint32_t memoryAllocated;
    TickType_t totalRunTime;
} TaskStats_t;

// Initialization helper for new tasks
void initAllTaskTLS(const char *taskName, uint32_t rngSeed) {
    // Initialize statistics
    TaskStats_t *stats = pvPortMalloc(sizeof(TaskStats_t));
    if (stats != NULL) {
        memset(stats, 0, sizeof(TaskStats_t));
        vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_STATS, stats);
    }
    
    // Initialize RNG
    RngState_t *rng = pvPortMalloc(sizeof(RngState_t));
    if (rng != NULL) {
        rng->seed = rngSeed;
        rng->callCount = 0;
        vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_RNG, rng);
    }
    
    // Initialize logging
    LogContext_t *log = pvPortMalloc(sizeof(LogContext_t));
    if (log != NULL) {
        strncpy(log->taskName, taskName, 15);
        log->taskName[15] = '\0';
        log->messageCount = 0;
        vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_LOG, log);
    }
}

// Cleanup all TLS for a task (if not using callbacks)
void cleanupAllTaskTLS(TaskHandle_t task) {
    void *ptr;
    
    ptr = pvTaskGetThreadLocalStoragePointer(task, TLS_INDEX_STATS);
    if (ptr) vPortFree(ptr);
    
    ptr = pvTaskGetThreadLocalStoragePointer(task, TLS_INDEX_RNG);
    if (ptr) vPortFree(ptr);
    
    ptr = pvTaskGetThreadLocalStoragePointer(task, TLS_INDEX_LOG);
    if (ptr) vPortFree(ptr);
}

// Update task statistics
void updateTaskStats(uint32_t cycles, uint32_t memory) {
    TaskStats_t *stats = (TaskStats_t *)pvTaskGetThreadLocalStoragePointer(
        NULL,
        TLS_INDEX_STATS
    );
    
    if (stats != NULL) {
        stats->cyclesExecuted += cycles;
        stats->memoryAllocated += memory;
        stats->totalRunTime = xTaskGetTickCount();
    }
}

void vApplicationTask(void *pvParameters) {
    // Initialize all TLS for this task
    initAllTaskTLS("APP_TASK", xTaskGetTickCount());
    
    for (;;) {
        // Use TLS-based features
        uint32_t random = taskLocalRandom();
        taskLog(2, "Processing with random value: %lu", random);
        
        // Track statistics
        updateTaskStats(100, 256);
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Note: In practice, tasks rarely exit, but if they do:
    // cleanupAllTaskTLS(NULL);
    // vTaskDelete(NULL);
}
```

## Use Cases for TLS

### 1. **Thread-Safe Library Integration**
When integrating third-party libraries that use global state, TLS allows each task to maintain separate library contexts without modifying the library code.

### 2. **Performance Profiling**
Store performance metrics per task without the overhead of mutexes or the complexity of passing context pointers through function calls.

### 3. **Error Handling and Recovery**
Maintain task-specific error states, retry counters, and recovery strategies that persist across function calls within the task.

### 4. **Resource Tracking**
Track memory allocations, file handles, or network connections on a per-task basis for debugging and resource management.

### 5. **Security Contexts**
Store authentication tokens, encryption keys, or permission levels unique to each task without exposing them globally.

## Best Practices

### Memory Management

Always free TLS-allocated memory, either through deletion callbacks or manual cleanup:

```c
// With callbacks (recommended)
#if (configTHREAD_LOCAL_STORAGE_DELETE_CALLBACKS == 1)
void myDeleteCallback(int xIndex, void *pvData) {
    if (pvData != NULL) {
        // Perform any necessary cleanup
        vPortFree(pvData);
    }
}
#endif
```

### Index Management

Use defined constants rather than magic numbers:

```c
// Good practice
#define TLS_INDEX_USER_DATA 0
void *data = pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_USER_DATA);

// Avoid this
void *data = pvTaskGetThreadLocalStoragePointer(NULL, 0);
```

### Null Checking

Always verify TLS pointers before use:

```c
MyData_t *data = (MyData_t *)pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_DATA);
if (data == NULL) {
    // Handle uninitialized TLS
    return ERROR_NOT_INITIALIZED;
}
// Safe to use data
```

### Initialization Timing

Initialize TLS early in the task function, typically before entering the main loop:

```c
void vMyTask(void *pvParameters) {
    // Initialize TLS immediately
    initTaskTLS();
    
    // Now enter main loop
    for (;;) {
        // TLS is available throughout
    }
}
```

## Common Pitfalls

1. **Forgetting to allocate memory**: TLS stores pointers, not the data itself
2. **Index conflicts**: Using the same index for different purposes
3. **Memory leaks**: Not freeing TLS memory when tasks are deleted
4. **Race conditions**: Multiple tasks accessing another task's TLS simultaneously
5. **Exceeding configured limit**: Using an index >= `configNUM_THREAD_LOCAL_STORAGE_POINTERS`

## Performance Considerations

- **Fast access**: TLS lookup is very fast (direct array indexing)
- **No synchronization overhead**: No mutexes needed for accessing own task's TLS
- **Memory cost**: Each task allocates an array of pointers (typically 4-8 bytes per index)
- **Zero contention**: Unlike global variables, TLS never causes cache line bouncing between cores

Thread Local Storage in FreeRTOS provides an elegant solution for maintaining per-task state without the complexity of passing context pointers throughout your application or the performance penalty of mutex-protected global variables.