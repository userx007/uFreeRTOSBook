#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#define SENSOR_A_DATA_BIT   (1 << 0)
#define SENSOR_B_DATA_BIT   (1 << 1)
#define ERROR_CONDITION_BIT (1 << 2)
#define TIMEOUT_BIT         (1 << 3)

EventGroupHandle_t xDataEventGroup;

void vSensorATask(void *pvParameters) {
    while (1) {
        // Simulate reading sensor A
        vTaskDelay(pdMS_TO_TICKS(500));
        printf("Sensor A: New data available\n");
        xEventGroupSetBits(xDataEventGroup, SENSOR_A_DATA_BIT);
    }
}

void vSensorBTask(void *pvParameters) {
    while (1) {
        // Simulate reading sensor B (slower)
        vTaskDelay(pdMS_TO_TICKS(1200));
        printf("Sensor B: New data available\n");
        xEventGroupSetBits(xDataEventGroup, SENSOR_B_DATA_BIT);
    }
}

void vTimeoutTask(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        printf("Timeout: Setting timeout flag\n");
        xEventGroupSetBits(xDataEventGroup, TIMEOUT_BIT);
    }
}

void vDataProcessorTask(void *pvParameters) {
    const EventBits_t xBitsToWaitFor = SENSOR_A_DATA_BIT | 
                                        SENSOR_B_DATA_BIT | 
                                        TIMEOUT_BIT;
    
    while (1) {
        printf("Processor: Waiting for data or timeout...\n");
        
        // Wait for ANY of the specified bits (OR condition)
        EventBits_t uxBits = xEventGroupWaitBits(
            xDataEventGroup,
            xBitsToWaitFor,
            pdTRUE,              // Clear bits on exit
            pdFALSE,             // Wait for ANY bit (OR condition)
            portMAX_DELAY
        );
        
        // Check which event occurred
        if (uxBits & SENSOR_A_DATA_BIT) {
            printf("Processor: Processing Sensor A data\n");
        }
        
        if (uxBits & SENSOR_B_DATA_BIT) {
            printf("Processor: Processing Sensor B data\n");
        }
        
        if (uxBits & TIMEOUT_BIT) {
            printf("Processor: Timeout occurred - using default values\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}