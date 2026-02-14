# 111. Priority Inversion Scenarios

## Detailed Description

**Priority inversion** is a critical scheduling problem in real-time operating systems where a high-priority task is indirectly preempted by a lower-priority task, violating the intended priority scheme and potentially causing deadline misses or system instability.

### What is Priority Inversion?

Priority inversion occurs when:
1. A low-priority task (L) acquires a shared resource (mutex/semaphore)
2. A high-priority task (H) attempts to acquire the same resource and blocks
3. A medium-priority task (M) preempts the low-priority task
4. The high-priority task must wait for both M and L to complete, effectively executing at lower priority

This scenario is problematic because task H, despite having the highest priority, cannot proceed until L releases the resource, and L cannot execute while M is running.

### Classic Example: Mars Pathfinder Incident

The most famous real-world example occurred in 1997 when NASA's Mars Pathfinder experienced system resets due to priority inversion between its information bus management task, meteorological data gathering task, and communications task.

### Types of Priority Inversion

**1. Bounded Priority Inversion**
- Duration is limited to the critical section length of the low-priority task
- Occurs when only direct resource contention exists
- Manageable with proper mutex design

**2. Unbounded Priority Inversion**
- Duration depends on execution time of unrelated medium-priority tasks
- Can extend indefinitely in worst case
- Requires mitigation strategies

### Mitigation Strategies

**1. Priority Inheritance Protocol (PIP)**
- Low-priority task temporarily inherits the priority of the highest-priority task waiting for the resource
- Built into FreeRTOS mutexes
- Simple to implement but can lead to chained blocking

**2. Priority Ceiling Protocol (PCP)**
- Each mutex has a priority ceiling equal to the highest priority of any task that may lock it
- Task holding the mutex temporarily runs at the ceiling priority
- Prevents priority inversion entirely but requires advance knowledge of task priorities

**3. Immediate Priority Ceiling Protocol (IPCP)**
- Task immediately assumes ceiling priority upon acquiring the mutex
- More predictable but potentially wasteful of CPU time

**4. Disable Preemption**
- Disabling task switching during critical sections
- Simple but impacts system responsiveness
- Only viable for very short critical sections

**5. Resource Design Strategies**
- Minimizing shared resources
- Reducing critical section duration
- Using lock-free data structures
- Task redesign to avoid resource sharing

## Programming Examples

### C/C++ with FreeRTOS

#### Example 1: Demonstrating Priority Inversion

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

// Shared resource protected by semaphore (NOT mutex - no priority inheritance)
SemaphoreHandle_t xResourceSemaphore;
volatile uint32_t sharedCounter = 0;

// Low priority task (Priority 1)
void vLowPriorityTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        printf("[LOW] Attempting to acquire resource...\n");
        
        if(xSemaphoreTake(xResourceSemaphore, portMAX_DELAY) == pdTRUE)
        {
            printf("[LOW] Acquired resource, starting long operation\n");
            
            // Simulate long critical section (50ms)
            for(volatile uint32_t i = 0; i < 1000000; i++)
            {
                sharedCounter++;
            }
            
            printf("[LOW] Releasing resource\n");
            xSemaphoreGive(xResourceSemaphore);
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
    }
}

// Medium priority task (Priority 2) - doesn't use the resource
void vMediumPriorityTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        printf("[MEDIUM] Running intensive computation (blocks LOW task)\n");
        
        // Simulate CPU-intensive work (30ms)
        for(volatile uint32_t i = 0; i < 600000; i++)
        {
            // Busy work
        }
        
        printf("[MEDIUM] Computation complete\n");
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}

// High priority task (Priority 3)
void vHighPriorityTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    vTaskDelay(pdMS_TO_TICKS(10)); // Let low priority task acquire resource first
    
    for(;;)
    {
        printf("[HIGH] *** URGENT: Need resource NOW! ***\n");
        TickType_t xStartTime = xTaskGetTickCount();
        
        if(xSemaphoreTake(xResourceSemaphore, portMAX_DELAY) == pdTRUE)
        {
            TickType_t xWaitTime = xTaskGetTickCount() - xStartTime;
            printf("[HIGH] Finally got resource after %lu ms (PRIORITY INVERSION!)\n", 
                   xWaitTime * portTICK_PERIOD_MS);
            
            // Quick critical section
            sharedCounter += 100;
            
            xSemaphoreGive(xResourceSemaphore);
            printf("[HIGH] Done\n");
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(150));
    }
}

void setup_priority_inversion_demo(void)
{
    // Using binary semaphore (NO priority inheritance)
    xResourceSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xResourceSemaphore);
    
    xTaskCreate(vLowPriorityTask, "LOW", 1000, NULL, 1, NULL);
    xTaskCreate(vMediumPriorityTask, "MEDIUM", 1000, NULL, 2, NULL);
    xTaskCreate(vHighPriorityTask, "HIGH", 1000, NULL, 3, NULL);
}
```

#### Example 2: Mitigation with Priority Inheritance (Mutex)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdio.h>

// Using mutex with priority inheritance
SemaphoreHandle_t xResourceMutex;
volatile uint32_t sharedData = 0;

// Task structure for tracking
typedef struct {
    const char *name;
    UBaseType_t priority;
} TaskInfo_t;

void vTaskWithMutex(void *pvParameters)
{
    TaskInfo_t *info = (TaskInfo_t *)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        printf("[%s] (Priority %u) Attempting to acquire mutex...\n", 
               info->name, info->priority);
        
        TickType_t xStartTime = xTaskGetTickCount();
        
        if(xSemaphoreTake(xResourceMutex, portMAX_DELAY) == pdTRUE)
        {
            TickType_t xWaitTime = xTaskGetTickCount() - xStartTime;
            UBaseType_t currentPriority = uxTaskPriorityGet(NULL);
            
            printf("[%s] Acquired mutex (waited %lu ms, running at priority %u)\n",
                   info->name, xWaitTime * portTICK_PERIOD_MS, currentPriority);
            
            // Critical section - duration varies by task
            uint32_t iterations = (info->priority == 1) ? 1000000 : 10000;
            for(volatile uint32_t i = 0; i < iterations; i++)
            {
                sharedData++;
            }
            
            printf("[%s] Releasing mutex\n", info->name);
            xSemaphoreGive(xResourceMutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
    }
}

void vCpuIntensiveTask(void *pvParameters)
{
    for(;;)
    {
        printf("[COMPUTE] Running (doesn't need mutex)\n");
        
        for(volatile uint32_t i = 0; i < 500000; i++)
        {
            // CPU work
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup_priority_inheritance_demo(void)
{
    // Create mutex (has priority inheritance built-in)
    xResourceMutex = xSemaphoreCreateMutex();
    
    static TaskInfo_t lowInfo = {"LOW", 1};
    static TaskInfo_t medInfo = {"MEDIUM", 2};
    static TaskInfo_t highInfo = {"HIGH", 3};
    
    xTaskCreate(vTaskWithMutex, "LOW", 1000, &lowInfo, 1, NULL);
    xTaskCreate(vCpuIntensiveTask, "COMPUTE", 1000, NULL, 2, NULL);
    xTaskCreate(vTaskWithMutex, "HIGH", 1000, &highInfo, 3, NULL);
    
    printf("Priority Inheritance Demo Started\n");
    printf("LOW task will inherit HIGH priority when HIGH blocks on mutex\n");
}
```

#### Example 3: Priority Ceiling Protocol Implementation

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Priority Ceiling Mutex structure
typedef struct {
    SemaphoreHandle_t mutex;
    UBaseType_t ceilingPriority;
    UBaseType_t originalPriority;
    TaskHandle_t ownerTask;
} PriorityCeilingMutex_t;

BaseType_t xPCMutexCreate(PriorityCeilingMutex_t *pcMutex, UBaseType_t ceiling)
{
    pcMutex->mutex = xSemaphoreCreateMutex();
    if(pcMutex->mutex == NULL)
    {
        return pdFAIL;
    }
    
    pcMutex->ceilingPriority = ceiling;
    pcMutex->originalPriority = 0;
    pcMutex->ownerTask = NULL;
    
    return pdPASS;
}

BaseType_t xPCMutexTake(PriorityCeilingMutex_t *pcMutex, TickType_t xBlockTime)
{
    if(xSemaphoreTake(pcMutex->mutex, xBlockTime) == pdTRUE)
    {
        // Save original priority and raise to ceiling
        TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
        pcMutex->ownerTask = currentTask;
        pcMutex->originalPriority = uxTaskPriorityGet(currentTask);
        
        if(pcMutex->originalPriority < pcMutex->ceilingPriority)
        {
            vTaskPrioritySet(currentTask, pcMutex->ceilingPriority);
            printf("Task elevated from %u to ceiling %u\n",
                   pcMutex->originalPriority, pcMutex->ceilingPriority);
        }
        
        return pdTRUE;
    }
    
    return pdFALSE;
}

BaseType_t xPCMutexGive(PriorityCeilingMutex_t *pcMutex)
{
    // Restore original priority
    if(pcMutex->ownerTask != NULL && 
       pcMutex->originalPriority < pcMutex->ceilingPriority)
    {
        vTaskPrioritySet(pcMutex->ownerTask, pcMutex->originalPriority);
        printf("Task restored to original priority %u\n", 
               pcMutex->originalPriority);
    }
    
    pcMutex->ownerTask = NULL;
    return xSemaphoreGive(pcMutex->mutex);
}

// Usage example
PriorityCeilingMutex_t sharedResourceMutex;

void vTaskUsingPCMutex(void *pvParameters)
{
    UBaseType_t taskPriority = (UBaseType_t)pvParameters;
    
    for(;;)
    {
        if(xPCMutexTake(&sharedResourceMutex, portMAX_DELAY) == pdTRUE)
        {
            printf("Task (priority %u) in critical section\n", taskPriority);
            
            // Critical section work
            vTaskDelay(pdMS_TO_TICKS(50));
            
            xPCMutexGive(&sharedResourceMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void setup_priority_ceiling_demo(void)
{
    // Create PC mutex with ceiling of 4 (highest possible task priority)
    xPCMutexCreate(&sharedResourceMutex, 4);
    
    xTaskCreate(vTaskUsingPCMutex, "Task1", 1000, (void *)1, 1, NULL);
    xTaskCreate(vTaskUsingPCMutex, "Task2", 1000, (void *)2, 2, NULL);
    xTaskCreate(vTaskUsingPCMutex, "Task3", 1000, (void *)3, 3, NULL);
}
```

#### Example 4: Detecting Priority Inversion

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Priority inversion monitoring
typedef struct {
    TaskHandle_t blockedTask;
    TaskHandle_t blockingTask;
    TickType_t startTime;
    UBaseType_t blockedPriority;
    UBaseType_t blockingPriority;
} PriorityInversionEvent_t;

#define MAX_INVERSION_EVENTS 10
PriorityInversionEvent_t inversionLog[MAX_INVERSION_EVENTS];
uint32_t inversionCount = 0;

// Wrapper to detect priority inversion
BaseType_t xSemaphoreTakeMonitored(SemaphoreHandle_t xSemaphore, 
                                    TickType_t xBlockTime,
                                    const char *taskName)
{
    TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
    UBaseType_t currentPriority = uxTaskPriorityGet(currentTask);
    TickType_t startTime = xTaskGetTickCount();
    
    BaseType_t result = xSemaphoreTake(xSemaphore, xBlockTime);
    
    if(result == pdTRUE)
    {
        TickType_t waitTime = xTaskGetTickCount() - startTime;
        
        // If we waited longer than expected, check for priority inversion
        if(waitTime > pdMS_TO_TICKS(10))
        {
            // Log potential priority inversion event
            if(inversionCount < MAX_INVERSION_EVENTS)
            {
                inversionLog[inversionCount].blockedTask = currentTask;
                inversionLog[inversionCount].startTime = startTime;
                inversionLog[inversionCount].blockedPriority = currentPriority;
                
                printf("*** PRIORITY INVERSION DETECTED ***\n");
                printf("    Task: %s (Priority %u)\n", taskName, currentPriority);
                printf("    Wait time: %lu ms\n", waitTime * portTICK_PERIOD_MS);
                
                inversionCount++;
            }
        }
    }
    
    return result;
}

void vMonitorTask(void *pvParameters)
{
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        if(inversionCount > 0)
        {
            printf("\n=== Priority Inversion Report ===\n");
            printf("Total inversions detected: %lu\n", inversionCount);
            printf("================================\n\n");
        }
    }
}
```

### Rust with FreeRTOS Bindings

```rust
use freertos_rust::{Task, Duration, Mutex, Semaphore, CurrentTask};
use core::cell::RefCell;

// Shared resource
struct SharedResource {
    counter: u32,
    data: [u8; 1024],
}

impl SharedResource {
    fn new() -> Self {
        SharedResource {
            counter: 0,
            data: [0; 1024],
        }
    }
    
    fn process(&mut self, iterations: u32) {
        for _ in 0..iterations {
            self.counter = self.counter.wrapping_add(1);
        }
    }
}

// Example 1: Priority Inversion Demonstration with Binary Semaphore
fn demonstrate_priority_inversion() {
    let resource = RefCell::new(SharedResource::new());
    let semaphore = Semaphore::new_binary().unwrap();
    semaphore.give();
    
    // Low priority task
    Task::new()
        .name("LowPriority")
        .priority(TaskPriority::from(1))
        .start(move || {
            loop {
                println!("[LOW] Attempting to acquire semaphore...");
                
                if semaphore.take(Duration::infinite()).is_ok() {
                    println!("[LOW] Acquired! Starting long operation...");
                    
                    let mut res = resource.borrow_mut();
                    res.process(1_000_000); // Long operation
                    
                    println!("[LOW] Releasing semaphore");
                    semaphore.give();
                }
                
                CurrentTask::delay(Duration::ms(200));
            }
        })
        .unwrap();
    
    // Medium priority task (CPU-bound, doesn't need resource)
    Task::new()
        .name("MediumPriority")
        .priority(TaskPriority::from(2))
        .start(|| {
            loop {
                println!("[MEDIUM] Running intensive computation...");
                
                // Simulate CPU work
                let mut dummy = 0u32;
                for _ in 0..600_000 {
                    dummy = dummy.wrapping_add(1);
                }
                
                println!("[MEDIUM] Computation complete");
                CurrentTask::delay(Duration::ms(100));
            }
        })
        .unwrap();
    
    // High priority task
    Task::new()
        .name("HighPriority")
        .priority(TaskPriority::from(3))
        .start(move || {
            CurrentTask::delay(Duration::ms(10)); // Let low priority start first
            
            loop {
                println!("[HIGH] *** URGENT: Need resource NOW! ***");
                let start_time = get_tick_count();
                
                if semaphore.take(Duration::infinite()).is_ok() {
                    let wait_time = get_tick_count() - start_time;
                    println!(
                        "[HIGH] Finally got resource after {} ms (PRIORITY INVERSION!)",
                        wait_time
                    );
                    
                    let mut res = resource.borrow_mut();
                    res.counter += 100; // Quick operation
                    
                    semaphore.give();
                    println!("[HIGH] Done");
                }
                
                CurrentTask::delay(Duration::ms(150));
            }
        })
        .unwrap();
}

// Example 2: Priority Inheritance with Mutex
fn demonstrate_priority_inheritance() {
    let resource = Mutex::new(SharedResource::new()).unwrap();
    
    // Helper function for tasks using mutex
    fn task_with_mutex(
        name: &'static str,
        priority: u8,
        mutex: &Mutex<SharedResource>,
        iterations: u32,
    ) {
        loop {
            println!("[{}] (Priority {}) Attempting to acquire mutex...", name, priority);
            let start_time = get_tick_count();
            
            match mutex.lock(Duration::infinite()) {
                Ok(mut guard) => {
                    let wait_time = get_tick_count() - start_time;
                    let current_priority = CurrentTask::get_priority();
                    
                    println!(
                        "[{}] Acquired mutex (waited {} ms, running at priority {})",
                        name, wait_time, current_priority
                    );
                    
                    guard.process(iterations);
                    
                    println!("[{}] Releasing mutex", name);
                    // Automatic release when guard drops
                }
                Err(e) => println!("[{}] Failed to acquire: {:?}", name, e),
            }
            
            CurrentTask::delay(Duration::ms(200));
        }
    }
    
    // Create tasks
    let resource_clone1 = resource.clone();
    Task::new()
        .name("LowPriority")
        .priority(TaskPriority::from(1))
        .start(move || task_with_mutex("LOW", 1, &resource_clone1, 1_000_000))
        .unwrap();
    
    Task::new()
        .name("Compute")
        .priority(TaskPriority::from(2))
        .start(|| {
            loop {
                println!("[COMPUTE] Running (doesn't need mutex)");
                let mut dummy = 0u32;
                for _ in 0..500_000 {
                    dummy = dummy.wrapping_add(1);
                }
                CurrentTask::delay(Duration::ms(100));
            }
        })
        .unwrap();
    
    let resource_clone2 = resource.clone();
    Task::new()
        .name("HighPriority")
        .priority(TaskPriority::from(3))
        .start(move || task_with_mutex("HIGH", 3, &resource_clone2, 10_000))
        .unwrap();
    
    println!("Priority Inheritance Demo Started");
    println!("LOW task will inherit HIGH priority when HIGH blocks on mutex");
}

// Example 3: Priority Ceiling Protocol in Rust
struct PriorityCeilingMutex<T> {
    inner: Mutex<T>,
    ceiling_priority: u8,
}

impl<T> PriorityCeilingMutex<T> {
    fn new(data: T, ceiling: u8) -> Result<Self, FreeRtosError> {
        Ok(PriorityCeilingMutex {
            inner: Mutex::new(data)?,
            ceiling_priority: ceiling,
        })
    }
    
    fn lock(&self, timeout: Duration) -> Result<PriorityCeilingGuard<T>, FreeRtosError> {
        let original_priority = CurrentTask::get_priority();
        
        // Raise priority to ceiling before acquiring lock
        if original_priority < self.ceiling_priority {
            CurrentTask::set_priority(TaskPriority::from(self.ceiling_priority));
            println!(
                "Task elevated from {} to ceiling {}",
                original_priority, self.ceiling_priority
            );
        }
        
        match self.inner.lock(timeout) {
            Ok(guard) => Ok(PriorityCeilingGuard {
                guard,
                original_priority,
                ceiling_priority: self.ceiling_priority,
            }),
            Err(e) => {
                // Restore priority on failure
                if original_priority < self.ceiling_priority {
                    CurrentTask::set_priority(TaskPriority::from(original_priority));
                }
                Err(e)
            }
        }
    }
}

struct PriorityCeilingGuard<'a, T> {
    guard: MutexGuard<'a, T>,
    original_priority: u8,
    ceiling_priority: u8,
}

impl<'a, T> Drop for PriorityCeilingGuard<'a, T> {
    fn drop(&mut self) {
        // Restore original priority when guard is dropped
        if self.original_priority < self.ceiling_priority {
            CurrentTask::set_priority(TaskPriority::from(self.original_priority));
            println!(
                "Task restored to original priority {}",
                self.original_priority
            );
        }
    }
}

impl<'a, T> core::ops::Deref for PriorityCeilingGuard<'a, T> {
    type Target = T;
    
    fn deref(&self) -> &Self::Target {
        &self.guard
    }
}

impl<'a, T> core::ops::DerefMut for PriorityCeilingGuard<'a, T> {
    fn deref_mut(&mut self) -> &mut Self::Target {
        &mut self.guard
    }
}

// Usage example
fn demonstrate_priority_ceiling() {
    let pc_mutex = PriorityCeilingMutex::new(SharedResource::new(), 4)
        .expect("Failed to create PC mutex");
    
    for priority in 1..=3 {
        let mutex_ref = &pc_mutex; // Share reference
        
        Task::new()
            .name(&format!("Task{}", priority))
            .priority(TaskPriority::from(priority))
            .start(move || {
                loop {
                    match mutex_ref.lock(Duration::infinite()) {
                        Ok(mut guard) => {
                            println!(
                                "Task (priority {}) in critical section",
                                priority
                            );
                            guard.process(10_000);
                        }
                        Err(e) => println!("Lock failed: {:?}", e),
                    }
                    
                    CurrentTask::delay(Duration::ms(100));
                }
            })
            .unwrap();
    }
}

// Helper function (pseudo-code, actual implementation depends on bindings)
fn get_tick_count() -> u32 {
    // Returns current tick count
    unsafe { freertos_sys::xTaskGetTickCount() }
}
```

## Summary

**Priority inversion** is a critical real-time systems problem where high-priority tasks are delayed by lower-priority tasks through resource contention. Understanding and mitigating priority inversion is essential for building reliable real-time systems.

**Key Takeaways:**

1. **Problem Recognition**: Priority inversion occurs when a high-priority task blocks on a resource held by a low-priority task, and medium-priority tasks prevent the low-priority task from releasing the resource.

2. **Types**: Bounded inversion (limited to critical section duration) is manageable, while unbounded inversion (affected by unrelated tasks) can cause severe deadline misses.

3. **Mitigation Strategies**:
   - **Priority Inheritance**: Low-priority task inherits high-priority when blocking occurs (FreeRTOS mutexes use this)
   - **Priority Ceiling**: Tasks run at maximum priority of any resource they hold
   - **Design Solutions**: Minimize shared resources, reduce critical section time, use lock-free structures

4. **FreeRTOS Implementation**: Use mutexes (not binary semaphores) for automatic priority inheritance, or implement custom priority ceiling protocols for deterministic behavior.

5. **Best Practices**:
   - Always use mutexes for shared resource protection (they have built-in priority inheritance)
   - Keep critical sections as short as possible
   - Monitor and log blocking times to detect priority inversion
   - Consider task design to minimize resource sharing
   - For safety-critical systems, implement priority ceiling protocol

6. **Trade-offs**: Priority inheritance is simple but can lead to chained blocking; priority ceiling is more deterministic but requires knowing all task priorities in advance and may waste CPU time.

Understanding priority inversion scenarios and their solutions is crucial for developing robust, predictable real-time systems that meet their timing requirements.