# Assembly Language Port Files in FreeRTOS

## Overview

Assembly language port files are critical components of FreeRTOS that handle low-level, processor-specific operations that cannot be efficiently implemented in C. These files contain the "glue" between the portable FreeRTOS kernel and the specific microcontroller architecture, managing context switching, interrupt handling, and stack manipulation.

## Key Components

### 1. **Context Switching**

Context switching is the process of saving the state of one task and restoring the state of another. This involves:

- **Saving the current task's context**: All CPU registers, program counter, and stack pointer
- **Restoring the next task's context**: Loading the saved state of the task being switched to

**Why Assembly?**
- Direct register access and manipulation
- Precise control over stack operations
- Minimal overhead and predictable timing
- Access to special CPU instructions not available in C

### 2. **PendSV Handler (ARM Cortex-M)**

The PendSV (Pendable Service) exception is used for context switching on ARM Cortex-M processors. It's triggered by the scheduler and runs at the lowest interrupt priority.

## ARM Cortex-M Context Switch Example

Here's a detailed breakdown of typical PendSV handler assembly code:

```assembly
; PendSV_Handler implementation for ARM Cortex-M3/M4
; This handler performs the actual context switch

PendSV_Handler:
    ; Disable interrupts during context switch
    CPSID   I
    
    ; Get the current task control block (TCB) pointer
    LDR     R3, =pxCurrentTCB    ; Load address of pxCurrentTCB
    LDR     R1, [R3]             ; Load pxCurrentTCB into R1
    
    ; Check if using FPU (Cortex-M4F/M7)
    TST     R14, #0x10           ; Test bit 4 of LR (EXC_RETURN)
    IT      EQ                   ; If zero, FPU was used
    VSTMDBEQ R0!, {S16-S31}      ; Save FPU registers S16-S31
    
    ; Save remaining context onto task stack
    MRS     R0, PSP              ; Get Process Stack Pointer
    STMDB   R0!, {R4-R11, R14}   ; Save R4-R11 and LR onto stack
    
    ; Save the new top of stack into the TCB
    STR     R0, [R1]             ; Store stack pointer in TCB
    
    ; --- Context saved, now select next task ---
    
    PUSH    {R3, R14}
    MOV     R0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
    MSR     BASEPRI, R0          ; Mask interrupts
    DSB                          ; Data Synchronization Barrier
    ISB                          ; Instruction Synchronization Barrier
    BL      vTaskSwitchContext   ; Call scheduler (C function)
    MOV     R0, #0
    MSR     BASEPRI, R0          ; Unmask interrupts
    POP     {R3, R14}
    
    ; --- Load context of next task ---
    
    LDR     R1, [R3]             ; Load new pxCurrentTCB
    LDR     R0, [R1]             ; Load stack pointer from TCB
    
    ; Restore R4-R11 and LR from new task stack
    LDMIA   R0!, {R4-R11, R14}
    
    ; Restore FPU registers if necessary
    TST     R14, #0x10
    IT      EQ
    VLDMIAEQ R0!, {S16-S31}
    
    ; Update Process Stack Pointer
    MSR     PSP, R0
    
    ; Enable interrupts
    CPSIE   I
    
    ; Return from exception (automatic hardware restore of R0-R3, R12, LR, PC, xPSR)
    BX      R14
```

## Detailed Explanation of Key Instructions

### Stack Operations

```assembly
STMDB   R0!, {R4-R11, R14}   ; Store Multiple Decrement Before
                              ; Saves registers to stack, updates R0
```

This instruction:
- Saves registers R4 through R11 and R14 (Link Register)
- Decrements the stack pointer (R0) before each store
- The `!` updates R0 with the new stack pointer value

### Special Register Access

```assembly
MRS     R0, PSP              ; Move to Register from Special register
                              ; Copies PSP (Process Stack Pointer) to R0

MSR     PSP, R0              ; Move to Special register from Register
                              ; Copies R0 to PSP
```

The Cortex-M has two stack pointers:
- **MSP (Main Stack Pointer)**: Used by interrupts and kernel
- **PSP (Process Stack Pointer)**: Used by tasks

### Interrupt Priority Manipulation

```assembly
MOV     R0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
MSR     BASEPRI, R0          ; Set base priority mask
```

This masks interrupts below a certain priority, ensuring critical sections aren't interrupted.

## Starting the First Task

Another important assembly function is the scheduler startup:

```assembly
vPortStartFirstTask:
    ; Set the MSP to the value defined in the vector table
    LDR     R0, =0xE000ED08      ; Load address of VTOR
    LDR     R0, [R0]             ; Load VTOR value
    LDR     R0, [R0]             ; Load initial MSP value
    MSR     MSP, R0              ; Set MSP
    
    ; Enable global interrupts
    CPSIE   I
    CPSIE   F
    DSB
    ISB
    
    ; Call SVC to start first task
    SVC     0                    ; Trigger SVC exception
    NOP
    NOP

; SVC Handler - used to start the first task
SVC_Handler:
    ; Get the location of the first task TCB
    LDR     R3, =pxCurrentTCB
    LDR     R1, [R3]
    LDR     R0, [R1]             ; Get stack pointer
    
    ; Restore context from first task
    LDMIA   R0!, {R4-R11, R14}
    MSR     PSP, R0
    MOV     R0, #0
    MSR     BASEPRI, R0
    
    ; Return from exception using PSP
    BX      R14
```

## Practical Example: Context Switch Flow

Let's trace a complete context switch when Task A yields to Task B:

```c
// Task A running, calls taskYIELD()
void vTaskA(void *pvParameters) {
    while(1) {
        // Do some work
        processData();
        
        // Yield to other tasks
        taskYIELD();  // This triggers PendSV
        
        // When we return here, context has been restored
    }
}
```

**Step-by-step process:**

1. **Task A calls taskYIELD()** which triggers PendSV interrupt
2. **CPU enters PendSV handler** (hardware automatically saves R0-R3, R12, LR, PC, xPSR)
3. **Assembly code saves remaining registers** (R4-R11, FPU registers if used)
4. **Stack pointer saved to Task A's TCB**
5. **vTaskSwitchContext() called** (C function that selects next task)
6. **Task B selected** as next task
7. **Task B's stack pointer loaded** from its TCB
8. **Task B's registers restored** from its stack
9. **Return from PendSV** (hardware restores R0-R3, R12, LR, PC, xPSR)
10. **Task B now running** from where it was suspended

## Reading Port Files

When examining port.c or portasm.s files, look for:

### Common Functions
- `pxPortInitialiseStack()` - Sets up initial task stack
- `vPortStartFirstTask()` - Starts the scheduler
- `vPortYield()` - Triggers context switch
- `xPortPendSVHandler()` or `PendSV_Handler()` - Context switch handler
- `xPortSysTickHandler()` or `SysTick_Handler()` - Timer tick handler

### Architecture-Specific Details

**For ARM Cortex-M:**
- Uses PendSV for context switching
- SysTick for time base
- Two stack pointers (MSP/PSP)
- Hardware floating-point support handling

**For other architectures:**
- Different interrupt mechanisms
- Stack growth direction variations
- Register sets and calling conventions
- Cache and memory barrier requirements

## Debugging Tips

When working with assembly port files:

1. **Use a debugger with register view** to watch context switches
2. **Set breakpoints in PendSV** to observe state transitions
3. **Check stack alignment** (ARM requires 8-byte alignment)
4. **Verify interrupt priorities** are configured correctly
5. **Monitor PSP and MSP** values during task switches

## Performance Considerations

Assembly port code is highly optimized because:
- Context switches occur frequently (every time slice)
- Overhead directly impacts system responsiveness
- Fewer instructions mean lower latency
- Predictable timing is critical for real-time behavior

A typical context switch on ARM Cortex-M takes 20-50 CPU cycles, which is remarkably fast thanks to optimized assembly implementation.

Understanding these assembly port files is essential for debugging complex issues, porting FreeRTOS to new platforms, or optimizing for specific hardware features.