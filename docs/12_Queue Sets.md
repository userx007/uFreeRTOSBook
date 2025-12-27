# FreeRTOS Queue Sets: Advanced Synchronization for Complex Systems

## Overview

Queue Sets are a powerful FreeRTOS feature that allows a task to block on multiple queues and semaphores simultaneously. Instead of polling multiple synchronization objects or creating separate tasks for each object, a task can wait for data or events from any member of a queue set in a single blocking operation.

## Core Concept

Think of a queue set as a "mailbox monitor" watching multiple mailboxes at once. Rather than checking each mailbox individually (polling), you wait until *any* mailbox receives something, then check which one has mail.

**Key characteristics:**
- A queue set is a collection of queues and/or semaphores
- A task blocks on the queue set, not individual members
- When any member receives data or is given, the task unblocks
- The task learns which member is ready and can then read from it
- Maximum efficiency: single blocking point for multiple event sources

## When to Use Queue Sets

Queue sets excel in scenarios where a single task needs to respond to multiple independent event sources:

1. **Protocol handlers** - Processing data from multiple communication channels
2. **Device drivers** - Monitoring multiple hardware interfaces
3. **User interface tasks** - Responding to various input sources (buttons, touch, network)
4. **System monitors** - Watching multiple subsystems for status changes
5. **Multi-source data processors** - Collecting data from diverse sensors or sources

## Implementation Patterns

### Basic Queue Set Structure

A typical queue set implementation follows this pattern:

1. Create the queues and/or semaphores
2. Create the queue set (size = sum of all member lengths)
3. Add members to the queue set
4. Block on the queue set
5. Identify which member is ready
6. Read from the appropriate member
7. Repeat

### Pattern 1: Multi-Channel Communication Handler

This pattern demonstrates handling data from multiple communication interfaces:

```c
#define QUEUE_SET_SIZE 15  // Total capacity of all members
#define UART_QUEUE_LENGTH 5
#define SPI_QUEUE_LENGTH 5
#define I2C_QUEUE_LENGTH 5

// Data structures for different interfaces
typedef struct {
    uint8_t data[64];
    uint16_t length;
} UartMessage_t;

typedef struct {
    uint8_t deviceAddr;
    uint8_t data[32];
} SpiMessage_t;

typedef struct {
    uint8_t slaveAddr;
    uint8_t data[16];
} I2cMessage_t;

// Global handles
QueueSetHandle_t xCommQueueSet;
QueueHandle_t xUartQueue;
QueueHandle_t xSpiQueue;
QueueHandle_t xI2cQueue;

void vCreateCommSystem(void) {
    // Create individual queues
    xUartQueue = xQueueCreate(UART_QUEUE_LENGTH, sizeof(UartMessage_t));
    xSpiQueue = xQueueCreate(SPI_QUEUE_LENGTH, sizeof(SpiMessage_t));
    xI2cQueue = xQueueCreate(I2C_QUEUE_LENGTH, sizeof(I2cMessage_t));
    
    // Create queue set with total capacity
    xCommQueueSet = xQueueCreateSet(QUEUE_SET_SIZE);
    
    // Add queues to the set
    xQueueAddToSet(xUartQueue, xCommQueueSet);
    xQueueAddToSet(xSpiQueue, xCommQueueSet);
    xQueueAddToSet(xI2cQueue, xCommQueueSet);
    
    // Create handler task
    xTaskCreate(vCommHandlerTask, "CommHandler", 1000, NULL, 3, NULL);
}

void vCommHandlerTask(void *pvParameters) {
    QueueSetMemberHandle_t xActiveMember;
    UartMessage_t uartMsg;
    SpiMessage_t spiMsg;
    I2cMessage_t i2cMsg;
    
    for (;;) {
        // Block until any queue receives data
        xActiveMember = xQueueSelectFromSet(xCommQueueSet, portMAX_DELAY);
        
        // Determine which queue has data
        if (xActiveMember == xUartQueue) {
            if (xQueueReceive(xUartQueue, &uartMsg, 0) == pdPASS) {
                // Process UART data
                processUartData(&uartMsg);
            }
        }
        else if (xActiveMember == xSpiQueue) {
            if (xQueueReceive(xSpiQueue, &spiMsg, 0) == pdPASS) {
                // Process SPI data
                processSpiData(&spiMsg);
            }
        }
        else if (xActiveMember == xI2cQueue) {
            if (xQueueReceive(xI2cQueue, &i2cMsg, 0) == pdPASS) {
                // Process I2C data
                processI2cData(&i2cMsg);
            }
        }
    }
}

// ISR or other task sends to queues
void UART_IRQHandler(void) {
    UartMessage_t msg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Read UART data into msg
    readUartData(&msg);
    
    // Send to queue (which is in a queue set)
    xQueueSendFromISR(xUartQueue, &msg, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Pattern 2: Combining Queues and Semaphores

Queue sets can mix queues and semaphores for event-driven architectures:

```c
// Sensor monitoring system
QueueSetHandle_t xSensorQueueSet;
QueueHandle_t xTemperatureQueue;
QueueHandle_t xPressureQueue;
SemaphoreHandle_t xAlarmSemaphore;
SemaphoreHandle_t xCalibrationSemaphore;

#define TEMP_QUEUE_LENGTH 10
#define PRESSURE_QUEUE_LENGTH 10
#define QUEUE_SET_SIZE 22  // 10 + 10 + 1 + 1 (semaphores count as 1)

typedef struct {
    float value;
    uint32_t timestamp;
} SensorData_t;

void vCreateSensorSystem(void) {
    // Create queues for sensor data
    xTemperatureQueue = xQueueCreate(TEMP_QUEUE_LENGTH, sizeof(SensorData_t));
    xPressureQueue = xQueueCreate(PRESSURE_QUEUE_LENGTH, sizeof(SensorData_t));
    
    // Create binary semaphores for events
    xAlarmSemaphore = xSemaphoreCreateBinary();
    xCalibrationSemaphore = xSemaphoreCreateBinary();
    
    // Create and populate queue set
    xSensorQueueSet = xQueueCreateSet(QUEUE_SET_SIZE);
    xQueueAddToSet(xTemperatureQueue, xSensorQueueSet);
    xQueueAddToSet(xPressureQueue, xSensorQueueSet);
    xQueueAddToSet(xAlarmSemaphore, xSensorQueueSet);
    xQueueAddToSet(xCalibrationSemaphore, xSensorQueueSet);
    
    xTaskCreate(vSensorMonitorTask, "SensorMonitor", 1024, NULL, 2, NULL);
}

void vSensorMonitorTask(void *pvParameters) {
    QueueSetMemberHandle_t xActiveMember;
    SensorData_t sensorData;
    
    for (;;) {
        // Wait for any event or data
        xActiveMember = xQueueSelectFromSet(xSensorQueueSet, portMAX_DELAY);
        
        if (xActiveMember == xTemperatureQueue) {
            xQueueReceive(xTemperatureQueue, &sensorData, 0);
            logTemperature(sensorData.value, sensorData.timestamp);
            checkTemperatureThresholds(sensorData.value);
        }
        else if (xActiveMember == xPressureQueue) {
            xQueueReceive(xPressureQueue, &sensorData, 0);
            logPressure(sensorData.value, sensorData.timestamp);
            checkPressureThresholds(sensorData.value);
        }
        else if (xActiveMember == xAlarmSemaphore) {
            xSemaphoreTake(xAlarmSemaphore, 0);
            handleAlarmCondition();
            // Trigger emergency shutdown or notification
        }
        else if (xActiveMember == xCalibrationSemaphore) {
            xSemaphoreTake(xCalibrationSemaphore, 0);
            performSensorCalibration();
            // Recalibrate all sensors
        }
    }
}
```

### Pattern 3: Priority-Based Processing

Implementing priority handling by checking high-priority sources first:

```c
void vSmartProcessorTask(void *pvParameters) {
    QueueSetMemberHandle_t xActiveMember;
    
    for (;;) {
        xActiveMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);
        
        // Always check critical queue first, even if another was signaled
        if (uxQueueMessagesWaiting(xCriticalQueue) > 0) {
            CriticalMsg_t criticalMsg;
            xQueueReceive(xCriticalQueue, &criticalMsg, 0);
            processCritical(&criticalMsg);
        }
        // Then check which member actually triggered
        else if (xActiveMember == xNormalQueue) {
            NormalMsg_t normalMsg;
            xQueueReceive(xNormalQueue, &normalMsg, 0);
            processNormal(&normalMsg);
        }
        else if (xActiveMember == xLowPriorityQueue) {
            LowPriorityMsg_t lowMsg;
            xQueueReceive(xLowPriorityQueue, &lowMsg, 0);
            processLowPriority(&lowMsg);
        }
    }
}
```

### Pattern 4: Network Protocol Handler

Real-world example of handling multiple network protocols:

```c
#define MAX_PACKET_SIZE 1500

typedef enum {
    PROTOCOL_TCP,
    PROTOCOL_UDP,
    PROTOCOL_ICMP
} ProtocolType_t;

typedef struct {
    ProtocolType_t protocol;
    uint8_t data[MAX_PACKET_SIZE];
    uint16_t length;
    uint32_t sourceIP;
} NetworkPacket_t;

QueueSetHandle_t xNetworkQueueSet;
QueueHandle_t xTcpQueue;
QueueHandle_t xUdpQueue;
QueueHandle_t xIcmpQueue;
SemaphoreHandle_t xLinkStatusSemaphore;

void vNetworkHandlerTask(void *pvParameters) {
    QueueSetMemberHandle_t xActiveMember;
    NetworkPacket_t packet;
    
    for (;;) {
        xActiveMember = xQueueSelectFromSet(xNetworkQueueSet, pdMS_TO_TICKS(100));
        
        if (xActiveMember == NULL) {
            // Timeout - perform housekeeping
            performNetworkMaintenance();
            continue;
        }
        
        if (xActiveMember == xTcpQueue) {
            xQueueReceive(xTcpQueue, &packet, 0);
            processTcpPacket(&packet);
            updateTcpStatistics();
        }
        else if (xActiveMember == xUdpQueue) {
            xQueueReceive(xUdpQueue, &packet, 0);
            processUdpPacket(&packet);
        }
        else if (xActiveMember == xIcmpQueue) {
            xQueueReceive(xIcmpQueue, &packet, 0);
            processIcmpPacket(&packet);
        }
        else if (xActiveMember == xLinkStatusSemaphore) {
            xSemaphoreTake(xLinkStatusSemaphore, 0);
            handleLinkStatusChange();
            reinitializeNetworkStack();
        }
    }
}
```

## Important Implementation Details

### Queue Set Sizing

The queue set size must equal the sum of all member queue lengths plus the number of semaphores:

```c
// Wrong - insufficient size
xQueueSet = xQueueCreateSet(10);  // Will fail to add all members

// Correct
#define Q1_LEN 5
#define Q2_LEN 8
#define NUM_SEMAPHORES 2
xQueueSet = xQueueCreateSet(Q1_LEN + Q2_LEN + NUM_SEMAPHORES);
```

### Reading After Selection

Always read from the selected queue with a timeout of 0, as the data is guaranteed to be available:

```c
xActiveMember = xQueueSelectFromSet(xQueueSet, portMAX_DELAY);

// Correct - no blocking needed
if (xActiveMember == xQueue1) {
    xQueueReceive(xQueue1, &data, 0);  // 0 timeout
}

// Inefficient but safe
if (xActiveMember == xQueue1) {
    xQueueReceive(xQueue1, &data, portMAX_DELAY);  // Unnecessary blocking
}
```

### Queue Set Immutability

Once data is in a queue that's part of a queue set, don't add or remove members:

```c
// Safe - modify empty queues
xQueueAddToSet(xNewQueue, xQueueSet);

// Dangerous - queue has data
xQueueSend(xQueue1, &data, 0);
xQueueRemoveFromSet(xQueue1, xQueueSet);  // May cause issues
```

## Advantages and Trade-offs

**Advantages:**
- Single blocking point reduces task overhead
- Cleaner code than polling or multiple tasks
- Efficient CPU usage
- Scalable to many event sources
- Deterministic response time

**Trade-offs:**
- Requires FreeRTOS configuration: `configUSE_QUEUE_SETS` must be 1
- Slightly more complex than simple queue operations
- Fixed size requires planning
- Cannot dynamically add/remove members safely with pending data
- Small memory overhead for the queue set structure

## Configuration Requirements

Enable queue sets in `FreeRTOSConfig.h`:

```c
#define configUSE_QUEUE_SETS 1
```

## Common Pitfalls

1. **Forgetting to read after selection** - The queue set only signals which member is ready; you must still read from it
2. **Incorrect sizing** - Queue set must accommodate all possible pending items
3. **Blocking on individual members** - Never block on members directly; always block on the set
4. **Dynamic modification** - Avoid adding/removing members during operation

## Conclusion

Queue sets are essential for building responsive, efficient systems that handle multiple asynchronous event sources. They eliminate the need for polling or creating multiple tasks, providing a clean architectural pattern for complex synchronization scenarios. When your system needs to monitor multiple communication channels, hardware interfaces, or event sources simultaneously, queue sets offer an elegant and performant solution.