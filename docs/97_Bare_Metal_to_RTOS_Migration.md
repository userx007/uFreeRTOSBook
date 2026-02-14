# Bare Metal to RTOS Migration

## Overview

Migrating from bare metal (super-loop) architectures to FreeRTOS is a common requirement as embedded systems grow in complexity. This transition enables better modularity, deterministic timing, and easier maintenance, but requires careful planning to avoid introducing timing issues or resource conflicts.

## Key Concepts

### Super-Loop Architecture
Traditional bare metal systems use a "super-loop" pattern:
```c
int main(void) {
    hardware_init();
    
    while(1) {
        poll_sensors();
        process_data();
        update_outputs();
        handle_communications();
        // Everything runs sequentially
    }
}
```

This approach has limitations:
- No inherent priority management
- Blocking operations delay everything
- Difficult to meet multiple timing requirements
- Poor scalability as features are added

### RTOS Benefits
- **Preemptive multitasking**: High-priority tasks run when needed
- **Deterministic timing**: Predictable response times
- **Modularity**: Each function becomes an independent task
- **Resource management**: Built-in synchronization primitives

## Migration Strategies

### 1. Phased Migration Approach

**Phase 1: Wrapper Task**
Start by wrapping the entire super-loop in a single RTOS task:

```c
// C Implementation
void legacy_task(void *params) {
    while(1) {
        poll_sensors();
        process_data();
        update_outputs();
        handle_communications();
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Add timing control
    }
}

int main(void) {
    hardware_init();
    
    xTaskCreate(legacy_task, "Legacy", 1024, NULL, 1, NULL);
    vTaskStartScheduler();
    
    // Should never reach here
    while(1);
}
```

```rust
// Rust Implementation (using freertos-rust)
use freertos_rust::*;

fn legacy_task(_: FreeRtosTaskHandle) {
    loop {
        poll_sensors();
        process_data();
        update_outputs();
        handle_communications();
        
        CurrentTask::delay(Duration::ms(10));
    }
}

fn main() {
    hardware_init();
    
    Task::new()
        .name("Legacy")
        .stack_size(1024)
        .priority(TaskPriority(1))
        .start(legacy_task)
        .unwrap();
    
    FreeRtosUtils::start_scheduler();
}
```

**Phase 2: Identify and Extract Tasks**

Analyze the super-loop to identify discrete functions:
- Different timing requirements
- Independent functionality
- Clear input/output boundaries
- Varying priority needs

```c
// C Implementation - Separated Tasks
void sensor_task(void *params) {
    TickType_t last_wake = xTaskGetTickCount();
    
    while(1) {
        poll_sensors();
        // Run every 50ms
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(50));
    }
}

void processing_task(void *params) {
    while(1) {
        // Wait for data from sensor task
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        process_data();
    }
}

void communication_task(void *params) {
    TickType_t last_wake = xTaskGetTickCount();
    
    while(1) {
        handle_communications();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}
```

```rust
// Rust Implementation - Separated Tasks
fn sensor_task(_: FreeRtosTaskHandle) {
    let mut last_wake = FreeRtosUtils::get_tick_count();
    
    loop {
        poll_sensors();
        CurrentTask::delay_until(&mut last_wake, Duration::ms(50));
    }
}

fn processing_task(_: FreeRtosTaskHandle) {
    loop {
        CurrentTask::wait_notification(u32::MAX, Duration::infinite());
        process_data();
    }
}

fn communication_task(_: FreeRtosTaskHandle) {
    let mut last_wake = FreeRtosUtils::get_tick_count();
    
    loop {
        handle_communications();
        CurrentTask::delay_until(&mut last_wake, Duration::ms(100));
    }
}
```

**Phase 3: Add Inter-Task Communication**

Replace global variables with RTOS mechanisms:

```c
// C Implementation
typedef struct {
    float temperature;
    float pressure;
    uint32_t timestamp;
} SensorData_t;

QueueHandle_t sensor_queue;
SemaphoreHandle_t data_mutex;

void sensor_task(void *params) {
    SensorData_t data;
    TickType_t last_wake = xTaskGetTickCount();
    
    while(1) {
        data.temperature = read_temperature();
        data.pressure = read_pressure();
        data.timestamp = xTaskGetTickCount();
        
        // Send to processing task
        xQueueSend(sensor_queue, &data, 0);
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(50));
    }
}

void processing_task(void *params) {
    SensorData_t data;
    
    while(1) {
        if(xQueueReceive(sensor_queue, &data, portMAX_DELAY) == pdTRUE) {
            // Protect shared resource
            xSemaphoreTake(data_mutex, portMAX_DELAY);
            process_sensor_data(&data);
            xSemaphoreGive(data_mutex);
        }
    }
}

int main(void) {
    hardware_init();
    
    // Create synchronization objects
    sensor_queue = xQueueCreate(10, sizeof(SensorData_t));
    data_mutex = xSemaphoreCreateMutex();
    
    xTaskCreate(sensor_task, "Sensor", 1024, NULL, 3, NULL);
    xTaskCreate(processing_task, "Process", 2048, NULL, 2, NULL);
    xTaskCreate(communication_task, "Comm", 1024, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    while(1);
}
```

```rust
// Rust Implementation
use std::sync::{Arc, Mutex};

struct SensorData {
    temperature: f32,
    pressure: f32,
    timestamp: u32,
}

fn sensor_task(queue: Arc<Queue<SensorData>>) -> impl FnOnce(FreeRtosTaskHandle) {
    move |_| {
        let mut last_wake = FreeRtosUtils::get_tick_count();
        
        loop {
            let data = SensorData {
                temperature: read_temperature(),
                pressure: read_pressure(),
                timestamp: FreeRtosUtils::get_tick_count(),
            };
            
            queue.send(data, Duration::ms(0)).ok();
            CurrentTask::delay_until(&mut last_wake, Duration::ms(50));
        }
    }
}

fn processing_task(
    queue: Arc<Queue<SensorData>>,
    mutex: Arc<Mutex<ProcessingState>>
) -> impl FnOnce(FreeRtosTaskHandle) {
    move |_| {
        loop {
            if let Ok(data) = queue.receive(Duration::infinite()) {
                let mut state = mutex.lock().unwrap();
                process_sensor_data(&data, &mut state);
            }
        }
    }
}

fn main() {
    hardware_init();
    
    let queue = Arc::new(Queue::new(10).unwrap());
    let mutex = Arc::new(Mutex::new(ProcessingState::new()));
    
    Task::new()
        .name("Sensor")
        .stack_size(1024)
        .priority(TaskPriority(3))
        .start(sensor_task(queue.clone()))
        .unwrap();
    
    Task::new()
        .name("Process")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(processing_task(queue, mutex))
        .unwrap();
    
    FreeRtosUtils::start_scheduler();
}
```

### 2. Identifying Tasks

**Guidelines for Task Decomposition:**

1. **Timing Requirements**: Different periods → separate tasks
2. **Priority**: Critical vs. background operations
3. **Independence**: Minimal coupling between functions
4. **Resource Usage**: Avoid conflicts (SPI, I2C, UART)

```c
// C Example - Task Identification
typedef enum {
    TASK_CRITICAL_CONTROL,   // 1ms - Highest priority
    TASK_SENSOR_READING,     // 50ms - High priority
    TASK_DATA_LOGGING,       // 1s - Medium priority
    TASK_LED_BLINK,          // 500ms - Low priority
    TASK_IDLE_PROCESSING     // Background - Lowest priority
} TaskType_t;

// Critical control loop
void critical_control_task(void *params) {
    TickType_t last_wake = xTaskGetTickCount();
    
    while(1) {
        read_control_inputs();
        update_pid_controller();
        apply_control_outputs();
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));
    }
}

// Priority assignment
xTaskCreate(critical_control_task, "Control", 512, NULL, 5, NULL);
xTaskCreate(sensor_task, "Sensors", 1024, NULL, 4, NULL);
xTaskCreate(logging_task, "Logger", 2048, NULL, 2, NULL);
xTaskCreate(led_task, "LED", 256, NULL, 1, NULL);
```

### 3. Managing Shared Resources

**Common Issues in Migration:**

```c
// C Implementation - WRONG: Race condition
uint32_t shared_counter = 0; // Global variable

void task1(void *params) {
    while(1) {
        shared_counter++; // NOT atomic!
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void task2(void *params) {
    while(1) {
        printf("Counter: %u\n", shared_counter); // Unsafe read
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Solution 1: Mutex Protection**

```c
// C Implementation - CORRECT
SemaphoreHandle_t counter_mutex;
uint32_t shared_counter = 0;

void task1(void *params) {
    while(1) {
        xSemaphoreTake(counter_mutex, portMAX_DELAY);
        shared_counter++;
        xSemaphoreGive(counter_mutex);
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void task2(void *params) {
    uint32_t local_copy;
    
    while(1) {
        xSemaphoreTake(counter_mutex, portMAX_DELAY);
        local_copy = shared_counter;
        xSemaphoreGive(counter_mutex);
        
        printf("Counter: %u\n", local_copy);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

```rust
// Rust Implementation - Type-safe mutex
use std::sync::{Arc, Mutex};

fn task1(counter: Arc<Mutex<u32>>) -> impl FnOnce(FreeRtosTaskHandle) {
    move |_| {
        loop {
            {
                let mut cnt = counter.lock().unwrap();
                *cnt += 1;
            } // Mutex automatically released
            
            CurrentTask::delay(Duration::ms(10));
        }
    }
}

fn task2(counter: Arc<Mutex<u32>>) -> impl FnOnce(FreeRtosTaskHandle) {
    move |_| {
        loop {
            let local_copy = {
                let cnt = counter.lock().unwrap();
                *cnt
            };
            
            println!("Counter: {}", local_copy);
            CurrentTask::delay(Duration::ms(100));
        }
    }
}
```

**Solution 2: Message Passing (Preferred)**

```c
// C Implementation - Queue-based
typedef enum {
    CMD_INCREMENT,
    CMD_GET_VALUE
} CounterCommand_t;

typedef struct {
    CounterCommand_t cmd;
    uint32_t *result; // For GET_VALUE
} CounterMsg_t;

QueueHandle_t counter_queue;

// Counter manager task - owns the data
void counter_manager_task(void *params) {
    uint32_t counter = 0;
    CounterMsg_t msg;
    
    while(1) {
        if(xQueueReceive(counter_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch(msg.cmd) {
                case CMD_INCREMENT:
                    counter++;
                    break;
                case CMD_GET_VALUE:
                    if(msg.result != NULL) {
                        *msg.result = counter;
                    }
                    break;
            }
        }
    }
}

void task1(void *params) {
    CounterMsg_t msg = { .cmd = CMD_INCREMENT };
    
    while(1) {
        xQueueSend(counter_queue, &msg, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

```rust
// Rust Implementation - Channel-based
enum CounterCommand {
    Increment,
    GetValue(tokio::sync::oneshot::Sender<u32>),
}

fn counter_manager_task(
    rx: Arc<Queue<CounterCommand>>
) -> impl FnOnce(FreeRtosTaskHandle) {
    move |_| {
        let mut counter: u32 = 0;
        
        loop {
            if let Ok(cmd) = rx.receive(Duration::infinite()) {
                match cmd {
                    CounterCommand::Increment => counter += 1,
                    CounterCommand::GetValue(tx) => {
                        tx.send(counter).ok();
                    }
                }
            }
        }
    }
}
```

### 4. Interrupt Handling Migration

```c
// C Implementation - Bare Metal ISR
volatile uint8_t uart_buffer[256];
volatile uint8_t uart_index = 0;

void UART_IRQHandler(void) {
    if(UART_STATUS & RX_READY) {
        uart_buffer[uart_index++] = UART_DATA;
    }
}

// RTOS Version - Use deferred processing
QueueHandle_t uart_queue;
TaskHandle_t uart_task_handle;

void UART_IRQHandler(void) {
    BaseType_t higher_priority_woken = pdFALSE;
    uint8_t data;
    
    if(UART_STATUS & RX_READY) {
        data = UART_DATA;
        xQueueSendFromISR(uart_queue, &data, &higher_priority_woken);
    }
    
    portYIELD_FROM_ISR(higher_priority_woken);
}

void uart_task(void *params) {
    uint8_t received_byte;
    
    while(1) {
        if(xQueueReceive(uart_queue, &received_byte, portMAX_DELAY) == pdTRUE) {
            // Process data in task context
            process_uart_data(received_byte);
        }
    }
}
```

## Complete Migration Example

### Before: Bare Metal Super-Loop

```c
// Bare metal implementation
#define LED_BLINK_INTERVAL_MS 500
#define SENSOR_READ_INTERVAL_MS 100

uint32_t led_timer = 0;
uint32_t sensor_timer = 0;
bool led_state = false;
float sensor_value = 0.0f;

int main(void) {
    hardware_init();
    
    while(1) {
        uint32_t now = get_tick_ms();
        
        // LED blinking
        if(now - led_timer >= LED_BLINK_INTERVAL_MS) {
            led_timer = now;
            led_state = !led_state;
            set_led(led_state);
        }
        
        // Sensor reading
        if(now - sensor_timer >= SENSOR_READ_INTERVAL_MS) {
            sensor_timer = now;
            sensor_value = read_sensor();
            
            if(sensor_value > THRESHOLD) {
                trigger_alarm();
            }
        }
        
        // Handle UART
        if(uart_data_available()) {
            uint8_t cmd = uart_read();
            process_command(cmd);
        }
    }
}
```

### After: FreeRTOS Implementation

```c
// FreeRTOS implementation
QueueHandle_t sensor_queue;
SemaphoreHandle_t alarm_semaphore;

void led_task(void *params) {
    bool state = false;
    TickType_t last_wake = xTaskGetTickCount();
    
    while(1) {
        state = !state;
        set_led(state);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
    }
}

void sensor_task(void *params) {
    TickType_t last_wake = xTaskGetTickCount();
    
    while(1) {
        float value = read_sensor();
        xQueueSend(sensor_queue, &value, 0);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
    }
}

void alarm_task(void *params) {
    float value;
    
    while(1) {
        if(xQueueReceive(sensor_queue, &value, portMAX_DELAY) == pdTRUE) {
            if(value > THRESHOLD) {
                trigger_alarm();
            }
        }
    }
}

void uart_task(void *params) {
    uint8_t cmd;
    
    while(1) {
        if(xQueueReceive(uart_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            process_command(cmd);
        }
    }
}

int main(void) {
    hardware_init();
    
    sensor_queue = xQueueCreate(5, sizeof(float));
    alarm_semaphore = xSemaphoreCreateBinary();
    
    xTaskCreate(led_task, "LED", 256, NULL, 1, NULL);
    xTaskCreate(sensor_task, "Sensor", 512, NULL, 3, NULL);
    xTaskCreate(alarm_task, "Alarm", 512, NULL, 4, NULL);
    xTaskCreate(uart_task, "UART", 1024, NULL, 2, NULL);
    
    vTaskStartScheduler();
    
    while(1);
}
```

## Summary

**Key Migration Principles:**
1. **Start Simple**: Wrap existing code in a single task first
2. **Incremental Decomposition**: Break into tasks one function at a time
3. **Eliminate Global State**: Replace with queues, mutexes, or message passing
4. **Priority Assignment**: Base on timing criticality and importance
5. **ISR Efficiency**: Keep ISRs short, defer processing to tasks
6. **Testing**: Validate each phase before proceeding

**Common Pitfalls:**
- Insufficient stack sizes causing mysterious crashes
- Priority inversion from improper mutex usage
- Forgetting to initialize synchronization objects
- Mixing blocking and non-blocking patterns inconsistently
- Underestimating memory overhead (stack per task + kernel)

**Benefits Realized:**
- Deterministic response times for critical functions
- Better CPU utilization through preemption
- Easier debugging with task-level isolation
- Scalability for adding new features
- Built-in timing services replace manual timer management

The migration from bare metal to RTOS is best approached methodically, with thorough testing at each stage to ensure system stability and timing correctness.