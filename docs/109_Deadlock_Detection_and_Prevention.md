# Deadlock Detection and Prevention in FreeRTOS

## Overview

Deadlock is a situation where two or more tasks are permanently blocked, each waiting for resources held by the other(s), creating a circular dependency that prevents any task from proceeding. In real-time systems like FreeRTOS, deadlocks can be catastrophic, causing system freezes and mission-critical failures.

## Understanding Deadlock

### The Four Necessary Conditions (Coffman Conditions)

For a deadlock to occur, all four conditions must be present simultaneously:

1. **Mutual Exclusion**: Resources cannot be shared (e.g., mutexes)
2. **Hold and Wait**: Tasks hold resources while waiting for others
3. **No Preemption**: Resources cannot be forcibly taken from tasks
4. **Circular Wait**: A circular chain of tasks waiting for resources

Breaking any one of these conditions prevents deadlock.

## Detection Strategies

### 1. Circular Wait Detection

The most common deadlock scenario involves circular dependencies in lock acquisition.

### 2. Timeout-Based Detection

Using timeouts on mutex/semaphore acquisition can detect potential deadlocks, though it doesn't prevent them entirely.

---

## C/C++ Implementation Examples

### Example 1: Deadlock Scenario (What NOT to Do)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

SemaphoreHandle_t mutexA;
SemaphoreHandle_t mutexB;

// Task 1: Acquires A, then B
void vTask1(void *pvParameters)
{
    while(1)
    {
        // Acquire mutex A
        xSemaphoreTake(mutexA, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10)); // Simulating work
        
        // Try to acquire mutex B - DEADLOCK RISK!
        xSemaphoreTake(mutexB, portMAX_DELAY);
        
        // Critical section
        printf("Task 1 executing\n");
        
        xSemaphoreGive(mutexB);
        xSemaphoreGive(mutexA);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task 2: Acquires B, then A (OPPOSITE ORDER - DEADLOCK!)
void vTask2(void *pvParameters)
{
    while(1)
    {
        // Acquire mutex B
        xSemaphoreTake(mutexB, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Try to acquire mutex A - DEADLOCK RISK!
        xSemaphoreTake(mutexA, portMAX_DELAY);
        
        // Critical section
        printf("Task 2 executing\n");
        
        xSemaphoreGive(mutexA);
        xSemaphoreGive(mutexB);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Example 2: Lock Ordering Strategy (Prevention)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

SemaphoreHandle_t mutexA;
SemaphoreHandle_t mutexB;

// PREVENTION: Always acquire mutexes in the same order (A before B)

void vTask1_Safe(void *pvParameters)
{
    while(1)
    {
        // Always acquire A first, then B
        xSemaphoreTake(mutexA, portMAX_DELAY);
        xSemaphoreTake(mutexB, portMAX_DELAY);
        
        // Critical section
        printf("Task 1 executing safely\n");
        
        // Release in reverse order
        xSemaphoreGive(mutexB);
        xSemaphoreGive(mutexA);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vTask2_Safe(void *pvParameters)
{
    while(1)
    {
        // Same order: A first, then B
        xSemaphoreTake(mutexA, portMAX_DELAY);
        xSemaphoreTake(mutexB, portMAX_DELAY);
        
        // Critical section
        printf("Task 2 executing safely\n");
        
        xSemaphoreGive(mutexB);
        xSemaphoreGive(mutexA);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Example 3: Timeout-Based Detection

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define MUTEX_TIMEOUT_MS 1000

SemaphoreHandle_t mutexA;
SemaphoreHandle_t mutexB;

typedef enum {
    LOCK_SUCCESS,
    LOCK_TIMEOUT,
    LOCK_DEADLOCK_DETECTED
} LockResult_t;

LockResult_t acquireTwoMutexes(SemaphoreHandle_t first, 
                                SemaphoreHandle_t second,
                                TickType_t timeout)
{
    BaseType_t result;
    
    // Try to acquire first mutex
    result = xSemaphoreTake(first, timeout);
    if (result != pdTRUE) {
        return LOCK_TIMEOUT;
    }
    
    // Try to acquire second mutex
    result = xSemaphoreTake(second, timeout);
    if (result != pdTRUE) {
        // Failed to get second mutex - release first and report
        xSemaphoreGive(first);
        return LOCK_DEADLOCK_DETECTED;
    }
    
    return LOCK_SUCCESS;
}

void vTaskWithTimeout(void *pvParameters)
{
    while(1)
    {
        LockResult_t lockResult = acquireTwoMutexes(
            mutexA, 
            mutexB, 
            pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)
        );
        
        switch(lockResult)
        {
            case LOCK_SUCCESS:
                // Critical section
                printf("Task acquired both locks successfully\n");
                xSemaphoreGive(mutexB);
                xSemaphoreGive(mutexA);
                break;
                
            case LOCK_TIMEOUT:
                printf("WARNING: Timeout acquiring first mutex\n");
                break;
                
            case LOCK_DEADLOCK_DETECTED:
                printf("WARNING: Potential deadlock detected, backing off\n");
                vTaskDelay(pdMS_TO_TICKS(50)); // Back off
                break;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Example 4: Try-Lock Pattern (Advanced Prevention)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define MAX_RETRY_ATTEMPTS 5

BaseType_t tryAcquireBothMutexes(SemaphoreHandle_t mutexA, 
                                  SemaphoreHandle_t mutexB)
{
    uint8_t retries = 0;
    
    while(retries < MAX_RETRY_ATTEMPTS)
    {
        // Try to acquire first mutex with zero timeout
        if (xSemaphoreTake(mutexA, 0) == pdTRUE)
        {
            // Got first mutex, try second
            if (xSemaphoreTake(mutexB, 0) == pdTRUE)
            {
                // Success! Got both mutexes
                return pdTRUE;
            }
            else
            {
                // Couldn't get second mutex, release first
                xSemaphoreGive(mutexA);
            }
        }
        
        // Back off exponentially
        vTaskDelay(pdMS_TO_TICKS(10 * (1 << retries)));
        retries++;
    }
    
    return pdFALSE; // Failed after max retries
}

void vTaskTryLock(void *pvParameters)
{
    while(1)
    {
        if (tryAcquireBothMutexes(mutexA, mutexB))
        {
            // Critical section
            printf("Acquired both mutexes using try-lock\n");
            
            xSemaphoreGive(mutexB);
            xSemaphoreGive(mutexA);
        }
        else
        {
            printf("Failed to acquire mutexes, continuing...\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Example 5: Resource Hierarchy Implementation

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Define mutex hierarchy levels
typedef enum {
    MUTEX_LEVEL_0 = 0,  // Lowest priority
    MUTEX_LEVEL_1 = 1,
    MUTEX_LEVEL_2 = 2,
    MUTEX_LEVEL_3 = 3   // Highest priority
} MutexLevel_t;

typedef struct {
    SemaphoreHandle_t handle;
    MutexLevel_t level;
    const char* name;
} HierarchicalMutex_t;

HierarchicalMutex_t networkMutex = {NULL, MUTEX_LEVEL_0, "Network"};
HierarchicalMutex_t storageMutex = {NULL, MUTEX_LEVEL_1, "Storage"};
HierarchicalMutex_t displayMutex = {NULL, MUTEX_LEVEL_2, "Display"};

BaseType_t acquireHierarchicalMutex(HierarchicalMutex_t* mutex,
                                     MutexLevel_t currentLevel,
                                     TickType_t timeout)
{
    // Ensure we're acquiring mutexes in increasing order
    if (mutex->level <= currentLevel) {
        printf("ERROR: Mutex hierarchy violation! "
               "Attempting to acquire %s (level %d) while holding level %d\n",
               mutex->name, mutex->level, currentLevel);
        configASSERT(0); // Halt in debug builds
        return pdFALSE;
    }
    
    return xSemaphoreTake(mutex->handle, timeout);
}

void vTaskHierarchical(void *pvParameters)
{
    while(1)
    {
        MutexLevel_t currentLevel = MUTEX_LEVEL_0 - 1; // Start below lowest
        
        // Acquire in correct hierarchy order
        if (acquireHierarchicalMutex(&networkMutex, currentLevel, portMAX_DELAY))
        {
            currentLevel = networkMutex.level;
            
            if (acquireHierarchicalMutex(&storageMutex, currentLevel, portMAX_DELAY))
            {
                currentLevel = storageMutex.level;
                
                // Critical section with both resources
                printf("Acquired network and storage mutexes\n");
                
                xSemaphoreGive(storageMutex.handle);
            }
            
            xSemaphoreGive(networkMutex.handle);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void initHierarchicalMutexes(void)
{
    networkMutex.handle = xSemaphoreCreateMutex();
    storageMutex.handle = xSemaphoreCreateMutex();
    displayMutex.handle = xSemaphoreCreateMutex();
}
```

---

## Rust Implementation Examples

### Example 1: Type-Safe Lock Ordering

```rust
#![no_std]
#![no_main]

use freertos_rust::*;
use core::marker::PhantomData;

// Type-level lock ordering using phantom types
struct Level0;
struct Level1;
struct Level2;

// Mutex wrapper with compile-time level checking
struct OrderedMutex<T, L> {
    mutex: Mutex<T>,
    _level: PhantomData<L>,
}

impl<T, L> OrderedMutex<T, L> {
    fn new(data: T) -> Self {
        Self {
            mutex: Mutex::new(data).unwrap(),
            _level: PhantomData,
        }
    }
}

// Lock guard that tracks the current level
struct LockGuard<'a, T, L> {
    guard: MutexGuard<'a, T>,
    _level: PhantomData<L>,
}

// Helper trait to enforce ordering at compile time
trait CanAcquireAfter<PrevLevel> {}

impl CanAcquireAfter<Level0> for Level1 {}
impl CanAcquireAfter<Level1> for Level2 {}

impl<T, L> OrderedMutex<T, L> {
    fn lock(&self, timeout: Duration) -> Option<LockGuard<T, L>> {
        self.mutex.lock(timeout).map(|guard| LockGuard {
            guard,
            _level: PhantomData,
        })
    }
}

// Safe acquisition function with compile-time checking
fn acquire_ordered<T1, T2, L1, L2>(
    first: &OrderedMutex<T1, L1>,
    second: &OrderedMutex<T2, L2>,
    timeout: Duration,
) -> Option<(LockGuard<T1, L1>, LockGuard<T2, L2>)>
where
    L2: CanAcquireAfter<L1>,
{
    let guard1 = first.lock(timeout)?;
    let guard2 = second.lock(timeout)?;
    Some((guard1, guard2))
}

// Example usage
static RESOURCE_A: OrderedMutex<u32, Level0> = OrderedMutex::new(0);
static RESOURCE_B: OrderedMutex<u32, Level1> = OrderedMutex::new(0);

fn safe_task(delay: Duration) {
    loop {
        // This compiles - correct order
        if let Some((mut a, mut b)) = acquire_ordered(
            &RESOURCE_A,
            &RESOURCE_B,
            Duration::ms(1000),
        ) {
            *a += 1;
            *b += 1;
            FreeRtosUtils::print("Task executed safely\n");
        }
        
        CurrentTask::delay(delay);
    }
}

// This would NOT compile - wrong order:
// acquire_ordered(&RESOURCE_B, &RESOURCE_A, Duration::ms(1000));
```

### Example 2: Timeout-Based Deadlock Detection in Rust

```rust
use freertos_rust::*;
use core::time::Duration;

enum LockResult {
    Success,
    Timeout,
    DeadlockDetected,
}

struct DeadlockDetector {
    mutex_a: Mutex<i32>,
    mutex_b: Mutex<i32>,
    timeout: Duration,
}

impl DeadlockDetector {
    fn new(timeout_ms: u32) -> Self {
        Self {
            mutex_a: Mutex::new(0).unwrap(),
            mutex_b: Mutex::new(0).unwrap(),
            timeout: Duration::ms(timeout_ms),
        }
    }
    
    fn acquire_both(&self) -> LockResult {
        // Try to acquire first mutex
        let guard_a = match self.mutex_a.lock(self.timeout) {
            Some(g) => g,
            None => return LockResult::Timeout,
        };
        
        // Try to acquire second mutex
        let guard_b = match self.mutex_b.lock(self.timeout) {
            Some(g) => g,
            None => {
                // Drop first guard automatically
                drop(guard_a);
                return LockResult::DeadlockDetected;
            }
        };
        
        // Critical section
        FreeRtosUtils::print("Both locks acquired\n");
        
        // Guards are automatically dropped
        LockResult::Success
    }
}

fn task_with_detection(detector: &'static DeadlockDetector) {
    loop {
        match detector.acquire_both() {
            LockResult::Success => {
                FreeRtosUtils::print("Operation completed\n");
            }
            LockResult::Timeout => {
                FreeRtosUtils::print("WARNING: Timeout\n");
            }
            LockResult::DeadlockDetected => {
                FreeRtosUtils::print("WARNING: Potential deadlock\n");
                CurrentTask::delay(Duration::ms(50));
            }
        }
        
        CurrentTask::delay(Duration::ms(100));
    }
}
```

### Example 3: Try-Lock Pattern with Exponential Backoff

```rust
use freertos_rust::*;

struct TryLockManager {
    mutex_a: Mutex<u32>,
    mutex_b: Mutex<u32>,
    max_retries: u8,
}

impl TryLockManager {
    fn new(max_retries: u8) -> Self {
        Self {
            mutex_a: Mutex::new(0).unwrap(),
            mutex_b: Mutex::new(0).unwrap(),
            max_retries,
        }
    }
    
    fn try_acquire_both(&self) -> bool {
        for attempt in 0..self.max_retries {
            // Try non-blocking acquire
            if let Some(guard_a) = self.mutex_a.lock(Duration::ms(0)) {
                if let Some(guard_b) = self.mutex_b.lock(Duration::ms(0)) {
                    // Success! Both acquired
                    FreeRtosUtils::print("Acquired both locks\n");
                    return true;
                }
                // guard_a dropped automatically
            }
            
            // Exponential backoff
            let backoff_ms = 10u32.saturating_mul(1 << attempt);
            CurrentTask::delay(Duration::ms(backoff_ms));
        }
        
        false
    }
}

fn try_lock_task(manager: &'static TryLockManager) {
    loop {
        if manager.try_acquire_both() {
            FreeRtosUtils::print("Operation succeeded\n");
        } else {
            FreeRtosUtils::print("Failed after retries\n");
        }
        
        CurrentTask::delay(Duration::ms(100));
    }
}
```

### Example 4: RAII-Based Resource Acquisition

```rust
use freertos_rust::*;
use core::ops::{Deref, DerefMut};

// Custom guard that ensures proper release order
struct MultiGuard<'a, T1, T2> {
    guard1: Option<MutexGuard<'a, T1>>,
    guard2: Option<MutexGuard<'a, T2>>,
}

impl<'a, T1, T2> MultiGuard<'a, T1, T2> {
    fn new(
        mutex1: &'a Mutex<T1>,
        mutex2: &'a Mutex<T2>,
        timeout: Duration,
    ) -> Option<Self> {
        let g1 = mutex1.lock(timeout)?;
        let g2 = mutex2.lock(timeout)?;
        
        Some(Self {
            guard1: Some(g1),
            guard2: Some(g2),
        })
    }
    
    fn get_first(&mut self) -> &mut T1 {
        self.guard1.as_mut().unwrap().deref_mut()
    }
    
    fn get_second(&mut self) -> &mut T2 {
        self.guard2.as_mut().unwrap().deref_mut()
    }
}

impl<'a, T1, T2> Drop for MultiGuard<'a, T1, T2> {
    fn drop(&mut self) {
        // Drop in reverse order (LIFO)
        drop(self.guard2.take());
        drop(self.guard1.take());
    }
}

// Usage example
fn raii_task(
    mutex_a: &'static Mutex<u32>,
    mutex_b: &'static Mutex<String>,
) {
    loop {
        if let Some(mut guard) = MultiGuard::new(
            mutex_a,
            mutex_b,
            Duration::ms(1000),
        ) {
            *guard.get_first() += 1;
            guard.get_second().push_str("updated");
            
            // Automatic release in correct order when guard drops
        }
        
        CurrentTask::delay(Duration::ms(100));
    }
}
```

---

## Best Practices Summary

### Prevention Strategies

1. **Lock Ordering**: Always acquire mutexes in a consistent global order
2. **Lock Hierarchy**: Define priority levels for resources
3. **Timeout Acquisition**: Never use infinite waits in production
4. **Try-Lock**: Use non-blocking attempts with backoff
5. **Minimize Lock Scope**: Hold locks for the shortest time possible
6. **Avoid Nested Locks**: Redesign to need only one lock at a time when possible

### Detection Strategies

1. **Timeout Monitoring**: Log timeout events for analysis
2. **Watchdog Timers**: Detect task starvation
3. **Resource Tracking**: Maintain ownership records (debug builds)
4. **Static Analysis**: Use tools to detect potential circular dependencies

### Recovery Strategies

1. **Exponential Backoff**: When deadlock detected, release and retry
2. **Priority Inversion**: Use priority inheritance mutexes
3. **Graceful Degradation**: Have fallback behavior when locks unavailable

---

## Summary

Deadlock prevention in FreeRTOS requires disciplined resource management. The C/C++ examples demonstrate practical timeout-based detection and lock ordering strategies, while Rust's type system enables compile-time prevention through phantom types and RAII patterns. Key takeaways: always acquire locks in consistent order, use timeouts instead of infinite waits, implement exponential backoff for retry logic, and leverage language features (like Rust's ownership) for safety. Combining multiple strategies—hierarchical locking, try-lock patterns, and timeout monitoring—creates robust systems resistant to deadlock conditions. Regular code reviews and static analysis tools complement runtime strategies for comprehensive deadlock prevention.