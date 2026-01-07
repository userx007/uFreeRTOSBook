#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"
#include <stdio.h>
#include <string.h>

#define LOGGER_BUFFER_SIZE 512

// Message buffer handle
MessageBufferHandle_t xLoggerMessageBuffer;

// Sensor data structures (variable sizes)
typedef struct
{
    uint8_t sensorId;
    uint32_t timestamp;
    float temperature;
} TemperatureSensor_t;

typedef struct
{
    uint8_t sensorId;
    uint32_t timestamp;
    float x, y, z;
} AccelerometerSensor_t;

typedef struct
{
    uint8_t sensorId;
    uint32_t timestamp;
    uint16_t dataLength;
    uint8_t rawData[32]; // Variable up to 32 bytes
} GenericSensor_t;

// Temperature sensor task
void vTemperatureSensorTask(void *pvParameters)
{
    TemperatureSensor_t tempData;
    uint32_t timestamp = 0;
    
    for (;;)
    {
        // Simulate reading temperature
        tempData.sensorId = 1;
        tempData.timestamp = timestamp++;
        tempData.temperature = 20.0f + (float)(timestamp % 10);
        
        // Send variable-length message
        xMessageBufferSend(
            xLoggerMessageBuffer,
            &tempData,
            sizeof(TemperatureSensor_t),
            pdMS_TO_TICKS(100)
        );
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Accelerometer sensor task
void vAccelerometerSensorTask(void *pvParameters)
{
    AccelerometerSensor_t accelData;
    uint32_t timestamp = 0;
    
    for (;;)
    {
        // Simulate reading accelerometer
        accelData.sensorId = 2;
        accelData.timestamp = timestamp++;
        accelData.x = (float)(timestamp % 100) / 10.0f;
        accelData.y = (float)((timestamp + 30) % 100) / 10.0f;
        accelData.z = 9.8f;
        
        xMessageBufferSend(
            xLoggerMessageBuffer,
            &accelData,
            sizeof(AccelerometerSensor_t),
            pdMS_TO_TICKS(100)
        );
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Generic sensor with variable data length
void vGenericSensorTask(void *pvParameters)
{
    GenericSensor_t genericData;
    uint32_t timestamp = 0;
    
    for (;;)
    {
        // Simulate variable-length sensor data
        genericData.sensorId = 3;
        genericData.timestamp = timestamp++;
        genericData.dataLength = (timestamp % 20) + 5; // 5-24 bytes
        
        for (int i = 0; i < genericData.dataLength; i++)
        {
            genericData.rawData[i] = i + timestamp;
        }
        
        // Send only the actual data (not the full array)
        size_t messageSize = sizeof(uint8_t) + sizeof(uint32_t) + 
                            sizeof(uint16_t) + genericData.dataLength;
        
        xMessageBufferSend(
            xLoggerMessageBuffer,
            &genericData,
            messageSize,
            pdMS_TO_TICKS(100)
        );
        
        vTaskDelay(pdMS_TO_TICKS(750));
    }
}

// Logger task that receives all sensor data
void vLoggerTask(void *pvParameters)
{
    uint8_t rxBuffer[128];
    size_t xReceivedBytes;
    
    for (;;)
    {
        xReceivedBytes = xMessageBufferReceive(
            xLoggerMessageBuffer,
            rxBuffer,
            sizeof(rxBuffer),
            portMAX_DELAY
        );
        
        if (xReceivedBytes > 0)
        {
            uint8_t sensorId = rxBuffer[0];
            uint32_t timestamp = *(uint32_t*)(&rxBuffer[1]);
            
            printf("[LOG] Sensor %d at time %lu: ", sensorId, timestamp);
            
            // Parse based on known sensor types
            if (sensorId == 1 && xReceivedBytes == sizeof(TemperatureSensor_t))
            {
                TemperatureSensor_t *pTemp = (TemperatureSensor_t*)rxBuffer;
                printf("Temperature = %.2f°C\n", pTemp->temperature);
            }
            else if (sensorId == 2 && xReceivedBytes == sizeof(AccelerometerSensor_t))
            {
                AccelerometerSensor_t *pAccel = (AccelerometerSensor_t*)rxBuffer;
                printf("Accel X=%.2f Y=%.2f Z=%.2f\n", 
                       pAccel->x, pAccel->y, pAccel->z);
            }
            else if (sensorId == 3)
            {
                GenericSensor_t *pGeneric = (GenericSensor_t*)rxBuffer;
                printf("Generic data (%d bytes): ", pGeneric->dataLength);
                for (int i = 0; i < pGeneric->dataLength && i < 10; i++)
                {
                    printf("%02X ", pGeneric->rawData[i]);
                }
                printf("\n");
            }
        }
    }
}

// Main application
int main(void)
{
    // Create message buffer
    xLoggerMessageBuffer = xMessageBufferCreate(LOGGER_BUFFER_SIZE);
    
    if (xLoggerMessageBuffer != NULL)
    {
        // Create sensor tasks
        xTaskCreate(vTemperatureSensorTask, "TempSensor", 200, NULL, 2, NULL);
        xTaskCreate(vAccelerometerSensorTask, "AccelSensor", 200, NULL, 2, NULL);
        xTaskCreate(vGenericSensorTask, "GenericSensor", 200, NULL, 2, NULL);
        
        // Create logger task with higher priority
        xTaskCreate(vLoggerTask, "Logger", 300, NULL, 3, NULL);
        
        // Start scheduler
        vTaskStartScheduler();
    }
    
    // Should never reach here
    for (;;);
    return 0;
}