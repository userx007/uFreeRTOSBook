# State Machine Implementation in FreeRTOS

## Overview

State machines are fundamental design patterns in embedded systems that model systems with distinct states and well-defined transitions between them. In FreeRTOS, state machines can be implemented as tasks that respond to events, making them ideal for handling complex control logic, protocol implementations, and user interface management.

## Detailed Description

### What is a State Machine?

A state machine (or finite state machine - FSM) consists of:
- **States**: Distinct conditions or modes of operation
- **Events**: Triggers that cause transitions
- **Transitions**: Rules defining how events move the system between states
- **Actions**: Operations performed during transitions or within states

### Hierarchical State Machines (HSM)

Hierarchical state machines add the concept of **nested states** where:
- States can contain sub-states
- Parent states handle common behavior
- Sub-states inherit parent behavior and can override it
- Reduces code duplication and improves maintainability

### Event-Driven Pattern in FreeRTOS

FreeRTOS naturally supports event-driven state machines through:
- **Queues**: For receiving events
- **Tasks**: For processing state logic
- **Semaphores/Event Groups**: For synchronization
- **Timers**: For timeout events

---

## C/C++ Implementation

### Basic State Machine Implementation

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Define states
typedef enum {
    STATE_IDLE,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_ERROR
} SystemState_t;

// Define events
typedef enum {
    EVENT_START,
    EVENT_STOP,
    EVENT_PAUSE,
    EVENT_RESUME,
    EVENT_ERROR_OCCURRED,
    EVENT_RESET
} SystemEvent_t;

// Event structure
typedef struct {
    SystemEvent_t event;
    void* pData;  // Optional event data
} StateEvent_t;

// State machine context
typedef struct {
    SystemState_t currentState;
    SystemState_t previousState;
    QueueHandle_t eventQueue;
    uint32_t errorCount;
} StateMachine_t;

// Function prototypes for state handlers
static void handleIdleState(StateMachine_t* sm, SystemEvent_t event);
static void handleRunningState(StateMachine_t* sm, SystemEvent_t event);
static void handlePausedState(StateMachine_t* sm, SystemEvent_t event);
static void handleErrorState(StateMachine_t* sm, SystemEvent_t event);

// State transition function
static void transitionToState(StateMachine_t* sm, SystemState_t newState) {
    printf("Transitioning from %d to %d\n", sm->currentState, newState);
    
    // Exit current state (optional cleanup)
    switch(sm->currentState) {
        case STATE_RUNNING:
            // Stop running processes
            break;
        default:
            break;
    }
    
    sm->previousState = sm->currentState;
    sm->currentState = newState;
    
    // Enter new state (optional initialization)
    switch(newState) {
        case STATE_RUNNING:
            // Start running processes
            break;
        default:
            break;
    }
}

// State handlers
static void handleIdleState(StateMachine_t* sm, SystemEvent_t event) {
    switch(event) {
        case EVENT_START:
            transitionToState(sm, STATE_RUNNING);
            break;
        case EVENT_ERROR_OCCURRED:
            transitionToState(sm, STATE_ERROR);
            break;
        default:
            // Ignore other events in IDLE state
            break;
    }
}

static void handleRunningState(StateMachine_t* sm, SystemEvent_t event) {
    switch(event) {
        case EVENT_STOP:
            transitionToState(sm, STATE_IDLE);
            break;
        case EVENT_PAUSE:
            transitionToState(sm, STATE_PAUSED);
            break;
        case EVENT_ERROR_OCCURRED:
            transitionToState(sm, STATE_ERROR);
            break;
        default:
            break;
    }
}

static void handlePausedState(StateMachine_t* sm, SystemEvent_t event) {
    switch(event) {
        case EVENT_RESUME:
            transitionToState(sm, STATE_RUNNING);
            break;
        case EVENT_STOP:
            transitionToState(sm, STATE_IDLE);
            break;
        case EVENT_ERROR_OCCURRED:
            transitionToState(sm, STATE_ERROR);
            break;
        default:
            break;
    }
}

static void handleErrorState(StateMachine_t* sm, SystemEvent_t event) {
    switch(event) {
        case EVENT_RESET:
            sm->errorCount = 0;
            transitionToState(sm, STATE_IDLE);
            break;
        default:
            break;
    }
}

// Main state machine task
void vStateMachineTask(void* pvParameters) {
    StateMachine_t sm;
    StateEvent_t event;
    
    // Initialize state machine
    sm.currentState = STATE_IDLE;
    sm.previousState = STATE_IDLE;
    sm.errorCount = 0;
    sm.eventQueue = xQueueCreate(10, sizeof(StateEvent_t));
    
    while(1) {
        // Wait for events
        if(xQueueReceive(sm.eventQueue, &event, portMAX_DELAY) == pdTRUE) {
            // Process event based on current state
            switch(sm.currentState) {
                case STATE_IDLE:
                    handleIdleState(&sm, event.event);
                    break;
                case STATE_RUNNING:
                    handleRunningState(&sm, event.event);
                    break;
                case STATE_PAUSED:
                    handlePausedState(&sm, event.event);
                    break;
                case STATE_ERROR:
                    handleErrorState(&sm, event.event);
                    break;
            }
        }
    }
}
```

### Hierarchical State Machine (C++)

```cpp
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <functional>
#include <map>

class HierarchicalStateMachine {
public:
    enum class State {
        SYSTEM_OFF,
        SYSTEM_ON,
        ON_IDLE,
        ON_ACTIVE,
        ON_ACTIVE_PROCESSING,
        ON_ACTIVE_WAITING
    };
    
    enum class Event {
        POWER_ON,
        POWER_OFF,
        ACTIVATE,
        DEACTIVATE,
        START_PROCESS,
        PROCESS_COMPLETE,
        TIMEOUT
    };
    
private:
    State currentState;
    State parentState;
    QueueHandle_t eventQueue;
    
    // State hierarchy map
    std::map<State, State> stateHierarchy = {
        {State::ON_IDLE, State::SYSTEM_ON},
        {State::ON_ACTIVE, State::SYSTEM_ON},
        {State::ON_ACTIVE_PROCESSING, State::ON_ACTIVE},
        {State::ON_ACTIVE_WAITING, State::ON_ACTIVE}
    };
    
    // State handler type
    using StateHandler = std::function<bool(Event)>;
    std::map<State, StateHandler> stateHandlers;
    
    void registerStateHandlers() {
        stateHandlers[State::SYSTEM_OFF] = [this](Event e) -> bool {
            if(e == Event::POWER_ON) {
                transitionTo(State::ON_IDLE);
                return true;
            }
            return false;
        };
        
        stateHandlers[State::SYSTEM_ON] = [this](Event e) -> bool {
            if(e == Event::POWER_OFF) {
                transitionTo(State::SYSTEM_OFF);
                return true;
            }
            return false;
        };
        
        stateHandlers[State::ON_IDLE] = [this](Event e) -> bool {
            if(e == Event::ACTIVATE) {
                transitionTo(State::ON_ACTIVE);
                return true;
            }
            return false;
        };
        
        stateHandlers[State::ON_ACTIVE] = [this](Event e) -> bool {
            if(e == Event::DEACTIVATE) {
                transitionTo(State::ON_IDLE);
                return true;
            }
            return false;
        };
        
        stateHandlers[State::ON_ACTIVE_PROCESSING] = [this](Event e) -> bool {
            if(e == Event::PROCESS_COMPLETE) {
                transitionTo(State::ON_ACTIVE_WAITING);
                return true;
            }
            return false;
        };
    }
    
    void transitionTo(State newState) {
        exitState(currentState);
        currentState = newState;
        enterState(currentState);
    }
    
    void enterState(State state) {
        printf("Entering state: %d\n", static_cast<int>(state));
        
        switch(state) {
            case State::SYSTEM_ON:
                // Initialize system resources
                break;
            case State::ON_ACTIVE_PROCESSING:
                // Start processing timer
                break;
            default:
                break;
        }
    }
    
    void exitState(State state) {
        printf("Exiting state: %d\n", static_cast<int>(state));
        
        switch(state) {
            case State::SYSTEM_ON:
                // Release system resources
                break;
            default:
                break;
        }
    }
    
    bool handleEvent(Event event) {
        State currentHandlerState = currentState;
        
        // Try handling event at current state level
        while(true) {
            auto handler = stateHandlers.find(currentHandlerState);
            if(handler != stateHandlers.end()) {
                if(handler->second(event)) {
                    return true;  // Event handled
                }
            }
            
            // Try parent state
            auto parent = stateHierarchy.find(currentHandlerState);
            if(parent == stateHierarchy.end()) {
                break;  // No parent, event not handled
            }
            currentHandlerState = parent->second;
        }
        
        return false;  // Event not handled
    }
    
public:
    HierarchicalStateMachine() : currentState(State::SYSTEM_OFF) {
        eventQueue = xQueueCreate(10, sizeof(Event));
        registerStateHandlers();
    }
    
    void run() {
        Event event;
        
        while(1) {
            if(xQueueReceive(eventQueue, &event, portMAX_DELAY) == pdTRUE) {
                handleEvent(event);
            }
        }
    }
    
    void postEvent(Event event) {
        xQueueSend(eventQueue, &event, 0);
    }
    
    State getCurrentState() const { return currentState; }
};

// Task wrapper
extern "C" void vHierarchicalStateMachineTask(void* pvParameters) {
    HierarchicalStateMachine* hsm = new HierarchicalStateMachine();
    hsm->run();
}
```

### Event-Driven State Machine with Timers

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

typedef enum {
    STATE_WAITING,
    STATE_AUTHENTICATED,
    STATE_SESSION_ACTIVE,
    STATE_TIMEOUT_WARNING,
    STATE_LOGGED_OUT
} SessionState_t;

typedef enum {
    EVENT_LOGIN,
    EVENT_LOGOUT,
    EVENT_ACTIVITY,
    EVENT_TIMEOUT_WARNING,
    EVENT_TIMEOUT
} SessionEvent_t;

typedef struct {
    SessionState_t currentState;
    QueueHandle_t eventQueue;
    TimerHandle_t sessionTimer;
    TimerHandle_t warningTimer;
    uint32_t userId;
} SessionStateMachine_t;

// Timer callbacks
void vSessionTimeoutCallback(TimerHandle_t xTimer) {
    SessionStateMachine_t* sm = (SessionStateMachine_t*)pvTimerGetTimerID(xTimer);
    SessionEvent_t event = EVENT_TIMEOUT;
    xQueueSend(sm->eventQueue, &event, 0);
}

void vWarningTimeoutCallback(TimerHandle_t xTimer) {
    SessionStateMachine_t* sm = (SessionStateMachine_t*)pvTimerGetTimerID(xTimer);
    SessionEvent_t event = EVENT_TIMEOUT_WARNING;
    xQueueSend(sm->eventQueue, &event, 0);
}

void processEvent(SessionStateMachine_t* sm, SessionEvent_t event) {
    SessionState_t nextState = sm->currentState;
    
    switch(sm->currentState) {
        case STATE_WAITING:
            if(event == EVENT_LOGIN) {
                nextState = STATE_AUTHENTICATED;
                // Start session timer (5 minutes)
                xTimerStart(sm->sessionTimer, 0);
                // Start warning timer (4 minutes)
                xTimerStart(sm->warningTimer, 0);
            }
            break;
            
        case STATE_AUTHENTICATED:
            if(event == EVENT_ACTIVITY) {
                // Reset timers on activity
                xTimerReset(sm->sessionTimer, 0);
                xTimerReset(sm->warningTimer, 0);
            } else if(event == EVENT_TIMEOUT_WARNING) {
                nextState = STATE_TIMEOUT_WARNING;
            } else if(event == EVENT_LOGOUT || event == EVENT_TIMEOUT) {
                nextState = STATE_LOGGED_OUT;
                xTimerStop(sm->sessionTimer, 0);
                xTimerStop(sm->warningTimer, 0);
            }
            break;
            
        case STATE_TIMEOUT_WARNING:
            if(event == EVENT_ACTIVITY) {
                nextState = STATE_AUTHENTICATED;
                xTimerReset(sm->sessionTimer, 0);
                xTimerReset(sm->warningTimer, 0);
            } else if(event == EVENT_TIMEOUT) {
                nextState = STATE_LOGGED_OUT;
                xTimerStop(sm->sessionTimer, 0);
                xTimerStop(sm->warningTimer, 0);
            }
            break;
            
        case STATE_LOGGED_OUT:
            if(event == EVENT_LOGIN) {
                nextState = STATE_WAITING;
            }
            break;
    }
    
    if(nextState != sm->currentState) {
        printf("State transition: %d -> %d\n", sm->currentState, nextState);
        sm->currentState = nextState;
    }
}

void vSessionStateMachineTask(void* pvParameters) {
    SessionStateMachine_t sm;
    SessionEvent_t event;
    
    sm.currentState = STATE_WAITING;
    sm.eventQueue = xQueueCreate(10, sizeof(SessionEvent_t));
    
    // Create timers
    sm.sessionTimer = xTimerCreate("SessionTimer",
                                    pdMS_TO_TICKS(300000), // 5 minutes
                                    pdFALSE,
                                    &sm,
                                    vSessionTimeoutCallback);
    
    sm.warningTimer = xTimerCreate("WarningTimer",
                                    pdMS_TO_TICKS(240000), // 4 minutes
                                    pdFALSE,
                                    &sm,
                                    vWarningTimeoutCallback);
    
    while(1) {
        if(xQueueReceive(sm.eventQueue, &event, portMAX_DELAY) == pdTRUE) {
            processEvent(&sm, event);
        }
    }
}
```

---

## Rust Implementation

### Basic State Machine in Rust

```rust
use freertos_rust::{Task, Queue, Duration};

#[derive(Debug, Clone, Copy, PartialEq)]
enum SystemState {
    Idle,
    Running,
    Paused,
    Error,
}

#[derive(Debug, Clone, Copy)]
enum SystemEvent {
    Start,
    Stop,
    Pause,
    Resume,
    ErrorOccurred,
    Reset,
}

struct StateMachine {
    current_state: SystemState,
    previous_state: SystemState,
    event_queue: Queue<SystemEvent>,
    error_count: u32,
}

impl StateMachine {
    fn new() -> Self {
        StateMachine {
            current_state: SystemState::Idle,
            previous_state: SystemState::Idle,
            event_queue: Queue::new(10).unwrap(),
            error_count: 0,
        }
    }
    
    fn transition_to(&mut self, new_state: SystemState) {
        println!("Transitioning from {:?} to {:?}", 
                 self.current_state, new_state);
        
        // Exit current state
        self.exit_state(self.current_state);
        
        self.previous_state = self.current_state;
        self.current_state = new_state;
        
        // Enter new state
        self.enter_state(new_state);
    }
    
    fn enter_state(&mut self, state: SystemState) {
        match state {
            SystemState::Running => {
                println!("Starting running processes...");
                // Initialize running state
            }
            SystemState::Error => {
                self.error_count += 1;
                println!("Error state entered. Count: {}", self.error_count);
            }
            _ => {}
        }
    }
    
    fn exit_state(&mut self, state: SystemState) {
        match state {
            SystemState::Running => {
                println!("Stopping running processes...");
                // Cleanup running state
            }
            _ => {}
        }
    }
    
    fn handle_idle_state(&mut self, event: SystemEvent) {
        match event {
            SystemEvent::Start => self.transition_to(SystemState::Running),
            SystemEvent::ErrorOccurred => self.transition_to(SystemState::Error),
            _ => {} // Ignore other events
        }
    }
    
    fn handle_running_state(&mut self, event: SystemEvent) {
        match event {
            SystemEvent::Stop => self.transition_to(SystemState::Idle),
            SystemEvent::Pause => self.transition_to(SystemState::Paused),
            SystemEvent::ErrorOccurred => self.transition_to(SystemState::Error),
            _ => {}
        }
    }
    
    fn handle_paused_state(&mut self, event: SystemEvent) {
        match event {
            SystemEvent::Resume => self.transition_to(SystemState::Running),
            SystemEvent::Stop => self.transition_to(SystemState::Idle),
            SystemEvent::ErrorOccurred => self.transition_to(SystemState::Error),
            _ => {}
        }
    }
    
    fn handle_error_state(&mut self, event: SystemEvent) {
        match event {
            SystemEvent::Reset => {
                self.error_count = 0;
                self.transition_to(SystemState::Idle);
            }
            _ => {}
        }
    }
    
    fn process_event(&mut self, event: SystemEvent) {
        match self.current_state {
            SystemState::Idle => self.handle_idle_state(event),
            SystemState::Running => self.handle_running_state(event),
            SystemState::Paused => self.handle_paused_state(event),
            SystemState::Error => self.handle_error_state(event),
        }
    }
    
    fn run(&mut self) {
        loop {
            if let Some(event) = self.event_queue.receive(Duration::infinite()) {
                self.process_event(event);
            }
        }
    }
    
    fn post_event(&self, event: SystemEvent) {
        let _ = self.event_queue.send(event, Duration::zero());
    }
}

// Task creation
pub fn create_state_machine_task() {
    Task::new()
        .name("StateMachine")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(|| {
            let mut sm = StateMachine::new();
            sm.run();
        })
        .unwrap();
}
```

### Hierarchical State Machine with Trait Pattern

```rust
use freertos_rust::{Task, Queue, Duration};
use std::collections::HashMap;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
enum State {
    SystemOff,
    SystemOn,
    OnIdle,
    OnActive,
    OnActiveProcessing,
    OnActiveWaiting,
}

#[derive(Debug, Clone, Copy)]
enum Event {
    PowerOn,
    PowerOff,
    Activate,
    Deactivate,
    StartProcess,
    ProcessComplete,
    Timeout,
}

trait StateHandler {
    fn handle_event(&mut self, event: Event, hsm: &mut HierarchicalStateMachine) -> bool;
    fn entry(&mut self) {}
    fn exit(&mut self) {}
}

struct SystemOffHandler;
impl StateHandler for SystemOffHandler {
    fn handle_event(&mut self, event: Event, hsm: &mut HierarchicalStateMachine) -> bool {
        match event {
            Event::PowerOn => {
                hsm.transition_to(State::OnIdle);
                true
            }
            _ => false
        }
    }
}

struct SystemOnHandler;
impl StateHandler for SystemOnHandler {
    fn handle_event(&mut self, event: Event, hsm: &mut HierarchicalStateMachine) -> bool {
        match event {
            Event::PowerOff => {
                hsm.transition_to(State::SystemOff);
                true
            }
            _ => false
        }
    }
    
    fn entry(&mut self) {
        println!("System ON - Initializing resources");
    }
    
    fn exit(&mut self) {
        println!("System OFF - Releasing resources");
    }
}

struct OnIdleHandler;
impl StateHandler for OnIdleHandler {
    fn handle_event(&mut self, event: Event, hsm: &mut HierarchicalStateMachine) -> bool {
        match event {
            Event::Activate => {
                hsm.transition_to(State::OnActive);
                true
            }
            _ => false
        }
    }
}

struct OnActiveHandler;
impl StateHandler for OnActiveHandler {
    fn handle_event(&mut self, event: Event, hsm: &mut HierarchicalStateMachine) -> bool {
        match event {
            Event::Deactivate => {
                hsm.transition_to(State::OnIdle);
                true
            }
            _ => false
        }
    }
}

struct OnActiveProcessingHandler;
impl StateHandler for OnActiveProcessingHandler {
    fn handle_event(&mut self, event: Event, hsm: &mut HierarchicalStateMachine) -> bool {
        match event {
            Event::ProcessComplete => {
                hsm.transition_to(State::OnActiveWaiting);
                true
            }
            _ => false
        }
    }
    
    fn entry(&mut self) {
        println!("Starting processing...");
    }
}

struct HierarchicalStateMachine {
    current_state: State,
    event_queue: Queue<Event>,
    hierarchy: HashMap<State, State>, // child -> parent mapping
}

impl HierarchicalStateMachine {
    fn new() -> Self {
        let mut hierarchy = HashMap::new();
        hierarchy.insert(State::OnIdle, State::SystemOn);
        hierarchy.insert(State::OnActive, State::SystemOn);
        hierarchy.insert(State::OnActiveProcessing, State::OnActive);
        hierarchy.insert(State::OnActiveWaiting, State::OnActive);
        
        HierarchicalStateMachine {
            current_state: State::SystemOff,
            event_queue: Queue::new(10).unwrap(),
            hierarchy,
        }
    }
    
    fn transition_to(&mut self, new_state: State) {
        println!("Transition: {:?} -> {:?}", self.current_state, new_state);
        
        // Exit current state (simplified - would need full hierarchy walk)
        self.exit_state(self.current_state);
        
        self.current_state = new_state;
        
        // Enter new state
        self.enter_state(new_state);
    }
    
    fn enter_state(&self, state: State) {
        // Call entry actions based on state
        match state {
            State::OnActiveProcessing => {
                println!("Entry: OnActiveProcessing");
            }
            _ => {}
        }
    }
    
    fn exit_state(&self, state: State) {
        // Call exit actions based on state
        match state {
            State::SystemOn => {
                println!("Exit: SystemOn");
            }
            _ => {}
        }
    }
    
    fn handle_event(&mut self, event: Event) -> bool {
        let mut current = self.current_state;
        
        // Try handling at current level, then walk up hierarchy
        loop {
            let handled = match current {
                State::SystemOff => SystemOffHandler.handle_event(event, self),
                State::SystemOn => SystemOnHandler.handle_event(event, self),
                State::OnIdle => OnIdleHandler.handle_event(event, self),
                State::OnActive => OnActiveHandler.handle_event(event, self),
                State::OnActiveProcessing => OnActiveProcessingHandler.handle_event(event, self),
                _ => false,
            };
            
            if handled {
                return true;
            }
            
            // Try parent state
            if let Some(&parent) = self.hierarchy.get(&current) {
                current = parent;
            } else {
                break;
            }
        }
        
        false
    }
    
    fn run(&mut self) {
        loop {
            if let Some(event) = self.event_queue.receive(Duration::infinite()) {
                self.handle_event(event);
            }
        }
    }
    
    fn post_event(&self, event: Event) {
        let _ = self.event_queue.send(event, Duration::zero());
    }
}
```

### Advanced: State Machine with Guards and Actions

```rust
use freertos_rust::{Task, Queue, Duration};

#[derive(Debug, Clone, Copy, PartialEq)]
enum State {
    Locked,
    Unlocked,
    Alarmed,
}

#[derive(Debug, Clone)]
enum Event {
    KeyInserted(u32),  // PIN code
    DoorOpened,
    DoorClosed,
    Timeout,
    Reset,
}

struct DoorStateMachine {
    current_state: State,
    event_queue: Queue<Event>,
    valid_pin: u32,
    failed_attempts: u32,
    max_attempts: u32,
}

impl DoorStateMachine {
    fn new(valid_pin: u32) -> Self {
        DoorStateMachine {
            current_state: State::Locked,
            event_queue: Queue::new(10).unwrap(),
            valid_pin,
            failed_attempts: 0,
            max_attempts: 3,
        }
    }
    
    // Guard functions
    fn is_valid_pin(&self, pin: u32) -> bool {
        pin == self.valid_pin
    }
    
    fn is_max_attempts_reached(&self) -> bool {
        self.failed_attempts >= self.max_attempts
    }
    
    // Action functions
    fn unlock_door(&mut self) {
        println!("Door unlocked!");
        self.failed_attempts = 0;
    }
    
    fn lock_door(&mut self) {
        println!("Door locked!");
    }
    
    fn trigger_alarm(&mut self) {
        println!("ALARM! Too many failed attempts!");
    }
    
    fn increment_failed_attempts(&mut self) {
        self.failed_attempts += 1;
        println!("Failed attempt {}/{}", self.failed_attempts, self.max_attempts);
    }
    
    fn handle_locked_state(&mut self, event: Event) {
        match event {
            Event::KeyInserted(pin) => {
                if self.is_valid_pin(pin) {
                    // Guard passed, perform action and transition
                    self.unlock_door();
                    self.current_state = State::Unlocked;
                } else {
                    self.increment_failed_attempts();
                    
                    if self.is_max_attempts_reached() {
                        self.trigger_alarm();
                        self.current_state = State::Alarmed;
                    }
                }
            }
            _ => {}
        }
    }
    
    fn handle_unlocked_state(&mut self, event: Event) {
        match event {
            Event::DoorOpened => {
                println!("Door opened");
            }
            Event::DoorClosed => {
                self.lock_door();
                self.current_state = State::Locked;
            }
            Event::Timeout => {
                println!("Auto-locking due to timeout");
                self.lock_door();
                self.current_state = State::Locked;
            }
            _ => {}
        }
    }
    
    fn handle_alarmed_state(&mut self, event: Event) {
        match event {
            Event::Reset => {
                println!("Alarm reset");
                self.failed_attempts = 0;
                self.current_state = State::Locked;
            }
            _ => {
                println!("System alarmed - only RESET accepted");
            }
        }
    }
    
    fn process_event(&mut self, event: Event) {
        println!("State: {:?}, Event: {:?}", self.current_state, event);
        
        match self.current_state {
            State::Locked => self.handle_locked_state(event),
            State::Unlocked => self.handle_unlocked_state(event),
            State::Alarmed => self.handle_alarmed_state(event),
        }
    }
    
    fn run(&mut self) {
        loop {
            if let Some(event) = self.event_queue.receive(Duration::infinite()) {
                self.process_event(event);
            }
        }
    }
}
```

---

## Summary

**State Machine Implementation in FreeRTOS** provides a structured approach to managing complex system behavior through:

### Key Concepts:
- **States**: Discrete system conditions with well-defined behavior
- **Events**: Triggers from queues, timers, or interrupts
- **Transitions**: Controlled movement between states based on events and guards
- **Actions**: Operations performed during transitions or within states

### Implementation Patterns:

1. **Simple FSM**: Switch-case based, suitable for straightforward control logic
2. **Hierarchical FSM**: Nested states with inheritance, reduces code duplication
3. **Event-Driven**: Queue-based event processing with FreeRTOS primitives
4. **Table-Driven**: Transition tables for complex state machines

### Benefits:
- **Clarity**: Explicit state representation makes code easier to understand
- **Maintainability**: Changes isolated to specific states
- **Testability**: Each state can be tested independently
- **Reliability**: Prevents invalid state combinations
- **Scalability**: Easy to add new states and transitions

### FreeRTOS Integration:
- **Tasks**: Each state machine runs as a dedicated task
- **Queues**: Event delivery mechanism
- **Timers**: Timeout and periodic events
- **Semaphores/Mutexes**: State synchronization across tasks
- **Event Groups**: Multiple condition-based transitions

### Best Practices:
- Keep state handlers simple and focused
- Use guards to validate transitions
- Implement entry/exit actions for state initialization/cleanup
- Consider using hierarchical state machines for complex systems
- Log state transitions for debugging
- Use enums for type-safe states and events
- Implement timeout handling for all states requiring it

State machines are fundamental to reliable embedded systems design, and FreeRTOS provides excellent support for implementing them efficiently and maintainably.