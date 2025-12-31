# Custom Port Development in FreeRTOS

## Overview

Custom port development is the process of adapting FreeRTOS to run on a new microcontroller architecture or compiler toolchain. A port consists of architecture-specific code that bridges FreeRTOS's portable kernel with the hardware's unique features like interrupt handling, context switching, and timer management.

## Core Components of a FreeRTOS Port

A FreeRTOS port typically consists of three main files:

1. **portmacro.h** - Contains port-specific data types, macros, and inline functions
2. **port.c** - Implements the core porting layer functions
3. **portasm.s** (or similar) - Assembly routines for context switching and critical operations

## Required Functions to Implement

### 1. Stack Initialization
```c
StackType_t *pxPortInitialiseStack(
    StackType_t *pxTopOfStack,
    TaskFunction_t pxCode,
    void *pvParameters
)
```

This function creates an initial stack frame for a new task that mimics what would exist after an interrupt. When the scheduler starts or switches to this task, it appears as though the task is returning from an interrupt.

**Example Implementation (ARM Cortex-M):**
```c
StackType_t *pxPortInitialiseStack(
    StackType_t *pxTopOfStack,
    TaskFunction_t pxCode,
    void *pvParameters
)
{
    /* Simulate the stack frame as it would be created by a context switch */
    pxTopOfStack--;
    *pxTopOfStack = 0x01000000;  /* xPSR - Thumb bit must be set */
    
    pxTopOfStack--;
    *pxTopOfStack = ((StackType_t)pxCode) & 0xfffffffeUL;  /* PC */
    
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t)prvTaskExitError;  /* LR */
    
    pxTopOfStack -= 5;  /* R12, R3, R2, R1 */
    *pxTopOfStack = (StackType_t)pvParameters;  /* R0 - task parameter */
    
    pxTopOfStack -= 8;  /* R11, R10, R9, R8, R7, R6, R5, R4 */
    
    return pxTopOfStack;
}
```

### 2. Starting the Scheduler
```c
BaseType_t xPortStartScheduler(void)
```

This function configures the tick timer interrupt and starts the first task.

**Example:**
```c
BaseType_t xPortStartScheduler(void)
{
    /* Configure the system tick timer */
    vPortSetupTimerInterrupt();
    
    /* Start the first task by restoring its context */
    prvStartFirstTask();
    
    /* Should never reach here */
    return pdFALSE;
}
```

### 3. Ending the Scheduler
```c
void vPortEndScheduler(void)
```

Stops the scheduler (rarely used in embedded systems).

### 4. Tick Interrupt Handler
```c
void xPortSysTickHandler(void)
{
    /* Increment the RTOS tick */
    if (xTaskIncrementTick() != pdFALSE) {
        /* A context switch is required */
        portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT;
    }
}
```

### 5. Context Switch Functions

**PendSV Handler (ARM Cortex-M example in assembly):**
```asm
__asm void xPortPendSVHandler(void)
{
    extern pxCurrentTCB
    extern vTaskSwitchContext
    
    PRESERVE8
    
    mrs r0, psp
    isb
    
    ldr r3, =pxCurrentTCB    /* Get location of current TCB */
    ldr r2, [r3]
    
    stmdb r0!, {r4-r11}      /* Save remaining registers */
    str r0, [r2]             /* Save new top of stack */
    
    stmdb sp!, {r3, r14}
    mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
    msr basepri, r0
    dsb
    isb
    bl vTaskSwitchContext
    mov r0, #0
    msr basepri, r0
    ldmia sp!, {r3, r14}
    
    ldr r1, [r3]             /* Get new task TCB */
    ldr r0, [r1]             /* Get new stack pointer */
    
    ldmia r0!, {r4-r11}      /* Restore registers */
    msr psp, r0
    isb
    bx r14
}
```

## Port-Specific Macros in portmacro.h

```c
/* Data types */
#define portCHAR        char
#define portFLOAT       float
#define portDOUBLE      double
#define portLONG        long
#define portSHORT       short
#define portSTACK_TYPE  uint32_t
#define portBASE_TYPE   long

/* Architecture specifics */
#define portSTACK_GROWTH            (-1)
#define portTICK_PERIOD_MS          ((TickType_t) 1000 / configTICK_RATE_HZ)
#define portBYTE_ALIGNMENT          8

/* Critical section management */
#define portDISABLE_INTERRUPTS()    __disable_irq()
#define portENABLE_INTERRUPTS()     __enable_irq()

extern void vPortEnterCritical(void);
extern void vPortExitCritical(void);
#define portENTER_CRITICAL()        vPortEnterCritical()
#define portEXIT_CRITICAL()         vPortExitCritical()

/* Task function macros */
#define portTASK_FUNCTION_PROTO(vFunction, pvParameters) \
    void vFunction(void *pvParameters)
#define portTASK_FUNCTION(vFunction, pvParameters) \
    void vFunction(void *pvParameters)

/* Yield from ISR */
#define portYIELD_FROM_ISR(x) \
    if(x) portYIELD()
```

## Testing Strategy for New Ports

### 1. Basic Functionality Tests

**Test 1: Single Task Execution**
```c
void vSimpleTask(void *pvParameters)
{
    volatile uint32_t counter = 0;
    
    for (;;) {
        counter++;
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        /* Use debugger to verify counter increments */
    }
}

void testSingleTask(void)
{
    xTaskCreate(vSimpleTask, "Simple", 128, NULL, 1, NULL);
    vTaskStartScheduler();
}
```

**Test 2: Multiple Task Context Switching**
```c
void vTask1(void *pvParameters)
{
    for (;;) {
        GPIO_SetBit(LED1);
        vTaskDelay(pdMS_TO_TICKS(100));
        GPIO_ClearBit(LED1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vTask2(void *pvParameters)
{
    for (;;) {
        GPIO_SetBit(LED2);
        vTaskDelay(pdMS_TO_TICKS(250));
        GPIO_ClearBit(LED2);
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void testContextSwitching(void)
{
    xTaskCreate(vTask1, "Task1", 128, NULL, 1, NULL);
    xTaskCreate(vTask2, "Task2", 128, NULL, 1, NULL);
    vTaskStartScheduler();
}
```

### 2. Stack Overflow Detection Test

```c
void vStackOverflowTask(void *pvParameters)
{
    volatile uint8_t largeArray[512];
    
    /* Fill array to use stack */
    for (int i = 0; i < 512; i++) {
        largeArray[i] = i;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskDelete(NULL);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* If this hook is called, stack overflow detection works */
    for (;;) {
        /* Trap execution */
    }
}
```

### 3. Interrupt Handling Test

```c
volatile uint32_t isrCounter = 0;
SemaphoreHandle_t xSemaphore;

void UART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    isrCounter++;
    
    /* Clear interrupt flag */
    UART_ClearInterrupt();
    
    /* Signal task from ISR */
    xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vInterruptTestTask(void *pvParameters)
{
    for (;;) {
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
            /* Process interrupt event */
        }
    }
}
```

### 4. Critical Section Test

```c
volatile uint32_t sharedResource = 0;

void vCriticalSectionTask(void *pvParameters)
{
    uint32_t localCopy;
    
    for (;;) {
        taskENTER_CRITICAL();
        {
            localCopy = sharedResource;
            vTaskDelay(1);  /* Should not allow task switch */
            sharedResource = localCopy + 1;
        }
        taskEXIT_CRITICAL();
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### 5. Floating Point Context Test

```c
void vFloatingPointTask(void *pvParameters)
{
    float value = 1.5f;
    
    for (;;) {
        value = value * 2.0f + 0.5f;
        
        /* Verify value remains consistent across context switches */
        configASSERT(value > 0.0f);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void testFloatingPoint(void)
{
    xTaskCreate(vFloatingPointTask, "FP1", 256, NULL, 2, NULL);
    xTaskCreate(vFloatingPointTask, "FP2", 256, NULL, 2, NULL);
    vTaskStartScheduler();
}
```

## Common Pitfalls and Debugging Tips

### 1. Stack Alignment Issues
Many architectures require specific stack alignment (typically 8 bytes for ARM). Ensure `portBYTE_ALIGNMENT` is set correctly and stack initialization respects this.

### 2. Interrupt Priority Configuration
On architectures with priority-based interrupts (like ARM Cortex-M), ensure `configMAX_SYSCALL_INTERRUPT_PRIORITY` is properly set and all FreeRTOS API calls from ISRs use appropriate priority levels.

### 3. Context Saving Completeness
Verify all registers that need preservation are saved during context switching, including:
- General purpose registers
- Program counter
- Status registers
- Floating point registers (if used)

### 4. Timer Configuration
The tick timer must generate interrupts at the correct frequency defined by `configTICK_RATE_HZ`. Use oscilloscope or logic analyzer to verify actual tick rate.

### 5. Memory Barriers
Modern processors may reorder instructions. Use appropriate memory barriers (DSB, ISB on ARM) to ensure correct operation, especially around critical sections and context switches.

## Validation Checklist

- [ ] Single task runs continuously
- [ ] Multiple tasks context switch correctly
- [ ] Task delays are accurate (verify with timer/scope)
- [ ] Stack overflow detection works
- [ ] ISR-to-task communication functions properly
- [ ] Critical sections prevent preemption
- [ ] Floating point context preserved (if applicable)
- [ ] Priority scheduling works correctly
- [ ] Semaphores, queues, and mutexes operate correctly
- [ ] Task notifications function properly
- [ ] Software timers work as expected
- [ ] Memory allocation/deallocation stable under stress
- [ ] System runs stable for extended periods (days)

Creating a robust FreeRTOS port requires careful attention to architecture details, thorough testing, and validation across all kernel features. The port serves as the foundation for all FreeRTOS functionality, so investing time in proper implementation and testing is essential.