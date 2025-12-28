# FreeRTOS Power Management

Power management is critical for battery-operated and energy-efficient embedded systems. FreeRTOS provides several mechanisms to reduce power consumption while maintaining system responsiveness.

## Core Concepts

### 1. Tickless Idle Mode

When all tasks are blocked (waiting for events, delays, or semaphores), the system enters idle mode. Normally, the scheduler still generates periodic tick interrupts, which wake the processor unnecessarily. **Tickless idle mode** suppresses these tick interrupts during idle periods, allowing the processor to sleep longer.

**How it works:**
- The idle task calculates how long the system can sleep (until the next task unblocks)
- It stops the tick interrupt and configures a timer to wake the system just before the next task needs to run
- The processor enters a low-power sleep mode
- Upon waking, the tick count is corrected to account for the sleep duration

### 2. Sleep Modes

Most microcontrollers offer multiple sleep modes with varying power savings and wake-up latencies:
- **Sleep/Idle mode**: CPU stops, peripherals continue
- **Deep sleep**: CPU and most peripherals stop, RAM retained
- **Standby/Shutdown**: Maximum power savings, limited retention

### 3. Power Consumption Trade-offs

The balance between power savings and responsiveness involves:
- **Sleep depth**: Deeper sleep saves more power but increases wake-up latency
- **Sleep duration**: Frequent short sleeps may consume more power than staying awake
- **Peripheral management**: Disabling unused peripherals during sleep

## Configuration and Implementation

### Enabling Tickless Idle

In `FreeRTOSConfig.h`:

```c
// Enable tickless idle mode
#define configUSE_TICKLESS_IDLE 1

// Minimum idle period for entering tickless mode (in ticks)
// Below this threshold, normal idle is used
#define configEXPECTED_IDLE_TIME_BEFORE_SLEEP 2

// Expected idle time before entering low-power mode
#define configPRE_SLEEP_PROCESSING(x)  /* Optional pre-sleep hook */
#define configPOST_SLEEP_PROCESSING(x) /* Optional post-sleep hook */
```

### Example 1: Basic Tickless Idle Implementation

This example shows a simple sensor monitoring application using tickless idle:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Simulated hardware functions
void HAL_EnterSleepMode(void) {
    // Configure MCU for sleep mode
    // SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    __WFI(); // Wait For Interrupt
}

void HAL_DisablePeripherals(void) {
    // Disable unnecessary peripherals to save power
    // ADC, unused timers, etc.
}

void HAL_EnablePeripherals(void) {
    // Re-enable peripherals after wake-up
}

// Pre-sleep processing hook
void vApplicationSleep(TickType_t xExpectedIdleTime) {
    // Called before entering sleep
    HAL_DisablePeripherals();
}

// Post-sleep processing hook  
void vApplicationWake(TickType_t xExpectedIdleTime) {
    // Called after waking from sleep
    HAL_EnablePeripherals();
}

// Sensor task - runs periodically
void vSensorTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(5000); // 5 seconds
    
    for(;;) {
        // Read sensor data
        uint16_t sensorValue = readSensor();
        
        // Process data
        if(sensorValue > THRESHOLD) {
            // Handle alert condition
            triggerAlert();
        }
        
        // Sleep until next period (enables tickless idle)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// Event-driven task - waits for external events
void vEventTask(void *pvParameters) {
    SemaphoreHandle_t xEventSemaphore = (SemaphoreHandle_t)pvParameters;
    
    for(;;) {
        // Block waiting for event (allows system to sleep)
        if(xSemaphoreTake(xEventSemaphore, portMAX_DELAY) == pdTRUE) {
            // Handle event
            processEvent();
        }
    }
}

int main(void) {
    // Create semaphore for event signaling
    SemaphoreHandle_t xEventSem = xSemaphoreCreateBinary();
    
    // Create tasks
    xTaskCreate(vSensorTask, "Sensor", 128, NULL, 2, NULL);
    xTaskCreate(vEventTask, "Event", 128, (void*)xEventSem, 2, NULL);
    
    // Start scheduler (enables tickless idle automatically)
    vTaskStartScheduler();
    
    return 0;
}
```

### Example 2: Custom Tickless Implementation with Power Modes

For advanced power management, you can implement custom tickless idle functions:

```c
#include "FreeRTOS.h"
#include "task.h"

// Power mode definitions
typedef enum {
    POWER_MODE_RUN,
    POWER_MODE_SLEEP,
    POWER_MODE_DEEP_SLEEP
} PowerMode_t;

// Determine appropriate power mode based on expected idle time
PowerMode_t determinePowerMode(TickType_t xExpectedIdleTime) {
    if(xExpectedIdleTime < pdMS_TO_TICKS(10)) {
        return POWER_MODE_RUN; // Too short to benefit from sleep
    }
    else if(xExpectedIdleTime < pdMS_TO_TICKS(100)) {
        return POWER_MODE_SLEEP; // Short sleep
    }
    else {
        return POWER_MODE_DEEP_SLEEP; // Long sleep for maximum savings
    }
}

// Custom tickless idle implementation
void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime) {
    TickType_t xModifiableIdleTime;
    PowerMode_t powerMode;
    
    // Disable interrupts while preparing to sleep
    __disable_irq();
    
    // Ensure it's still worth sleeping
    if(eTaskConfirmSleepModeStatus() == eAbortSleep) {
        __enable_irq();
        return;
    }
    
    // Determine power mode
    powerMode = determinePowerMode(xExpectedIdleTime);
    
    if(powerMode == POWER_MODE_RUN) {
        __enable_irq();
        return;
    }
    
    // Stop the SysTick interrupt
    portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;
    
    // Configure low-power timer to wake at the correct time
    xModifiableIdleTime = xExpectedIdleTime;
    configLPTIMER(xModifiableIdleTime);
    
    // Enter sleep mode
    __DSB(); // Data Synchronization Barrier
    __WFI(); // Wait For Interrupt
    __ISB(); // Instruction Synchronization Barrier
    
    // Recalculate how long we actually slept
    TickType_t xActualSleepTime = getLPTIMERElapsedTime();
    
    // Restart SysTick
    portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;
    
    // Correct the tick count
    vTaskStepTick(xActualSleepTime);
    
    __enable_irq();
}
```

### Example 3: Dynamic Power Management System

A more sophisticated example with multiple power strategies:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

// Power management state
typedef struct {
    uint32_t sleepCount;
    uint32_t wakeCount;
    uint32_t totalSleepTime;
    PowerMode_t currentMode;
} PowerStats_t;

static PowerStats_t powerStats = {0};

// Power-aware data acquisition task
void vDataAcquisitionTask(void *pvParameters) {
    QueueHandle_t xDataQueue = (QueueHandle_t)pvParameters;
    TickType_t xLastWakeTime;
    const TickType_t xNormalPeriod = pdMS_TO_TICKS(1000);  // 1 second
    const TickType_t xLowPowerPeriod = pdMS_TO_TICKS(10000); // 10 seconds
    TickType_t xCurrentPeriod = xNormalPeriod;
    bool lowPowerMode = false;
    
    xLastWakeTime = xTaskGetTickCount();
    
    for(;;) {
        // Read sensor
        uint16_t data = acquireData();
        
        // Send to processing queue
        xQueueSend(xDataQueue, &data, 0);
        
        // Check battery level to adjust sampling rate
        uint8_t batteryLevel = getBatteryLevel();
        
        if(batteryLevel < 20 && !lowPowerMode) {
            // Enter aggressive power saving
            xCurrentPeriod = xLowPowerPeriod;
            lowPowerMode = true;
            reduceSensorPower(); // Lower sensor accuracy for power savings
        }
        else if(batteryLevel > 30 && lowPowerMode) {
            // Exit power saving mode
            xCurrentPeriod = xNormalPeriod;
            lowPowerMode = false;
            restoreSensorPower();
        }
        
        // Sleep until next acquisition (long delay enables deep sleep)
        vTaskDelayUntil(&xLastWakeTime, xCurrentPeriod);
    }
}

// Communication task - event-driven with timeout
void vCommTask(void *pvParameters) {
    QueueHandle_t xDataQueue = (QueueHandle_t)pvParameters;
    uint16_t data;
    const TickType_t xTimeout = pdMS_TO_TICKS(30000); // 30 second timeout
    
    for(;;) {
        // Wait for data with timeout (allows long sleep periods)
        if(xQueueReceive(xDataQueue, &data, xTimeout) == pdTRUE) {
            // Data received - transmit it
            transmitData(data);
            powerStats.wakeCount++;
        }
        else {
            // Timeout - perform housekeeping if needed
            performHousekeeping();
        }
    }
}

// Watchdog refresh task (prevents deep sleep issues with external watchdog)
void vWatchdogTask(void *pvParameters) {
    const TickType_t xWatchdogPeriod = pdMS_TO_TICKS(500);
    
    for(;;) {
        refreshWatchdog();
        vTaskDelay(xWatchdogPeriod);
    }
}

// Pre-sleep hook - prepare system for sleep
void vApplicationSleep(TickType_t xExpectedIdleTime) {
    // Determine sleep mode based on expected idle time
    if(xExpectedIdleTime > pdMS_TO_TICKS(100)) {
        // Long sleep expected - enter deep sleep
        disableHighPowerPeripherals();
        configureWakeupSources();
        powerStats.sleepCount++;
    }
}

// Post-sleep hook - restore system after wake
void vApplicationWake(TickType_t xExpectedIdleTime) {
    if(xExpectedIdleTime > pdMS_TO_TICKS(100)) {
        // Waking from deep sleep
        reinitializePeripherals();
        powerStats.totalSleepTime += xExpectedIdleTime;
    }
}

// Timer callback for periodic status reporting
void vStatusTimerCallback(TimerHandle_t xTimer) {
    // Report power statistics (causes brief wake-up)
    reportPowerStats(&powerStats);
}

int main(void) {
    QueueHandle_t xDataQueue = xQueueCreate(10, sizeof(uint16_t));
    TimerHandle_t xStatusTimer;
    
    // Create tasks with appropriate priorities
    // Higher priority = wakes system more frequently
    xTaskCreate(vDataAcquisitionTask, "DAQ", 256, 
                (void*)xDataQueue, 2, NULL);
    xTaskCreate(vCommTask, "Comm", 256, 
                (void*)xDataQueue, 2, NULL);
    xTaskCreate(vWatchdogTask, "WDT", 128, NULL, 3, NULL);
    
    // Create software timer for status reporting (1 hour period)
    xStatusTimer = xTimerCreate("Status", pdMS_TO_TICKS(3600000),
                                 pdTRUE, NULL, vStatusTimerCallback);
    xTimerStart(xStatusTimer, 0);
    
    // Start scheduler
    vTaskStartScheduler();
    
    return 0;
}
```

## Best Practices for Power Management

### 1. Task Design for Low Power
- Use blocking operations (queues, semaphores) rather than polling
- Design tasks with long delay periods when possible
- Group infrequent operations to minimize wake-ups

### 2. Peripheral Management
- Disable unused peripherals before sleep
- Use DMA to reduce CPU wake-ups
- Configure wake-up sources appropriately

### 3. Clock Management
```c
void optimizeClockForSleep(TickType_t xExpectedIdleTime) {
    if(xExpectedIdleTime > pdMS_TO_TICKS(1000)) {
        // Switch to low-frequency clock before long sleep
        switchToLowSpeedClock();
    }
}
```

### 4. Power Measurement
- Monitor actual power consumption during development
- Use tickless idle statistics to verify sleep efficiency
- Balance power savings against wake-up latency requirements

### 5. Consider External Factors
- External interrupts can wake the system
- Communication protocols may require response time guarantees
- Real-time constraints may limit power saving opportunities

## Common Pitfalls

1. **Too-frequent wake-ups**: Tasks with short delays prevent deep sleep
2. **Peripheral conflicts**: Some peripherals don't support certain sleep modes
3. **Clock accuracy**: Low-power oscillators may be less accurate
4. **Debugger interference**: Debugging can prevent proper sleep mode entry
5. **Interrupt latency**: Deeper sleep modes have longer wake-up times

Power management in FreeRTOS requires careful balancing of application requirements, hardware capabilities, and energy constraints. Tickless idle mode provides an excellent foundation, but optimal results come from holistic system design that considers task timing, peripheral usage, and sleep mode characteristics.