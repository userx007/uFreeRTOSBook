# FreeRTOS Stream Buffers: Detailed Guide

Stream Buffers are a FreeRTOS IPC (Inter-Process Communication) mechanism designed for efficient, lock-free transfer of continuous byte streams between tasks, or between an interrupt and a task. They're optimized for scenarios where data flows in one direction as a stream of bytes rather than discrete messages.

## Core Concept

A Stream Buffer is essentially a circular buffer (ring buffer) that allows one task or ISR to write bytes and another task to read them. The key characteristic is that Stream Buffers handle **arbitrary-length byte sequences** rather than fixed-size messages.

**Key Properties:**
- **Single writer, single reader** architecture (lock-free when used correctly)
- **Byte-oriented** rather than message-oriented
- **Variable-length transfers** - write and read operations can transfer any number of bytes
- **Efficient** - optimized for high-throughput streaming data
- **Blocking capabilities** - tasks can block waiting for space (write) or data (read)

## Creating a Stream Buffer

```c
#include "stream_buffer.h"

// Create a stream buffer with 1000 bytes of storage
// Trigger level set to 10 bytes (reader unblocks when 10+ bytes available)
StreamBufferHandle_t xStreamBuffer;

void setup_stream_buffer(void)
{
    xStreamBuffer = xStreamBufferCreate(
        1000,  // Total buffer size in bytes
        10     // Trigger level - minimum bytes before xStreamBufferReceive() unblocks
    );
    
    if (xStreamBuffer == NULL) {
        // Failed to create stream buffer - insufficient heap memory
    }
}
```

## Basic Operations

### Writing to Stream Buffer

```c
void vSenderTask(void *pvParameters)
{
    const char *pcDataToSend = "Hello from sender task!";
    size_t xBytesSent;
    
    for (;;)
    {
        // Send data to stream buffer
        // Wait up to 100ms if buffer is full
        xBytesSent = xStreamBufferSend(
            xStreamBuffer,           // Stream buffer handle
            pcDataToSend,            // Data to send
            strlen(pcDataToSend),    // Number of bytes to send
            pdMS_TO_TICKS(100)       // Max time to block if full
        );
        
        if (xBytesSent != strlen(pcDataToSend)) {
            // Not all bytes were sent (buffer full or timeout)
            printf("Only sent %d bytes\n", xBytesSent);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Reading from Stream Buffer

```c
void vReceiverTask(void *pvParameters)
{
    uint8_t ucRxBuffer[100];
    size_t xBytesReceived;
    
    for (;;)
    {
        // Receive data from stream buffer
        // Block indefinitely until trigger level is reached
        xBytesReceived = xStreamBufferReceive(
            xStreamBuffer,           // Stream buffer handle
            ucRxBuffer,              // Buffer to receive into
            sizeof(ucRxBuffer),      // Maximum bytes to receive
            portMAX_DELAY            // Block indefinitely
        );
        
        if (xBytesReceived > 0) {
            // Process received data
            ucRxBuffer[xBytesReceived] = '\0';  // Null terminate if string
            printf("Received %d bytes: %s\n", xBytesReceived, ucRxBuffer);
        }
    }
}
```

## Practical Example: UART Serial Data Handling

This is one of the most common use cases for Stream Buffers - buffering serial data from a UART interrupt to a processing task.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

// Stream buffer for UART RX data
StreamBufferHandle_t xUARTRxStreamBuffer;

// UART initialization
void vUARTInit(void)
{
    // Create stream buffer: 512 bytes, trigger at 1 byte
    xUARTRxStreamBuffer = xStreamBufferCreate(512, 1);
    
    if (xUARTRxStreamBuffer != NULL) {
        // Configure UART hardware
        UART_Init();
        UART_EnableRxInterrupt();
    }
}

// UART RX Interrupt Service Routine
void UART_RX_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ucReceivedByte;
    
    // Read byte from UART hardware register
    ucReceivedByte = UART_ReadByte();
    
    // Send to stream buffer from ISR
    xStreamBufferSendFromISR(
        xUARTRxStreamBuffer,
        &ucReceivedByte,
        1,                              // One byte
        &xHigherPriorityTaskWoken
    );
    
    // Request context switch if a higher priority task was woken
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task that processes UART data
void vUARTProcessingTask(void *pvParameters)
{
    uint8_t ucBuffer[128];
    size_t xBytesReceived;
    
    for (;;)
    {
        // Block until data is available
        xBytesReceived = xStreamBufferReceive(
            xUARTRxStreamBuffer,
            ucBuffer,
            sizeof(ucBuffer),
            portMAX_DELAY
        );
        
        // Process the received data
        if (xBytesReceived > 0) {
            // Example: Echo back or parse protocol
            processSerialData(ucBuffer, xBytesReceived);
        }
    }
}
```

## Advanced Example: GPS NMEA Sentence Parser

Stream Buffers are ideal for handling variable-length GPS data where complete sentences arrive over time.

```c
StreamBufferHandle_t xGPSStreamBuffer;

void vGPSTaskInit(void)
{
    // Create buffer large enough for several NMEA sentences
    xGPSStreamBuffer = xStreamBufferCreate(1024, 1);
}

// GPS UART ISR - accumulates incoming GPS data
void GPS_UART_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t ucByte;
    
    while (GPS_UART_DataAvailable()) {
        ucByte = GPS_UART_ReadByte();
        
        xStreamBufferSendFromISR(
            xGPSStreamBuffer,
            &ucByte,
            1,
            &xHigherPriorityTaskWoken
        );
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task that parses complete NMEA sentences
void vGPSParserTask(void *pvParameters)
{
    uint8_t ucLineBuffer[100];
    size_t xBufferIndex = 0;
    uint8_t ucByte;
    
    for (;;)
    {
        // Read one byte at a time
        if (xStreamBufferReceive(xGPSStreamBuffer, &ucByte, 1, portMAX_DELAY) > 0)
        {
            // Accumulate until newline (complete sentence)
            if (ucByte == '\n' || ucByte == '\r') {
                if (xBufferIndex > 0) {
                    ucLineBuffer[xBufferIndex] = '\0';
                    
                    // Parse complete NMEA sentence
                    if (strncmp((char*)ucLineBuffer, "$GPGGA", 6) == 0) {
                        parseGPGGA((char*)ucLineBuffer);
                    } else if (strncmp((char*)ucLineBuffer, "$GPRMC", 6) == 0) {
                        parseGPRMC((char*)ucLineBuffer);
                    }
                    
                    xBufferIndex = 0;
                }
            } else if (xBufferIndex < sizeof(ucLineBuffer) - 1) {
                ucLineBuffer[xBufferIndex++] = ucByte;
            }
        }
    }
}
```

## Checking Buffer Status

```c
void vMonitorTask(void *pvParameters)
{
    size_t xBytesAvailable;
    size_t xSpaceAvailable;
    BaseType_t xIsEmpty, xIsFull;
    
    for (;;)
    {
        // Query buffer status
        xBytesAvailable = xStreamBufferBytesAvailable(xStreamBuffer);
        xSpaceAvailable = xStreamBufferSpacesAvailable(xStreamBuffer);
        xIsEmpty = xStreamBufferIsEmpty(xStreamBuffer);
        xIsFull = xStreamBufferIsFull(xStreamBuffer);
        
        printf("Buffer: %d bytes available, %d spaces free\n",
               xBytesAvailable, xSpaceAvailable);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Stream Buffers vs Message Buffers

While Stream Buffers and Message Buffers are similar, they serve different purposes:

### Stream Buffers
- **Byte stream** - continuous flow of bytes
- **Variable read size** - can read any number of bytes up to available
- **No message boundaries** - data is just a sequence of bytes
- **Use case**: Serial data (UART, SPI), audio/video streams, continuous sensor data
- **Trigger level**: Reader unblocks when N bytes are available

```c
// Stream Buffer: Write 10 bytes, read 7, read 3 - no boundaries
xStreamBufferSend(xStreamBuffer, data, 10, timeout);
xStreamBufferReceive(xStreamBuffer, buffer, 7, timeout);  // Gets 7 bytes
xStreamBufferReceive(xStreamBuffer, buffer, 3, timeout);  // Gets remaining 3
```

### Message Buffers
- **Discrete messages** - each write is a separate message
- **Fixed read size** - each read returns exactly one complete message
- **Message boundaries preserved** - messages don't merge or split
- **Use case**: Protocol packets, command/response pairs, structured data
- **Automatic length prefix**: Message length is stored automatically

```c
// Message Buffer: Write 2 messages, read 2 complete messages
xMessageBufferSend(xMessageBuffer, msg1, 10, timeout);   // Message 1: 10 bytes
xMessageBufferSend(xMessageBuffer, msg2, 5, timeout);    // Message 2: 5 bytes
xMessageBufferReceive(xMessageBuffer, buffer, 100, timeout); // Gets all 10 bytes of msg1
xMessageBufferReceive(xMessageBuffer, buffer, 100, timeout); // Gets all 5 bytes of msg2
```

### Comparison Table

| Feature | Stream Buffer | Message Buffer |
|---------|--------------|----------------|
| Data unit | Bytes | Messages |
| Boundaries | No boundaries | Preserves message boundaries |
| Read operation | Can read partial data | Always reads complete message |
| Length info | Not stored | Automatically stored per message |
| Overhead | Minimal | 4 bytes per message |
| Best for | Continuous streams | Discrete packets |

## Example: When to Use Each

**Use Stream Buffer for:**
```c
// Continuous sensor data stream
void vADCTask(void *pvParameters)
{
    uint16_t samples[64];
    
    for (;;) {
        readADCSamples(samples, 64);
        // Send continuous stream - no message boundaries needed
        xStreamBufferSend(xADCStream, samples, 128, portMAX_DELAY);
    }
}
```

**Use Message Buffer for:**
```c
// Protocol commands with varying lengths
void vCommandTask(void *pvParameters)
{
    CommandPacket_t cmd1 = {0x01, 5, {1,2,3,4,5}};
    CommandPacket_t cmd2 = {0x02, 10, {1,2,3,4,5,6,7,8,9,10}};
    
    // Each command is a discrete message
    xMessageBufferSend(xCmdBuffer, &cmd1, sizeof(cmd1), portMAX_DELAY);
    xMessageBufferSend(xCmdBuffer, &cmd2, sizeof(cmd2), portMAX_DELAY);
    // Receiver will get complete commands, never partial
}
```

## Important Considerations

**Trigger Level:** Set appropriately based on your application. For serial data, trigger level of 1 ensures minimal latency. For bulk transfers, higher trigger levels reduce task switching overhead.

**Single Producer/Single Consumer:** Stream Buffers are optimized for one writer and one reader. Multiple writers or readers require external synchronization.

**Memory Allocation:** Stream Buffers use heap memory. Ensure `configSUPPORT_DYNAMIC_ALLOCATION` is set to 1 in `FreeRTOSConfig.h`.

**ISR Safety:** Always use `xStreamBufferSendFromISR()` and `xStreamBufferReceiveFromISR()` from interrupt context.

**Buffer Sizing:** Size the buffer to handle burst traffic. For UART at 115200 baud, roughly 11,520 bytes/second, so a 512-1024 byte buffer provides adequate buffering for typical processing delays.

Stream Buffers provide an efficient, purpose-built solution for handling continuous data streams in FreeRTOS applications, particularly excelling in scenarios involving serial communication and sensor data acquisition.