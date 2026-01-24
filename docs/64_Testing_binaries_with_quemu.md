# Using QEMU to Test STM32 Binaries

QEMU can emulate certain STM32 microcontrollers, allowing you to test firmware without physical hardware. Here's a comprehensive guide on setup, usage, and limitations.

## Supported STM32 Boards

QEMU has built-in support for several STM32 boards:
- **STM32F405** (used in boards like the STM32F4-Discovery)
- **Netduino 2** (STM32F205)
- **Olimex STM32-H405**
- **STM32VLDISCOVERY** (STM32F100)

You can check available machines with:
```bash
qemu-system-arm -machine help | grep stm32
```

## Basic Usage

### Running a Binary

```bash
qemu-system-arm -M netduino2 \
    -kernel your_firmware.elf \
    -nographic
```

Key options:
- `-M` or `-machine`: Specifies the board/machine type
- `-kernel`: Your compiled firmware (ELF format preferred)
- `-nographic`: Disables graphical window, routes serial to terminal
- `-s`: Start GDB server on port 1234
- `-S`: Start paused, waiting for GDB connection

### Example with Debugging

```bash
qemu-system-arm -M netduino2 \
    -kernel firmware.elf \
    -nographic \
    -s -S
```

Then in another terminal:
```bash
arm-none-eabi-gdb firmware.elf
(gdb) target remote localhost:1234
(gdb) continue
```

## Setting Up Printf Tracing

### Using Semihosting

Semihosting allows your embedded code to use the host's I/O facilities. This is the easiest way to get printf output.

**In your code:**
```c
// Add to your startup or main file
extern void initialise_monitor_handles(void);

int main(void) {
    initialise_monitor_handles();  // Enable semihosting
    
    printf("Hello from STM32!\n");
    // Your code here
}
```

**Compile with semihosting support:**
```bash
arm-none-eabi-gcc ... --specs=rdimon.specs -lrdimon
```

**Run QEMU with semihosting:**
```bash
qemu-system-arm -M netduino2 \
    -kernel firmware.elf \
    -semihosting-config enable=on,target=native \
    -nographic
```

### Using UART/USART

For more realistic testing, you can use the emulated UART:

**Configure serial output in QEMU:**
```bash
qemu-system-arm -M netduino2 \
    -kernel firmware.elf \
    -serial stdio
```

This redirects USART output to your terminal's stdout.

**Your firmware must configure the UART properly:**
```c
// Example UART initialization for STM32
void UART_Init(void) {
    // Enable USART2 clock
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    
    // Configure GPIO pins, baud rate, etc.
    USART2->BRR = 0x683;  // 9600 baud @ 16MHz
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void UART_SendChar(char c) {
    while(!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}
```

## Connecting to Serial Terminal (PuTTY/minicom)

### Using Unix Sockets or TCP

**Create a virtual serial port:**
```bash
qemu-system-arm -M netduino2 \
    -kernel firmware.elf \
    -serial tcp::5555,server,nowait
```

**Connect with PuTTY:**
- Connection type: Raw or Telnet
- Host: localhost
- Port: 5555

**Connect with minicom/screen:**
```bash
telnet localhost 5555
# or
nc localhost 5555
```

### Using PTY (Linux/Mac)

```bash
qemu-system-arm -M netduino2 \
    -kernel firmware.elf \
    -serial pty
```

QEMU will output something like: `char device redirected to /dev/pts/3`

Then connect:
```bash
minicom -D /dev/pts/3
# or
screen /dev/pts/3
```

## Major Limitations

### Hardware Limitations

1. **Incomplete peripheral emulation**: Many STM32 peripherals are not fully emulated or not emulated at all (ADC, DAC, advanced timers, DMA in many cases, USB, CAN, etc.)

2. **Timing inaccuracies**: QEMU doesn't precisely emulate timing. Delays, timer interrupts, and real-time behavior may not match real hardware.

3. **Limited board support**: Only a few STM32 variants are supported. If you're using STM32L4, STM32H7, or newer families, support may be minimal or nonexistent.

4. **GPIO limitations**: GPIO emulation is basic. External hardware interactions (buttons, LEDs, sensors) require special handling or aren't possible.

5. **No analog peripherals**: ADC/DAC operations won't work realistically.

6. **DMA support varies**: DMA may work for some peripherals but not others.

### Practical Limitations

1. **Power management**: Sleep modes, low-power modes, and clock management may not behave correctly.

2. **Flash/EEPROM**: Non-volatile memory emulation is limited.

3. **Interrupts**: While basic interrupts work, complex interrupt scenarios may not behave identically to hardware.

4. **External memories**: SDRAM, external flash often not emulated.

## Best Practices

**What works well in QEMU:**
- Basic algorithm testing
- Control flow verification
- Simple UART communication
- Basic interrupt handling
- Code structure validation
- GDB debugging of logic errors

**What requires real hardware:**
- Precise timing measurements
- Power consumption testing
- Peripheral driver validation (beyond basic UART/GPIO)
- Real-world sensor integration
- Production testing
- Bootloader development (often)

## Example Complete Workflow

```bash
# 1. Compile with debugging symbols and semihosting
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -g \
    -T linker_script.ld \
    --specs=rdimon.specs -lrdimon \
    main.c startup.c -o firmware.elf

# 2. Run in QEMU with semihosting
qemu-system-arm -M netduino2 \
    -kernel firmware.elf \
    -semihosting-config enable=on,target=native \
    -nographic

# Or with GDB debugging
qemu-system-arm -M netduino2 \
    -kernel firmware.elf \
    -semihosting-config enable=on,target=native \
    -nographic -s -S

# 3. In another terminal, debug
arm-none-eabi-gdb firmware.elf
(gdb) target remote :1234
(gdb) load
(gdb) break main
(gdb) continue
```

## Alternative: Renode

For more comprehensive STM32 testing, consider **Renode**, which offers better peripheral support and more accurate emulation for embedded systems than QEMU, though it has a steeper learning curve.

QEMU is excellent for quick testing of core logic and algorithms, but always validate on real hardware before deployment.