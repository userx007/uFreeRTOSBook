# FreeRTOS: Queues vs Stream Buffers vs Message Buffers

These are three different inter-task communication mechanisms in FreeRTOS, each optimized for specific use cases.

## 1. Queues

**Purpose**: Pass discrete data items (messages) between tasks with full task synchronization.

**Characteristics**:
- Stores fixed-size items (structs, pointers, primitives)
- FIFO ordering (can be configurable)
- Full blocking semantics on both send and receive
- Supports multiple readers and writers
- Item-based, not byte-based
- Overhead per item for queue management

**Example Use Case**: Sending sensor readings between tasks

```c
// Define a structure for sensor data
typedef struct {
    uint8_t sensorId;
    float temperature;
    uint32_t timestamp;
} SensorData_t;

// Create queue that holds 10 sensor readings
QueueHandle_t sensorQueue;

void setup() {
    sensorQueue = xQueueCreate(10, sizeof(SensorData_t));
}

// Producer task
void sensorTask(void *pvParameters) {
    SensorData_t reading;
    
    while(1) {
        reading.sensorId = 1;
        reading.temperature = readTemperature();
        reading.timestamp = xTaskGetTickCount();
        
        // Send to queue (block for 100ms if queue full)
        if(xQueueSend(sensorQueue, &reading, pdMS_TO_TICKS(100)) == pdPASS) {
            // Successfully sent
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Consumer task
void processingTask(void *pvParameters) {
    SensorData_t reading;
    
    while(1) {
        // Wait indefinitely for data
        if(xQueueReceive(sensorQueue, &reading, portMAX_DELAY) == pdPASS) {
            printf("Sensor %d: %.2f°C at %lu\n", 
                   reading.sensorId, reading.temperature, reading.timestamp);
        }
    }
}
```

## 2. Stream Buffers

**Purpose**: Pass a continuous stream of bytes from one task to another (single producer, single consumer only).

**Characteristics**:
- Byte-oriented, variable-length data
- Optimized for single writer, single reader
- Lower overhead than queues
- No message boundaries preserved
- Can read partial data
- Ideal for streaming data like UART reception

**Example Use Case**: UART data streaming

```c
StreamBufferHandle_t uartStreamBuffer;

void setup() {
    // Create 1KB stream buffer with trigger level of 50 bytes
    uartStreamBuffer = xStreamBufferCreate(1024, 50);
}

// UART interrupt or receive task (producer)
void UART_IRQHandler(void) {
    uint8_t receivedByte = UART_ReadByte();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Send single byte from ISR
    xStreamBufferSendFromISR(uartStreamBuffer, 
                             &receivedByte, 
                             1, 
                             &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Processing task (consumer)
void uartProcessingTask(void *pvParameters) {
    uint8_t buffer[128];
    size_t bytesReceived;
    
    while(1) {
        // Read up to 128 bytes (blocks until trigger level reached or timeout)
        bytesReceived = xStreamBufferReceive(uartStreamBuffer,
                                             buffer,
                                             sizeof(buffer),
                                             portMAX_DELAY);
        
        if(bytesReceived > 0) {
            // Process the received bytes
            processData(buffer, bytesReceived);
        }
    }
}
```

## 3. Message Buffers

**Purpose**: Pass variable-length messages between tasks while preserving message boundaries (single producer, single consumer).

**Characteristics**:
- Built on top of stream buffers
- Preserves message boundaries (unlike stream buffers)
- Variable-length messages
- Single writer, single reader only
- Each receive gets exactly one complete message
- Automatically adds length prefix to each message

**Example Use Case**: Passing variable-length protocol packets

```c
MessageBufferHandle_t packetMessageBuffer;

void setup() {
    // Create message buffer of 512 bytes
    packetMessageBuffer = xMessageBufferCreate(512);
}

// Network receive task (producer)
void networkReceiveTask(void *pvParameters) {
    uint8_t packet[256];
    size_t packetLen;
    
    while(1) {
        // Receive variable-length packet from network
        packetLen = receiveNetworkPacket(packet, sizeof(packet));
        
        if(packetLen > 0) {
            // Send entire packet as one message
            xMessageBufferSend(packetMessageBuffer,
                              packet,
                              packetLen,
                              pdMS_TO_TICKS(100));
        }
    }
}

// Packet processing task (consumer)
void packetProcessingTask(void *pvParameters) {
    uint8_t receivedPacket[256];
    size_t receivedLen;
    
    while(1) {
        // Receive exactly one complete message
        receivedLen = xMessageBufferReceive(packetMessageBuffer,
                                           receivedPacket,
                                           sizeof(receivedPacket),
                                           portMAX_DELAY);
        
        if(receivedLen > 0) {
            // Process complete packet
            printf("Received packet of %d bytes\n", receivedLen);
            processPacket(receivedPacket, receivedLen);
        }
    }
}
```

## Key Differences Summary

| Feature | Queues | Stream Buffers | Message Buffers |
|---------|--------|----------------|-----------------|
| **Data Unit** | Fixed-size items | Continuous bytes | Variable-length messages |
| **Message Boundaries** | Yes | No | Yes |
| **Producers/Consumers** | Multiple/Multiple | One/One | One/One |
| **Use Case** | Structured data items | Byte streams (UART, ADC) | Variable packets/frames |
| **Overhead** | Higher (per item) | Lower | Low-Medium |
| **Partial Reads** | No | Yes | No |
| **Typical Example** | Commands, events | Serial data | Network packets |

## When to Use Which

**Use Queues when**:
- You need multiple producers or consumers
- You're passing structured data (structs, enums)
- You need queue sets or mailbox semantics
- Message ordering and atomic transfer of items is critical

**Use Stream Buffers when**:
- You have continuous byte stream data
- Message boundaries don't matter
- You want lowest overhead
- Single producer, single consumer is sufficient
- Example: UART RX, ADC samples, audio streams

**Use Message Buffers when**:
- You have variable-length messages
- Message boundaries must be preserved
- Single producer, single consumer is sufficient
- Example: Protocol packets, variable commands, frames

The choice depends on the specific requirements for data structure, synchronization needs, and performance constraints.