#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#define TASK_1_SYNC_BIT  (1 << 0)
#define TASK_2_SYNC_BIT  (1 << 1)
#define TASK_3_SYNC_BIT  (1 << 2)
#define ALL_SYNC_BITS    (TASK_1_SYNC_BIT | TASK_2_SYNC_BIT | TASK_3_SYNC_BIT)

EventGroupHandle_t xSyncEventGroup;

void vWorkerTask1(void *pvParameters) {
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("Task 1: Starting work cycle %d\n", cycle);
        
        // Do some work (variable duration)
        vTaskDelay(pdMS_TO_TICKS(100 + (cycle * 50)));
        
        printf("Task 1: Finished work, waiting at sync point\n");
        
        // Signal completion and wait for others
        xEventGroupSync(
            xSyncEventGroup,
            TASK_1_SYNC_BIT,     // Bit to set (my completion)
            ALL_SYNC_BITS,       // Bits to wait for (everyone's completion)
            portMAX_DELAY
        );
        
        printf("Task 1: All tasks synchronized, continuing...\n");
    }
    
    vTaskDelete(NULL);
}

void vWorkerTask2(void *pvParameters) {
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("Task 2: Starting work cycle %d\n", cycle);
        
        vTaskDelay(pdMS_TO_TICKS(200 + (cycle * 30)));
        
        printf("Task 2: Finished work, waiting at sync point\n");
        
        xEventGroupSync(
            xSyncEventGroup,
            TASK_2_SYNC_BIT,
            ALL_SYNC_BITS,
            portMAX_DELAY
        );
        
        printf("Task 2: All tasks synchronized, continuing...\n");
    }
    
    vTaskDelete(NULL);
}

void vWorkerTask3(void *pvParameters) {
    for (int cycle = 0; cycle < 5; cycle++) {
        printf("Task 3: Starting work cycle %d\n", cycle);
        
        vTaskDelay(pdMS_TO_TICKS(150 + (cycle * 40)));
        
        printf("Task 3: Finished work, waiting at sync point\n");
        
        xEventGroupSync(
            xSyncEventGroup,
            TASK_3_SYNC_BIT,
            ALL_SYNC_BITS,
            portMAX_DELAY
        );
        
        printf("Task 3: All tasks synchronized, continuing...\n");
    }
    
    vTaskDelete(NULL);
}