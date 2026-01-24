# RTOS Concepts and Scheduling Theory

## Understanding Real-Time Operating Systems

Real-Time Operating Systems (RTOS) are specialized operating systems designed to process data and events within strictly defined time constraints. Unlike general-purpose operating systems (GPOS) like Windows or Linux, an RTOS prioritizes **deterministic behavior** and **predictable response times** over throughput or fairness.

### Key Differences: RTOS vs General-Purpose OS

**General-Purpose OS (GPOS):**
- Optimized for average performance and throughput
- Unpredictable task execution timing
- Complex scheduling algorithms focusing on fairness
- Large memory footprint with extensive features
- Example: A video might stutter momentarily while the OS handles background tasks

**Real-Time OS (RTOS):**
- Optimized for deterministic, predictable timing
- Guaranteed response within specified deadlines
- Simpler, priority-based scheduling
- Minimal memory footprint and overhead
- Example: An airbag controller must respond within milliseconds, every time

The critical distinction is that an RTOS guarantees when a task will execute, while a GPOS only tries to execute tasks efficiently on average.

## Scheduling Fundamentals

### Preemptive Scheduling

In **preemptive scheduling**, the scheduler can interrupt a currently running task to allow a higher-priority task to execute immediately. This is the default mode in FreeRTOS.

**How it works:**
1. A high-priority task becomes ready (e.g., waiting for data that just arrived)
2. The scheduler immediately suspends the currently running lower-priority task
3. The scheduler performs a context switch to the high-priority task
4. The high-priority task executes
5. When complete, the scheduler resumes the lower-priority task

**Example scenario:**
```c
// Task priorities (higher number = higher priority)
#define SENSOR_TASK_PRIORITY    3
#define DATA_PROCESS_PRIORITY   2
#define LED_BLINK_PRIORITY      1

void vSensorTask(void *pvParameters) {
    while(1) {
        // Wait for sensor interrupt (blocked state)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // High-priority sensor data arrived!
        uint16_t sensorValue = readSensor();
        processCriticalData(sensorValue);
        
        // Task completes quickly and blocks again
    }
}

void vDataProcessTask(void *pvParameters) {
    while(1) {
        // This task is preempted when sensor data arrives
        performComplexCalculations();  // Takes 50ms
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

When the sensor interrupt occurs during `performComplexCalculations()`, FreeRTOS immediately preempts the data processing task and switches to the sensor task. The calculation resumes after the sensor task blocks again.

**Advantages:**
- Immediate response to high-priority events
- Better real-time performance
- Reduced latency for critical tasks

**Challenges:**
- Risk of priority inversion (discussed later)
- Requires careful synchronization between tasks
- More complex to debug

### Cooperative Scheduling

In **cooperative scheduling**, tasks must explicitly yield control back to the scheduler. A task runs until it voluntarily gives up the CPU.

**In FreeRTOS**, cooperative scheduling is enabled by setting `configUSE_PREEMPTION` to 0 in `FreeRTOSConfig.h`.

**Example scenario:**
```c
void vCooperativeTask1(void *pvParameters) {
    while(1) {
        processDataChunk();     // Process one chunk
        taskYIELD();            // Voluntarily yield to other tasks
    }
}

void vCooperativeTask2(void *pvParameters) {
    while(1) {
        updateDisplay();        // Update display
        taskYIELD();            // Voluntarily yield to other tasks
    }
}
```

**Advantages:**
- Simpler synchronization (tasks control when they can be interrupted)
- No need for mutexes in some cases
- More predictable for certain applications

**Disadvantages:**
- Poor responsiveness (tasks must wait for others to yield)
- One misbehaving task can block the entire system
- Not suitable for hard real-time requirements

**When to use cooperative scheduling:**
- Simple systems with well-behaved tasks
- Legacy code migration
- Systems where task synchronization complexity must be minimized

## Time-Slicing (Round-Robin Scheduling)

When multiple tasks have the **same priority**, FreeRTOS can use time-slicing to share CPU time fairly among them.

**Configuration:**
Enable time-slicing by setting `configUSE_TIME_SLICING` to 1 in `FreeRTOSConfig.h`.

**How it works:**
Each task at the same priority level gets a time slice (one tick period by default). When the slice expires, the scheduler switches to the next ready task at that priority.

**Example:**
```c
// Three tasks at the same priority
#define EQUAL_PRIORITY  2

void vTask1(void *pvParameters) {
    while(1) {
        // Runs for one time slice
        performWork1();
        // Automatically switched out after tick interrupt
    }
}

void vTask2(void *pvParameters) {
    while(1) {
        performWork2();
    }
}

void vTask3(void *pvParameters) {
    while(1) {
        performWork3();
    }
}

// In main()
xTaskCreate(vTask1, "Task1", 128, NULL, EQUAL_PRIORITY, NULL);
xTaskCreate(vTask2, "Task2", 128, NULL, EQUAL_PRIORITY, NULL);
xTaskCreate(vTask3, "Task3", 128, NULL, EQUAL_PRIORITY, NULL);
```

**Execution pattern with 1ms tick rate:**
- Task1 runs for 1ms → Task2 runs for 1ms → Task3 runs for 1ms → Task1 runs for 1ms → ...

**Important notes:**
- Time-slicing only occurs between tasks of equal priority
- Higher priority tasks always preempt lower priority tasks immediately
- If time-slicing is disabled, the first ready task at a priority level runs until it blocks

## Context Switching

A **context switch** is the process of saving the state of one task and restoring the state of another. This is fundamental to multitasking.

### What Gets Saved During a Context Switch

**Task context includes:**
- CPU registers (general-purpose registers, stack pointer, program counter)
- Task stack contents
- Task state information (priority, delay counters, etc.)

**The process:**
1. **Save context**: Store all CPU registers of the current task to its stack
2. **Select next task**: Scheduler determines which task should run next
3. **Restore context**: Load CPU registers from the new task's stack
4. **Resume execution**: New task continues from where it was interrupted

**Example context switch triggers in FreeRTOS:**
```c
void vHighPriorityTask(void *pvParameters) {
    while(1) {
        // Wait for event (task blocks, context switch occurs)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Context switch here
        
        // Process event quickly
        handleEvent();
        
        // Block again (another context switch)
    }
}

void vLowPriorityTask(void *pvParameters) {
    while(1) {
        // Voluntary delay (context switch occurs)
        vTaskDelay(pdMS_TO_TICKS(100));  // Context switch here
        
        // Perform periodic work
        doPeriodicTask();
    }
}
```

### Context Switch Overhead

Every context switch has a time cost, typically 1-10 microseconds depending on the processor architecture.

**Example overhead calculation:**
- ARM Cortex-M4 @ 168MHz: ~1-2 microseconds per context switch
- If switching 1000 times per second: 1-2ms total overhead (0.1-0.2% CPU)
- But if switching 10,000 times per second: 10-20ms (1-2% CPU)

**Best practices to minimize overhead:**
- Don't create too many tasks
- Avoid unnecessary task delays or blocking operations
- Use direct-to-task notifications instead of queues when possible
- Batch work in tasks rather than switching frequently

## Scheduling Policies in FreeRTOS

FreeRTOS uses a **fixed-priority preemptive scheduler** with optional time-slicing:

**Priority rules:**
1. The highest priority ready task always runs
2. If multiple tasks share the highest priority, they share CPU time (if time-slicing enabled)
3. Lower priority tasks only run when all higher priority tasks are blocked

**Task states:**
- **Running**: Currently executing on the CPU
- **Ready**: Able to run but waiting for CPU time
- **Blocked**: Waiting for an event (delay, queue, semaphore, etc.)
- **Suspended**: Explicitly suspended via `vTaskSuspend()`

**Practical example - Priority-based system:**
```c
#define EMERGENCY_PRIORITY      4  // Emergency stop button
#define CONTROL_LOOP_PRIORITY   3  // Motor control (10ms deadline)
#define SENSOR_READ_PRIORITY    2  // Sensor reading (50ms deadline)
#define DISPLAY_PRIORITY        1  // UI updates (no hard deadline)
#define IDLE_PRIORITY           0  // Background logging

void vEmergencyTask(void *pvParameters) {
    while(1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // Preempts everything immediately
        activateEmergencyStop();
    }
}

void vMotorControlTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while(1) {
        // Runs every 10ms, preempts lower priorities
        updatePIDController();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10));
    }
}

void vDisplayTask(void *pvParameters) {
    while(1) {
        // Only runs when higher priority tasks are blocked
        updateLCD();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

## Real-World Application Example

Consider an automotive engine control unit (ECU):

```c
// Critical tasks with strict timing requirements
#define SPARK_TIMING_PRIORITY       5  // Must fire within 100μs
#define FUEL_INJECTION_PRIORITY     4  // Must trigger within 500μs
#define SENSOR_SAMPLING_PRIORITY    3  // Read every 1ms
#define ENGINE_CALCULATION_PRIORITY 2  // Complex math, 10ms budget
#define DIAGNOSTICS_PRIORITY        1  // Background, no deadline

void vSparkTimingTask(void *pvParameters) {
    while(1) {
        // Wait for crankshaft position interrupt
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // CRITICAL: Must execute immediately (preempts everything)
        triggerSparkPlug();  // ~50μs execution time
    }
}

void vEngineDiagnosticsTask(void *pvParameters) {
    while(1) {
        // Only runs when all critical tasks are idle
        checkErrorCodes();
        logTelemetry();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

In this system, the spark timing task will **always** preempt the diagnostics task, ensuring the engine runs correctly even if diagnostics are processing.

## Summary

RTOS scheduling theory emphasizes predictability and determinism. FreeRTOS provides a flexible, priority-based preemptive scheduler that ensures high-priority tasks always get CPU time when needed. Understanding these concepts is essential for designing responsive, reliable embedded systems where timing guarantees are critical.


