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
        "OneShot",                 // Timer name
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
Two buttons – single debounce timer (safe design)

```c
#define DEBOUNCE_TIME_MS   50

typedef enum
{
    BUTTON_1 = 0,
    BUTTON_2 = 1,
} Button_t;

#define BUTTON_MASK(b) (1U << (b))

static TimerHandle_t xDebounceTimer;
static volatile uint32_t ulPendingButtons = 0;

/* Debounce callback (Timer Service task context) */
static void vDebounceCallback(TimerHandle_t xTimer)
{
    uint32_t pending = ulPendingButtons;

    /* Clear pending flags */
    ulPendingButtons = 0;

    if (pending & BUTTON_MASK(BUTTON_1))
    {
        if (HAL_GPIO_ReadPin(BUTTON1_GPIO_PORT, BUTTON1_PIN) == GPIO_PIN_SET)
        {
            printf("Button 1 pressed (debounced)\n");
        }
    }

    if (pending & BUTTON_MASK(BUTTON_2))
    {
        if (HAL_GPIO_ReadPin(BUTTON2_GPIO_PORT, BUTTON2_PIN) == GPIO_PIN_SET)
        {
            printf("Button 2 pressed (debounced)\n");
        }
    }
}


/* EXTI ISR callback */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (GPIO_Pin == BUTTON1_PIN)
    {
        ulPendingButtons |= BUTTON_MASK(BUTTON_1);
    }
    else if (GPIO_Pin == BUTTON2_PIN)
    {
        ulPendingButtons |= BUTTON_MASK(BUTTON_2);
    }

    /* Restart shared debounce timer */
    xTimerResetFromISR(xDebounceTimer, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/* Init */
void Buttons_Init(void)
{
    xDebounceTimer = xTimerCreate(
        "BTN_DB",
        pdMS_TO_TICKS(DEBOUNCE_TIME_MS),
        pdFALSE,
        NULL,
        vDebounceCallback
    );

    configASSERT(xDebounceTimer);
}
```

## Example: STM32 + FreeRTOS Two Button Debounce using Software Timers

```c
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include <stdio.h>

/* Button GPIO definitions (example)  */
#define BUTTON1_GPIO_PORT   GPIOA
#define BUTTON1_PIN         GPIO_PIN_0

#define BUTTON2_GPIO_PORT   GPIOA
#define BUTTON2_PIN         GPIO_PIN_1

/* Button IDs  */
typedef enum
{
    BUTTON_1 = 0,
    BUTTON_2 = 1,
    BUTTON_COUNT
} Button_t;

/* Globals  */
static TimerHandle_t xDebounceTimers[BUTTON_COUNT];


/* Debounce Timer Callback (runs in Timer Service task) */
static void vDebounceCallback(TimerHandle_t xTimer)
{
    Button_t button = (Button_t) pvTimerGetTimerID(xTimer);
    GPIO_PinState state;

    switch (button)
    {
        case BUTTON_1:
            state = HAL_GPIO_ReadPin(BUTTON1_GPIO_PORT, BUTTON1_PIN);
            break;

        case BUTTON_2:
            state = HAL_GPIO_ReadPin(BUTTON2_GPIO_PORT, BUTTON2_PIN);
            break;

        default:
            return;
    }

    if (state == GPIO_PIN_SET)   // button still pressed after debounce
    {
        printf("Button %d pressed (debounced)\n", button);
        /* Handle button press here (notify task, set event, etc.) */
    }
}

/* Button / Timer Init (call before scheduler start) */
static void Buttons_Init(void)
{
    xDebounceTimers[BUTTON_1] = xTimerCreate(
        "BTN1_DB",
        pdMS_TO_TICKS(50),
        pdFALSE,                 // one-shot
        (void *)BUTTON_1,
        vDebounceCallback
    );

    xDebounceTimers[BUTTON_2] = xTimerCreate(
        "BTN2_DB",
        pdMS_TO_TICKS(50),
        pdFALSE,
        (void *)BUTTON_2,
        vDebounceCallback
    );

    configASSERT(xDebounceTimers[BUTTON_1]);
    configASSERT(xDebounceTimers[BUTTON_2]);
}


/* HAL EXTI Callback (called from ISR) */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (GPIO_Pin == BUTTON1_PIN)
    {
        xTimerResetFromISR(
            xDebounceTimers[BUTTON_1],
            &xHigherPriorityTaskWoken
        );
    }
    else if (GPIO_Pin == BUTTON2_PIN)
    {
        xTimerResetFromISR(
            xDebounceTimers[BUTTON_2],
            &xHigherPriorityTaskWoken
        );
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/* EXTI IRQ Handlers (CubeMX usually generates these) */
void EXTI0_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BUTTON1_PIN);
}

void EXTI1_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(BUTTON2_PIN);
}


/* Example Idle Task (optional) */
static void vDummyTask(void *argument)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* MAIN */
int main(void)
{
    HAL_Init();
    SystemClock_Config();    // generated by CubeMX

    /* GPIO init must configure:
     * - BUTTON pins as input
     * - EXTI interrupt enabled
     * - Pull-up or pull-down as needed
     */
    MX_GPIO_Init();          // generated by CubeMX

    Buttons_Init();

    xTaskCreate(
        vDummyTask,
        "Dummy",
        256,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    vTaskStartScheduler();

    /* Should never reach here */
    while (1)
    {
    }
}
```

## Example: STM32 + FreeRTOS Two Buttons  
(With Debounce + Short / Long Press, one debounce timer + one long-press timer per button)

```c
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include <stdio.h>

/* Timing  */
#define DEBOUNCE_MS      50
#define LONG_PRESS_MS   1000

/* GPIO  */
#define BUTTON1_GPIO_PORT   GPIOA
#define BUTTON1_PIN         GPIO_PIN_0

#define BUTTON2_GPIO_PORT   GPIOA
#define BUTTON2_PIN         GPIO_PIN_1

/* Button IDs  */
typedef enum
{
    BUTTON_1 = 0,
    BUTTON_2 = 1,
    BUTTON_COUNT
} Button_t;

/* Button state  */
typedef enum
{
    BTN_IDLE = 0,
    BTN_DEBOUNCED,
    BTN_LONG_SENT
} ButtonState_t;

/* Button control block  */
typedef struct
{
    TimerHandle_t debounceTimer;
    TimerHandle_t longTimer;
    GPIO_TypeDef *port;
    uint16_t      pin;
    ButtonState_t state;
} ButtonCtrl_t;

/* Globals  */
static ButtonCtrl_t Buttons[BUTTON_COUNT];

/* Long-press timer callback */
static void vLongPressCallback(TimerHandle_t xTimer)
{
    Button_t btn = (Button_t) pvTimerGetTimerID(xTimer);
    ButtonCtrl_t *b = &Buttons[btn];

    if (HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_SET)
    {
        b->state = BTN_LONG_SENT;
        printf("Button %d LONG press\n", btn);
    }
}


/* Debounce timer callback */
static void vDebounceCallback(TimerHandle_t xTimer)
{
    Button_t btn = (Button_t) pvTimerGetTimerID(xTimer);
    ButtonCtrl_t *b = &Buttons[btn];

    if (HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_SET)
    {
        /* Button is stable and pressed */
        b->state = BTN_DEBOUNCED;

        /* Start long-press detection */
        xTimerStart(b->longTimer, 0);
    }
}


/* HAL EXTI callback (ISR context) */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        ButtonCtrl_t *b = &Buttons[i];

        if (GPIO_Pin == b->pin)
        {
            if (HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_SET)
            {
                /* Button pressed → debounce */
                xTimerResetFromISR(b->debounceTimer, &xHigherPriorityTaskWoken);
            }
            else
            {
                /* Button released */
                xTimerStopFromISR(b->longTimer, &xHigherPriorityTaskWoken);

                if (b->state == BTN_DEBOUNCED)
                {
                    printf("Button %d SHORT press\n", i);
                }

                b->state = BTN_IDLE;
            }
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/* Button init */
static void Buttons_Init(void)
{
    Buttons[BUTTON_1] = (ButtonCtrl_t){
        .port = BUTTON1_GPIO_PORT,
        .pin  = BUTTON1_PIN,
        .state = BTN_IDLE
    };

    Buttons[BUTTON_2] = (ButtonCtrl_t){
        .port = BUTTON2_GPIO_PORT,
        .pin  = BUTTON2_PIN,
        .state = BTN_IDLE
    };

    for (int i = 0; i < BUTTON_COUNT; i++)
    {
        Buttons[i].debounceTimer = xTimerCreate(
            "DB",
            pdMS_TO_TICKS(DEBOUNCE_MS),
            pdFALSE,
            (void *)i,
            vDebounceCallback
        );

        Buttons[i].longTimer = xTimerCreate(
            "LP",
            pdMS_TO_TICKS(LONG_PRESS_MS),
            pdFALSE,
            (void *)i,
            vLongPressCallback
        );

        configASSERT(Buttons[i].debounceTimer);
        configASSERT(Buttons[i].longTimer);
    }
}

/* Dummy task */
static void vIdleTask(void *arg)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();   /* Configure EXTI for both buttons */

    Buttons_Init();

    xTaskCreate(vIdleTask, "Idle", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1) {}
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