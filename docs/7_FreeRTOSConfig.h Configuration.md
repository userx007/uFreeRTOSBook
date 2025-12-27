# FreeRTOS Configuration: Mastering FreeRTOSConfig.h

## Overview

`FreeRTOSConfig.h` is the central configuration file for FreeRTOS that allows you to customize the RTOS behavior for your specific application and hardware platform. This file controls everything from basic timing parameters to advanced features, memory management, and debugging capabilities. Proper configuration is crucial for optimal performance, memory usage, and feature availability.

## Core Configuration Categories

### 1. **Basic System Configuration**

#### Clock and Tick Configuration

```c
// System clock frequency (Hz) - must match your MCU clock
#define configCPU_CLOCK_HZ              ( ( unsigned long ) 72000000 )

// RTOS tick rate (Hz) - how often the tick interrupt fires
#define configTICK_RATE_HZ              ( ( TickType_t ) 1000 )  // 1ms tick

// Maximum number of ticks to suppress in tickless idle mode
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP   2
```

**Example Application:**
```c
// With configTICK_RATE_HZ = 1000 (1ms tick)
vTaskDelay(100);  // Delays for 100 ticks = 100ms

// With configTICK_RATE_HZ = 100 (10ms tick)
vTaskDelay(100);  // Delays for 100 ticks = 1000ms
```

#### Task Priority Levels

```c
// Maximum number of priority levels (0 to configMAX_PRIORITIES - 1)
#define configMAX_PRIORITIES            ( 7 )

// Idle task priority is always 0
// Priority 6 would be highest with this setting
```

**Example Priority Scheme:**
```c
#define PRIORITY_IDLE       0  // Idle task (automatic)
#define PRIORITY_LOW        1  // Background tasks
#define PRIORITY_NORMAL     3  // Regular application tasks
#define PRIORITY_HIGH       5  // Time-critical tasks
#define PRIORITY_REALTIME   6  // ISR-like urgency
```

### 2. **Memory Management Configuration**

#### Heap Size and Allocation Scheme

```c
// Total heap size for FreeRTOS dynamic allocation
#define configTOTAL_HEAP_SIZE           ( ( size_t ) ( 20 * 1024 ) )  // 20KB

// Memory allocation scheme selection (heap_1.c through heap_5.c)
// This is selected by including the appropriate heap_x.c in your project
```

**Memory Allocation Schemes Comparison:**

```c
/* heap_1.c - Simplest, no deallocation
 * - Use when: Tasks created once at startup, no dynamic deletion
 * - Advantages: Fast, deterministic, no fragmentation
 * - Disadvantages: Cannot free memory
 */

/* heap_2.c - Allows free, but fragmentation possible
 * - Use when: Frequent create/delete of same-sized blocks
 * - Advantages: Simple, reasonably fast
 * - Disadvantages: Can fragment, best-fit algorithm
 */

/* heap_3.c - Wraps malloc/free
 * - Use when: Need thread-safe malloc/free
 * - Advantages: Uses standard library, no FreeRTOS heap limit
 * - Disadvantages: Not deterministic, depends on stdlib implementation
 */

/* heap_4.c - Coalescence algorithm (RECOMMENDED)
 * - Use when: General purpose with create/delete operations
 * - Advantages: Combines adjacent free blocks, deterministic
 * - Disadvantages: Slightly more complex
 */

/* heap_5.c - Like heap_4 but spans multiple memory regions
 * - Use when: Non-contiguous memory regions (internal + external RAM)
 * - Advantages: Full control over memory layout
 * - Disadvantages: Requires explicit initialization
 */
```

**Example: heap_5 Configuration for Multiple Memory Regions**

```c
#define configAPPLICATION_ALLOCATED_HEAP    1

// In your initialization code:
void vApplicationInit(void)
{
    HeapRegion_t xHeapRegions[] =
    {
        // Internal SRAM - fast but limited
        { ( uint8_t * ) 0x20000000UL, 0x8000 },    // 32KB
        
        // External SDRAM - slower but large
        { ( uint8_t * ) 0xC0000000UL, 0x80000 },   // 512KB
        
        // Terminator
        { NULL, 0 }
    };
    
    vPortDefineHeapRegions( xHeapRegions );
}
```

#### Stack Overflow Detection

```c
// Stack overflow checking method
#define configCHECK_FOR_STACK_OVERFLOW      2

/* Method 1: Check stack pointer still within valid range
 * Method 2: Also check for stack pattern corruption (more thorough)
 */
```

**Example Stack Overflow Handler:**
```c
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName )
{
    // Called if stack overflow detected
    printf("Stack overflow in task: %s\n", pcTaskName);
    
    // Log to flash memory or trigger safe shutdown
    for(;;)
    {
        // Halt system - don't allow corrupted stack to continue
    }
}
```

### 3. **Task and Scheduler Configuration**

```c
// Minimum stack size (in words, not bytes!)
#define configMINIMAL_STACK_SIZE        ( ( unsigned short ) 128 )

// Maximum task name length
#define configMAX_TASK_NAME_LEN         ( 16 )

// Use preemption (vs cooperative scheduling)
#define configUSE_PREEMPTION            1

// Use time slicing for equal-priority tasks
#define configUSE_TIME_SLICING          1

// Idle task should yield to other idle-priority tasks
#define configIDLE_SHOULD_YIELD         1
```

**Example Demonstrating Time Slicing:**

```c
// With configUSE_TIME_SLICING = 1
void vTask1(void *pvParameters)
{
    while(1)
    {
        printf("Task 1 running\n");
        // No delay - will run for one tick then yield to Task 2
    }
}

void vTask2(void *pvParameters)
{
    while(1)
    {
        printf("Task 2 running\n");
        // No delay - will run for one tick then yield to Task 1
    }
}

// Both tasks at same priority will alternate each tick period
xTaskCreate(vTask1, "Task1", 200, NULL, 2, NULL);
xTaskCreate(vTask2, "Task2", 200, NULL, 2, NULL);
```

### 4. **Feature Enable/Disable Flags**

```c
// Mutex support
#define configUSE_MUTEXES                   1

// Recursive mutexes
#define configUSE_RECURSIVE_MUTEXES         1

// Counting semaphores
#define configUSE_COUNTING_SEMAPHORES       1

// Software timers
#define configUSE_TIMERS                    1
#define configTIMER_TASK_PRIORITY           ( 5 )
#define configTIMER_QUEUE_LENGTH            ( 10 )
#define configTIMER_TASK_STACK_DEPTH        ( configMINIMAL_STACK_SIZE * 2 )

// Task notifications (lightweight alternative to semaphores/queues)
#define configUSE_TASK_NOTIFICATIONS        1

// Direct-to-task notifications
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3

// Queue sets (for blocking on multiple queues)
#define configUSE_QUEUE_SETS                1

// Event groups
#define configUSE_EVENT_GROUPS              1

// Stream buffers
#define configUSE_STREAM_BUFFERS            1

// Message buffers
#define configUSE_MESSAGE_BUFFERS           1
```

**Example Using Conditional Features:**

```c
#if configUSE_TIMERS == 1
    // Software timer example
    TimerHandle_t xLEDTimer;
    
    void vLEDTimerCallback(TimerHandle_t xTimer)
    {
        static uint8_t ledState = 0;
        ledState = !ledState;
        GPIO_WritePin(LED_PIN, ledState);
    }
    
    void vSetupLEDTimer(void)
    {
        xLEDTimer = xTimerCreate(
            "LED",                      // Name
            pdMS_TO_TICKS(500),        // Period: 500ms
            pdTRUE,                     // Auto-reload
            0,                          // Timer ID
            vLEDTimerCallback           // Callback
        );
        
        if(xLEDTimer != NULL)
            xTimerStart(xLEDTimer, 0);
    }
#endif
```

### 5. **Runtime Statistics and Debugging**

```c
// Include task names in TCB
#define configUSE_TRACE_FACILITY            1

// Generate runtime statistics
#define configGENERATE_RUN_TIME_STATS       1

// Provide functions for runtime stats
extern void vConfigureTimerForRunTimeStats(void);
extern unsigned long ulGetRunTimeCounterValue(void);

#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()    vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()            ulGetRunTimeCounterValue()

// Include additional task information
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

// Record the highest stack usage
#define configRECORD_STACK_HIGH_ADDRESS         1
```

**Example Runtime Statistics Implementation:**

```c
// Use a high-resolution timer (e.g., 10x tick rate)
static uint32_t ulHighResolutionTimer = 0;

void vConfigureTimerForRunTimeStats(void)
{
    // Setup hardware timer to increment ulHighResolutionTimer
    // Should be 10-20x faster than tick rate for accuracy
    TIM_Init(TIM2, 72000000 / 10000);  // 10kHz if tick is 1kHz
}

unsigned long ulGetRunTimeCounterValue(void)
{
    return ulHighResolutionTimer;
}

void TIM2_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM2, TIM_IT_Update))
    {
        ulHighResolutionTimer++;
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}

// Usage in application:
void vPrintTaskStats(void)
{
    char statsBuffer[512];
    vTaskGetRunTimeStats(statsBuffer);
    printf("Task Statistics:\n%s", statsBuffer);
    
    /* Output example:
     * Task            Abs Time        % Time
     * ************************************************
     * IDLE            123456789       89%
     * SensorTask      12345678        9%
     * DisplayTask     2345678         2%
     */
}
```

### 6. **Memory and Task Tracking**

```c
// Track number of malloc calls that failed
#define configUSE_MALLOC_FAILED_HOOK        1

// Track tasks in running state
#define configUSE_APPLICATION_TASK_TAG      1

// Include query functions
#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_xTaskResumeFromISR          1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1
#define INCLUDE_xTaskGetSchedulerState      1
#define INCLUDE_xTaskGetCurrentTaskHandle   1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetIdleTaskHandle      1
#define INCLUDE_eTaskGetState               1
```

**Example Malloc Failed Hook:**

```c
void vApplicationMallocFailedHook(void)
{
    // Called when pvPortMalloc() returns NULL
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    
    // Get task count
    uxArraySize = uxTaskGetNumberOfTasks();
    
    // Allocate array from remaining stack (not heap)
    pxTaskStatusArray = pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
    
    if(pxTaskStatusArray != NULL)
    {
        // Get task information
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, 
                                           uxArraySize, 
                                           NULL);
        
        printf("Memory allocation failed! Current tasks:\n");
        for(x = 0; x < uxArraySize; x++)
        {
            printf("Task: %s, Stack HWM: %u\n",
                   pxTaskStatusArray[x].pcTaskName,
                   pxTaskStatusArray[x].usStackHighWaterMark);
        }
        
        vPortFree(pxTaskStatusArray);
    }
    
    // Take corrective action or halt
    for(;;);
}
```

### 7. **Interrupt Configuration**

```c
// Interrupt priority configuration (Cortex-M specific)
#define configPRIO_BITS                     4  // 16 priority levels
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

// Kernel interrupt priorities
#define configKERNEL_INTERRUPT_PRIORITY     ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
```

**Critical Interrupt Priority Concept:**

```c
/*
 * Priority Levels (lower number = higher priority):
 * 0-4:   HIGH PRIORITY - Cannot call FreeRTOS API functions
 * 5-15:  LOWER PRIORITY - Can call FromISR() API functions
 * 
 * configMAX_SYSCALL_INTERRUPT_PRIORITY (5) is the threshold
 */

// HIGH priority ISR - cannot use FreeRTOS APIs
void EXTI0_IRQHandler(void)  // Priority 3
{
    // Time-critical processing only
    // NO xQueueSendFromISR, xSemaphoreGiveFromISR, etc.
    
    ProcessCriticalData();
    EXTI_ClearITPendingBit(EXTI_Line0);
}

// LOWER priority ISR - can use FreeRTOS APIs
void USART1_IRQHandler(void)  // Priority 7
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t data = USART_ReceiveData(USART1);
    
    // Safe to call FromISR functions
    xQueueSendFromISR(xUARTQueue, &data, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### 8. **Advanced Configuration Options**

```c
// Use tickless idle mode for low power
#define configUSE_TICKLESS_IDLE             1

// Use 16-bit tick type (saves RAM but limits delay times)
#define configUSE_16_BIT_TICKS              0  // Use 32-bit for flexibility

// Support static allocation (no heap needed)
#define configSUPPORT_STATIC_ALLOCATION     1

// Support dynamic allocation
#define configSUPPORT_DYNAMIC_ALLOCATION    1

// Use daemon task startup hook
#define configUSE_DAEMON_TASK_STARTUP_HOOK  1

// Check task return address
#define configCHECK_FOR_STACK_OVERFLOW      2

// Include application-defined privileged functions (MPU)
#define configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS 0
```

**Example Static Allocation:**

```c
#if configSUPPORT_STATIC_ALLOCATION == 1

// Static task creation - no heap usage
static StaticTask_t xTaskBuffer;
static StackType_t xStack[200];

void vCreateStaticTask(void)
{
    TaskHandle_t xHandle = NULL;
    
    xHandle = xTaskCreateStatic(
        vTaskCode,          // Task function
        "StaticTask",       // Name
        200,                // Stack size (words)
        NULL,               // Parameters
        2,                  // Priority
        xStack,             // Stack buffer
        &xTaskBuffer        // Task control block
    );
}

// Provide idle task memory (required with static allocation)
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

#if configUSE_TIMERS == 1
// Provide timer task memory (required with timers + static allocation)
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
#endif

#endif
```

## Complete Example Configuration

Here's a production-ready `FreeRTOSConfig.h` for an ARM Cortex-M4 application:

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions
 *-----------------------------------------------------------*/

// System Configuration
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 168000000 )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                    ( 7 )
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 130 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) ( 40 * 1024 ) )
#define configMAX_TASK_NAME_LEN                 ( 16 )
#define configUSE_TRACE_FACILITY                1
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configQUEUE_REGISTRY_SIZE               8
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_APPLICATION_TASK_TAG          0
#define configUSE_COUNTING_SEMAPHORES           1
#define configGENERATE_RUN_TIME_STATS           1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

// Software Timer Configuration
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               ( 6 )
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            ( configMINIMAL_STACK_SIZE * 2 )

// Co-routine Configuration
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         ( 2 )

// Task Notifications
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3

// Event Groups and Stream Buffers
#define configUSE_EVENT_GROUPS                  1
#define configUSE_STREAM_BUFFERS                1

// Memory Allocation
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        1

// Function Includes
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 1
#define INCLUDE_xTaskGetHandle                  1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_eTaskGetState                   1

// Cortex-M Specific Definitions
#ifdef __NVIC_PRIO_BITS
    #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
    #define configPRIO_BITS         4
#endif

#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5

#define configKERNEL_INTERRUPT_PRIORITY         ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

// FreeRTOS MPU specific definitions (if using)
#define configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS 0

// Assert Definition
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

// Runtime Stats Timer
extern void vConfigureTimerForRunTimeStats( void );
extern unsigned long ulGetRunTimeCounterValue( void );
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()         ulGetRunTimeCounterValue()

// SVC and PendSV Handlers (map to FreeRTOS handlers)
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
```

## Configuration Best Practices

1. **Start Conservative**: Begin with larger stack sizes and heap, then optimize based on measured usage

2. **Enable Debugging Early**: Turn on stack checking and runtime stats during development

3. **Match Hardware**: Ensure `configCPU_CLOCK_HZ` exactly matches your system clock

4. **Choose Appropriate Tick Rate**: 1000 Hz (1ms) is typical; slower rates reduce overhead but limit timing precision

5. **Plan Priority Levels**: Use fewer priorities for simpler systems to reduce scheduling overhead

6. **Memory Scheme Selection**: heap_4 is recommended for most applications; heap_5 for complex memory layouts

7. **Disable Unused Features**: Each enabled feature consumes memory; only enable what you need

8. **Document Your Choices**: Comment your configuration file explaining why specific values were chosen

This comprehensive configuration control makes FreeRTOS highly portable and adaptable to diverse embedded systems while maintaining predictable real-time behavior.