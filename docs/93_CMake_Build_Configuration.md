# CMake Build Configuration for FreeRTOS

## Detailed Description

CMake build configuration for FreeRTOS involves creating a flexible, portable, and maintainable build system that can handle the complexity of embedded systems development. Unlike traditional Makefiles, CMake provides a higher-level abstraction that generates platform-specific build files and simplifies cross-compilation, dependency management, and multi-target builds.

### Key Concepts

**1. Cross-Compilation Setup**
FreeRTOS targets embedded processors (ARM Cortex-M, RISC-V, etc.), requiring cross-compilation toolchains. CMake handles this through toolchain files that specify the compiler, linker, and target architecture.

**2. Modular Configuration**
FreeRTOS projects typically consist of:
- Core FreeRTOS kernel sources
- Port-specific files (architecture-dependent)
- Application code
- Hardware abstraction layers (HAL)
- Third-party libraries (middleware, drivers)

**3. Build Variants**
Managing multiple configurations (Debug/Release), different hardware targets, and feature sets within a single build system.

**4. Dependency Management**
Integrating FreeRTOS with libraries like FreeRTOS-Plus-TCP, CMSIS, vendor SDKs, and custom middleware.

---

## C/C++ Implementation

### Basic CMakeLists.txt for FreeRTOS

```c
cmake_minimum_required(VERSION 3.15)

# Set toolchain file before project()
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/arm-none-eabi-gcc.cmake)

project(FreeRTOS_Project C CXX ASM)

# FreeRTOS configuration
set(FREERTOS_KERNEL_PATH "${CMAKE_SOURCE_DIR}/FreeRTOS-Kernel")
set(FREERTOS_PORT "GCC/ARM_CM4F" CACHE STRING "FreeRTOS port")
set(FREERTOS_HEAP "heap_4" CACHE STRING "FreeRTOS heap implementation")

# Add FreeRTOS kernel
add_library(freertos_kernel STATIC
    ${FREERTOS_KERNEL_PATH}/tasks.c
    ${FREERTOS_KERNEL_PATH}/queue.c
    ${FREERTOS_KERNEL_PATH}/list.c
    ${FREERTOS_KERNEL_PATH}/timers.c
    ${FREERTOS_KERNEL_PATH}/event_groups.c
    ${FREERTOS_KERNEL_PATH}/stream_buffer.c
    ${FREERTOS_KERNEL_PATH}/portable/MemMang/${FREERTOS_HEAP}.c
    ${FREERTOS_KERNEL_PATH}/portable/${FREERTOS_PORT}/port.c
)

target_include_directories(freertos_kernel PUBLIC
    ${FREERTOS_KERNEL_PATH}/include
    ${FREERTOS_KERNEL_PATH}/portable/${FREERTOS_PORT}
    ${CMAKE_SOURCE_DIR}/config  # FreeRTOSConfig.h location
)

# Compiler flags for ARM Cortex-M4F
target_compile_options(freertos_kernel PRIVATE
    -mcpu=cortex-m4
    -mthumb
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

# Application executable
add_executable(app
    src/main.c
    src/tasks.c
    src/startup.c
)

target_link_libraries(app PRIVATE freertos_kernel)

# Linker script
set_target_properties(app PROPERTIES
    LINK_FLAGS "-T${CMAKE_SOURCE_DIR}/linker/STM32F407VG.ld -Wl,--gc-sections"
)
```

### Toolchain File (arm-none-eabi-gcc.cmake)

```cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

# Specify the cross compiler
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

# Skip compiler test (cross-compilation)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# Search for programs only in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

### Multi-Target Configuration

```cmake
# Board-specific configurations
option(TARGET_BOARD "Target board" "STM32F407")

if(TARGET_BOARD STREQUAL "STM32F407")
    set(MCU_FLAGS "-mcpu=cortex-m4 -mfpu=fpv4-sp-d16")
    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/STM32F407.ld")
    add_definitions(-DSTM32F407xx)
elseif(TARGET_BOARD STREQUAL "STM32F103")
    set(MCU_FLAGS "-mcpu=cortex-m3")
    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/STM32F103.ld")
    add_definitions(-DSTM32F103xB)
endif()

# Apply MCU flags
add_compile_options(${MCU_FLAGS} -mthumb)
```

### Application Code Example

```c
// main.c - FreeRTOS application using CMake build
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Task handles
static TaskHandle_t xTaskLEDHandle = NULL;

// LED blink task
void vTaskLED(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(500);

    for (;;)
    {
        // Toggle LED (hardware-specific)
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

int main(void)
{
    // Hardware initialization
    SystemClock_Config();
    HAL_Init();
    
    // Create tasks
    xTaskCreate(vTaskLED,
                "LED Task",
                configMINIMAL_STACK_SIZE,
                NULL,
                tskIDLE_PRIORITY + 1,
                &xTaskLEDHandle);
    
    // Start scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    for (;;);
}
```

### Third-Party Library Integration

```cmake
# Adding FreeRTOS-Plus-TCP
set(FREERTOS_PLUS_TCP_PATH "${CMAKE_SOURCE_DIR}/FreeRTOS-Plus/Source/FreeRTOS-Plus-TCP")

add_library(freertos_plus_tcp STATIC
    ${FREERTOS_PLUS_TCP_PATH}/FreeRTOS_IP.c
    ${FREERTOS_PLUS_TCP_PATH}/FreeRTOS_TCP_IP.c
    ${FREERTOS_PLUS_TCP_PATH}/FreeRTOS_UDP_IP.c
    ${FREERTOS_PLUS_TCP_PATH}/FreeRTOS_Sockets.c
    ${FREERTOS_PLUS_TCP_PATH}/FreeRTOS_Stream_Buffer.c
    ${FREERTOS_PLUS_TCP_PATH}/portable/NetworkInterface/STM32Fxx/NetworkInterface.c
)

target_link_libraries(app PRIVATE freertos_kernel freertos_plus_tcp)
```

---

## Rust Implementation

Rust integration with FreeRTOS typically uses FFI (Foreign Function Interface) to call FreeRTOS C APIs. The build configuration uses `build.rs` and `Cargo.toml`.

### Cargo.toml

```toml
[package]
name = "freertos-rust-app"
version = "0.1.0"
edition = "2021"

[dependencies]
freertos-rust = "0.3"  # FreeRTOS bindings for Rust
cortex-m = "0.7"
cortex-m-rt = "0.7"
panic-halt = "0.2"

[build-dependencies]
cc = "1.0"
cmake = "0.1"

[profile.release]
opt-level = "z"      # Optimize for size
lto = true           # Link-time optimization
codegen-units = 1

[[bin]]
name = "app"
test = false
bench = false
```

### build.rs - CMake Integration

```rust
// build.rs - Bridge between Cargo and CMake
use cmake::Config;
use std::env;

fn main() {
    // Get output directory
    let out_dir = env::var("OUT_DIR").unwrap();
    
    // Configure and build FreeRTOS using CMake
    let dst = Config::new("freertos")
        .define("FREERTOS_PORT", "GCC/ARM_CM4F")
        .define("FREERTOS_HEAP", "heap_4")
        .define("CMAKE_TOOLCHAIN_FILE", "cmake/arm-none-eabi-gcc.cmake")
        .build();
    
    // Link the FreeRTOS library
    println!("cargo:rustc-link-search=native={}/lib", dst.display());
    println!("cargo:rustc-link-lib=static=freertos_kernel");
    
    // Link standard ARM libraries
    println!("cargo:rustc-link-lib=static=gcc");
    println!("cargo:rustc-link-lib=static=c");
    println!("cargo:rustc-link-lib=static=nosys");
    
    // Rerun if files change
    println!("cargo:rerun-if-changed=freertos/");
    println!("cargo:rerun-if-changed=CMakeLists.txt");
}
```

### Rust Application Code

```rust
// src/main.rs - Rust application using FreeRTOS
#![no_std]
#![no_main]

use cortex_m_rt::entry;
use freertos_rust::*;
use panic_halt as _;

// Task function in Rust
fn led_task(_: FreeRtosTaskHandle) {
    let delay = Duration::ms(500);
    
    loop {
        // Toggle LED (unsafe hardware access)
        unsafe {
            toggle_led();
        }
        
        FreeRtosUtils::delay(delay);
    }
}

#[entry]
fn main() -> ! {
    // Initialize hardware
    unsafe {
        system_init();
    }
    
    // Create FreeRTOS task
    Task::new()
        .name("LED")
        .stack_size(256)
        .priority(TaskPriority(1))
        .start(led_task)
        .unwrap();
    
    // Start FreeRTOS scheduler
    FreeRtosUtils::start_scheduler();
}

// Hardware abstraction (FFI to C)
extern "C" {
    fn system_init();
    fn toggle_led();
}
```

### CMakeLists.txt for Rust FFI

```cmake
# CMakeLists.txt in freertos/ directory for Rust build
cmake_minimum_required(VERSION 3.15)
project(freertos_for_rust C ASM)

set(FREERTOS_KERNEL_PATH "${CMAKE_SOURCE_DIR}/../FreeRTOS-Kernel")

# FreeRTOS kernel library
add_library(freertos_kernel STATIC
    ${FREERTOS_KERNEL_PATH}/tasks.c
    ${FREERTOS_KERNEL_PATH}/queue.c
    ${FREERTOS_KERNEL_PATH}/list.c
    ${FREERTOS_KERNEL_PATH}/timers.c
    ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c
    ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM4F/port.c
    # Hardware abstraction for Rust
    ${CMAKE_SOURCE_DIR}/hal/hal_stm32.c
)

target_include_directories(freertos_kernel PUBLIC
    ${FREERTOS_KERNEL_PATH}/include
    ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM4F
    ${CMAKE_SOURCE_DIR}/../config
)

install(TARGETS freertos_kernel
    ARCHIVE DESTINATION lib
)
```

### Hardware Abstraction Layer (C side)

```c
// hal/hal_stm32.c - C hardware functions called from Rust
#include "stm32f4xx_hal.h"

void system_init(void)
{
    HAL_Init();
    SystemClock_Config();
    
    // Initialize GPIO for LED
    __HAL_RCC_GPIOD_CLK_ENABLE();
    
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
}

void toggle_led(void)
{
    HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_12);
}
```

---

## Summary

CMake build configuration for FreeRTOS provides a modern, flexible approach to managing embedded systems projects:

**Key Benefits:**
- **Portability**: Single build system works across platforms (Windows, Linux, macOS)
- **Cross-compilation**: Seamless support for embedded toolchains through toolchain files
- **Modularity**: Clean separation of kernel, ports, application, and third-party code
- **Scalability**: Easy management of multiple targets, boards, and build configurations
- **Integration**: Straightforward inclusion of libraries and SDKs

**C/C++ Approach:**
- Uses traditional CMakeLists.txt files to define targets and dependencies
- Toolchain files specify cross-compiler and target architecture
- Flexible configuration through cache variables and conditional compilation
- Direct integration with vendor HALs and middleware

**Rust Approach:**
- Leverages Cargo as the primary build system
- Uses `build.rs` to invoke CMake for building C components (FreeRTOS kernel)
- FFI bridges Rust safe code with C hardware abstraction
- Combines Rust's memory safety with FreeRTOS's real-time capabilities
- Requires careful management of linking and library dependencies

**Best Practices:**
1. Separate toolchain configuration from project logic
2. Use cache variables for configurable options
3. Create reusable CMake modules for common patterns
4. Version control toolchain files alongside source code
5. Implement proper dependency tracking to avoid unnecessary rebuilds
6. Document build requirements and target-specific configurations

This approach enables professional-grade embedded development with maintainable, testable, and portable build systems suitable for both individual projects and large-scale production environments.