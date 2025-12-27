# FreeRTOS Co-routines: Lightweight Concurrency for Memory-Constrained Systems

## Overview

Co-routines in FreeRTOS are a lightweight alternative to tasks designed specifically for severely memory-constrained systems. While tasks each have their own stack and operate with full preemption, co-routines share a single stack and use cooperative multitasking, making them much more memory-efficient but with important behavioral differences.

## Key Characteristics

Co-routines differ fundamentally from tasks in several ways:

**Memory efficiency**: All co-routines share a single stack, drastically reducing RAM usage compared to tasks where each requires its own stack (typically 512-2048 bytes minimum).

**Cooperative scheduling**: Co-routines must explicitly yield control back to the scheduler. They cannot be preempted mid-execution like tasks, meaning a co-routine runs until it voluntarily gives up control.

**Limited functionality**: Co-routines have a more restricted API compared to tasks. They can't use certain FreeRTOS features that tasks can, and their programming model is more constrained.

**Execution context**: When a co-routine yields, its execution state must be saved in variables rather than on a stack, requiring careful programming with specific macros.

## Implementation Details

Co-routines are implemented using a technique called Duff's device, which allows a function to resume execution from where it last yielded. FreeRTOS provides specific macros to handle this:

**crSTART()**: Must be called at the beginning of every co-routine function.

**crEND()**: Must be called at the end of every co-routine function.

**crDELAY()**: Yields control for a specified number of ticks.

**crQUEUE_SEND()** and **crQUEUE_RECEIVE()**: Non-blocking queue operations for co-routines.

Here's a basic example structure:

```c
void vCoRoutineFunction(CoRoutineHandle_t xHandle, 
                         UBaseType_t uxIndex)
{
    // Co-routine state must be static or global
    static uint32_t ulCounter;
    
    crSTART(xHandle);
    
    for(;;)
    {
        // Do some work
        ulCounter++;
        
        // Yield for 100 ticks
        crDELAY(xHandle, 100);
        
        // More work
        // Process ulCounter
    }
    
    crEND();
}
```

## Practical Examples

### Example 1: LED Blinking Co-routines

This example shows multiple co-routines controlling different LEDs with minimal memory overhead:

```c
#include "FreeRTOS.h"
#include "croutine.h"

#define LED1_PIN 1
#define LED2_PIN 2
#define LED3_PIN 3

// Co-routine to blink LED1 rapidly
void vLED1Blink(CoRoutineHandle_t xHandle, UBaseType_t uxIndex)
{
    static uint8_t ledState = 0;
    
    crSTART(xHandle);
    
    for(;;)
    {
        ledState = !ledState;
        GPIO_Write(LED1_PIN, ledState);
        
        // Yield for 200ms
        crDELAY(xHandle, pdMS_TO_TICKS(200));
    }
    
    crEND();
}

// Co-routine to blink LED2 slowly
void vLED2Blink(CoRoutineHandle_t xHandle, UBaseType_t uxIndex)
{
    static uint8_t ledState = 0;
    
    crSTART(xHandle);
    
    for(;;)
    {
        ledState = !ledState;
        GPIO_Write(LED2_PIN, ledState);
        
        // Yield for 1000ms
        crDELAY(xHandle, pdMS_TO_TICKS(1000));
    }
    
    crEND();
}

// Co-routine to create a heartbeat pattern
void vHeartbeatPattern(CoRoutineHandle_t xHandle, UBaseType_t uxIndex)
{
    crSTART(xHandle);
    
    for(;;)
    {
        // First pulse
        GPIO_Write(LED3_PIN, 1);
        crDELAY(xHandle, pdMS_TO_TICKS(100));
        GPIO_Write(LED3_PIN, 0);
        crDELAY(xHandle, pdMS_TO_TICKS(100));
        
        // Second pulse
        GPIO_Write(LED3_PIN, 1);
        crDELAY(xHandle, pdMS_TO_TICKS(100));
        GPIO_Write(LED3_PIN, 0);
        
        // Long pause
        crDELAY(xHandle, pdMS_TO_TICKS(800));
    }
    
    crEND();
}

int main(void)
{
    // Initialize hardware
    GPIO_Init();
    
    // Create co-routines with priority 0
    xCoRoutineCreate(vLED1Blink, 0, 0);
    xCoRoutineCreate(vLED2Blink, 0, 1);
    xCoRoutineCreate(vHeartbeatPattern, 0, 2);
    
    // Start the scheduler
    vCoRoutineSchedule();
    vTaskStartScheduler();
    
    // Should never reach here
    for(;;);
}
```

### Example 2: Sensor Reading with Queue Communication

This example demonstrates co-routines communicating through queues:

```c
#include "FreeRTOS.h"
#include "croutine.h"
#include "queue.h"

#define QUEUE_LENGTH 5

static QueueHandle_t xSensorQueue;

// Co-routine that reads temperature sensor
void vTemperatureSensor(CoRoutineHandle_t xHandle, UBaseType_t uxIndex)
{
    static int16_t temperature;
    static BaseType_t xResult;
    
    crSTART(xHandle);
    
    for(;;)
    {
        // Read temperature from sensor (simulated)
        temperature = ReadTemperatureSensor();
        
        // Try to send to queue (non-blocking)
        crQUEUE_SEND(xHandle, 
                     xSensorQueue, 
                     &temperature, 
                     0,  // No block time
                     &xResult);
        
        if(xResult == pdPASS)
        {
            // Successfully sent
        }
        
        // Yield for 500ms between readings
        crDELAY(xHandle, pdMS_TO_TICKS(500));
    }
    
    crEND();
}

// Co-routine that processes sensor data
void vDataProcessor(CoRoutineHandle_t xHandle, UBaseType_t uxIndex)
{
    static int16_t receivedTemp;
    static BaseType_t xResult;
    static uint32_t averageTemp = 0;
    static uint8_t sampleCount = 0;
    
    crSTART(xHandle);
    
    for(;;)
    {
        // Try to receive from queue (non-blocking)
        crQUEUE_RECEIVE(xHandle, 
                        xSensorQueue, 
                        &receivedTemp, 
                        0,  // No block time
                        &xResult);
        
        if(xResult == pdPASS)
        {
            // Update running average
            averageTemp = ((averageTemp * sampleCount) + receivedTemp) 
                          / (sampleCount + 1);
            sampleCount++;
            
            if(sampleCount >= 10)
            {
                // Process average every 10 samples
                ProcessTemperatureAverage(averageTemp);
                sampleCount = 0;
                averageTemp = 0;
            }
        }
        
        // Yield briefly
        crDELAY(xHandle, pdMS_TO_TICKS(10));
    }
    
    crEND();
}

int main(void)
{
    // Create queue for sensor data
    xSensorQueue = xQueueCreate(QUEUE_LENGTH, sizeof(int16_t));
    
    if(xSensorQueue != NULL)
    {
        // Create co-routines
        xCoRoutineCreate(vTemperatureSensor, 0, 0);
        xCoRoutineCreate(vDataProcessor, 0, 1);
        
        // Start scheduler
        vCoRoutineSchedule();
        vTaskStartScheduler();
    }
    
    for(;;);
}
```

### Example 3: State Machine Implementation

Co-routines work well for implementing state machines:

```c
#include "FreeRTOS.h"
#include "croutine.h"

typedef enum {
    STATE_IDLE,
    STATE_WARM_UP,
    STATE_ACTIVE,
    STATE_COOLDOWN,
    STATE_ERROR
} SystemState_t;

void vSystemController(CoRoutineHandle_t xHandle, UBaseType_t uxIndex)
{
    static SystemState_t currentState = STATE_IDLE;
    static uint32_t stateTimer = 0;
    static uint8_t errorCode = 0;
    
    crSTART(xHandle);
    
    for(;;)
    {
        switch(currentState)
        {
            case STATE_IDLE:
                // Wait for start signal
                if(CheckStartButton())
                {
                    currentState = STATE_WARM_UP;
                    stateTimer = 0;
                    InitializeSystem();
                }
                crDELAY(xHandle, pdMS_TO_TICKS(100));
                break;
                
            case STATE_WARM_UP:
                // Warm up for 5 seconds
                if(stateTimer++ >= 50)  // 50 * 100ms = 5s
                {
                    if(SystemReady())
                    {
                        currentState = STATE_ACTIVE;
                        stateTimer = 0;
                    }
                    else
                    {
                        currentState = STATE_ERROR;
                        errorCode = 1;
                    }
                }
                IndicateWarmingUp();
                crDELAY(xHandle, pdMS_TO_TICKS(100));
                break;
                
            case STATE_ACTIVE:
                // Normal operation
                PerformMainFunction();
                
                if(CheckStopButton())
                {
                    currentState = STATE_COOLDOWN;
                    stateTimer = 0;
                }
                else if(DetectError())
                {
                    currentState = STATE_ERROR;
                    errorCode = 2;
                }
                
                crDELAY(xHandle, pdMS_TO_TICKS(50));
                break;
                
            case STATE_COOLDOWN:
                // Cool down for 3 seconds
                if(stateTimer++ >= 30)  // 30 * 100ms = 3s
                {
                    currentState = STATE_IDLE;
                    ShutdownSystem();
                }
                IndicateCoolingDown();
                crDELAY(xHandle, pdMS_TO_TICKS(100));
                break;
                
            case STATE_ERROR:
                // Handle error condition
                DisplayError(errorCode);
                
                if(CheckResetButton())
                {
                    currentState = STATE_IDLE;
                    ClearError();
                }
                
                crDELAY(xHandle, pdMS_TO_TICKS(200));
                break;
        }
    }
    
    crEND();
}
```

## Tasks vs Co-routines: When to Use Each

### Use Tasks When:

- You have sufficient RAM (generally 8KB+ available)
- You need preemptive multitasking
- You require priority-based scheduling with true preemption
- You need to use blocking API calls
- You want simpler, more straightforward code
- You need stack-intensive operations or deep function call chains

### Use Co-routines When:

- RAM is severely limited (less than 4KB available)
- You have many simple, periodic operations
- You can accept cooperative scheduling
- Your "tasks" are naturally structured as state machines
- You don't need deep function nesting within the co-routine
- Low-power operation is critical (fewer context switches)

## Important Limitations and Considerations

**All variables must be static**: Local variables in co-routines don't persist across yields, so any state must be declared static.

**No blocking calls**: Co-routines cannot use blocking FreeRTOS API calls. They must use the non-blocking co-routine equivalents.

**Function calls are problematic**: Calling functions from within a co-routine can be tricky because the shared stack means function call state isn't preserved across yields.

**Priority inversion**: Since co-routines are cooperative, a lower-priority co-routine that doesn't yield will block higher-priority ones.

**Limited debugging**: The macro-based implementation makes debugging more difficult than with regular tasks.

## Configuration Requirements

To use co-routines, you need to configure FreeRTOSConfig.h:

```c
// Enable co-routine functionality
#define configUSE_CO_ROUTINES           1

// Maximum number of co-routine priorities
#define configMAX_CO_ROUTINE_PRIORITIES 2
```

## Real-World Application Scenario

Co-routines are ideal for systems like:

- **Tiny IoT sensors**: A battery-powered sensor node with 2KB RAM running on an 8-bit microcontroller, managing multiple sensor readings, LED indicators, and low-power radio communication.

- **Simple industrial controllers**: A PLC-like device with minimal RAM that needs to monitor multiple inputs and control outputs with straightforward logic.

- **Legacy system upgrades**: Adding RTOS capabilities to an existing bare-metal application on hardware with very limited resources.

Co-routines represent an important tool in the FreeRTOS ecosystem for extreme memory optimization, though modern projects with adequate resources typically favor the more flexible task-based approach. Understanding co-routines helps you make informed decisions when every byte of RAM counts.