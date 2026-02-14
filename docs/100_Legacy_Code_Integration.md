# Legacy Code Integration in FreeRTOS

## Overview

Legacy code integration is the process of incorporating existing, often older codebases into a FreeRTOS-based real-time system. This is common when migrating from bare-metal or other RTOS environments, or when reusing proven libraries that weren't designed for multitasking. The main challenge is that legacy code is frequently **non-reentrant** (not thread-safe), meaning it cannot be safely called from multiple tasks simultaneously.

## Core Challenges

### 1. **Non-Reentrancy**
Legacy code often uses:
- Global/static variables without protection
- Shared hardware resources without synchronization
- Blocking delays or busy-wait loops
- Direct hardware register access

### 2. **Timing Assumptions**
Legacy code may assume:
- Exclusive CPU access
- Specific execution timing
- Immediate hardware response

### 3. **Resource Management**
- No awareness of RTOS task priorities
- Unbounded execution times
- Direct interrupt handling

## Integration Strategies

### Strategy 1: Wrapper Tasks (Serialization)
Create a dedicated task that owns the legacy code, with other tasks communicating through queues or semaphores.

### Strategy 2: Critical Sections
Protect legacy code execution with RTOS primitives (mutexes, critical sections).

### Strategy 3: Code Isolation
Run legacy code in a separate execution context with controlled interfaces.

---

## C/C++ Implementation Examples

### Example 1: Queue-Based Wrapper Task

```c
// Legacy non-reentrant sensor library
typedef struct {
    uint8_t data[128];
    uint16_t index;
} LegacySensorState_t;

static LegacySensorState_t g_sensor_state; // Global state - NOT thread-safe

// Legacy function - uses global state, not reentrant
int legacy_read_sensor(uint8_t sensor_id, float* value) {
    // Accesses global g_sensor_state
    // Performs blocking I2C read
    // Returns sensor value
    g_sensor_state.index++;
    *value = (float)(g_sensor_state.index % 100);
    vTaskDelay(pdMS_TO_TICKS(10)); // Simulated blocking operation
    return 0;
}

// Request/response structures
typedef struct {
    uint8_t sensor_id;
    SemaphoreHandle_t response_sem;
    float* result;
    int* status;
} SensorRequest_t;

// Global queue for sensor requests
static QueueHandle_t sensor_request_queue;

// Wrapper task - serializes all access to legacy code
void legacy_sensor_wrapper_task(void* pvParameters) {
    SensorRequest_t request;
    
    while (1) {
        // Wait for requests from other tasks
        if (xQueueReceive(sensor_request_queue, &request, portMAX_DELAY)) {
            // Execute legacy code in isolation
            *request.status = legacy_read_sensor(request.sensor_id, request.result);
            
            // Signal completion
            xSemaphoreGive(request.response_sem);
        }
    }
}

// Safe API for other tasks to call
int safe_read_sensor(uint8_t sensor_id, float* value, TickType_t timeout) {
    SensorRequest_t request;
    int status;
    
    request.sensor_id = sensor_id;
    request.result = value;
    request.status = &status;
    request.response_sem = xSemaphoreCreateBinary();
    
    if (request.response_sem == NULL) {
        return -1;
    }
    
    // Send request to wrapper task
    if (xQueueSend(sensor_request_queue, &request, timeout) != pdPASS) {
        vSemaphoreDelete(request.response_sem);
        return -2;
    }
    
    // Wait for response
    if (xSemaphoreTake(request.response_sem, timeout) != pdPASS) {
        vSemaphoreDelete(request.response_sem);
        return -3;
    }
    
    vSemaphoreDelete(request.response_sem);
    return status;
}

// Application task using the safe API
void application_task(void* pvParameters) {
    float temperature;
    
    while (1) {
        if (safe_read_sensor(1, &temperature, pdMS_TO_TICKS(100)) == 0) {
            printf("Temperature: %.2f\n", temperature);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Initialization
void setup_legacy_integration(void) {
    sensor_request_queue = xQueueCreate(10, sizeof(SensorRequest_t));
    
    xTaskCreate(legacy_sensor_wrapper_task, "SensorWrapper", 
                2048, NULL, 3, NULL);
    xTaskCreate(application_task, "App", 
                2048, NULL, 2, NULL);
}
```

### Example 2: Mutex-Protected Legacy Code

```c
// Legacy display library with global state
typedef struct {
    uint8_t framebuffer[320*240];
    uint16_t cursor_x;
    uint16_t cursor_y;
} LegacyDisplay_t;

static LegacyDisplay_t g_display;

void legacy_draw_pixel(uint16_t x, uint16_t y, uint8_t color) {
    g_display.framebuffer[y * 320 + x] = color;
}

void legacy_draw_text(const char* text) {
    // Uses and modifies cursor position
    while (*text) {
        legacy_draw_pixel(g_display.cursor_x++, g_display.cursor_y, *text);
        text++;
    }
}

// RTOS-safe wrapper with mutex
static SemaphoreHandle_t display_mutex;

void safe_display_init(void) {
    memset(&g_display, 0, sizeof(g_display));
    display_mutex = xSemaphoreCreateMutex();
}

int safe_draw_text(const char* text, TickType_t timeout) {
    if (xSemaphoreTake(display_mutex, timeout) != pdPASS) {
        return -1;
    }
    
    legacy_draw_text(text);
    
    xSemaphoreGive(display_mutex);
    return 0;
}

// Complex operation using multiple legacy calls
int safe_draw_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h, 
                   uint8_t color, TickType_t timeout) {
    if (xSemaphoreTake(display_mutex, timeout) != pdPASS) {
        return -1;
    }
    
    // Multiple legacy calls protected by single mutex
    for (uint16_t i = x; i < x + w; i++) {
        legacy_draw_pixel(i, y, color);
        legacy_draw_pixel(i, y + h - 1, color);
    }
    for (uint16_t i = y; i < y + h; i++) {
        legacy_draw_pixel(x, i, color);
        legacy_draw_pixel(x + w - 1, i, color);
    }
    
    xSemaphoreGive(display_mutex);
    return 0;
}
```

### Example 3: Adapter Pattern with State Isolation

```c
// Legacy UART driver (assumes single-threaded access)
static uint8_t uart_tx_buffer[256];
static volatile uint8_t uart_tx_head = 0;
static volatile uint8_t uart_tx_tail = 0;

void legacy_uart_send_byte(uint8_t byte) {
    uart_tx_buffer[uart_tx_head++] = byte;
    // Trigger hardware
}

// Adapter with isolated state per task
typedef struct {
    uint8_t task_id;
    QueueHandle_t tx_queue;
    TaskHandle_t task_handle;
} UartAdapter_t;

#define MAX_UART_TASKS 4
static UartAdapter_t uart_adapters[MAX_UART_TASKS];
static uint8_t adapter_count = 0;
static SemaphoreHandle_t uart_hw_mutex;

// Central UART task that manages hardware
void uart_manager_task(void* pvParameters) {
    uint8_t byte;
    
    while (1) {
        // Round-robin through all registered adapters
        for (int i = 0; i < adapter_count; i++) {
            if (xQueueReceive(uart_adapters[i].tx_queue, &byte, 0) == pdPASS) {
                xSemaphoreTake(uart_hw_mutex, portMAX_DELAY);
                legacy_uart_send_byte(byte);
                xSemaphoreGive(uart_hw_mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Register a task for UART access
UartAdapter_t* uart_register_task(void) {
    if (adapter_count >= MAX_UART_TASKS) {
        return NULL;
    }
    
    UartAdapter_t* adapter = &uart_adapters[adapter_count++];
    adapter->task_id = adapter_count - 1;
    adapter->tx_queue = xQueueCreate(32, sizeof(uint8_t));
    adapter->task_handle = xTaskGetCurrentTaskHandle();
    
    return adapter;
}

// Safe send function for tasks
int uart_send_string(UartAdapter_t* adapter, const char* str, TickType_t timeout) {
    if (adapter == NULL) return -1;
    
    while (*str) {
        if (xQueueSend(adapter->tx_queue, str, timeout) != pdPASS) {
            return -2;
        }
        str++;
    }
    return 0;
}
```

---

## Rust Implementation Examples

### Example 1: Queue-Based Wrapper (Rust)

```rust
use freertos_rust::{Task, Queue, Duration, Semaphore};
use core::sync::atomic::{AtomicU16, Ordering};

// Legacy sensor state (simulated)
static mut SENSOR_STATE: SensorState = SensorState::new();

struct SensorState {
    data: [u8; 128],
    index: u16,
}

impl SensorState {
    const fn new() -> Self {
        SensorState {
            data: [0; 128],
            index: 0,
        }
    }
}

// Legacy non-reentrant function (unsafe, uses global mutable state)
unsafe fn legacy_read_sensor(sensor_id: u8) -> Result<f32, i32> {
    // Access global mutable state - NOT thread-safe
    SENSOR_STATE.index = SENSOR_STATE.index.wrapping_add(1);
    
    // Simulate blocking operation
    Task::delay(Duration::ms(10));
    
    Ok((SENSOR_STATE.index % 100) as f32)
}

// Request/Response structures
struct SensorRequest {
    sensor_id: u8,
    response_tx: freertos_rust::Sender<Result<f32, i32>>,
}

// Safe wrapper using message passing
struct SensorWrapper {
    request_queue: Queue<SensorRequest>,
}

impl SensorWrapper {
    fn new() -> Self {
        SensorWrapper {
            request_queue: Queue::new(10).unwrap(),
        }
    }
    
    // Spawn the wrapper task
    fn spawn(self) {
        Task::new()
            .name("SensorWrapper")
            .stack_size(2048)
            .priority(freertos_rust::TaskPriority(3))
            .start(move || {
                self.run();
            })
            .unwrap();
    }
    
    // Main wrapper task loop
    fn run(&self) {
        loop {
            if let Some(request) = self.request_queue.receive(Duration::infinite()) {
                // Execute legacy code in isolation
                let result = unsafe { legacy_read_sensor(request.sensor_id) };
                
                // Send response back
                let _ = request.response_tx.send(result);
            }
        }
    }
    
    // Safe API for other tasks
    fn read_sensor(&self, sensor_id: u8, timeout: Duration) -> Result<f32, i32> {
        let (tx, rx) = freertos_rust::channel().unwrap();
        
        let request = SensorRequest {
            sensor_id,
            response_tx: tx,
        };
        
        // Send request
        self.request_queue.send(request, timeout)
            .map_err(|_| -1)?;
        
        // Wait for response
        rx.receive(timeout).ok_or(-2)?
    }
}

// Application task
fn application_task(wrapper: &'static SensorWrapper) {
    loop {
        match wrapper.read_sensor(1, Duration::ms(100)) {
            Ok(temp) => println!("Temperature: {:.2}", temp),
            Err(e) => println!("Error: {}", e),
        }
        
        Task::delay(Duration::ms(1000));
    }
}
```

### Example 2: Mutex-Protected Legacy Code (Rust)

```rust
use freertos_rust::{Mutex, Duration};

// Legacy display structure
struct LegacyDisplay {
    framebuffer: [u8; 320 * 240],
    cursor_x: u16,
    cursor_y: u16,
}

impl LegacyDisplay {
    fn new() -> Self {
        LegacyDisplay {
            framebuffer: [0; 320 * 240],
            cursor_x: 0,
            cursor_y: 0,
        }
    }
    
    // Legacy methods (not thread-safe)
    fn draw_pixel(&mut self, x: u16, y: u16, color: u8) {
        let idx = (y as usize) * 320 + (x as usize);
        if idx < self.framebuffer.len() {
            self.framebuffer[idx] = color;
        }
    }
    
    fn draw_text(&mut self, text: &str) {
        for ch in text.chars() {
            self.draw_pixel(self.cursor_x, self.cursor_y, ch as u8);
            self.cursor_x += 1;
        }
    }
}

// Thread-safe wrapper
struct SafeDisplay {
    display: Mutex<LegacyDisplay>,
}

impl SafeDisplay {
    fn new() -> Self {
        SafeDisplay {
            display: Mutex::new(LegacyDisplay::new()).unwrap(),
        }
    }
    
    fn draw_text(&self, text: &str, timeout: Duration) -> Result<(), ()> {
        let mut display = self.display.lock(timeout).ok_or(())?;
        display.draw_text(text);
        Ok(())
    }
    
    fn draw_box(&self, x: u16, y: u16, w: u16, h: u16, 
                color: u8, timeout: Duration) -> Result<(), ()> {
        let mut display = self.display.lock(timeout).ok_or(())?;
        
        // Multiple operations under single lock
        for i in x..(x + w) {
            display.draw_pixel(i, y, color);
            display.draw_pixel(i, y + h - 1, color);
        }
        for i in y..(y + h) {
            display.draw_pixel(x, i, color);
            display.draw_pixel(x + w - 1, i, color);
        }
        
        Ok(())
    }
}
```

### Example 3: Type-Safe Adapter Pattern (Rust)

```rust
use freertos_rust::{Queue, Task, Duration, Mutex};

// Legacy UART driver (simulated)
struct LegacyUart {
    tx_buffer: [u8; 256],
    tx_head: usize,
    tx_tail: usize,
}

impl LegacyUart {
    fn new() -> Self {
        LegacyUart {
            tx_buffer: [0; 256],
            tx_head: 0,
            tx_tail: 0,
        }
    }
    
    fn send_byte(&mut self, byte: u8) {
        self.tx_buffer[self.tx_head] = byte;
        self.tx_head = (self.tx_head + 1) % 256;
    }
}

// Per-task adapter
struct UartAdapter {
    tx_queue: Queue<u8>,
}

impl UartAdapter {
    fn new() -> Self {
        UartAdapter {
            tx_queue: Queue::new(32).unwrap(),
        }
    }
    
    fn send_str(&self, s: &str, timeout: Duration) -> Result<(), ()> {
        for byte in s.bytes() {
            self.tx_queue.send(byte, timeout).map_err(|_| ())?;
        }
        Ok(())
    }
}

// Central UART manager
struct UartManager {
    hardware: Mutex<LegacyUart>,
    adapters: Vec<UartAdapter>,
}

impl UartManager {
    fn new() -> Self {
        UartManager {
            hardware: Mutex::new(LegacyUart::new()).unwrap(),
            adapters: Vec::new(),
        }
    }
    
    fn register_adapter(&mut self) -> &UartAdapter {
        self.adapters.push(UartAdapter::new());
        self.adapters.last().unwrap()
    }
    
    fn spawn(self) {
        Task::new()
            .name("UartManager")
            .stack_size(2048)
            .priority(freertos_rust::TaskPriority(4))
            .start(move || {
                self.run();
            })
            .unwrap();
    }
    
    fn run(&self) {
        loop {
            // Service all adapters
            for adapter in &self.adapters {
                if let Some(byte) = adapter.tx_queue.receive(Duration::ms(0)) {
                    if let Ok(mut hw) = self.hardware.lock(Duration::ms(10)) {
                        hw.send_byte(byte);
                    }
                }
            }
            Task::delay(Duration::ms(1));
        }
    }
}
```

---

## Best Practices

### 1. **Identify Legacy Code Characteristics**
- Audit for global/static variables
- Check for hardware dependencies
- Analyze timing requirements
- Document blocking operations

### 2. **Choose the Right Strategy**
- **Wrapper Tasks**: Best for complex legacy libraries with multiple entry points
- **Mutexes**: Good for simple libraries with short execution times
- **Adapters**: Ideal when multiple tasks need independent access

### 3. **Define Clear Interfaces**
- Create clean API boundaries
- Use timeout parameters on all blocking operations
- Return error codes for failure cases
- Document thread-safety guarantees

### 4. **Handle Timing Constraints**
- Ensure wrapper task has appropriate priority
- Account for queueing delays
- Monitor worst-case response times
- Use direct-to-task notifications for urgent requests

### 5. **Testing**
- Stress test with concurrent access from multiple tasks
- Verify deterministic behavior under load
- Check for priority inversion issues
- Validate timeout handling

---

## Summary

**Legacy code integration** in FreeRTOS requires careful wrapping of non-reentrant code to make it safe for multitasking environments. The three main approaches are:

1. **Wrapper Tasks with Queues**: Serialize access by funneling all requests through a dedicated task that owns the legacy code
2. **Mutex Protection**: Guard legacy code with mutexes for simpler cases with short critical sections
3. **Adapter Pattern**: Create per-task adapters with a central manager for complex scenarios requiring isolated state

In **C/C++**, implementation typically uses FreeRTOS queues, semaphores, and mutexes directly. In **Rust**, the type system provides additional safety through ownership rules and type-safe wrappers around FreeRTOS primitives.

Key considerations include maintaining timing guarantees, preventing priority inversion, proper error handling with timeouts, and thorough testing under concurrent load. The goal is to preserve the functionality of proven legacy code while ensuring thread-safe operation in a multitasking RTOS environment.