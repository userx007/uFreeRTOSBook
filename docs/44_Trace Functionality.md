# FreeRTOS Trace Functionality

## Overview

FreeRTOS trace functionality provides a powerful mechanism for capturing and analyzing runtime behavior of your real-time system. It allows developers to record detailed information about task execution, context switches, interrupt activity, queue operations, and other kernel events. This data is invaluable for debugging, performance optimization, and understanding timing relationships in complex multitasking systems.

## Trace Macros System

FreeRTOS includes a comprehensive set of trace macros that are strategically placed throughout the kernel code. These macros act as instrumentation points that fire when specific kernel events occur.

### Key Trace Macro Categories

**Task-related macros:**
- `traceTASK_SWITCHED_IN()` - Called when a task begins executing
- `traceTASK_SWITCHED_OUT()` - Called when a task stops executing
- `traceTASK_CREATE()` - Called when a task is created
- `traceTASK_DELETE()` - Called when a task is deleted
- `traceTASK_DELAY()` - Called when a task enters blocked state due to delay
- `traceTASK_PRIORITY_SET()` - Called when task priority changes

**Queue-related macros:**
- `traceQUEUE_SEND()` - Called when data is sent to a queue
- `traceQUEUE_RECEIVE()` - Called when data is received from a queue
- `traceQUEUE_SEND_FAILED()` - Called when queue send fails
- `traceQUEUE_RECEIVE_FAILED()` - Called when queue receive fails

**Interrupt-related macros:**
- `traceISR_ENTER()` - Called when entering an ISR
- `traceISR_EXIT()` - Called when exiting an ISR

### Enabling Trace Macros

By default, trace macros are defined as empty macros that compile to nothing. To enable tracing, you need to:

1. Set `configUSE_TRACE_FACILITY` to `1` in FreeRTOSConfig.h
2. Define the trace macros to call your trace recording functions

```c
/* FreeRTOSConfig.h */
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

/* Include trace recorder header if using Tracealyzer */
#include "trcRecorder.h"
```

## Integration with Tracealyzer

Tracealyzer by Percepio is one of the most popular trace visualization tools for FreeRTOS. It provides timeline views, CPU load graphs, and detailed event analysis.

### Basic Tracealyzer Setup

```c
/* main.c */
#include "FreeRTOS.h"
#include "task.h"
#include "trcRecorder.h"

/* Task handles */
TaskHandle_t xSensorTask;
TaskHandle_t xProcessTask;
TaskHandle_t xDisplayTask;

/* Queue handle */
QueueHandle_t xDataQueue;

void vSensorTask(void *pvParameters)
{
    uint32_t sensorData;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        /* Read sensor - simulate with counter */
        sensorData = HAL_ReadSensor();
        
        /* Send to queue with trace */
        if(xQueueSend(xDataQueue, &sensorData, pdMS_TO_TICKS(10)) == pdPASS)
        {
            /* Trace macro automatically called by FreeRTOS */
            vTracePrint(xSensorTask, "Sensor data sent: %d", sensorData);
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}

void vProcessTask(void *pvParameters)
{
    uint32_t receivedData;
    uint32_t processedData;
    
    for(;;)
    {
        if(xQueueReceive(xDataQueue, &receivedData, portMAX_DELAY) == pdPASS)
        {
            vTracePrint(xProcessTask, "Processing data: %d", receivedData);
            
            /* Simulate processing time */
            processedData = receivedData * 2;
            vTaskDelay(pdMS_TO_TICKS(50));
            
            vTracePrint(xProcessTask, "Processing complete: %d", processedData);
        }
    }
}

void vDisplayTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        vTracePrint(xDisplayTask, "Updating display");
        
        /* Update display */
        HAL_UpdateDisplay();
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
    }
}

int main(void)
{
    /* Initialize hardware */
    HAL_Init();
    
    /* Initialize trace recorder - must be done before scheduler starts */
    vTraceEnable(TRC_START);
    
    /* Create queue */
    xDataQueue = xQueueCreate(10, sizeof(uint32_t));
    vTraceSetQueueName(xDataQueue, "DataQueue");
    
    /* Create tasks */
    xTaskCreate(vSensorTask, "Sensor", 200, NULL, 3, &xSensorTask);
    xTaskCreate(vProcessTask, "Process", 200, NULL, 2, &xProcessTask);
    xTaskCreate(vDisplayTask, "Display", 200, NULL, 1, &xDisplayTask);
    
    /* Start scheduler */
    vTaskStartScheduler();
    
    /* Should never reach here */
    for(;;);
}
```

### Advanced Tracealyzer Features

**User Events:**
```c
/* Define custom event channels */
traceString chn_motor = xTraceRegisterString("Motor Control");
traceString chn_network = xTraceRegisterString("Network Events");

void vMotorControlTask(void *pvParameters)
{
    int speed = 0;
    
    for(;;)
    {
        speed = CalculateMotorSpeed();
        
        /* Log custom event with data */
        vTracePrintF(chn_motor, "Speed set to %d RPM", speed);
        
        SetMotorSpeed(speed);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**State Machines Tracking:**
```c
typedef enum {
    STATE_IDLE,
    STATE_CONNECTING,
    STATE_CONNECTED,
    STATE_TRANSMITTING,
    STATE_ERROR
} NetworkState_t;

void vNetworkTask(void *pvParameters)
{
    NetworkState_t currentState = STATE_IDLE;
    traceString states[5];
    
    /* Register state names */
    states[STATE_IDLE] = xTraceRegisterString("IDLE");
    states[STATE_CONNECTING] = xTraceRegisterString("CONNECTING");
    states[STATE_CONNECTED] = xTraceRegisterString("CONNECTED");
    states[STATE_TRANSMITTING] = xTraceRegisterString("TRANSMITTING");
    states[STATE_ERROR] = xTraceRegisterString("ERROR");
    
    for(;;)
    {
        switch(currentState)
        {
            case STATE_IDLE:
                vTracePrint(chn_network, "Entering IDLE state");
                /* Wait for connection request */
                break;
                
            case STATE_CONNECTING:
                vTracePrint(chn_network, "Entering CONNECTING state");
                if(AttemptConnection())
                {
                    currentState = STATE_CONNECTED;
                }
                else
                {
                    currentState = STATE_ERROR;
                }
                break;
                
            case STATE_CONNECTED:
                vTracePrint(chn_network, "Entering CONNECTED state");
                currentState = STATE_TRANSMITTING;
                break;
                
            case STATE_TRANSMITTING:
                vTracePrint(chn_network, "Transmitting data");
                TransmitData();
                break;
                
            case STATE_ERROR:
                vTracePrint(chn_network, "ERROR state - resetting");
                currentState = STATE_IDLE;
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## Integration with SEGGER SystemView

SystemView is another popular trace tool that provides real-time visualization of system behavior.

### SystemView Configuration

```c
/* FreeRTOSConfig.h */
#define configUSE_TRACE_FACILITY                1
#define INCLUDE_uxTaskGetStackHighWaterMark     1

/* Include SystemView */
#include "SEGGER_SYSVIEW_FreeRTOS.h"

/* Define trace macros for SystemView */
#define traceTASK_SWITCHED_IN() SEGGER_SYSVIEW_OnTaskStartExec(pxCurrentTCB->uxTCBNumber)
#define traceTASK_SWITCHED_OUT() SEGGER_SYSVIEW_OnTaskStopExec()
```

### SystemView Implementation Example

```c
/* main.c with SystemView */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "SEGGER_SYSVIEW.h"

/* Semaphore for resource protection */
SemaphoreHandle_t xResourceSemaphore;

void vHighPriorityTask(void *pvParameters)
{
    for(;;)
    {
        SEGGER_SYSVIEW_PrintfTarget("HighPrio: Waiting for resource");
        
        if(xSemaphoreTake(xResourceSemaphore, pdMS_TO_TICKS(100)) == pdPASS)
        {
            SEGGER_SYSVIEW_PrintfTarget("HighPrio: Resource acquired");
            
            /* Use resource */
            vTaskDelay(pdMS_TO_TICKS(30));
            
            SEGGER_SYSVIEW_PrintfTarget("HighPrio: Resource released");
            xSemaphoreGive(xResourceSemaphore);
        }
        else
        {
            SEGGER_SYSVIEW_PrintfTarget("HighPrio: Timeout waiting for resource");
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void vLowPriorityTask(void *pvParameters)
{
    for(;;)
    {
        SEGGER_SYSVIEW_PrintfTarget("LowPrio: Waiting for resource");
        
        if(xSemaphoreTake(xResourceSemaphore, portMAX_DELAY) == pdPASS)
        {
            SEGGER_SYSVIEW_PrintfTarget("LowPrio: Resource acquired");
            
            /* Use resource for longer period */
            vTaskDelay(pdMS_TO_TICKS(100));
            
            SEGGER_SYSVIEW_PrintfTarget("LowPrio: Resource released");
            xSemaphoreGive(xResourceSemaphore);
        }
        
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

int main(void)
{
    HAL_Init();
    
    /* Initialize SystemView */
    SEGGER_SYSVIEW_Conf();
    SEGGER_SYSVIEW_Start();
    
    /* Create semaphore */
    xResourceSemaphore = xSemaphoreCreateMutex();
    
    /* Create tasks */
    xTaskCreate(vHighPriorityTask, "HighPrio", 200, NULL, 3, NULL);
    xTaskCreate(vLowPriorityTask, "LowPrio", 200, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    for(;;);
}
```

## Custom Trace Implementation

For systems where commercial tools aren't suitable, you can implement custom tracing:

```c
/* custom_trace.h */
#ifndef CUSTOM_TRACE_H
#define CUSTOM_TRACE_H

#include <stdint.h>

typedef enum {
    EVENT_TASK_SWITCHED_IN,
    EVENT_TASK_SWITCHED_OUT,
    EVENT_TASK_CREATED,
    EVENT_QUEUE_SEND,
    EVENT_QUEUE_RECEIVE,
    EVENT_ISR_ENTER,
    EVENT_ISR_EXIT
} TraceEvent_t;

typedef struct {
    uint32_t timestamp;
    TraceEvent_t eventType;
    void *objectHandle;
    uint32_t param1;
    uint32_t param2;
} TraceRecord_t;

#define TRACE_BUFFER_SIZE 1000

void vTraceInit(void);
void vTraceRecordEvent(TraceEvent_t event, void *handle, uint32_t p1, uint32_t p2);
void vTraceDump(void);

#endif
```

```c
/* custom_trace.c */
#include "custom_trace.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

static TraceRecord_t traceBuffer[TRACE_BUFFER_SIZE];
static volatile uint32_t traceIndex = 0;
static volatile uint32_t traceOverflow = 0;

void vTraceInit(void)
{
    traceIndex = 0;
    traceOverflow = 0;
}

void vTraceRecordEvent(TraceEvent_t event, void *handle, uint32_t p1, uint32_t p2)
{
    if(traceIndex < TRACE_BUFFER_SIZE)
    {
        traceBuffer[traceIndex].timestamp = xTaskGetTickCount();
        traceBuffer[traceIndex].eventType = event;
        traceBuffer[traceIndex].objectHandle = handle;
        traceBuffer[traceIndex].param1 = p1;
        traceBuffer[traceIndex].param2 = p2;
        traceIndex++;
    }
    else
    {
        traceOverflow = 1;
    }
}

void vTraceDump(void)
{
    printf("\n=== Trace Dump ===\n");
    printf("Total events: %lu\n", traceIndex);
    printf("Overflow: %s\n\n", traceOverflow ? "YES" : "NO");
    
    for(uint32_t i = 0; i < traceIndex; i++)
    {
        printf("[%lu] ", traceBuffer[i].timestamp);
        
        switch(traceBuffer[i].eventType)
        {
            case EVENT_TASK_SWITCHED_IN:
                printf("TASK_IN: %p\n", traceBuffer[i].objectHandle);
                break;
            case EVENT_TASK_SWITCHED_OUT:
                printf("TASK_OUT: %p\n", traceBuffer[i].objectHandle);
                break;
            case EVENT_QUEUE_SEND:
                printf("QUEUE_SEND: %p\n", traceBuffer[i].objectHandle);
                break;
            default:
                printf("UNKNOWN\n");
                break;
        }
    }
}

/* FreeRTOSConfig.h trace macro definitions */
#define traceTASK_SWITCHED_IN() \
    vTraceRecordEvent(EVENT_TASK_SWITCHED_IN, pxCurrentTCB, 0, 0)

#define traceTASK_SWITCHED_OUT() \
    vTraceRecordEvent(EVENT_TASK_SWITCHED_OUT, pxCurrentTCB, 0, 0)

#define traceQUEUE_SEND(pxQueue) \
    vTraceRecordEvent(EVENT_QUEUE_SEND, pxQueue, 0, 0)
```

## Analyzing System Behavior

### Identifying Priority Inversion

```c
/* Example demonstrating priority inversion detection via trace */
SemaphoreHandle_t xSharedResource;

void vLowPriorityTaskPI(void *pvParameters)
{
    for(;;)
    {
        xSemaphoreTake(xSharedResource, portMAX_DELAY);
        vTracePrint(NULL, "LowPrio: Holding resource");
        
        /* Long operation */
        vTaskDelay(pdMS_TO_TICKS(100));
        
        xSemaphoreGive(xSharedResource);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vMediumPriorityTaskPI(void *pvParameters)
{
    for(;;)
    {
        vTracePrint(NULL, "MediumPrio: Running compute task");
        
        /* CPU-intensive work */
        for(volatile int i = 0; i < 100000; i++);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void vHighPriorityTaskPI(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(50)); // Let low priority task acquire resource
    
    for(;;)
    {
        vTracePrint(NULL, "HighPrio: Need resource NOW");
        
        xSemaphoreTake(xSharedResource, portMAX_DELAY);
        vTracePrint(NULL, "HighPrio: Got resource (finally!)");
        
        xSemaphoreGive(xSharedResource);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
/* When analyzed in trace tool, you'll see HighPrio blocked 
   while MediumPrio runs, even though HighPrio has higher priority */
```

### CPU Load Analysis

Trace tools can show you exact CPU utilization per task, helping identify performance bottlenecks and optimize task timing.

### Timing Validation

Use traces to verify that real-time deadlines are being met and identify worst-case execution times for critical tasks.

## Best Practices

1. **Enable tracing early in development** to catch timing issues before they become problems
2. **Use meaningful task and object names** for easier trace interpretation
3. **Add custom user events** at critical points in your application logic
4. **Monitor trace buffer usage** to ensure you're capturing all relevant events
5. **Correlate traces with logic analyzer data** for complete system visibility
6. **Use snapshot mode** for continuous monitoring without stopping the system
7. **Archive traces** of issues for later analysis and regression testing

Trace functionality transforms FreeRTOS development from guesswork into data-driven optimization, making it an essential tool for professional embedded systems development.