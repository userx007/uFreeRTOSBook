# Implementing GPIO toggle macros for logic analyzer debugging in both C and Rust

## C Implementation

**Option 1: Direct Register Access (Most Efficient)**

```c
// config.h - Compile-time configuration
#ifdef DEBUG_LOGIC_ANALYZER
    #define LA_ENABLED 1
    // Define your GPIO port and pins
    #define LA_PORT GPIOA
    #define LA_PIN_TASK1 (1 << 0)
    #define LA_PIN_TASK2 (1 << 1)
    #define LA_PIN_ISR   (1 << 2)
#else
    #define LA_ENABLED 0
#endif

// Macros for toggling
#if LA_ENABLED
    #define LA_SET(pin)   do { LA_PORT->BSRR = (pin); } while(0)
    #define LA_CLEAR(pin) do { LA_PORT->BSRR = (pin) << 16; } while(0)
    #define LA_TOGGLE(pin) do { LA_PORT->ODR ^= (pin); } while(0)
    
    // Scope macros for automatic toggle on entry/exit
    #define LA_SCOPE(pin) \
        LA_SET(pin); \
        for(int _la_i = 1; _la_i; _la_i = 0, LA_CLEAR(pin))
#else
    #define LA_SET(pin)
    #define LA_CLEAR(pin)
    #define LA_TOGGLE(pin)
    #define LA_SCOPE(pin)
#endif
```

**Usage:**

```c
void task_function(void) {
    LA_SCOPE(LA_PIN_TASK1) {
        // Your task code here
        // Pin automatically cleared on exit
    }
}

void interrupt_handler(void) {
    LA_SET(LA_PIN_ISR);
    // Handle interrupt
    LA_CLEAR(LA_PIN_ISR);
}
```

**Option 2: Inline Functions (Better Type Safety)**

```c
#if LA_ENABLED
static inline void la_set(uint32_t pin) {
    LA_PORT->BSRR = pin;
}

static inline void la_clear(uint32_t pin) {
    LA_PORT->BSRR = pin << 16;
}
#else
static inline void la_set(uint32_t pin) { (void)pin; }
static inline void la_clear(uint32_t pin) { (void)pin; }
#endif
```

## Rust Implementation

**Option 1: Zero-Cost Abstractions with Const Generics**

```rust
// In your PAC or HAL wrapper
pub struct LogicAnalyzerPin<const PIN: u8> {
    // Phantom data or actual GPIO reference
}

#[cfg(feature = "logic-analyzer")]
impl<const PIN: u8> LogicAnalyzerPin<PIN> {
    #[inline(always)]
    pub fn set(&self) {
        unsafe {
            (*GPIOA::ptr()).bsrr.write(|w| w.bits(1 << PIN));
        }
    }
    
    #[inline(always)]
    pub fn clear(&self) {
        unsafe {
            (*GPIOA::ptr()).bsrr.write(|w| w.bits(1 << (PIN + 16)));
        }
    }
    
    #[inline(always)]
    pub fn toggle(&self) {
        unsafe {
            let odr = &(*GPIOA::ptr()).odr;
            odr.modify(|r, w| w.bits(r.bits() ^ (1 << PIN)));
        }
    }
}

#[cfg(not(feature = "logic-analyzer"))]
impl<const PIN: u8> LogicAnalyzerPin<PIN> {
    #[inline(always)]
    pub fn set(&self) {}
    
    #[inline(always)]
    pub fn clear(&self) {}
    
    #[inline(always)]
    pub fn toggle(&self) {}
}
```

**Option 2: Macro-Based Approach**

```rust
#[cfg(feature = "logic-analyzer")]
#[macro_export]
macro_rules! la_set {
    ($pin:expr) => {
        unsafe {
            (*GPIOA::ptr()).bsrr.write(|w| w.bits($pin));
        }
    };
}

#[cfg(not(feature = "logic-analyzer"))]
#[macro_export]
macro_rules! la_set {
    ($pin:expr) => {};
}

// RAII guard for automatic cleanup
#[cfg(feature = "logic-analyzer")]
pub struct LaScope {
    pin: u32,
}

#[cfg(feature = "logic-analyzer")]
impl LaScope {
    #[inline(always)]
    pub fn new(pin: u32) -> Self {
        la_set!(pin);
        Self { pin }
    }
}

#[cfg(feature = "logic-analyzer")]
impl Drop for LaScope {
    #[inline(always)]
    fn drop(&mut self) {
        la_clear!(self.pin);
    }
}
```

**Usage:**

```rust
fn task_function() {
    let _scope = LaScope::new(LA_PIN_TASK1);
    // Task code here
    // Pin automatically cleared when _scope drops
}
```

## Key Optimization Strategies

1. **Conditional Compilation**: Use preprocessor directives (C) or cargo features (Rust) to completely eliminate overhead in release builds

2. **Inline Everything**: Use `inline` or `#[inline(always)]` to ensure no function call overhead

3. **Direct Register Access**: Use BSRR (Bit Set/Reset Register) instead of read-modify-write for atomic operations

4. **Pin Assignment Strategy**: 
   - Assign related tasks to adjacent pins for easier viewing
   - Reserve specific pins for ISRs, context switches, and critical sections

5. **Timestamp Macros**: Add a pulse macro for events:
```c
#define LA_PULSE(pin) do { LA_SET(pin); LA_CLEAR(pin); } while(0)
```

6. **Channel Mapping Document**: Keep a comment block mapping pins to functions:
```c
// CH0 (PA0): Main task
// CH1 (PA1): Communication task  
// CH2 (PA2): ISR entry
// CH3 (PA3): Context switch
```

The compile-time conditional approach ensures zero overhead when disabled, while the scope-based pattern prevents forgetting to clear pins after setting them.