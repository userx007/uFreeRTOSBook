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