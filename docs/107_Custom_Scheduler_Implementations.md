# Custom Scheduler Implementations in FreeRTOS

## Overview

Custom scheduler implementations in FreeRTOS involve modifying or replacing the default preemptive priority-based scheduler to meet specialized real-time system requirements. This advanced topic requires deep understanding of kernel internals, timing requirements, and task management mechanisms.

## Core Concepts

### FreeRTOS Default Scheduler

FreeRTOS uses a **preemptive priority-based scheduler** with the following characteristics:

- **Fixed-priority preemptive scheduling**: Higher priority tasks preempt lower priority tasks
- **Time slicing**: Tasks of equal priority share CPU time via round-robin
- **Tick-based timing**: System tick interrupt drives scheduling decisions
- **O(1) scheduler**: Task selection time is constant regardless of task count

### When to Customize the Scheduler

Custom schedulers are needed for:

1. **Rate Monotonic Scheduling (RMS)**: Periodic tasks with deadline requirements
2. **Earliest Deadline First (EDF)**: Dynamic priority based on deadlines
3. **Mixed-criticality systems**: Safety-critical and non-critical tasks
4. **Energy-aware scheduling**: Power optimization in battery-powered devices
5. **Application-specific policies**: Domain-specific scheduling requirements

## Architecture and Internals

### Key Scheduler Components

```c
// Core scheduler data structures (simplified)
typedef struct tskTaskControlBlock {
    volatile StackType_t *pxTopOfStack;
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    UBaseType_t uxPriority;
    StackType_t *pxStack;
    char pcTaskName[configMAX_TASK_NAME_LEN];
    UBaseType_t uxCriticalNesting;
    // Additional fields...
} tskTCB;

// Ready task lists (one per priority level)
static List_t pxReadyTasksLists[configMAX_PRIORITIES];

// Currently running task
PRIVILEGED_DATA TCB_t * volatile pxCurrentTCB = NULL;
```

### Scheduler Critical Functions

1. **vTaskSwitchContext()**: Selects next task to run
2. **xTaskIncrementTick()**: Handles tick interrupt and unblocks tasks
3. **vTaskStartScheduler()**: Initializes and starts scheduler
4. **vTaskSuspendAll()/xTaskResumeAll()**: Scheduler suspension

## Programming Examples

### Example 1: Basic Scheduler Hook (C)

```c
// Custom task selection hook in FreeRTOSConfig.h
#define configUSE_APPLICATION_TASK_TAG 1

// Custom scheduler hook function
void vApplicationTaskSwitchHook(void) {
    // Called on every context switch
    TaskHandle_t xCurrentTask = xTaskGetCurrentTaskHandle();
    
    // Log task switches for analysis
    static uint32_t ulSwitchCount = 0;
    ulSwitchCount++;
    
    // Custom scheduling logic or monitoring
    // Example: Track CPU usage per task
}

// Implementing a simple load balancer for dual-core
typedef struct {
    TaskHandle_t xTask;
    uint8_t ucPreferredCore;
    uint32_t ulExecutionTime;
} TaskLoadInfo_t;

void vCustomLoadBalancer(void) {
    TaskLoadInfo_t xTaskInfo[MAX_TASKS];
    uint32_t ulCore0Load = 0, ulCore1Load = 0;
    
    // Analyze task execution times
    for (int i = 0; i < ulTaskCount; i++) {
        if (xTaskInfo[i].ucPreferredCore == 0) {
            ulCore0Load += xTaskInfo[i].ulExecutionTime;
        } else {
            ulCore1Load += xTaskInfo[i].ulExecutionTime;
        }
    }
    
    // Rebalance if load difference exceeds threshold
    if (abs(ulCore0Load - ulCore1Load) > LOAD_THRESHOLD) {
        // Migrate tasks between cores
        vTaskCoreAffinitySet(xTaskInfo[i].xTask, 
                             ulCore0Load > ulCore1Load ? 1 : 0);
    }
}
```

### Example 2: Earliest Deadline First (EDF) Scheduler (C)

```c
// EDF Task Control Block extension
typedef struct {
    TCB_t xBaseTCB;
    TickType_t xDeadline;       // Absolute deadline
    TickType_t xPeriod;         // Task period
    TickType_t xReleaseTime;    // Next release time
    BaseType_t xMissedDeadlines; // Deadline miss counter
} EDFTaskControlBlock_t;

// Custom EDF ready list (sorted by deadline)
static List_t xEDFReadyList;

// Modified task selection for EDF
void vTaskSwitchContext_EDF(void) {
    // Disable interrupts
    taskENTER_CRITICAL();
    
    // Get task with earliest deadline
    ListItem_t *pxEarliestDeadlineItem = listGET_HEAD_ENTRY(&xEDFReadyList);
    EDFTaskControlBlock_t *pxNextTask = 
        (EDFTaskControlBlock_t *)listGET_LIST_ITEM_OWNER(pxEarliestDeadlineItem);
    
    // Check for deadline violations
    TickType_t xCurrentTime = xTaskGetTickCount();
    if (pxNextTask->xDeadline < xCurrentTime) {
        pxNextTask->xMissedDeadlines++;
        // Handle deadline miss (logging, recovery, etc.)
        vHandleDeadlineMiss(pxNextTask);
    }
    
    // Update current TCB
    pxCurrentTCB = (TCB_t *)pxNextTask;
    
    taskEXIT_CRITICAL();
}

// Periodic task release function
void vPeriodicTaskRelease(EDFTaskControlBlock_t *pxTask) {
    TickType_t xCurrentTime = xTaskGetTickCount();
    
    // Calculate next deadline
    pxTask->xReleaseTime = xCurrentTime;
    pxTask->xDeadline = xCurrentTime + pxTask->xPeriod;
    
    // Insert into EDF ready list (sorted by deadline)
    vListInsertSorted(&xEDFReadyList, &pxTask->xBaseTCB.xStateListItem,
                      pxTask->xDeadline);
}

// Helper function to insert task sorted by deadline
void vListInsertSorted(List_t *pxList, ListItem_t *pxNewItem, 
                       TickType_t xDeadline) {
    ListItem_t *pxIterator;
    const TickType_t xValueOfInsertion = xDeadline;
    
    // Find insertion point
    for (pxIterator = (ListItem_t *)&(pxList->xListEnd);
         pxIterator->pxNext->xItemValue <= xValueOfInsertion;
         pxIterator = pxIterator->pxNext) {
        // Empty loop body
    }
    
    // Insert at correct position
    pxNewItem->xItemValue = xValueOfInsertion;
    pxNewItem->pxNext = pxIterator->pxNext;
    pxIterator->pxNext->pxPrevious = pxNewItem;
    pxNewItem->pxPrevious = pxIterator;
    pxIterator->pxNext = pxNewItem;
    
    pxList->uxNumberOfItems++;
}
```

### Example 3: Rate Monotonic Scheduler (C++)

```cpp
// Rate Monotonic Scheduling implementation
class RMSTask {
private:
    TaskHandle_t handle;
    uint32_t period_ms;
    uint32_t execution_time_ms;
    uint32_t deadline_ms;
    TickType_t last_wake_time;
    
public:
    RMSTask(const char* name, uint32_t period, uint32_t exec_time, 
            uint32_t stack_size, UBaseType_t priority)
        : period_ms(period), execution_time_ms(exec_time), 
          deadline_ms(period), last_wake_time(0) {
        
        // Create task with priority inversely proportional to period
        // (shorter period = higher priority in RMS)
        xTaskCreate(taskFunction, name, stack_size, this, priority, &handle);
    }
    
    static void taskFunction(void* params) {
        RMSTask* task = static_cast<RMSTask*>(params);
        task->last_wake_time = xTaskGetTickCount();
        
        while (true) {
            TickType_t start_time = xTaskGetTickCount();
            
            // Execute periodic work
            task->periodicWork();
            
            // Check if deadline was met
            TickType_t end_time = xTaskGetTickCount();
            uint32_t execution_time = (end_time - start_time) * portTICK_PERIOD_MS;
            
            if (execution_time > task->deadline_ms) {
                // Deadline missed - handle error
                task->handleDeadlineMiss(execution_time);
            }
            
            // Wait for next period
            vTaskDelayUntil(&task->last_wake_time, 
                           pdMS_TO_TICKS(task->period_ms));
        }
    }
    
    virtual void periodicWork() = 0; // To be implemented by derived classes
    
    void handleDeadlineMiss(uint32_t actual_time) {
        // Log deadline violation
        printf("Task deadline missed! Expected: %lu ms, Actual: %lu ms\n",
               deadline_ms, actual_time);
    }
    
    // RMS schedulability test (Liu & Layland bound)
    static bool isSchedulable(std::vector<RMSTask*>& tasks) {
        size_t n = tasks.size();
        double utilization = 0.0;
        
        for (auto* task : tasks) {
            utilization += static_cast<double>(task->execution_time_ms) / 
                          task->period_ms;
        }
        
        // RMS schedulability bound: U <= n(2^(1/n) - 1)
        double bound = n * (pow(2.0, 1.0/n) - 1.0);
        return utilization <= bound;
    }
};

// Example usage
class SensorTask : public RMSTask {
public:
    SensorTask() : RMSTask("Sensor", 100, 20, 2048, 3) {}
    
    void periodicWork() override {
        // Read sensor data every 100ms
        readSensorData();
    }
    
private:
    void readSensorData() {
        // Sensor reading logic
    }
};

class ControlTask : public RMSTask {
public:
    ControlTask() : RMSTask("Control", 50, 15, 2048, 4) {}
    
    void periodicWork() override {
        // Control loop every 50ms
        updateController();
    }
    
private:
    void updateController() {
        // Control algorithm
    }
};
```

### Example 4: Custom Scheduler in Rust

```rust
// Rust implementation using FreeRTOS bindings
use freertos_rust::{Task, Duration, CurrentTask};
use core::sync::atomic::{AtomicU32, Ordering};

// Custom scheduler for energy-aware task management
pub struct EnergyAwareScheduler {
    high_power_tasks: Vec<TaskHandle>,
    low_power_tasks: Vec<TaskHandle>,
    battery_level: AtomicU32,
    power_mode: AtomicU32,
}

impl EnergyAwareScheduler {
    pub fn new() -> Self {
        Self {
            high_power_tasks: Vec::new(),
            low_power_tasks: Vec::new(),
            battery_level: AtomicU32::new(100),
            power_mode: AtomicU32::new(0), // 0 = normal, 1 = power save
        }
    }
    
    pub fn register_task(&mut self, handle: TaskHandle, is_high_power: bool) {
        if is_high_power {
            self.high_power_tasks.push(handle);
        } else {
            self.low_power_tasks.push(handle);
        }
    }
    
    pub fn update_power_mode(&self) {
        let battery = self.battery_level.load(Ordering::Relaxed);
        
        if battery < 20 {
            // Enter power save mode
            self.power_mode.store(1, Ordering::Relaxed);
            self.suspend_high_power_tasks();
        } else if battery > 30 && self.power_mode.load(Ordering::Relaxed) == 1 {
            // Exit power save mode
            self.power_mode.store(0, Ordering::Relaxed);
            self.resume_high_power_tasks();
        }
    }
    
    fn suspend_high_power_tasks(&self) {
        for task in &self.high_power_tasks {
            unsafe {
                freertos_sys::vTaskSuspend(task.0);
            }
        }
    }
    
    fn resume_high_power_tasks(&self) {
        for task in &self.high_power_tasks {
            unsafe {
                freertos_sys::vTaskResume(task.0);
            }
        }
    }
}

// Wrapper for task handle
pub struct TaskHandle(*mut freertos_sys::tskTaskControlBlock);

// EDF scheduler implementation in Rust
pub struct EdfTask {
    handle: Option<Task>,
    period: Duration,
    deadline: Duration,
    next_release: u32,
}

impl EdfTask {
    pub fn new(name: &str, period: Duration, deadline: Duration, 
               priority: u8, stack_size: u16) -> Self {
        let task = Task::new()
            .name(name)
            .priority(priority.into())
            .stack_size(stack_size)
            .start(move || {
                Self::task_loop(period, deadline);
            })
            .unwrap();
            
        Self {
            handle: Some(task),
            period,
            deadline,
            next_release: 0,
        }
    }
    
    fn task_loop(period: Duration, deadline: Duration) {
        let mut last_wake = CurrentTask::get_tick_count();
        
        loop {
            let start = CurrentTask::get_tick_count();
            
            // Execute task work
            Self::do_work();
            
            // Check deadline
            let end = CurrentTask::get_tick_count();
            if end - start > deadline.as_ticks() {
                // Deadline missed
                Self::handle_deadline_miss();
            }
            
            // Wait for next period
            CurrentTask::delay_until(&mut last_wake, period);
        }
    }
    
    fn do_work() {
        // Task-specific work
    }
    
    fn handle_deadline_miss() {
        // Handle deadline violation
        println!("Deadline missed!");
    }
}

// Priority inheritance protocol implementation
pub struct PriorityInheritanceMutex {
    mutex: freertos_rust::Mutex<()>,
    owner_priority: AtomicU32,
    original_priority: AtomicU32,
}

impl PriorityInheritanceMutex {
    pub fn new() -> Self {
        Self {
            mutex: freertos_rust::Mutex::new(()).unwrap(),
            owner_priority: AtomicU32::new(0),
            original_priority: AtomicU32::new(0),
        }
    }
    
    pub fn lock(&self) -> Result<freertos_rust::MutexGuard<()>, ()> {
        // Get current task priority
        let current_priority = unsafe {
            freertos_sys::uxTaskPriorityGet(core::ptr::null_mut())
        };
        
        // Implement priority inheritance
        let owner_prio = self.owner_priority.load(Ordering::Acquire);
        if current_priority > owner_prio {
            // Raise owner's priority
            self.boost_owner_priority(current_priority);
        }
        
        self.mutex.lock(Duration::infinite())
    }
    
    fn boost_owner_priority(&self, new_priority: u32) {
        unsafe {
            // Boost mutex owner's priority
            freertos_sys::vTaskPrioritySet(core::ptr::null_mut(), new_priority);
        }
    }
}

// Example usage
pub fn setup_custom_scheduler() {
    let mut scheduler = EnergyAwareScheduler::new();
    
    // Create high-power task
    let high_power = Task::new()
        .name("HighPower")
        .priority(2.into())
        .stack_size(2048)
        .start(|| {
            loop {
                // Heavy processing
                CurrentTask::delay(Duration::ms(100));
            }
        })
        .unwrap();
    
    // Create low-power task
    let low_power = Task::new()
        .name("LowPower")
        .priority(1.into())
        .stack_size(1024)
        .start(|| {
            loop {
                // Lightweight monitoring
                CurrentTask::delay(Duration::ms(1000));
            }
        })
        .unwrap();
    
    // Monitor battery and adjust scheduling
    Task::new()
        .name("PowerMonitor")
        .priority(3.into())
        .stack_size(1024)
        .start(move || {
            loop {
                scheduler.update_power_mode();
                CurrentTask::delay(Duration::ms(5000));
            }
        })
        .unwrap();
}
```

### Example 5: Mixed-Criticality Scheduler (C)

```c
// Mixed-criticality system implementation
typedef enum {
    CRITICALITY_LOW = 0,
    CRITICALITY_MEDIUM = 1,
    CRITICALITY_HIGH = 2,
    CRITICALITY_SAFETY = 3
} TaskCriticality_t;

typedef struct {
    TCB_t xBaseTCB;
    TaskCriticality_t xCriticality;
    TickType_t xWCET_Low;      // Worst-case execution time (low mode)
    TickType_t xWCET_High;     // Worst-case execution time (high mode)
    BaseType_t xBudgetExceeded;
} CriticalTaskControlBlock_t;

// System criticality mode
static volatile TaskCriticality_t xSystemCriticalityMode = CRITICALITY_LOW;

// Mode change handler
void vHandleModeChange(TaskCriticality_t xNewMode) {
    taskENTER_CRITICAL();
    
    xSystemCriticalityMode = xNewMode;
    
    // Suspend tasks below current criticality level
    TaskHandle_t xHandle = NULL;
    do {
        xHandle = xTaskGetNext(xHandle);
        if (xHandle != NULL) {
            CriticalTaskControlBlock_t *pxTask = 
                (CriticalTaskControlBlock_t *)xHandle;
            
            if (pxTask->xCriticality < xNewMode) {
                vTaskSuspend(xHandle);
            } else if (eTaskGetState(xHandle) == eSuspended) {
                vTaskResume(xHandle);
            }
        }
    } while (xHandle != NULL);
    
    taskEXIT_CRITICAL();
}

// Budget monitoring
void vTaskBudgetMonitor(void *pvParameters) {
    CriticalTaskControlBlock_t *pxTask = 
        (CriticalTaskControlBlock_t *)pvParameters;
    TickType_t xStartTime, xEndTime, xExecutionTime;
    
    while (1) {
        xStartTime = xTaskGetTickCount();
        
        // Execute task work
        pxTask->xBaseTCB.pxTaskCode(pxTask->xBaseTCB.pvParameters);
        
        xEndTime = xTaskGetTickCount();
        xExecutionTime = xEndTime - xStartTime;
        
        // Check budget violation
        TickType_t xBudget = (xSystemCriticalityMode == CRITICALITY_LOW) ?
                             pxTask->xWCET_Low : pxTask->xWCET_High;
        
        if (xExecutionTime > xBudget) {
            pxTask->xBudgetExceeded = pdTRUE;
            // Trigger mode change if necessary
            if (pxTask->xCriticality >= CRITICALITY_HIGH) {
                vHandleModeChange(CRITICALITY_HIGH);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## Summary

**Custom scheduler implementations** in FreeRTOS enable specialized real-time behavior beyond the default priority-based scheduling:

**Key Capabilities:**
- **Alternative algorithms**: EDF, RMS, mixed-criticality, energy-aware scheduling
- **Kernel modification**: Direct manipulation of task lists and context switching
- **Application-specific policies**: Domain-tailored scheduling strategies
- **Advanced features**: Priority inheritance, deadline monitoring, budget enforcement

**Implementation Approaches:**
1. **Hook functions**: Non-invasive monitoring and minor adjustments
2. **Modified task selection**: Replacing vTaskSwitchContext() logic
3. **Extended TCB**: Adding scheduling metadata to task control blocks
4. **Hybrid schedulers**: Combining multiple scheduling policies

**Critical Considerations:**
- **Timing guarantees**: Maintain real-time properties and schedulability
- **Overhead**: Custom schedulers must remain deterministic and efficient
- **Testing**: Extensive validation required for safety-critical systems
- **Portability**: Platform-specific optimizations may be needed

**Use Cases:**
- Safety-critical systems (automotive, aerospace, medical)
- Energy-constrained IoT devices
- Hard real-time control systems
- Mixed-criticality embedded platforms

Custom schedulers provide powerful capabilities but require deep kernel knowledge and careful implementation to ensure system correctness and timing guarantees.