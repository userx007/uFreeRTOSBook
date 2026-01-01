# FreeRTOS Data Types Reference

- **Handle Types** - TaskHandle_t, QueueHandle_t, SemaphoreHandle_t, TimerHandle_t, etc.
- **Basic Types** - BaseType_t, UBaseType_t, TickType_t (architecture-dependent integer types)
- **Function Pointer Types** - TaskFunction_t, TimerCallbackFunction_t
- **Enumerations** - eTaskState, eSleepModeStatus, eNotifyAction
- **Structure Types** - TaskStatus_t, TimeOut_t, TaskParameters_t
- **Static Allocation Types** - StaticTask_t, StaticQueue_t, etc.
- **Event Types** - EventBits_t

Each entry includes a description and the possible values or ranges the type can hold. Note that many FreeRTOS types are architecture-dependent (8-bit, 16-bit, or 32-bit) and are configured through the FreeRTOSConfig.h file.

# FreeRTOS Data Types Reference

## Handle Types

### TaskHandle_t
**Description:** Handle (pointer) to a task control block (TCB).  
**Possible Values:** NULL (invalid/no task) or a valid pointer to a task structure. Used to reference and manipulate tasks.

### QueueHandle_t
**Description:** Handle to a queue.  
**Possible Values:** NULL (queue creation failed) or a valid pointer to a queue structure. Used for inter-task communication.

### SemaphoreHandle_t
**Description:** Handle to a semaphore (binary, counting, mutex, or recursive mutex).  
**Possible Values:** NULL (creation failed) or a valid pointer to a semaphore structure. Same underlying type as QueueHandle_t.

### TimerHandle_t
**Description:** Handle to a software timer.  
**Possible Values:** NULL (timer creation failed) or a valid pointer to a timer structure.

### EventGroupHandle_t
**Description:** Handle to an event group (used for event flag management).  
**Possible Values:** NULL (creation failed) or a valid pointer to an event group structure.

### StreamBufferHandle_t
**Description:** Handle to a stream buffer.  
**Possible Values:** NULL (creation failed) or a valid pointer to a stream buffer structure.

### MessageBufferHandle_t
**Description:** Handle to a message buffer.  
**Possible Values:** NULL (creation failed) or a valid pointer to a message buffer structure.

## Basic Types

### BaseType_t
**Description:** Basic signed integer type, architecture-dependent.  
**Possible Values:** 
- On 32-bit architectures: typically `int32_t` (-2,147,483,648 to 2,147,483,647)
- On 16-bit architectures: typically `int16_t` (-32,768 to 32,767)
- On 8-bit architectures: typically `int8_t` (-128 to 127)
- Commonly used for return values: `pdTRUE` (1), `pdFALSE` (0), `pdPASS` (1), `pdFAIL` (0)

### UBaseType_t
**Description:** Basic unsigned integer type, architecture-dependent.  
**Possible Values:**
- On 32-bit architectures: typically `uint32_t` (0 to 4,294,967,295)
- On 16-bit architectures: typically `uint16_t` (0 to 65,535)
- On 8-bit architectures: typically `uint8_t` (0 to 255)
- Used for priority levels, queue lengths, etc.

### TickType_t
**Description:** Type used for tick count values.  
**Possible Values:**
- If `configUSE_16_BIT_TICKS` is 1: `uint16_t` (0 to 65,535)
- If `configUSE_16_BIT_TICKS` is 0: `uint32_t` (0 to 4,294,967,295)
- Special value: `portMAX_DELAY` (maximum value, used for infinite blocking)

## Function Pointer Types

### TaskFunction_t
**Description:** Pointer to a task function.  
**Possible Values:** Address of a function with signature `void functionName(void *pvParameters)`. Cannot be NULL when creating a task.

### TimerCallbackFunction_t
**Description:** Pointer to a timer callback function.  
**Possible Values:** Address of a function with signature `void callbackName(TimerHandle_t xTimer)`. Cannot be NULL when creating a timer.

### TaskHookFunction_t
**Description:** Pointer to a task hook function (idle, tick, malloc failed, stack overflow).  
**Possible Values:** Address of a hook function with appropriate signature depending on the hook type.

## Status and Priority Types

### eTaskState
**Description:** Enumeration of possible task states.  
**Possible Values:**
- `eRunning` (0) - Task is currently executing
- `eReady` (1) - Task is ready to run
- `eBlocked` (2) - Task is blocked (waiting)
- `eSuspended` (3) - Task is suspended
- `eDeleted` (4) - Task has been deleted but not yet freed
- `eInvalid` (5) - Invalid state

### eSleepModeStatus
**Description:** Enumeration for tickless idle mode.  
**Possible Values:**
- `eAbortSleep` (0) - Don't enter low power mode
- `eStandardSleep` (1) - Enter low power mode
- `eNoTasksWaitingTimeout` (2) - No tasks waiting with timeout

## Structure Types

### TaskStatus_t
**Description:** Structure containing task status information.  
**Possible Values:** Structure with fields including:
- `xHandle` (TaskHandle_t) - Task handle
- `pcTaskName` (const char *) - Task name
- `xTaskNumber` (UBaseType_t) - Task number
- `eCurrentState` (eTaskState) - Current task state
- `uxCurrentPriority` (UBaseType_t) - Current priority
- `uxBasePriority` (UBaseType_t) - Base priority
- `ulRunTimeCounter` (uint32_t) - Runtime counter
- `pxStackBase` (StackType_t *) - Stack base pointer
- `usStackHighWaterMark` (uint16_t) - Minimum remaining stack

### TimeOut_t
**Description:** Structure used for tracking timeouts.  
**Possible Values:** Structure with fields:
- `xOverflowCount` (BaseType_t) - Tick count overflow counter
- `xTimeOnEntering` (TickType_t) - Tick count when timeout started

### MemoryRegion_t
**Description:** Structure defining MPU memory regions (when using MPU support).  
**Possible Values:** Structure with fields:
- `pvBaseAddress` (void *) - Region base address
- `ulLengthInBytes` (uint32_t) - Region length
- `ulParameters` (uint32_t) - Region access parameters

### TaskParameters_t
**Description:** Structure for task creation with static allocation and MPU support.  
**Possible Values:** Structure with fields including task function, name, stack size, parameters, priority, stack buffer, and MPU regions.

## Stack and Memory Types

### StackType_t
**Description:** Type used for stack elements.  
**Possible Values:**
- Architecture-dependent: typically `uint32_t`, `uint16_t`, or `uint8_t`
- Represents individual stack memory units
- Arrays of this type hold task stacks

### StaticTask_t
**Description:** Opaque structure for static task allocation.  
**Possible Values:** Platform-specific structure large enough to hold TCB data. Used when `configSUPPORT_STATIC_ALLOCATION` is enabled.

### StaticQueue_t
**Description:** Opaque structure for static queue allocation.  
**Possible Values:** Platform-specific structure large enough to hold queue data.

### StaticSemaphore_t
**Description:** Opaque structure for static semaphore allocation.  
**Possible Values:** Platform-specific structure large enough to hold semaphore data.

### StaticTimer_t
**Description:** Opaque structure for static timer allocation.  
**Possible Values:** Platform-specific structure large enough to hold timer data.

### StaticEventGroup_t
**Description:** Opaque structure for static event group allocation.  
**Possible Values:** Platform-specific structure large enough to hold event group data.

### StaticStreamBuffer_t
**Description:** Opaque structure for static stream buffer allocation.  
**Possible Values:** Platform-specific structure large enough to hold stream buffer data.

## Event and Notification Types

### EventBits_t
**Description:** Type for event group bits.  
**Possible Values:**
- If `configUSE_16_BIT_TICKS` is 1: 8-bit value (bits 0-7 usable)
- If `configUSE_16_BIT_TICKS` is 0: 24-bit value (bits 0-23 usable)
- Bitwise combinations of event flags

### eNotifyAction
**Description:** Enumeration for task notification actions.  
**Possible Values:**
- `eNoAction` (0) - No action, just increment notification count
- `eSetBits` (1) - Set bits in notification value
- `eIncrement` (2) - Increment notification value
- `eSetValueWithOverwrite` (3) - Set value with overwrite
- `eSetValueWithoutOverwrite` (4) - Set value without overwrite

## Constant Values

### Common Return Values
- `pdTRUE` / `pdPASS` = 1 (success)
- `pdFALSE` / `pdFAIL` = 0 (failure)
- `errQUEUE_FULL` = 0 (queue is full)
- `errQUEUE_EMPTY` = 0 (queue is empty)
- `portMAX_DELAY` = Maximum TickType_t value (infinite wait)