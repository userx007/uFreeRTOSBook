# FreeRTOS Message Buffers: Comprehensive Guide

## Overview

Message buffers are a FreeRTOS IPC (Inter-Process Communication) mechanism designed specifically for passing variable-length data between tasks or between an interrupt and a task. They provide a lightweight, efficient way to transfer arbitrary-sized messages without the overhead of traditional queues.

## Key Characteristics

**Message buffers differ from queues in fundamental ways:**

- **Variable-length messages**: Each message can have a different size, whereas queue items are fixed-size
- **FIFO ordering**: Messages are retrieved in the order they were sent
- **Single reader/writer optimization**: Designed primarily for one sender and one receiver
- **Byte-stream abstraction**: Internally manages raw bytes with length information
- **Lightweight**: Less overhead than queues for variable-sized data

## Internal Implementation

Message buffers use a circular buffer with the following structure:

```
[Message 1 Length][Message 1 Data][Message 2 Length][Message 2 Data]...
```

**Key implementation details:**

1. **Length prefix**: Each message is prefixed with its length (typically 4 bytes on 32-bit systems)
2. **Circular buffer**: Uses modulo arithmetic to wrap around when reaching the end
3. **Task notification mechanism**: Uses direct-to-task notifications for efficient blocking/unblocking
4. **Atomic operations**: Head and tail pointers updated atomically to prevent corruption
5. **No memory allocation per message**: Buffer allocated once at creation time

## Creating Message Buffers

```c
#include "FreeRTOS.h"
#include "message_buffer.h"

// Create a message buffer that can hold up to 200 bytes total
#define MESSAGE_BUFFER_SIZE 200
MessageBufferHandle_t xMessageBuffer;

void setup_message_buffer(void)
{
    xMessageBuffer = xMessageBufferCreate(MESSAGE_BUFFER_SIZE);
    
    if (xMessageBuffer == NULL)
    {
        // Failed to create message buffer
        // Handle error
    }
}
```

**Static allocation alternative:**

```c
// For static allocation (no dynamic memory)
#define MESSAGE_BUFFER_SIZE 200
static uint8_t ucBufferStorage[MESSAGE_BUFFER_SIZE + sizeof(StaticMessageBuffer_t)];
static StaticMessageBuffer_t xMessageBufferStruct;

MessageBufferHandle_t xMessageBuffer;

void setup_static_message_buffer(void)
{
    xMessageBuffer = xMessageBufferCreateStatic(
        MESSAGE_BUFFER_SIZE,
        ucBufferStorage,
        &xMessageBufferStruct
    );
}
```

## Sending Messages

```c
void vSenderTask(void *pvParameters)
{
    const char *messages[] = {
        "Short message",
        "This is a longer message with more data",
        "Variable"
    };
    
    size_t xBytesSent;
    
    for (;;)
    {
        for (int i = 0; i < 3; i++)
        {
            size_t message_length = strlen(messages[i]) + 1; // Include null terminator
            
            // Send message with 100ms timeout
            xBytesSent = xMessageBufferSend(
                xMessageBuffer,
                (void *)messages[i],
                message_length,
                pdMS_TO_TICKS(100)
            );
            
            if (xBytesSent != message_length)
            {
                // Failed to send complete message
                // Buffer might be full
            }
            
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}
```

**Sending from ISR:**

```c
void UART_IRQHandler(void)
{
    static uint8_t rxBuffer[64];
    static uint8_t rxIndex = 0;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Assume we've received a complete packet
    uint8_t receivedByte = UART_ReadByte();
    rxBuffer[rxIndex++] = receivedByte;
    
    if (receivedByte == '\n' || rxIndex >= sizeof(rxBuffer))
    {
        // Send the complete packet
        xMessageBufferSendFromISR(
            xMessageBuffer,
            rxBuffer,
            rxIndex,
            &xHigherPriorityTaskWoken
        );
        
        rxIndex = 0; // Reset for next packet
        
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
```

## Receiving Messages

```c
void vReceiverTask(void *pvParameters)
{
    uint8_t rxBuffer[100];
    size_t xReceivedBytes;
    
    for (;;)
    {
        // Wait indefinitely for a message
        xReceivedBytes = xMessageBufferReceive(
            xMessageBuffer,
            rxBuffer,
            sizeof(rxBuffer),
            portMAX_DELAY
        );
        
        if (xReceivedBytes > 0)
        {
            // Process the received message
            rxBuffer[xReceivedBytes] = '\0'; // Null terminate if string
            printf("Received %d bytes: %s\n", xReceivedBytes, rxBuffer);
            
            // Process message...
        }
    }
}
```

## Practical Example: Sensor Data Logger

Here's a complete example showing a sensor system that logs variable-length sensor readings:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"
#include <stdio.h>
#include <string.h>

#define LOGGER_BUFFER_SIZE 512

// Message buffer handle
MessageBufferHandle_t xLoggerMessageBuffer;

// Sensor data structures (variable sizes)
typedef struct
{
    uint8_t sensorId;
    uint32_t timestamp;
    float temperature;
} TemperatureSensor_t;

typedef struct
{
    uint8_t sensorId;
    uint32_t timestamp;
    float x, y, z;
} AccelerometerSensor_t;

typedef struct
{
    uint8_t sensorId;
    uint32_t timestamp;
    uint16_t dataLength;
    uint8_t rawData[32]; // Variable up to 32 bytes
} GenericSensor_t;

// Temperature sensor task
void vTemperatureSensorTask(void *pvParameters)
{
    TemperatureSensor_t tempData;
    uint32_t timestamp = 0;
    
    for (;;)
    {
        // Simulate reading temperature
        tempData.sensorId = 1;
        tempData.timestamp = timestamp++;
        tempData.temperature = 20.0f + (float)(timestamp % 10);
        
        // Send variable-length message
        xMessageBufferSend(
            xLoggerMessageBuffer,
            &tempData,
            sizeof(TemperatureSensor_t),
            pdMS_TO_TICKS(100)
        );
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Accelerometer sensor task
void vAccelerometerSensorTask(void *pvParameters)
{
    AccelerometerSensor_t accelData;
    uint32_t timestamp = 0;
    
    for (;;)
    {
        // Simulate reading accelerometer
        accelData.sensorId = 2;
        accelData.timestamp = timestamp++;
        accelData.x = (float)(timestamp % 100) / 10.0f;
        accelData.y = (float)((timestamp + 30) % 100) / 10.0f;
        accelData.z = 9.8f;
        
        xMessageBufferSend(
            xLoggerMessageBuffer,
            &accelData,
            sizeof(AccelerometerSensor_t),
            pdMS_TO_TICKS(100)
        );
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Generic sensor with variable data length
void vGenericSensorTask(void *pvParameters)
{
    GenericSensor_t genericData;
    uint32_t timestamp = 0;
    
    for (;;)
    {
        // Simulate variable-length sensor data
        genericData.sensorId = 3;
        genericData.timestamp = timestamp++;
        genericData.dataLength = (timestamp % 20) + 5; // 5-24 bytes
        
        for (int i = 0; i < genericData.dataLength; i++)
        {
            genericData.rawData[i] = i + timestamp;
        }
        
        // Send only the actual data (not the full array)
        size_t messageSize = sizeof(uint8_t) + sizeof(uint32_t) + 
                            sizeof(uint16_t) + genericData.dataLength;
        
        xMessageBufferSend(
            xLoggerMessageBuffer,
            &genericData,
            messageSize,
            pdMS_TO_TICKS(100)
        );
        
        vTaskDelay(pdMS_TO_TICKS(750));
    }
}

// Logger task that receives all sensor data
void vLoggerTask(void *pvParameters)
{
    uint8_t rxBuffer[128];
    size_t xReceivedBytes;
    
    for (;;)
    {
        xReceivedBytes = xMessageBufferReceive(
            xLoggerMessageBuffer,
            rxBuffer,
            sizeof(rxBuffer),
            portMAX_DELAY
        );
        
        if (xReceivedBytes > 0)
        {
            uint8_t sensorId = rxBuffer[0];
            uint32_t timestamp = *(uint32_t*)(&rxBuffer[1]);
            
            printf("[LOG] Sensor %d at time %lu: ", sensorId, timestamp);
            
            // Parse based on known sensor types
            if (sensorId == 1 && xReceivedBytes == sizeof(TemperatureSensor_t))
            {
                TemperatureSensor_t *pTemp = (TemperatureSensor_t*)rxBuffer;
                printf("Temperature = %.2f°C\n", pTemp->temperature);
            }
            else if (sensorId == 2 && xReceivedBytes == sizeof(AccelerometerSensor_t))
            {
                AccelerometerSensor_t *pAccel = (AccelerometerSensor_t*)rxBuffer;
                printf("Accel X=%.2f Y=%.2f Z=%.2f\n", 
                       pAccel->x, pAccel->y, pAccel->z);
            }
            else if (sensorId == 3)
            {
                GenericSensor_t *pGeneric = (GenericSensor_t*)rxBuffer;
                printf("Generic data (%d bytes): ", pGeneric->dataLength);
                for (int i = 0; i < pGeneric->dataLength && i < 10; i++)
                {
                    printf("%02X ", pGeneric->rawData[i]);
                }
                printf("\n");
            }
        }
    }
}

// Main application
int main(void)
{
    // Create message buffer
    xLoggerMessageBuffer = xMessageBufferCreate(LOGGER_BUFFER_SIZE);
    
    if (xLoggerMessageBuffer != NULL)
    {
        // Create sensor tasks
        xTaskCreate(vTemperatureSensorTask, "TempSensor", 200, NULL, 2, NULL);
        xTaskCreate(vAccelerometerSensorTask, "AccelSensor", 200, NULL, 2, NULL);
        xTaskCreate(vGenericSensorTask, "GenericSensor", 200, NULL, 2, NULL);
        
        // Create logger task with higher priority
        xTaskCreate(vLoggerTask, "Logger", 300, NULL, 3, NULL);
        
        // Start scheduler
        vTaskStartScheduler();
    }
    
    // Should never reach here
    for (;;);
    return 0;
}
```

## Message Buffers vs Queues: When to Use Each

### Use Message Buffers When:

1. **Variable-length data**: Messages have different sizes (sensor packets, strings, serialized data)
2. **Single reader/writer**: One task sends, one task receives (optimal use case)
3. **Byte-stream data**: Working with raw byte sequences
4. **Memory efficiency**: Want to avoid wasting space on maximum-sized queue items
5. **Simple streaming**: Data flows in one direction

**Example scenarios:**
- UART/serial data packets of varying length
- Logging systems with variable message sizes
- Protocol parsers receiving different packet types
- Audio/video streaming with variable frame sizes

### Use Queues When:

1. **Fixed-size items**: All messages are the same size
2. **Multiple readers/writers**: Several tasks need to send or receive
3. **Structured data**: Working with well-defined data structures
4. **Priority ordering**: Need to prioritize certain messages
5. **Atomic operations**: Need to pass pointers or handles atomically

**Example scenarios:**
- Command processing (fixed command structures)
- Event systems (event IDs with fixed parameters)
- Resource pools (passing handles between tasks)
- State machine events

## Comparison Table

| Feature | Message Buffers | Queues |
|---------|----------------|--------|
| Message size | Variable | Fixed |
| Optimal readers | 1 | Multiple supported |
| Optimal writers | 1 | Multiple supported |
| Memory efficiency | High (no wasted space) | Lower (allocates max size) |
| Implementation | Circular byte buffer | Array of fixed items |
| Overhead per message | ~4 bytes (length) | None (size known) |
| Use case | Byte streams, packets | Structured commands, events |

## Advanced Example: Protocol Handler

Here's an example showing message buffers handling a simple packet protocol:

```c
#define PROTOCOL_BUFFER_SIZE 1024

// Packet types
typedef enum
{
    PACKET_TYPE_HEARTBEAT = 0x01,
    PACKET_TYPE_DATA = 0x02,
    PACKET_TYPE_CONFIG = 0x03,
    PACKET_TYPE_ERROR = 0xFF
} PacketType_t;

// Generic packet header
typedef struct __attribute__((packed))
{
    uint8_t type;
    uint16_t length;
    uint8_t data[];
} Packet_t;

MessageBufferHandle_t xProtocolBuffer;

// Protocol receiver (e.g., from UART ISR)
void vProtocolReceiverTask(void *pvParameters)
{
    uint8_t packetBuffer[256];
    
    for (;;)
    {
        // Simulate receiving packets of different types and sizes
        
        // Send heartbeat (small packet)
        Packet_t *heartbeat = (Packet_t*)packetBuffer;
        heartbeat->type = PACKET_TYPE_HEARTBEAT;
        heartbeat->length = 0;
        xMessageBufferSend(xProtocolBuffer, heartbeat, 
                          sizeof(Packet_t), pdMS_TO_TICKS(10));
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Send data packet (variable size)
        Packet_t *dataPacket = (Packet_t*)packetBuffer;
        dataPacket->type = PACKET_TYPE_DATA;
        dataPacket->length = 50;
        for (int i = 0; i < 50; i++)
        {
            dataPacket->data[i] = i;
        }
        xMessageBufferSend(xProtocolBuffer, dataPacket, 
                          sizeof(Packet_t) + 50, pdMS_TO_TICKS(10));
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Protocol parser
void vProtocolParserTask(void *pvParameters)
{
    uint8_t rxBuffer[256];
    size_t bytesReceived;
    
    for (;;)
    {
        bytesReceived = xMessageBufferReceive(
            xProtocolBuffer,
            rxBuffer,
            sizeof(rxBuffer),
            portMAX_DELAY
        );
        
        if (bytesReceived >= sizeof(Packet_t))
        {
            Packet_t *packet = (Packet_t*)rxBuffer;
            
            switch (packet->type)
            {
                case PACKET_TYPE_HEARTBEAT:
                    printf("Heartbeat received\n");
                    break;
                    
                case PACKET_TYPE_DATA:
                    printf("Data packet: %d bytes\n", packet->length);
                    // Process data...
                    break;
                    
                case PACKET_TYPE_CONFIG:
                    printf("Config packet: %d bytes\n", packet->length);
                    // Apply configuration...
                    break;
                    
                default:
                    printf("Unknown packet type: 0x%02X\n", packet->type);
                    break;
            }
        }
    }
}
```

## Important Considerations

**Buffer sizing:**
- Total buffer size must accommodate the largest expected message plus overhead
- Consider: `buffer_size = (max_message_size + 4) * num_messages`
- Add extra space for length prefixes (4 bytes per message on 32-bit systems)

**Thread safety:**
- Message buffers are thread-safe between one reader and one writer
- Multiple writers or readers require external synchronization (mutex)

**Performance:**
- Message buffers use task notifications internally (very efficient)
- Minimal context switching overhead
- Zero-copy not possible (data must be copied in/out)

**Partial reads:**
- If receive buffer is too small, the message is NOT partially read
- The entire message is discarded if it doesn't fit
- Always ensure receive buffer is large enough for largest expected message

**Interrupt safety:**
- Use `xMessageBufferSendFromISR()` and `xMessageBufferReceiveFromISR()` in ISRs
- Regular functions cannot be called from interrupt context

This comprehensive coverage of message buffers should give you a solid foundation for using them effectively in FreeRTOS applications, understanding when they're the right choice over queues, and implementing them correctly in real-world scenarios.