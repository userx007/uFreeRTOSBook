# Critical Sections in FreeRTOS

## Overview

Critical sections are segments of code that must execute atomically without interruption to prevent race conditions and data corruption. In FreeRTOS, critical sections protect shared resources by temporarily disabling interrupts, ensuring that no context switch or interrupt can occur during the protected code execution.

## Core Concepts

### What is a Critical Section?

A critical section is a code region where:
- Shared resources (variables, hardware registers, data structures) are accessed
- Operations must complete atomically
- No interruption can be tolerated without risking data corruption
- Thread safety must be guaranteed

### Why Critical Sections Are Needed

Consider this vulnerable code without protection:

```c
volatile uint32_t sharedCounter = 0;

void incrementCounter(void) {
    sharedCounter++;  // NOT atomic! Multiple assembly instructions:
                      // 1. Load value from memory
                      // 2. Increment value
                      // 3. Store value back to memory
}
```

If an interrupt or context switch occurs between these steps, data corruption can occur.

## FreeRTOS Critical Section Macros

### Basic Macros

**taskENTER_CRITICAL()** - Disables interrupts up to a configurable priority level
**taskEXIT_CRITICAL()** - Re-enables interrupts to their previous state

```c
void safeIncrement(void) {
    taskENTER_CRITICAL();
    sharedCounter++;  // Protected from interruption
    taskEXIT_CRITICAL();
}
```

### ISR-Safe Macros

For use within Interrupt Service Routines:

**taskENTER_CRITICAL_FROM_ISR()** - Returns a state variable
**taskEXIT_CRITICAL_FROM_ISR(state)** - Restores the saved state

```c
void vInterruptHandler(void) {
    UBaseType_t uxSavedInterruptStatus;
    
    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    
    // Critical section code
    sharedData = processData();
    
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}
```

## Interrupt Disabling Mechanism

### How It Works

On ARM Cortex-M processors, FreeRTOS uses `configMAX_SYSCALL_INTERRUPT_PRIORITY` to determine which interrupts to disable:

- **High priority interrupts** (numerically lower values) - Continue running
- **Low priority interrupts** (at or below threshold) - Disabled during critical sections
- **FreeRTOS API calls** - Only allowed from interrupts at or below this priority

```c
// In FreeRTOSConfig.h
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  (5 << (8 - configPRIO_BITS))
```

### Priority Levels Example

```
Priority 0 (highest)  ─── Critical system interrupt (never disabled)
Priority 1-4          ─── High priority (never disabled by FreeRTOS)
Priority 5            ─── configMAX_SYSCALL_INTERRUPT_PRIORITY
Priority 6-15 (lowest)─── Disabled during critical sections
```

## Nested Critical Sections

Critical sections can be nested, with FreeRTOS tracking the nesting depth:

```c
volatile uint32_t criticalNesting = 0;

void outerFunction(void) {
    taskENTER_CRITICAL();  // criticalNesting = 1
    
    sharedData1 = 100;
    innerFunction();
    
    taskEXIT_CRITICAL();   // criticalNesting = 0
}

void innerFunction(void) {
    taskENTER_CRITICAL();  // criticalNesting = 2
    
    sharedData2 = 200;
    
    taskEXIT_CRITICAL();   // criticalNesting = 1 (interrupts still disabled)
}
```

Interrupts are only re-enabled when the outermost `taskEXIT_CRITICAL()` is called.

## Practical Examples

### Example 1: Protecting a Shared Counter

```c
#include "FreeRTOS.h"
#include "task.h"

volatile uint32_t globalCounter = 0;

void vTask1(void *pvParameters) {
    while (1) {
        taskENTER_CRITICAL();
        globalCounter++;
        taskEXIT_CRITICAL();
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vTask2(void *pvParameters) {
    while (1) {
        taskENTER_CRITICAL();
        globalCounter += 5;
        taskEXIT_CRITICAL();
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

### Example 2: Protecting Complex Data Structure

```c
typedef struct {
    uint32_t id;
    float temperature;
    uint8_t status;
} SensorData_t;

SensorData_t sensorReading;

void updateSensorData(uint32_t id, float temp, uint8_t status) {
    taskENTER_CRITICAL();
    
    // All fields updated atomically
    sensorReading.id = id;
    sensorReading.temperature = temp;
    sensorReading.status = status;
    
    taskEXIT_CRITICAL();
}

SensorData_t getSensorData(void) {
    SensorData_t localCopy;
    
    taskENTER_CRITICAL();
    
    // Read all fields atomically
    localCopy = sensorReading;
    
    taskEXIT_CRITICAL();
    
    return localCopy;
}
```

### Example 3: Shared Buffer Between Task and ISR

```c
#define BUFFER_SIZE 64
volatile uint8_t rxBuffer[BUFFER_SIZE];
volatile uint8_t rxHead = 0;
volatile uint8_t rxTail = 0;

// Called from UART ISR
void UART_IRQHandler(void) {
    UBaseType_t uxSavedInterruptStatus;
    uint8_t receivedByte;
    
    if (UART_RX_Ready()) {
        receivedByte = UART_ReadByte();
        
        uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
        
        uint8_t nextHead = (rxHead + 1) % BUFFER_SIZE;
        if (nextHead != rxTail) {  // Buffer not full
            rxBuffer[rxHead] = receivedByte;
            rxHead = nextHead;
        }
        
        taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
    }
}

// Called from task
uint8_t readFromBuffer(void) {
    uint8_t data = 0;
    
    taskENTER_CRITICAL();
    
    if (rxHead != rxTail) {  // Buffer not empty
        data = rxBuffer[rxTail];
        rxTail = (rxTail + 1) % BUFFER_SIZE;
    }
    
    taskEXIT_CRITICAL();
    
    return data;
}
```

### Example 4: Protecting Hardware Register Access

```c
#define LED_PORT    GPIOA
#define LED_PIN_1   (1 << 0)
#define LED_PIN_2   (1 << 1)

void setLEDPattern(uint8_t pattern) {
    taskENTER_CRITICAL();
    
    // Read-Modify-Write operation must be atomic
    uint32_t portValue = LED_PORT->ODR;
    portValue &= ~(LED_PIN_1 | LED_PIN_2);  // Clear LED bits
    portValue |= (pattern & 0x03);           // Set new pattern
    LED_PORT->ODR = portValue;
    
    taskEXIT_CRITICAL();
}
```

### Example 5: State Machine Protection

```c
typedef enum {
    STATE_IDLE,
    STATE_PROCESSING,
    STATE_COMPLETE,
    STATE_ERROR
} SystemState_t;

volatile SystemState_t systemState = STATE_IDLE;
volatile uint32_t processData = 0;

BaseType_t startProcessing(uint32_t data) {
    BaseType_t result = pdFALSE;
    
    taskENTER_CRITICAL();
    
    if (systemState == STATE_IDLE) {
        systemState = STATE_PROCESSING;
        processData = data;
        result = pdTRUE;
    }
    
    taskEXIT_CRITICAL();
    
    return result;
}

void completeProcessing(void) {
    taskENTER_CRITICAL();
    
    if (systemState == STATE_PROCESSING) {
        systemState = STATE_COMPLETE;
        processData = 0;
    }
    
    taskEXIT_CRITICAL();
}
```

## Best Practices

### Keep Critical Sections Short

```c
// BAD: Long critical section
taskENTER_CRITICAL();
complexCalculation();      // Takes 1ms
updateSharedData();
moreProcessing();          // Takes 2ms
taskEXIT_CRITICAL();

// GOOD: Minimal critical section
complexCalculation();      // Outside critical section
uint32_t result = moreProcessing();

taskENTER_CRITICAL();
updateSharedData();        // Only essential part protected
taskEXIT_CRITICAL();
```

### Avoid Blocking Calls

```c
// WRONG: Never do this!
taskENTER_CRITICAL();
vTaskDelay(100);           // Blocks with interrupts disabled!
taskEXIT_CRITICAL();

// WRONG: Don't call blocking APIs
taskENTER_CRITICAL();
xQueueSend(queue, &data, portMAX_DELAY);  // Can block!
taskEXIT_CRITICAL();
```

### Use Appropriate Synchronization

Critical sections are not always the best choice:

```c
// For simple flag: Use critical section
taskENTER_CRITICAL();
isReady = true;
taskEXIT_CRITICAL();

// For longer protection: Use mutex
xSemaphoreTake(mutex, portMAX_DELAY);
performComplexOperation();
xSemaphoreGive(mutex);

// For ISR to task signaling: Use queue or semaphore
xQueueSendFromISR(queue, &data, &xHigherPriorityTaskWoken);
```

## Common Pitfalls

### Forgetting to Exit Critical Section

```c
void riskyFunction(void) {
    taskENTER_CRITICAL();
    
    if (errorCondition) {
        return;  // BUG: Exits without taskEXIT_CRITICAL()!
    }
    
    updateData();
    taskEXIT_CRITICAL();
}

// FIX: Always exit
void safeFunction(void) {
    taskENTER_CRITICAL();
    
    if (!errorCondition) {
        updateData();
    }
    
    taskEXIT_CRITICAL();  // Always reached
}
```

### Mismatched ISR Macros

```c
// WRONG: Using task macros in ISR
void ISR_Handler(void) {
    taskENTER_CRITICAL();  // Wrong macro!
    processInterrupt();
    taskEXIT_CRITICAL();
}

// CORRECT: Using ISR macros
void ISR_Handler(void) {
    UBaseType_t uxSavedInterruptStatus;
    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    processInterrupt();
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}
```

## Performance Considerations

Critical sections disable interrupts, affecting system responsiveness:

```c
// Measured on Cortex-M4 @ 100MHz
taskENTER_CRITICAL();
// Interrupts disabled for duration of this code
sharedValue++;            // ~10 instruction cycles
taskEXIT_CRITICAL();
// Total interrupt latency: ~50-100 cycles

// Keep below 10-20 microseconds for real-time systems
```

## Alternatives to Critical Sections

When critical sections are too heavy:

1. **Atomic operations** (ARM Cortex-M3+):
```c
uint32_t oldValue = __sync_fetch_and_add(&sharedCounter, 1);
```

2. **Interrupt masking for specific peripherals**
3. **Mutex/Semaphore for longer operations**
4. **Lock-free algorithms for high-performance scenarios**

## Summary

Critical sections are essential for protecting shared resources in FreeRTOS, but should be used judiciously. Keep them short, always pair ENTER with EXIT, use ISR-specific macros in interrupt handlers, and consider alternative synchronization mechanisms for longer operations. Proper use of critical sections ensures data integrity while maintaining system responsiveness.