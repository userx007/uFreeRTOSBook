# MPU (Memory Protection Unit) Support in FreeRTOS

## Overview

The Memory Protection Unit (MPU) is a hardware component available on many ARM Cortex-M processors that enables memory protection by controlling access permissions to different memory regions. FreeRTOS provides MPU support to create a more secure and robust embedded system by isolating tasks and preventing unauthorized memory access.

## Key Concepts

### What the MPU Does

The MPU allows you to:
- Define memory regions with specific access permissions (read, write, execute)
- Separate privileged and unprivileged code execution
- Protect critical system resources from accidental or malicious corruption
- Catch memory access violations through hardware exceptions
- Implement task isolation for safety-critical systems

### Privileged vs Unprivileged Tasks

**Privileged Tasks:**
- Run in privileged mode with full access to all system resources
- Can access all memory regions and peripherals
- Can execute privileged instructions
- Typically used for critical system services

**Unprivileged Tasks:**
- Run in unprivileged (user) mode with restricted access
- Can only access memory regions explicitly granted to them
- Cannot execute privileged instructions
- Ideal for application code that doesn't need system-level access

## Configuration

### Enabling MPU Support

To use FreeRTOS with MPU support, you need to:

1. **Use an MPU-enabled port** - FreeRTOS provides specific MPU ports for ARM Cortex-M3/M4/M7 processors
2. **Enable MPU in FreeRTOSConfig.h:**

```c
#define configUSE_MPU_WRAPPERS_V1  1
#define configTOTAL_MPU_REGIONS    8  /* Number of MPU regions available */
#define configTEX_S_C_B_FLASH      0x07UL  /* Flash memory attributes */
#define configTEX_S_C_B_SRAM       0x07UL  /* SRAM memory attributes */
```

3. **Define memory regions:**

```c
/* Define accessible memory regions */
#define configENABLE_ACCESS_CONTROL_LIST  1
```

## Creating MPU-Protected Tasks

### Task Creation with MPU Regions

When creating unprivileged tasks, you define memory regions they can access:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "mpu_wrappers.h"

/* Define memory regions for the task */
static const MemoryRegion_t xAltRegions[portNUM_CONFIGURABLE_REGIONS] = 
{
    /* Base Address    Length                       Parameters */
    { ucSharedMemory,  32,                          portMPU_REGION_READ_WRITE },
    { ucTaskMemory,    1024,                        portMPU_REGION_READ_WRITE },
    { 0,               0,                           0 }  /* End of array marker */
};

/* Task parameters structure */
TaskParameters_t xTaskParameters = 
{
    .pvTaskCode     = vUnprivilegedTask,
    .pcName         = "Unprivileged",
    .usStackDepth   = 256,
    .pvParameters   = NULL,
    .uxPriority     = 2,
    .puxStackBuffer = xTaskStack,
    .xRegions       = xAltRegions
};

void setup_tasks(void)
{
    TaskHandle_t xHandle;
    
    /* Create restricted (unprivileged) task */
    xTaskCreateRestricted(&xTaskParameters, &xHandle);
}
```

### Unprivileged Task Example

```c
/* This task runs in unprivileged mode */
void vUnprivilegedTask(void *pvParameters)
{
    uint32_t ulCounter = 0;
    
    for(;;)
    {
        /* Can access memory defined in xAltRegions */
        ucSharedMemory[0] = ulCounter++;
        
        /* This would cause a memory fault if uncommented:
         * *((uint32_t*)0x40000000) = 0;  // Accessing unauthorized peripheral
         */
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### Privileged Task Example

```c
/* Create a privileged task using standard xTaskCreate */
void vPrivilegedTask(void *pvParameters)
{
    for(;;)
    {
        /* Can access all system resources */
        /* Can call privileged functions */
        
        /* Perform critical system operations */
        configureHardware();
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup_privileged_task(void)
{
    /* Standard task creation - runs in privileged mode */
    xTaskCreate(vPrivilegedTask, "Privileged", 256, NULL, 3, NULL);
}
```

## Memory Region Definitions

### Region Parameters

Memory regions are configured with specific attributes:

```c
/* Common region parameter definitions */
#define portMPU_REGION_READ_WRITE              (0x03UL << 24UL)
#define portMPU_REGION_PRIVILEGED_READ_ONLY    (0x05UL << 24UL)
#define portMPU_REGION_READ_ONLY               (0x06UL << 24UL)
#define portMPU_REGION_PRIVILEGED_READ_WRITE   (0x01UL << 24UL)
#define portMPU_REGION_CACHEABLE_BUFFERABLE    (0x07UL << 16UL)
#define portMPU_REGION_EXECUTE_NEVER           (0x01UL << 28UL)
```

### Complete MPU Configuration Example

```c
#include "FreeRTOS.h"
#include "task.h"
#include "mpu_wrappers.h"

/* Shared memory buffer accessible by unprivileged tasks */
static uint8_t ucSharedMemory[512] __attribute__((aligned(512)));

/* Task-specific memory */
static uint8_t ucTask1Memory[256] __attribute__((aligned(256)));
static uint8_t ucTask2Memory[256] __attribute__((aligned(256)));

/* Stack buffers for unprivileged tasks */
static StackType_t xTask1Stack[256] __attribute__((aligned(256)));
static StackType_t xTask2Stack[256] __attribute__((aligned(256)));

/* Unprivileged Task 1 */
void vTask1(void *pvParameters)
{
    for(;;)
    {
        /* Access own memory */
        ucTask1Memory[0]++;
        
        /* Access shared memory */
        ucSharedMemory[0] = ucTask1Memory[0];
        
        /* Cannot access Task 2's memory - would fault */
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* Unprivileged Task 2 */
void vTask2(void *pvParameters)
{
    for(;;)
    {
        /* Access own memory */
        ucTask2Memory[0]++;
        
        /* Access shared memory */
        ucSharedMemory[1] = ucTask2Memory[0];
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void create_mpu_tasks(void)
{
    TaskHandle_t xHandle;
    
    /* Configure Task 1 regions */
    static const MemoryRegion_t xTask1Regions[portNUM_CONFIGURABLE_REGIONS] = 
    {
        { ucSharedMemory, 512, portMPU_REGION_READ_WRITE },
        { ucTask1Memory,  256, portMPU_REGION_READ_WRITE },
        { 0, 0, 0 }
    };
    
    TaskParameters_t xTask1Parameters = 
    {
        .pvTaskCode     = vTask1,
        .pcName         = "Task1",
        .usStackDepth   = 256,
        .pvParameters   = NULL,
        .uxPriority     = 2,
        .puxStackBuffer = xTask1Stack,
        .xRegions       = xTask1Regions
    };
    
    /* Configure Task 2 regions */
    static const MemoryRegion_t xTask2Regions[portNUM_CONFIGURABLE_REGIONS] = 
    {
        { ucSharedMemory, 512, portMPU_REGION_READ_WRITE },
        { ucTask2Memory,  256, portMPU_REGION_READ_WRITE },
        { 0, 0, 0 }
    };
    
    TaskParameters_t xTask2Parameters = 
    {
        .pvTaskCode     = vTask2,
        .pcName         = "Task2",
        .usStackDepth   = 256,
        .pvParameters   = NULL,
        .uxPriority     = 2,
        .puxStackBuffer = xTask2Stack,
        .xRegions       = xTask2Regions
    };
    
    /* Create the restricted tasks */
    xTaskCreateRestricted(&xTask1Parameters, &xHandle);
    xTaskCreateRestricted(&xTask2Parameters, &xHandle);
}
```

## Protected API Calls

### MPU Wrappers

FreeRTOS provides MPU wrapper functions that allow unprivileged tasks to safely call kernel APIs:

```c
/* Unprivileged task making protected API calls */
void vUnprivilegedTask(void *pvParameters)
{
    QueueHandle_t xQueue;
    uint32_t ulValue = 0;
    
    /* These API calls are automatically wrapped */
    for(;;)
    {
        /* Send to queue - uses MPU wrapper internally */
        xQueueSend(xQueue, &ulValue, portMAX_DELAY);
        
        /* Delay - uses MPU wrapper */
        vTaskDelay(pdMS_TO_TICKS(100));
        
        ulValue++;
    }
}
```

### Custom Privileged Functions

You can create functions that temporarily elevate to privileged mode:

```c
/* Function that needs privileged access */
void vPrivilegedFunction(void)
{
    /* This function runs in privileged mode even when called
     * from an unprivileged task */
    
    /* Access hardware registers */
    *((volatile uint32_t*)0x40000000) = 0x12345678;
    
    /* Perform other privileged operations */
}

/* Declare the function as privileged */
PRIVILEGED_FUNCTION void vPrivilegedFunction(void);
```

## Practical Example: Sensor Data Processing System

Here's a complete example showing MPU usage in a sensor system:

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "mpu_wrappers.h"

/* Shared sensor data buffer */
typedef struct {
    uint16_t temperature;
    uint16_t pressure;
    uint16_t humidity;
} SensorData_t;

static SensorData_t xSensorBuffer __attribute__((aligned(32)));
static uint8_t ucProcessedData[1024] __attribute__((aligned(1024)));

/* Stacks for unprivileged tasks */
static StackType_t xSensorStack[512] __attribute__((aligned(512)));
static StackType_t xProcessStack[512] __attribute__((aligned(512)));

/* Privileged task - reads hardware sensors */
void vSensorReadTask(void *pvParameters)
{
    for(;;)
    {
        /* Direct hardware access (privileged) */
        xSensorBuffer.temperature = read_temperature_sensor();
        xSensorBuffer.pressure = read_pressure_sensor();
        xSensorBuffer.humidity = read_humidity_sensor();
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Unprivileged task - processes sensor data */
void vDataProcessTask(void *pvParameters)
{
    for(;;)
    {
        /* Can read sensor buffer */
        uint16_t temp = xSensorBuffer.temperature;
        
        /* Process data in protected memory region */
        ucProcessedData[0] = (temp >> 8) & 0xFF;
        ucProcessedData[1] = temp & 0xFF;
        
        /* Cannot access hardware directly - would fault */
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void init_sensor_system(void)
{
    /* Create privileged sensor reading task */
    xTaskCreate(vSensorReadTask, "SensorRead", 512, NULL, 3, NULL);
    
    /* Configure regions for unprivileged processing task */
    static const MemoryRegion_t xProcessRegions[portNUM_CONFIGURABLE_REGIONS] = 
    {
        { &xSensorBuffer,  sizeof(SensorData_t), portMPU_REGION_READ_ONLY },
        { ucProcessedData, 1024,                 portMPU_REGION_READ_WRITE },
        { 0, 0, 0 }
    };
    
    TaskParameters_t xProcessParams = 
    {
        .pvTaskCode     = vDataProcessTask,
        .pcName         = "DataProcess",
        .usStackDepth   = 512,
        .pvParameters   = NULL,
        .uxPriority     = 2,
        .puxStackBuffer = xProcessStack,
        .xRegions       = xProcessRegions
    };
    
    TaskHandle_t xHandle;
    xTaskCreateRestricted(&xProcessParams, &xHandle);
}
```

## Benefits and Use Cases

**Safety-Critical Systems:**
- Medical devices requiring task isolation
- Automotive systems with ASIL requirements
- Industrial control systems

**Security Applications:**
- IoT devices with untrusted code execution
- Systems running third-party plugins
- Multi-tenant embedded systems

**Debugging Benefits:**
- Catch buffer overflows immediately
- Detect stack corruption early
- Identify unauthorized memory access

## Important Considerations

1. **Hardware Requirement:** MPU support requires compatible ARM Cortex-M processors (M3, M4, M7, M33, M55)

2. **Memory Alignment:** MPU regions must be properly aligned based on their size

3. **Region Limitations:** Most processors support 8 MPU regions, with FreeRTOS using some for system purposes

4. **Performance Overhead:** Minimal overhead for context switches with MPU reconfiguration

5. **Debugging:** MPU faults generate HardFault exceptions that need proper handlers for debugging

The MPU support in FreeRTOS provides a powerful mechanism for creating more secure and robust embedded systems by enforcing memory protection at the hardware level, making it essential for safety-critical and security-sensitive applications.