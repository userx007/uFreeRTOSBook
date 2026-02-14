# FreeRTOS Interrupt Priority Issues, particularly focusing on ARM Cortex-M NVIC configuration.

## Detailed Description

### Understanding Interrupt Priorities in FreeRTOS

Interrupt priority configuration is one of the most critical—and frequently misconfigured—aspects of FreeRTOS systems, especially on ARM Cortex-M processors. The complexity arises from the interaction between the hardware's Nested Vectored Interrupt Controller (NVIC), FreeRTOS's kernel protection mechanisms, and application-level interrupt handlers.

### The Core Problem

FreeRTOS uses a critical section mechanism to protect kernel data structures from corruption. On ARM Cortex-M processors, this is implemented using the `configMAX_SYSCALL_INTERRUPT_PRIORITY` setting, which defines a priority threshold. Interrupts with priorities **numerically higher** than this threshold cannot safely call FreeRTOS API functions, as they would bypass the kernel's protection mechanisms.

### ARM Cortex-M Priority Numbering

ARM Cortex-M processors use an inverted priority scheme that often confuses developers:

- **Lower numerical values = Higher logical priority**
- **Higher numerical values = Lower logical priority**
- Priority 0 is the highest priority
- The number of priority bits varies by implementation (typically 3-8 bits)

Additionally, ARM implements **priority grouping**, where priority bits are split into:
- **Preemption priority**: Determines whether an interrupt can preempt another
- **Sub-priority**: Used as a tie-breaker when multiple interrupts are pending

### FreeRTOS Priority Constraints

FreeRTOS divides interrupts into two categories:

1. **FreeRTOS-safe interrupts**: Priorities at or below (numerically higher than) `configMAX_SYSCALL_INTERRUPT_PRIORITY`. These can call FreeRTOS API functions ending in `FromISR()`.

2. **Ultra-high-priority interrupts**: Priorities above (numerically lower than) `configMAX_SYSCALL_INTERRUPT_PRIORITY`. These cannot call any FreeRTOS APIs but are never disabled by FreeRTOS, ensuring minimal latency.

### Common Configuration Issues

1. **Incorrect `configMAX_SYSCALL_INTERRUPT_PRIORITY` value**: Not accounting for the number of implemented priority bits
2. **Using unshifted priority values**: Forgetting that ARM only implements the upper bits
3. **Calling FreeRTOS APIs from high-priority interrupts**: Violating the safety boundary
4. **Not configuring `configKERNEL_INTERRUPT_PRIORITY`**: Required for proper tick interrupt operation
5. **Priority inversion**: Setting peripheral interrupts higher than the maximum safe priority when they need to use FreeRTOS APIs

---

## C/C++ Programming Examples

### Example 1: Correct Priority Configuration (FreeRTOSConfig.h)

```c
/* ARM Cortex-M specific configuration */

/* The number of priority bits implemented in hardware.
   Check your MCU's datasheet - common values: 3, 4, or 8 bits */
#define configPRIO_BITS 4  // Example: STM32F4 has 4 bits

/* The lowest interrupt priority that can call FreeRTOS APIs.
   Must be shifted left as ARM only uses upper bits.
   Typical value: 5 for 4-bit implementation */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << (8 - configPRIO_BITS))

/* Kernel interrupt priority - should be set to lowest priority (highest number) */
#define configKERNEL_INTERRUPT_PRIORITY (15 << (8 - configPRIO_BITS))

/* Enable interrupt priority assertions in debug builds */
#define configASSERT(x) if((x) == 0) { taskDISABLE_INTERRUPTS(); for(;;); }
```

### Example 2: Configuring NVIC Priorities Correctly

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Example: UART interrupt that uses FreeRTOS APIs */
void setup_uart_interrupt(void)
{
    /* Calculate safe priority for FreeRTOS API calls
       Priority 6 is numerically higher than configMAX_SYSCALL_INTERRUPT_PRIORITY (5)
       so it's safe to call FromISR functions */
    uint32_t safe_priority = 6;
    
    /* CRITICAL: Shift the priority value to account for unimplemented bits */
    uint32_t nvic_priority = safe_priority << (8 - configPRIO_BITS);
    
    /* Configure NVIC using CMSIS functions */
    NVIC_SetPriority(USART1_IRQn, nvic_priority);
    NVIC_EnableIRQ(USART1_IRQn);
}

/* Example: Ultra-high-priority interrupt that cannot use FreeRTOS APIs */
void setup_critical_timer(void)
{
    /* Priority 2 is numerically lower than configMAX_SYSCALL_INTERRUPT_PRIORITY (5)
       This interrupt will NEVER be masked by FreeRTOS - ultra-low latency
       But CANNOT call any FreeRTOS functions */
    uint32_t critical_priority = 2;
    uint32_t nvic_priority = critical_priority << (8 - configPRIO_BITS);
    
    NVIC_SetPriority(TIM2_IRQn, nvic_priority);
    NVIC_EnableIRQ(TIM2_IRQn);
}
```

### Example 3: Safe ISR with FreeRTOS API Calls

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Queue and semaphore handles */
QueueHandle_t uart_queue;
SemaphoreHandle_t data_ready_sem;

/* UART ISR - priority configured to allow FreeRTOS API calls */
void USART1_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    char received_byte;
    
    /* Check if RX interrupt */
    if (USART1->SR & USART_SR_RXNE)
    {
        received_byte = USART1->DR;
        
        /* Send to queue from ISR - safe because priority is configured correctly */
        xQueueSendFromISR(uart_queue, &received_byte, &xHigherPriorityTaskWoken);
        
        /* Give semaphore to wake processing task */
        xSemaphoreGiveFromISR(data_ready_sem, &xHigherPriorityTaskWoken);
    }
    
    /* Request context switch if higher priority task was woken */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Ultra-high-priority ISR - NO FreeRTOS calls allowed */
static volatile uint32_t timer_count = 0;
static volatile bool emergency_flag = false;

void TIM2_IRQHandler(void)
{
    /* This ISR has priority 2 - higher than configMAX_SYSCALL_INTERRUPT_PRIORITY
       Cannot call ANY FreeRTOS functions, but has guaranteed low latency */
    
    if (TIM2->SR & TIM_SR_UIF)
    {
        TIM2->SR = ~TIM_SR_UIF;  // Clear interrupt
        
        timer_count++;
        
        /* Direct hardware access only - no OS calls */
        if (timer_count > 1000)
        {
            GPIOA->BSRR = GPIO_BSRR_BS5;  // Emergency LED on
            emergency_flag = true;
        }
    }
}
```

### Example 4: Debugging Priority Issues

```c
#include "FreeRTOS.h"
#include "task.h"

/* Helper function to validate interrupt priorities at runtime */
void validate_interrupt_priority(IRQn_Type IRQn, const char* name)
{
    uint32_t priority = NVIC_GetPriority(IRQn);
    uint32_t max_syscall_priority = configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8 - configPRIO_BITS);
    
    printf("Interrupt %s (IRQn=%d):\n", name, IRQn);
    printf("  Priority value: %lu\n", priority);
    printf("  Max syscall priority: %lu\n", max_syscall_priority);
    
    if (priority < max_syscall_priority)
    {
        printf("  WARNING: Priority too high for FreeRTOS API calls!\n");
        printf("  This interrupt cannot safely call FromISR functions.\n");
    }
    else
    {
        printf("  OK: Can safely call FreeRTOS FromISR functions.\n");
    }
}

/* Task to check priority configuration at startup */
void vPriorityCheckTask(void *pvParameters)
{
    printf("\n=== FreeRTOS Interrupt Priority Check ===\n");
    printf("configPRIO_BITS: %d\n", configPRIO_BITS);
    printf("configMAX_SYSCALL_INTERRUPT_PRIORITY: 0x%02X\n", 
           configMAX_SYSCALL_INTERRUPT_PRIORITY);
    printf("configKERNEL_INTERRUPT_PRIORITY: 0x%02X\n\n",
           configKERNEL_INTERRUPT_PRIORITY);
    
    /* Check specific interrupts */
    validate_interrupt_priority(USART1_IRQn, "USART1");
    validate_interrupt_priority(TIM2_IRQn, "TIM2");
    validate_interrupt_priority(DMA1_Stream0_IRQn, "DMA1_Stream0");
    
    /* This task only runs once */
    vTaskDelete(NULL);
}
```

---

## Rust Programming Examples

### Example 1: Type-Safe Priority Configuration

```rust
// Type-safe interrupt priority wrapper for ARM Cortex-M
use cortex_m::peripheral::NVIC;
use cortex_m::interrupt;

/// Number of priority bits implemented in hardware
const PRIO_BITS: u8 = 4; // STM32F4 example

/// Maximum priority that can call FreeRTOS APIs (shifted value)
const MAX_SYSCALL_PRIORITY: u8 = 5 << (8 - PRIO_BITS);

/// Kernel interrupt priority (lowest logical priority)
const KERNEL_PRIORITY: u8 = 15 << (8 - PRIO_BITS);

/// Type-safe interrupt priority levels
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum InterruptPriority {
    /// Ultra-high priority (0-4) - Cannot call FreeRTOS APIs
    UltraHigh(u8),
    /// FreeRTOS-safe priority (5-15) - Can call FromISR functions
    FreeRtosSafe(u8),
}

impl InterruptPriority {
    /// Create a new priority, validating the value
    pub fn new(priority: u8) -> Result<Self, &'static str> {
        if priority > 15 {
            return Err("Priority must be 0-15");
        }
        
        let max_syscall = MAX_SYSCALL_PRIORITY >> (8 - PRIO_BITS);
        
        if priority < max_syscall {
            Ok(InterruptPriority::UltraHigh(priority))
        } else {
            Ok(InterruptPriority::FreeRtosSafe(priority))
        }
    }
    
    /// Get the NVIC-compatible priority value (shifted)
    pub fn to_nvic_priority(&self) -> u8 {
        let raw_priority = match self {
            InterruptPriority::UltraHigh(p) => *p,
            InterruptPriority::FreeRtosSafe(p) => *p,
        };
        raw_priority << (8 - PRIO_BITS)
    }
    
    /// Check if this priority can safely call FreeRTOS APIs
    pub fn can_call_freertos(&self) -> bool {
        matches!(self, InterruptPriority::FreeRtosSafe(_))
    }
}

/// Configure an interrupt with type-safe priority
pub fn configure_interrupt<I>(
    nvic: &mut NVIC,
    interrupt: I,
    priority: InterruptPriority
) -> Result<(), &'static str>
where
    I: cortex_m::interrupt::InterruptNumber,
{
    unsafe {
        // Set priority
        nvic.set_priority(interrupt, priority.to_nvic_priority());
        
        // Enable interrupt
        NVIC::unmask(interrupt);
    }
    
    Ok(())
}
```

### Example 2: Safe UART ISR with FreeRTOS Integration

```rust
use freertos_rust::{FreeRtosError, Queue, BinarySemaphore};
use cortex_m::interrupt;

// Static handles (in real code, use proper initialization)
static mut UART_QUEUE: Option<Queue<u8>> = None;
static mut DATA_READY_SEM: Option<BinarySemaphore> = None;

/// Initialize UART with proper priority
pub fn init_uart() -> Result<(), &'static str> {
    // Create queue and semaphore
    unsafe {
        UART_QUEUE = Some(Queue::new(128)?);
        DATA_READY_SEM = Some(BinarySemaphore::new()?);
    }
    
    // Configure interrupt with FreeRTOS-safe priority
    let priority = InterruptPriority::new(6)?; // Priority 6 is safe
    
    cortex_m::interrupt::free(|_cs| {
        let mut nvic = unsafe { cortex_m::Peripherals::steal().NVIC };
        configure_interrupt(&mut nvic, stm32f4::Interrupt::USART1, priority)?;
    });
    
    Ok(())
}

/// UART interrupt handler
#[interrupt]
fn USART1() {
    // Access peripherals
    let usart = unsafe { &(*stm32f4::USART1::ptr()) };
    
    // Check if RX interrupt
    if usart.sr.read().rxne().bit_is_set() {
        let received_byte = usart.dr.read().bits() as u8;
        
        unsafe {
            if let Some(queue) = &mut UART_QUEUE {
                // Send to queue - safe because we configured priority correctly
                let _ = queue.send_from_isr(received_byte);
            }
            
            if let Some(sem) = &mut DATA_READY_SEM {
                // Give semaphore to wake task
                let _ = sem.give_from_isr();
            }
        }
    }
}
```

### Example 3: Ultra-High-Priority Timer ISR

```rust
use core::sync::atomic::{AtomicU32, AtomicBool, Ordering};
use cortex_m::interrupt;

// Shared data using atomics (no OS primitives in high-priority ISR)
static TIMER_COUNT: AtomicU32 = AtomicU32::new(0);
static EMERGENCY_FLAG: AtomicBool = AtomicBool::new(false);

/// Initialize critical timer with ultra-high priority
pub fn init_critical_timer() -> Result<(), &'static str> {
    // Configure with priority 2 - higher than FreeRTOS threshold
    // This interrupt will NEVER be disabled by FreeRTOS
    let priority = InterruptPriority::new(2)?;
    
    cortex_m::interrupt::free(|_cs| {
        let mut nvic = unsafe { cortex_m::Peripherals::steal().NVIC };
        configure_interrupt(&mut nvic, stm32f4::Interrupt::TIM2, priority)?;
    });
    
    Ok(())
}

/// Timer interrupt handler - NO FreeRTOS calls allowed!
#[interrupt]
fn TIM2() {
    let tim = unsafe { &(*stm32f4::TIM2::ptr()) };
    
    // Check and clear interrupt flag
    if tim.sr.read().uif().bit_is_set() {
        tim.sr.modify(|_, w| w.uif().clear_bit());
        
        // Only atomic operations and direct hardware access
        let count = TIMER_COUNT.fetch_add(1, Ordering::Relaxed);
        
        if count > 1000 {
            // Direct GPIO access for emergency LED
            let gpioa = unsafe { &(*stm32f4::GPIOA::ptr()) };
            gpioa.bsrr.write(|w| unsafe { w.bits(1 << 5) });
            
            EMERGENCY_FLAG.store(true, Ordering::Release);
        }
    }
}

/// Task to monitor emergency flag (runs in FreeRTOS context)
pub fn emergency_monitor_task() {
    loop {
        if EMERGENCY_FLAG.load(Ordering::Acquire) {
            // Handle emergency condition
            // Can safely use FreeRTOS APIs here
            handle_emergency();
            EMERGENCY_FLAG.store(false, Ordering::Release);
            TIMER_COUNT.store(0, Ordering::Relaxed);
        }
        
        freertos_rust::CurrentTask::delay(Duration::ms(10));
    }
}

fn handle_emergency() {
    // Emergency handling logic
}
```

### Example 4: Priority Validation and Debugging

```rust
use cortex_m::peripheral::NVIC;

/// Validate interrupt configuration at runtime
pub struct PriorityValidator;

impl PriorityValidator {
    /// Check if an interrupt is properly configured
    pub fn validate<I>(interrupt: I, name: &str) -> Result<(), String>
    where
        I: cortex_m::interrupt::InterruptNumber,
    {
        let priority = unsafe { NVIC::get_priority(interrupt) };
        let max_syscall = MAX_SYSCALL_PRIORITY >> (8 - PRIO_BITS);
        let raw_priority = priority >> (8 - PRIO_BITS);
        
        println!("\n=== Interrupt: {} ===", name);
        println!("IRQ Number: {}", interrupt.number());
        println!("Priority (raw): {}", raw_priority);
        println!("Priority (NVIC): 0x{:02X}", priority);
        println!("Max syscall priority: {}", max_syscall);
        
        if raw_priority < max_syscall {
            let msg = format!(
                "WARNING: {} priority ({}) is too high for FreeRTOS API calls!",
                name, raw_priority
            );
            println!("{}", msg);
            Err(msg)
        } else {
            println!("OK: Can safely call FreeRTOS FromISR functions");
            Ok(())
        }
    }
    
    /// Comprehensive system check
    pub fn check_system() {
        println!("\n========== FreeRTOS Priority Configuration ==========");
        println!("Priority bits: {}", PRIO_BITS);
        println!("Max syscall priority: 0x{:02X}", MAX_SYSCALL_PRIORITY);
        println!("Kernel priority: 0x{:02X}", KERNEL_PRIORITY);
        println!("====================================================\n");
        
        // Check specific interrupts
        let _ = Self::validate(stm32f4::Interrupt::USART1, "USART1");
        let _ = Self::validate(stm32f4::Interrupt::TIM2, "TIM2");
        let _ = Self::validate(stm32f4::Interrupt::DMA1_STREAM0, "DMA1_Stream0");
    }
}
```

---

## Summary

**Interrupt priority configuration** is a critical aspect of FreeRTOS development on ARM Cortex-M processors that requires careful attention to:

1. **Priority numbering**: Remember that lower numbers mean higher priority on ARM architectures

2. **Bit shifting**: ARM only implements the upper priority bits, requiring proper shifting of priority values

3. **The safety boundary**: `configMAX_SYSCALL_INTERRUPT_PRIORITY` divides interrupts into FreeRTOS-safe (can call `FromISR` functions) and ultra-high-priority (cannot call any FreeRTOS APIs)

4. **Configuration values**: Both `configMAX_SYSCALL_INTERRUPT_PRIORITY` and `configKERNEL_INTERRUPT_PRIORITY` must be properly configured based on the number of implemented priority bits

5. **Common mistakes**: Incorrectly configured priorities lead to hard-to-debug crashes, race conditions, and system instability

**Best practices** include using type-safe wrappers (especially in Rust), validating priorities at startup, clearly documenting which interrupts can use FreeRTOS APIs, and using ultra-high-priority interrupts sparingly for truly time-critical operations. Proper interrupt priority configuration ensures both system safety and deterministic real-time behavior.