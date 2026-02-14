# Code Coverage Analysis in FreeRTOS

## Detailed Description

Code coverage analysis is a critical quality assurance technique used in embedded systems development, particularly for safety-critical applications running FreeRTOS. It measures which parts of your codebase are executed during testing, helping identify untested code paths that could harbor bugs or fail in production.

### Why Code Coverage Matters for FreeRTOS Applications

In safety-critical systems (medical devices, automotive, aerospace), regulatory standards like DO-178C, IEC 61508, and ISO 26262 mandate specific code coverage levels:

- **Statement Coverage (C0)**: Every line of code executed at least once
- **Branch Coverage (C1)**: Every decision branch (if/else) taken both ways
- **Modified Condition/Decision Coverage (MC/DC)**: Each condition independently affects decision outcomes
- **Path Coverage**: All possible execution paths tested

For FreeRTOS applications, coverage analysis must account for:
- Task switching and scheduling behavior
- Interrupt service routines (ISRs)
- Queue and semaphore operations
- Timer callbacks
- Idle task and hook functions
- Memory management operations

### Coverage Tools Ecosystem

**GCC-based Tools (gcov/lcov)**
- Built into GCC toolchain
- Generates `.gcda` and `.gcno` files
- Works with cross-compilation for embedded targets
- Limited real-time analysis capabilities

**Commercial Tools**
- VectorCAST, LDRA, Parasoft C/C++test
- Provide MC/DC coverage
- Better support for embedded targets
- Integrated certification reporting

**Modern Alternatives**
- llvm-cov (Clang/LLVM)
- Better integration with modern toolchains
- Supports Rust natively

### Challenges in RTOS Coverage Analysis

1. **Timing Sensitivity**: Instrumentation adds overhead, affecting real-time behavior
2. **Non-determinism**: Task scheduling makes reproducible tests difficult
3. **Hardware Dependencies**: Coverage on target vs. simulation differences
4. **ISR Coverage**: Interrupt-driven code needs special handling
5. **Concurrency**: Race conditions may only appear under specific timing

## Programming with Code Coverage

### C/C++ Implementation

#### Basic Setup with gcov

```c
/* freertos_coverage_example.c */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

/* Compile with: -fprofile-arcs -ftest-coverage -O0 */

/* Global queue handle */
QueueHandle_t xDataQueue;

/* Coverage target: sensor processing task */
void vSensorTask(void *pvParameters)
{
    uint32_t sensorValue;
    BaseType_t xStatus;
    
    for (;;)
    {
        /* Simulate sensor reading */
        sensorValue = (uint32_t)(xTaskGetTickCount() % 100);
        
        /* Branch 1: Threshold checking */
        if (sensorValue > 75)
        {
            printf("High reading: %u\n", sensorValue);
            /* This branch should be covered by tests */
        }
        else if (sensorValue < 25)
        {
            printf("Low reading: %u\n", sensorValue);
            /* This branch should be covered too */
        }
        else
        {
            printf("Normal reading: %u\n", sensorValue);
        }
        
        /* Branch 2: Queue send with error handling */
        xStatus = xQueueSend(xDataQueue, &sensorValue, pdMS_TO_TICKS(100));
        
        if (xStatus != pdPASS)
        {
            /* Error path - needs coverage */
            printf("Queue full! Dropping data\n");
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/* Processing task with multiple code paths */
void vProcessTask(void *pvParameters)
{
    uint32_t receivedData;
    uint32_t processedCount = 0;
    
    for (;;)
    {
        if (xQueueReceive(xDataQueue, &receivedData, portMAX_DELAY) == pdPASS)
        {
            processedCount++;
            
            /* Complex decision requiring MC/DC coverage */
            if ((receivedData > 50) && (processedCount > 10))
            {
                printf("Trigger action A\n");
            }
            else if ((receivedData > 50) || (processedCount > 10))
            {
                printf("Trigger action B\n");
            }
            else
            {
                printf("Normal processing\n");
            }
        }
    }
}

/* Test harness function */
void vRunCoverageTests(void)
{
    uint32_t testValue;
    
    /* Test 1: High threshold path */
    testValue = 80;
    xQueueSend(xDataQueue, &testValue, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* Test 2: Low threshold path */
    testValue = 20;
    xQueueSend(xDataQueue, &testValue, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* Test 3: Normal range path */
    testValue = 50;
    xQueueSend(xDataQueue, &testValue, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    /* Test 4: Queue full scenario - fill queue to trigger error path */
    for (int i = 0; i < 20; i++) {
        xQueueSend(xDataQueue, &testValue, 0);
    }
}

/* Main application */
int main(void)
{
    /* Create queue */
    xDataQueue = xQueueCreate(10, sizeof(uint32_t));
    
    if (xDataQueue != NULL)
    {
        /* Create tasks */
        xTaskCreate(vSensorTask, "Sensor", 1000, NULL, 2, NULL);
        xTaskCreate(vProcessTask, "Process", 1000, NULL, 1, NULL);
        
        /* Optional: Run automated tests before starting scheduler */
        #ifdef COVERAGE_TEST_MODE
        vRunCoverageTests();
        #endif
        
        /* Start scheduler */
        vTaskStartScheduler();
    }
    
    /* Should never reach here */
    for (;;);
    
    return 0;
}
```

#### Advanced Coverage Infrastructure

```c
/* coverage_hooks.c - Integration with FreeRTOS hooks */
#include "FreeRTOS.h"
#include "task.h"

#ifdef COVERAGE_ENABLED

/* External gcov functions */
extern void __gcov_flush(void);
extern void __gcov_reset(void);

/* Coverage data management */
static volatile uint32_t coverageFlushRequested = 0;

/* Idle hook to flush coverage data periodically */
void vApplicationIdleHook(void)
{
    static TickType_t lastFlushTime = 0;
    TickType_t currentTime = xTaskGetTickCount();
    
    /* Flush coverage data every 10 seconds */
    if ((currentTime - lastFlushTime) > pdMS_TO_TICKS(10000))
    {
        taskENTER_CRITICAL();
        __gcov_flush();
        taskEXIT_CRITICAL();
        
        lastFlushTime = currentTime;
    }
}

/* Stack overflow hook - ensure coverage data saved before crash */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    taskDISABLE_INTERRUPTS();
    __gcov_flush();
    
    /* Log error */
    printf("Stack overflow in task: %s\n", pcTaskName);
    
    for (;;); /* Halt */
}

/* Malloc failed hook */
void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    __gcov_flush();
    
    printf("Malloc failed!\n");
    
    for (;;);
}

/* Custom function to force coverage dump on demand */
void vForceCoverageFlush(void)
{
    taskENTER_CRITICAL();
    __gcov_flush();
    taskEXIT_CRITICAL();
}

#endif /* COVERAGE_ENABLED */
```

#### MC/DC Coverage Example

```c
/* mcdc_example.c - Modified Condition/Decision Coverage patterns */

typedef struct {
    uint8_t temperatureHigh;
    uint8_t pressureLow;
    uint8_t emergencyStop;
} SystemState_t;

/* Function requiring MC/DC coverage for safety certification */
uint8_t xCheckSafetyConditions(SystemState_t *pxState)
{
    /*
     * MC/DC requires each condition to independently affect outcome
     * Test cases needed:
     * 1. T=1, P=1, E=1 -> unsafe
     * 2. T=1, P=1, E=0 -> unsafe (T causes unsafe)
     * 3. T=1, P=0, E=0 -> safe
     * 4. T=0, P=1, E=0 -> unsafe (P causes unsafe)
     * 5. T=0, P=0, E=0 -> safe
     * 6. T=0, P=0, E=1 -> unsafe (E causes unsafe)
     */
    
    if (pxState->temperatureHigh || 
        pxState->pressureLow || 
        pxState->emergencyStop)
    {
        return 0; /* Unsafe */
    }
    
    return 1; /* Safe */
}

/* MC/DC test suite */
void vRunMCDCTests(void)
{
    SystemState_t xState;
    uint8_t result;
    
    /* Test case 1: All true */
    xState.temperatureHigh = 1;
    xState.pressureLow = 1;
    xState.emergencyStop = 1;
    result = xCheckSafetyConditions(&xState);
    configASSERT(result == 0);
    
    /* Test case 2: Temperature independently causes unsafe */
    xState.temperatureHigh = 1;
    xState.pressureLow = 0;
    xState.emergencyStop = 0;
    result = xCheckSafetyConditions(&xState);
    configASSERT(result == 0);
    
    /* Test case 3: Safe condition */
    xState.temperatureHigh = 0;
    xState.pressureLow = 0;
    xState.emergencyStop = 0;
    result = xCheckSafetyConditions(&xState);
    configASSERT(result == 1);
    
    /* Test case 4: Pressure independently causes unsafe */
    xState.temperatureHigh = 0;
    xState.pressureLow = 1;
    xState.emergencyStop = 0;
    result = xCheckSafetyConditions(&xState);
    configASSERT(result == 0);
    
    /* Test case 5: Emergency independently causes unsafe */
    xState.temperatureHigh = 0;
    xState.pressureLow = 0;
    xState.emergencyStop = 1;
    result = xCheckSafetyConditions(&xState);
    configASSERT(result == 0);
    
    printf("MC/DC tests passed\n");
}
```

### Rust Implementation

Rust's coverage tools (llvm-cov, tarpaulin) integrate well with embedded development:

```rust
// lib.rs - FreeRTOS bindings with coverage support
#![no_std]
#![cfg_attr(coverage, feature(coverage_attribute))]

use freertos_rust::*;

/// Safety-critical queue wrapper with coverage tracking
pub struct SafeQueue<T> {
    queue: Queue<T>,
    stats: QueueStats,
}

#[derive(Default)]
struct QueueStats {
    send_attempts: u32,
    send_failures: u32,
    receive_attempts: u32,
    receive_failures: u32,
}

impl<T: Copy> SafeQueue<T> {
    pub fn new(length: usize) -> Option<Self> {
        Queue::new(length).map(|queue| SafeQueue {
            queue,
            stats: QueueStats::default(),
        })
    }
    
    /// Send with coverage tracking
    #[cfg_attr(coverage, coverage(on))]
    pub fn send(&mut self, item: T, timeout: Duration) -> Result<(), FreeRtosError> {
        self.stats.send_attempts += 1;
        
        match self.queue.send(item, timeout) {
            Ok(_) => Ok(()),
            Err(e) => {
                // Branch coverage: error path
                self.stats.send_failures += 1;
                Err(e)
            }
        }
    }
    
    /// Receive with coverage tracking
    #[cfg_attr(coverage, coverage(on))]
    pub fn receive(&mut self, timeout: Duration) -> Result<T, FreeRtosError> {
        self.stats.receive_attempts += 1;
        
        match self.queue.receive(timeout) {
            Ok(item) => Ok(item),
            Err(e) => {
                // Branch coverage: timeout/error path
                self.stats.receive_failures += 1;
                Err(e)
            }
        }
    }
    
    pub fn get_stats(&self) -> &QueueStats {
        &self.stats
    }
}

/// Sensor processing with multiple code paths for coverage
#[cfg_attr(coverage, coverage(on))]
pub fn process_sensor_reading(value: u32) -> SensorAction {
    // Branch coverage target
    if value > 75 {
        SensorAction::HighAlert
    } else if value < 25 {
        SensorAction::LowAlert
    } else {
        SensorAction::Normal
    }
}

#[derive(Debug, PartialEq)]
pub enum SensorAction {
    HighAlert,
    LowAlert,
    Normal,
}

/// MC/DC coverage example in Rust
#[cfg_attr(coverage, coverage(on))]
pub fn check_safety_conditions(
    temperature_high: bool,
    pressure_low: bool,
    emergency_stop: bool,
) -> bool {
    // Requires MC/DC test coverage
    !(temperature_high || pressure_low || emergency_stop)
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_sensor_high_range() {
        assert_eq!(process_sensor_reading(80), SensorAction::HighAlert);
    }
    
    #[test]
    fn test_sensor_low_range() {
        assert_eq!(process_sensor_reading(20), SensorAction::LowAlert);
    }
    
    #[test]
    fn test_sensor_normal_range() {
        assert_eq!(process_sensor_reading(50), SensorAction::Normal);
    }
    
    #[test]
    fn test_mcdc_all_safe() {
        assert_eq!(check_safety_conditions(false, false, false), true);
    }
    
    #[test]
    fn test_mcdc_temperature_causes_unsafe() {
        assert_eq!(check_safety_conditions(true, false, false), false);
    }
    
    #[test]
    fn test_mcdc_pressure_causes_unsafe() {
        assert_eq!(check_safety_conditions(false, true, false), false);
    }
    
    #[test]
    fn test_mcdc_emergency_causes_unsafe() {
        assert_eq!(check_safety_conditions(false, false, true), false);
    }
    
    #[test]
    fn test_queue_operations() {
        let mut queue: SafeQueue<u32> = SafeQueue::new(5).unwrap();
        
        // Test send success path
        assert!(queue.send(42, Duration::ms(100)).is_ok());
        
        // Test receive success path
        assert_eq!(queue.receive(Duration::ms(100)).unwrap(), 42);
        
        // Test receive timeout path
        assert!(queue.receive(Duration::ms(10)).is_err());
        
        let stats = queue.get_stats();
        assert_eq!(stats.send_attempts, 1);
        assert_eq!(stats.receive_failures, 1);
    }
}
```

#### Rust Coverage Configuration

```toml
# Cargo.toml
[package]
name = "freertos-coverage"
version = "0.1.0"
edition = "2021"

[dependencies]
freertos-rust = "0.4"

[profile.coverage]
inherits = "test"
opt-level = 0
debug = true

# .cargo/config.toml for coverage
[build]
rustflags = ["-C", "instrument-coverage"]

[target.'cfg(all(target_arch = "arm", target_os = "none"))']
runner = "probe-run --chip STM32F407VG"
```

#### Running Rust Coverage

```bash
# Generate coverage with llvm-cov
cargo install cargo-llvm-cov

# Run tests with coverage
cargo llvm-cov --html

# For embedded targets (simulation)
cargo llvm-cov --target thumbv7em-none-eabihf --html

# Generate lcov format for CI integration
cargo llvm-cov --lcov --output-path coverage.lcov

# Coverage with specific threshold
cargo llvm-cov --fail-under-lines 80
```

### Build Scripts and Automation

#### Makefile for C/C++ Coverage

```makefile
# Makefile with gcov integration
CC = arm-none-eabi-gcc
CFLAGS = -mcpu=cortex-m4 -mthumb -O0 -g
COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage --coverage

# Source files
SOURCES = main.c tasks.c coverage_hooks.c
FREERTOS_SRC = $(wildcard FreeRTOS/Source/*.c)

# Coverage build
coverage: CFLAGS += $(COVERAGE_FLAGS)
coverage: LDFLAGS += -lgcov
coverage: all

all: firmware.elf

firmware.elf: $(SOURCES) $(FREERTOS_SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

# Run tests and generate coverage report
test-coverage:
	./run_tests.sh
	gcov $(SOURCES)
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory coverage_html

# Clean coverage data
clean-coverage:
	rm -f *.gcda *.gcno *.gcov coverage.info
	rm -rf coverage_html

.PHONY: coverage test-coverage clean-coverage
```

#### Python Test Automation

```python
#!/usr/bin/env python3
# coverage_runner.py - Automated coverage test execution

import subprocess
import json
import sys
from pathlib import Path

class CoverageRunner:
    def __init__(self, target="simulator"):
        self.target = target
        self.coverage_threshold = 85.0  # 85% for safety-critical
        
    def build_with_coverage(self):
        """Build firmware with coverage instrumentation"""
        cmd = ["make", "clean", "&&", "make", "coverage"]
        result = subprocess.run(cmd, shell=True, capture_output=True)
        return result.returncode == 0
    
    def run_tests(self):
        """Execute test suite on target"""
        if self.target == "simulator":
            cmd = ["qemu-system-arm", "-M", "lm3s6965evb", 
                   "-nographic", "-kernel", "firmware.elf"]
        else:
            cmd = ["openocd", "-f", "run_tests.cfg"]
        
        result = subprocess.run(cmd, capture_output=True, timeout=60)
        return result.returncode == 0
    
    def generate_coverage_report(self):
        """Generate and parse coverage data"""
        # Run gcov
        subprocess.run(["gcov", "*.c"], check=True)
        
        # Run lcov
        subprocess.run([
            "lcov", "--capture",
            "--directory", ".",
            "--output-file", "coverage.info"
        ], check=True)
        
        # Generate HTML report
        subprocess.run([
            "genhtml", "coverage.info",
            "--output-directory", "coverage_html"
        ], check=True)
        
        # Parse coverage percentage
        result = subprocess.run(
            ["lcov", "--summary", "coverage.info"],
            capture_output=True, text=True
        )
        
        # Extract line coverage percentage
        for line in result.stdout.split('\n'):
            if "lines" in line:
                percentage = float(line.split(':')[1].strip().rstrip('%'))
                return percentage
        
        return 0.0
    
    def check_certification_requirements(self, coverage):
        """Verify coverage meets certification standards"""
        requirements = {
            "DO-178C Level A": 100.0,  # MC/DC required
            "DO-178C Level B": 100.0,  # Decision coverage
            "IEC 61508 SIL 3": 95.0,
            "ISO 26262 ASIL D": 90.0,
            "General Safety": self.coverage_threshold
        }
        
        print("\n=== Certification Coverage Requirements ===")
        for standard, required in requirements.items():
            status = "PASS" if coverage >= required else "FAIL"
            print(f"{standard}: {required}% required - {status}")
        
        return coverage >= self.coverage_threshold
    
    def run_full_analysis(self):
        """Complete coverage analysis workflow"""
        print("Starting coverage analysis...")
        
        # Step 1: Build
        print("\n[1/4] Building with coverage instrumentation...")
        if not self.build_with_coverage():
            print("ERROR: Build failed")
            return False
        
        # Step 2: Run tests
        print("\n[2/4] Running test suite...")
        if not self.run_tests():
            print("WARNING: Some tests may have failed")
        
        # Step 3: Generate report
        print("\n[3/4] Generating coverage report...")
        coverage = self.generate_coverage_report()
        print(f"Overall line coverage: {coverage}%")
        
        # Step 4: Check requirements
        print("\n[4/4] Checking certification requirements...")
        passed = self.check_certification_requirements(coverage)
        
        print(f"\nCoverage report generated in: coverage_html/index.html")
        
        return passed

if __name__ == "__main__":
    runner = CoverageRunner(target="simulator")
    success = runner.run_full_analysis()
    sys.exit(0 if success else 1)
```

## Summary

Code coverage analysis is essential for FreeRTOS safety-critical applications to meet certification requirements and ensure software quality. Key takeaways:

**Coverage Levels**: Different safety standards require varying coverage depths—from basic statement coverage (C0) to rigorous MC/DC for DO-178C Level A certification. FreeRTOS applications must achieve 85-100% coverage depending on the safety integrity level.

**Tools**: GCC's gcov/lcov provide free basic coverage for C/C++, while commercial tools offer advanced MC/DC analysis. Rust developers benefit from native llvm-cov integration with better embedded support.

**RTOS Challenges**: Coverage analysis in real-time systems faces unique hurdles—instrumentation overhead affects timing, task scheduling introduces non-determinism, and ISR code requires special handling. Solutions include running tests on simulators, using task-aware coverage hooks, and carefully managing coverage data persistence.

**Best Practices**: Integrate coverage into CI/CD pipelines, maintain automated test suites targeting all code paths, use FreeRTOS hooks to flush coverage data safely, and implement MC/DC test patterns for complex boolean logic in safety-critical functions.

**Certification**: Different domains demand specific coverage metrics: aerospace (DO-178C) requires MC/DC for highest criticality, automotive (ISO 26262) mandates 90%+ for ASIL D, and industrial (IEC 61508) requires 95% for SIL 3. Proper coverage infrastructure with traceable tests is mandatory for certification audits.