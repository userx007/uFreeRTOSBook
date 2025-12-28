# Building and Using FreeRTOS as a Linux Application for x86

FreeRTOS provides a **POSIX/Linux simulator port** that allows you to run FreeRTOS applications as regular Linux processes on x86 (both 32-bit and 64-bit) systems. This is invaluable for testing, debugging, and learning FreeRTOS concepts without needing embedded hardware.

## Overview of the Linux Port

The FreeRTOS POSIX port uses **pthread** (POSIX threads) to simulate FreeRTOS tasks. Each FreeRTOS task runs as a separate pthread, and the FreeRTOS scheduler is implemented using pthread synchronization primitives. This approach provides:

- Fast development and testing cycles
- Access to powerful debugging tools (gdb, valgrind, sanitizers)
- Easy integration with Linux development environments
- No hardware requirements

## Setting Up the Environment

### Prerequisites

You'll need:
- GCC compiler (gcc or clang)
- Make or CMake
- POSIX threads library (usually included with glibc)
- Git (to clone FreeRTOS)

```bash
# On Ubuntu/Debian
sudo apt-get install build-essential git

# On Fedora/RHEL
sudo dnf install gcc make git
```

### Obtaining FreeRTOS

```bash
# Clone the FreeRTOS repository
git clone https://github.com/FreeRTOS/FreeRTOS.git
cd FreeRTOS

# The POSIX port is located at:
# FreeRTOS/Source/portable/ThirdParty/GCC/Posix/
```

## Project Structure

A typical FreeRTOS Linux project structure:

```
my_freertos_project/
├── main.c                 # Your application code
├── FreeRTOSConfig.h      # Configuration file
├── Makefile              # Build configuration
└── FreeRTOS/             # FreeRTOS source
    ├── Source/
    │   ├── tasks.c
    │   ├── queue.c
    │   ├── timers.c
    │   └── portable/
    │       └── ThirdParty/GCC/Posix/
    └── Demo/Posix_GCC/    # Example demo
```

## FreeRTOSConfig.h Configuration

This file configures FreeRTOS behavior. Here's a typical configuration for Linux:

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Scheduling configuration */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configTICK_RATE_HZ                      1000
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                2048
#define configMAX_TASK_NAME_LEN                 12

/* Memory allocation */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (100 * 1024)  // 100KB

/* Hook functions */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2

/* Co-routine definitions */
#define configUSE_CO_ROUTINES                   0

/* Software timer definitions */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               2
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE

/* Optional functions */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetCurrentTaskHandle       1

/* POSIX-specific settings */
#define configUSE_POSIX_ERRNO                   1

#endif /* FREERTOS_CONFIG_H */
```

## Example 1: Basic Task Creation

Here's a simple example demonstrating task creation and synchronization:

```c
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>

/* Task function prototypes */
void vTask1(void *pvParameters);
void vTask2(void *pvParameters);

int main(void)
{
    printf("FreeRTOS Linux Demo Starting...\n");

    /* Create two tasks */
    xTaskCreate(
        vTask1,              /* Task function */
        "Task1",             /* Task name */
        1000,                /* Stack size (words) */
        NULL,                /* Task parameter */
        1,                   /* Priority */
        NULL                 /* Task handle */
    );

    xTaskCreate(
        vTask2,
        "Task2",
        1000,
        NULL,
        1,
        NULL
    );

    /* Start the scheduler */
    vTaskStartScheduler();

    /* Should never reach here */
    printf("Scheduler failed to start!\n");
    return 0;
}

void vTask1(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        printf("Task 1 is running\n");
        vTaskDelay(pdMS_TO_TICKS(1000));  // Delay 1 second
    }
}

void vTask2(void *pvParameters)
{
    (void)pvParameters;
    
    for(;;)
    {
        printf("Task 2 is running\n");
        vTaskDelay(pdMS_TO_TICKS(1500));  // Delay 1.5 seconds
    }
}
```

## Example 2: Queue Communication

This example shows inter-task communication using queues:

```c
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#define QUEUE_LENGTH    5
#define ITEM_SIZE       sizeof(uint32_t)

QueueHandle_t xQueue;

void vProducerTask(void *pvParameters);
void vConsumerTask(void *pvParameters);

int main(void)
{
    printf("Queue Example Starting...\n");

    /* Create a queue */
    xQueue = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);

    if(xQueue != NULL)
    {
        /* Create producer and consumer tasks */
        xTaskCreate(vProducerTask, "Producer", 1000, NULL, 2, NULL);
        xTaskCreate(vConsumerTask, "Consumer", 1000, NULL, 1, NULL);

        /* Start scheduler */
        vTaskStartScheduler();
    }
    else
    {
        printf("Queue creation failed!\n");
    }

    return 0;
}

void vProducerTask(void *pvParameters)
{
    (void)pvParameters;
    uint32_t ulValueToSend = 0;
    BaseType_t xStatus;

    for(;;)
    {
        xStatus = xQueueSend(xQueue, &ulValueToSend, pdMS_TO_TICKS(100));

        if(xStatus == pdPASS)
        {
            printf("Producer: Sent %lu\n", ulValueToSend);
            ulValueToSend++;
        }
        else
        {
            printf("Producer: Queue full!\n");
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vConsumerTask(void *pvParameters)
{
    (void)pvParameters;
    uint32_t ulReceivedValue;
    BaseType_t xStatus;

    for(;;)
    {
        xStatus = xQueueReceive(xQueue, &ulReceivedValue, pdMS_TO_TICKS(100));

        if(xStatus == pdPASS)
        {
            printf("Consumer: Received %lu\n", ulReceivedValue);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## Example 3: Semaphores for Synchronization

Binary semaphore example for synchronizing tasks:

```c
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

SemaphoreHandle_t xBinarySemaphore;

void vPeriodicTask(void *pvParameters);
void vHandlerTask(void *pvParameters);

int main(void)
{
    printf("Semaphore Example Starting...\n");

    /* Create binary semaphore */
    xBinarySemaphore = xSemaphoreCreateBinary();

    if(xBinarySemaphore != NULL)
    {
        xTaskCreate(vPeriodicTask, "Periodic", 1000, NULL, 2, NULL);
        xTaskCreate(vHandlerTask, "Handler", 1000, NULL, 1, NULL);

        vTaskStartScheduler();
    }

    return 0;
}

void vPeriodicTask(void *pvParameters)
{
    (void)pvParameters;

    for(;;)
    {
        printf("Periodic: Generating event...\n");
        xSemaphoreGive(xBinarySemaphore);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void vHandlerTask(void *pvParameters)
{
    (void)pvParameters;

    for(;;)
    {
        /* Wait for semaphore (block indefinitely) */
        xSemaphoreTake(xBinarySemaphore, portMAX_DELAY);
        printf("Handler: Event received and processed!\n");
    }
}
```

## Makefile for Building

Here's a comprehensive Makefile:

```makefile
# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -I. -IFreeRTOS/Source/include \
         -IFreeRTOS/Source/portable/ThirdParty/GCC/Posix
LDFLAGS = -pthread -lrt

# FreeRTOS source files
FREERTOS_SRC = \
    FreeRTOS/Source/tasks.c \
    FreeRTOS/Source/queue.c \
    FreeRTOS/Source/list.c \
    FreeRTOS/Source/timers.c \
    FreeRTOS/Source/portable/ThirdParty/GCC/Posix/port.c \
    FreeRTOS/Source/portable/MemMang/heap_3.c

# Your application source
APP_SRC = main.c

# Object files
OBJS = $(FREERTOS_SRC:.c=.o) $(APP_SRC:.c=.o)

# Target executable
TARGET = freertos_app

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
```

## Building and Running

```bash
# Build the project
make

# Run the application
./freertos_app

# Or use make run
make run

# Clean build artifacts
make clean
```

## Debugging with GDB

One major advantage of the Linux port is easy debugging:

```bash
# Compile with debug symbols
gcc -g -Wall -pthread main.c [other files] -o freertos_app

# Run with GDB
gdb ./freertos_app

# GDB commands
(gdb) break main
(gdb) run
(gdb) info threads      # Show all threads (FreeRTOS tasks)
(gdb) thread 2          # Switch to thread 2
(gdb) backtrace         # Show call stack
(gdb) continue
```

## Using Valgrind for Memory Analysis

```bash
# Check for memory leaks
valgrind --leak-check=full ./freertos_app

# Check for threading issues
valgrind --tool=helgrind ./freertos_app
```

## Important Considerations

**Timing Differences**: The Linux simulator doesn't provide real-time guarantees. Task timing is approximate and depends on Linux scheduling.

**Stack Size**: Stack sizes in FreeRTOSConfig.h are in words (typically 4 bytes on 32-bit, 8 bytes on 64-bit). On Linux, you may need larger stacks than on embedded systems.

**Heap Selection**: The POSIX port typically uses heap_3.c, which wraps malloc/free. This is different from embedded systems that might use other heap implementations.

**Signal Handling**: The POSIX port uses signals (SIGALRM, SIGUSR1) internally for task switching. Avoid interfering with these in your application.

**Exit Behavior**: To cleanly exit a FreeRTOS Linux application, you can call `exit(0)` from any task, though this isn't typical for embedded systems.

## Advanced Example: Software Timers

```c
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

TimerHandle_t xTimer;
int timerCount = 0;

void vTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    timerCount++;
    printf("Timer fired! Count: %d\n", timerCount);
    
    if(timerCount >= 10)
    {
        printf("Stopping timer after 10 iterations\n");
        xTimerStop(xTimer, 0);
    }
}

int main(void)
{
    printf("Software Timer Example\n");

    /* Create a periodic timer (2 second period) */
    xTimer = xTimerCreate(
        "MyTimer",                    // Timer name
        pdMS_TO_TICKS(2000),         // Period (2 seconds)
        pdTRUE,                       // Auto-reload
        (void *)0,                    // Timer ID
        vTimerCallback                // Callback function
    );

    if(xTimer != NULL)
    {
        xTimerStart(xTimer, 0);
        vTaskStartScheduler();
    }

    return 0;
}
```

## Benefits for Learning

The Linux port is excellent for learning because you can:

1. Quickly iterate on code without flashing hardware
2. Use printf debugging extensively
3. Leverage powerful debugging and profiling tools
4. Test complex scenarios easily
5. Learn FreeRTOS concepts before moving to embedded targets
6. Develop unit tests for your RTOS applications

This approach significantly reduces the learning curve and development time when working with FreeRTOS concepts.

# Organizing Multiple FreeRTOS Linux Projects with Shared Sources

When working with multiple FreeRTOS projects, you'll want to avoid duplicating the FreeRTOS source code while keeping each project independent. Here are several proven approaches:

## Approach 1: Centralized FreeRTOS Installation (Recommended)

This approach keeps one FreeRTOS installation that all projects reference.

### Directory Structure

```
~/development/
├── freertos/                          # Shared FreeRTOS sources
│   ├── FreeRTOS/
│   │   └── Source/
│   │       ├── include/
│   │       ├── portable/
│   │       ├── tasks.c
│   │       ├── queue.c
│   │       └── ...
│   └── README.md
│
├── projects/
│   ├── project1/
│   │   ├── src/
│   │   │   └── main.c
│   │   ├── include/
│   │   │   └── FreeRTOSConfig.h
│   │   ├── Makefile
│   │   └── build/
│   │
│   ├── project2/
│   │   ├── src/
│   │   │   └── main.c
│   │   ├── include/
│   │   │   └── FreeRTOSConfig.h
│   │   ├── Makefile
│   │   └── build/
│   │
│   └── project3/
│       └── ...
```

### Project Makefile with Shared Sources

Each project has its own Makefile that references the shared FreeRTOS location:

```makefile
# Project configuration
PROJECT_NAME = project1
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include

# FreeRTOS paths (adjust to your installation)
FREERTOS_ROOT = ../../freertos/FreeRTOS
FREERTOS_SRC = $(FREERTOS_ROOT)/Source
FREERTOS_PORT = $(FREERTOS_SRC)/portable/ThirdParty/GCC/Posix
FREERTOS_INC = $(FREERTOS_SRC)/include

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g \
         -I$(INC_DIR) \
         -I$(FREERTOS_INC) \
         -I$(FREERTOS_PORT)

LDFLAGS = -pthread -lrt

# FreeRTOS source files
FREERTOS_SOURCES = \
    $(FREERTOS_SRC)/tasks.c \
    $(FREERTOS_SRC)/queue.c \
    $(FREERTOS_SRC)/list.c \
    $(FREERTOS_SRC)/timers.c \
    $(FREERTOS_SRC)/event_groups.c \
    $(FREERTOS_PORT)/port.c \
    $(FREERTOS_SRC)/portable/MemMang/heap_3.c

# Project source files
PROJECT_SOURCES = $(wildcard $(SRC_DIR)/*.c)

# All sources
SOURCES = $(FREERTOS_SOURCES) $(PROJECT_SOURCES)

# Object files (placed in build directory)
FREERTOS_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(FREERTOS_SOURCES:.c=.o)))
PROJECT_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(PROJECT_SOURCES:.c=.o)))
OBJS = $(FREERTOS_OBJS) $(PROJECT_OBJS)

# Target executable
TARGET = $(BUILD_DIR)/$(PROJECT_NAME)

# VPATH for finding source files
VPATH = $(SRC_DIR):$(dir $(FREERTOS_SOURCES))

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
    mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
    $(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(BUILD_DIR)/%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -rf $(BUILD_DIR)

run: $(TARGET)
    $(TARGET)

.PHONY: all clean run
```

### Environment Variable Approach

Set a system-wide environment variable for the FreeRTOS location:

```bash
# Add to ~/.bashrc or ~/.zshrc
export FREERTOS_ROOT="$HOME/development/freertos/FreeRTOS"
```

Then in your Makefiles:

```makefile
# Use environment variable
FREERTOS_ROOT ?= $(HOME)/development/freertos/FreeRTOS

# If FREERTOS_ROOT is not set, show error
ifeq ($(FREERTOS_ROOT),)
    $(error FREERTOS_ROOT is not set. Please set it to your FreeRTOS installation directory)
endif

FREERTOS_SRC = $(FREERTOS_ROOT)/Source
# ... rest of Makefile
```

## Approach 2: CMake-Based Build System (Modern Approach)

CMake provides better dependency management and is more portable.

### Root CMakeLists.txt for FreeRTOS Library

Create a reusable CMake configuration for FreeRTOS:

```cmake
# ~/development/freertos/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(FreeRTOS_Posix C)

# FreeRTOS source files
set(FREERTOS_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS/Source)
set(FREERTOS_PORT_DIR ${FREERTOS_SRC_DIR}/portable/ThirdParty/GCC/Posix)

set(FREERTOS_SOURCES
    ${FREERTOS_SRC_DIR}/tasks.c
    ${FREERTOS_SRC_DIR}/queue.c
    ${FREERTOS_SRC_DIR}/list.c
    ${FREERTOS_SRC_DIR}/timers.c
    ${FREERTOS_SRC_DIR}/event_groups.c
    ${FREERTOS_PORT_DIR}/port.c
    ${FREERTOS_SRC_DIR}/portable/MemMang/heap_3.c
)

# Create FreeRTOS as a library
add_library(freertos_posix STATIC ${FREERTOS_SOURCES})

target_include_directories(freertos_posix PUBLIC
    ${FREERTOS_SRC_DIR}/include
    ${FREERTOS_PORT_DIR}
)

target_compile_options(freertos_posix PRIVATE
    -Wall
    -Wextra
)

target_link_libraries(freertos_posix PUBLIC
    pthread
    rt
)
```

### Project CMakeLists.txt

```cmake
# ~/development/projects/project1/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)
project(MyFreeRTOSProject C)

set(CMAKE_C_STANDARD 11)

# Point to shared FreeRTOS
set(FREERTOS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../freertos" 
    CACHE PATH "Path to FreeRTOS root directory")

# Add FreeRTOS subdirectory
add_subdirectory(${FREERTOS_ROOT} ${CMAKE_BINARY_DIR}/freertos)

# Project executable
add_executable(${PROJECT_NAME}
    src/main.c
    # Add more source files here
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    freertos_posix
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -g
)
```

### Building with CMake

```bash
# In project directory
mkdir build
cd build
cmake ..
make

# Or specify FreeRTOS location explicitly
cmake -DFREERTOS_ROOT=/path/to/freertos ..
make
```

## Approach 3: Git Submodules

Use Git submodules to include FreeRTOS in each project while maintaining a single source.

### Setup

```bash
# In your project directory
cd ~/development/projects/project1

# Add FreeRTOS as a submodule
git submodule add https://github.com/FreeRTOS/FreeRTOS.git external/FreeRTOS

# Initialize and update
git submodule update --init --recursive
```

### Project Structure

```
project1/
├── src/
│   └── main.c
├── include/
│   └── FreeRTOSConfig.h
├── external/
│   └── FreeRTOS/          # Git submodule
├── Makefile
└── .gitmodules
```

### Makefile for Submodule Approach

```makefile
# FreeRTOS paths relative to project
FREERTOS_ROOT = external/FreeRTOS
FREERTOS_SRC = $(FREERTOS_ROOT)/FreeRTOS/Source
FREERTOS_PORT = $(FREERTOS_SRC)/portable/ThirdParty/GCC/Posix

# Rest of Makefile similar to Approach 1
# ...
```

### Updating Submodules

```bash
# Update FreeRTOS in all projects
cd project1
git submodule update --remote external/FreeRTOS
```

## Approach 4: Symbolic Links

Create symbolic links in each project pointing to the shared FreeRTOS installation.

```bash
# In project directory
cd ~/development/projects/project1
ln -s ../../freertos/FreeRTOS ./FreeRTOS

# Now you can reference it locally
```

### Makefile with Symbolic Link

```makefile
# FreeRTOS is now accessible as if it's in the project
FREERTOS_ROOT = FreeRTOS
FREERTOS_SRC = $(FREERTOS_ROOT)/Source
# ... rest similar to other approaches
```

## Approach 5: Package Manager Style (Advanced)

Create a system-wide installation using a package-like structure.

### Installation Script

```bash
#!/bin/bash
# install_freertos.sh

INSTALL_PREFIX="/usr/local"
FREERTOS_VERSION="10.5.1"

# Install headers
sudo mkdir -p $INSTALL_PREFIX/include/freertos
sudo cp -r FreeRTOS/Source/include/* $INSTALL_PREFIX/include/freertos/
sudo cp -r FreeRTOS/Source/portable/ThirdParty/GCC/Posix/*.h $INSTALL_PREFIX/include/freertos/

# Build and install library
gcc -c -I$INSTALL_PREFIX/include/freertos \
    FreeRTOS/Source/*.c \
    FreeRTOS/Source/portable/ThirdParty/GCC/Posix/port.c \
    FreeRTOS/Source/portable/MemMang/heap_3.c

ar rcs libfreertos.a *.o
sudo cp libfreertos.a $INSTALL_PREFIX/lib/

echo "FreeRTOS installed to $INSTALL_PREFIX"
```

### Project Makefile

```makefile
CFLAGS = -I/usr/local/include/freertos -I./include
LDFLAGS = -L/usr/local/lib -lfreertos -pthread -lrt

$(TARGET): $(PROJECT_OBJS)
    $(CC) $(PROJECT_OBJS) $(LDFLAGS) -o $(TARGET)
```

## Recommended Project Template Structure

Here's a complete template for organizing multiple projects:

```
~/freertos-workspace/
├── freertos/                          # Shared FreeRTOS (git clone)
│   ├── FreeRTOS/
│   └── .git/
│
├── common/                            # Shared utilities across projects
│   ├── utils.c
│   ├── utils.h
│   └── common.mk                      # Common Makefile snippets
│
├── projects/
│   ├── 01-basic-tasks/
│   │   ├── src/
│   │   │   └── main.c
│   │   ├── include/
│   │   │   └── FreeRTOSConfig.h
│   │   ├── Makefile
│   │   └── README.md
│   │
│   ├── 02-queue-demo/
│   │   ├── src/
│   │   ├── include/
│   │   ├── Makefile
│   │   └── README.md
│   │
│   └── 03-semaphore-mutex/
│       └── ...
│
├── build-all.sh                       # Script to build all projects
└── README.md
```

### Common Makefile Include (common.mk)

```makefile
# common/common.mk
# Include this in all project Makefiles

# Workspace root
WORKSPACE_ROOT = $(realpath $(dir $(lastword $(MAKEFILE_LIST)))/..)

# FreeRTOS configuration
FREERTOS_ROOT = $(WORKSPACE_ROOT)/freertos/FreeRTOS
FREERTOS_SRC = $(FREERTOS_ROOT)/Source
FREERTOS_PORT = $(FREERTOS_SRC)/portable/ThirdParty/GCC/Posix
FREERTOS_INC = $(FREERTOS_SRC)/include

# Common includes
COMMON_INC = $(WORKSPACE_ROOT)/common

# Standard compiler flags
COMMON_CFLAGS = -Wall -Wextra -pthread -g \
                -I$(FREERTOS_INC) \
                -I$(FREERTOS_PORT) \
                -I$(COMMON_INC)

COMMON_LDFLAGS = -pthread -lrt

# FreeRTOS sources
FREERTOS_SOURCES = \
    $(FREERTOS_SRC)/tasks.c \
    $(FREERTOS_SRC)/queue.c \
    $(FREERTOS_SRC)/list.c \
    $(FREERTOS_SRC)/timers.c \
    $(FREERTOS_SRC)/event_groups.c \
    $(FREERTOS_PORT)/port.c \
    $(FREERTOS_SRC)/portable/MemMang/heap_3.c
```

### Project Makefile Using common.mk

```makefile
# projects/01-basic-tasks/Makefile

PROJECT_NAME = basic_tasks
BUILD_DIR = build
SRC_DIR = src
INC_DIR = include

# Include common configuration
include ../../common/common.mk

CC = gcc
CFLAGS = $(COMMON_CFLAGS) -I$(INC_DIR)
LDFLAGS = $(COMMON_LDFLAGS)

# Project sources
PROJECT_SOURCES = $(wildcard $(SRC_DIR)/*.c)

# All sources
SOURCES = $(FREERTOS_SOURCES) $(PROJECT_SOURCES)

# Object files
FREERTOS_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(FREERTOS_SOURCES:.c=.o)))
PROJECT_OBJS = $(addprefix $(BUILD_DIR)/, $(notdir $(PROJECT_SOURCES:.c=.o)))
OBJS = $(FREERTOS_OBJS) $(PROJECT_OBJS)

TARGET = $(BUILD_DIR)/$(PROJECT_NAME)

VPATH = $(SRC_DIR):$(dir $(FREERTOS_SOURCES))

all: $(BUILD_DIR) $(TARGET)

$(BUILD_DIR):
    mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJS)
    $(CC) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(BUILD_DIR)/%.o: %.c
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    rm -rf $(BUILD_DIR)

run: $(TARGET)
    $(TARGET)

.PHONY: all clean run
```

### Build All Projects Script

```bash
#!/bin/bash
# build-all.sh

set -e  # Exit on error

WORKSPACE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECTS_DIR="$WORKSPACE_ROOT/projects"

echo "Building all FreeRTOS projects..."
echo "=================================="

for project_dir in "$PROJECTS_DIR"/*/; do
    project_name=$(basename "$project_dir")
    echo ""
    echo "Building: $project_name"
    echo "------------------------"
    
    cd "$project_dir"
    
    if [ -f "Makefile" ]; then
        make clean
        make
        echo "✓ $project_name built successfully"
    else
        echo "✗ No Makefile found in $project_name"
    fi
done

echo ""
echo "=================================="
echo "Build complete!"
```

## Version Management

### Using Specific FreeRTOS Versions

```bash
# In shared freertos directory
cd ~/development/freertos
git checkout V10.5.1  # Checkout specific version

# Or use tags
git tag -l  # List available versions
git checkout tags/V10.5.1
```

### Per-Project Version Control

If different projects need different FreeRTOS versions, use the submodule approach with version specification:

```bash
cd project1/external/FreeRTOS
git checkout V10.5.1

cd ../../project2/external/FreeRTOS
git checkout V11.0.0
```

## Best Practices Summary

1. **Use centralized installation** for most use cases - simplest and most efficient
2. **Use CMake** for complex projects with multiple dependencies
3. **Use Git submodules** when projects need different FreeRTOS versions
4. **Create common configuration files** (common.mk) to reduce duplication
5. **Document the workspace structure** in a root README
6. **Use build scripts** to compile all projects at once
7. **Keep FreeRTOSConfig.h project-specific** - never share it
8. **Version control your setup** - document which FreeRTOS version each project uses

This organization keeps your workspace clean, maintainable, and scalable as you add more projects.