# FreeRTOS Naming Convention - Complete Prefix Guide

FreeRTOS uses a systematic prefix-based naming convention to improve code readability and indicate the scope, type, or module of identifiers.

## Function Prefixes

Functions are prefixed based on **return type** and **module**:

### Return Type Prefixes:
- **`v`** - Functions returning `void` (e.g., `vTaskDelay`, `vTaskDelete`)
- **`x`** - Functions returning non-void types like handles, BaseType_t, or status codes (e.g., `xTaskCreate`, `xQueueSend`)
- **`prv`** - Private/static functions local to a file (e.g., `prvInitialiseTaskLists`)

### Module Prefixes (combined with return type):
- **`vTask`** / **`xTask`** - Task management API
- **`vQueue`** / **`xQueue`** - Queue API
- **`vSemaphore`** / **`xSemaphore`** - Semaphore API
- **`xTimer`** / **`vTimer`** - Software timer API
- **`xEventGroup`** - Event group API
- **`xStream`** - Stream buffer API
- **`xMessageBuffer`** - Message buffer API

## Variable Prefixes

Variables use **type-based** prefixes:

- **`c`** - char
- **`s`** - short (int16_t)
- **`l`** - long (int32_t)
- **`x`** - BaseType_t, TickType_t, or handle types
- **`u`** - unsigned (combined: `uc`, `us`, `ul`, `ux`)
- **`p`** - pointer (e.g., `pcTaskName` = pointer to char)
- **`pp`** - pointer to pointer
- **`e`** - enumeration type (e.g., `eTaskState`)

### Examples:
- `xTaskHandle` - handle variable
- `pcTaskName` - pointer to char (string)
- `ulTimeout` - unsigned long
- `eCurrentState` - enumeration variable
- `pxQueue` - pointer to a structure/handle

## Macro Prefixes

- **`port`** - Port-specific macros (e.g., `portMAX_DELAY`, `portYIELD`)
- **`config`** - Configuration macros in FreeRTOSConfig.h (e.g., `configUSE_PREEMPTION`, `configTICK_RATE_HZ`)
- **`pd`** - Project defines/constants (e.g., `pdTRUE`, `pdFALSE`, `pdPASS`, `pdFAIL`)
- **`err`** - Error codes (e.g., `errQUEUE_FULL`, `errQUEUE_EMPTY`)
- **`task`** - Task-related macros (e.g., `taskENTER_CRITICAL`, `taskYIELD`)

## Type Definition Prefixes

- Structures and types typically end with **`_t`** (e.g., `TaskHandle_t`, `QueueHandle_t`)
- No specific prefix for structure names, but they follow a descriptive naming pattern

## General Naming Rules

1. **Macros and constants**: UPPER_CASE with underscores
2. **Functions and variables**: camelCase
3. **File-scope static variables**: Often prefixed with `x` or appropriate type prefix
4. **ISR versions**: Functions callable from ISR often end with **`FromISR`** (e.g., `xQueueSendFromISR`)

## Quick Reference Table

| Prefix | Meaning | Example |
|--------|---------|---------|
| `v` | void function | `vTaskDelay()` |
| `x` | non-void function | `xTaskCreate()` |
| `prv` | private/static function | `prvIdleTask()` |
| `c` | char variable | `cChar` |
| `s` | short variable | `sValue` |
| `l` | long variable | `lCounter` |
| `x` | BaseType_t/handle variable | `xResult` |
| `u` | unsigned | `ulCount` |
| `p` | pointer | `pxQueue` |
| `e` | enumeration | `eState` |
| `port` | port-specific macro | `portMAX_DELAY` |
| `config` | configuration macro | `configUSE_PREEMPTION` |
| `pd` | project define | `pdTRUE` |
| `err` | error code | `errQUEUE_FULL` |

This convention makes FreeRTOS code self-documenting and helps developers quickly understand the type, scope, and purpose of identifiers at a glance.