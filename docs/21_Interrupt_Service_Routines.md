# Interrupt Service Routines (ISRs) in FreeRTOS

## Overview

Interrupt Service Routines (ISRs) are critical functions that execute in response to hardware interrupts. In FreeRTOS, ISRs require special handling because they operate outside the normal task scheduling context and have strict timing requirements. Understanding how to write FreeRTOS-compatible ISRs is essential for building responsive real-time systems.

## Key Concepts

### Why ISRs Need Special Treatment

ISRs execute asynchronously when hardware events occur, interrupting whatever task or code is currently running. This creates several challenges:

- **Context**: ISRs run outside the normal task context, so regular FreeRTOS API functions cannot be used
- **Timing**: ISRs must execute quickly to avoid blocking other interrupts and degrading system responsiveness
- **Thread Safety**: ISRs can preempt tasks at any point, requiring careful synchronization
- **Stack Usage**: ISRs typically use a separate interrupt stack with limited space

### The FromISR() API Variants

FreeRTOS provides special API functions suffixed with `FromISR()` that are interrupt-safe. These functions differ from their regular counterparts in several ways:

- They don't block or wait
- They use a different mechanism for context switching
- They accept a special parameter `pxHigherPriorityTaskWoken` to optimize context switches
- They're generally faster and more deterministic

Common FromISR() functions include:
- `xQueueSendFromISR()` / `xQueueReceiveFromISR()`
- `xSemaphoreGiveFromISR()` / `xSemaphoreTakeFromISR()`
- `xEventGroupSetBitsFromISR()`
- `xTimerStartFromISR()` / `xTimerStopFromISR()`
- `vTaskNotifyGiveFromISR()` / `xTaskNotifyFromISR()`

## Interrupt Priorities and FreeRTOS

### Priority Levels

FreeRTOS divides interrupts into two categories based on priority:

**1. FreeRTOS-Managed Interrupts (Lower Priority)**
- Priority at or below `configMAX_SYSCALL_INTERRUPT_PRIORITY`
- Can safely call FreeRTOS FromISR() APIs
- May be disabled during critical sections
- Used for interrupts that need to interact with FreeRTOS tasks

**2. Unmanaged Interrupts (Higher Priority)**
- Priority above `configMAX_SYSCALL_INTERRUPT_PRIORITY`
- Cannot call any FreeRTOS API functions
- Never disabled by FreeRTOS (always responsive)
- Used for extremely time-critical events

The priority numbering is architecture-dependent. On ARM Cortex-M, **lower numeric values mean higher priority**.

## Writing FreeRTOS-Compatible ISRs

### Basic Structure

Here's the canonical structure for a FreeRTOS ISR:

```c
void USART1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Clear the interrupt flag (hardware-specific)
    if (USART1->SR & USART_SR_RXNE)
    {
        // Read data to clear flag
        uint8_t receivedData = USART1->DR;
        
        // Send data to queue
        xQueueSendFromISR(xUARTQueue, &receivedData, &xHigherPriorityTaskWoken);
    }
    
    // Request context switch if a higher priority task was woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### The xHigherPriorityTaskWoken Mechanism

This parameter optimizes context switching:

1. Initialize to `pdFALSE` at the start of the ISR
2. Pass its address to FromISR() functions
3. If the API call unblocks a task with higher priority than the current task, it sets this to `pdTRUE`
4. At the end of the ISR, call `portYIELD_FROM_ISR()` with this value
5. If `pdTRUE`, a context switch occurs immediately when the ISR exits

This ensures the highest priority ready task runs as soon as possible.

## Practical Examples

### Example 1: UART Receive with Queue

This example shows how to handle UART reception and pass data to a task:

```c
// Global queue handle
QueueHandle_t xUARTQueue;

// Task that processes UART data
void vUARTProcessingTask(void *pvParameters)
{
    uint8_t receivedByte;
    
    for (;;)
    {
        // Block waiting for data from ISR
        if (xQueueReceive(xUARTQueue, &receivedByte, portMAX_DELAY) == pdTRUE)
        {
            // Process the received byte
            processData(receivedByte);
        }
    }
}

// UART ISR
void USART2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t data;
    
    // Check if receive register not empty
    if (USART2->SR & USART_SR_RXNE)
    {
        data = USART2->DR;  // Reading DR clears RXNE flag
        
        // Send to queue (don't block if queue is full)
        xQueueSendFromISR(xUARTQueue, &data, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Initialization
void setup(void)
{
    // Create queue for 100 bytes
    xUARTQueue = xQueueCreate(100, sizeof(uint8_t));
    
    // Create processing task
    xTaskCreate(vUARTProcessingTask, "UART", 200, NULL, 2, NULL);
    
    // Configure UART hardware and enable interrupt
    // ... hardware setup code ...
}
```

### Example 2: Binary Semaphore for Event Signaling

Using a semaphore to signal time-critical events:

```c
SemaphoreHandle_t xButtonSemaphore;

// Button processing task
void vButtonTask(void *pvParameters)
{
    for (;;)
    {
        // Block until button press occurs
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE)
        {
            // Debounce delay
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // Process button press
            handleButtonPress();
        }
    }
}

// External interrupt ISR for button
void EXTI0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Check if the interrupt is from our line
    if (EXTI->PR & EXTI_PR_PR0)
    {
        // Clear the pending bit
        EXTI->PR = EXTI_PR_PR0;
        
        // Give semaphore to unblock task
        xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Setup
void setup(void)
{
    xButtonSemaphore = xSemaphoreCreateBinary();
    xTaskCreate(vButtonTask, "Button", 128, NULL, 3, NULL);
    
    // Configure GPIO and EXTI for button
    // ... hardware configuration ...
}
```

### Example 3: Task Notification (Most Efficient Method)

Task notifications are the fastest and most memory-efficient way to signal tasks from ISRs:

```c
TaskHandle_t xADCTaskHandle;

// ADC conversion complete task
void vADCTask(void *pvParameters)
{
    uint32_t ulNotificationValue;
    
    for (;;)
    {
        // Wait for notification from ADC ISR
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        if (ulNotificationValue > 0)
        {
            // Read ADC result
            uint16_t adcValue = ADC1->DR;
            
            // Process the ADC reading
            processADCValue(adcValue);
        }
    }
}

// ADC conversion complete ISR
void ADC_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Check end of conversion flag
    if (ADC1->SR & ADC_SR_EOC)
    {
        // Clear flag (often done by reading DR)
        ADC1->SR &= ~ADC_SR_EOC;
        
        // Notify task (increment notification value)
        vTaskNotifyGiveFromISR(xADCTaskHandle, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Setup
void setup(void)
{
    xTaskCreate(vADCTask, "ADC", 128, NULL, 2, &xADCTaskHandle);
    
    // Configure ADC and enable interrupt
    // ... ADC configuration ...
}
```

### Example 4: Timer Interrupt with Deferred Processing

Complex processing deferred from a timer interrupt:

```c
TaskHandle_t xPeriodicTaskHandle;

// Periodic processing task
void vPeriodicTask(void *pvParameters)
{
    const uint32_t PROCESS_BIT = (1 << 0);
    uint32_t ulNotificationValue;
    
    for (;;)
    {
        // Wait for notification with bits
        xTaskNotifyWait(0, PROCESS_BIT, &ulNotificationValue, portMAX_DELAY);
        
        if (ulNotificationValue & PROCESS_BIT)
        {
            // Perform time-consuming periodic processing
            performSensorFusion();
            updateDisplayData();
            logStatistics();
        }
    }
}

// Timer ISR (fires every 10ms)
void TIM2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Check update interrupt flag
    if (TIM2->SR & TIM_SR_UIF)
    {
        // Clear flag
        TIM2->SR &= ~TIM_SR_UIF;
        
        // Do minimal work in ISR
        static uint8_t counter = 0;
        counter++;
        
        // Every 100ms (10ms * 10), notify task
        if (counter >= 10)
        {
            counter = 0;
            
            // Set bit 0 in task's notification value
            xTaskNotifyFromISR(xPeriodicTaskHandle,
                             (1 << 0),
                             eSetBits,
                             &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Example 5: Configuring Interrupt Priorities (ARM Cortex-M)

Proper priority configuration is crucial:

```c
// FreeRTOSConfig.h settings
#define configPRIO_BITS 4  // 4 priority bits on Cortex-M4
#define configKERNEL_INTERRUPT_PRIORITY (15 << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << (8 - configPRIO_BITS))

// In your initialization code
void setupInterrupts(void)
{
    // Critical interrupt - higher than FreeRTOS syscall priority
    // Cannot call FreeRTOS APIs, but never delayed
    NVIC_SetPriority(TIM1_IRQn, 3);  // Priority 3 (high)
    
    // FreeRTOS-managed interrupt - can call FromISR() functions
    NVIC_SetPriority(USART1_IRQn, 7);  // Priority 7 (medium)
    NVIC_SetPriority(EXTI0_IRQn, 10);  // Priority 10 (lower)
    
    // Enable interrupts
    NVIC_EnableIRQ(TIM1_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

// Unmanaged ISR (highest priority) - NO FreeRTOS APIs
void TIM1_IRQHandler(void)
{
    // Clear interrupt
    TIM1->SR &= ~TIM_SR_UIF;
    
    // Direct hardware control only
    GPIOA->BSRR = GPIO_BSRR_BS0;  // Set pin high immediately
    
    // NO FreeRTOS API calls allowed here!
}
```

## Best Practices

### Keep ISRs Short
Process only essential work in the ISR and defer complex operations to tasks:

```c
// BAD: Too much work in ISR
void UART_IRQHandler(void)
{
    uint8_t data = UART->DR;
    processData(data);        // Complex processing
    updateDisplay();          // Time-consuming
    calculateChecksum();      // Not time-critical
    portYIELD_FROM_ISR(pdFALSE);
}

// GOOD: Minimal ISR, defer to task
void UART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t data = UART->DR;
    xQueueSendFromISR(xQueue, &data, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Always Clear Interrupt Flags
Failure to clear flags causes infinite interrupt loops:

```c
void DMA1_Channel1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Always clear flags before processing
    if (DMA1->ISR & DMA_ISR_TCIF1)
    {
        DMA1->IFCR = DMA_IFCR_CTCIF1;  // Clear transfer complete flag
        
        // Now process the event
        xSemaphoreGiveFromISR(xDMASemaphore, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Handle Queue Full Conditions
Don't assume queues always have space:

```c
void SENSOR_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    SensorData_t data;
    
    readSensorData(&data);
    
    // Check if send succeeded
    if (xQueueSendFromISR(xSensorQueue, &data, &xHigherPriorityTaskWoken) != pdPASS)
    {
        // Queue full - handle error (increment counter, set flag, etc.)
        sensorOverrunCount++;
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Critical Sections in ISRs
Use taskENTER_CRITICAL_FROM_ISR() for ISR-specific critical sections:

```c
void IRQHandler(void)
{
    UBaseType_t uxSavedInterruptStatus;
    
    // Save interrupt status and disable interrupts at this priority and below
    uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    
    // Critical section - access shared data
    sharedVariable++;
    
    // Restore interrupt status
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
    
    // Continue with normal ISR processing...
}
```

## Common Pitfalls

1. **Using blocking API calls in ISRs** - Never use functions without FromISR suffix
2. **Forgetting portYIELD_FROM_ISR()** - Delays task switching until next tick
3. **Not initializing xHigherPriorityTaskWoken** - Results in undefined behavior
4. **Incorrect priority configuration** - Interrupts that need FreeRTOS APIs set too high
5. **Excessive ISR duration** - Blocks other interrupts and degrades responsiveness
6. **Not protecting shared data** - Race conditions between tasks and ISRs

## Summary

FreeRTOS ISRs require careful attention to several key aspects: using the correct FromISR() API variants, properly managing interrupt priorities relative to `configMAX_SYSCALL_INTERRUPT_PRIORITY`, keeping ISR execution time minimal by deferring processing to tasks, and utilizing the `xHigherPriorityTaskWoken` mechanism for efficient context switching. By following these principles and using appropriate synchronization primitives like queues, semaphores, or task notifications, you can build responsive real-time systems that effectively handle asynchronous hardware events while maintaining deterministic behavior.