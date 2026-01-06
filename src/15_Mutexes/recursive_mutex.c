#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

SemaphoreHandle_t xRecursiveMutex;
uint32_t shared_data = 0;

// Function that calls itself recursively
void vProcessData(uint32_t depth)
{
    if (xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY) == pdTRUE)
    {
        printf("  Level %u: Processing data = %u\n", depth, shared_data);
        shared_data++;
        
        if (depth > 0)
        {
            // Recursive call - will take the mutex again
            vProcessData(depth - 1);
        }
        
        // Must release for each take
        xSemaphoreGiveRecursive(xRecursiveMutex);
    }
}

// High-level function that also needs the mutex
void vUpdateData(uint32_t value)
{
    if (xSemaphoreTakeRecursive(xRecursiveMutex, portMAX_DELAY) == pdTRUE)
    {
        printf("Updating data with value %u\n", value);
        shared_data = value;
        
        // Call another function that needs the same mutex
        vProcessData(3);
        
        printf("Update complete. Final data = %u\n", shared_data);
        
        xSemaphoreGiveRecursive(xRecursiveMutex);
    }
}

void vTask(void *pvParameters)
{
    for (;;)
    {
        vUpdateData(100);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    xRecursiveMutex = xSemaphoreCreateRecursiveMutex();
    
    if (xRecursiveMutex != NULL)
    {
        xTaskCreate(vTask, "Task", 1000, NULL, 1, NULL);
        vTaskStartScheduler();
    }
    
    for (;;);
    return 0;
}