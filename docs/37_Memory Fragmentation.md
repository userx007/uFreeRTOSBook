# Memory Fragmentation in FreeRTOS

Memory fragmentation is a critical issue in embedded systems using FreeRTOS, particularly when dynamic memory allocation is employed. Understanding fragmentation patterns, their causes, and mitigation strategies is essential for building robust real-time applications.

## What is Memory Fragmentation?

Memory fragmentation occurs when free memory becomes divided into small, non-contiguous blocks, making it difficult or impossible to allocate larger memory chunks even though the total free memory appears sufficient. In FreeRTOS, this primarily affects heap memory used for tasks, queues, semaphores, and other kernel objects.

There are two types of fragmentation:

**External Fragmentation**: Free memory exists but is scattered in small blocks that cannot satisfy larger allocation requests.

**Internal Fragmentation**: Allocated memory blocks contain unused space due to alignment requirements or allocation granularity.

## Heap Implementations and Fragmentation

FreeRTOS provides five heap implementations (heap_1 through heap_5), but heap_2 and heap_4 are most susceptible to fragmentation issues because they allow memory to be freed.

### heap_2: Best-Fit Algorithm with Fragmentation Risk

Heap_2 uses a best-fit algorithm and allows memory to be freed, but **does not coalesce adjacent free blocks**. This makes it highly prone to fragmentation.

**Characteristics:**
- Finds the smallest free block that fits the allocation request
- Does not merge adjacent free blocks when memory is freed
- Deterministic allocation time
- Suitable for applications with fixed-size allocations

**Fragmentation scenario with heap_2:**

```c
// Example demonstrating heap_2 fragmentation
void demonstrate_heap2_fragmentation(void)
{
    // Allocate three 100-byte blocks
    void *block1 = pvPortMalloc(100);
    void *block2 = pvPortMalloc(100);
    void *block3 = pvPortMalloc(100);
    
    // Free the middle block
    vPortFree(block2);
    
    // Now we have: [block1:100][FREE:100][block3:100]
    
    // Free block1
    vPortFree(block1);
    
    // Now we have: [FREE:100][FREE:100][block3:100]
    // But these two free blocks are NOT coalesced!
    // They remain as two separate 100-byte blocks
    
    // This will FAIL even though 200 bytes are free
    void *large_block = pvPortMalloc(150);
    
    if (large_block == NULL) {
        // Allocation failed due to fragmentation
        printf("Fragmentation: Cannot allocate 150 bytes from 200 free bytes\n");
    }
    
    vPortFree(block3);
}
```

### heap_4: First-Fit with Coalescing

Heap_4 addresses the fragmentation problem by **coalescing adjacent free blocks** when memory is freed. This significantly reduces external fragmentation.

**Characteristics:**
- Uses first-fit algorithm
- Automatically merges adjacent free blocks
- More complex allocation logic
- Better fragmentation resistance
- Recommended for most applications requiring dynamic allocation

**Example showing heap_4's advantage:**

```c
void demonstrate_heap4_coalescing(void)
{
    // Same allocation pattern as before
    void *block1 = pvPortMalloc(100);
    void *block2 = pvPortMalloc(100);
    void *block3 = pvPortMalloc(100);
    
    vPortFree(block2);  // [block1:100][FREE:100][block3:100]
    vPortFree(block1);  // heap_4 coalesces: [FREE:200][block3:100]
    
    // This will SUCCEED because adjacent blocks were merged
    void *large_block = pvPortMalloc(150);
    
    if (large_block != NULL) {
        printf("Success: heap_4 coalesced blocks\n");
    }
    
    vPortFree(large_block);
    vPortFree(block3);
}
```

## Strategies to Minimize Fragmentation

### 1. Static Allocation Wherever Possible

The best way to avoid fragmentation is to minimize dynamic allocation:

```c
// Instead of dynamic allocation
TaskHandle_t xTaskHandle;
xTaskCreate(vTaskCode, "Task", 1000, NULL, 1, &xTaskHandle);

// Use static allocation
StaticTask_t xTaskBuffer;
StackType_t xStack[1000];

TaskHandle_t xTaskHandle = xTaskCreateStatic(
    vTaskCode,
    "Task",
    1000,
    NULL,
    1,
    xStack,
    &xTaskBuffer
);
```

### 2. Allocate During Initialization Only

Allocate all memory during system initialization before the scheduler starts:

```c
// Global handles
QueueHandle_t xQueue1, xQueue2;
SemaphoreHandle_t xMutex;
TaskHandle_t xTask1, xTask2;

void system_init(void)
{
    // All allocations happen here, before scheduler starts
    xQueue1 = xQueueCreate(10, sizeof(uint32_t));
    xQueue2 = xQueueCreate(5, sizeof(DataStruct_t));
    xMutex = xSemaphoreCreateMutex();
    
    xTaskCreate(vTask1, "Task1", 500, NULL, 2, &xTask1);
    xTaskCreate(vTask2, "Task2", 500, NULL, 2, &xTask2);
    
    // Start scheduler - no more allocations after this
    vTaskStartScheduler();
}
```

### 3. Use Fixed-Size Memory Pools

Implement memory pools for frequently allocated/deallocated objects:

```c
#define POOL_SIZE 10
#define BUFFER_SIZE 256

typedef struct {
    uint8_t data[BUFFER_SIZE];
    bool in_use;
} PoolBuffer_t;

static PoolBuffer_t buffer_pool[POOL_SIZE];
static SemaphoreHandle_t pool_mutex;

void init_buffer_pool(void)
{
    pool_mutex = xSemaphoreCreateMutex();
    
    for (int i = 0; i < POOL_SIZE; i++) {
        buffer_pool[i].in_use = false;
    }
}

PoolBuffer_t* allocate_from_pool(void)
{
    PoolBuffer_t *buffer = NULL;
    
    xSemaphoreTake(pool_mutex, portMAX_DELAY);
    
    for (int i = 0; i < POOL_SIZE; i++) {
        if (!buffer_pool[i].in_use) {
            buffer_pool[i].in_use = true;
            buffer = &buffer_pool[i];
            break;
        }
    }
    
    xSemaphoreGive(pool_mutex);
    return buffer;
}

void free_to_pool(PoolBuffer_t *buffer)
{
    if (buffer >= buffer_pool && buffer < buffer_pool + POOL_SIZE) {
        xSemaphoreTake(pool_mutex, portMAX_DELAY);
        buffer->in_use = false;
        xSemaphoreGive(pool_mutex);
    }
}
```

### 4. Allocate Similar-Sized Objects Together

Group allocations by size to reduce fragmentation:

```c
void smart_initialization(void)
{
    // Allocate all small objects first
    SemaphoreHandle_t sem1 = xSemaphoreCreateBinary();
    SemaphoreHandle_t sem2 = xSemaphoreCreateBinary();
    SemaphoreHandle_t sem3 = xSemaphoreCreateBinary();
    
    // Then medium-sized objects
    QueueHandle_t queue1 = xQueueCreate(10, sizeof(uint32_t));
    QueueHandle_t queue2 = xQueueCreate(10, sizeof(uint32_t));
    
    // Finally large objects
    TaskHandle_t task1, task2, task3;
    xTaskCreate(vTask1, "T1", 1000, NULL, 1, &task1);
    xTaskCreate(vTask2, "T2", 1000, NULL, 1, &task2);
    xTaskCreate(vTask3, "T3", 1000, NULL, 1, &task3);
}
```

### 5. Avoid Frequent Allocation/Deallocation Cycles

```c
// BAD: Frequent allocation/deallocation
void bad_task(void *params)
{
    while (1) {
        char *buffer = pvPortMalloc(256);
        process_data(buffer);
        vPortFree(buffer);  // Fragmenting the heap
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// GOOD: Allocate once, reuse
void good_task(void *params)
{
    char *buffer = pvPortMalloc(256);
    
    if (buffer == NULL) {
        vTaskDelete(NULL);  // Handle allocation failure
        return;
    }
    
    while (1) {
        process_data(buffer);  // Reuse the same buffer
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Monitoring Heap Usage

### 1. Getting Free Heap Size

```c
void monitor_heap_basic(void)
{
    size_t free_heap = xPortGetFreeHeapSize();
    printf("Current free heap: %u bytes\n", free_heap);
}
```

### 2. Getting Minimum Ever Free Heap

This shows the worst-case heap usage:

```c
void monitor_heap_watermark(void)
{
    size_t min_free_heap = xPortGetMinimumEverFreeHeapSize();
    printf("Minimum free heap ever: %u bytes\n", min_free_heap);
    
    // Calculate peak usage
    size_t total_heap = configTOTAL_HEAP_SIZE;
    size_t peak_usage = total_heap - min_free_heap;
    printf("Peak heap usage: %u bytes (%.1f%%)\n", 
           peak_usage, 
           (peak_usage * 100.0) / total_heap);
}
```

### 3. Comprehensive Heap Statistics (heap_4 and heap_5)

```c
void monitor_heap_detailed(void)
{
    HeapStats_t heap_stats;
    vPortGetHeapStats(&heap_stats);
    
    printf("=== Heap Statistics ===\n");
    printf("Available heap space: %u bytes\n", 
           heap_stats.xAvailableHeapSpaceInBytes);
    printf("Largest free block: %u bytes\n", 
           heap_stats.xSizeOfLargestFreeBlockInBytes);
    printf("Smallest free block: %u bytes\n", 
           heap_stats.xSizeOfSmallestFreeBlockInBytes);
    printf("Number of free blocks: %u\n", 
           heap_stats.xNumberOfFreeBlocks);
    printf("Minimum ever free: %u bytes\n", 
           heap_stats.xMinimumEverFreeBytesRemaining);
    printf("Successful allocations: %u\n", 
           heap_stats.xNumberOfSuccessfulAllocations);
    printf("Successful frees: %u\n", 
           heap_stats.xNumberOfSuccessfulFrees);
}
```

### 4. Fragmentation Detection

```c
bool check_fragmentation(void)
{
    HeapStats_t stats;
    vPortGetHeapStats(&stats);
    
    size_t total_free = stats.xAvailableHeapSpaceInBytes;
    size_t largest_block = stats.xSizeOfLargestFreeBlockInBytes;
    size_t num_blocks = stats.xNumberOfFreeBlocks;
    
    printf("Total free: %u, Largest block: %u, Blocks: %u\n",
           total_free, largest_block, num_blocks);
    
    // If largest block is much smaller than total free memory,
    // we have significant fragmentation
    float fragmentation_ratio = (float)largest_block / total_free;
    
    if (fragmentation_ratio < 0.5 && num_blocks > 5) {
        printf("WARNING: High fragmentation detected!\n");
        printf("Fragmentation ratio: %.2f\n", fragmentation_ratio);
        return true;
    }
    
    return false;
}
```

### 5. Periodic Heap Monitoring Task

```c
void vHeapMonitorTask(void *params)
{
    const TickType_t monitoring_period = pdMS_TO_TICKS(5000);
    HeapStats_t stats;
    
    while (1) {
        vPortGetHeapStats(&stats);
        
        // Log to console or save to diagnostics buffer
        printf("\n[%lu] Heap Status:\n", xTaskGetTickCount());
        printf("  Free: %u bytes\n", stats.xAvailableHeapSpaceInBytes);
        printf("  Largest block: %u bytes\n", 
               stats.xSizeOfLargestFreeBlockInBytes);
        printf("  Free blocks: %u\n", stats.xNumberOfFreeBlocks);
        
        // Check for fragmentation
        if (stats.xNumberOfFreeBlocks > 10) {
            printf("  WARNING: Possible fragmentation\n");
        }
        
        // Check for low memory
        if (stats.xAvailableHeapSpaceInBytes < 1024) {
            printf("  CRITICAL: Low memory!\n");
        }
        
        vTaskDelay(monitoring_period);
    }
}
```

### 6. Allocation Failure Handling

```c
void safe_allocation_example(void)
{
    const size_t buffer_size = 1024;
    void *buffer = pvPortMalloc(buffer_size);
    
    if (buffer == NULL) {
        // Log the failure with heap diagnostics
        printf("ERROR: Failed to allocate %u bytes\n", buffer_size);
        
        HeapStats_t stats;
        vPortGetHeapStats(&stats);
        printf("Available: %u, Largest block: %u\n",
               stats.xAvailableHeapSpaceInBytes,
               stats.xSizeOfLargestFreeBlockInBytes);
        
        // Take corrective action
        // - Delete non-critical tasks
        // - Free cached data
        // - Reset system if critical
        return;
    }
    
    // Use buffer...
    
    vPortFree(buffer);
}
```

## Complete Example: Fragmentation-Aware System

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

// Configuration
#define ENABLE_HEAP_MONITORING 1
#define FRAGMENTATION_THRESHOLD 0.6

// Memory statistics structure
typedef struct {
    uint32_t allocation_count;
    uint32_t deallocation_count;
    uint32_t failed_allocations;
    size_t peak_usage;
} MemoryStats_t;

static MemoryStats_t memory_stats = {0};
static SemaphoreHandle_t stats_mutex;

// Wrapper functions to track allocations
void* tracked_malloc(size_t size)
{
    void *ptr = pvPortMalloc(size);
    
    xSemaphoreTake(stats_mutex, portMAX_DELAY);
    
    if (ptr != NULL) {
        memory_stats.allocation_count++;
        
        // Update peak usage
        size_t current_used = configTOTAL_HEAP_SIZE - xPortGetFreeHeapSize();
        if (current_used > memory_stats.peak_usage) {
            memory_stats.peak_usage = current_used;
        }
    } else {
        memory_stats.failed_allocations++;
        printf("MALLOC FAILED: %u bytes\n", size);
    }
    
    xSemaphoreGive(stats_mutex);
    return ptr;
}

void tracked_free(void *ptr)
{
    if (ptr != NULL) {
        vPortFree(ptr);
        
        xSemaphoreTake(stats_mutex, portMAX_DELAY);
        memory_stats.deallocation_count++;
        xSemaphoreGive(stats_mutex);
    }
}

// Monitoring task
void vMemoryMonitorTask(void *params)
{
    while (1) {
        HeapStats_t heap_stats;
        vPortGetHeapStats(&heap_stats);
        
        float frag_ratio = (float)heap_stats.xSizeOfLargestFreeBlockInBytes / 
                          heap_stats.xAvailableHeapSpaceInBytes;
        
        printf("\n=== Memory Report ===\n");
        printf("Allocations: %lu, Frees: %lu, Failed: %lu\n",
               memory_stats.allocation_count,
               memory_stats.deallocation_count,
               memory_stats.failed_allocations);
        printf("Peak usage: %u bytes\n", memory_stats.peak_usage);
        printf("Current free: %u bytes\n", heap_stats.xAvailableHeapSpaceInBytes);
        printf("Largest block: %u bytes\n", heap_stats.xSizeOfLargestFreeBlockInBytes);
        printf("Fragmentation ratio: %.2f\n", frag_ratio);
        
        if (frag_ratio < FRAGMENTATION_THRESHOLD) {
            printf("WARNING: Fragmentation detected!\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

int main(void)
{
    stats_mutex = xSemaphoreCreateMutex();
    
    // Create monitoring task
    xTaskCreate(vMemoryMonitorTask, "MemMon", 512, NULL, 1, NULL);
    
    // Create application tasks...
    
    vTaskStartScheduler();
    return 0;
}
```

## Best Practices Summary

1. **Choose heap_4** for most applications requiring dynamic memory management
2. **Allocate during initialization** and avoid runtime allocation when possible
3. **Use static allocation** for predictable memory usage
4. **Implement memory pools** for frequently used objects
5. **Monitor heap statistics** regularly to detect issues early
6. **Size your heap appropriately** with margin for peak usage
7. **Test worst-case scenarios** during development
8. **Handle allocation failures** gracefully with fallback strategies

By following these strategies and actively monitoring heap usage, you can build robust FreeRTOS applications that avoid fragmentation-related failures and maintain predictable real-time behavior.