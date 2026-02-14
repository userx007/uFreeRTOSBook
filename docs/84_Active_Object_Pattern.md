# Active Object Pattern in FreeRTOS

## Introduction

The Active Object Pattern is a powerful concurrency design pattern that encapsulates behavior, state, and execution context within a single, self-contained object. In FreeRTOS and real-time embedded systems, this pattern provides a structured approach to concurrent programming by combining object-oriented design with task-based concurrency. Each active object runs in its own task context and communicates with other objects exclusively through asynchronous message passing, eliminating the need for shared state and reducing synchronization complexity.

## Core Concepts

The Active Object Pattern consists of several key components working together:

**Encapsulated Task Context**: Each active object has its own dedicated FreeRTOS task that processes events independently. This task runs an event loop that continuously retrieves and processes messages from the object's queue.

**Message Queue**: The primary communication mechanism is a message queue (implemented using FreeRTOS queues) that decouples senders from receivers. External entities interact with the active object by posting messages to this queue rather than calling methods directly.

**Event-Driven Behavior**: The active object processes events sequentially in its own task context, maintaining internal state without requiring locks or mutexes since all state access is serialized through the message queue.

**Asynchronous Interface**: All public operations are asynchronous - they post messages and return immediately, allowing the caller to continue without blocking.

## Benefits in Embedded Systems

This pattern offers several advantages for real-time systems. It eliminates race conditions by serializing all access to the object's state through its event queue. The pattern naturally prevents deadlocks since there are no shared locks between active objects. It also provides better modularity and testability since each active object is an independent unit with well-defined message interfaces. Additionally, the pattern simplifies reasoning about concurrent behavior since each active object processes events sequentially.

## C/C++ Implementation

Here's a comprehensive example of implementing the Active Object Pattern in C for FreeRTOS:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>
#include <stdbool.h>

// Message types for the active object
typedef enum {
    MSG_PROCESS_DATA,
    MSG_SET_PARAMETER,
    MSG_GET_STATUS,
    MSG_SHUTDOWN
} MessageType_t;

// Generic message structure
typedef struct {
    MessageType_t type;
    union {
        struct {
            uint32_t data;
            uint32_t timestamp;
        } processData;
        struct {
            uint32_t paramId;
            int32_t value;
        } setParam;
        struct {
            uint32_t* statusOut;
            SemaphoreHandle_t doneSemaphore;
        } getStatus;
    } payload;
} Message_t;

// Active Object structure
typedef struct {
    TaskHandle_t taskHandle;
    QueueHandle_t messageQueue;
    // Internal state (private)
    uint32_t state;
    uint32_t parameter;
    bool running;
} ActiveObject_t;

// Active Object task function
static void activeObjectTask(void* pvParameters) {
    ActiveObject_t* ao = (ActiveObject_t*)pvParameters;
    Message_t msg;
    
    ao->running = true;
    
    while (ao->running) {
        // Wait for messages in the queue
        if (xQueueReceive(ao->messageQueue, &msg, portMAX_DELAY) == pdTRUE) {
            // Process messages based on type
            switch (msg.type) {
                case MSG_PROCESS_DATA:
                    // Process data - all state access is serialized here
                    ao->state = msg.payload.processData.data * ao->parameter;
                    // Perform processing...
                    vTaskDelay(pdMS_TO_TICKS(10)); // Simulate work
                    break;
                    
                case MSG_SET_PARAMETER:
                    // Update parameter
                    if (msg.payload.setParam.paramId == 1) {
                        ao->parameter = msg.payload.setParam.value;
                    }
                    break;
                    
                case MSG_GET_STATUS:
                    // Return status (synchronous query pattern)
                    *(msg.payload.getStatus.statusOut) = ao->state;
                    // Signal completion
                    xSemaphoreGive(msg.payload.getStatus.doneSemaphore);
                    break;
                    
                case MSG_SHUTDOWN:
                    ao->running = false;
                    break;
            }
        }
    }
    
    // Cleanup
    vTaskDelete(NULL);
}

// Constructor - Create and initialize the active object
ActiveObject_t* ActiveObject_Create(uint32_t queueLength, UBaseType_t priority) {
    ActiveObject_t* ao = pvPortMalloc(sizeof(ActiveObject_t));
    if (ao == NULL) {
        return NULL;
    }
    
    // Initialize state
    ao->state = 0;
    ao->parameter = 1;
    ao->running = false;
    
    // Create message queue
    ao->messageQueue = xQueueCreate(queueLength, sizeof(Message_t));
    if (ao->messageQueue == NULL) {
        vPortFree(ao);
        return NULL;
    }
    
    // Create task
    BaseType_t result = xTaskCreate(
        activeObjectTask,
        "ActiveObject",
        configMINIMAL_STACK_SIZE * 2,
        ao,
        priority,
        &ao->taskHandle
    );
    
    if (result != pdPASS) {
        vQueueDelete(ao->messageQueue);
        vPortFree(ao);
        return NULL;
    }
    
    return ao;
}

// Asynchronous interface - Post a message to process data
bool ActiveObject_ProcessData(ActiveObject_t* ao, uint32_t data, uint32_t timestamp) {
    Message_t msg;
    msg.type = MSG_PROCESS_DATA;
    msg.payload.processData.data = data;
    msg.payload.processData.timestamp = timestamp;
    
    // Non-blocking send
    return xQueueSend(ao->messageQueue, &msg, 0) == pdTRUE;
}

// Asynchronous interface - Set parameter
bool ActiveObject_SetParameter(ActiveObject_t* ao, uint32_t paramId, int32_t value) {
    Message_t msg;
    msg.type = MSG_SET_PARAMETER;
    msg.payload.setParam.paramId = paramId;
    msg.payload.setParam.value = value;
    
    return xQueueSend(ao->messageQueue, &msg, 0) == pdTRUE;
}

// Synchronous query - Get status (using semaphore for synchronization)
bool ActiveObject_GetStatus(ActiveObject_t* ao, uint32_t* status, TickType_t timeout) {
    Message_t msg;
    msg.type = MSG_GET_STATUS;
    msg.payload.getStatus.statusOut = status;
    
    // Create a binary semaphore for synchronization
    SemaphoreHandle_t doneSem = xSemaphoreCreateBinary();
    if (doneSem == NULL) {
        return false;
    }
    
    msg.payload.getStatus.doneSemaphore = doneSem;
    
    // Send message
    if (xQueueSend(ao->messageQueue, &msg, timeout) != pdTRUE) {
        vSemaphoreDelete(doneSem);
        return false;
    }
    
    // Wait for completion
    bool result = xSemaphoreTake(doneSem, timeout) == pdTRUE;
    vSemaphoreDelete(doneSem);
    
    return result;
}

// Shutdown the active object
void ActiveObject_Shutdown(ActiveObject_t* ao, TickType_t timeout) {
    Message_t msg;
    msg.type = MSG_SHUTDOWN;
    xQueueSend(ao->messageQueue, &msg, timeout);
}
```

Here's a C++ implementation using classes for better encapsulation:

```cpp
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <memory>
#include <functional>

class ActiveObject {
public:
    // Message type with polymorphic behavior
    class Message {
    public:
        virtual ~Message() = default;
        virtual void execute(ActiveObject& ao) = 0;
    };
    
    // Specific message types
    class ProcessDataMsg : public Message {
        uint32_t data_;
        uint32_t timestamp_;
    public:
        ProcessDataMsg(uint32_t data, uint32_t timestamp) 
            : data_(data), timestamp_(timestamp) {}
        
        void execute(ActiveObject& ao) override {
            ao.state_ = data_ * ao.parameter_;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    };
    
    class SetParameterMsg : public Message {
        uint32_t value_;
    public:
        explicit SetParameterMsg(uint32_t value) : value_(value) {}
        
        void execute(ActiveObject& ao) override {
            ao.parameter_ = value_;
        }
    };
    
    class GetStatusMsg : public Message {
        uint32_t& statusRef_;
        SemaphoreHandle_t doneSem_;
    public:
        GetStatusMsg(uint32_t& status, SemaphoreHandle_t sem)
            : statusRef_(status), doneSem_(sem) {}
        
        void execute(ActiveObject& ao) override {
            statusRef_ = ao.state_;
            xSemaphoreGive(doneSem_);
        }
    };

private:
    TaskHandle_t taskHandle_;
    QueueHandle_t messageQueue_;
    uint32_t state_;
    uint32_t parameter_;
    volatile bool running_;
    
    // Task function
    static void taskFunction(void* pvParameters) {
        ActiveObject* self = static_cast<ActiveObject*>(pvParameters);
        self->run();
    }
    
    void run() {
        running_ = true;
        Message* msg;
        
        while (running_) {
            if (xQueueReceive(messageQueue_, &msg, portMAX_DELAY) == pdTRUE) {
                msg->execute(*this);
                delete msg;  // Clean up message
            }
        }
        
        vTaskDelete(NULL);
    }

public:
    ActiveObject(size_t queueLength, UBaseType_t priority)
        : taskHandle_(nullptr)
        , messageQueue_(nullptr)
        , state_(0)
        , parameter_(1)
        , running_(false) {
        
        // Create queue for message pointers
        messageQueue_ = xQueueCreate(queueLength, sizeof(Message*));
        configASSERT(messageQueue_ != nullptr);
        
        // Create task
        BaseType_t result = xTaskCreate(
            taskFunction,
            "ActiveObject",
            2048,
            this,
            priority,
            &taskHandle_
        );
        configASSERT(result == pdPASS);
    }
    
    ~ActiveObject() {
        if (messageQueue_) {
            vQueueDelete(messageQueue_);
        }
    }
    
    // Asynchronous operations
    bool processData(uint32_t data, uint32_t timestamp) {
        Message* msg = new ProcessDataMsg(data, timestamp);
        if (xQueueSend(messageQueue_, &msg, 0) != pdTRUE) {
            delete msg;
            return false;
        }
        return true;
    }
    
    bool setParameter(uint32_t value) {
        Message* msg = new SetParameterMsg(value);
        if (xQueueSend(messageQueue_, &msg, 0) != pdTRUE) {
            delete msg;
            return false;
        }
        return true;
    }
    
    // Synchronous query
    bool getStatus(uint32_t& status, TickType_t timeout = portMAX_DELAY) {
        SemaphoreHandle_t doneSem = xSemaphoreCreateBinary();
        if (!doneSem) return false;
        
        Message* msg = new GetStatusMsg(status, doneSem);
        
        if (xQueueSend(messageQueue_, &msg, timeout) != pdTRUE) {
            delete msg;
            vSemaphoreDelete(doneSem);
            return false;
        }
        
        bool result = xSemaphoreTake(doneSem, timeout) == pdTRUE;
        vSemaphoreDelete(doneSem);
        return result;
    }
    
    void shutdown() {
        running_ = false;
    }
};

// Usage example
void exampleUsage() {
    ActiveObject ao(10, tskIDLE_PRIORITY + 2);
    
    // Asynchronous operations - return immediately
    ao.processData(42, xTaskGetTickCount());
    ao.setParameter(5);
    
    // Synchronous query
    uint32_t status;
    if (ao.getStatus(status, pdMS_TO_TICKS(100))) {
        // Use status value
    }
}
```

## Rust Implementation

Rust's ownership model and type system make it particularly well-suited for implementing the Active Object Pattern. Here's a comprehensive implementation using `freertos-rust` bindings:

```rust
use freertos_rust::*;
use std::sync::Arc;

// Message types using Rust enums
enum Message {
    ProcessData {
        data: u32,
        timestamp: u32,
    },
    SetParameter {
        param_id: u32,
        value: i32,
    },
    GetStatus {
        response_tx: Sender<u32>,
    },
    Shutdown,
}

// Active Object structure
pub struct ActiveObject {
    message_queue: Queue<Message>,
    task_handle: Option<Task>,
}

// Internal state (not accessible from outside)
struct ActiveObjectState {
    state: u32,
    parameter: u32,
    running: bool,
}

impl ActiveObject {
    pub fn new(queue_length: usize, priority: UBaseType) -> Result<Self, FreeRtosError> {
        let message_queue = Queue::new(queue_length)?;
        let queue_clone = message_queue.clone();
        
        // Spawn the active object task
        let task_handle = Task::new()
            .name("ActiveObject")
            .stack_size(2048)
            .priority(TaskPriority(priority))
            .start(move || {
                Self::run_loop(queue_clone);
            })?;
        
        Ok(ActiveObject {
            message_queue,
            task_handle: Some(task_handle),
        })
    }
    
    // The main event loop running in its own task
    fn run_loop(queue: Queue<Message>) {
        let mut state = ActiveObjectState {
            state: 0,
            parameter: 1,
            running: true,
        };
        
        while state.running {
            match queue.receive(Duration::infinite()) {
                Ok(msg) => {
                    Self::handle_message(&mut state, msg);
                }
                Err(_) => {
                    // Queue error, could log here
                    break;
                }
            }
        }
    }
    
    fn handle_message(state: &mut ActiveObjectState, msg: Message) {
        match msg {
            Message::ProcessData { data, timestamp } => {
                // Process data - all state access is serialized here
                state.state = data.wrapping_mul(state.parameter);
                
                // Simulate work
                CurrentTask::delay(Duration::ms(10));
                
                // Could log timestamp or perform other operations
            }
            
            Message::SetParameter { param_id, value } => {
                if param_id == 1 && value >= 0 {
                    state.parameter = value as u32;
                }
            }
            
            Message::GetStatus { response_tx } => {
                // Send current state back to caller
                let _ = response_tx.send(state.state, Duration::ms(0));
            }
            
            Message::Shutdown => {
                state.running = false;
            }
        }
    }
    
    // Public asynchronous interface
    pub fn process_data(&self, data: u32, timestamp: u32) -> Result<(), FreeRtosError> {
        self.message_queue.send(
            Message::ProcessData { data, timestamp },
            Duration::ms(0)
        )
    }
    
    pub fn set_parameter(&self, param_id: u32, value: i32) -> Result<(), FreeRtosError> {
        self.message_queue.send(
            Message::SetParameter { param_id, value },
            Duration::ms(0)
        )
    }
    
    // Synchronous query using channels
    pub fn get_status(&self, timeout: Duration) -> Result<u32, FreeRtosError> {
        // Create a one-shot channel for the response
        let (tx, rx) = channel::<u32>(1)?;
        
        // Send the query message
        self.message_queue.send(
            Message::GetStatus { response_tx: tx },
            timeout
        )?;
        
        // Wait for response
        rx.receive(timeout)
    }
    
    pub fn shutdown(&self) -> Result<(), FreeRtosError> {
        self.message_queue.send(Message::Shutdown, Duration::ms(100))
    }
}

// More type-safe version using traits
trait MessageHandler {
    fn handle(&mut self, msg: Box<dyn MessageTrait>);
}

trait MessageTrait: Send {
    fn execute(&self, state: &mut ActiveObjectState);
}

// Type-safe message implementations
struct ProcessDataMessage {
    data: u32,
    timestamp: u32,
}

impl MessageTrait for ProcessDataMessage {
    fn execute(&self, state: &mut ActiveObjectState) {
        state.state = self.data.wrapping_mul(state.parameter);
        CurrentTask::delay(Duration::ms(10));
    }
}

struct SetParameterMessage {
    value: u32,
}

impl MessageTrait for SetParameterMessage {
    fn execute(&self, state: &mut ActiveObjectState) {
        state.parameter = self.value;
    }
}

// Enhanced Active Object with better type safety
pub struct TypeSafeActiveObject {
    message_queue: Queue<Box<dyn MessageTrait>>,
}

impl TypeSafeActiveObject {
    pub fn new(queue_length: usize, priority: UBaseType) -> Result<Self, FreeRtosError> {
        let message_queue = Queue::new(queue_length)?;
        let queue_clone = message_queue.clone();
        
        Task::new()
            .name("TypeSafeAO")
            .stack_size(2048)
            .priority(TaskPriority(priority))
            .start(move || {
                let mut state = ActiveObjectState {
                    state: 0,
                    parameter: 1,
                    running: true,
                };
                
                while state.running {
                    if let Ok(msg) = queue_clone.receive(Duration::infinite()) {
                        msg.execute(&mut state);
                    }
                }
            })?;
        
        Ok(TypeSafeActiveObject { message_queue })
    }
    
    pub fn send_message(&self, msg: Box<dyn MessageTrait>) -> Result<(), FreeRtosError> {
        self.message_queue.send(msg, Duration::ms(0))
    }
}

// Usage example
fn example_usage() -> Result<(), FreeRtosError> {
    let active_obj = ActiveObject::new(10, 2)?;
    
    // Asynchronous operations
    active_obj.process_data(42, 1000)?;
    active_obj.set_parameter(1, 5)?;
    
    // Synchronous query
    match active_obj.get_status(Duration::ms(100)) {
        Ok(status) => {
            // Use status value
            println!("Status: {}", status);
        }
        Err(e) => {
            println!("Failed to get status: {:?}", e);
        }
    }
    
    // Shutdown
    active_obj.shutdown()?;
    
    Ok(())
}

// Example with async/await patterns (if using an async runtime)
#[cfg(feature = "async")]
pub mod async_active_object {
    use super::*;
    
    pub struct AsyncActiveObject {
        sender: async_channel::Sender<Message>,
    }
    
    impl AsyncActiveObject {
        pub fn new(queue_length: usize) -> Self {
            let (tx, rx) = async_channel::bounded(queue_length);
            
            // Spawn async task
            Task::new()
                .start(move || {
                    let mut state = ActiveObjectState {
                        state: 0,
                        parameter: 1,
                        running: true,
                    };
                    
                    async_runtime::block_on(async {
                        while let Ok(msg) = rx.recv().await {
                            // Handle message asynchronously
                            handle_async_message(&mut state, msg).await;
                        }
                    });
                })
                .unwrap();
            
            AsyncActiveObject { sender: tx }
        }
        
        pub async fn process_data(&self, data: u32) -> Result<(), async_channel::SendError<Message>> {
            self.sender.send(Message::ProcessData { 
                data, 
                timestamp: 0 
            }).await
        }
    }
}
```

## Advanced Pattern: Multi-Level Active Objects

For complex systems, you can create hierarchies of active objects:

```c
// Parent active object that manages child active objects
typedef struct {
    ActiveObject_t* sensor_processor;
    ActiveObject_t* data_logger;
    ActiveObject_t* network_manager;
    QueueHandle_t commandQueue;
    TaskHandle_t coordinatorTask;
} SystemCoordinator_t;

// The coordinator receives high-level commands and dispatches
// to appropriate child active objects
static void coordinatorTask(void* pvParameters) {
    SystemCoordinator_t* coord = (SystemCoordinator_t*)pvParameters;
    SystemCommand_t cmd;
    
    while (1) {
        if (xQueueReceive(coord->commandQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.type) {
                case CMD_PROCESS_SENSOR:
                    ActiveObject_ProcessData(
                        coord->sensor_processor,
                        cmd.sensorData,
                        xTaskGetTickCount()
                    );
                    break;
                    
                case CMD_LOG_DATA:
                    // Forward to logger
                    break;
                    
                case CMD_SEND_NETWORK:
                    // Forward to network manager
                    break;
            }
        }
    }
}
```

## Summary

The Active Object Pattern is an essential design pattern for building robust concurrent systems in FreeRTOS. By encapsulating each concurrent entity in its own task with message-based communication, it eliminates the complexity and pitfalls of shared-state concurrency. The pattern's key strengths include eliminating race conditions through serialized event processing, natural prevention of deadlocks, improved modularity with clear message-based interfaces, and simplified reasoning about concurrent behavior.

In C implementations, the pattern requires careful manual memory management and explicit message structures, but provides full control and minimal overhead. C++ implementations benefit from polymorphism and RAII for cleaner resource management. Rust implementations leverage the type system and ownership model to enforce safety at compile time, making many concurrency bugs impossible.

The Active Object Pattern is particularly valuable in embedded systems where you need predictable, deterministic behavior with clear separation of concerns. It scales well from simple single-object systems to complex hierarchies of cooperating active objects, and the message-based interface makes the system highly testable and maintainable. When combined with FreeRTOS's efficient task scheduling and queue mechanisms, this pattern provides a powerful foundation for building reliable real-time embedded applications.