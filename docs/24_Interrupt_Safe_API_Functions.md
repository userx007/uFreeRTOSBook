# Interrupt-Safe API Functions in FreeRTOS

## Overview

FreeRTOS provides two distinct sets of API functions: standard APIs for use in task context and interrupt-safe variants specifically designed for Interrupt Service Routines (ISRs). Understanding this distinction is critical because calling standard FreeRTOS APIs from an ISR can lead to system crashes, corruption, or unpredictable behavior.

## Why Separate ISR APIs?

Standard FreeRTOS APIs may block or perform operations that aren't safe in interrupt context. ISRs must execute quickly and cannot block. The interrupt-safe APIs are designed to be non-blocking and use special mechanisms to defer context switches until the ISR completes.

## Identifying Interrupt-Safe APIs

Interrupt-safe functions follow a naming convention: they end with `FromISR`. For example:

- `xQueueSend()` → `xQueueSendFromISR()`
- `xSemaphoreGive()` → `xSemaphoreGiveFromISR()`
- `xTaskNotifyGive()` → `vTaskNotifyGiveFromISR()`
- `xEventGroupSetBits()` → `xEventGroupSetBitsFromISR()`

## The xHigherPriorityTaskWoken Mechanism

This is the key mechanism that makes interrupt-safe APIs work efficiently. Here's how it functions:

### Purpose
When an ISR unblocks a task (by sending to a queue, giving a semaphore, etc.), that task might have a higher priority than the currently running task. Rather than switching context immediately (which would be inefficient), FreeRTOS uses a flag to track whether a context switch is needed.

### How It Works

1. Declare a variable initialized to `pdFALSE` before calling ISR-safe APIs
2. Pass the address of this variable to each `FromISR` function
3. The function sets it to `pdTRUE` if a higher-priority task was unblocked
4. At the end of the ISR, use `portYIELD_FROM_ISR()` to perform the context switch if needed

## Example 1: Basic Queue Send from ISR

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

QueueHandle_t xDataQueue;

// Task that processes data from the queue
void vProcessingTask(void *pvParameters)
{
    uint32_t receivedValue;
    
    for (;;)
    {
        // Block waiting for data from ISR
        if (xQueueReceive(xDataQueue, &receivedValue, portMAX_DELAY) == pdPASS)
        {
            printf("Received: %lu\n", receivedValue);
            // Process the data...
        }
    }
}

// Interrupt Service Routine (e.g., UART receive)
void UART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t receivedData;
    
    // Read data from hardware register
    receivedData = UART_DATA_REGISTER;
    
    // Send to queue - use FromISR variant
    xQueueSendFromISR(xDataQueue, &receivedData, &xHigherPriorityTaskWoken);
    
    // Force context switch if a higher priority task was woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void main(void)
{
    // Create queue
    xDataQueue = xQueueCreate(10, sizeof(uint32_t));
    
    // Create processing task
    xTaskCreate(vProcessingTask, "Processor", 1000, NULL, 2, NULL);
    
    // Enable UART interrupt
    NVIC_EnableIRQ(UART_IRQn);
    
    vTaskStartScheduler();
}
```

## Example 2: Binary Semaphore for ISR Synchronization

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

SemaphoreHandle_t xButtonSemaphore;

// Task that handles button press
void vButtonHandlerTask(void *pvParameters)
{
    for (;;)
    {
        // Block until semaphore is given by ISR
        if (xSemaphoreTake(xButtonSemaphore, portMAX_DELAY) == pdTRUE)
        {
            printf("Button pressed - handling event\n");
            // Debounce and process button press
            vTaskDelay(pdMS_TO_TICKS(50)); // Simple debounce
        }
    }
}

// Button interrupt handler
void BUTTON_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Clear interrupt flag
    BUTTON_CLEAR_FLAG();
    
    // Give semaphore to unblock handler task
    xSemaphoreGiveFromISR(xButtonSemaphore, &xHigherPriorityTaskWoken);
    
    // Context switch if needed
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void main(void)
{
    // Create binary semaphore
    xButtonSemaphore = xSemaphoreCreateBinary();
    
    // Create handler task with high priority
    xTaskCreate(vButtonHandlerTask, "ButtonHandler", 1000, NULL, 3, NULL);
    
    // Configure and enable button interrupt
    NVIC_EnableIRQ(BUTTON_IRQn);
    
    vTaskStartScheduler();
}
```

## Example 3: Task Notifications from ISR

Task notifications are the most efficient synchronization mechanism when you only need to wake one specific task.

```c
#include "FreeRTOS.h"
#include "task.h"

TaskHandle_t xDataProcessorHandle = NULL;

// High-priority task that processes ADC data
void vADCProcessorTask(void *pvParameters)
{
    uint32_t notificationValue;
    
    for (;;)
    {
        // Wait for notification from ADC ISR
        notificationValue = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        if (notificationValue > 0)
        {
            // Read and process ADC data
            uint16_t adcValue = ADC_READ_RESULT();
            printf("ADC Value: %u\n", adcValue);
            
            // Process the data...
        }
    }
}

// ADC conversion complete interrupt
void ADC_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Notify the task (increment notification value)
    vTaskNotifyGiveFromISR(xDataProcessorHandle, &xHigherPriorityTaskWoken);
    
    // Clear interrupt flag
    ADC_CLEAR_FLAG();
    
    // Yield if necessary
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void main(void)
{
    // Create task and save handle
    xTaskCreate(vADCProcessorTask, "ADCProc", 1000, NULL, 4, &xDataProcessorHandle);
    
    // Configure ADC and enable interrupt
    NVIC_EnableIRQ(ADC_IRQn);
    
    vTaskStartScheduler();
}
```

## Example 4: Multiple ISR APIs with Single Context Switch

When calling multiple `FromISR` functions in one ISR, you use the same variable for all calls and only check at the end.

```c
QueueHandle_t xQueue1, xQueue2;
SemaphoreHandle_t xSemaphore;

void COMPLEX_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t data1 = 100, data2 = 200;
    
    // Multiple operations - use same flag
    xQueueSendFromISR(xQueue1, &data1, &xHigherPriorityTaskWoken);
    xQueueSendFromISR(xQueue2, &data2, &xHigherPriorityTaskWoken);
    xSemaphoreGiveFromISR(xSemaphore, &xHigherPriorityTaskWoken);
    
    // Single context switch check at the end
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## Common Interrupt-Safe APIs

### Queue Operations
- `xQueueSendFromISR()` / `xQueueSendToBackFromISR()`
- `xQueueSendToFrontFromISR()`
- `xQueueReceiveFromISR()`
- `xQueueOverwriteFromISR()`

### Semaphore Operations
- `xSemaphoreGiveFromISR()`
- `xSemaphoreTakeFromISR()` (rarely used)

### Task Notifications
- `vTaskNotifyGiveFromISR()`
- `xTaskNotifyFromISR()`
- `xTaskNotifyAndQueryFromISR()`

### Event Groups
- `xEventGroupSetBitsFromISR()`

### Timer Operations
- `xTimerPendFunctionCallFromISR()`
- `xTimerStartFromISR()` / `xTimerStopFromISR()`
- `xTimerChangePeriodFromISR()`
- `xTimerResetFromISR()`

## Important Considerations

### ISR Priority Configuration
FreeRTOS-aware interrupts must be at or below `configMAX_SYSCALL_INTERRUPT_PRIORITY`. Interrupts above this priority cannot safely call FreeRTOS APIs at all.

```c
// In FreeRTOSConfig.h
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  (5)

// In your interrupt configuration
NVIC_SetPriority(UART_IRQn, 6);  // Can use FreeRTOS APIs (lower priority)
NVIC_SetPriority(CRITICAL_IRQn, 3);  // CANNOT use FreeRTOS APIs (higher priority)
```

### Keep ISRs Short
Even with `FromISR` APIs, keep interrupt handlers as brief as possible. Move actual processing to tasks.

### Never Block in ISR
Functions like `vTaskDelay()`, `xQueueReceive()` with timeout, or any blocking operation must never be called from an ISR.

### Return Values
Some `FromISR` functions return `pdPASS`/`pdFAIL` to indicate success. Always check these when it matters for your application logic.

## Example 5: Real-World Sensor Data Collection System

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define SENSOR_QUEUE_LENGTH 20

typedef struct {
    uint32_t timestamp;
    uint16_t temperature;
    uint16_t pressure;
} SensorData_t;

QueueHandle_t xSensorQueue;
TaskHandle_t xDataLoggerHandle;

// Data logger task - processes and logs sensor readings
void vDataLoggerTask(void *pvParameters)
{
    SensorData_t sensorReading;
    
    for (;;)
    {
        if (xQueueReceive(xSensorQueue, &sensorReading, portMAX_DELAY) == pdPASS)
        {
            // Log to SD card, send over network, etc.
            printf("[%lu] Temp: %u, Press: %u\n", 
                   sensorReading.timestamp,
                   sensorReading.temperature,
                   sensorReading.pressure);
        }
    }
}

// Timer interrupt - samples sensors at regular intervals
void TIMER_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    SensorData_t reading;
    
    // Read sensors
    reading.timestamp = xTaskGetTickCountFromISR();
    reading.temperature = READ_TEMP_SENSOR();
    reading.pressure = READ_PRESSURE_SENSOR();
    
    // Send to queue - if full, we'll lose this sample
    if (xQueueSendFromISR(xSensorQueue, &reading, &xHigherPriorityTaskWoken) != pdPASS)
    {
        // Queue full - increment error counter or set flag
    }
    
    // Clear timer interrupt
    TIMER_CLEAR_FLAG();
    
    // Context switch if logger task was waiting
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void main(void)
{
    // Create queue for sensor data
    xSensorQueue = xQueueCreate(SENSOR_QUEUE_LENGTH, sizeof(SensorData_t));
    
    // Create data logger task
    xTaskCreate(vDataLoggerTask, "DataLogger", 2000, NULL, 2, &xDataLoggerHandle);
    
    // Configure timer for periodic sampling
    TIMER_INIT();
    NVIC_EnableIRQ(TIMER_IRQn);
    
    vTaskStartScheduler();
}
```

## Forcing Context Switches

The `portYIELD_FROM_ISR()` macro is what actually triggers the context switch. On ARM Cortex-M processors, this typically sets the PendSV interrupt, which performs the actual context switch after all other ISRs complete.

```c
// Typical implementation concept (port-specific)
#define portYIELD_FROM_ISR(x) \
    if (x != pdFALSE) { \
        portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT; \
    }
```

This deferred context switching is crucial for efficiency and correctness, ensuring that context switches happen at safe points rather than in the middle of interrupt processing.