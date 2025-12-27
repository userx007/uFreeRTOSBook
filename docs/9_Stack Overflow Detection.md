# Stack Overflow Detection in FreeRTOS

Stack overflow is one of the most common and dangerous bugs in embedded systems. When a task uses more stack space than allocated, it corrupts adjacent memory regions, leading to unpredictable behavior, crashes, or silent data corruption. FreeRTOS provides built-in mechanisms to detect stack overflows during development.

## Understanding Stack Growth

Before diving into detection methods, it's crucial to understand how stacks work across different architectures:

**Stack Growth Direction:**
- **Downward growth** (most common): ARM Cortex-M, x86, RISC-V
  - Stack pointer starts at high memory address and decrements
  - Stack "grows down" toward lower addresses
- **Upward growth** (rare): Some legacy architectures
  - Stack pointer starts at low memory address and increments

FreeRTOS abstracts this through the `portSTACK_GROWTH` macro, which is automatically configured for your architecture in the port layer.

**Stack Frame Contents:**
Each task's stack holds:
- Local variables
- Function parameters
- Return addresses
- CPU register context (saved during context switches)
- Interrupt frames (if preempted during execution)

## Configuring Stack Overflow Detection

Stack overflow checking is controlled by `configCHECK_FOR_STACK_OVERFLOW` in `FreeRTOSConfig.h`:

```c
/* FreeRTOSConfig.h */

// Disable checking (production builds)
#define configCHECK_FOR_STACK_OVERFLOW 0

// Enable Method 1
#define configCHECK_FOR_STACK_OVERFLOW 1

// Enable Method 2 (recommended)
#define configCHECK_FOR_STACK_OVERFLOW 2
```

## Method 1: Check on Context Switch

**How it works:**
- Checks if the stack pointer has gone beyond the allocated stack boundaries
- Only checks during task context switches
- Minimal overhead (~10-20 CPU cycles)

**Detection mechanism:**
```c
// Pseudocode of what FreeRTOS does internally
if (pxCurrentTCB->pxTopOfStack < pxCurrentTCB->pxStack)
{
    // Stack overflow detected!
    vApplicationStackOverflowHook(pxCurrentTCB, pcTaskGetName(pxCurrentTCB));
}
```

**Limitations:**
- Only detects overflow at context switch time
- Can miss overflow that occurs and gets "fixed" before next switch
- Won't catch overflow in ISRs or during long-running task execution

**Example scenario where Method 1 fails:**
```c
void vProblematicTask(void *pvParameters)
{
    for(;;)
    {
        // Deep recursion or large local array
        uint8_t largeBuffer[2048];  // Overflows stack
        processData(largeBuffer);   // Uses the buffer
        
        // Buffer goes out of scope, stack pointer "recovers"
        vTaskDelay(pdMS_TO_TICKS(100));  // Context switch happens here
        // Method 1 won't detect the overflow!
    }
}
```

## Method 2: Pattern Checking (Recommended)

**How it works:**
- Writes a known pattern (0xA5A5A5A5...) to the last 16-20 bytes of the stack during task creation
- Checks if this pattern is still intact during context switches
- Catches overflows even if stack pointer recovers

**Detection mechanism:**
```c
// FreeRTOS fills end of stack with pattern during xTaskCreate()
// Then checks periodically:
for (int i = 0; i < 16; i++)
{
    if (pxCurrentTCB->pxStack[i] != STACK_FILL_BYTE)  // Usually 0xA5
    {
        // Pattern corrupted - overflow detected!
        vApplicationStackOverflowHook(pxCurrentTCB, pcTaskGetName(pxCurrentTCB));
    }
}
```

**Advantages:**
- Detects temporary overflows that recover before context switch
- Better coverage with minimal additional overhead
- Industry standard approach

**Limitations:**
- Still only checks at context switch
- Uses ~16 bytes of each task's stack for the pattern
- Won't detect overflow beyond the pattern region

## Implementing the Hook Function

When overflow is detected, FreeRTOS calls `vApplicationStackOverflowHook()`. You **must** implement this function:

```c
/* In your application code */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* This function will be called if a stack overflow is detected.
     * WARNING: This runs in interrupt context - keep it simple! */
    
    // Method 1: Halt and inspect with debugger
    taskDISABLE_INTERRUPTS();
    for(;;)
    {
        // Breakpoint here to inspect xTask and pcTaskName
        __asm volatile ("nop");
    }
    
    // Method 2: Log and attempt recovery (risky!)
    // logError("Stack overflow in task: %s", pcTaskName);
    // vTaskDelete(xTask);  // Delete offending task
    
    // Method 3: Trigger watchdog reset
    // NVIC_SystemReset();
}
```

**Important notes:**
- This hook runs with interrupts disabled
- System is in unstable state - memory may be corrupted
- Best practice: halt system and debug, don't try to continue
- In production, log error and reset the system

## Complete Example: Stack Overflow Detection

```c
/* FreeRTOSConfig.h */
#define configCHECK_FOR_STACK_OVERFLOW 2
#define configMINIMAL_STACK_SIZE 128  // In words, not bytes!

/* main.c */
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

// Task that intentionally causes stack overflow
void vBadStackTask(void *pvParameters)
{
    uint32_t counter = 0;
    
    for(;;)
    {
        // Simulate deep call stack or large local variables
        uint8_t wasteStack[512];  // Large local array
        
        // Use the array so compiler doesn't optimize it away
        memset(wasteStack, counter++, sizeof(wasteStack));
        
        printf("Bad task iteration: %lu\n", counter);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Well-behaved task with proper stack usage
void vGoodTask(void *pvParameters)
{
    uint32_t counter = 0;
    
    for(;;)
    {
        printf("Good task iteration: %lu\n", counter++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Required stack overflow hook
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("\n!!! STACK OVERFLOW DETECTED !!!\n");
    printf("Task: %s\n", pcTaskName);
    printf("Task handle: %p\n", (void*)xTask);
    
    taskDISABLE_INTERRUPTS();
    while(1)
    {
        // Halt here - inspect variables in debugger
    }
}

int main(void)
{
    // Create task with insufficient stack
    xTaskCreate(vBadStackTask,
                "BadStack",
                64,  // Only 64 words (256 bytes) - too small!
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
    
    // Create task with adequate stack
    xTaskCreate(vGoodTask,
                "GoodTask",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                NULL);
    
    vTaskStartScheduler();
    
    // Should never reach here
    for(;;);
}
```

## Debugging Stack Issues

### 1. Determining Actual Stack Usage

Use `uxTaskGetStackHighWaterMark()` to measure how much stack headroom remains:

```c
void vMonitorTask(void *pvParameters)
{
    TaskHandle_t xTaskToMonitor = (TaskHandle_t)pvParameters;
    
    for(;;)
    {
        UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(xTaskToMonitor);
        
        // Returns minimum free stack space (in words)
        printf("Task stack headroom: %u words (%u bytes)\n",
               uxHighWaterMark,
               uxHighWaterMark * sizeof(StackType_t));
        
        if (uxHighWaterMark < 32)  // Less than 32 words free
        {
            printf("WARNING: Stack running low!\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

### 2. Runtime Stack Analysis

```c
void vAnalyzeAllTasks(void)
{
    TaskStatus_t *pxTaskStatusArray;
    UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    
    if (pxTaskStatusArray != NULL)
    {
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);
        
        printf("\nTask Stack Analysis:\n");
        printf("%-16s | Stack Base | Current SP | Free Space\n", "Task Name");
        printf("--------------------------------------------------------\n");
        
        for (UBaseType_t i = 0; i < uxArraySize; i++)
        {
            printf("%-16s | %10p | %10p | %4u words\n",
                   pxTaskStatusArray[i].pcTaskName,
                   pxTaskStatusArray[i].pxStackBase,
                   pxTaskStatusArray[i].pxTopOfStack,
                   pxTaskStatusArray[i].usStackHighWaterMark);
        }
        
        vPortFree(pxTaskStatusArray);
    }
}
```

### 3. Common Stack Issues and Solutions

**Problem: Overflow on first task switch**
```c
// Wrong - stack too small for context save
xTaskCreate(vTask, "Task", 32, NULL, 1, NULL);  // Only 32 words!

// Fixed - account for context size (typically 64-128 words minimum)
xTaskCreate(vTask, "Task", 256, NULL, 1, NULL);
```

**Problem: Large local variables**
```c
// Bad - allocates 4KB on stack
void vTask(void *pvParameters)
{
    uint8_t buffer[4096];  // Stack overflow likely!
    processBuffer(buffer);
}

// Fixed - use dynamic allocation or static storage
void vTask(void *pvParameters)
{
    uint8_t *buffer = pvPortMalloc(4096);
    if (buffer != NULL)
    {
        processBuffer(buffer);
        vPortFree(buffer);
    }
}

// Or use static storage
void vTask(void *pvParameters)
{
    static uint8_t buffer[4096];  // Stored in .bss, not on stack
    processBuffer(buffer);
}
```

**Problem: Deep recursion**
```c
// Dangerous - each recursive call adds stack frame
uint32_t factorial(uint32_t n)
{
    if (n <= 1) return 1;
    return n * factorial(n - 1);  // Can overflow with large n
}

// Better - use iteration
uint32_t factorial(uint32_t n)
{
    uint32_t result = 1;
    for (uint32_t i = 2; i <= n; i++)
    {
        result *= i;
    }
    return result;
}
```

## Best Practices

1. **Always enable Method 2 during development** - disable only in final production builds for performance
2. **Add generous margin** - allocate 25-50% more stack than measured high-water mark
3. **Monitor in testing** - use `uxTaskGetStackHighWaterMark()` during stress tests
4. **Avoid large local variables** - use heap allocation or static storage
5. **Watch for library functions** - printf, sprintf can use hundreds of bytes
6. **Account for interrupt nesting** - ISRs use task stacks on Cortex-M
7. **Test worst-case paths** - deepest call chains and largest variable combinations

Stack overflow detection is critical during development but has runtime overhead. The small performance cost of Method 2 is worthwhile insurance against catastrophic system failures.