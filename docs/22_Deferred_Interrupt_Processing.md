# Deferred Interrupt Processing in FreeRTOS

## Overview

Deferred interrupt processing is a critical design pattern in real-time systems where interrupt service routines (ISRs) are kept as short as possible by deferring non-critical work to RTOS tasks. This approach maintains low interrupt latency while allowing complex processing to occur in a more flexible task context.

## Why Defer Interrupt Processing?

**Interrupt Context Limitations:**
- ISRs run at high priority, blocking other interrupts
- Limited stack space available
- Cannot call blocking API functions
- Cannot use FreeRTOS APIs that might block
- Long ISRs increase interrupt latency for other interrupts

**Benefits of Deferring:**
- Minimizes time spent with interrupts disabled
- Allows prioritization of deferred work through task priorities
- Enables use of full RTOS API in task context
- Improves system responsiveness and determinism

## Mechanisms for Deferring Work

### 1. Binary Semaphores

Binary semaphores provide a lightweight synchronization mechanism where the ISR "gives" a semaphore and a handler task "takes" it.

**Example: UART Data Reception**

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Global semaphore handle
SemaphoreHandle_t xUartSemaphore;

// Circular buffer for received data
#define BUFFER_SIZE 128
volatile uint8_t rxBuffer[BUFFER_SIZE];
volatile uint16_t rxWriteIndex = 0;
volatile uint16_t rxReadIndex = 0;

// ISR: Minimal processing - just signal the event
void UART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (UART_GetITStatus(UART_IT_RXNE)) {
        // Read data byte
        uint8_t data = UART_ReceiveData();
        
        // Store in buffer
        rxBuffer[rxWriteIndex] = data;
        rxWriteIndex = (rxWriteIndex + 1) % BUFFER_SIZE;
        
        // Signal handler task
        xSemaphoreGiveFromISR(xUartSemaphore, &xHigherPriorityTaskWoken);
        
        // Clear interrupt flag
        UART_ClearITPendingBit(UART_IT_RXNE);
    }
    
    // Perform context switch if needed
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Handler task: Performs the actual processing
void vUartHandlerTask(void *pvParameters)
{
    uint8_t receivedData[BUFFER_SIZE];
    uint16_t dataCount;
    
    while (1) {
        // Wait for ISR to signal data arrival
        if (xSemaphoreTake(xUartSemaphore, portMAX_DELAY) == pdTRUE) {
            // Extract data from circular buffer
            dataCount = 0;
            while (rxReadIndex != rxWriteIndex) {
                receivedData[dataCount++] = rxBuffer[rxReadIndex];
                rxReadIndex = (rxReadIndex + 1) % BUFFER_SIZE;
            }
            
            // Process the data (complex operations allowed here)
            processUartData(receivedData, dataCount);
            
            // Update displays, logs, etc.
            updateUserInterface(receivedData, dataCount);
        }
    }
}

void setupUartDeferred(void)
{
    // Create binary semaphore
    xUartSemaphore = xSemaphoreCreateBinary();
    
    // Create handler task with appropriate priority
    xTaskCreate(vUartHandlerTask, "UART_Handler", 256, NULL, 3, NULL);
    
    // Enable UART interrupt
    UART_ITConfig(UART_IT_RXNE, ENABLE);
}
```

### 2. Task Notifications (More Efficient)

Task notifications are a lightweight, faster alternative to semaphore when one ISR signals one specific task.

**Example: ADC Conversion Complete**

```c
// Task handle for the ADC processing task
TaskHandle_t xAdcTaskHandle = NULL;

// ADC values buffer
#define ADC_CHANNELS 4
volatile uint16_t adcValues[ADC_CHANNELS];

// ISR: Ultra-minimal processing
void ADC_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (ADC_GetITStatus(ADC_IT_EOC)) {
        // Read converted values quickly
        for (int i = 0; i < ADC_CHANNELS; i++) {
            adcValues[i] = ADC_GetConversionValue(i);
        }
        
        // Notify handler task (much faster than semaphore)
        vTaskNotifyGiveFromISR(xAdcTaskHandle, &xHigherPriorityTaskWoken);
        
        ADC_ClearITPendingBit(ADC_IT_EOC);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Handler task with processing logic
void vAdcProcessingTask(void *pvParameters)
{
    uint16_t localValues[ADC_CHANNELS];
    float processedData[ADC_CHANNELS];
    
    while (1) {
        // Wait for notification from ISR (blocks indefinitely)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Copy values atomically
        taskENTER_CRITICAL();
        memcpy(localValues, (const void*)adcValues, sizeof(adcValues));
        taskEXIT_CRITICAL();
        
        // Perform complex calculations
        for (int i = 0; i < ADC_CHANNELS; i++) {
            // Convert to voltage
            processedData[i] = (localValues[i] * 3.3f) / 4096.0f;
            
            // Apply calibration
            processedData[i] = applyCalibratedCurve(processedData[i], i);
            
            // Filter noise
            processedData[i] = applyDigitalFilter(processedData[i], i);
        }
        
        // Update control algorithms
        updatePIDControllers(processedData);
        
        // Log data if needed
        logSensorData(processedData, ADC_CHANNELS);
    }
}

void setupAdcDeferred(void)
{
    // Create processing task and save its handle
    xTaskCreate(vAdcProcessingTask, "ADC_Proc", 512, NULL, 4, &xAdcTaskHandle);
    
    // Configure and enable ADC interrupt
    ADC_ITConfig(ADC_IT_EOC, ENABLE);
    NVIC_EnableIRQ(ADC_IRQn);
}
```

### 3. Queues for Data Transfer

When the ISR needs to pass actual data (not just a signal), queues are the best mechanism.

**Example: Multi-Sensor Data Collection**

```c
// Data structure for sensor readings
typedef struct {
    uint8_t sensorId;
    uint32_t timestamp;
    float value;
    uint8_t flags;
} SensorReading_t;

// Queue handle
QueueHandle_t xSensorQueue;

// External interrupt for sensor 1
void EXTI0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (EXTI_GetITStatus(EXTI_Line0)) {
        SensorReading_t reading;
        
        // Quick data collection
        reading.sensorId = 1;
        reading.timestamp = xTaskGetTickCountFromISR();
        reading.value = readSensor1();
        reading.flags = getSensor1Status();
        
        // Send to queue (non-blocking from ISR)
        xQueueSendFromISR(xSensorQueue, &reading, &xHigherPriorityTaskWoken);
        
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// External interrupt for sensor 2
void EXTI1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (EXTI_GetITStatus(EXTI_Line1)) {
        SensorReading_t reading;
        
        reading.sensorId = 2;
        reading.timestamp = xTaskGetTickCountFromISR();
        reading.value = readSensor2();
        reading.flags = getSensor2Status();
        
        xQueueSendFromISR(xSensorQueue, &reading, &xHigherPriorityTaskWoken);
        
        EXTI_ClearITPendingBit(EXTI_Line1);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Handler task processes all sensor data
void vSensorProcessingTask(void *pvParameters)
{
    SensorReading_t reading;
    
    while (1) {
        // Wait for data from any sensor
        if (xQueueReceive(xSensorQueue, &reading, portMAX_DELAY) == pdTRUE) {
            // Process based on sensor ID
            switch (reading.sensorId) {
                case 1:
                    processSensor1Data(reading.value);
                    checkSensor1Alarms(reading.value, reading.flags);
                    break;
                    
                case 2:
                    processSensor2Data(reading.value);
                    checkSensor2Alarms(reading.value, reading.flags);
                    break;
            }
            
            // Store to database
            storeSensorReading(&reading);
            
            // Update trending
            updateTrendAnalysis(reading.sensorId, reading.value);
        }
    }
}

void setupSensorDeferred(void)
{
    // Create queue (10 readings deep)
    xSensorQueue = xQueueCreate(10, sizeof(SensorReading_t));
    
    // Create processing task
    xTaskCreate(vSensorProcessingTask, "Sensor_Proc", 512, NULL, 3, NULL);
    
    // Enable interrupts
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI1_IRQn);
}
```

## Advanced Pattern: Deferred Interrupt Handler Task

FreeRTOS provides a special utility for creating dedicated interrupt handler tasks.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

// Global task handle
TaskHandle_t xDeferredHandlerTask = NULL;

// Network packet structure
typedef struct {
    uint8_t data[1500];
    uint16_t length;
    uint32_t timestamp;
} NetworkPacket_t;

NetworkPacket_t currentPacket;

// Ethernet interrupt
void ETH_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (ETH_GetDMAITStatus(ETH_DMA_IT_R)) {
        // Read packet metadata
        currentPacket.length = ETH_GetReceivedFrameLength();
        currentPacket.timestamp = xTaskGetTickCountFromISR();
        
        // Quick DMA read into buffer
        ETH_DMARxDesc_GetBuffer(currentPacket.data);
        
        // Notify handler with value indicating packet type
        xTaskNotifyFromISR(xDeferredHandlerTask, 
                          ETH_DMA_IT_R, 
                          eSetBits, 
                          &xHigherPriorityTaskWoken);
        
        ETH_DMAClearITPendingBit(ETH_DMA_IT_R);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Dedicated deferred handler
void vDeferredInterruptHandler(void *pvParameters)
{
    uint32_t ulNotificationValue;
    
    while (1) {
        // Wait for interrupt notification
        xTaskNotifyWait(0x00, 0xFFFFFFFF, &ulNotificationValue, portMAX_DELAY);
        
        // Check which interrupt occurred
        if (ulNotificationValue & ETH_DMA_IT_R) {
            // Process received packet
            parseEthernetFrame(&currentPacket);
            
            // Route to appropriate protocol handler
            if (isIPPacket(&currentPacket)) {
                processIPPacket(&currentPacket);
            } else if (isARPPacket(&currentPacket)) {
                processARPPacket(&currentPacket);
            }
            
            // Update statistics
            updateNetworkStats();
        }
    }
}

void setupEthernetDeferred(void)
{
    // Create high-priority handler task
    xTaskCreate(vDeferredInterruptHandler, 
                "ETH_Handler", 
                1024, 
                NULL, 
                configMAX_PRIORITIES - 1,  // High priority
                &xDeferredHandlerTask);
    
    // Enable Ethernet DMA interrupt
    ETH_DMAITConfig(ETH_DMA_IT_R, ENABLE);
    NVIC_EnableIRQ(ETH_IRQn);
}
```

## Best Practices

**ISR Guidelines:**
1. Keep ISRs under 10-20 microseconds when possible
2. Only read hardware registers and store data
3. Use `FromISR` API variants exclusively
4. Always use `xHigherPriorityTaskWoken` parameter
5. Call `portYIELD_FROM_ISR` at the end

**Handler Task Guidelines:**
1. Set appropriate task priority (usually high but below critical tasks)
2. Use adequate stack size for processing requirements
3. Implement timeout handling where appropriate
4. Consider using task notifications for single-source events
5. Use queues when multiple ISRs feed one handler

**Performance Optimization:**
- Task notifications are ~45% faster than binary semaphores
- Direct-to-task notifications avoid queue overhead
- Counting semaphores can track multiple pending events
- Queue size should accommodate burst scenarios

This pattern is fundamental to building responsive, deterministic real-time systems with FreeRTOS.