# 104. Multicore Synchronization in FreeRTOS

## Detailed Description

Multicore synchronization in FreeRTOS addresses the challenges of coordinating tasks and shared resources across multiple processor cores in Symmetric Multiprocessing (SMP) systems. Unlike single-core systems where preemption is the primary concern, multicore systems must handle true parallel execution where multiple tasks can run simultaneously on different cores.

### Key Concepts

**1. SMP (Symmetric Multiprocessing) Architecture**
- Multiple cores share the same memory space
- Tasks can migrate between cores or be pinned to specific cores
- Requires careful coordination to prevent race conditions and data corruption

**2. Cache Coherency**
- Each core typically has its own cache (L1, sometimes L2)
- Hardware cache coherency protocols (like MESI, MOESI) ensure data consistency
- Software must still be aware of cache line bouncing and false sharing
- Memory barriers and atomic operations interact with cache coherency

**3. Spinlocks**
- Busy-waiting locks suitable for very short critical sections
- More efficient than blocking when lock hold time is minimal
- Can cause priority inversion issues if not used carefully
- Must be paired with interrupt disabling or other mechanisms

**4. Inter-Core Communication**
- Message passing between cores
- Shared memory with synchronization primitives
- Lock-free queues and ring buffers
- Hardware-specific mechanisms (interrupts, mailboxes)

## Programming Examples

### C/C++ Examples

#### 1. Basic Spinlock Implementation

```c
#include "FreeRTOS.h"
#include "task.h"
#include "portmacro.h"

// Spinlock structure
typedef struct {
    volatile uint32_t lock;
    volatile uint32_t owner_core;
} spinlock_t;

// Initialize spinlock
void spinlock_init(spinlock_t *spin) {
    spin->lock = 0;
    spin->owner_core = 0xFF;  // Invalid core ID
}

// Acquire spinlock with timeout
BaseType_t spinlock_acquire(spinlock_t *spin, TickType_t timeout) {
    TickType_t start = xTaskGetTickCount();
    uint32_t current_core = xPortGetCoreID();
    
    while (1) {
        // Try to acquire lock atomically
        uint32_t expected = 0;
        if (__atomic_compare_exchange_n(&spin->lock, &expected, 1,
                                        false, __ATOMIC_ACQUIRE, 
                                        __ATOMIC_RELAXED)) {
            spin->owner_core = current_core;
            return pdTRUE;
        }
        
        // Check timeout
        if (timeout != portMAX_DELAY) {
            if ((xTaskGetTickCount() - start) >= timeout) {
                return pdFALSE;
            }
        }
        
        // Brief yield to prevent starvation
        taskYIELD();
    }
}

// Release spinlock
void spinlock_release(spinlock_t *spin) {
    spin->owner_core = 0xFF;
    __atomic_store_n(&spin->lock, 0, __ATOMIC_RELEASE);
}

// Example usage with shared counter
typedef struct {
    spinlock_t lock;
    uint32_t counter;
} shared_data_t;

shared_data_t g_shared_data;

void producer_task(void *pvParameters) {
    spinlock_init(&g_shared_data.lock);
    
    while (1) {
        if (spinlock_acquire(&g_shared_data.lock, pdMS_TO_TICKS(100))) {
            g_shared_data.counter++;
            printf("Core %d: Counter = %lu\n", 
                   xPortGetCoreID(), g_shared_data.counter);
            spinlock_release(&g_shared_data.lock);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

#### 2. Lock-Free Inter-Core Queue

```c
#include <stdatomic.h>

#define QUEUE_SIZE 256
#define CACHE_LINE_SIZE 64

// Cache-aligned structure to prevent false sharing
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) {
    atomic_uint_fast32_t head;
    uint8_t padding1[CACHE_LINE_SIZE - sizeof(atomic_uint_fast32_t)];
    
    atomic_uint_fast32_t tail;
    uint8_t padding2[CACHE_LINE_SIZE - sizeof(atomic_uint_fast32_t)];
    
    void* buffer[QUEUE_SIZE];
} lockfree_queue_t;

// Initialize queue
void lockfree_queue_init(lockfree_queue_t *q) {
    atomic_store_explicit(&q->head, 0, memory_order_relaxed);
    atomic_store_explicit(&q->tail, 0, memory_order_relaxed);
    memset(q->buffer, 0, sizeof(q->buffer));
}

// Enqueue (producer)
BaseType_t lockfree_enqueue(lockfree_queue_t *q, void *item) {
    uint32_t current_tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    uint32_t next_tail = (current_tail + 1) % QUEUE_SIZE;
    uint32_t current_head = atomic_load_explicit(&q->head, memory_order_acquire);
    
    // Check if queue is full
    if (next_tail == current_head) {
        return pdFALSE;
    }
    
    q->buffer[current_tail] = item;
    
    // Memory barrier to ensure write completes before updating tail
    atomic_store_explicit(&q->tail, next_tail, memory_order_release);
    
    return pdTRUE;
}

// Dequeue (consumer)
BaseType_t lockfree_dequeue(lockfree_queue_t *q, void **item) {
    uint32_t current_head = atomic_load_explicit(&q->head, memory_order_relaxed);
    uint32_t current_tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    
    // Check if queue is empty
    if (current_head == current_tail) {
        return pdFALSE;
    }
    
    *item = q->buffer[current_head];
    
    uint32_t next_head = (current_head + 1) % QUEUE_SIZE;
    atomic_store_explicit(&q->head, next_head, memory_order_release);
    
    return pdTRUE;
}

// Example: Inter-core messaging
lockfree_queue_t core_to_core_queue;

void core0_sender_task(void *pvParameters) {
    uint32_t msg_id = 0;
    
    while (1) {
        uint32_t *msg = pvPortMalloc(sizeof(uint32_t));
        *msg = msg_id++;
        
        if (lockfree_enqueue(&core_to_core_queue, msg)) {
            printf("Core 0 sent message: %lu\n", *msg);
        } else {
            pvPortFree(msg);
            printf("Queue full!\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void core1_receiver_task(void *pvParameters) {
    while (1) {
        void *msg;
        
        if (lockfree_dequeue(&core_to_core_queue, &msg)) {
            printf("Core 1 received message: %lu\n", *(uint32_t*)msg);
            pvPortFree(msg);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

#### 3. Critical Section with Cache Coherency Awareness

```c
// Prevent false sharing with proper alignment
typedef struct __attribute__((aligned(CACHE_LINE_SIZE))) {
    SemaphoreHandle_t mutex;
    uint8_t padding1[CACHE_LINE_SIZE - sizeof(SemaphoreHandle_t)];
    
    volatile uint32_t shared_value;
    uint8_t padding2[CACHE_LINE_SIZE - sizeof(uint32_t)];
    
    volatile uint32_t stats_core0;
    uint8_t padding3[CACHE_LINE_SIZE - sizeof(uint32_t)];
    
    volatile uint32_t stats_core1;
    uint8_t padding4[CACHE_LINE_SIZE - sizeof(uint32_t)];
} cache_aligned_data_t;

cache_aligned_data_t g_data;

void multicore_task(void *pvParameters) {
    uint32_t core_id = xPortGetCoreID();
    
    while (1) {
        // Use mutex for shared data access
        if (xSemaphoreTake(g_data.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_data.shared_value++;
            
            // Release with memory barrier
            __atomic_thread_fence(__ATOMIC_RELEASE);
            xSemaphoreGive(g_data.mutex);
        }
        
        // Update core-specific stats (no locking needed due to alignment)
        if (core_id == 0) {
            g_data.stats_core0++;
        } else {
            g_data.stats_core1++;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

### Rust Examples

#### 1. Spinlock Implementation in Rust

```rust
use core::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use core::cell::UnsafeCell;
use freertos_rust::*;

pub struct Spinlock<T> {
    locked: AtomicBool,
    owner_core: AtomicU32,
    data: UnsafeCell<T>,
}

unsafe impl<T: Send> Sync for Spinlock<T> {}
unsafe impl<T: Send> Send for Spinlock<T> {}

impl<T> Spinlock<T> {
    pub const fn new(data: T) -> Self {
        Spinlock {
            locked: AtomicBool::new(false),
            owner_core: AtomicU32::new(0xFF),
            data: UnsafeCell::new(data),
        }
    }
    
    pub fn lock(&self) -> SpinlockGuard<T> {
        loop {
            // Try to acquire lock
            if self.locked
                .compare_exchange_weak(
                    false,
                    true,
                    Ordering::Acquire,
                    Ordering::Relaxed
                )
                .is_ok()
            {
                // Successfully acquired
                self.owner_core.store(
                    get_current_core_id(),
                    Ordering::Relaxed
                );
                break;
            }
            
            // Yield to prevent busy-waiting starvation
            CurrentTask::delay(Duration::ms(0));
        }
        
        SpinlockGuard { lock: self }
    }
    
    pub fn try_lock(&self) -> Option<SpinlockGuard<T>> {
        if self.locked
            .compare_exchange(
                false,
                true,
                Ordering::Acquire,
                Ordering::Relaxed
            )
            .is_ok()
        {
            self.owner_core.store(
                get_current_core_id(),
                Ordering::Relaxed
            );
            Some(SpinlockGuard { lock: self })
        } else {
            None
        }
    }
}

pub struct SpinlockGuard<'a, T> {
    lock: &'a Spinlock<T>,
}

impl<'a, T> core::ops::Deref for SpinlockGuard<'a, T> {
    type Target = T;
    
    fn deref(&self) -> &T {
        unsafe { &*self.lock.data.get() }
    }
}

impl<'a, T> core::ops::DerefMut for SpinlockGuard<'a, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.lock.data.get() }
    }
}

impl<'a, T> Drop for SpinlockGuard<'a, T> {
    fn drop(&mut self) {
        self.lock.owner_core.store(0xFF, Ordering::Relaxed);
        self.lock.locked.store(false, Ordering::Release);
    }
}

// Usage example
static SHARED_COUNTER: Spinlock<u32> = Spinlock::new(0);

fn producer_task(_: TaskParameter) {
    loop {
        {
            let mut counter = SHARED_COUNTER.lock();
            *counter += 1;
            println!("Core {}: Counter = {}", get_current_core_id(), *counter);
        } // Lock automatically released here
        
        CurrentTask::delay(Duration::ms(10));
    }
}
```

#### 2. Lock-Free Queue in Rust

```rust
use core::sync::atomic::{AtomicUsize, Ordering};
use core::ptr;

const QUEUE_SIZE: usize = 256;

#[repr(align(64))] // Cache line alignment
pub struct LockFreeQueue<T> {
    head: AtomicUsize,
    _padding1: [u8; 64 - core::mem::size_of::<AtomicUsize>()],
    
    tail: AtomicUsize,
    _padding2: [u8; 64 - core::mem::size_of::<AtomicUsize>()],
    
    buffer: [AtomicPtr<T>; QUEUE_SIZE],
}

impl<T> LockFreeQueue<T> {
    pub const fn new() -> Self {
        const INIT: AtomicPtr<u8> = AtomicPtr::new(ptr::null_mut());
        
        LockFreeQueue {
            head: AtomicUsize::new(0),
            _padding1: [0; 64 - core::mem::size_of::<AtomicUsize>()],
            tail: AtomicUsize::new(0),
            _padding2: [0; 64 - core::mem::size_of::<AtomicUsize>()],
            buffer: [INIT; QUEUE_SIZE],
        }
    }
    
    pub fn enqueue(&self, item: Box<T>) -> Result<(), Box<T>> {
        let current_tail = self.tail.load(Ordering::Relaxed);
        let next_tail = (current_tail + 1) % QUEUE_SIZE;
        let current_head = self.head.load(Ordering::Acquire);
        
        // Check if full
        if next_tail == current_head {
            return Err(item);
        }
        
        // Store item
        self.buffer[current_tail].store(
            Box::into_raw(item),
            Ordering::Relaxed
        );
        
        // Update tail with release semantics
        self.tail.store(next_tail, Ordering::Release);
        
        Ok(())
    }
    
    pub fn dequeue(&self) -> Option<Box<T>> {
        let current_head = self.head.load(Ordering::Relaxed);
        let current_tail = self.tail.load(Ordering::Acquire);
        
        // Check if empty
        if current_head == current_tail {
            return None;
        }
        
        // Retrieve item
        let item_ptr = self.buffer[current_head].load(Ordering::Relaxed);
        
        // Update head
        let next_head = (current_head + 1) % QUEUE_SIZE;
        self.head.store(next_head, Ordering::Release);
        
        unsafe { Some(Box::from_raw(item_ptr)) }
    }
}

// Usage example
static INTER_CORE_QUEUE: LockFreeQueue<u32> = LockFreeQueue::new();

fn sender_task(_: TaskParameter) {
    let mut msg_id = 0u32;
    
    loop {
        let msg = Box::new(msg_id);
        
        match INTER_CORE_QUEUE.enqueue(msg) {
            Ok(_) => println!("Sent: {}", msg_id),
            Err(_) => println!("Queue full"),
        }
        
        msg_id += 1;
        CurrentTask::delay(Duration::ms(100));
    }
}

fn receiver_task(_: TaskParameter) {
    loop {
        if let Some(msg) = INTER_CORE_QUEUE.dequeue() {
            println!("Core {} received: {}", get_current_core_id(), *msg);
        }
        
        CurrentTask::delay(Duration::ms(10));
    }
}
```

#### 3. Memory Barrier and Atomic Operations

```rust
use core::sync::atomic::{fence, Ordering, AtomicU32};

#[repr(align(64))]
pub struct CoreStats {
    counter: AtomicU32,
    _padding: [u8; 60],
}

impl CoreStats {
    pub const fn new() -> Self {
        CoreStats {
            counter: AtomicU32::new(0),
            _padding: [0; 60],
        }
    }
    
    pub fn increment(&self) {
        self.counter.fetch_add(1, Ordering::Relaxed);
    }
    
    pub fn get(&self) -> u32 {
        fence(Ordering::Acquire);
        self.counter.load(Ordering::Relaxed)
    }
}

static CORE0_STATS: CoreStats = CoreStats::new();
static CORE1_STATS: CoreStats = CoreStats::new();

fn stats_task(_: TaskParameter) {
    let core_id = get_current_core_id();
    
    loop {
        if core_id == 0 {
            CORE0_STATS.increment();
        } else {
            CORE1_STATS.increment();
        }
        
        // Periodic reporting with memory barriers
        if CORE0_STATS.get() % 1000 == 0 {
            fence(Ordering::SeqCst);
            println!("Core 0: {}, Core 1: {}", 
                     CORE0_STATS.get(), 
                     CORE1_STATS.get());
        }
        
        CurrentTask::delay(Duration::ms(1));
    }
}
```

## Summary

Multicore synchronization in FreeRTOS requires careful consideration of hardware architecture, cache behavior, and atomic operations. Key takeaways include:

1. **Spinlocks** are ideal for very short critical sections where the overhead of context switching exceeds busy-waiting costs
2. **Cache coherency** must be managed through proper alignment, memory barriers, and atomic operations to prevent false sharing
3. **Lock-free data structures** enable efficient inter-core communication without blocking, but require deep understanding of memory ordering
4. **Memory barriers** (`Acquire`, `Release`, `SeqCst`) ensure proper visibility of shared data across cores
5. **Cache line alignment** prevents false sharing where independent variables unnecessarily contend for cache lines
6. **Core affinity** can be used strategically to reduce cache misses and improve locality

Both C/C++ and Rust provide atomic operations and memory ordering primitives, though Rust's type system provides additional safety guarantees at compile time. Proper multicore synchronization is critical for correctness, performance, and predictability in SMP RTOS systems.