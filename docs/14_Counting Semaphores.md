# Counting Semaphores in FreeRTOS

## Overview

Counting semaphores are synchronization primitives that maintain an integer count, allowing multiple tasks to access a finite number of identical resources. Unlike binary semaphores (which have only two states: available or unavailable), counting semaphores can count from zero up to a maximum value, making them ideal for managing pools of resources.

## Core Concepts

**Count Value**: The semaphore maintains a count that represents:
- The number of available resources (when positive)
- The number of tasks waiting for resources (when zero, with tasks blocked)

**Operations**:
- **Give (xSemaphoreGive)**: Increments the count, signaling resource availability
- **Take (xSemaphoreTake)**: Decrements the count, acquiring a resource; blocks if count is zero

## Creating Counting Semaphores

```c
#include "FreeRTOS.h"
#include "semphr.h"

SemaphoreHandle_t xCountingSemaphore;

// Create a counting semaphore with max count of 5 and initial count of 5
xCountingSemaphore = xSemaphoreCreateCounting(5,  // Maximum count
                                              5); // Initial count

if (xCountingSemaphore == NULL) {
    // Creation failed - insufficient heap memory
}
```

## Example 1: Managing a Pool of Serial Ports

This example demonstrates managing access to multiple identical UART peripherals.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define MAX_UART_PORTS 3

SemaphoreHandle_t xUartPoolSemaphore;
uint8_t uart_ports[MAX_UART_PORTS] = {0, 1, 2}; // Port IDs
SemaphoreHandle_t xPortArrayMutex;

typedef struct {
    uint8_t port_id;
    bool in_use;
} UartPort_t;

UartPort_t uart_pool[MAX_UART_PORTS];

void init_uart_pool(void) {
    // Initialize UART pool
    for (int i = 0; i < MAX_UART_PORTS; i++) {
        uart_pool[i].port_id = i;
        uart_pool[i].in_use = false;
    }
    
    // Create counting semaphore for port availability
    xUartPoolSemaphore = xSemaphoreCreateCounting(MAX_UART_PORTS, 
                                                   MAX_UART_PORTS);
    
    // Mutex to protect port allocation
    xPortArrayMutex = xSemaphoreCreateMutex();
}

int8_t acquire_uart_port(TickType_t xBlockTime) {
    // Wait for an available port
    if (xSemaphoreTake(xUartPoolSemaphore, xBlockTime) == pdTRUE) {
        // Find and allocate a free port
        xSemaphoreTake(xPortArrayMutex, portMAX_DELAY);
        
        for (int i = 0; i < MAX_UART_PORTS; i++) {
            if (!uart_pool[i].in_use) {
                uart_pool[i].in_use = true;
                xSemaphoreGive(xPortArrayMutex);
                return uart_pool[i].port_id;
            }
        }
        
        xSemaphoreGive(xPortArrayMutex);
    }
    
    return -1; // No port available
}

void release_uart_port(uint8_t port_id) {
    xSemaphoreTake(xPortArrayMutex, portMAX_DELAY);
    
    for (int i = 0; i < MAX_UART_PORTS; i++) {
        if (uart_pool[i].port_id == port_id) {
            uart_pool[i].in_use = false;
            break;
        }
    }
    
    xSemaphoreGive(xPortArrayMutex);
    
    // Signal that a port is now available
    xSemaphoreGive(xUartPoolSemaphore);
}

void vCommunicationTask(void *pvParameters) {
    int8_t port;
    
    while (1) {
        // Try to acquire a UART port (wait up to 1 second)
        port = acquire_uart_port(pdMS_TO_TICKS(1000));
        
        if (port >= 0) {
            // Successfully acquired a port
            printf("Task acquired UART port %d\n", port);
            
            // Simulate communication work
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Release the port when done
            release_uart_port(port);
            printf("Task released UART port %d\n", port);
        } else {
            printf("No UART port available\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Example 2: Parking Lot Management System

A practical example showing how counting semaphores manage limited parking spaces.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

#define TOTAL_PARKING_SPACES 10

SemaphoreHandle_t xParkingSemaphore;
volatile uint32_t cars_parked = 0;

void init_parking_system(void) {
    // Create semaphore with 10 spaces, all initially available
    xParkingSemaphore = xSemaphoreCreateCounting(TOTAL_PARKING_SPACES,
                                                  TOTAL_PARKING_SPACES);
}

void vCarArrivalTask(void *pvParameters) {
    uint32_t car_id = (uint32_t)pvParameters;
    
    while (1) {
        // Simulate random car arrivals
        vTaskDelay(pdMS_TO_TICKS(rand() % 3000 + 1000));
        
        printf("Car %lu: Attempting to enter parking lot...\n", car_id);
        
        // Try to take a parking space (wait up to 5 seconds)
        if (xSemaphoreTake(xParkingSemaphore, pdMS_TO_TICKS(5000)) == pdTRUE) {
            cars_parked++;
            printf("Car %lu: PARKED! (Spaces occupied: %lu/%d)\n", 
                   car_id, cars_parked, TOTAL_PARKING_SPACES);
            
            // Car stays parked for random duration
            vTaskDelay(pdMS_TO_TICKS(rand() % 5000 + 2000));
            
            // Car leaves - release the space
            cars_parked--;
            xSemaphoreGive(xParkingSemaphore);
            printf("Car %lu: LEFT parking lot (Spaces occupied: %lu/%d)\n",
                   car_id, cars_parked, TOTAL_PARKING_SPACES);
        } else {
            printf("Car %lu: Parking FULL - driving away!\n", car_id);
        }
    }
}

void vParkingMonitorTask(void *pvParameters) {
    while (1) {
        UBaseType_t available = uxSemaphoreGetCount(xParkingSemaphore);
        printf("=== Monitor: %lu/%d spaces available ===\n",
               available, TOTAL_PARKING_SPACES);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
```

## Example 3: Network Packet Buffer Pool

Managing a fixed pool of memory buffers for network operations.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define NUM_BUFFERS 20
#define BUFFER_SIZE 512

typedef struct {
    uint8_t data[BUFFER_SIZE];
    bool in_use;
    uint32_t buffer_id;
} NetworkBuffer_t;

SemaphoreHandle_t xBufferPoolSemaphore;
NetworkBuffer_t buffer_pool[NUM_BUFFERS];
SemaphoreHandle_t xBufferPoolMutex;

void init_buffer_pool(void) {
    // Initialize all buffers
    for (int i = 0; i < NUM_BUFFERS; i++) {
        buffer_pool[i].in_use = false;
        buffer_pool[i].buffer_id = i;
    }
    
    // Counting semaphore tracks available buffers
    xBufferPoolSemaphore = xSemaphoreCreateCounting(NUM_BUFFERS, NUM_BUFFERS);
    xBufferPoolMutex = xSemaphoreCreateMutex();
}

NetworkBuffer_t* allocate_buffer(TickType_t xBlockTime) {
    NetworkBuffer_t* buffer = NULL;
    
    // Wait for available buffer
    if (xSemaphoreTake(xBufferPoolSemaphore, xBlockTime) == pdTRUE) {
        // Protected allocation from pool
        xSemaphoreTake(xBufferPoolMutex, portMAX_DELAY);
        
        for (int i = 0; i < NUM_BUFFERS; i++) {
            if (!buffer_pool[i].in_use) {
                buffer_pool[i].in_use = true;
                buffer = &buffer_pool[i];
                break;
            }
        }
        
        xSemaphoreGive(xBufferPoolMutex);
    }
    
    return buffer;
}

void free_buffer(NetworkBuffer_t* buffer) {
    if (buffer == NULL) return;
    
    xSemaphoreTake(xBufferPoolMutex, portMAX_DELAY);
    buffer->in_use = false;
    xSemaphoreGive(xBufferPoolMutex);
    
    // Signal buffer is available
    xSemaphoreGive(xBufferPoolSemaphore);
}

void vNetworkTransmitTask(void *pvParameters) {
    NetworkBuffer_t* tx_buffer;
    
    while (1) {
        // Wait for data to transmit (simulated)
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Allocate buffer from pool
        tx_buffer = allocate_buffer(pdMS_TO_TICKS(1000));
        
        if (tx_buffer != NULL) {
            printf("TX: Allocated buffer %lu\n", tx_buffer->buffer_id);
            
            // Fill buffer with data and transmit (simulated)
            vTaskDelay(pdMS_TO_TICKS(50));
            
            // Free buffer back to pool
            free_buffer(tx_buffer);
            printf("TX: Freed buffer %lu\n", tx_buffer->buffer_id);
        } else {
            printf("TX: Buffer allocation timeout!\n");
        }
    }
}

void vNetworkReceiveTask(void *pvParameters) {
    NetworkBuffer_t* rx_buffer;
    
    while (1) {
        // Wait for incoming packet (simulated)
        vTaskDelay(pdMS_TO_TICKS(150));
        
        rx_buffer = allocate_buffer(pdMS_TO_TICKS(500));
        
        if (rx_buffer != NULL) {
            printf("RX: Allocated buffer %lu\n", rx_buffer->buffer_id);
            
            // Receive data into buffer (simulated)
            vTaskDelay(pdMS_TO_TICKS(30));
            
            free_buffer(rx_buffer);
            printf("RX: Freed buffer %lu\n", rx_buffer->buffer_id);
        } else {
            printf("RX: No buffers available - packet dropped!\n");
        }
    }
}
```

## Example 4: Producer-Consumer with Bounded Resource

Using counting semaphores to implement flow control between producers and consumers.

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define QUEUE_SIZE 10

SemaphoreHandle_t xEmptySlotsSemaphore;  // Counts empty slots
SemaphoreHandle_t xFullSlotsSemaphore;   // Counts full slots
SemaphoreHandle_t xQueueMutex;

uint32_t circular_buffer[QUEUE_SIZE];
volatile uint32_t write_index = 0;
volatile uint32_t read_index = 0;

void init_bounded_queue(void) {
    // Initially all slots are empty
    xEmptySlotsSemaphore = xSemaphoreCreateCounting(QUEUE_SIZE, QUEUE_SIZE);
    
    // Initially no slots are full
    xFullSlotsSemaphore = xSemaphoreCreateCounting(QUEUE_SIZE, 0);
    
    xQueueMutex = xSemaphoreCreateMutex();
}

bool produce_item(uint32_t item, TickType_t xBlockTime) {
    // Wait for an empty slot
    if (xSemaphoreTake(xEmptySlotsSemaphore, xBlockTime) == pdTRUE) {
        // Protect buffer access
        xSemaphoreTake(xQueueMutex, portMAX_DELAY);
        
        circular_buffer[write_index] = item;
        write_index = (write_index + 1) % QUEUE_SIZE;
        
        xSemaphoreGive(xQueueMutex);
        
        // Signal that a slot is now full
        xSemaphoreGive(xFullSlotsSemaphore);
        return true;
    }
    
    return false; // Queue full
}

bool consume_item(uint32_t* item, TickType_t xBlockTime) {
    // Wait for a full slot
    if (xSemaphoreTake(xFullSlotsSemaphore, xBlockTime) == pdTRUE) {
        // Protect buffer access
        xSemaphoreTake(xQueueMutex, portMAX_DELAY);
        
        *item = circular_buffer[read_index];
        read_index = (read_index + 1) % QUEUE_SIZE;
        
        xSemaphoreGive(xQueueMutex);
        
        // Signal that a slot is now empty
        xSemaphoreGive(xEmptySlotsSemaphore);
        return true;
    }
    
    return false; // Queue empty
}

void vProducerTask(void *pvParameters) {
    uint32_t item_count = 0;
    
    while (1) {
        item_count++;
        
        if (produce_item(item_count, pdMS_TO_TICKS(1000))) {
            printf("Producer: Added item %lu\n", item_count);
        } else {
            printf("Producer: Queue full, couldn't add item %lu\n", item_count);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vConsumerTask(void *pvParameters) {
    uint32_t item;
    
    while (1) {
        if (consume_item(&item, pdMS_TO_TICKS(2000))) {
            printf("Consumer: Retrieved item %lu\n", item);
        } else {
            printf("Consumer: Queue empty, nothing to consume\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
```

## Typical Use Cases

### 1. Resource Pool Management
Managing a fixed number of identical resources such as DMA channels, SPI peripherals, memory blocks, or hardware timers.

### 2. Rate Limiting
Controlling the maximum number of concurrent operations, such as limiting active network connections or simultaneous file accesses.

### 3. Producer-Consumer Flow Control
Coordinating multiple producers and consumers accessing a bounded buffer, preventing overflow and underflow conditions.

### 4. Connection Pooling
Managing database connections, network sockets, or other limited communication channels in embedded systems.

### 5. Interrupt Event Counting
Counting events from interrupts where multiple events can occur before tasks process them, ensuring no events are lost.

## Important API Functions

```c
// Create counting semaphore
SemaphoreHandle_t xSemaphoreCreateCounting(
    UBaseType_t uxMaxCount,      // Maximum count value
    UBaseType_t uxInitialCount   // Initial count value
);

// Take (acquire) semaphore
BaseType_t xSemaphoreTake(
    SemaphoreHandle_t xSemaphore,
    TickType_t xBlockTime
);

// Give (release) semaphore
BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);

// Get current count (available resources)
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t xSemaphore);

// ISR-safe versions
BaseType_t xSemaphoreTakeFromISR(
    SemaphoreHandle_t xSemaphore,
    BaseType_t *pxHigherPriorityTaskWoken
);

BaseType_t xSemaphoreGiveFromISR(
    SemaphoreHandle_t xSemaphore,
    BaseType_t *pxHigherPriorityTaskWoken
);
```

## Best Practices

**Initialize Correctly**: Set the maximum and initial count values appropriately based on the actual number of resources available.

**Avoid Count Overflow**: Never call xSemaphoreGive more times than the maximum count, as behavior is undefined.

**Pair Take and Give Operations**: Every successful take should eventually be matched with a give to prevent resource leaks.

**Use with Mutexes**: When the counting semaphore tracks access to a pool of resources, often a mutex is needed to protect the actual allocation/deallocation of specific resources.

**Monitor for Starvation**: Ensure lower-priority tasks can eventually acquire resources by using appropriate timeout values and task priorities.

**ISR Considerations**: Use ISR-safe variants when giving semaphores from interrupt handlers, and always check the return value to trigger context switches when needed.

Counting semaphores provide a powerful and efficient mechanism for managing finite pools of resources in FreeRTOS applications, enabling robust multi-task coordination and preventing resource exhaustion.