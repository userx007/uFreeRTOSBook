# Reactor and Proactor Patterns in FreeRTOS

## Quick Overview

**Reactor Pattern** (Synchronous Event-Driven):
- Waits for events from multiple sources
- Dispatches events to registered handlers synchronously
- Simple, low-latency, perfect for interrupt-driven embedded systems
- Uses FreeRTOS queues for event demultiplexing

**Proactor Pattern** (Asynchronous I/O):
- Initiates asynchronous operations that complete in the background
- Application continues working while I/O happens
- Handlers process completion events
- Ideal for DMA-capable peripherals and high-throughput scenarios

## What's Included

✅ **Detailed explanations** of both patterns with flow diagrams  
✅ **Full C implementations** with practical examples (UART, buttons, ADC, timers)  
✅ **Modern C++ version** using templates and std::function  
✅ **Rust implementations** with trait-based handlers and async/await  
✅ **Performance comparison** and when to use each pattern  
✅ **Real-world usage examples** from embedded systems

The guide includes complete, production-ready code that you can adapt for your FreeRTOS projects!

# Reactor and Proactor Patterns in FreeRTOS

## Table of Contents
1. [Introduction](#introduction)
2. [Reactor Pattern](#reactor-pattern)
3. [Proactor Pattern](#proactor-pattern)
4. [Implementation in C/C++](#implementation-in-cc)
5. [Implementation in Rust](#implementation-in-rust)
6. [Performance Considerations](#performance-considerations)
7. [Summary](#summary)

---

## Introduction

The **Reactor** and **Proactor** patterns are event-driven architectural patterns designed to handle multiple concurrent I/O operations efficiently. In embedded systems running FreeRTOS, these patterns enable responsive applications that can manage multiple input sources, sensors, communication interfaces, and asynchronous events without blocking.

### Key Differences

| Aspect | Reactor Pattern | Proactor Pattern |
|--------|----------------|------------------|
| **I/O Model** | Synchronous, non-blocking | Asynchronous |
| **Control Flow** | Application reads/writes data | OS/HAL performs I/O |
| **Notification** | Ready to read/write | Operation completed |
| **Complexity** | Simpler | More complex |
| **Efficiency** | Good for many sources | Better for high-throughput |

---

## Reactor Pattern

### Concept

The Reactor pattern handles service requests delivered concurrently by one or more inputs. It demultiplexes incoming requests and dispatches them synchronously to associated request handlers.

**Components:**
1. **Resources (Handles)**: I/O sources (UART, SPI, ADC, timers)
2. **Synchronous Event Demultiplexer**: Waits for events (FreeRTOS queues/event groups)
3. **Dispatcher**: Routes events to handlers
4. **Event Handlers**: Process specific events

### Flow:
```
1. Register event sources with reactor
2. Reactor waits for events (blocking or polling)
3. When event occurs, reactor identifies source
4. Dispatcher calls appropriate handler
5. Handler processes event synchronously
6. Return to step 2
```

---

## Proactor Pattern

### Concept

The Proactor pattern initiates asynchronous operations and handles their completion events. The application initiates operations, and the system notifies when they complete.

**Components:**
1. **Asynchronous Operation Processor**: OS/HAL layer performing actual I/O
2. **Completion Dispatcher**: Notifies when operations complete
3. **Proactor**: Manages async operations
4. **Completion Handlers**: Process completed operations
5. **Initiator**: Starts async operations

### Flow:
```
1. Application initiates async operation
2. Operation processor performs I/O in background
3. Application continues other work
4. When complete, completion event generated
5. Dispatcher calls completion handler
6. Handler processes result
```

---

## Implementation in C/C++

### Reactor Pattern in C

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"

/* Event types */
typedef enum {
    EVENT_UART_RX,
    EVENT_BUTTON_PRESS,
    EVENT_TIMER_EXPIRED,
    EVENT_ADC_READY,
    EVENT_NONE
} EventType_t;

/* Event structure */
typedef struct {
    EventType_t type;
    void *data;
    uint32_t dataLen;
} Event_t;

/* Event handler function pointer */
typedef void (*EventHandler_t)(Event_t *event);

/* Reactor structure */
typedef struct {
    QueueHandle_t eventQueue;
    EventHandler_t handlers[16];  // Max 16 event types
    uint32_t handlerCount;
    TaskHandle_t reactorTask;
} Reactor_t;

/* Global reactor instance */
static Reactor_t reactor;

/* Initialize reactor */
void Reactor_Init(void) {
    reactor.eventQueue = xQueueCreate(32, sizeof(Event_t));
    reactor.handlerCount = 0;
    
    configASSERT(reactor.eventQueue != NULL);
    
    /* Create reactor task */
    xTaskCreate(Reactor_Task, "Reactor", 512, NULL, 
                configMAX_PRIORITIES - 1, &reactor.reactorTask);
}

/* Register event handler */
void Reactor_RegisterHandler(EventType_t type, EventHandler_t handler) {
    if (type < 16) {
        reactor.handlers[type] = handler;
    }
}

/* Post event to reactor */
BaseType_t Reactor_PostEvent(EventType_t type, void *data, uint32_t dataLen) {
    Event_t event = {
        .type = type,
        .data = data,
        .dataLen = dataLen
    };
    
    return xQueueSend(reactor.eventQueue, &event, 0);
}

/* Post event from ISR */
BaseType_t Reactor_PostEventFromISR(EventType_t type, void *data, 
                                    uint32_t dataLen, BaseType_t *pxHigherPriorityTaskWoken) {
    Event_t event = {
        .type = type,
        .data = data,
        .dataLen = dataLen
    };
    
    return xQueueSendFromISR(reactor.eventQueue, &event, pxHigherPriorityTaskWoken);
}

/* Reactor main task */
static void Reactor_Task(void *pvParameters) {
    Event_t event;
    
    for (;;) {
        /* Wait for events (blocking) - this is the demultiplexer */
        if (xQueueReceive(reactor.eventQueue, &event, portMAX_DELAY) == pdTRUE) {
            /* Dispatch to appropriate handler */
            if (event.type < 16 && reactor.handlers[event.type] != NULL) {
                reactor.handlers[event.type](&event);
            }
            
            /* Free event data if needed */
            if (event.data != NULL) {
                vPortFree(event.data);
            }
        }
    }
}

/* Example event handlers */
static void HandleUartRx(Event_t *event) {
    uint8_t *data = (uint8_t *)event->data;
    printf("UART RX: %.*s\n", event->dataLen, data);
    
    /* Process received data */
    // ... protocol parsing, state machine updates, etc.
}

static void HandleButtonPress(Event_t *event) {
    uint32_t buttonId = *(uint32_t *)event->data;
    printf("Button %u pressed\n", buttonId);
    
    /* Handle button action */
    // ... menu navigation, mode changes, etc.
}

static void HandleTimerExpired(Event_t *event) {
    uint32_t timerId = *(uint32_t *)event->data;
    printf("Timer %u expired\n", timerId);
    
    /* Handle timeout */
    // ... periodic tasks, watchdog refresh, etc.
}

static void HandleAdcReady(Event_t *event) {
    uint16_t adcValue = *(uint16_t *)event->data;
    printf("ADC reading: %u\n", adcValue);
    
    /* Process sensor data */
    // ... filtering, threshold checks, etc.
}

/* Application setup */
void App_Setup(void) {
    /* Initialize reactor */
    Reactor_Init();
    
    /* Register handlers */
    Reactor_RegisterHandler(EVENT_UART_RX, HandleUartRx);
    Reactor_RegisterHandler(EVENT_BUTTON_PRESS, HandleButtonPress);
    Reactor_RegisterHandler(EVENT_TIMER_EXPIRED, HandleTimerExpired);
    Reactor_RegisterHandler(EVENT_ADC_READY, HandleAdcReady);
    
    /* Initialize peripherals and start generating events */
    // UART_Init(), Button_Init(), Timer_Init(), ADC_Init()
}

/* Example: UART interrupt posts event to reactor */
void UART_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (UART_GetRxFlag()) {
        uint8_t *rxData = pvPortMalloc(UART_RX_SIZE);
        uint32_t len = UART_Read(rxData, UART_RX_SIZE);
        
        Reactor_PostEventFromISR(EVENT_UART_RX, rxData, len, 
                                 &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### Proactor Pattern in C

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Asynchronous operation types */
typedef enum {
    ASYNC_OP_READ,
    ASYNC_OP_WRITE,
    ASYNC_OP_TRANSFER
} AsyncOpType_t;

/* Completion status */
typedef enum {
    COMPLETION_SUCCESS,
    COMPLETION_ERROR,
    COMPLETION_TIMEOUT
} CompletionStatus_t;

/* Completion handler function pointer */
typedef void (*CompletionHandler_t)(void *context, CompletionStatus_t status, 
                                    void *data, uint32_t dataLen);

/* Asynchronous operation structure */
typedef struct {
    AsyncOpType_t type;
    void *resource;           // UART, SPI, etc.
    void *buffer;
    uint32_t bufferLen;
    CompletionHandler_t handler;
    void *context;
    TickType_t timeout;
} AsyncOperation_t;

/* Completion event */
typedef struct {
    AsyncOperation_t *operation;
    CompletionStatus_t status;
    uint32_t bytesTransferred;
} CompletionEvent_t;

/* Proactor structure */
typedef struct {
    QueueHandle_t completionQueue;
    QueueHandle_t operationQueue;
    TaskHandle_t processorTask;
    TaskHandle_t dispatcherTask;
    SemaphoreHandle_t opMutex;
} Proactor_t;

static Proactor_t proactor;

/* Initialize proactor */
void Proactor_Init(void) {
    proactor.completionQueue = xQueueCreate(16, sizeof(CompletionEvent_t));
    proactor.operationQueue = xQueueCreate(16, sizeof(AsyncOperation_t *));
    proactor.opMutex = xSemaphoreCreateMutex();
    
    configASSERT(proactor.completionQueue != NULL);
    configASSERT(proactor.operationQueue != NULL);
    configASSERT(proactor.opMutex != NULL);
    
    /* Create processor and dispatcher tasks */
    xTaskCreate(Proactor_ProcessorTask, "AsyncProc", 512, NULL, 
                tskIDLE_PRIORITY + 2, &proactor.processorTask);
    xTaskCreate(Proactor_DispatcherTask, "AsyncDisp", 512, NULL, 
                tskIDLE_PRIORITY + 3, &proactor.dispatcherTask);
}

/* Initiate asynchronous operation */
BaseType_t Proactor_AsyncRead(void *resource, void *buffer, uint32_t len,
                              CompletionHandler_t handler, void *context,
                              TickType_t timeout) {
    AsyncOperation_t *op = pvPortMalloc(sizeof(AsyncOperation_t));
    
    if (op == NULL) {
        return pdFAIL;
    }
    
    op->type = ASYNC_OP_READ;
    op->resource = resource;
    op->buffer = buffer;
    op->bufferLen = len;
    op->handler = handler;
    op->context = context;
    op->timeout = timeout;
    
    return xQueueSend(proactor.operationQueue, &op, 0);
}

/* Initiate asynchronous write */
BaseType_t Proactor_AsyncWrite(void *resource, const void *buffer, uint32_t len,
                               CompletionHandler_t handler, void *context,
                               TickType_t timeout) {
    AsyncOperation_t *op = pvPortMalloc(sizeof(AsyncOperation_t));
    
    if (op == NULL) {
        return pdFAIL;
    }
    
    op->type = ASYNC_OP_WRITE;
    op->resource = resource;
    op->buffer = (void *)buffer;
    op->bufferLen = len;
    op->handler = handler;
    op->context = context;
    op->timeout = timeout;
    
    return xQueueSend(proactor.operationQueue, &op, 0);
}

/* Asynchronous operation processor task */
static void Proactor_ProcessorTask(void *pvParameters) {
    AsyncOperation_t *op;
    CompletionEvent_t completion;
    
    for (;;) {
        /* Wait for async operations */
        if (xQueueReceive(proactor.operationQueue, &op, portMAX_DELAY) == pdTRUE) {
            completion.operation = op;
            completion.bytesTransferred = 0;
            
            /* Perform the actual I/O operation */
            switch (op->type) {
                case ASYNC_OP_READ: {
                    /* Simulate async read with DMA or interrupt-driven I/O */
                    int result = HAL_PerformAsyncRead(op->resource, op->buffer, 
                                                     op->bufferLen, op->timeout);
                    
                    if (result >= 0) {
                        completion.status = COMPLETION_SUCCESS;
                        completion.bytesTransferred = result;
                    } else if (result == -1) {
                        completion.status = COMPLETION_TIMEOUT;
                    } else {
                        completion.status = COMPLETION_ERROR;
                    }
                    break;
                }
                
                case ASYNC_OP_WRITE: {
                    /* Simulate async write */
                    int result = HAL_PerformAsyncWrite(op->resource, op->buffer, 
                                                      op->bufferLen, op->timeout);
                    
                    if (result >= 0) {
                        completion.status = COMPLETION_SUCCESS;
                        completion.bytesTransferred = result;
                    } else if (result == -1) {
                        completion.status = COMPLETION_TIMEOUT;
                    } else {
                        completion.status = COMPLETION_ERROR;
                    }
                    break;
                }
                
                default:
                    completion.status = COMPLETION_ERROR;
                    break;
            }
            
            /* Post completion event */
            xQueueSend(proactor.completionQueue, &completion, portMAX_DELAY);
        }
    }
}

/* Completion dispatcher task */
static void Proactor_DispatcherTask(void *pvParameters) {
    CompletionEvent_t completion;
    
    for (;;) {
        /* Wait for completion events */
        if (xQueueReceive(proactor.completionQueue, &completion, 
                         portMAX_DELAY) == pdTRUE) {
            AsyncOperation_t *op = completion.operation;
            
            /* Call completion handler */
            if (op->handler != NULL) {
                op->handler(op->context, completion.status, 
                           op->buffer, completion.bytesTransferred);
            }
            
            /* Free operation structure */
            vPortFree(op);
        }
    }
}

/* Example completion handlers */
static void OnReadComplete(void *context, CompletionStatus_t status, 
                          void *data, uint32_t dataLen) {
    if (status == COMPLETION_SUCCESS) {
        printf("Read completed: %u bytes\n", dataLen);
        /* Process received data */
        ProcessIncomingData(data, dataLen);
    } else if (status == COMPLETION_TIMEOUT) {
        printf("Read timeout\n");
    } else {
        printf("Read error\n");
    }
}

static void OnWriteComplete(void *context, CompletionStatus_t status, 
                           void *data, uint32_t dataLen) {
    if (status == COMPLETION_SUCCESS) {
        printf("Write completed: %u bytes\n", dataLen);
        /* Free buffer if dynamically allocated */
        vPortFree(data);
    } else {
        printf("Write failed\n");
        vPortFree(data);
    }
}

/* Example usage */
void Example_ProactorUsage(void) {
    static uint8_t rxBuffer[256];
    uint8_t *txBuffer;
    
    /* Initiate async read */
    Proactor_AsyncRead(UART1_Handle, rxBuffer, sizeof(rxBuffer),
                      OnReadComplete, NULL, pdMS_TO_TICKS(1000));
    
    /* Prepare and initiate async write */
    txBuffer = pvPortMalloc(128);
    memcpy(txBuffer, "Hello World", 11);
    Proactor_AsyncWrite(UART1_Handle, txBuffer, 11,
                       OnWriteComplete, NULL, pdMS_TO_TICKS(500));
    
    /* Application can continue doing other work while I/O happens */
}
```

### Advanced C++ Reactor with Templates

```cpp
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <functional>
#include <map>
#include <memory>

namespace RTOS {

// Event base class
class Event {
public:
    virtual ~Event() = default;
    virtual uint32_t getType() const = 0;
};

// Templated event
template<typename T>
class TypedEvent : public Event {
private:
    T data_;
    uint32_t type_;
    
public:
    TypedEvent(uint32_t type, const T& data) : type_(type), data_(data) {}
    
    uint32_t getType() const override { return type_; }
    const T& getData() const { return data_; }
};

// Event handler interface
class EventHandler {
public:
    virtual ~EventHandler() = default;
    virtual void handleEvent(const Event& event) = 0;
};

// Reactor class
class Reactor {
private:
    QueueHandle_t eventQueue_;
    std::map<uint32_t, std::function<void(const Event&)>> handlers_;
    TaskHandle_t taskHandle_;
    bool running_;
    
    static void taskFunction(void* param) {
        Reactor* reactor = static_cast<Reactor*>(param);
        reactor->eventLoop();
    }
    
    void eventLoop() {
        Event* event;
        
        while (running_) {
            if (xQueueReceive(eventQueue_, &event, portMAX_DELAY) == pdTRUE) {
                dispatch(event);
                delete event;
            }
        }
    }
    
    void dispatch(const Event* event) {
        auto it = handlers_.find(event->getType());
        if (it != handlers_.end()) {
            it->second(*event);
        }
    }
    
public:
    Reactor(uint32_t queueSize = 32) : running_(false) {
        eventQueue_ = xQueueCreate(queueSize, sizeof(Event*));
        configASSERT(eventQueue_ != nullptr);
    }
    
    ~Reactor() {
        stop();
        if (eventQueue_) {
            vQueueDelete(eventQueue_);
        }
    }
    
    void start(UBaseType_t priority = tskIDLE_PRIORITY + 1) {
        running_ = true;
        xTaskCreate(taskFunction, "Reactor", 1024, this, priority, &taskHandle_);
    }
    
    void stop() {
        running_ = false;
        if (taskHandle_) {
            vTaskDelete(taskHandle_);
            taskHandle_ = nullptr;
        }
    }
    
    template<typename T>
    void registerHandler(uint32_t type, std::function<void(const T&)> handler) {
        handlers_[type] = [handler](const Event& event) {
            const TypedEvent<T>& typedEvent = static_cast<const TypedEvent<T>&>(event);
            handler(typedEvent.getData());
        };
    }
    
    template<typename T>
    bool postEvent(uint32_t type, const T& data, TickType_t timeout = 0) {
        Event* event = new TypedEvent<T>(type, data);
        if (xQueueSend(eventQueue_, &event, timeout) != pdTRUE) {
            delete event;
            return false;
        }
        return true;
    }
};

// Example usage with modern C++
struct SensorData {
    float temperature;
    float humidity;
    uint32_t timestamp;
};

struct ButtonEvent {
    uint8_t buttonId;
    bool pressed;
};

void setupReactor() {
    static Reactor reactor;
    
    // Register lambda handlers
    reactor.registerHandler<SensorData>(1, [](const SensorData& data) {
        printf("Sensor: Temp=%.2f, Humidity=%.2f\n", 
               data.temperature, data.humidity);
    });
    
    reactor.registerHandler<ButtonEvent>(2, [](const ButtonEvent& event) {
        printf("Button %d %s\n", event.buttonId, 
               event.pressed ? "pressed" : "released");
    });
    
    reactor.start();
    
    // Post events from anywhere
    SensorData sensorData = {25.5f, 60.0f, xTaskGetTickCount()};
    reactor.postEvent(1, sensorData);
    
    ButtonEvent buttonEvent = {1, true};
    reactor.postEvent(2, buttonEvent);
}

} // namespace RTOS
```

---

## Implementation in Rust

### Reactor Pattern in Rust

```rust
#![no_std]
#![no_main]

use freertos_rust::*;
use core::cell::RefCell;
use alloc::vec::Vec;
use alloc::boxed::Box;

// Event types
#[derive(Debug, Clone, Copy)]
pub enum EventType {
    UartRx,
    ButtonPress,
    TimerExpired,
    AdcReady,
}

// Event structure with dynamic data
pub struct Event {
    event_type: EventType,
    data: Option<Box<[u8]>>,
}

impl Event {
    pub fn new(event_type: EventType, data: Option<Box<[u8]>>) -> Self {
        Self { event_type, data }
    }
}

// Event handler trait
pub trait EventHandler: Send {
    fn handle(&mut self, event: &Event);
}

// Reactor structure
pub struct Reactor {
    event_queue: Queue<Event>,
    handlers: RefCell<Vec<Option<Box<dyn EventHandler>>>>,
}

impl Reactor {
    pub fn new(queue_size: usize) -> Self {
        Self {
            event_queue: Queue::new(queue_size).unwrap(),
            handlers: RefCell::new(Vec::with_capacity(16)),
        }
    }
    
    pub fn register_handler<H: EventHandler + 'static>(
        &self,
        event_type: EventType,
        handler: H,
    ) {
        let mut handlers = self.handlers.borrow_mut();
        let index = event_type as usize;
        
        // Resize if needed
        if index >= handlers.len() {
            handlers.resize_with(index + 1, || None);
        }
        
        handlers[index] = Some(Box::new(handler));
    }
    
    pub fn post_event(&self, event: Event) -> Result<(), FreeRtosError> {
        self.event_queue.send(event, Duration::zero())
    }
    
    pub fn post_event_from_isr(&self, event: Event) -> Result<bool, FreeRtosError> {
        self.event_queue.send_from_isr(event)
    }
    
    pub fn run(&self) -> ! {
        loop {
            if let Ok(event) = self.event_queue.receive(Duration::infinite()) {
                self.dispatch(&event);
            }
        }
    }
    
    fn dispatch(&self, event: &Event) {
        let mut handlers = self.handlers.borrow_mut();
        let index = event.event_type as usize;
        
        if index < handlers.len() {
            if let Some(ref mut handler) = handlers[index] {
                handler.handle(event);
            }
        }
    }
    
    pub fn spawn(self, priority: u8) -> Task {
        Task::new()
            .name("Reactor")
            .stack_size(2048)
            .priority(TaskPriority(priority))
            .start(move || {
                self.run();
            })
            .unwrap()
    }
}

// Example event handlers
struct UartRxHandler;

impl EventHandler for UartRxHandler {
    fn handle(&mut self, event: &Event) {
        if let Some(ref data) = event.data {
            // Process UART data
            println!("UART RX: {} bytes", data.len());
            // Parse protocol, update state machine, etc.
        }
    }
}

struct ButtonHandler {
    button_state: [bool; 4],
}

impl ButtonHandler {
    fn new() -> Self {
        Self {
            button_state: [false; 4],
        }
    }
}

impl EventHandler for ButtonHandler {
    fn handle(&mut self, event: &Event) {
        if let Some(ref data) = event.data {
            if !data.is_empty() {
                let button_id = data[0] as usize;
                if button_id < 4 {
                    self.button_state[button_id] = !self.button_state[button_id];
                    println!("Button {} state: {}", button_id, self.button_state[button_id]);
                }
            }
        }
    }
}

struct AdcHandler;

impl EventHandler for AdcHandler {
    fn handle(&mut self, event: &Event) {
        if let Some(ref data) = event.data {
            if data.len() >= 2 {
                let value = u16::from_le_bytes([data[0], data[1]]);
                println!("ADC value: {}", value);
                
                // Process sensor data: filtering, thresholds, etc.
                if value > 3000 {
                    // Trigger alarm
                }
            }
        }
    }
}

// Application setup
pub fn setup_reactor() {
    let reactor = Reactor::new(32);
    
    // Register handlers
    reactor.register_handler(EventType::UartRx, UartRxHandler);
    reactor.register_handler(EventType::ButtonPress, ButtonHandler::new());
    reactor.register_handler(EventType::AdcReady, AdcHandler);
    
    // Spawn reactor task
    reactor.spawn(3);
    
    // Note: In a real application, you would store the reactor
    // in a static or pass it to other tasks
}

// Example: Posting events from ISR
#[no_mangle]
pub extern "C" fn uart_irq_handler(reactor: &'static Reactor) {
    // Read UART data
    let mut buffer = Box::new([0u8; 64]);
    let len = unsafe { uart_read(buffer.as_mut_ptr(), 64) };
    
    // Resize to actual length
    let data = buffer[..len as usize].into();
    
    let event = Event::new(EventType::UartRx, Some(data));
    let _ = reactor.post_event_from_isr(event);
}
```

### Proactor Pattern in Rust

```rust
use freertos_rust::*;
use core::future::Future;
use core::pin::Pin;
use core::task::{Context, Poll, Waker};
use alloc::sync::Arc;
use alloc::boxed::Box;

// Async operation types
#[derive(Debug, Clone, Copy)]
pub enum AsyncOpType {
    Read,
    Write,
    Transfer,
}

// Completion status
#[derive(Debug, Clone, Copy)]
pub enum CompletionStatus {
    Success,
    Error,
    Timeout,
}

// Completion result
pub struct CompletionResult {
    pub status: CompletionStatus,
    pub bytes_transferred: usize,
}

// Async operation
pub struct AsyncOperation {
    op_type: AsyncOpType,
    buffer: Box<[u8]>,
    waker: Option<Waker>,
    result: Option<CompletionResult>,
}

impl AsyncOperation {
    pub fn new(op_type: AsyncOpType, buffer: Box<[u8]>) -> Self {
        Self {
            op_type,
            buffer,
            waker: None,
            result: None,
        }
    }
    
    pub fn complete(&mut self, status: CompletionStatus, bytes: usize) {
        self.result = Some(CompletionResult {
            status,
            bytes_transferred: bytes,
        });
        
        if let Some(waker) = self.waker.take() {
            waker.wake();
        }
    }
}

// Future wrapper for async operations
pub struct AsyncIoFuture {
    operation: Arc<Mutex<AsyncOperation>>,
}

impl AsyncIoFuture {
    pub fn new(operation: Arc<Mutex<AsyncOperation>>) -> Self {
        Self { operation }
    }
}

impl Future for AsyncIoFuture {
    type Output = CompletionResult;
    
    fn poll(self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        let mut op = self.operation.lock().unwrap();
        
        match &op.result {
            Some(result) => Poll::Ready(*result),
            None => {
                op.waker = Some(cx.waker().clone());
                Poll::Pending
            }
        }
    }
}

// Proactor
pub struct Proactor {
    operation_queue: Queue<Arc<Mutex<AsyncOperation>>>,
    completion_queue: Queue<Arc<Mutex<AsyncOperation>>>,
}

impl Proactor {
    pub fn new() -> Self {
        Self {
            operation_queue: Queue::new(16).unwrap(),
            completion_queue: Queue::new(16).unwrap(),
        }
    }
    
    pub fn async_read(&self, buffer: Box<[u8]>) -> AsyncIoFuture {
        let operation = Arc::new(Mutex::new(AsyncOperation::new(
            AsyncOpType::Read,
            buffer,
        )));
        
        self.operation_queue.send(operation.clone(), Duration::zero()).unwrap();
        AsyncIoFuture::new(operation)
    }
    
    pub fn async_write(&self, buffer: Box<[u8]>) -> AsyncIoFuture {
        let operation = Arc::new(Mutex::new(AsyncOperation::new(
            AsyncOpType::Write,
            buffer,
        )));
        
        self.operation_queue.send(operation.clone(), Duration::zero()).unwrap();
        AsyncIoFuture::new(operation)
    }
    
    // Processor task - performs actual I/O
    pub fn spawn_processor(&self, priority: u8) -> Task {
        let op_queue = self.operation_queue.clone();
        let comp_queue = self.completion_queue.clone();
        
        Task::new()
            .name("AsyncProc")
            .stack_size(2048)
            .priority(TaskPriority(priority))
            .start(move || {
                loop {
                    if let Ok(operation) = op_queue.receive(Duration::infinite()) {
                        // Perform I/O operation
                        let result = Self::perform_io(&operation);
                        
                        // Post to completion queue
                        comp_queue.send(operation.clone(), Duration::zero()).ok();
                        
                        // Complete the operation
                        let mut op = operation.lock().unwrap();
                        op.complete(result.status, result.bytes_transferred);
                    }
                }
            })
            .unwrap()
    }
    
    fn perform_io(operation: &Arc<Mutex<AsyncOperation>>) -> CompletionResult {
        let op = operation.lock().unwrap();
        
        // Simulate I/O operation
        match op.op_type {
            AsyncOpType::Read => {
                // Perform actual read via DMA or interrupt-driven I/O
                let bytes = unsafe { 
                    hal_async_read(op.buffer.as_ptr() as *mut u8, op.buffer.len())
                };
                
                CompletionResult {
                    status: if bytes > 0 { 
                        CompletionStatus::Success 
                    } else { 
                        CompletionStatus::Error 
                    },
                    bytes_transferred: bytes as usize,
                }
            }
            AsyncOpType::Write => {
                let bytes = unsafe { 
                    hal_async_write(op.buffer.as_ptr(), op.buffer.len())
                };
                
                CompletionResult {
                    status: if bytes > 0 { 
                        CompletionStatus::Success 
                    } else { 
                        CompletionStatus::Error 
                    },
                    bytes_transferred: bytes as usize,
                }
            }
            AsyncOpType::Transfer => {
                // DMA transfer
                CompletionResult {
                    status: CompletionStatus::Success,
                    bytes_transferred: op.buffer.len(),
                }
            }
        }
    }
}

// Example async task using proactor
pub async fn communication_task(proactor: &Proactor) {
    loop {
        // Async read
        let rx_buffer = Box::new([0u8; 256]);
        let result = proactor.async_read(rx_buffer).await;
        
        if let CompletionStatus::Success = result.status {
            println!("Received {} bytes", result.bytes_transferred);
            
            // Process data and prepare response
            let response = prepare_response();
            
            // Async write
            let write_result = proactor.async_write(response).await;
            
            if let CompletionStatus::Success = write_result.status {
                println!("Sent {} bytes", write_result.bytes_transferred);
            }
        }
        
        // Task can do other work or delay
        Task::delay(Duration::ms(100));
    }
}

fn prepare_response() -> Box<[u8]> {
    Box::new([0u8; 128])
}

// HAL stubs (would be implemented by actual hardware layer)
unsafe fn hal_async_read(buffer: *mut u8, len: usize) -> isize {
    // Actual DMA or interrupt-driven read
    0
}

unsafe fn hal_async_write(buffer: *const u8, len: usize) -> isize {
    // Actual DMA or interrupt-driven write
    0
}
```

### Modern Async Rust with Embassy-like Pattern

```rust
use freertos_rust::*;
use core::future::Future;

// Async I/O abstraction
pub struct AsyncUart {
    handle: *mut core::ffi::c_void,
}

impl AsyncUart {
    pub fn new(handle: *mut core::ffi::c_void) -> Self {
        Self { handle }
    }
    
    pub async fn read(&mut self, buffer: &mut [u8]) -> Result<usize, ()> {
        AsyncRead::new(self.handle, buffer).await
    }
    
    pub async fn write(&mut self, buffer: &[u8]) -> Result<usize, ()> {
        AsyncWrite::new(self.handle, buffer).await
    }
}

struct AsyncRead<'a> {
    handle: *mut core::ffi::c_void,
    buffer: &'a mut [u8],
    waker: Option<Waker>,
}

impl<'a> AsyncRead<'a> {
    fn new(handle: *mut core::ffi::c_void, buffer: &'a mut [u8]) -> Self {
        Self {
            handle,
            buffer,
            waker: None,
        }
    }
}

impl Future for AsyncRead<'_> {
    type Output = Result<usize, ()>;
    
    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        // Check if data is available
        let available = unsafe { uart_data_available(self.handle) };
        
        if available > 0 {
            let bytes = unsafe {
                uart_read_nonblocking(
                    self.handle,
                    self.buffer.as_mut_ptr(),
                    self.buffer.len().min(available)
                )
            };
            Poll::Ready(Ok(bytes))
        } else {
            // Register waker for interrupt notification
            self.waker = Some(cx.waker().clone());
            unsafe { uart_register_waker(self.handle, &self.waker) };
            Poll::Pending
        }
    }
}

// Application using async/await
pub async fn sensor_monitoring_task() {
    let mut uart = AsyncUart::new(get_uart_handle());
    let mut buffer = [0u8; 128];
    
    loop {
        // Asynchronously wait for sensor data
        match uart.read(&mut buffer).await {
            Ok(len) => {
                process_sensor_data(&buffer[..len]);
                
                // Send acknowledgment
                let ack = b"OK\r\n";
                uart.write(ack).await.ok();
            }
            Err(_) => {
                println!("UART read error");
            }
        }
    }
}

// HAL stubs
unsafe fn uart_data_available(handle: *mut core::ffi::c_void) -> usize { 0 }
unsafe fn uart_read_nonblocking(
    handle: *mut core::ffi::c_void,
    buffer: *mut u8,
    len: usize
) -> usize { 0 }
unsafe fn uart_register_waker(
    handle: *mut core::ffi::c_void,
    waker: &Option<Waker>
) {}
fn get_uart_handle() -> *mut core::ffi::c_void { 
    core::ptr::null_mut() 
}
fn process_sensor_data(data: &[u8]) {}
```

---

## Performance Considerations

### Memory Usage

**Reactor Pattern:**
- Event queue size: `N events × sizeof(Event)`
- Handler table: Small, typically fixed size
- Stack per task: Moderate (depends on handler complexity)

**Proactor Pattern:**
- Operation queue + Completion queue: `2N operations × sizeof(Operation)`
- Additional buffers for async I/O
- Multiple tasks (processor + dispatcher): 2× stack overhead

### Latency

**Reactor:**
- Low latency for event handling
- Direct dispatch from queue to handler
- Single context switch per event

**Proactor:**
- Higher latency due to multi-stage processing
- Two context switches: initiation → completion
- Better for high-throughput scenarios

### CPU Utilization

**Reactor:**
- Efficient for I/O-bound workloads
- Blocks when no events available
- Good for multiple low-bandwidth sources

**Proactor:**
- Better CPU utilization during I/O
- Application continues while I/O in progress
- Ideal for DMA-capable peripherals

### Choosing Between Patterns

| Use Case | Recommended Pattern |
|----------|-------------------|
| Multiple sensors polling | Reactor |
| High-speed UART/SPI with DMA | Proactor |
| Button/interrupt handling | Reactor |
| File system operations | Proactor |
| Network packet processing | Hybrid |
| Real-time control loops | Reactor |

---

## Summary

### Reactor Pattern

**Strengths:**
- ✅ Simple to implement and understand
- ✅ Low overhead and latency
- ✅ Excellent for event-driven systems
- ✅ Natural fit for FreeRTOS queues and event groups
- ✅ Synchronous flow easier to debug

**Weaknesses:**
- ❌ Handler blocks during I/O
- ❌ Less efficient for long-running operations
- ❌ May require multiple tasks for concurrent I/O

**Best For:**
- Interrupt-driven sensor systems
- Button and user input handling
- Protocol state machines
- Low to moderate I/O throughput

### Proactor Pattern

**Strengths:**
- ✅ Non-blocking I/O maximizes CPU utilization
- ✅ Excellent for DMA and async hardware
- ✅ Scales well with high-throughput operations
- ✅ Application continues during I/O

**Weaknesses:**
- ❌ More complex implementation
- ❌ Higher memory overhead
- ❌ Additional latency from multi-stage processing
- ❌ Harder to debug

**Best For:**
- High-speed communication (UART, SPI, I2C with DMA)
- File system and flash operations
- Network protocol stacks
- Multi-channel data acquisition

### Implementation Recommendations

1. **Start with Reactor** for most embedded applications - it's simpler and sufficient for many use cases

2. **Use Proactor** when:
   - DMA is available and critical
   - I/O operations are time-consuming
   - You need to maximize CPU utilization

3. **Hybrid Approach**: Use Reactor for control flow and fast events, Proactor for heavyweight I/O operations

4. **Language Choice**:
   - **C**: Maximum control, lowest overhead, widest toolchain support
   - **C++**: Better abstraction with templates and RAII, moderate overhead
   - **Rust**: Memory safety, zero-cost abstractions, async/await support, growing ecosystem

### Key Takeaways

- Both patterns enable responsive, event-driven embedded systems
- Reactor is synchronous and simpler; Proactor is asynchronous and more complex
- FreeRTOS primitives (queues, tasks, notifications) support both patterns
- Choose based on I/O characteristics, latency requirements, and system complexity
- Modern Rust with async/await provides the most ergonomic proactor implementation
- Performance depends on event rate, I/O duration, and hardware capabilities

These patterns form the foundation of scalable, maintainable embedded architectures that can handle complex I/O requirements while maintaining system responsiveness.