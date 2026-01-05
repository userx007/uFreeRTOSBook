# FreeRTOS Memory Management Schemes

FreeRTOS provides five different heap memory management implementations (heap_1 through heap_5), each designed for specific use cases with different trade-offs. Understanding these schemes is crucial for optimizing your embedded system's memory usage and performance.

## Overview of Memory Management in FreeRTOS

When FreeRTOS creates tasks, queues, semaphores, or other kernel objects, it needs to allocate memory dynamically. The choice of heap implementation affects:

- Memory efficiency and fragmentation
- Determinism and execution speed
- Safety and complexity
- Ability to free allocated memory

## heap_1: The Simplest Implementation

**Characteristics:**
- Memory can only be allocated, never freed
- Extremely simple and deterministic
- No fragmentation possible
- Very small code size

**How it works:**
heap_1 subdivides a single array into smaller blocks as allocation requests are made. It maintains a simple pointer that moves forward with each allocation.

**When to use:**
- Applications that never delete tasks, queues, or other kernel objects after creation
- Safety-critical systems where determinism is paramount
- Systems with very limited code space
- Prototype/demo applications

**Example:**

```c
// Configure heap size in FreeRTOSConfig.h
#define configTOTAL_HEAP_SIZE 10240

void app_main(void)
{
    // Create tasks during initialization
    xTaskCreate(vTask1, "Task1", 200, NULL, 1, NULL);
    xTaskCreate(vTask2, "Task2", 200, NULL, 1, NULL);
    
    // Create queue - will never be deleted
    QueueHandle_t xQueue = xQueueCreate(10, sizeof(int));
    
    // These objects exist for the lifetime of the application
    // heap_1 is perfect for this use case
    
    vTaskStartScheduler();
}
```

## heap_2: Permits Memory Freeing with Best-Fit Algorithm

**Characteristics:**
- Allows memory allocation and deallocation
- Uses best-fit algorithm to minimize wasted space
- Can suffer from fragmentation over time
- Does not coalesce adjacent free blocks
- Deterministic but slower than heap_1

**How it works:**
Maintains a linked list of free memory blocks. When allocating, it searches for the smallest block that fits the request. When freeing, it returns the block to the free list but doesn't merge adjacent blocks.

**When to use:**
- Applications that create and delete tasks repeatedly, but with predictable sizes
- Systems where objects of the same size are allocated/deallocated
- When you need better memory utilization than heap_1 but can tolerate some fragmentation

**Example:**

```c
// Task that creates and deletes worker tasks
void vManagerTask(void *pvParameters)
{
    TaskHandle_t xWorkerHandle;
    
    for(;;)
    {
        // Create a worker task
        xTaskCreate(vWorkerTask, "Worker", 
                   configMINIMAL_STACK_SIZE, 
                   NULL, 2, &xWorkerHandle);
        
        // Let it run for some time
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Delete the worker task - memory is freed
        vTaskDelete(xWorkerHandle);
        
        // With heap_2, this pattern works but may fragment
        // memory if task sizes vary
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

**Fragmentation scenario with heap_2:**

```c
// This pattern can cause fragmentation
void problematic_pattern(void)
{
    // Allocate different sized blocks
    uint8_t *pSmall1 = pvPortMalloc(100);
    uint8_t *pLarge = pvPortMalloc(1000);
    uint8_t *pSmall2 = pvPortMalloc(100);
    
    // Free the large block in the middle
    vPortFree(pLarge);
    
    // Now we have a 1000-byte hole
    // If we allocate many small blocks, that space is wasted
    // because heap_2 doesn't coalesce blocks
}
```

## heap_3: Wrapper Around malloc()/free()

**Characteristics:**
- Simply wraps the compiler's standard malloc() and free()
- Thread-safe (adds mutual exclusion)
- Behavior depends on compiler implementation
- Heap size controlled by linker configuration, not configTOTAL_HEAP_SIZE
- Usually less efficient than other FreeRTOS implementations

**How it works:**
Calls the standard library malloc() and free() but protects them with a critical section to make them thread-safe.

**When to use:**
- Porting existing code that uses malloc()/free()
- When you need to share heap with other libraries
- Development/debugging (can use standard memory analysis tools)

**Example:**

```c
// FreeRTOSConfig.h - configTOTAL_HEAP_SIZE is ignored
// Heap size set in linker script instead

void vTask(void *pvParameters)
{
    // These allocations use the standard library heap
    char *buffer1 = pvPortMalloc(512);
    char *buffer2 = malloc(256);  // Can mix if needed
    
    if(buffer1 != NULL && buffer2 != NULL)
    {
        // Use buffers
        sprintf(buffer1, "FreeRTOS allocation");
        sprintf(buffer2, "Standard allocation");
        
        // Free them
        vPortFree(buffer1);
        free(buffer2);  // Can mix if needed
    }
}
```

## heap_4: Coalescing Best-Fit Algorithm (Most Commonly Used)

**Characteristics:**
- Allows allocation and deallocation
- Coalesces adjacent free blocks to prevent fragmentation
- More complex than heap_2 but much better fragmentation behavior
- Good balance between determinism and efficiency
- **Most popular choice for general-purpose applications**

**How it works:**
Maintains a linked list of free blocks. When freeing memory, it merges adjacent free blocks into larger blocks, significantly reducing fragmentation.

**When to use:**
- Most general-purpose embedded applications
- Systems that frequently create/delete objects of varying sizes
- When you need the ability to free memory without excessive fragmentation
- Default choice unless you have specific requirements

**Example:**

```c
// Dynamic task creation system
typedef struct {
    char taskName[32];
    TaskFunction_t taskCode;
    uint16_t stackSize;
} TaskConfig_t;

void vDynamicTaskManager(void *pvParameters)
{
    TaskHandle_t xTaskHandles[10] = {NULL};
    uint8_t taskCount = 0;
    
    for(;;)
    {
        // Receive task configuration
        TaskConfig_t config;
        if(receive_task_config(&config))
        {
            // Create task with varying stack sizes
            if(xTaskCreate(config.taskCode, 
                          config.taskName,
                          config.stackSize,  // Variable size
                          NULL, 2, 
                          &xTaskHandles[taskCount]) == pdPASS)
            {
                taskCount++;
            }
        }
        
        // Periodically clean up completed tasks
        for(uint8_t i = 0; i < taskCount; i++)
        {
            if(task_is_complete(xTaskHandles[i]))
            {
                vTaskDelete(xTaskHandles[i]);
                // heap_4 coalesces freed memory efficiently
                xTaskHandles[i] = NULL;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Buffer management with varying sizes
void vBufferManager(void)
{
    uint8_t *buffers[5];
    size_t sizes[] = {256, 512, 1024, 128, 768};
    
    // Allocate buffers of different sizes
    for(int i = 0; i < 5; i++)
    {
        buffers[i] = pvPortMalloc(sizes[i]);
    }
    
    // Free in random order
    vPortFree(buffers[2]);  // Free 1024
    vPortFree(buffers[0]);  // Free 256
    vPortFree(buffers[4]);  // Free 768
    
    // heap_4 will coalesce these into larger free blocks
    // making efficient use of memory for future allocations
    
    // Allocate a large buffer - heap_4 can use coalesced space
    uint8_t *largeBuffer = pvPortMalloc(2048);
    
    vPortFree(buffers[1]);
    vPortFree(buffers[3]);
    vPortFree(largeBuffer);
}
```

**Monitoring heap_4 usage:**

```c
void vHeapMonitorTask(void *pvParameters)
{
    size_t freeHeapSize;
    size_t minEverFreeHeap;
    
    for(;;)
    {
        freeHeapSize = xPortGetFreeHeapSize();
        minEverFreeHeap = xPortGetMinimumEverFreeHeapSize();
        
        printf("Current free heap: %u bytes\n", freeHeapSize);
        printf("Minimum ever free: %u bytes\n", minEverFreeHeap);
        printf("High water mark: %u bytes\n", 
               configTOTAL_HEAP_SIZE - minEverFreeHeap);
        
        // Alert if heap usage is concerning
        if(freeHeapSize < 1024)
        {
            printf("WARNING: Low heap!\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

## heap_5: Multiple Non-Contiguous Memory Regions

**Characteristics:**
- Like heap_4 but can manage multiple separate memory regions
- Useful for systems with complex memory architectures
- Can use multiple RAM banks or internal/external memory
- Requires explicit initialization before scheduler starts
- Same coalescing algorithm as heap_4 within each region

**How it works:**
Manages multiple memory regions as separate heaps, each with its own free block list. You define which memory regions to use at runtime.

**When to use:**
- Microcontrollers with multiple RAM banks (internal + external)
- Systems with different memory types (fast RAM, slow RAM)
- When you want to use non-contiguous memory spaces
- Advanced memory management with specific performance requirements

**Example:**

```c
// Define memory regions
#define INTERNAL_RAM_START 0x20000000
#define INTERNAL_RAM_SIZE  (64 * 1024)
#define EXTERNAL_RAM_START 0x60000000
#define EXTERNAL_RAM_SIZE  (512 * 1024)

// Memory region descriptors
const HeapRegion_t xHeapRegions[] = 
{
    // Internal fast RAM - use for critical data
    { (uint8_t *) INTERNAL_RAM_START, INTERNAL_RAM_SIZE },
    
    // External slower RAM - use for bulk data
    { (uint8_t *) EXTERNAL_RAM_START, EXTERNAL_RAM_SIZE },
    
    // Terminator
    { NULL, 0 }
};

int main(void)
{
    // Initialize heap_5 with memory regions
    // Must be called BEFORE any FreeRTOS API calls
    vPortDefineHeapRegions(xHeapRegions);
    
    // Now create tasks, queues, etc.
    xTaskCreate(vCriticalTask, "Critical", 1000, NULL, 3, NULL);
    xTaskCreate(vBulkDataTask, "BulkData", 2000, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    return 0;
}

// Example with strategic allocation
void vMemoryIntensiveTask(void *pvParameters)
{
    // Small, frequently accessed data - likely in fast RAM
    uint8_t *criticalBuffer = pvPortMalloc(256);
    
    // Large data buffer - may be in external RAM
    uint8_t *largeBuffer = pvPortMalloc(100 * 1024);
    
    // FreeRTOS allocates from regions in order
    // First region fills up first
    
    if(criticalBuffer != NULL && largeBuffer != NULL)
    {
        // Use buffers...
        process_data(criticalBuffer, largeBuffer);
        
        vPortFree(criticalBuffer);
        vPortFree(largeBuffer);
    }
}
```

**Advanced heap_5 with specific region targeting:**

```c
// Custom allocator for specific memory regions
// (This requires modifying heap_5.c or creating wrapper functions)

typedef enum {
    MEMORY_FAST,
    MEMORY_SLOW,
    MEMORY_DMA_CAPABLE
} MemoryType_t;

// Wrapper to allocate from specific region (conceptual)
void* pvPortMallocFromRegion(size_t xSize, MemoryType_t type)
{
    // This is a conceptual example - actual implementation
    // would require modifications to heap_5
    
    switch(type)
    {
        case MEMORY_FAST:
            // Allocate from internal RAM
            break;
        case MEMORY_SLOW:
            // Allocate from external RAM
            break;
        case MEMORY_DMA_CAPABLE:
            // Allocate from DMA-accessible region
            break;
    }
    
    return pvPortMalloc(xSize);  // Standard allocation
}
```

## Implementing Custom Memory Allocation Schemes

You can create your own memory allocator by implementing these functions:

```c
void *pvPortMalloc(size_t xWantedSize);
void vPortFree(void *pv);
void vPortInitialiseBlocks(void);
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
```

**Example: Simple Pool Allocator**

```c
// Custom pool allocator for fixed-size blocks
#define POOL_BLOCK_SIZE 128
#define POOL_NUM_BLOCKS 32

typedef struct MemoryBlock {
    struct MemoryBlock *pNext;
    uint8_t data[POOL_BLOCK_SIZE];
} MemoryBlock_t;

static MemoryBlock_t memoryPool[POOL_NUM_BLOCKS];
static MemoryBlock_t *pFreeList = NULL;

void vPortInitialiseBlocks(void)
{
    // Initialize free list
    for(int i = 0; i < POOL_NUM_BLOCKS - 1; i++)
    {
        memoryPool[i].pNext = &memoryPool[i + 1];
    }
    memoryPool[POOL_NUM_BLOCKS - 1].pNext = NULL;
    pFreeList = &memoryPool[0];
}

void *pvPortMalloc(size_t xWantedSize)
{
    MemoryBlock_t *pBlock = NULL;
    
    if(xWantedSize <= POOL_BLOCK_SIZE && pFreeList != NULL)
    {
        taskENTER_CRITICAL();
        {
            pBlock = pFreeList;
            pFreeList = pFreeList->pNext;
        }
        taskEXIT_CRITICAL();
    }
    
    return (pBlock != NULL) ? pBlock->data : NULL;
}

void vPortFree(void *pv)
{
    if(pv != NULL)
    {
        // Calculate block address from data pointer
        MemoryBlock_t *pBlock = (MemoryBlock_t *)
            ((uint8_t *)pv - offsetof(MemoryBlock_t, data));
        
        taskENTER_CRITICAL();
        {
            pBlock->pNext = pFreeList;
            pFreeList = pBlock;
        }
        taskEXIT_CRITICAL();
    }
}

size_t xPortGetFreeHeapSize(void)
{
    size_t count = 0;
    MemoryBlock_t *pBlock = pFreeList;
    
    while(pBlock != NULL)
    {
        count++;
        pBlock = pBlock->pNext;
    }
    
    return count * POOL_BLOCK_SIZE;
}
```

## Comparison Table and Selection Guide

| Scheme | Allocation | Deallocation | Fragmentation | Determinism | Best For |
|--------|-----------|--------------|---------------|-------------|----------|
| heap_1 | Yes | No | None | Highest | Static systems, safety-critical |
| heap_2 | Yes | Yes | Possible | High | Same-size objects |
| heap_3 | Yes | Yes | Depends | Depends | Porting, debugging |
| heap_4 | Yes | Yes | Minimal | Good | General purpose |
| heap_5 | Yes | Yes | Minimal | Good | Complex memory layouts |

## Selection Method
Add exactly one of these files to your project:

heap_1.c <br>
heap_2.c <br>
heap_3.c <br>
heap_4.c <br>
heap_5.c <br>

## Practical Recommendations

**For most applications:** Use **heap_4**. It provides the best balance of features and is well-tested.

**For safety-critical systems:** Use **heap_1** and design your system to never delete objects.

**For systems with variable object lifetimes:** Use **heap_4** with careful monitoring of heap usage.

**For complex hardware with multiple RAM banks:** Use **heap_5** to leverage all available memory.

**For debugging:** Temporarily switch to **heap_3** to use standard memory debugging tools.

Always monitor your heap usage during development using `xPortGetFreeHeapSize()` and `xPortGetMinimumEverFreeHeapSize()` to ensure you've allocated sufficient heap and aren't running into memory exhaustion.

