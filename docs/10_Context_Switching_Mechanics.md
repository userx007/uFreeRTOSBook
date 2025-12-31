# Context Switching Mechanics in FreeRTOS

## Overview

Context switching is the fundamental mechanism that enables multitasking in FreeRTOS. It's the process of saving the execution state of one task and restoring the state of another, creating the illusion that multiple tasks run simultaneously on a single CPU core.

## What is Context Switching?

A context switch occurs when the scheduler decides to pause execution of the current task and resume execution of a different task. The "context" refers to all the information needed to resume a task exactly where it left off, including:

- CPU register values
- Program counter (PC)
- Stack pointer (SP)
- Processor status register
- Any floating-point unit (FPU) registers if applicable

## When Context Switches Occur

Context switches in FreeRTOS happen in several scenarios:

1. **Preemptive scheduling** - A higher-priority task becomes ready
2. **Time-slice expiration** - In round-robin scheduling, when a task's time quantum expires
3. **Voluntary yielding** - When a task calls `taskYIELD()`
4. **Blocking operations** - When a task enters a blocked state (waiting for queue, semaphore, etc.)
5. **ISR completion** - After an interrupt service routine, if a higher-priority task was made ready

## Assembly-Level Mechanics

The context switching process varies by architecture, but the general pattern is consistent. Let's examine ARM Cortex-M as an example, which is one of the most popular architectures for FreeRTOS.

### ARM Cortex-M Context Switch Process

The Cortex-M architecture has hardware support for context switching, which FreeRTOS leverages through the PendSV exception.

**Registers to Save/Restore:**

**Hardware-saved registers (automatic):**
- R0-R3 (argument/scratch registers)
- R12 (intra-procedure call scratch register)
- LR (Link Register / R14)
- PC (Program Counter / R15)
- xPSR (Program Status Register)

**Software-saved registers (manual):**
- R4-R11 (general purpose registers)
- PSP/MSP (Process/Main Stack Pointer)
- FPU registers S16-S31 (if FPU is used)

### Example: Cortex-M3/M4 Context Switch Code

Here's a simplified version of the FreeRTOS context switch implementation for ARM Cortex-M:

```assembly
; PendSV Handler - triggers context switch
PendSV_Handler:
    ; Disable interrupts
    CPSID   I
    
    ; Get current task's stack pointer
    MRS     R0, PSP                 ; Get Process Stack Pointer
    
    ; Save remaining context (R4-R11) manually
    ; Hardware automatically saved R0-R3, R12, LR, PC, xPSR
    STMDB   R0!, {R4-R11}          ; Store multiple, decrement before
    
    ; Save the new top of stack into the task's TCB
    LDR     R1, =pxCurrentTCB      ; Get address of current TCB pointer
    LDR     R2, [R1]                ; Get current TCB
    STR     R0, [R2]                ; Save stack pointer in TCB
    
    ; --- Context switch happens here ---
    ; Select next task to run
    PUSH    {R3, R14}               ; Save registers
    CPSID   I                       ; Ensure interrupts disabled
    BL      vTaskSwitchContext      ; Call FreeRTOS task selector
    CPSIE   I                       ; Re-enable interrupts
    POP     {R3, R14}               ; Restore registers
    
    ; Load new task's stack pointer from TCB
    LDR     R1, =pxCurrentTCB      ; Get address of current TCB pointer
    LDR     R2, [R1]                ; Get new current TCB
    LDR     R0, [R2]                ; Load stack pointer from TCB
    
    ; Restore context (R4-R11)
    LDMIA   R0!, {R4-R11}          ; Load multiple, increment after
    
    ; Update PSP with new task's stack pointer
    MSR     PSP, R0
    
    ; Ensure exception return uses Process Stack
    ORR     LR, LR, #0x04
    
    ; Enable interrupts
    CPSIE   I
    
    ; Return - hardware will restore R0-R3, R12, LR, PC, xPSR
    BX      LR
```

### Breaking Down the Process

**Step 1: Save Current Task Context**
```assembly
MRS     R0, PSP                 ; Get current stack pointer
STMDB   R0!, {R4-R11}          ; Save R4-R11 to stack
STR     R0, [pxCurrentTCB]     ; Save new stack pointer to TCB
```

**Step 2: Select Next Task**
```c
// In vTaskSwitchContext() - simplified
void vTaskSwitchContext(void) {
    // Find highest priority ready task
    taskSELECT_HIGHEST_PRIORITY_TASK();
    
    // Update pxCurrentTCB to point to new task
    pxCurrentTCB = pxReadyTasksLists[uxTopReadyPriority].pxNext;
}
```

**Step 3: Restore New Task Context**
```assembly
LDR     R0, [pxCurrentTCB]     ; Load new task's stack pointer
LDMIA   R0!, {R4-R11}          ; Restore R4-R11 from stack
MSR     PSP, R0                 ; Update stack pointer
BX      LR                      ; Return (hardware restores rest)
```

## Task Control Block (TCB) Structure

The TCB contains all information about a task, including where its context is saved:

```c
typedef struct tskTaskControlBlock {
    volatile StackType_t *pxTopOfStack;  // Points to last item on stack
    
    ListItem_t xStateListItem;            // Task's position in state list
    ListItem_t xEventListItem;            // For event handling
    UBaseType_t uxPriority;               // Task priority
    StackType_t *pxStack;                 // Points to start of stack
    char pcTaskName[configMAX_TASK_NAME_LEN];
    
    #if (configUSE_TRACE_FACILITY == 1)
        UBaseType_t uxTCBNumber;
        UBaseType_t uxTaskNumber;
    #endif
    
    #if (configUSE_MUTEXES == 1)
        UBaseType_t uxBasePriority;
        UBaseType_t uxMutexesHeld;
    #endif
    
    // ... other fields
} tskTCB;
```

## Stack Layout During Context Switch

Here's what a task's stack looks like after a context switch:

```
High Memory
┌─────────────────────┐
│   Task Variables    │ ← Original stack usage
│   Local Variables   │
├─────────────────────┤
│   xPSR              │ ← Hardware saved (exception entry)
│   PC (Return Addr)  │
│   LR (R14)          │
│   R12               │
│   R3                │
│   R2                │
│   R1                │
│   R0                │
├─────────────────────┤
│   R11               │ ← Software saved (in PendSV)
│   R10               │
│   R9                │
│   R8                │
│   R7                │
│   R6                │
│   R5                │
│   R4                │
└─────────────────────┘ ← pxTopOfStack points here
Low Memory
```

## Context Switch Overhead

The overhead of context switching includes several components:

### Time Overhead

**Direct costs:**
- Saving registers: ~20-40 CPU cycles
- Restoring registers: ~20-40 CPU cycles
- Scheduler decision: ~50-100 CPU cycles
- Cache/pipeline effects: Variable, can be significant

**Example calculation for Cortex-M4 at 168 MHz:**
```
Register save/restore: 80 cycles = 0.48 μs
Scheduler overhead: 100 cycles = 0.60 μs
Pipeline flush penalty: ~50 cycles = 0.30 μs
Total: ~230 cycles = 1.4 μs per context switch
```

### Indirect Costs

1. **Cache pollution** - New task may cause cache misses
2. **Pipeline stalls** - Branch prediction needs to relearn
3. **TLB misses** - If using memory protection units
4. **Interrupt latency** - Brief period with interrupts disabled

### Measuring Context Switch Time

Here's practical code to measure context switch overhead:

```c
#include "FreeRTOS.h"
#include "task.h"

// High-resolution timer functions (platform-specific)
volatile uint32_t contextSwitchCount = 0;
volatile uint32_t totalSwitchTime = 0;

void vTaskA(void *pvParameters) {
    uint32_t startTime, endTime;
    
    while(1) {
        // Measure time before yield
        startTime = GET_CPU_CYCLE_COUNT();
        
        // Force context switch
        taskYIELD();
        
        // Measure time after switch back
        endTime = GET_CPU_CYCLE_COUNT();
        
        // This includes switch away AND switch back
        totalSwitchTime += (endTime - startTime);
        contextSwitchCount++;
        
        // Calculate average
        if(contextSwitchCount >= 1000) {
            uint32_t avgTime = totalSwitchTime / (contextSwitchCount * 2);
            printf("Average context switch: %lu cycles\n", avgTime);
            
            contextSwitchCount = 0;
            totalSwitchTime = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vTaskB(void *pvParameters) {
    // Simple task that just yields
    while(1) {
        taskYIELD();
    }
}
```

## Optimizing Context Switch Performance

### 1. Lazy Context Switching

Some architectures support lazy FPU context saving:

```c
// Only save FPU registers if task actually used them
#if (configUSE_TASK_FPU_SUPPORT == 1)
    if(task_used_fpu) {
        // Save FPU registers S16-S31
        __asm volatile (
            "VSTMDB R0!, {S16-S31}"
        );
    }
#endif
```

### 2. Minimize Priority Levels

Fewer priority levels mean faster scheduler decisions:

```c
// config file
#define configMAX_PRIORITIES  5  // Instead of 32
```

### 3. Use Cooperative Scheduling When Possible

```c
// In FreeRTOSConfig.h
#define configUSE_PREEMPTION  0  // Cooperative mode
```

This eliminates preemptive context switches, only switching when tasks voluntarily yield.

## Advanced Example: Context Switch with Statistics

Here's a complete example showing context switching with runtime statistics:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"

// Enable runtime statistics
#define configGENERATE_RUN_TIME_STATS       1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1

// Task handles
TaskHandle_t xHighPriorityTask;
TaskHandle_t xMediumPriorityTask;
TaskHandle_t xLowPriorityTask;

// High priority task - runs frequently, triggers many switches
void vHighPriorityTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while(1) {
        // Simulate work
        volatile uint32_t i;
        for(i = 0; i < 1000; i++);
        
        printf("High priority task running\n");
        
        // Delay, allowing other tasks to run (context switch)
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}

// Medium priority task
void vMediumPriorityTask(void *pvParameters) {
    while(1) {
        printf("Medium priority task running\n");
        
        // Voluntarily yield (context switch)
        taskYIELD();
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// Low priority task - monitors context switches
void vLowPriorityTask(void *pvParameters) {
    char statsBuffer[512];
    
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Get runtime statistics
        vTaskGetRunTimeStats(statsBuffer);
        printf("\n=== Task Statistics ===\n%s\n", statsBuffer);
        
        // Get task states
        printf("Task States:\n");
        printf("High Priority: %d\n", eTaskGetState(xHighPriorityTask));
        printf("Medium Priority: %d\n", eTaskGetState(xMediumPriorityTask));
        printf("Low Priority: %d\n", eTaskGetState(xLowPriorityTask));
    }
}

int main(void) {
    // Initialize hardware
    SystemInit();
    
    // Create tasks with different priorities
    xTaskCreate(vHighPriorityTask, "High", 128, NULL, 3, &xHighPriorityTask);
    xTaskCreate(vMediumPriorityTask, "Medium", 128, NULL, 2, &xMediumPriorityTask);
    xTaskCreate(vLowPriorityTask, "Low", 256, NULL, 1, &xLowPriorityTask);
    
    // Start scheduler - first context switch happens here
    vTaskStartScheduler();
    
    // Should never reach here
    while(1);
}
```

## Context Switch Tracing

For debugging, you can hook into context switches:

```c
// In FreeRTOSConfig.h
#define traceTASK_SWITCHED_IN() \
    printf("Switched IN: %s\n", pxCurrentTCB->pcTaskName)

#define traceTASK_SWITCHED_OUT() \
    printf("Switched OUT: %s\n", pxCurrentTCB->pcTaskName)

// More detailed tracing
extern void vTraceContextSwitch(void* pxOldTCB, void* pxNewTCB);

#define traceTASK_SWITCHED_OUT() \
    vTraceContextSwitch(pxCurrentTCB, pxNextTCB)
```

## Platform-Specific Considerations

### Different Architectures Handle Context Switching Differently

**ARM Cortex-M (M0/M3/M4/M7):**
- Hardware stacking of 8 registers
- Uses PendSV for context switching
- Efficient, optimized design for RTOS

**ARM Cortex-A/R:**
- More complex with multiple processor modes
- May require saving banked registers
- FIQ/IRQ mode considerations

**RISC-V:**
- Software-managed context switching
- Must save all 32 registers manually
- Interrupt controller integration varies

**x86:**
- Large context (many registers, FPU, SSE)
- Higher context switch overhead
- Typically uses timer interrupt for preemption

## Key Takeaways

Context switching is the heartbeat of FreeRTOS multitasking. Understanding its mechanics helps you write more efficient embedded applications by minimizing unnecessary switches, choosing appropriate task priorities, and understanding the true cost of multitasking. The overhead, while present, is typically small enough that the benefits of structured multitasking far outweigh the costs in most embedded applications.