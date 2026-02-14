# FreeRTOS Simulation and Emulation

## Overview

Simulation and emulation are crucial techniques for developing and testing FreeRTOS applications without requiring physical hardware. This approach accelerates development cycles, enables early testing, facilitates debugging in a controlled environment, and allows developers to work on embedded systems projects even when hardware is unavailable or expensive.

## Key Concepts

### Simulation vs. Emulation

- **Simulation**: Models the behavior of the target system at a functional level. The Windows simulator port, for example, simulates FreeRTOS behavior using native Windows threads but doesn't simulate actual microcontroller peripherals.

- **Emulation**: Replicates the actual hardware architecture more closely. QEMU, for instance, emulates ARM Cortex-M processors, providing cycle-accurate or near-accurate behavior including peripheral registers.

### Benefits

1. **No Hardware Required**: Test code before hardware availability
2. **Faster Debugging**: Use familiar desktop debugging tools
3. **Reproducible Testing**: Consistent test environments
4. **Integration Testing**: Easier CI/CD integration
5. **Education**: Learn FreeRTOS concepts without hardware investment

## Windows Simulator Port

The Windows simulator port runs FreeRTOS using Windows threads to simulate RTOS tasks. This is the simplest way to get started with FreeRTOS development.

### C Example: Basic Windows Simulator Setup

```c
/* FreeRTOS Windows Simulator Example */
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Task priorities */
#define PRODUCER_PRIORITY (tskIDLE_PRIORITY + 2)
#define CONSUMER_PRIORITY (tskIDLE_PRIORITY + 1)

/* Queue handle */
static QueueHandle_t xDataQueue;

/* Producer task */
static void vProducerTask(void *pvParameters)
{
    uint32_t ulCounter = 0;
    const TickType_t xDelay = pdMS_TO_TICKS(1000);
    
    for (;;)
    {
        printf("Producer: Sending %lu\n", ulCounter);
        
        if (xQueueSend(xDataQueue, &ulCounter, portMAX_DELAY) != pdPASS)
        {
            printf("Producer: Failed to send data\n");
        }
        
        ulCounter++;
        vTaskDelay(xDelay);
    }
}

/* Consumer task */
static void vConsumerTask(void *pvParameters)
{
    uint32_t ulReceivedValue;
    
    for (;;)
    {
        if (xQueueReceive(xDataQueue, &ulReceivedValue, portMAX_DELAY) == pdPASS)
        {
            printf("Consumer: Received %lu\n", ulReceivedValue);
        }
    }
}

/* Main function for Windows simulator */
int main(void)
{
    printf("FreeRTOS Windows Simulator Demo\n");
    
    /* Create queue */
    xDataQueue = xQueueCreate(5, sizeof(uint32_t));
    
    if (xDataQueue == NULL)
    {
        printf("Failed to create queue\n");
        return -1;
    }
    
    /* Create tasks */
    xTaskCreate(vProducerTask, "Producer", 1000, NULL, PRODUCER_PRIORITY, NULL);
    xTaskCreate(vConsumerTask, "Consumer", 1000, NULL, CONSUMER_PRIORITY, NULL);
    
    /* Start scheduler */
    printf("Starting scheduler...\n");
    vTaskStartScheduler();
    
    /* Should never reach here */
    printf("Scheduler ended unexpectedly\n");
    return 0;
}
```

### FreeRTOSConfig.h for Windows Simulator

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Windows-specific settings */
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configTICK_RATE_HZ                      1000
#define configMINIMAL_STACK_SIZE                ((unsigned short)256)
#define configTOTAL_HEAP_SIZE                   ((size_t)(65 * 1024))
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_TRACE_FACILITY                1
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configQUEUE_REGISTRY_SIZE               8
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1

/* Task priorities */
#define configMAX_PRIORITIES                    7

/* Software timer definitions */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                5
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* Set the following definitions to 1 to include the API function */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xTimerPendFunctionCall          1

/* Windows simulator specific */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

#endif /* FREERTOS_CONFIG_H */
```

## QEMU Emulation

QEMU provides more realistic hardware emulation, supporting various ARM Cortex-M boards and peripherals.

### C Example: QEMU ARM Cortex-M3 Application

```c
/* FreeRTOS on QEMU ARM Cortex-M3 (STM32-like) */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Simulated UART base address */
#define UART0_BASE    0x40004000
#define UART_DR       (*(volatile uint32_t *)(UART0_BASE + 0x00))
#define UART_FR       (*(volatile uint32_t *)(UART0_BASE + 0x18))

/* Simple UART output function */
static void uart_putc(char c)
{
    /* Wait for UART to be ready */
    while (UART_FR & (1 << 5));
    UART_DR = c;
}

static void uart_puts(const char *str)
{
    while (*str)
    {
        if (*str == '\n')
            uart_putc('\r');
        uart_putc(*str++);
    }
}

/* Blinky task simulation */
static void vBlinkTask(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(500);
    uint32_t ulCounter = 0;
    char buffer[32];
    
    for (;;)
    {
        snprintf(buffer, sizeof(buffer), "Blink: %lu\n", ulCounter++);
        uart_puts(buffer);
        
        vTaskDelay(xDelay);
    }
}

/* Monitoring task */
static void vMonitorTask(void *pvParameters)
{
    const TickType_t xDelay = pdMS_TO_TICKS(2000);
    TaskStatus_t xTaskDetails;
    char buffer[64];
    
    for (;;)
    {
        /* Get task statistics */
        vTaskGetInfo(NULL, &xTaskDetails, pdTRUE, eInvalid);
        
        snprintf(buffer, sizeof(buffer), 
                 "Monitor: Free heap = %u bytes\n", 
                 xPortGetFreeHeapSize());
        uart_puts(buffer);
        
        vTaskDelay(xDelay);
    }
}

/* Main application entry point */
int main(void)
{
    uart_puts("FreeRTOS QEMU Demo Starting...\n");
    
    /* Create tasks */
    xTaskCreate(vBlinkTask, "Blink", 128, NULL, 2, NULL);
    xTaskCreate(vMonitorTask, "Monitor", 128, NULL, 1, NULL);
    
    /* Start scheduler */
    uart_puts("Starting scheduler\n");
    vTaskStartScheduler();
    
    /* Should never reach here */
    for (;;);
    return 0;
}
```

### QEMU Command Line Example

```bash
# Run ARM Cortex-M3 with 128KB RAM
qemu-system-arm \
    -M lm3s6965evb \
    -kernel application.elf \
    -nographic \
    -serial stdio \
    -monitor none
```

## Rust Implementation

Rust support for FreeRTOS requires bindings and careful handling of unsafe code.

### Rust Example: FreeRTOS Task Creation

```rust
// FreeRTOS Rust bindings wrapper
#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::ptr;

// External FreeRTOS C functions
extern "C" {
    fn xTaskCreate(
        pvTaskCode: extern "C" fn(*mut core::ffi::c_void),
        pcName: *const u8,
        usStackDepth: u16,
        pvParameters: *mut core::ffi::c_void,
        uxPriority: u32,
        pxCreatedTask: *mut *mut core::ffi::c_void,
    ) -> i32;
    
    fn vTaskStartScheduler();
    fn vTaskDelay(xTicksToDelay: u32);
    fn xQueueCreate(uxQueueLength: u32, uxItemSize: u32) -> *mut core::ffi::c_void;
    fn xQueueSend(
        xQueue: *mut core::ffi::c_void,
        pvItemToQueue: *const core::ffi::c_void,
        xTicksToWait: u32,
    ) -> i32;
    fn xQueueReceive(
        xQueue: *mut core::ffi::c_void,
        pvBuffer: *mut core::ffi::c_void,
        xTicksToWait: u32,
    ) -> i32;
}

const PORT_MAX_DELAY: u32 = 0xFFFFFFFF;

// Queue handle (static for simplicity)
static mut DATA_QUEUE: *mut core::ffi::c_void = ptr::null_mut();

// Producer task
extern "C" fn producer_task(_arg: *mut core::ffi::c_void) {
    let mut counter: u32 = 0;
    
    loop {
        unsafe {
            // Simulate sending data
            xQueueSend(DATA_QUEUE, &counter as *const u32 as *const core::ffi::c_void, PORT_MAX_DELAY);
            counter += 1;
            
            // Delay 1000 ticks (1 second if tick is 1ms)
            vTaskDelay(1000);
        }
    }
}

// Consumer task
extern "C" fn consumer_task(_arg: *mut core::ffi::c_void) {
    let mut received_value: u32 = 0;
    
    loop {
        unsafe {
            if xQueueReceive(
                DATA_QUEUE,
                &mut received_value as *mut u32 as *mut core::ffi::c_void,
                PORT_MAX_DELAY
            ) == 1 {
                // Process received value
                // In real application, you might use UART or LED
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn main() -> ! {
    unsafe {
        // Create queue with 5 elements of u32 size
        DATA_QUEUE = xQueueCreate(5, core::mem::size_of::<u32>() as u32);
        
        if DATA_QUEUE.is_null() {
            panic!("Failed to create queue");
        }
        
        // Create producer task
        let task_name_producer = b"Producer\0";
        xTaskCreate(
            producer_task,
            task_name_producer.as_ptr(),
            256,
            ptr::null_mut(),
            2,
            ptr::null_mut(),
        );
        
        // Create consumer task
        let task_name_consumer = b"Consumer\0";
        xTaskCreate(
            consumer_task,
            task_name_consumer.as_ptr(),
            256,
            ptr::null_mut(),
            1,
            ptr::null_mut(),
        );
        
        // Start the scheduler
        vTaskStartScheduler();
    }
    
    // Should never reach here
    loop {}
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
```

### Rust with Modern FreeRTOS Wrapper (Using freertos-rust crate)

```rust
// Using freertos-rust crate for safer abstractions
use freertos_rust::*;
use core::time::Duration;

// Producer task
fn producer_task(queue: Arc<Queue<u32>>) {
    let mut counter: u32 = 0;
    
    loop {
        // Send data to queue
        queue.send(counter, Duration::MAX).unwrap();
        counter += 1;
        
        // Delay for 1 second
        CurrentTask::delay(Duration::from_millis(1000));
    }
}

// Consumer task
fn consumer_task(queue: Arc<Queue<u32>>) {
    loop {
        // Wait for data from queue
        if let Ok(value) = queue.receive(Duration::MAX) {
            // Process received value
            println!("Received: {}", value);
        }
    }
}

#[no_mangle]
pub fn main() {
    // Create a queue
    let queue = Arc::new(Queue::new(5).unwrap());
    
    // Clone queue handles for tasks
    let producer_queue = queue.clone();
    let consumer_queue = queue.clone();
    
    // Create producer task
    Task::new()
        .name("Producer")
        .stack_size(256)
        .priority(TaskPriority(2))
        .start(move || producer_task(producer_queue))
        .unwrap();
    
    // Create consumer task
    Task::new()
        .name("Consumer")
        .stack_size(256)
        .priority(TaskPriority(1))
        .start(move || consumer_task(consumer_queue))
        .unwrap();
    
    // Start the scheduler
    FreeRtosUtils::start_scheduler();
}
```

## Advanced Testing Patterns

### C++: Mock Hardware Layer

```cpp
// Hardware abstraction for testing
class IHardwareInterface {
public:
    virtual void writeGPIO(uint8_t pin, bool value) = 0;
    virtual bool readGPIO(uint8_t pin) = 0;
    virtual void delay(uint32_t ms) = 0;
    virtual ~IHardwareInterface() = default;
};

// Simulated implementation
class SimulatedHardware : public IHardwareInterface {
private:
    std::map<uint8_t, bool> gpio_state;
    
public:
    void writeGPIO(uint8_t pin, bool value) override {
        gpio_state[pin] = value;
        std::cout << "GPIO " << (int)pin << " set to " << value << std::endl;
    }
    
    bool readGPIO(uint8_t pin) override {
        return gpio_state[pin];
    }
    
    void delay(uint32_t ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
};

// Application task using abstraction
extern "C" void vApplicationTask(void *pvParameters) {
    IHardwareInterface *hw = static_cast<IHardwareInterface*>(pvParameters);
    
    for (;;) {
        hw->writeGPIO(LED_PIN, true);
        hw->delay(500);
        hw->writeGPIO(LED_PIN, false);
        hw->delay(500);
    }
}

// Main with dependency injection
int main() {
    SimulatedHardware hw;
    
    xTaskCreate(vApplicationTask, "App", 1000, &hw, 2, NULL);
    vTaskStartScheduler();
    
    return 0;
}
```

## Summary

**Simulation and emulation are essential tools for FreeRTOS development**, providing:

- **Windows Simulator**: Quick prototyping using native OS threads, ideal for learning task synchronization and algorithm testing
- **QEMU Emulation**: Hardware-accurate testing with ARM Cortex-M emulation, peripheral simulation, and realistic timing
- **Cross-language Support**: Works with C, C++, and Rust, enabling modern development practices
- **Testing Benefits**: Reproducible environments, CI/CD integration, debugging with familiar tools, and no hardware dependencies

**Key takeaways**: Start with the Windows simulator for initial development, transition to QEMU for hardware-specific testing, use hardware abstractions for portability, and leverage simulation for automated testing pipelines. This approach significantly reduces time-to-market and improves code quality before deploying to actual embedded hardware.