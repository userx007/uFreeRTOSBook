#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

// Define event bits for different initialization states
#define WIFI_READY_BIT      (1 << 0)  // Bit 0
#define SENSOR_READY_BIT    (1 << 1)  // Bit 1
#define DATABASE_READY_BIT  (1 << 2)  // Bit 2
#define ALL_READY_BITS      (WIFI_READY_BIT | SENSOR_READY_BIT | DATABASE_READY_BIT)

EventGroupHandle_t xSystemEventGroup;

void vWiFiInitTask(void *pvParameters) {
    // Simulate WiFi initialization
    printf("WiFi: Starting initialization...\n");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // WiFi is ready - set the bit
    xEventGroupSetBits(xSystemEventGroup, WIFI_READY_BIT);
    printf("WiFi: Ready!\n");
    
    vTaskDelete(NULL);
}

void vSensorInitTask(void *pvParameters) {
    // Simulate sensor initialization
    printf("Sensors: Starting initialization...\n");
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    // Sensors are ready
    xEventGroupSetBits(xSystemEventGroup, SENSOR_READY_BIT);
    printf("Sensors: Ready!\n");
    
    vTaskDelete(NULL);
}

void vDatabaseInitTask(void *pvParameters) {
    // Simulate database initialization
    printf("Database: Starting initialization...\n");
    vTaskDelay(pdMS_TO_TICKS(800));
    
    // Database is ready
    xEventGroupSetBits(xSystemEventGroup, DATABASE_READY_BIT);
    printf("Database: Ready!\n");
    
    vTaskDelete(NULL);
}

void vMainApplicationTask(void *pvParameters) {
    printf("Main App: Waiting for all subsystems...\n");
    
    // Wait for ALL three bits to be set
    EventBits_t uxBits = xEventGroupWaitBits(
        xSystemEventGroup,      // Event group handle
        ALL_READY_BITS,         // Bits to wait for
        pdFALSE,                // Don't clear bits on exit
        pdTRUE,                 // Wait for ALL bits (AND condition)
        portMAX_DELAY           // Wait indefinitely
    );
    
    if ((uxBits & ALL_READY_BITS) == ALL_READY_BITS) {
        printf("Main App: All systems ready - starting application!\n");
        
        while (1) {
            // Main application logic here
            printf("Main App: Running...\n");
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

void app_main(void) {
    // Create the event group
    xSystemEventGroup = xEventGroupCreate();
    
    if (xSystemEventGroup != NULL) {
        // Create initialization tasks
        xTaskCreate(vWiFiInitTask, "WiFi", 2048, NULL, 2, NULL);
        xTaskCreate(vSensorInitTask, "Sensor", 2048, NULL, 2, NULL);
        xTaskCreate(vDatabaseInitTask, "Database", 2048, NULL, 2, NULL);
        xTaskCreate(vMainApplicationTask, "MainApp", 2048, NULL, 1, NULL);
    }
}