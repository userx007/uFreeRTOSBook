# FreeRTOS configuration parameters in `FreeRTOSConfig.h`

## **Hardware Description Parameters**

**configCPU_CLOCK_HZ**
- Description: Frequency of the clock that drives the peripheral used to generate the kernel's periodic tick interrupt
- Type: Integer (Hz)
- Example: `80000000` for 80MHz

**configTICK_RATE_HZ**
- Description: Frequency of the RTOS tick interrupt (how many times per second the tick interrupt occurs)
- Type: Integer (Hz)
- Range: Typically 100-1000 Hz
- Common values: `1000` (1ms tick), `100` (10ms tick)
- Note: Higher values increase timing resolution but also overhead

**configSYSTICK_CLOCK_HZ**
- Description: Frequency of the SysTick clock (if different from CPU clock)
- Type: Integer (Hz)
- Required: Only for some ports

---

## **Scheduler Configuration**

**configUSE_PREEMPTION**
- Description: Enable preemptive scheduling
- Values: `1` (preemptive), `0` (cooperative)
- Note: Set to 1 for most applications

**configUSE_TIME_SLICING**
- Description: Enable time slicing between tasks of equal priority
- Values: `1` (enabled), `0` (disabled)
- Default: 1 when not defined

**configUSE_PORT_OPTIMISED_TASK_SELECTION**
- Description: Use port-specific method for task selection (faster but limited priorities)
- Values: `1` (enabled), `0` (disabled)
- Note: Limits `configMAX_PRIORITIES` to 32 when enabled

**configUSE_TICKLESS_IDLE**
- Description: Enable tickless idle mode for low power applications
- Values: `0` (disabled), `1` (simple tickless), `2` (advanced tickless)

**configIDLE_SHOULD_YIELD**
- Description: Idle task yields to other idle priority tasks
- Values: `1` (enabled), `0` (disabled)
- Default: 1

---

## **Memory Management**

**configTOTAL_HEAP_SIZE**
- Description: Total amount of RAM available in the heap for FreeRTOS objects
- Type: Size_t (bytes)
- Example: `( ( size_t ) ( 17 * 1024 ) )` for 17KB
- Note: Only used with heap_1.c through heap_4.c

**configMINIMAL_STACK_SIZE**
- Description: Size of stack allocated to idle task
- Type: StackType_t (words, not bytes)
- Example: `( ( unsigned short ) 128 )`
- Note: 1 word = 4 bytes on 32-bit architectures

**configSUPPORT_STATIC_ALLOCATION**
- Description: Enable static memory allocation for tasks, queues, etc.
- Values: `1` (enabled), `0` (disabled)
- Default: 0

**configSUPPORT_DYNAMIC_ALLOCATION**
- Description: Enable dynamic memory allocation
- Values: `1` (enabled), `0` (disabled)
- Default: 1

**configAPPLICATION_ALLOCATED_HEAP**
- Description: Application provides heap array instead of FreeRTOS allocating it
- Values: `1` (enabled), `0` (disabled)

**configSTACK_ALLOCATION_FROM_SEPARATE_HEAP**
- Description: Use separate heap for task stacks
- Values: `1` (enabled), `0` (disabled)

---

## **Task Configuration**

**configMAX_PRIORITIES**
- Description: Number of available task priorities
- Type: Integer
- Range: 1 to 32 (or unlimited if not using optimized selection)
- Common: 5-10
- Note: Lower values consume less RAM

**configMAX_TASK_NAME_LEN**
- Description: Maximum length of task names including null terminator
- Type: Integer
- Common: 10-16

**configUSE_16_BIT_TICKS**
- Description: Use 16-bit type for tick count
- Values: `1` (16-bit), `0` (32-bit)
- Note: Only use 1 on 8/16-bit architectures

**configNUM_THREAD_LOCAL_STORAGE_POINTERS**
- Description: Number of thread local storage pointers per task
- Type: Integer
- Default: 0

**configUSE_TASK_NOTIFICATIONS**
- Description: Enable task notifications
- Values: `1` (enabled), `0` (disabled)
- Default: 1

**configTASK_NOTIFICATION_ARRAY_ENTRIES**
- Description: Number of notification indexes per task
- Type: Integer
- Default: 1

---

## **Hook Functions**

**configUSE_IDLE_HOOK**
- Description: Enable idle hook function
- Values: `1` (enabled), `0` (disabled)
- Function: `void vApplicationIdleHook( void )`

**configUSE_TICK_HOOK**
- Description: Enable tick hook function
- Values: `1` (enabled), `0` (disabled)
- Function: `void vApplicationTickHook( void )`
- Warning: Runs in ISR context

**configUSE_MALLOC_FAILED_HOOK**
- Description: Enable malloc failed hook
- Values: `1` (enabled), `0` (disabled)
- Function: `void vApplicationMallocFailedHook( void )`

**configUSE_DAEMON_TASK_STARTUP_HOOK**
- Description: Enable timer daemon startup hook
- Values: `1` (enabled), `0` (disabled)
- Function: `void vApplicationDaemonTaskStartupHook( void )`

**configCHECK_FOR_STACK_OVERFLOW**
- Description: Enable stack overflow detection
- Values: `0` (disabled), `1` (method 1), `2` (method 2)
- Function: `void vApplicationStackOverflowHook( TaskHandle_t, char * )`

---

## **Optional Features**

**configUSE_MUTEXES**
- Description: Include mutex functionality
- Values: `1` (enabled), `0` (disabled)

**configUSE_RECURSIVE_MUTEXES**
- Description: Include recursive mutex functionality
- Values: `1` (enabled), `0` (disabled)

**configUSE_COUNTING_SEMAPHORES**
- Description: Include counting semaphore functionality
- Values: `1` (enabled), `0` (disabled)

**configUSE_QUEUE_SETS**
- Description: Include queue set functionality
- Values: `1` (enabled), `0` (disabled)

**configUSE_ALTERNATIVE_API**
- Description: Include alternative queue API (deprecated)
- Values: `1` (enabled), `0` (disabled)
- Note: Not recommended for new designs

**configUSE_TIMERS**
- Description: Include software timer functionality
- Values: `1` (enabled), `0` (disabled)

**configTIMER_TASK_PRIORITY**
- Description: Priority of timer service task
- Type: Integer (priority level)
- Note: Usually set high (e.g., `configMAX_PRIORITIES - 1`)

**configTIMER_QUEUE_LENGTH**
- Description: Length of timer command queue
- Type: Integer
- Common: 5-10

**configTIMER_TASK_STACK_DEPTH**
- Description: Stack size for timer task
- Type: Integer (words)

---

## **Co-routine Configuration** (Legacy)

**configUSE_CO_ROUTINES**
- Description: Include co-routine functionality
- Values: `1` (enabled), `0` (disabled)
- Note: Rarely used, not recommended for new designs

**configMAX_CO_ROUTINE_PRIORITIES**
- Description: Number of co-routine priorities
- Type: Integer

---

## **Debug and Statistics**

**configUSE_TRACE_FACILITY**
- Description: Include additional structure members for trace/debug
- Values: `1` (enabled), `0` (disabled)

**configUSE_STATS_FORMATTING_FUNCTIONS**
- Description: Include functions to format statistics as human-readable ASCII
- Values: `1` (enabled), `0` (disabled)
- Note: Requires `configUSE_TRACE_FACILITY`

**configGENERATE_RUN_TIME_STATS**
- Description: Enable collection of runtime statistics
- Values: `1` (enabled), `0` (disabled)
- Requires: Definition of `portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()` and `portGET_RUN_TIME_COUNTER_VALUE()`

**configRECORD_STACK_HIGH_ADDRESS**
- Description: Record stack high address for debugging
- Values: `1` (enabled), `0` (disabled)

---

## **Assert and Error Handling**

**configASSERT( x )**
- Description: Macro for assertions
- Example: `#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }`
- Note: Should halt execution on failure

---

## **Interrupt Priority Configuration** (Cortex-M specific)

**configKERNEL_INTERRUPT_PRIORITY**
- Description: Priority of the kernel interrupt (tick and PendSV)
- Type: Integer
- Note: Should be set to lowest priority

**configMAX_SYSCALL_INTERRUPT_PRIORITY** / **configMAX_API_CALL_INTERRUPT_PRIORITY**
- Description: Highest interrupt priority from which interrupt-safe API functions can be called
- Type: Integer
- Range: 0 (highest) to configKERNEL_INTERRUPT_PRIORITY
- Critical: Must NOT be set to 0

**configLIBRARY_LOWEST_INTERRUPT_PRIORITY**
- Description: Lowest interrupt priority (board specific)
- Type: Integer
- Example: `15` for Cortex-M with 4 priority bits

**configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY**
- Description: Maximum syscall interrupt priority
- Type: Integer
- Example: `5`

**configPRIO_BITS**
- Description: Number of priority bits implemented in hardware
- Type: Integer
- Example: `4` (16 priority levels)

---

## **MPU (Memory Protection Unit) Configuration**

**configENABLE_MPU**
- Description: Enable MPU support
- Values: `1` (enabled), `0` (disabled)

**configENABLE_FPU**
- Description: Enable FPU (Floating Point Unit) support
- Values: `1` (enabled), `0` (disabled)

**configENABLE_TRUSTZONE**
- Description: Enable TrustZone support (ARMv8-M)
- Values: `1` (enabled), `0` (disabled)

---

## **Additional Configuration**

**configINCLUDE_APPLICATION_DEFINED_PRIVILEGED_FUNCTIONS**
- Description: Include application defined privileged functions
- Values: `1` (enabled), `0` (disabled)

**INCLUDE_xxx Functions**
Many optional API functions can be excluded to save space:

- **INCLUDE_vTaskPrioritySet** - `1`/`0`
- **INCLUDE_uxTaskPriorityGet** - `1`/`0`
- **INCLUDE_vTaskDelete** - `1`/`0`
- **INCLUDE_vTaskSuspend** - `1`/`0`
- **INCLUDE_vTaskDelayUntil** - `1`/`0`
- **INCLUDE_vTaskDelay** - `1`/`0`
- **INCLUDE_xTaskGetSchedulerState** - `1`/`0`
- **INCLUDE_xTaskGetCurrentTaskHandle** - `1`/`0`
- **INCLUDE_uxTaskGetStackHighWaterMark** - `1`/`0`
- **INCLUDE_xTaskGetIdleTaskHandle** - `1`/`0`
- **INCLUDE_eTaskGetState** - `1`/`0`
- **INCLUDE_xEventGroupSetBitFromISR** - `1`/`0`
- **INCLUDE_xTimerPendFunctionCall** - `1`/`0`
- **INCLUDE_xTaskAbortDelay** - `1`/`0`
- **INCLUDE_xTaskGetHandle** - `1`/`0`
- **INCLUDE_xSemaphoreGetMutexHolder** - `1`/`0`

---

## **Example Minimal Configuration**

```c
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      80000000
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128
#define configTOTAL_HEAP_SIZE                   10240
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               3
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE
```

For the most current and complete documentation, refer to the official FreeRTOS website at https://www.freertos.org/a00110.html (though it appears to have been reorganized recently).