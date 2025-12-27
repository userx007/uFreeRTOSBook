# Queue Fundamentals in FreeRTOS

## Overview

Queues are the primary mechanism for inter-task communication in FreeRTOS. They provide a safe, RTOS-managed way to pass data between tasks, and from interrupt service routines (ISRs) to tasks. Queues implement a First-In-First-Out (FIFO) data structure, though FreeRTOS also supports sending data to the front of a queue for priority handling.

## Core Concepts

### What is a Queue?

A queue is a data storage buffer that can hold a finite number of fixed-size data items. Tasks and ISRs can write items to the queue (send) and read items from the queue (receive). The queue acts as a safe intermediary, handling all the synchronization and mutual exclusion automatically.

**Key characteristics:**
- **Thread-safe**: Multiple tasks can safely access the same queue
- **Blocking capability**: Tasks can wait for data to arrive or for space to become available
- **Data copying**: FreeRTOS queues work by copying data, not passing pointers (though you can copy pointers)
- **Fixed item size**: All items in a queue must be the same size
- **FIFO ordering**: Items are typically retrieved in the order they were sent

## Creating Queues

### Queue Creation

```c
#include "FreeRTOS.h"
#include "queue.h"

// Queue handle - used to reference the queue
QueueHandle_t xQueue;

// Create a queue that can hold 10 items, each of size uint32_t
void vCreateQueue(void)
{
    xQueue = xQueueCreate(
        10,                    // Queue length (number of items)
        sizeof(uint32_t)       // Size of each item in bytes
    );
    
    if (xQueue == NULL)
    {
        // Queue creation failed - insufficient heap memory
        // Handle error appropriately
    }
    else
    {
        // Queue created successfully
    }
}
```

### Creating Queues for Structures

```c
// Define a data structure for sensor readings
typedef struct
{
    uint8_t sensorID;
    float temperature;
    float humidity;
    uint32_t timestamp;
} SensorData_t;

QueueHandle_t xSensorQueue;

void vCreateSensorQueue(void)
{
    // Create queue for 5 sensor data structures
    xSensorQueue = xQueueCreate(5, sizeof(SensorData_t));
    
    if (xSensorQueue != NULL)
    {
        // Queue ready for use
    }
}
```

## Queue Operations

### Sending to a Queue (xQueueSend)

```c
void vProducerTask(void *pvParameters)
{
    uint32_t valueToSend = 0;
    BaseType_t xStatus;
    
    for (;;)
    {
        // Send data to the queue
        xStatus = xQueueSend(
            xQueue,              // Queue handle
            &valueToSend,        // Pointer to data to send
            pdMS_TO_TICKS(100)   // Block time: wait 100ms if queue full
        );
        
        if (xStatus == pdPASS)
        {
            // Data successfully sent
            valueToSend++;
        }
        else
        {
            // Queue was full, data not sent
            // Handle timeout appropriately
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

### Receiving from a Queue (xQueueReceive)

```c
void vConsumerTask(void *pvParameters)
{
    uint32_t receivedValue;
    BaseType_t xStatus;
    
    for (;;)
    {
        // Receive data from queue
        xStatus = xQueueReceive(
            xQueue,              // Queue handle
            &receivedValue,      // Buffer to receive data
            pdMS_TO_TICKS(500)   // Block time: wait 500ms if queue empty
        );
        
        if (xStatus == pdPASS)
        {
            // Data successfully received
            printf("Received: %lu\n", receivedValue);
        }
        else
        {
            // Queue was empty, no data received
            printf("Queue timeout\n");
        }
    }
}
```

### Peeking at Queue Data (xQueuePeek)

Peeking allows you to read data from a queue without removing it.

```c
void vMonitorTask(void *pvParameters)
{
    uint32_t peekedValue;
    BaseType_t xStatus;
    
    for (;;)
    {
        // Peek at the front item without removing it
        xStatus = xQueuePeek(
            xQueue,
            &peekedValue,
            pdMS_TO_TICKS(100)
        );
        
        if (xStatus == pdPASS)
        {
            printf("Next value in queue: %lu\n", peekedValue);
            // Item is still in the queue for other tasks to receive
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Sending to Front of Queue (xQueueSendToFront)

For priority or urgent messages:

```c
void vUrgentMessageTask(void *pvParameters)
{
    uint32_t urgentData = 0xFFFF;
    
    for (;;)
    {
        // Check for urgent condition
        if (bUrgentConditionDetected())
        {
            // Send to front of queue (LIFO for this item)
            xQueueSendToFront(
                xQueue,
                &urgentData,
                pdMS_TO_TICKS(10)
            );
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Blocking Times

Blocking time determines how long a task will wait if a queue operation cannot complete immediately.

### Common Blocking Time Values

```c
// Don't block at all - return immediately
xQueueSend(xQueue, &data, 0);

// Block for 100 milliseconds
xQueueSend(xQueue, &data, pdMS_TO_TICKS(100));

// Block indefinitely until space is available
xQueueSend(xQueue, &data, portMAX_DELAY);

// Convert different time units
TickType_t xTicksToWait = pdMS_TO_TICKS(500);  // 500 milliseconds
```

### Blocking Behavior Example

```c
void vDemonstrationTask(void *pvParameters)
{
    uint32_t data = 42;
    BaseType_t xStatus;
    
    // Non-blocking send - returns immediately
    xStatus = xQueueSend(xQueue, &data, 0);
    if (xStatus != pdPASS)
    {
        // Queue was full, couldn't send
    }
    
    // Blocking send with timeout
    xStatus = xQueueSend(xQueue, &data, pdMS_TO_TICKS(100));
    if (xStatus != pdPASS)
    {
        // Queue was full for entire 100ms timeout period
    }
    
    // Blocking send indefinitely
    xStatus = xQueueSend(xQueue, &data, portMAX_DELAY);
    // This line only executes after data is successfully sent
    
    vTaskDelete(NULL);
}
```

## Queue Operations from ISRs

ISRs require special non-blocking queue functions that use a different mechanism for task notification.

```c
QueueHandle_t xQueue;

void vSomeISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t isrData = 100;
    
    // Send from ISR - never blocks
    xQueueSendFromISR(
        xQueue,
        &isrData,
        &xHigherPriorityTaskWoken
    );
    
    // If sending to queue unblocked a higher priority task,
    // yield to it immediately
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void vAnotherISR(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t receivedData;
    
    // Receive from ISR - never blocks
    if (xQueueReceiveFromISR(xQueue, &receivedData, &xHigherPriorityTaskWoken) == pdPASS)
    {
        // Process received data quickly
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## Queue Design Patterns

### 1. Producer-Consumer Pattern

Multiple producers sending data to multiple consumers:

```c
QueueHandle_t xDataQueue;

typedef struct
{
    uint8_t producerID;
    uint32_t data;
} DataPacket_t;

// Producer tasks
void vProducerTask(void *pvParameters)
{
    uint8_t myID = (uint8_t)pvParameters;
    DataPacket_t packet;
    
    for (;;)
    {
        packet.producerID = myID;
        packet.data = generateData();
        
        xQueueSend(xDataQueue, &packet, portMAX_DELAY);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Consumer tasks
void vConsumerTask(void *pvParameters)
{
    DataPacket_t packet;
    
    for (;;)
    {
        if (xQueueReceive(xDataQueue, &packet, portMAX_DELAY) == pdPASS)
        {
            processData(packet.producerID, packet.data);
        }
    }
}

void vSetupProducerConsumer(void)
{
    xDataQueue = xQueueCreate(20, sizeof(DataPacket_t));
    
    // Create 3 producers
    xTaskCreate(vProducerTask, "Prod1", 200, (void *)1, 2, NULL);
    xTaskCreate(vProducerTask, "Prod2", 200, (void *)2, 2, NULL);
    xTaskCreate(vProducerTask, "Prod3", 200, (void *)3, 2, NULL);
    
    // Create 2 consumers
    xTaskCreate(vConsumerTask, "Cons1", 200, NULL, 2, NULL);
    xTaskCreate(vConsumerTask, "Cons2", 200, NULL, 2, NULL);
}
```

### 2. Mailbox Pattern (Queue of Length 1)

For holding the most recent value:

```c
QueueHandle_t xMailbox;

typedef struct
{
    float temperature;
    float pressure;
} SensorReading_t;

void vSetupMailbox(void)
{
    // Mailbox is a queue with length 1
    xMailbox = xQueueCreate(1, sizeof(SensorReading_t));
}

void vSensorTask(void *pvParameters)
{
    SensorReading_t reading;
    
    for (;;)
    {
        reading.temperature = readTemperature();
        reading.pressure = readPressure();
        
        // Overwrite old value if mailbox is full (non-blocking)
        xQueueOverwrite(xMailbox, &reading);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vDisplayTask(void *pvParameters)
{
    SensorReading_t reading;
    
    for (;;)
    {
        // Always get the latest reading
        if (xQueuePeek(xMailbox, &reading, portMAX_DELAY) == pdPASS)
        {
            updateDisplay(reading.temperature, reading.pressure);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

### 3. Command Queue Pattern

Centralized command processor:

```c
typedef enum
{
    CMD_START,
    CMD_STOP,
    CMD_RESET,
    CMD_SET_SPEED,
    CMD_GET_STATUS
} CommandType_t;

typedef struct
{
    CommandType_t command;
    uint32_t parameter;
    QueueHandle_t responseQueue;  // For sending responses back
} Command_t;

QueueHandle_t xCommandQueue;

void vCommandProcessor(void *pvParameters)
{
    Command_t cmd;
    uint32_t response;
    
    for (;;)
    {
        if (xQueueReceive(xCommandQueue, &cmd, portMAX_DELAY) == pdPASS)
        {
            switch (cmd.command)
            {
                case CMD_START:
                    startMotor();
                    response = STATUS_OK;
                    break;
                    
                case CMD_STOP:
                    stopMotor();
                    response = STATUS_OK;
                    break;
                    
                case CMD_SET_SPEED:
                    setSpeed(cmd.parameter);
                    response = STATUS_OK;
                    break;
                    
                case CMD_GET_STATUS:
                    response = getSystemStatus();
                    break;
                    
                default:
                    response = STATUS_ERROR;
                    break;
            }
            
            // Send response back if response queue provided
            if (cmd.responseQueue != NULL)
            {
                xQueueSend(cmd.responseQueue, &response, 0);
            }
        }
    }
}

// Task that sends commands
void vControlTask(void *pvParameters)
{
    Command_t cmd;
    QueueHandle_t xResponseQueue;
    uint32_t response;
    
    // Create response queue
    xResponseQueue = xQueueCreate(1, sizeof(uint32_t));
    
    for (;;)
    {
        // Send a command
        cmd.command = CMD_SET_SPEED;
        cmd.parameter = 1500;
        cmd.responseQueue = xResponseQueue;
        
        xQueueSend(xCommandQueue, &cmd, portMAX_DELAY);
        
        // Wait for response
        if (xQueueReceive(xResponseQueue, &response, pdMS_TO_TICKS(1000)) == pdPASS)
        {
            if (response == STATUS_OK)
            {
                // Command successful
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

### 4. Event Logging Pattern

Multiple sources logging to a central logger:

```c
typedef enum
{
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel_t;

typedef struct
{
    LogLevel_t level;
    char message[64];
    uint32_t timestamp;
} LogEntry_t;

QueueHandle_t xLogQueue;

void vLoggerTask(void *pvParameters)
{
    LogEntry_t entry;
    
    for (;;)
    {
        if (xQueueReceive(xLogQueue, &entry, portMAX_DELAY) == pdPASS)
        {
            // Write to log file, UART, etc.
            printf("[%lu] %s: %s\n", 
                   entry.timestamp,
                   getLevelString(entry.level),
                   entry.message);
        }
    }
}

// Helper function for logging (can be called from any task)
void vLog(LogLevel_t level, const char *message)
{
    LogEntry_t entry;
    
    entry.level = level;
    entry.timestamp = xTaskGetTickCount();
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';
    
    // Non-blocking send - if queue full, drop the log
    xQueueSend(xLogQueue, &entry, 0);
}

void vSomeTask(void *pvParameters)
{
    for (;;)
    {
        vLog(LOG_INFO, "Task iteration started");
        
        if (errorCondition())
        {
            vLog(LOG_ERROR, "Error detected in task");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Practical Complete Example: Sensor Data Pipeline

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

// Data structures
typedef struct
{
    uint8_t sensorID;
    float value;
    uint32_t timestamp;
} RawSensorData_t;

typedef struct
{
    uint8_t sensorID;
    float filtered_value;
    float average;
    uint32_t sample_count;
} ProcessedSensorData_t;

// Queue handles
QueueHandle_t xRawDataQueue;
QueueHandle_t xProcessedDataQueue;

// Sensor reading task (simulates 3 sensors)
void vSensorTask(void *pvParameters)
{
    uint8_t sensorID = (uint8_t)pvParameters;
    RawSensorData_t reading;
    
    for (;;)
    {
        // Simulate sensor reading
        reading.sensorID = sensorID;
        reading.value = 20.0f + (rand() % 100) / 10.0f;  // 20-30 range
        reading.timestamp = xTaskGetTickCount();
        
        // Send to processing queue
        if (xQueueSend(xRawDataQueue, &reading, pdMS_TO_TICKS(100)) != pdPASS)
        {
            printf("Sensor %d: Queue full!\n", sensorID);
        }
        
        // Different sampling rates for different sensors
        vTaskDelay(pdMS_TO_TICKS(100 * sensorID));
    }
}

// Data processing task
void vProcessingTask(void *pvParameters)
{
    RawSensorData_t rawData;
    ProcessedSensorData_t processedData;
    static float runningSum[3] = {0};
    static uint32_t sampleCount[3] = {0};
    
    for (;;)
    {
        // Wait for raw data
        if (xQueueReceive(xRawDataQueue, &rawData, portMAX_DELAY) == pdPASS)
        {
            uint8_t id = rawData.sensorID - 1;  // 0-indexed
            
            // Simple filtering (moving average)
            runningSum[id] += rawData.value;
            sampleCount[id]++;
            
            // Prepare processed data
            processedData.sensorID = rawData.sensorID;
            processedData.filtered_value = rawData.value * 0.9f + runningSum[id] / sampleCount[id] * 0.1f;
            processedData.average = runningSum[id] / sampleCount[id];
            processedData.sample_count = sampleCount[id];
            
            // Send to display queue
            xQueueSend(xProcessedDataQueue, &processedData, portMAX_DELAY);
            
            // Reset running average periodically
            if (sampleCount[id] > 100)
            {
                runningSum[id] = 0;
                sampleCount[id] = 0;
            }
        }
    }
}

// Display task
void vDisplayTask(void *pvParameters)
{
    ProcessedSensorData_t data;
    
    for (;;)
    {
        if (xQueueReceive(xProcessedDataQueue, &data, portMAX_DELAY) == pdPASS)
        {
            printf("Sensor %d | Filtered: %.2f | Avg: %.2f | Samples: %lu\n",
                   data.sensorID,
                   data.filtered_value,
                   data.average,
                   data.sample_count);
        }
    }
}

// Monitoring task (uses peek to check queue status)
void vMonitorTask(void *pvParameters)
{
    UBaseType_t uxRawQueueItems, uxProcessedQueueItems;
    
    for (;;)
    {
        uxRawQueueItems = uxQueueMessagesWaiting(xRawDataQueue);
        uxProcessedQueueItems = uxQueueMessagesWaiting(xProcessedDataQueue);
        
        printf("Queue Status - Raw: %u/10 | Processed: %u/5\n",
               uxRawQueueItems, uxProcessedQueueItems);
        
        // Check for queue overflow conditions
        if (uxRawQueueItems > 8)
        {
            printf("WARNING: Raw data queue nearly full!\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int main(void)
{
    // Create queues
    xRawDataQueue = xQueueCreate(10, sizeof(RawSensorData_t));
    xProcessedDataQueue = xQueueCreate(5, sizeof(ProcessedSensorData_t));
    
    if (xRawDataQueue != NULL && xProcessedDataQueue != NULL)
    {
        // Create sensor tasks (3 sensors)
        xTaskCreate(vSensorTask, "Sensor1", 200, (void *)1, 2, NULL);
        xTaskCreate(vSensorTask, "Sensor2", 200, (void *)2, 2, NULL);
        xTaskCreate(vSensorTask, "Sensor3", 200, (void *)3, 2, NULL);
        
        // Create processing task
        xTaskCreate(vProcessingTask, "Process", 300, NULL, 3, NULL);
        
        // Create display task
        xTaskCreate(vDisplayTask, "Display", 200, NULL, 1, NULL);
        
        // Create monitoring task
        xTaskCreate(vMonitorTask, "Monitor", 200, NULL, 1, NULL);
        
        // Start scheduler
        vTaskStartScheduler();
    }
    
    // Should never reach here
    for (;;);
    return 0;
}
```

## Best Practices

1. **Queue Sizing**: Size queues based on worst-case scenarios considering producer/consumer rates and processing delays
2. **Item Size**: Keep queue items small or use pointers to larger data structures (with careful memory management)
3. **Blocking Times**: Choose appropriate blocking times - avoid portMAX_DELAY unless you're certain the queue will eventually have space/data
4. **Error Handling**: Always check return values from queue operations
5. **ISR Usage**: Use FromISR variants in interrupt handlers and keep ISRs short
6. **Memory Management**: Be aware that queues allocate memory from the heap; check for creation failures
7. **Queue Depth**: Monitor queue usage to optimize queue depth and detect performance issues

Queues are fundamental to building robust multitasking applications in FreeRTOS, enabling clean separation of concerns and reliable inter-task communication.