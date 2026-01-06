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