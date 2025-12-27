# Task Priorities and Priority Assignment in FreeRTOS

## Overview

Task priority assignment is one of the most critical design decisions in real-time systems. FreeRTOS uses a preemptive priority-based scheduler where higher priority tasks always run before lower priority tasks. Strategic priority assignment ensures deterministic behavior, meets timing constraints, and prevents common scheduling pitfalls like priority inversion.

## FreeRTOS Priority System

FreeRTOS priorities range from 0 (lowest) to `configMAX_PRIORITIES - 1` (highest). The idle task always runs at priority 0. When multiple tasks are ready to run, the scheduler selects the highest priority task. If multiple tasks share the same priority, FreeRTOS uses round-robin time-slicing (if enabled).

**Key characteristics:**
- **Preemptive scheduling**: A higher priority task preempts lower priority tasks immediately
- **Priority levels**: Typically 5-32 priority levels (configurable)
- **No automatic priority adjustment**: Priorities remain static unless explicitly changed
- **Deterministic**: Scheduling decisions are predictable and bounded

## Strategic Priority Assignment Approaches

### 1. Rate-Monotonic Scheduling (RMS)

Rate-Monotonic Scheduling assigns priorities inversely proportional to task periods: tasks with shorter periods receive higher priorities.

**Theory**: For periodic tasks, RMS is optimal for fixed-priority scheduling. If a task set is schedulable by any fixed-priority algorithm, it's schedulable by RMS.

**Schedulability bound**: For n tasks, CPU utilization must be ≤ n(2^(1/n) - 1)
- 2 tasks: ~82.8%
- 3 tasks: ~78.0%
- Many tasks: approaches ~69.3%

**Example:**

```c
/* Rate-Monotonic Priority Assignment */

// Task periods and priorities (RMS)
// Task A: Period = 10ms  → Highest Priority (3)
// Task B: Period = 20ms  → Medium Priority (2)
// Task C: Period = 50ms  → Lowest Priority (1)

#define TASK_A_PRIORITY  3
#define TASK_B_PRIORITY  2
#define TASK_C_PRIORITY  1

void vTaskA(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(10); // 10ms period
    
    for(;;)
    {
        // Execute task work (assume 3ms execution time)
        performTaskAWork();
        
        // Wait for next period
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void vTaskB(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(20); // 20ms period
    
    for(;;)
    {
        // Execute task work (assume 5ms execution time)
        performTaskBWork();
        
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void vTaskC(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(50); // 50ms period
    
    for(;;)
    {
        // Execute task work (assume 10ms execution time)
        performTaskCWork();
        
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void setup_rms_tasks(void)
{
    xTaskCreate(vTaskA, "TaskA", 200, NULL, TASK_A_PRIORITY, NULL);
    xTaskCreate(vTaskB, "TaskB", 200, NULL, TASK_B_PRIORITY, NULL);
    xTaskCreate(vTaskC, "TaskC", 200, NULL, TASK_C_PRIORITY, NULL);
}
```

**When to use RMS:**
- Periodic tasks with well-defined execution times
- Hard real-time requirements
- Simple, analyzable systems
- Tasks where response time correlates with period

### 2. Deadline-Monotonic Scheduling (DMS)

Deadline-Monotonic Scheduling assigns priorities based on relative deadlines: tasks with shorter deadlines receive higher priorities. This is more flexible than RMS when deadlines don't equal periods.

**Advantage**: Can handle cases where deadline ≠ period (constrained or arbitrary deadlines)

**Example:**

```c
/* Deadline-Monotonic Priority Assignment */

// Task specifications:
// Task X: Period=30ms, Deadline=15ms, Execution=5ms  → Priority 3
// Task Y: Period=40ms, Deadline=25ms, Execution=8ms  → Priority 2
// Task Z: Period=50ms, Deadline=50ms, Execution=10ms → Priority 1

#define TASK_X_PRIORITY  3  // Shortest deadline
#define TASK_Y_PRIORITY  2  // Medium deadline
#define TASK_Z_PRIORITY  1  // Longest deadline

typedef struct {
    TickType_t period;
    TickType_t deadline;
    TickType_t executionTime;
    const char *name;
} TaskTiming_t;

void vTaskX(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(30);
    const TickType_t xDeadline = pdMS_TO_TICKS(15);
    
    for(;;)
    {
        TickType_t xStartTime = xTaskGetTickCount();
        
        // Execute critical work (5ms)
        performCriticalWork();
        
        // Check if deadline was met
        TickType_t xElapsed = xTaskGetTickCount() - xStartTime;
        if(xElapsed > xDeadline) {
            handleDeadlineMiss("TaskX");
        }
        
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void vTaskY(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(40);
    const TickType_t xDeadline = pdMS_TO_TICKS(25);
    
    for(;;)
    {
        TickType_t xStartTime = xTaskGetTickCount();
        
        performModerateWork(); // 8ms
        
        TickType_t xElapsed = xTaskGetTickCount() - xStartTime;
        if(xElapsed > xDeadline) {
            handleDeadlineMiss("TaskY");
        }
        
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
```

**When to use DMS:**
- Tasks with deadlines shorter than their periods
- Mixed hard and soft real-time requirements
- More complex timing constraints

### 3. Priority Assignment Best Practices

**Practical guidelines for assigning priorities:**

```c
/* Multi-layer Priority Architecture */

// Critical System Tasks (Priorities 10-15)
#define PRIORITY_SAFETY_MONITOR    15  // Safety-critical monitoring
#define PRIORITY_FAULT_HANDLER     14  // Fault detection/handling
#define PRIORITY_WATCHDOG          13  // System watchdog

// Real-time Control Tasks (Priorities 7-9)
#define PRIORITY_MOTOR_CONTROL     9   // Fast motor control loop
#define PRIORITY_SENSOR_FUSION     8   // Sensor data processing
#define PRIORITY_CONTROL_LOOP      7   // Control algorithm

// Communication Tasks (Priorities 4-6)
#define PRIORITY_CAN_RX            6   // CAN bus reception
#define PRIORITY_UART_HANDLER      5   // UART communication
#define PRIORITY_NETWORK           4   // Network stack

// Application Tasks (Priorities 2-3)
#define PRIORITY_USER_INTERFACE    3   // UI updates
#define PRIORITY_DATA_LOGGING      2   // Background logging

// Background Tasks (Priority 1)
#define PRIORITY_DIAGNOSTICS       1   // System diagnostics
#define PRIORITY_IDLE              0   // Idle task (automatic)

void create_layered_system(void)
{
    // Safety-critical tasks
    xTaskCreate(vSafetyMonitor, "Safety", 256, NULL, 
                PRIORITY_SAFETY_MONITOR, NULL);
    
    // Real-time control
    xTaskCreate(vMotorControl, "Motor", 512, NULL, 
                PRIORITY_MOTOR_CONTROL, NULL);
    
    // Communication
    xTaskCreate(vCANReceive, "CAN_RX", 384, NULL, 
                PRIORITY_CAN_RX, NULL);
    
    // Application
    xTaskCreate(vUserInterface, "UI", 512, NULL, 
                PRIORITY_USER_INTERFACE, NULL);
    
    // Background
    xTaskCreate(vDiagnostics, "Diag", 256, NULL, 
                PRIORITY_DIAGNOSTICS, NULL);
}
```

## Priority Inversion and Prevention

Priority inversion occurs when a high-priority task is blocked waiting for a resource held by a low-priority task, while a medium-priority task preempts the low-priority task, causing unbounded delays.

### Classic Priority Inversion Scenario

```c
/* PROBLEMATIC: Unbounded Priority Inversion */

SemaphoreHandle_t xResourceMutex;

// Low priority task
void vLowPriorityTask(void *pvParameters)
{
    for(;;)
    {
        xSemaphoreTake(xResourceMutex, portMAX_DELAY);
        
        // Access shared resource
        accessSharedResource();
        // If preempted here by medium priority task,
        // high priority task is blocked indefinitely!
        
        xSemaphoreGive(xResourceMutex);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Medium priority task (no resource access)
void vMediumPriorityTask(void *pvParameters)
{
    for(;;)
    {
        // Long-running work that preempts low priority
        performLengthyComputation(); // Blocks high priority indirectly!
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// High priority task
void vHighPriorityTask(void *pvParameters)
{
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // Blocked waiting for low priority task
        // while medium priority runs!
        xSemaphoreTake(xResourceMutex, portMAX_DELAY);
        
        accessSharedResource();
        
        xSemaphoreGive(xResourceMutex);
    }
}
```

### Solution 1: Priority Inheritance Mutex

FreeRTOS provides mutexes with priority inheritance to prevent unbounded priority inversion.

```c
/* SOLUTION: Priority Inheritance Mutex */

SemaphoreHandle_t xResourceMutex;

void setup_priority_inheritance(void)
{
    // Create mutex (automatically supports priority inheritance)
    xResourceMutex = xSemaphoreCreateMutex();
    
    if(xResourceMutex == NULL) {
        // Handle error
        return;
    }
    
    // Create tasks with different priorities
    xTaskCreate(vLowPriorityTask, "Low", 256, NULL, 1, NULL);
    xTaskCreate(vMediumPriorityTask, "Med", 256, NULL, 2, NULL);
    xTaskCreate(vHighPriorityTask, "High", 256, NULL, 3, NULL);
}

void vLowPriorityTask(void *pvParameters)
{
    for(;;)
    {
        xSemaphoreTake(xResourceMutex, portMAX_DELAY);
        
        // While holding mutex, this task inherits priority 3
        // if high-priority task blocks on the mutex
        accessSharedResource();
        
        xSemaphoreGive(xResourceMutex);
        // Priority returns to 1
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vHighPriorityTask(void *pvParameters)
{
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(20));
        
        // If low priority task holds mutex, it temporarily
        // inherits priority 3, preventing medium priority
        // task from preempting
        xSemaphoreTake(xResourceMutex, portMAX_DELAY);
        
        accessSharedResource();
        
        xSemaphoreGive(xResourceMutex);
    }
}
```

### Solution 2: Priority Ceiling Protocol

Assign each resource a priority ceiling equal to the highest priority of any task that uses it.

```c
/* Priority Ceiling Protocol Implementation */

// Define resource access priorities
#define RESOURCE_A_CEILING  5  // Used by tasks priority 1, 3, 5
#define RESOURCE_B_CEILING  7  // Used by tasks priority 2, 5, 7

SemaphoreHandle_t xResourceA, xResourceB;

void vTaskWithCeilingProtocol(void *pvParameters)
{
    UBaseType_t uxOriginalPriority = uxTaskPriorityGet(NULL);
    
    for(;;)
    {
        // Raise priority to resource ceiling before taking mutex
        vTaskPrioritySet(NULL, RESOURCE_A_CEILING);
        
        xSemaphoreTake(xResourceA, portMAX_DELAY);
        
        // Access resource - no task with priority < ceiling can preempt
        accessResourceA();
        
        xSemaphoreGive(xResourceA);
        
        // Restore original priority
        vTaskPrioritySet(NULL, uxOriginalPriority);
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
```

### Solution 3: Minimize Critical Sections

The most effective solution is to minimize shared resource usage.

```c
/* Minimize Shared Resource Access */

// BAD: Long critical section
void vBadApproach(void)
{
    xSemaphoreTake(xMutex, portMAX_DELAY);
    
    readSensorData();           // Unnecessary in critical section
    processData();              // Unnecessary in critical section
    updateSharedVariable();     // Only this needs protection
    performCalculations();      // Unnecessary in critical section
    
    xSemaphoreGive(xMutex);
}

// GOOD: Minimal critical section
void vGoodApproach(void)
{
    // Do work outside critical section
    SensorData_t data = readSensorData();
    ProcessedData_t processed = processData(data);
    
    // Only protect the actual shared access
    xSemaphoreTake(xMutex, portMAX_DELAY);
    updateSharedVariable(processed);
    xSemaphoreGive(xMutex);
    
    // Continue work outside critical section
    performCalculations(processed);
}
```

## Dynamic Priority Changes

Sometimes runtime priority adjustment is necessary.

```c
/* Dynamic Priority Management */

TaskHandle_t xControlTask, xDataTask;

void vAdaptivePrioritySystem(void)
{
    SystemMode_t currentMode = getSystemMode();
    
    switch(currentMode) {
        case MODE_STARTUP:
            // During startup, prioritize initialization
            vTaskPrioritySet(xDataTask, 5);
            vTaskPrioritySet(xControlTask, 3);
            break;
            
        case MODE_NORMAL:
            // Normal operation: control has priority
            vTaskPrioritySet(xControlTask, 5);
            vTaskPrioritySet(xDataTask, 3);
            break;
            
        case MODE_EMERGENCY:
            // Emergency: boost safety tasks
            vTaskPrioritySet(xControlTask, 7);
            vTaskPrioritySet(xDataTask, 2);
            break;
    }
}

// Mode-aware task
void vControlTask(void *pvParameters)
{
    for(;;)
    {
        // Check if mode changed
        if(modeChangeDetected()) {
            vAdaptivePrioritySystem();
        }
        
        performControlLoop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## Practical Priority Assignment Example: Motor Control System

```c
/* Complete Motor Control System with Strategic Priorities */

#define PRIORITY_EMERGENCY_STOP   10  // Highest - safety critical
#define PRIORITY_CURRENT_CONTROL   8  // Fast inner loop (1kHz)
#define PRIORITY_VELOCITY_CONTROL  6  // Medium loop (200Hz)
#define PRIORITY_POSITION_CONTROL  4  // Slow outer loop (50Hz)
#define PRIORITY_USER_INPUT        3  // User interface
#define PRIORITY_TELEMETRY         2  // Data logging
#define PRIORITY_DIAGNOSTICS       1  // Background diagnostics

SemaphoreHandle_t xMotorDataMutex;

typedef struct {
    float current;
    float velocity;
    float position;
    uint32_t faultStatus;
} MotorData_t;

MotorData_t xMotorData;

void vEmergencyStopTask(void *pvParameters)
{
    // Highest priority - immediate response to faults
    for(;;)
    {
        if(checkEmergencyConditions()) {
            disableMotorPower();
            setSystemFault();
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // 1ms check rate
    }
}

void vCurrentControlTask(void *pvParameters)
{
    // Fast inner loop - 1kHz rate-monotonic priority
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1); // 1ms
    
    for(;;)
    {
        float measuredCurrent = readCurrentSensor();
        float controlOutput = calculateCurrentControl(measuredCurrent);
        
        setPWMOutput(controlOutput);
        
        // Update shared data with mutex
        xSemaphoreTake(xMotorDataMutex, portMAX_DELAY);
        xMotorData.current = measuredCurrent;
        xSemaphoreGive(xMotorDataMutex);
        
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void vVelocityControlTask(void *pvParameters)
{
    // Medium loop - 5ms period
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(5);
    
    for(;;)
    {
        float measuredVelocity = calculateVelocity();
        float velocityCommand = computeVelocityControl();
        
        xSemaphoreTake(xMotorDataMutex, portMAX_DELAY);
        xMotorData.velocity = measuredVelocity;
        xSemaphoreGive(xMotorDataMutex);
        
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void vPositionControlTask(void *pvParameters)
{
    // Slow outer loop - 20ms period
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(20);
    
    for(;;)
    {
        float measuredPosition = readEncoderPosition();
        float positionError = calculatePositionError();
        
        xSemaphoreTake(xMotorDataMutex, portMAX_DELAY);
        xMotorData.position = measuredPosition;
        xSemaphoreGive(xMotorDataMutex);
        
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

void setupMotorControlSystem(void)
{
    // Create mutex with priority inheritance
    xMotorDataMutex = xSemaphoreCreateMutex();
    
    // Create tasks with strategic priorities
    xTaskCreate(vEmergencyStopTask, "EmgStop", 256, NULL, 
                PRIORITY_EMERGENCY_STOP, NULL);
    xTaskCreate(vCurrentControlTask, "Current", 512, NULL, 
                PRIORITY_CURRENT_CONTROL, NULL);
    xTaskCreate(vVelocityControlTask, "Velocity", 512, NULL, 
                PRIORITY_VELOCITY_CONTROL, NULL);
    xTaskCreate(vPositionControlTask, "Position", 512, NULL, 
                PRIORITY_POSITION_CONTROL, NULL);
    xTaskCreate(vUserInputTask, "UserInput", 384, NULL, 
                PRIORITY_USER_INPUT, NULL);
    xTaskCreate(vTelemetryTask, "Telemetry", 512, NULL, 
                PRIORITY_TELEMETRY, NULL);
}
```

## Key Takeaways

**Strategic Priority Assignment:**
- Use Rate-Monotonic for periodic tasks (priority ∝ 1/period)
- Use Deadline-Monotonic when deadlines ≠ periods
- Group tasks into logical priority bands
- Leave gaps between priority levels for future expansion

**Avoiding Priority Inversion:**
- Always use mutexes (not binary semaphores) for shared resources
- Enable priority inheritance (automatic with FreeRTOS mutexes)
- Minimize critical section duration
- Consider priority ceiling protocol for complex systems
- Avoid nested resource locks when possible

**Priority Assignment Principles:**
- Safety-critical tasks: highest priorities
- Time-critical tasks: high priorities (RMS/DMS)
- Communication: medium priorities
- User interface: lower priorities
- Background tasks: lowest priorities
- Never use priority 0 except for idle task

Proper priority assignment is fundamental to achieving deterministic real-time behavior in FreeRTOS applications.