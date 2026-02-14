# Cross-Platform Development in FreeRTOS

## Overview

Cross-platform development in FreeRTOS involves creating portable real-time applications that can run on different microcontroller architectures (ARM Cortex-M, RISC-V, ESP32, etc.) with minimal code changes. This approach abstracts hardware-specific details behind consistent interfaces, enabling code reuse and easier platform migration.

## Key Concepts

### 1. **Hardware Abstraction Layer (HAL)**
A software layer that isolates hardware-specific code from application logic, providing uniform APIs across different platforms.

### 2. **Portability Layers**
- **FreeRTOS Port Layer**: Architecture-specific implementations (task switching, interrupts)
- **Driver Abstraction**: Unified interfaces for peripherals (GPIO, UART, SPI, I2C)
- **Board Support Package (BSP)**: Platform-specific initialization and configuration

### 3. **Compilation Strategy**
Using conditional compilation, separate source files, and build systems to manage platform differences.

---

## C/C++ Implementation

### Hardware Abstraction Layer Example

**hal_gpio.h** (Platform-independent interface)
```c
#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_INPUT_PULLUP,
    GPIO_MODE_INPUT_PULLDOWN
} gpio_mode_t;

typedef enum {
    GPIO_LOW = 0,
    GPIO_HIGH = 1
} gpio_state_t;

// Platform-independent GPIO API
bool hal_gpio_init(uint8_t pin, gpio_mode_t mode);
void hal_gpio_write(uint8_t pin, gpio_state_t state);
gpio_state_t hal_gpio_read(uint8_t pin);
void hal_gpio_toggle(uint8_t pin);

#endif // HAL_GPIO_H
```

**hal_gpio_stm32.c** (STM32 implementation)
```c
#include "hal_gpio.h"
#include "stm32f4xx_hal.h"

// Pin mapping structure
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} gpio_pin_map_t;

static gpio_pin_map_t pin_map[] = {
    {GPIOA, GPIO_PIN_0},
    {GPIOA, GPIO_PIN_1},
    // ... more mappings
};

bool hal_gpio_init(uint8_t pin, gpio_mode_t mode) {
    if (pin >= sizeof(pin_map) / sizeof(pin_map[0])) {
        return false;
    }
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin_map[pin].pin;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    switch (mode) {
        case GPIO_MODE_INPUT:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            break;
        case GPIO_MODE_OUTPUT:
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
            GPIO_InitStruct.Pull = GPIO_NOPULL;
            break;
        case GPIO_MODE_INPUT_PULLUP:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_PULLUP;
            break;
        case GPIO_MODE_INPUT_PULLDOWN:
            GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
            GPIO_InitStruct.Pull = GPIO_PULLDOWN;
            break;
    }
    
    HAL_GPIO_Init(pin_map[pin].port, &GPIO_InitStruct);
    return true;
}

void hal_gpio_write(uint8_t pin, gpio_state_t state) {
    HAL_GPIO_WritePin(pin_map[pin].port, pin_map[pin].pin, 
                      state == GPIO_HIGH ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

gpio_state_t hal_gpio_read(uint8_t pin) {
    return HAL_GPIO_ReadPin(pin_map[pin].port, pin_map[pin].pin) == GPIO_PIN_SET 
           ? GPIO_HIGH : GPIO_LOW;
}

void hal_gpio_toggle(uint8_t pin) {
    HAL_GPIO_TogglePin(pin_map[pin].port, pin_map[pin].pin);
}
```

**hal_gpio_esp32.c** (ESP32 implementation)
```c
#include "hal_gpio.h"
#include "driver/gpio.h"

bool hal_gpio_init(uint8_t pin, gpio_mode_t mode) {
    gpio_config_t io_conf = {0};
    io_conf.pin_bit_mask = (1ULL << pin);
    
    switch (mode) {
        case GPIO_MODE_INPUT:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            break;
        case GPIO_MODE_OUTPUT:
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            break;
        case GPIO_MODE_INPUT_PULLUP:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            break;
        case GPIO_MODE_INPUT_PULLDOWN:
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
            break;
    }
    
    io_conf.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&io_conf) == ESP_OK;
}

void hal_gpio_write(uint8_t pin, gpio_state_t state) {
    gpio_set_level(pin, state);
}

gpio_state_t hal_gpio_read(uint8_t pin) {
    return gpio_get_level(pin) ? GPIO_HIGH : GPIO_LOW;
}

void hal_gpio_toggle(uint8_t pin) {
    gpio_set_level(pin, !gpio_get_level(pin));
}
```

### Platform-Independent Application

```c
#include "FreeRTOS.h"
#include "task.h"
#include "hal_gpio.h"

// Application configuration (platform-specific values in separate config files)
#define LED_PIN 13
#define BUTTON_PIN 2

void vBlinkTask(void *pvParameters) {
    hal_gpio_init(LED_PIN, GPIO_MODE_OUTPUT);
    
    while (1) {
        hal_gpio_toggle(LED_PIN);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vButtonTask(void *pvParameters) {
    hal_gpio_init(BUTTON_PIN, GPIO_MODE_INPUT_PULLUP);
    gpio_state_t last_state = GPIO_HIGH;
    
    while (1) {
        gpio_state_t current_state = hal_gpio_read(BUTTON_PIN);
        
        if (current_state == GPIO_LOW && last_state == GPIO_HIGH) {
            // Button pressed
            configPRINTF(("Button pressed!\r\n"));
        }
        
        last_state = current_state;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

int main(void) {
    // Platform-specific initialization
    #ifdef PLATFORM_STM32
        HAL_Init();
        SystemClock_Config();
    #elif defined(PLATFORM_ESP32)
        // ESP32-specific init handled by ESP-IDF
    #endif
    
    xTaskCreate(vBlinkTask, "Blink", 256, NULL, 1, NULL);
    xTaskCreate(vButtonTask, "Button", 256, NULL, 1, NULL);
    
    vTaskStartScheduler();
    
    // Should never reach here
    for (;;);
    return 0;
}
```

### UART Abstraction Layer

**hal_uart.h**
```c
#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    UART_BAUDRATE_9600 = 9600,
    UART_BAUDRATE_115200 = 115200
} uart_baudrate_t;

typedef void (*uart_rx_callback_t)(uint8_t *data, size_t len);

bool hal_uart_init(uint8_t uart_num, uart_baudrate_t baudrate);
bool hal_uart_send(uint8_t uart_num, const uint8_t *data, size_t len);
bool hal_uart_register_rx_callback(uint8_t uart_num, uart_rx_callback_t callback);

#endif // HAL_UART_H
```

### C++ Abstraction with Templates

```cpp
// hal_peripheral.hpp - Generic peripheral interface
template<typename T>
class PeripheralInterface {
public:
    virtual bool init(const T& config) = 0;
    virtual bool deinit() = 0;
    virtual ~PeripheralInterface() = default;
};

// GPIO abstraction
struct GpioConfig {
    uint8_t pin;
    gpio_mode_t mode;
};

class GpioPort : public PeripheralInterface<GpioConfig> {
protected:
    uint8_t pin_;
    
public:
    virtual bool init(const GpioConfig& config) override {
        pin_ = config.pin;
        return hal_gpio_init(config.pin, config.mode);
    }
    
    virtual bool deinit() override { return true; }
    
    void write(gpio_state_t state) {
        hal_gpio_write(pin_, state);
    }
    
    gpio_state_t read() {
        return hal_gpio_read(pin_);
    }
    
    void toggle() {
        hal_gpio_toggle(pin_);
    }
};

// Usage in FreeRTOS task
extern "C" void vLedTask(void *pvParameters) {
    GpioPort led;
    GpioConfig config = {.pin = 13, .mode = GPIO_MODE_OUTPUT};
    
    if (!led.init(config)) {
        vTaskDelete(NULL);
        return;
    }
    
    while (1) {
        led.toggle();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## Rust Implementation

### Rust HAL Traits

**hal_gpio.rs**
```rust
#![no_std]

pub enum GpioMode {
    Input,
    Output,
    InputPullUp,
    InputPullDown,
}

pub enum GpioState {
    Low,
    High,
}

// Platform-independent GPIO trait
pub trait GpioPin {
    type Error;
    
    fn init(&mut self, mode: GpioMode) -> Result<(), Self::Error>;
    fn write(&mut self, state: GpioState) -> Result<(), Self::Error>;
    fn read(&self) -> Result<GpioState, Self::Error>;
    
    fn toggle(&mut self) -> Result<(), Self::Error> {
        let current = self.read()?;
        match current {
            GpioState::Low => self.write(GpioState::High),
            GpioState::High => self.write(GpioState::Low),
        }
    }
}
```

### STM32 Implementation

**stm32_gpio.rs**
```rust
use crate::hal_gpio::{GpioPin, GpioMode, GpioState};
use stm32f4xx_hal::gpio::{Pin, Output, PushPull, Input, PullUp, PullDown};
use stm32f4xx_hal::prelude::*;

pub struct Stm32GpioPin<const P: char, const N: u8> {
    pin: Option<Pin<P, N, Output<PushPull>>>,
}

impl<const P: char, const N: u8> Stm32GpioPin<P, N> {
    pub fn new() -> Self {
        Self { pin: None }
    }
}

// Note: This is simplified - actual implementation would need more type-level programming
impl<const P: char, const N: u8> GpioPin for Stm32GpioPin<P, N> {
    type Error = ();
    
    fn init(&mut self, mode: GpioMode) -> Result<(), Self::Error> {
        // Platform-specific initialization
        // In real code, you'd get the pin from the device peripherals
        Ok(())
    }
    
    fn write(&mut self, state: GpioState) -> Result<(), Self::Error> {
        if let Some(ref mut pin) = self.pin {
            match state {
                GpioState::High => pin.set_high(),
                GpioState::Low => pin.set_low(),
            }
        }
        Ok(())
    }
    
    fn read(&self) -> Result<GpioState, Self::Error> {
        // Implementation for reading
        Ok(GpioState::Low)
    }
}
```

### ESP32 Implementation

**esp32_gpio.rs**
```rust
use crate::hal_gpio::{GpioPin, GpioMode, GpioState};
use esp_idf_hal::gpio::*;

pub struct Esp32GpioPin<T: Pin> {
    pin: Option<PinDriver<'static, T, Output>>,
}

impl<T: Pin> Esp32GpioPin<T> {
    pub fn new() -> Self {
        Self { pin: None }
    }
}

impl<T: Pin + OutputPin> GpioPin for Esp32GpioPin<T> {
    type Error = esp_idf_hal::gpio::EspError;
    
    fn init(&mut self, mode: GpioMode) -> Result<(), Self::Error> {
        // ESP32-specific initialization
        Ok(())
    }
    
    fn write(&mut self, state: GpioState) -> Result<(), Self::Error> {
        if let Some(ref mut pin) = self.pin {
            match state {
                GpioState::High => pin.set_high()?,
                GpioState::Low => pin.set_low()?,
            }
        }
        Ok(())
    }
    
    fn read(&self) -> Result<GpioState, Self::Error> {
        Ok(GpioState::Low)
    }
}
```

### Platform-Independent FreeRTOS Task

**main.rs**
```rust
#![no_std]
#![no_main]

use freertos_rust::*;
use crate::hal_gpio::{GpioPin, GpioMode, GpioState};

// Conditional compilation for different platforms
#[cfg(feature = "stm32")]
use crate::stm32_gpio::Stm32GpioPin as PlatformGpio;

#[cfg(feature = "esp32")]
use crate::esp32_gpio::Esp32GpioPin as PlatformGpio;

fn blink_task<P: GpioPin>(mut led: P) {
    led.init(GpioMode::Output).unwrap();
    
    loop {
        led.toggle().unwrap();
        CurrentTask::delay(Duration::ms(500));
    }
}

fn button_task<P: GpioPin>(mut button: P) {
    button.init(GpioMode::InputPullUp).unwrap();
    let mut last_state = GpioState::High;
    
    loop {
        let current_state = button.read().unwrap();
        
        match (current_state, last_state) {
            (GpioState::Low, GpioState::High) => {
                // Button pressed
                // Handle event
            },
            _ => {}
        }
        
        last_state = current_state;
        CurrentTask::delay(Duration::ms(50));
    }
}

#[no_mangle]
fn app_main() {
    // Platform-specific initialization happens in BSP
    
    let led = PlatformGpio::new();
    let button = PlatformGpio::new();
    
    Task::new()
        .name("Blink")
        .stack_size(2048)
        .priority(TaskPriority(1))
        .start(move || blink_task(led))
        .unwrap();
    
    Task::new()
        .name("Button")
        .stack_size(2048)
        .priority(TaskPriority(1))
        .start(move || button_task(button))
        .unwrap();
    
    FreeRtosUtils::start_scheduler();
}
```

### Advanced Rust: Generic HAL with Associated Types

```rust
pub trait HardwareAbstraction {
    type Gpio: GpioPin;
    type Uart: UartInterface;
    type Timer: TimerInterface;
    
    fn create_gpio(&self, pin: u8) -> Self::Gpio;
    fn create_uart(&self, uart_num: u8) -> Self::Uart;
    fn create_timer(&self, timer_num: u8) -> Self::Timer;
}

// Application code that works across platforms
pub struct SensorController<H: HardwareAbstraction> {
    led: H::Gpio,
    uart: H::Uart,
}

impl<H: HardwareAbstraction> SensorController<H> {
    pub fn new(hal: &H) -> Self {
        Self {
            led: hal.create_gpio(13),
            uart: hal.create_uart(0),
        }
    }
    
    pub fn run(&mut self) {
        self.led.init(GpioMode::Output).ok();
        
        loop {
            self.led.toggle().ok();
            CurrentTask::delay(Duration::ms(1000));
        }
    }
}
```

---

## Build System Configuration

### CMakeLists.txt (Multi-platform)

```cmake
cmake_minimum_required(VERSION 3.15)

# Platform selection
if(NOT DEFINED PLATFORM)
    set(PLATFORM "STM32" CACHE STRING "Target platform")
endif()

project(FreeRTOS_CrossPlatform C CXX)

# Common sources
set(COMMON_SOURCES
    src/main.c
    src/app_tasks.c
)

# HAL interface
set(HAL_INTERFACE
    hal/hal_gpio.h
    hal/hal_uart.h
)

# Platform-specific sources
if(PLATFORM STREQUAL "STM32")
    add_subdirectory(platform/stm32)
    set(PLATFORM_SOURCES
        hal/stm32/hal_gpio_stm32.c
        hal/stm32/hal_uart_stm32.c
    )
    add_definitions(-DPLATFORM_STM32)
    
elseif(PLATFORM STREQUAL "ESP32")
    add_subdirectory(platform/esp32)
    set(PLATFORM_SOURCES
        hal/esp32/hal_gpio_esp32.c
        hal/esp32/hal_uart_esp32.c
    )
    add_definitions(-DPLATFORM_ESP32)
endif()

add_executable(${PROJECT_NAME}
    ${COMMON_SOURCES}
    ${PLATFORM_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
```

### Cargo.toml (Rust)

```toml
[package]
name = "freertos-cross-platform"
version = "0.1.0"
edition = "2021"

[dependencies]
freertos-rust = "0.3"

# Platform-specific dependencies
[target.'cfg(feature = "stm32")'.dependencies]
stm32f4xx-hal = "0.14"

[target.'cfg(feature = "esp32")'.dependencies]
esp-idf-hal = "0.41"

[features]
default = []
stm32 = []
esp32 = []

[profile.release]
opt-level = "z"
lto = true
```

---

## Summary

**Cross-platform development in FreeRTOS** enables:

1. **Code Reusability**: Write application logic once, run on multiple architectures
2. **Maintainability**: Centralized business logic with platform-specific code isolated in HAL
3. **Flexibility**: Easy migration between hardware platforms with minimal refactoring
4. **Testing**: Platform-independent code can be unit tested on host systems

**Key Strategies**:
- **Hardware Abstraction Layers** provide uniform APIs hiding platform differences
- **Conditional compilation** manages platform-specific code paths
- **Build system configuration** (CMake, Cargo) automates multi-platform builds
- **Trait-based design** (Rust) or interfaces (C++) enable polymorphic hardware access

**Best Practices**:
- Keep HAL interfaces minimal and focused
- Use configuration files for platform-specific constants
- Document platform assumptions and limitations
- Implement comprehensive error handling across abstraction boundaries
- Test on all target platforms regularly during development

This approach significantly reduces development time when targeting multiple embedded platforms while maintaining the real-time guarantees that FreeRTOS provides.