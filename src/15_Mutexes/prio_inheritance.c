#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

SemaphoreHandle_t xMutex;
volatile uint32_t shared_counter = 0;

// Low priority task
void vLowPriorityTask(void *pvParameters)
{
    for (;;)
    {
        printf("Low: Attempting to take mutex\n");
        
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            printf("Low: Acquired mutex, working...\n");
            
            // Simulate long critical section
            for (uint32_t i = 0; i < 1000000; i++)
            {
                shared_counter++;
            }
            
            printf("Low: Releasing mutex\n");
            xSemaphoreGive(xMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Medium priority task (doesn't use mutex)
void vMediumPriorityTask(void *pvParameters)
{
    for (;;)
    {
        printf("Medium: Running (no mutex needed)\n");
        
        // Simulate work
        for (uint32_t i = 0; i < 500000; i++)
        {
            // Busy work
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// High priority task
void vHighPriorityTask(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(100)); // Let low priority task run first
    
    for (;;)
    {
        printf("High: Attempting to take mutex\n");
        
        if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
        {
            printf("High: Acquired mutex, working...\n");
            shared_counter += 10;
            
            printf("High: Releasing mutex\n");
            xSemaphoreGive(xMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    // Create mutex with priority inheritance
    xMutex = xSemaphoreCreateMutex();
    
    if (xMutex != NULL)
    {
        xTaskCreate(vLowPriorityTask, "Low", 1000, NULL, 1, NULL);
        xTaskCreate(vMediumPriorityTask, "Medium", 1000, NULL, 2, NULL);
        xTaskCreate(vHighPriorityTask, "High", 1000, NULL, 3, NULL);
        
        vTaskStartScheduler();
    }
    
    // Should never reach here
    for (;;);
    return 0;
}