# Testing STM32F103xx binaries based on FreeRTOS with Renode

## Overview

Renode is an open-source simulation framework that allows you to test embedded firmware without physical hardware. It's particularly useful for testing STM32F103xx microcontrollers running FreeRTOS applications, enabling continuous integration, automated testing, and debugging in a virtual environment.

## Key Concepts

**Renode** simulates the entire embedded system including:
- CPU architecture (ARM Cortex-M3 for STM32F103xx)
- Peripherals (GPIO, UART, SPI, I2C, timers, etc.)
- Memory layout
- Interrupt controllers

**Benefits for FreeRTOS Testing:**
- Test task scheduling and timing behavior
- Verify inter-task communication (queues, semaphores, mutexes)
- Debug race conditions and timing issues
- Run automated tests in CI/CD pipelines
- No physical hardware required

## Minimal Example

Let's create a simple FreeRTOS application that blinks an LED and sends messages over UART, then test it with Renode.

### Step 1: Create a Simple FreeRTOS Application

```c
// main.c - Simple LED blink with UART output
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f10x.h"
#include <stdio.h>

void vBlinkTask(void *pvParameters) {
    while(1) {
        GPIOC->ODR ^= GPIO_Pin_13;  // Toggle LED on PC13
        printf("LED Toggle\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    // Initialize GPIO for LED (PC13)
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(0xF << 20);
    GPIOC->CRH |= (0x2 << 20);  // Output 2MHz
    
    xTaskCreate(vBlinkTask, "Blink", 128, NULL, 1, NULL);
    vTaskStartScheduler();
    
    while(1);
}
```

### Step 2: Build the Binary

Using ARM GCC toolchain:

```bash
# Install ARM toolchain
sudo apt-get install gcc-arm-none-eabi

# Compile (assuming you have proper linker script and FreeRTOS sources)
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -DSTM32F10X_MD \
    -I./FreeRTOS/Source/include \
    -I./FreeRTOS/Source/portable/GCC/ARM_CM3 \
    -I./Device/ST/STM32F10x/Include \
    -I./CMSIS/Include \
    main.c \
    FreeRTOS/Source/tasks.c \
    FreeRTOS/Source/queue.c \
    FreeRTOS/Source/list.c \
    FreeRTOS/Source/portable/GCC/ARM_CM3/port.c \
    -T stm32f103.ld \
    -o firmware.elf

# Create binary
arm-none-eabi-objcopy -O binary firmware.elf firmware.bin
```

### Step 3: Install Renode

```bash
# On Ubuntu/Debian
wget https://github.com/renode/renode/releases/download/v1.15.0/renode_1.15.0_amd64.deb
sudo apt install ./renode_1.15.0_amd64.deb

# Or use Mono on other systems
# Download from: https://github.com/renode/renode/releases
```

### Step 4: Create Renode Script

Create a file `stm32f103_test.resc`:

```renode
# Load STM32F103 platform
using sysbus
mach create "stm32f103"
machine LoadPlatformDescription @platforms/cpus/stm32f103.repl

# Load the firmware binary
sysbus LoadELF @firmware.elf

# Setup UART monitoring
showAnalyzer sysbus.usart1

# Start emulation
start
```

### Step 5: Run the Test

```bash
# Start Renode and run the script
renode stm32f103_test.resc
```

In the Renode console, you can:
```
# Monitor UART output
(monitor) sysbus.usart1 RecordToFile @uart_output.txt

# Check GPIO state
(monitor) sysbus.gpioPortC.C13 State

# Run for specific time
(monitor) emulation RunFor "00:00:10"

# Examine memory
(monitor) sysbus ReadDoubleWord 0x20000000
```

### Step 6: Automated Testing with Robot Framework

Create `test.robot`:

```robot
*** Settings ***
Suite Setup     Setup
Suite Teardown  Teardown
Test Teardown   Test Teardown
Resource        ${RENODEKEYWORDS}

*** Test Cases ***
Should Boot And Toggle LED
    Execute Command    mach create "stm32f103"
    Execute Command    machine LoadPlatformDescription @platforms/cpus/stm32f103.repl
    Execute Command    sysbus LoadELF @firmware.elf
    
    Create Terminal Tester  sysbus.usart1
    
    Start Emulation
    Wait For Line On Uart   LED Toggle    timeout=5
    Wait For Line On Uart   LED Toggle    timeout=2
```

Run the test:
```bash
renode-test test.robot
```

## Testing Approach

**Unit Testing:**
- Test individual FreeRTOS tasks in isolation
- Verify queue operations and semaphore behavior
- Check timer accuracy using Renode's time virtualization

**Integration Testing:**
- Test multi-task interactions
- Verify peripheral communication (UART, SPI, I2C)
- Validate interrupt handling

**Debugging Tips:**
- Use `emulation RunFor` to control execution time
- Set breakpoints: `sysbus.cpu SetBreakpointAtAddress 0x08000100`
- Inspect task states and memory in real-time
- Use Renode's GDB integration for step-through debugging

This approach provides a complete development and testing workflow without requiring physical STM32 hardware.