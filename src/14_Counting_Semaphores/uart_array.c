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