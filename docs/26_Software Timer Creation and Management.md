# Software Timer Creation and Management in FreeRTOS

## Overview

Software timers in FreeRTOS provide a mechanism to execute callback functions at specified time intervals without blocking tasks. Unlike hardware timers, software timers are managed by the FreeRTOS kernel and don't require dedicated hardware resources. They're ideal for periodic operations, timeout mechanisms, and delayed function execution.

## Key Concepts

### Timer Service Task (Daemon Task)

FreeRTOS manages all software timers through a single daemon task called the Timer Service Task. This task:

- Runs at a priority defined by `configTIMER_TASK_PRIORITY` in FreeRTOSConfig.h
- Processes timer commands from a timer command queue
- Executes timer callback functions when timers expire
- Has its own stack size defined by `configTIMER_TASK_STACK_DEPTH`

**Important**: Timer callbacks execute in the context of the timer service task, so they should be short and must not block.

### Timer Types

**One-Shot Timers**: Execute their callback function once and then stop automatically.

**Auto-Reload Timers**: Execute their callback function repeatedly at regular intervals until explicitly stopped.

### Timer States

- **Dormant**: Timer exists but is not running
- **Running**: Timer is active and counting toward expiration

## Configuration Requirements

In FreeRTOSConfig.h:

```c
#define configUSE_TIMERS                1
#define configTIMER_TASK_PRIORITY       3  // Should be high enough
#define configTIMER_TASK_STACK_DEPTH    256
#define configTIMER_QUEUE_LENGTH        10
```

## Creating Software Timers

### Static Creation

```c
#include "FreeRTOS.h"
#include "timers.h"

// Timer handle
TimerHandle_t xOneShotTimer;
TimerHandle_t xAutoReloadTimer;

// Static storage for timer
StaticTimer_t xTimerBuffer;

void vOneShotCallback(TimerHandle_t xTimer)
{
    // This executes once after the timer period
    printf("One-shot timer expired!\n");
    
    // Can restart the timer from within callback if needed
    // xTimerStart(xTimer, 0);
}

void vAutoReloadCallback(TimerHandle_t xTimer)
{
    // This executes repeatedly every timer period
    static uint32_t ulCount = 0;
    ulCount++;
    printf("Auto-reload timer tick: %lu\n", ulCount);
}

void setup_timers(void)
{
    // Create one-shot timer (3000ms period)
    xOneShotTimer = xTimerCreate(
        "OneShot",                  // Timer name
        pdMS_TO_TICKS(3000),       // Period in ticks (3 seconds)
        pdFALSE,                   // Auto-reload: pdFALSE = one-shot
        (void *)0,                 // Timer ID
        vOneShotCallback           // Callback function
    );
    
    // Create auto-reload timer (1000ms period)
    xAutoReloadTimer = xTimerCreate(
        "AutoReload",              // Timer name
        pdMS_TO_TICKS(1000),       // Period in ticks (1 second)
        pdTRUE,                    // Auto-reload: pdTRUE = repeating
        (void *)1,                 // Timer ID
        vAutoReloadCallback        // Callback function
    );
    
    // Start the timers
    if (xOneShotTimer != NULL)
    {
        xTimerStart(xOneShotTimer, 0);
    }
    
    if (xAutoReloadTimer != NULL)
    {
        xTimerStart(xAutoReloadTimer, 0);
    }
}
```

## Timer Management Functions

### Starting and Stopping

```c
// Start a timer
BaseType_t xTimerStart(TimerHandle_t xTimer, TickType_t xBlockTime);

// Stop a timer
BaseType_t xTimerStop(TimerHandle_t xTimer, TickType_t xBlockTime);

// Reset a timer (restart from beginning)
BaseType_t xTimerReset(TimerHandle_t xTimer, TickType_t xBlockTime);

// Example usage
if (xTimerStart(xMyTimer, portMAX_DELAY) != pdPASS)
{
    // Timer command could not be sent to the timer command queue
}
```

### Changing Timer Period

```c
BaseType_t xTimerChangePeriod(TimerHandle_t xTimer, 
                               TickType_t xNewPeriod, 
                               TickType_t xBlockTime);

// Example: Change timer period to 2 seconds
xTimerChangePeriod(xMyTimer, pdMS_TO_TICKS(2000), 100);
```

### ISR-Safe Versions

For calling from interrupts:

```c
BaseType_t xTimerStartFromISR(TimerHandle_t xTimer, 
                               BaseType_t *pxHigherPriorityTaskWoken);

BaseType_t xTimerStopFromISR(TimerHandle_t xTimer, 
                              BaseType_t *pxHigherPriorityTaskWoken);

BaseType_t xTimerResetFromISR(TimerHandle_t xTimer, 
                               BaseType_t *pxHigherPriorityTaskWoken);
```

## Practical Example: LED Blinker with Timeout

```c
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

// Timer handles
TimerHandle_t xBlinkTimer;
TimerHandle_t xTimeoutTimer;

// LED state
static volatile uint8_t ucLedState = 0;

void vBlinkCallback(TimerHandle_t xTimer)
{
    // Toggle LED every 500ms
    ucLedState = !ucLedState;
    GPIO_WritePin(LED_GPIO_Port, LED_Pin, ucLedState);
}

void vTimeoutCallback(TimerHandle_t xTimer)
{
    // Stop blinking after 10 seconds
    printf("Timeout reached - stopping LED blink\n");
    xTimerStop(xBlinkTimer, 0);
    GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0); // Turn off LED
}

void vApplicationSetup(void)
{
    // Create auto-reload timer for LED blinking (500ms)
    xBlinkTimer = xTimerCreate(
        "Blink",
        pdMS_TO_TICKS(500),
        pdTRUE,              // Auto-reload
        (void *)0,
        vBlinkCallback
    );
    
    // Create one-shot timer for timeout (10 seconds)
    xTimeoutTimer = xTimerCreate(
        "Timeout",
        pdMS_TO_TICKS(10000),
        pdFALSE,             // One-shot
        (void *)1,
        vTimeoutCallback
    );
    
    // Start both timers
    xTimerStart(xBlinkTimer, 0);
    xTimerStart(xTimeoutTimer, 0);
}
```

## Example: Debouncing with Timer ID

```c
#define BUTTON_1    0
#define BUTTON_2    1

TimerHandle_t xDebounceTimer;

void vDebounceCallback(TimerHandle_t xTimer)
{
    // Get which button triggered the timer
    uint32_t ulButtonID = (uint32_t)pvTimerGetTimerID(xTimer);
    
    // Read the stable button state
    if (GPIO_ReadPin(ulButtonID == BUTTON_1 ? BUTTON1_PORT : BUTTON2_PORT,
                     ulButtonID == BUTTON_1 ? BUTTON1_PIN : BUTTON2_PIN))
    {
        printf("Button %lu pressed (debounced)\n", ulButtonID);
        // Handle button press
    }
}

void BUTTON_IRQ_Handler(uint32_t buttonID)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Set timer ID to identify which button
    vTimerSetTimerID(xDebounceTimer, (void *)buttonID);
    
    // Start/reset 50ms debounce timer
    xTimerResetFromISR(xDebounceTimer, &xHigherPriorityTaskWoken);
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void setup_debounce(void)
{
    xDebounceTimer = xTimerCreate(
        "Debounce",
        pdMS_TO_TICKS(50),
        pdFALSE,           // One-shot
        (void *)0,
        vDebounceCallback
    );
}
```

## Example: Watchdog-Style Monitoring

```c
#define TASK_TIMEOUT_MS    5000

TimerHandle_t xWatchdogTimer;
TaskHandle_t xMonitoredTask;

void vWatchdogCallback(TimerHandle_t xTimer)
{
    // Task hasn't reset the timer - it might be stuck
    printf("ERROR: Task timeout detected!\n");
    
    // Take corrective action
    vTaskDelete(xMonitoredTask);
    
    // Recreate task or restart system
    xTaskCreate(vMonitoredTaskFunction, "Monitored", 128, NULL, 2, &xMonitoredTask);
    xTimerStart(xWatchdogTimer, 0);
}

void vMonitoredTaskFunction(void *pvParameters)
{
    while (1)
    {
        // Do work
        process_data();
        
        // Reset watchdog timer to indicate we're alive
        xTimerReset(xWatchdogTimer, 0);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup_watchdog(void)
{
    xWatchdogTimer = xTimerCreate(
        "Watchdog",
        pdMS_TO_TICKS(TASK_TIMEOUT_MS),
        pdFALSE,           // One-shot
        (void *)0,
        vWatchdogCallback
    );
}
```

## Best Practices and Considerations

### Timer Callback Guidelines

1. **Keep callbacks short**: They execute in the timer service task context and block other timer operations
2. **Never block**: Don't call functions that might block (vTaskDelay, queue receives with timeout, etc.)
3. **Use ISR-safe functions**: If interacting with queues/semaphores, use FromISR versions with zero block time
4. **Consider deferring work**: Post to a queue for a worker task to handle complex operations

### Timer Service Task Priority

The timer task priority should be set appropriately:
- Too low: Timer callbacks might not execute on time
- Too high: May starve other tasks
- Typical: Set higher than most application tasks but lower than critical real-time tasks

### Memory Considerations

```c
// Dynamic allocation (default)
TimerHandle_t xTimer = xTimerCreate(...);

// Static allocation (no heap required)
StaticTimer_t xTimerBuffer;
TimerHandle_t xTimer = xTimerCreateStatic(
    "Timer",
    pdMS_TO_TICKS(1000),
    pdTRUE,
    (void *)0,
    vCallback,
    &xTimerBuffer  // Static storage
);
```

### Deleting Timers

```c
BaseType_t xTimerDelete(TimerHandle_t xTimer, TickType_t xBlockTime);

// Stop and delete a timer
xTimerStop(xMyTimer, portMAX_DELAY);
xTimerDelete(xMyTimer, portMAX_DELAY);
xMyTimer = NULL;
```

## Common Pitfalls

1. **Forgetting to enable timers**: Must set `configUSE_TIMERS` to 1
2. **Inadequate timer queue length**: If many timers or frequent commands, increase `configTIMER_QUEUE_LENGTH`
3. **Blocking in callbacks**: Causes timer service task to stall
4. **Wrong timer type**: Using one-shot when auto-reload is needed (or vice versa)
5. **Not checking return values**: Timer commands can fail if the queue is full

Software timers provide an elegant, resource-efficient way to handle time-based operations in FreeRTOS without creating dedicated tasks for each timed event.