# Interrupt Nesting and Priority in FreeRTOS

## Overview

Interrupt nesting and priority management are critical aspects of FreeRTOS that ensure real-time responsiveness while maintaining system stability. FreeRTOS provides mechanisms to safely handle interrupts at different priority levels, allowing higher-priority interrupts to preempt lower-priority ones while protecting critical sections of the RTOS kernel.

## Understanding Interrupt Priority Levels

FreeRTOS divides interrupts into two categories based on their priority:

**1. High-Priority Interrupts (Above configMAX_SYSCALL_INTERRUPT_PRIORITY)**
- Cannot call FreeRTOS API functions
- Never masked by the RTOS
- Provide the lowest latency response
- Must handle everything without RTOS support

**2. Lower-Priority Interrupts (At or below configMAX_SYSCALL_INTERRUPT_PRIORITY)**
- Can safely call FreeRTOS "FromISR" API functions
- May be temporarily masked during critical sections
- Can interact with tasks through queues, semaphores, etc.

## configMAX_SYSCALL_INTERRUPT_PRIORITY Configuration

This configuration parameter defines the highest interrupt priority from which FreeRTOS API calls can be made. Its meaning varies by processor architecture.

### ARM Cortex-M Specific Behavior

On ARM Cortex-M processors, interrupt priorities work in a counter-intuitive way: numerically **lower** values represent **higher** priorities.

```c
/* FreeRTOSConfig.h */

/* Maximum priority for interrupts that can call FreeRTOS APIs */
/* Higher numeric value = lower priority */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191  /* Priority 5 */

/* Cortex-M typically has priorities 0-255 (or subset based on implemented bits) */
/* Priority 0 = Highest priority (never masked by FreeRTOS)
   Priority 191 = Threshold for syscall safety
   Priority 255 = Lowest priority */

/* Number of priority bits implemented (processor dependent) */
#define configPRIO_BITS    4  /* 16 priority levels (0-15) */
```

### Priority Level Calculation

```c
/* For a Cortex-M with 4 priority bits (16 levels) */
/* Priorities are in upper bits of an 8-bit field */

/* Highest priority - never calls FreeRTOS */
#define CRITICAL_IRQ_PRIORITY    0    /* Binary: 0000 0000 */

/* Can call FreeRTOS APIs */
#define TIMER_IRQ_PRIORITY       128  /* Binary: 1000 0000 (priority 8) */
#define UART_IRQ_PRIORITY        160  /* Binary: 1010 0000 (priority 10) */

/* Lowest priority */
#define BACKGROUND_IRQ_PRIORITY  240  /* Binary: 1111 0000 (priority 15) */

/* Set in NVIC */
NVIC_SetPriority(TIMER_IRQn, TIMER_IRQ_PRIORITY);
```

## Interrupt Masking Mechanisms

FreeRTOS uses interrupt masking to protect critical sections where kernel data structures are being modified.

### Critical Section Macros

```c
/* Enter critical section - disables interrupts up to 
   configMAX_SYSCALL_INTERRUPT_PRIORITY */
taskENTER_CRITICAL();

/* Modify shared data */
sharedCounter++;

/* Exit critical section - restores interrupt state */
taskEXIT_CRITICAL();
```

### From ISR Critical Sections

```c
void UART_IRQHandler(void)
{
    UBaseType_t savedInterruptStatus;
    
    /* Save interrupt mask state and disable interrupts */
    savedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    
    /* Access shared resource safely */
    processUARTData();
    
    /* Restore interrupt mask state */
    taskEXIT_CRITICAL_FROM_ISR(savedInterruptStatus);
}
```

## Practical Example: Multi-Priority Interrupt System

Here's a comprehensive example demonstrating interrupt nesting with different priority levels:

```c
/* FreeRTOSConfig.h configuration */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    191
#define configKERNEL_INTERRUPT_PRIORITY         255

/* Interrupt priorities */
#define EMERGENCY_STOP_PRIORITY    0    /* Highest - no FreeRTOS calls */
#define MOTOR_CONTROL_PRIORITY     128  /* Can use FreeRTOS APIs */
#define SENSOR_READ_PRIORITY       160  /* Lower priority */
#define LOGGING_PRIORITY           224  /* Lowest priority */

/* Shared data structure */
typedef struct {
    uint32_t motorSpeed;
    uint32_t sensorValue;
    bool emergencyStop;
} SystemData_t;

volatile SystemData_t systemData;
SemaphoreHandle_t dataMutex;
QueueHandle_t eventQueue;

/* Hardware setup */
void setupInterrupts(void)
{
    /* Emergency stop - highest priority, cannot use FreeRTOS */
    NVIC_SetPriority(EXTI0_IRQn, EMERGENCY_STOP_PRIORITY);
    NVIC_EnableIRQ(EXTI0_IRQn);
    
    /* Motor control - can use FreeRTOS APIs */
    NVIC_SetPriority(TIM1_IRQn, MOTOR_CONTROL_PRIORITY);
    NVIC_EnableIRQ(TIM1_IRQn);
    
    /* Sensor reading */
    NVIC_SetPriority(ADC_IRQn, SENSOR_READ_PRIORITY);
    NVIC_EnableIRQ(ADC_IRQn);
    
    /* Logging - lowest priority */
    NVIC_SetPriority(UART_IRQn, LOGGING_PRIORITY);
    NVIC_EnableIRQ(UART_IRQn);
}

/* Emergency stop - highest priority, no FreeRTOS calls */
void EXTI0_IRQHandler(void)
{
    /* This ISR cannot be preempted and cannot call FreeRTOS APIs */
    /* Direct hardware control only */
    
    MOTOR_PORT->BSRR = MOTOR_DISABLE_PIN;  /* Stop motor immediately */
    systemData.emergencyStop = true;        /* Set flag (atomic on Cortex-M) */
    
    /* Cannot use FreeRTOS functions here! */
    /* The following would be UNSAFE:
       xSemaphoreGiveFromISR(dataMutex, NULL);  // WRONG!
    */
    
    EXTI->PR = EXTI_PR_PR0;  /* Clear interrupt flag */
}

/* Motor control - can use FreeRTOS APIs */
void TIM1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t motorEvent;
    
    /* This ISR can be preempted by emergency stop */
    /* Can safely call FreeRTOS FromISR functions */
    
    if (TIM1->SR & TIM_SR_UIF)
    {
        /* Check emergency stop flag */
        if (systemData.emergencyStop)
        {
            /* Already stopped by emergency handler */
            TIM1->CR1 &= ~TIM_CR1_CEN;  /* Disable timer */
        }
        else
        {
            /* Normal motor control */
            motorEvent = readMotorEncoder();
            
            /* Send to queue - safe because priority allows FreeRTOS calls */
            xQueueSendFromISR(eventQueue, &motorEvent, 
                            &xHigherPriorityTaskWoken);
        }
        
        TIM1->SR &= ~TIM_SR_UIF;  /* Clear interrupt flag */
    }
    
    /* Request context switch if needed */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Sensor reading - lower priority, can be nested */
void ADC_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t sensorValue;
    
    /* This ISR can be preempted by both emergency stop and motor control */
    
    if (ADC1->SR & ADC_SR_EOC)
    {
        sensorValue = ADC1->DR;  /* Read conversion result */
        
        /* Protect shared data access */
        UBaseType_t savedState = taskENTER_CRITICAL_FROM_ISR();
        systemData.sensorValue = sensorValue;
        taskEXIT_CRITICAL_FROM_ISR(savedState);
        
        /* Notify task if threshold exceeded */
        if (sensorValue > THRESHOLD)
        {
            xSemaphoreGiveFromISR(dataMutex, &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Logging - lowest priority */
void UART_IRQHandler(void)
{
    static char logBuffer[64];
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* This ISR has lowest priority and can be preempted by all others */
    
    if (USART1->SR & USART_SR_TXE)
    {
        /* Can be interrupted at any time during this operation */
        if (logBufferIndex < logBufferSize)
        {
            USART1->DR = logBuffer[logBufferIndex++];
        }
        else
        {
            USART1->CR1 &= ~USART_CR1_TXEIE;  /* Disable TX interrupt */
            xSemaphoreGiveFromISR(txCompleteSemaphore, 
                                &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## Safe Interrupt Nesting Patterns

### Pattern 1: High-Priority Non-RTOS Interrupt with Flag

```c
volatile bool urgentEventFlag = false;

/* High-priority ISR - no FreeRTOS calls */
void HighPriority_IRQHandler(void)
{
    /* Ultra-low latency response */
    handleUrgentHardware();
    
    /* Set flag for lower-priority handler */
    urgentEventFlag = true;
    
    /* Trigger lower-priority interrupt to handle FreeRTOS communication */
    NVIC_SetPendingIRQ(LowPriority_IRQn);
}

/* Lower-priority ISR - can use FreeRTOS */
void LowPriority_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (urgentEventFlag)
    {
        urgentEventFlag = false;
        
        /* Now safe to use FreeRTOS APIs */
        xTaskNotifyFromISR(handlerTask, URGENT_EVENT, 
                          eSetBits, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Pattern 2: Deferred Interrupt Processing

```c
/* Quick ISR that defers work to task */
void FastISR_Handler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    /* Minimal processing in ISR */
    uint32_t timestamp = readHardwareTimer();
    
    /* Wake up handler task */
    xTaskNotifyFromISR(processingTask, timestamp, 
                      eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Task does the heavy lifting */
void processingTask(void *pvParameters)
{
    uint32_t notificationValue;
    
    while (1)
    {
        /* Wait for ISR notification */
        xTaskNotifyWait(0, 0xFFFFFFFF, &notificationValue, portMAX_DELAY);
        
        /* Process data with full FreeRTOS services available */
        performComplexProcessing(notificationValue);
    }
}
```

## Common Pitfalls and Solutions

### Pitfall 1: Calling FreeRTOS APIs from High-Priority Interrupts

```c
/* WRONG - High priority interrupt calling FreeRTOS */
void HighPriorityISR(void)  /* Priority < configMAX_SYSCALL_INTERRUPT_PRIORITY */
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(queue, &data, &xHigherPriorityTaskWoken);  /* CRASH! */
}

/* CORRECT - Use appropriate priority */
void CorrectPriorityISR(void)  /* Priority >= configMAX_SYSCALL_INTERRUPT_PRIORITY */
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(queue, &data, &xHigherPriorityTaskWoken);  /* Safe */
}
```

### Pitfall 2: Incorrect Critical Section Usage

```c
/* WRONG - Using task critical section in ISR */
void BadISR(void)
{
    taskENTER_CRITICAL();  /* Will disable ALL interrupts! */
    sharedData++;
    taskEXIT_CRITICAL();
}

/* CORRECT - Use ISR-specific critical section */
void GoodISR(void)
{
    UBaseType_t savedState = taskENTER_CRITICAL_FROM_ISR();
    sharedData++;
    taskEXIT_CRITICAL_FROM_ISR(savedState);
}
```

### Pitfall 3: Priority Inversion Through Masking

```c
/* Problem: Long critical section blocks higher-priority interrupts */
void ProblematicCode(void)
{
    taskENTER_CRITICAL();
    
    /* Long operation - blocks all maskable interrupts */
    for (int i = 0; i < 10000; i++) {
        complexCalculation();
    }
    
    taskEXIT_CRITICAL();
}

/* Solution: Minimize critical section duration */
void BetterCode(void)
{
    prepareData();  /* Outside critical section */
    
    taskENTER_CRITICAL();
    sharedData = preparedValue;  /* Quick atomic update */
    taskEXIT_CRITICAL();
    
    postProcess();  /* Outside critical section */
}
```

## Architecture-Specific Considerations

### ARM Cortex-M BASEPRI Register

FreeRTOS on Cortex-M uses the BASEPRI register for selective interrupt masking:

```c
/* BASEPRI masks interrupts at or below its value */
/* Value of 0 = no masking */

/* When entering critical section, FreeRTOS sets: */
BASEPRI = configMAX_SYSCALL_INTERRUPT_PRIORITY;

/* This allows interrupts with priority 0 to (threshold-1) to still fire */
/* Interrupts at threshold or lower priority are blocked */
```

## Best Practices Summary

1. **Set priorities carefully**: Reserve lowest numeric priorities (0-configMAX_SYSCALL_INTERRUPT_PRIORITY) for time-critical interrupts that don't need RTOS services

2. **Minimize ISR duration**: Keep all ISRs short; defer processing to tasks when possible

3. **Use FromISR variants**: Always use FreeRTOS functions with "FromISR" suffix in interrupt handlers

4. **Protect shared data**: Use critical sections or mutexes appropriately when accessing data shared between ISRs and tasks

5. **Test interrupt nesting**: Verify that nested interrupts behave correctly under load

6. **Document priorities**: Maintain clear documentation of interrupt priority assignments and their rationale

7. **Avoid long critical sections**: Keep critical sections as short as possible to minimize interrupt latency

This comprehensive understanding of interrupt nesting and priority management is essential for building robust, responsive real-time systems with FreeRTOS.