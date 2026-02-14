# DMA and Zero-Copy Patterns in FreeRTOS

## Overview

Direct Memory Access (DMA) is a hardware feature that allows peripherals to transfer data to/from memory without CPU intervention. In FreeRTOS systems, DMA can dramatically improve performance and reduce CPU load, but it introduces challenges around memory coherency, thread safety, and buffer management. Zero-copy patterns eliminate unnecessary data copying, further optimizing throughput and latency.

## Core Concepts

### DMA Fundamentals

DMA controllers transfer data independently of the CPU, operating on physical memory addresses. Key characteristics:

- **Asynchronous operation**: DMA runs concurrently with CPU tasks
- **Direct memory access**: Bypasses CPU cache in many architectures
- **Interrupt-driven completion**: DMA generates interrupts when transfers complete
- **Hardware-specific configuration**: Each peripheral has unique DMA requirements

### Cache Coherency Issues

Modern microcontrollers use data caches to accelerate CPU memory access. Problems arise when:

1. **CPU writes to memory**: Data may sit in cache, not visible to DMA
2. **DMA writes to memory**: Updated data bypasses cache, CPU reads stale cached values
3. **Speculative reads**: CPU may pre-fetch data before DMA completes

### Zero-Copy Strategies

Traditional I/O involves multiple copies: peripheral → DMA buffer → application buffer. Zero-copy eliminates intermediate copies by:

- **Direct buffer sharing**: Applications work directly on DMA buffers
- **Buffer ownership transfer**: Explicit handoff between DMA and tasks
- **Scatter-gather DMA**: Operating on discontiguous memory regions

## Implementation Patterns

### 1. Cache-Safe DMA Buffers

**Key Requirements:**
- Align buffers to cache line boundaries
- Use uncached memory regions or explicit cache operations
- Ensure buffer size is cache-line multiple

### 2. Double/Circular Buffering

Allows continuous streaming without data loss:
- One buffer fills via DMA while another processes
- Ring buffers for continuous data streams
- Ping-pong buffers for discrete transfers

### 3. DMA Completion Synchronization

Use FreeRTOS primitives to coordinate:
- Semaphores/event groups signal completion
- Task notifications for low-latency wakeup
- Queue-based buffer management

## C/C++ Implementation

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include <string.h>

// Cache line size (typically 32 bytes on Cortex-M7)
#define CACHE_LINE_SIZE 32
#define DMA_BUFFER_SIZE 512

// Macro to align to cache line boundary
#define CACHE_ALIGN __attribute__((aligned(CACHE_LINE_SIZE)))

// DMA buffer structure with ownership tracking
typedef struct {
    uint8_t data[DMA_BUFFER_SIZE] CACHE_ALIGN;
    size_t length;
    uint32_t timestamp;
    volatile uint8_t owned_by_dma;
} DMABuffer_t;

// Double buffer for ping-pong operation
static DMABuffer_t dma_buffers[2] CACHE_ALIGN;
static volatile uint8_t active_buffer = 0;

// Synchronization primitives
static SemaphoreHandle_t dma_complete_sem;
static QueueHandle_t filled_buffer_queue;

// Hardware abstraction (platform-specific)
typedef struct {
    volatile uint32_t *src_addr;
    volatile uint32_t *dst_addr;
    volatile uint32_t count;
    volatile uint32_t ctrl;
} DMA_Channel_t;

static DMA_Channel_t *dma_channel = (DMA_Channel_t*)0x40020000; // Example address

// Cache management functions (ARM Cortex-M specific)
static inline void cache_clean(void *addr, size_t size) {
    #ifdef __ARM_ARCH_7EM__
    // SCB_CleanDCache_by_Addr for Cortex-M7
    uint32_t start = (uint32_t)addr & ~(CACHE_LINE_SIZE - 1);
    uint32_t end = (uint32_t)addr + size;
    for (uint32_t va = start; va < end; va += CACHE_LINE_SIZE) {
        __asm volatile("dc cvac, %0" :: "r"(va) : "memory");
    }
    __asm volatile("dsb" ::: "memory");
    #endif
}

static inline void cache_invalidate(void *addr, size_t size) {
    #ifdef __ARM_ARCH_7EM__
    // SCB_InvalidateDCache_by_Addr for Cortex-M7
    uint32_t start = (uint32_t)addr & ~(CACHE_LINE_SIZE - 1);
    uint32_t end = (uint32_t)addr + size;
    for (uint32_t va = start; va < end; va += CACHE_LINE_SIZE) {
        __asm volatile("dc ivac, %0" :: "r"(va) : "memory");
    }
    __asm volatile("dsb" ::: "memory");
    #endif
}

// Initialize DMA subsystem
void dma_init(void) {
    // Create synchronization primitives
    dma_complete_sem = xSemaphoreCreateBinary();
    filled_buffer_queue = xQueueCreate(2, sizeof(DMABuffer_t*));
    
    // Initialize buffers
    memset(dma_buffers, 0, sizeof(dma_buffers));
    
    // Enable DMA interrupt (platform-specific)
    NVIC_EnableIRQ(DMA_IRQn);
}

// Start DMA transfer (zero-copy transmit)
BaseType_t dma_transmit_zero_copy(const uint8_t *data, size_t len) {
    if (len > DMA_BUFFER_SIZE) {
        return pdFAIL;
    }
    
    // Get next buffer
    uint8_t buf_idx = active_buffer;
    DMABuffer_t *buf = &dma_buffers[buf_idx];
    
    // Wait if buffer still owned by DMA
    while (buf->owned_by_dma) {
        vTaskDelay(1);
    }
    
    // Copy data to DMA buffer
    memcpy(buf->data, data, len);
    buf->length = len;
    buf->owned_by_dma = 1;
    
    // Clean cache to ensure DMA sees updated data
    cache_clean(buf->data, len);
    
    // Configure DMA transfer
    dma_channel->src_addr = (uint32_t*)buf->data;
    dma_channel->dst_addr = (uint32_t*)0x40001000; // UART TX register
    dma_channel->count = len;
    dma_channel->ctrl = DMA_ENABLE | DMA_MEM_TO_PERIPH | DMA_IRQ_EN;
    
    // Switch to next buffer for ping-pong
    active_buffer = !active_buffer;
    
    return pdPASS;
}

// Start DMA receive (zero-copy pattern)
void dma_receive_start(void) {
    uint8_t buf_idx = active_buffer;
    DMABuffer_t *buf = &dma_buffers[buf_idx];
    
    buf->owned_by_dma = 1;
    
    // Invalidate cache before DMA writes
    cache_invalidate(buf->data, DMA_BUFFER_SIZE);
    
    // Configure DMA receive
    dma_channel->src_addr = (uint32_t*)0x40001004; // UART RX register
    dma_channel->dst_addr = (uint32_t*)buf->data;
    dma_channel->count = DMA_BUFFER_SIZE;
    dma_channel->ctrl = DMA_ENABLE | DMA_PERIPH_TO_MEM | DMA_IRQ_EN;
}

// DMA interrupt handler
void DMA_IRQHandler(void) {
    BaseType_t higher_priority_woken = pdFALSE;
    
    // Get completed buffer
    uint8_t completed_idx = !active_buffer;
    DMABuffer_t *buf = &dma_buffers[completed_idx];
    
    // Clear interrupt flag
    dma_channel->ctrl &= ~DMA_IRQ_FLAG;
    
    // Invalidate cache to read DMA-written data
    cache_invalidate(buf->data, buf->length);
    
    // Release ownership
    buf->owned_by_dma = 0;
    buf->timestamp = xTaskGetTickCountFromISR();
    
    // Signal completion
    xSemaphoreGiveFromISR(dma_complete_sem, &higher_priority_woken);
    
    // Queue buffer for processing (zero-copy handoff)
    xQueueSendFromISR(filled_buffer_queue, &buf, &higher_priority_woken);
    
    portYIELD_FROM_ISR(higher_priority_woken);
}

// Consumer task (zero-copy processing)
void process_task(void *params) {
    DMABuffer_t *buf;
    
    while (1) {
        // Wait for filled buffer
        if (xQueueReceive(filled_buffer_queue, &buf, portMAX_DELAY) == pdPASS) {
            // Process data directly from DMA buffer (zero-copy)
            for (size_t i = 0; i < buf->length; i++) {
                // Process buf->data[i] without copying
                process_byte(buf->data[i]);
            }
            
            // Buffer can be reused after processing
            // No explicit free needed for static buffers
        }
    }
}

// Circular buffer with DMA (scatter-gather pattern)
#define CIRCULAR_BUF_SIZE 2048
static uint8_t circular_buffer[CIRCULAR_BUF_SIZE] CACHE_ALIGN;
static volatile uint32_t write_index = 0;
static volatile uint32_t read_index = 0;

void dma_circular_start(void) {
    // Configure DMA in circular mode
    dma_channel->src_addr = (uint32_t*)0x40001004;
    dma_channel->dst_addr = (uint32_t*)circular_buffer;
    dma_channel->count = CIRCULAR_BUF_SIZE;
    dma_channel->ctrl = DMA_ENABLE | DMA_CIRCULAR | DMA_HALF_IRQ | DMA_FULL_IRQ;
}

void DMA_Circular_IRQHandler(void) {
    BaseType_t higher_priority_woken = pdFALSE;
    
    if (dma_channel->ctrl & DMA_HALF_COMPLETE) {
        // First half complete, invalidate first half
        cache_invalidate(circular_buffer, CIRCULAR_BUF_SIZE / 2);
        write_index = CIRCULAR_BUF_SIZE / 2;
    } else if (dma_channel->ctrl & DMA_FULL_COMPLETE) {
        // Second half complete, invalidate second half
        cache_invalidate(circular_buffer + CIRCULAR_BUF_SIZE / 2, CIRCULAR_BUF_SIZE / 2);
        write_index = 0; // Wrap around
    }
    
    xTaskNotifyFromISR(process_task_handle, write_index, eSetValueWithOverwrite, &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}
```

## C++ Implementation (Modern Patterns)

```cpp
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <array>
#include <atomic>
#include <optional>
#include <memory>
#include <span>

// Cache-aligned allocator
template<typename T, size_t Alignment = 32>
class CacheAlignedAllocator {
public:
    using value_type = T;
    
    T* allocate(size_t n) {
        void* ptr = nullptr;
        if (posix_memalign(&ptr, Alignment, n * sizeof(T)) == 0) {
            return static_cast<T*>(ptr);
        }
        throw std::bad_alloc();
    }
    
    void deallocate(T* ptr, size_t) {
        free(ptr);
    }
};

// RAII wrapper for DMA buffer ownership
class DMABuffer {
private:
    alignas(32) std::array<uint8_t, 512> data_;
    std::atomic<bool> owned_by_dma_{false};
    size_t length_{0};
    
public:
    // Acquire ownership for CPU
    bool acquire() {
        bool expected = false;
        return owned_by_dma_.compare_exchange_strong(expected, false);
    }
    
    // Transfer ownership to DMA
    void release_to_dma() {
        owned_by_dma_.store(true, std::memory_order_release);
    }
    
    // Get data span (zero-copy access)
    std::span<uint8_t> get_span() {
        return std::span<uint8_t>(data_.data(), length_);
    }
    
    std::span<const uint8_t> get_span() const {
        return std::span<const uint8_t>(data_.data(), length_);
    }
    
    void set_length(size_t len) { length_ = len; }
    size_t get_length() const { return length_; }
    
    uint8_t* data() { return data_.data(); }
    const uint8_t* data() const { return data_.data(); }
};

// Zero-copy DMA manager
class DMAManager {
private:
    static constexpr size_t NUM_BUFFERS = 4;
    std::array<DMABuffer, NUM_BUFFERS> buffers_;
    QueueHandle_t free_queue_;
    QueueHandle_t filled_queue_;
    SemaphoreHandle_t dma_mutex_;
    
    void cache_clean(void* addr, size_t size) {
        #ifdef __ARM_ARCH_7EM__
        uint32_t start = reinterpret_cast<uint32_t>(addr) & ~31u;
        uint32_t end = reinterpret_cast<uint32_t>(addr) + size;
        for (uint32_t va = start; va < end; va += 32) {
            asm volatile("dc cvac, %0" :: "r"(va) : "memory");
        }
        asm volatile("dsb" ::: "memory");
        #endif
    }
    
    void cache_invalidate(void* addr, size_t size) {
        #ifdef __ARM_ARCH_7EM__
        uint32_t start = reinterpret_cast<uint32_t>(addr) & ~31u;
        uint32_t end = reinterpret_cast<uint32_t>(addr) + size;
        for (uint32_t va = start; va < end; va += 32) {
            asm volatile("dc ivac, %0" :: "r"(va) : "memory");
        }
        asm volatile("dsb" ::: "memory");
        #endif
    }
    
public:
    DMAManager() {
        free_queue_ = xQueueCreate(NUM_BUFFERS, sizeof(DMABuffer*));
        filled_queue_ = xQueueCreate(NUM_BUFFERS, sizeof(DMABuffer*));
        dma_mutex_ = xSemaphoreCreateMutex();
        
        // Initially all buffers are free
        for (auto& buf : buffers_) {
            DMABuffer* ptr = &buf;
            xQueueSend(free_queue_, &ptr, 0);
        }
    }
    
    // Get free buffer (zero-copy acquisition)
    std::optional<DMABuffer*> get_free_buffer(TickType_t timeout = portMAX_DELAY) {
        DMABuffer* buf;
        if (xQueueReceive(free_queue_, &buf, timeout) == pdPASS) {
            return buf;
        }
        return std::nullopt;
    }
    
    // Submit buffer for DMA transmission
    bool transmit(DMABuffer* buf) {
        if (!buf) return false;
        
        xSemaphoreTake(dma_mutex_, portMAX_DELAY);
        
        buf->release_to_dma();
        cache_clean(buf->data(), buf->get_length());
        
        // Configure and start DMA (hardware-specific)
        start_dma_transfer(buf->data(), buf->get_length());
        
        xSemaphoreGive(dma_mutex_);
        return true;
    }
    
    // Called from DMA ISR
    void on_transfer_complete(DMABuffer* buf) {
        BaseType_t higher_priority_woken = pdFALSE;
        
        cache_invalidate(buf->data(), buf->get_length());
        
        // Hand buffer to consumer
        xQueueSendFromISR(filled_queue_, &buf, &higher_priority_woken);
        
        portYIELD_FROM_ISR(higher_priority_woken);
    }
    
    // Consumer gets filled buffer
    std::optional<DMABuffer*> get_filled_buffer(TickType_t timeout = portMAX_DELAY) {
        DMABuffer* buf;
        if (xQueueReceive(filled_queue_, &buf, timeout) == pdPASS) {
            return buf;
        }
        return std::nullopt;
    }
    
    // Return buffer to free pool
    void return_buffer(DMABuffer* buf) {
        xQueueSend(free_queue_, &buf, portMAX_DELAY);
    }
    
private:
    void start_dma_transfer(uint8_t* addr, size_t len) {
        // Platform-specific DMA initiation
    }
};

// Usage example
extern "C" void producer_task(void* params) {
    auto& dma = *static_cast<DMAManager*>(params);
    
    while (true) {
        // Get free buffer (zero-copy)
        auto buf_opt = dma.get_free_buffer();
        if (!buf_opt) continue;
        
        DMABuffer* buf = *buf_opt;
        
        // Fill buffer with data
        auto span = buf->get_span();
        size_t len = fill_with_sensor_data(span.data(), span.size());
        buf->set_length(len);
        
        // Submit for DMA (zero-copy transfer)
        dma.transmit(buf);
    }
}

extern "C" void consumer_task(void* params) {
    auto& dma = *static_cast<DMAManager*>(params);
    
    while (true) {
        // Get filled buffer (zero-copy)
        auto buf_opt = dma.get_filled_buffer();
        if (!buf_opt) continue;
        
        DMABuffer* buf = *buf_opt;
        
        // Process data directly from DMA buffer
        auto span = buf->get_span();
        process_data(span);
        
        // Return to pool
        dma.return_buffer(buf);
    }
}
```

## Rust Implementation

```rust
#![no_std]

use core::sync::atomic::{AtomicBool, Ordering};
use core::cell::UnsafeCell;
use freertos_rust::*;

// Cache-aligned DMA buffer
#[repr(align(32))]
struct CacheAligned<T>(T);

// DMA buffer with ownership tracking
struct DmaBuffer {
    data: CacheAligned<[u8; 512]>,
    length: usize,
    owned_by_dma: AtomicBool,
}

impl DmaBuffer {
    const fn new() -> Self {
        Self {
            data: CacheAligned([0u8; 512]),
            length: 0,
            owned_by_dma: AtomicBool::new(false),
        }
    }
    
    fn acquire(&self) -> bool {
        self.owned_by_dma
            .compare_exchange(true, false, Ordering::Acquire, Ordering::Relaxed)
            .is_ok()
    }
    
    fn release_to_dma(&self) {
        self.owned_by_dma.store(true, Ordering::Release);
    }
    
    fn as_slice(&self) -> &[u8] {
        &self.data.0[..self.length]
    }
    
    fn as_mut_slice(&mut self) -> &mut [u8] {
        &mut self.data.0[..self.length]
    }
    
    fn data_ptr(&self) -> *const u8 {
        self.data.0.as_ptr()
    }
    
    fn data_mut_ptr(&mut self) -> *mut u8 {
        self.data.0.as_mut_ptr()
    }
}

// Safe wrapper for cache operations
mod cache {
    pub unsafe fn clean(addr: *const u8, size: usize) {
        #[cfg(target_arch = "arm")]
        {
            let start = (addr as usize) & !31;
            let end = (addr as usize) + size;
            
            for va in (start..end).step_by(32) {
                core::arch::asm!(
                    "dc cvac, {0}",
                    in(reg) va,
                    options(nostack, preserves_flags)
                );
            }
            
            core::arch::asm!("dsb", options(nostack, preserves_flags));
        }
    }
    
    pub unsafe fn invalidate(addr: *const u8, size: usize) {
        #[cfg(target_arch = "arm")]
        {
            let start = (addr as usize) & !31;
            let end = (addr as usize) + size;
            
            for va in (start..end).step_by(32) {
                core::arch::asm!(
                    "dc ivac, {0}",
                    in(reg) va,
                    options(nostack, preserves_flags)
                );
            }
            
            core::arch::asm!("dsb", options(nostack, preserves_flags));
        }
    }
}

// Zero-copy DMA manager
struct DmaManager {
    buffers: [DmaBuffer; 4],
    free_queue: Queue<*mut DmaBuffer>,
    filled_queue: Queue<*mut DmaBuffer>,
    semaphore: Semaphore,
}

impl DmaManager {
    fn new() -> Self {
        let mut manager = Self {
            buffers: [
                DmaBuffer::new(),
                DmaBuffer::new(),
                DmaBuffer::new(),
                DmaBuffer::new(),
            ],
            free_queue: Queue::new(4).unwrap(),
            filled_queue: Queue::new(4).unwrap(),
            semaphore: Semaphore::new_binary().unwrap(),
        };
        
        // Add all buffers to free queue
        for buf in &mut manager.buffers {
            manager.free_queue.send(buf as *mut DmaBuffer, Duration::zero()).ok();
        }
        
        manager
    }
    
    fn get_free_buffer(&self, timeout: Duration) -> Option<&mut DmaBuffer> {
        self.free_queue
            .receive(timeout)
            .ok()
            .map(|ptr| unsafe { &mut *ptr })
    }
    
    fn transmit(&self, buf: &mut DmaBuffer) -> Result<(), ()> {
        self.semaphore.take(Duration::infinite()).ok()?;
        
        buf.release_to_dma();
        
        // Clean cache before DMA reads
        unsafe {
            cache::clean(buf.data_ptr(), buf.length);
        }
        
        // Start DMA transfer (platform-specific)
        unsafe {
            start_dma_transfer(buf.data_ptr(), buf.length);
        }
        
        self.semaphore.give();
        Ok(())
    }
    
    fn on_transfer_complete(&self, buf: &mut DmaBuffer) {
        // Invalidate cache after DMA writes
        unsafe {
            cache::invalidate(buf.data_ptr(), buf.length);
        }
        
        // Send to filled queue
        self.filled_queue.send(buf as *mut DmaBuffer, Duration::zero()).ok();
    }
    
    fn get_filled_buffer(&self, timeout: Duration) -> Option<&mut DmaBuffer> {
        self.filled_queue
            .receive(timeout)
            .ok()
            .map(|ptr| unsafe { &mut *ptr })
    }
    
    fn return_buffer(&self, buf: &mut DmaBuffer) {
        self.free_queue.send(buf as *mut DmaBuffer, Duration::zero()).ok();
    }
}

// Safe static instance with interior mutability
static DMA_MANAGER: UnsafeCell<Option<DmaManager>> = UnsafeCell::new(None);

fn init_dma() {
    unsafe {
        *DMA_MANAGER.get() = Some(DmaManager::new());
    }
}

fn get_dma_manager() -> &'static DmaManager {
    unsafe {
        (*DMA_MANAGER.get()).as_ref().unwrap()
    }
}

// Producer task
fn producer_task(_: TaskParameters) {
    let dma = get_dma_manager();
    
    loop {
        // Get free buffer (zero-copy)
        if let Some(buf) = dma.get_free_buffer(Duration::infinite()) {
            // Fill buffer with data
            let data = fill_sensor_data(buf.as_mut_slice());
            
            // Submit for DMA transfer
            dma.transmit(buf).ok();
        }
    }
}

// Consumer task
fn consumer_task(_: TaskParameters) {
    let dma = get_dma_manager();
    
    loop {
        // Get filled buffer (zero-copy)
        if let Some(buf) = dma.get_filled_buffer(Duration::infinite()) {
            // Process data directly from DMA buffer
            process_data(buf.as_slice());
            
            // Return buffer to pool
            dma.return_buffer(buf);
        }
    }
}

// DMA interrupt handler
#[no_mangle]
extern "C" fn DMA_IRQHandler() {
    let dma = get_dma_manager();
    
    // Get completed buffer (platform-specific)
    if let Some(buf_ptr) = get_completed_dma_buffer() {
        let buf = unsafe { &mut *buf_ptr };
        dma.on_transfer_complete(buf);
    }
}

// Platform-specific functions (stubs)
unsafe fn start_dma_transfer(_addr: *const u8, _len: usize) {
    // Configure DMA controller registers
}

fn get_completed_dma_buffer() -> Option<*mut DmaBuffer> {
    // Read from DMA status registers
    None
}

fn fill_sensor_data(_buf: &mut [u8]) -> usize {
    // Fill with actual sensor data
    0
}

fn process_data(_data: &[u8]) {
    // Process received data
}
```

## Summary

DMA integration with FreeRTOS requires careful attention to:

**Cache Coherency:**
- Align buffers to cache line boundaries (typically 32 bytes)
- Clean cache before DMA reads data from memory
- Invalidate cache after DMA writes data to memory
- Use uncached memory regions for critical buffers on supported platforms

**Thread Safety:**
- Track buffer ownership with atomic flags or semaphores
- Protect DMA configuration with mutexes
- Use ISR-safe primitives for DMA completion signaling
- Employ queue-based buffer management for handoff

**Zero-Copy Optimization:**
- Eliminate intermediate copies by direct buffer sharing
- Use double/circular buffering for continuous streams
- Implement buffer pools to avoid dynamic allocation
- Transfer buffer ownership rather than copying data

**Performance Benefits:**
- Reduced CPU load (DMA operates independently)
- Lower latency (no copy overhead)
- Higher throughput (concurrent operation)
- Deterministic timing (offload to hardware)

**Common Pitfalls:**
- Forgetting cache operations leads to data corruption
- Buffer reuse before DMA completion causes race conditions
- Unaligned buffers waste cache lines and reduce efficiency
- Missing memory barriers on weakly-ordered architectures

The examples demonstrate practical patterns for STM32-class microcontrollers with Cortex-M cores, but the principles apply broadly across embedded platforms. Always consult your specific hardware reference manual for DMA controller details and cache architecture.