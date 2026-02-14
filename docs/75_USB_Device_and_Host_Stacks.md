# USB Device and Host Stacks in FreeRTOS

**Key Topics:**
- USB fundamentals and architecture layers
- Device and Host stack implementations
- CDC, HID, and MSC device class implementations
- FreeRTOS integration patterns (interrupt-driven, task notifications)
- Task priority configuration and interrupt management

**Code Examples Include:**
- **C/C++**: CDC virtual COM port, HID keyboard, MSC mass storage, USB Host for flash drives
- **Rust**: Async CDC with Embassy, HID mouse with usb-device, MSC with RTIC

**Best Practices Covered:**
- Interrupt priority configuration
- DMA buffer alignment
- Error handling and recovery
- Power management (suspend/resume)
- Thread safety with mutexes
- Debugging strategies

The document provides production-ready examples suitable for STM32, TinyUSB, and other common embedded platforms.


## Table of Contents
1. [Introduction](#introduction)
2. [USB Fundamentals](#usb-fundamentals)
3. [USB Device Stack Architecture](#usb-device-stack-architecture)
4. [USB Host Stack Architecture](#usb-host-stack-architecture)
5. [USB Device Classes](#usb-device-classes)
6. [FreeRTOS Integration](#freertos-integration)
7. [Task Priorities and Interrupt Management](#task-priorities-and-interrupt-management)
8. [Implementation Examples in C/C++](#implementation-examples-in-cc)
9. [Implementation Examples in Rust](#implementation-examples-in-rust)
10. [Best Practices](#best-practices)
11. [Summary](#summary)

---

## Introduction

USB (Universal Serial Bus) functionality in embedded systems enables versatile communication with PCs, peripherals, and other USB-enabled devices. FreeRTOS applications often require USB support for:

- **Device mode**: Acting as a USB peripheral (e.g., virtual COM port, mass storage, keyboard/mouse)
- **Host mode**: Controlling USB peripherals (e.g., reading flash drives, connecting keyboards)
- **On-The-Go (OTG)**: Switching between device and host modes

This document covers integrating USB stacks with FreeRTOS, implementing common device classes (CDC, HID, MSC), and managing the unique challenges of USB interrupt timing and task coordination.

---

## USB Fundamentals

### USB Architecture Layers

```
┌─────────────────────────────────┐
│   Application Layer             │
│   (CDC, HID, MSC classes)       │
├─────────────────────────────────┤
│   USB Class Driver Layer        │
├─────────────────────────────────┤
│   USB Core Layer                │
│   (Enumeration, Control)        │
├─────────────────────────────────┤
│   USB Hardware Abstraction      │
├─────────────────────────────────┤
│   Hardware (USB PHY/Controller) │
└─────────────────────────────────┘
```

### Key USB Concepts

- **Endpoints**: Communication channels (IN/OUT directions)
- **Pipes**: Logical connection between host and device endpoints
- **Descriptors**: Data structures describing device capabilities
- **Enumeration**: Process where host discovers and configures device
- **Transfers**: Bulk, Interrupt, Isochronous, Control

### USB Speed Modes

- **Low Speed**: 1.5 Mbps (keyboards, mice)
- **Full Speed**: 12 Mbps (audio, HID devices)
- **High Speed**: 480 Mbps (storage, video)
- **Super Speed**: 5 Gbps (USB 3.0+)

---

## USB Device Stack Architecture

### Core Components

```c
// USB Device Stack Components
typedef struct {
    USBCore_Handle core;          // USB core driver
    USBDeviceClass* class_driver; // Class-specific driver (CDC/HID/MSC)
    USBDescriptors descriptors;   // Device, config, string descriptors
    USBEndpoints endpoints;       // Endpoint configuration
    USBState state;               // Current device state
} USBDevice_Handle;
```

### Device States

```
DETACHED → ATTACHED → POWERED → DEFAULT → 
ADDRESS → CONFIGURED → SUSPENDED
```

### Common USB Device Libraries

- **TinyUSB**: Lightweight, cross-platform
- **STM32 USB Device Library**: STM32-specific
- **Synopsys USB IP**: Common in ARM SoCs
- **Microchip Harmony USB**: PIC32/SAM platforms

---

## USB Host Stack Architecture

### Core Components

```c
// USB Host Stack Components
typedef struct {
    USBCore_Handle core;           // USB core driver
    USBHostClass* class_drivers;   // Array of class drivers
    USBDevice* connected_devices;  // Connected device list
    USBHubInfo hub_info;          // Hub enumeration data
    USBHostState state;           // Host state
} USBHost_Handle;
```

### Host Responsibilities

1. **Device Detection**: Detect device attachment/detachment
2. **Enumeration**: Read descriptors, assign address
3. **Configuration**: Set device configuration
4. **Class Driver Loading**: Match and load appropriate drivers
5. **Data Transfer**: Manage IN/OUT transfers
6. **Power Management**: Control VBUS, suspend/resume

---

## USB Device Classes

### 1. CDC (Communications Device Class)

Used for virtual COM ports, serial communication, and networking.

**Typical Use Cases**:
- Debug console over USB
- Firmware update interface
- Modem emulation
- Network adapter (CDC-ECM/NCM)

**Key Features**:
- Control interface (endpoint 0 + interrupt endpoint)
- Data interface (bulk IN/OUT endpoints)
- Line coding support (baud rate, parity, stop bits)
- Control line states (DTR, RTS)

### 2. HID (Human Interface Device)

Used for keyboards, mice, game controllers, and custom input devices.

**Typical Use Cases**:
- Custom keyboards/keypads
- Position sensors
- Volume controls
- Game controllers

**Key Features**:
- Report descriptors define data format
- Interrupt endpoints for low latency
- No device drivers needed on most OSes
- Boot protocol for BIOS compatibility

### 3. MSC (Mass Storage Class)

Implements USB flash drive functionality.

**Typical Use Cases**:
- SD card reader
- Internal flash as storage
- Data logging device
- Firmware update via file copy

**Key Features**:
- Bulk-only transport (BOT)
- SCSI command set
- FAT/exFAT filesystem support
- Read/write block operations

---

## FreeRTOS Integration

### Challenges with USB and RTOS

1. **Timing Constraints**: USB has strict timing requirements (SOF every 1ms)
2. **Interrupt Handling**: USB interrupts must be serviced quickly
3. **Buffer Management**: DMA and packet buffers need careful synchronization
4. **Task Coordination**: Multiple tasks may access USB simultaneously

### Integration Patterns

#### Pattern 1: Interrupt-Driven with Task Notification

```c
// USB ISR signals task via notification
void USB_IRQHandler(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t int_status = USB_GetInterruptStatus();
    
    if (int_status & USB_INT_RXREADY) {
        // Signal USB task
        vTaskNotifyGiveFromISR(usb_task_handle, &xHigherPriorityTaskWoken);
    }
    
    USB_ClearInterrupts(int_status);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void usb_task(void *params) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        USB_ProcessEvents();
    }
}
```

#### Pattern 2: Direct ISR Processing with Deferred Work

```c
// Critical work in ISR, deferred work in task
void USB_IRQHandler(void) {
    // Immediate: Read data from hardware FIFO to buffer
    usb_rx_buffer[usb_rx_head++] = USB_ReadFIFO();
    
    // Deferred: Signal processing task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(usb_queue, &usb_rx_head, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

---

## Task Priorities and Interrupt Management

### Priority Assignment Guidelines

```c
// Priority hierarchy for USB system
#define USB_ISR_PRIORITY           5  // NVIC priority (lower = higher)
#define USB_SERVICE_TASK_PRIORITY  4  // FreeRTOS priority (higher = higher)
#define USB_CLASS_TASK_PRIORITY    3
#define USB_APPLI_TASK_PRIORITY    2
#define IDLE_TASK_PRIORITY         0

// Ensure USB ISR can preempt all tasks
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
```

### Interrupt Configuration

**Key Principles**:
1. USB ISR priority must be ≤ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`
2. USB service task should have high priority but below critical real-time tasks
3. Class-specific tasks can have lower priorities
4. Use interrupt nesting carefully

### Buffer and Queue Sizing

```c
// Queue sizing based on USB endpoints
#define USB_RX_QUEUE_LENGTH    16  // Number of packets
#define USB_TX_QUEUE_LENGTH    8
#define USB_PACKET_SIZE        64  // Full Speed max packet

// DMA buffers (must be in DMA-accessible memory)
static uint8_t usb_rx_buffer[USB_PACKET_SIZE] __attribute__((aligned(4)));
static uint8_t usb_tx_buffer[USB_PACKET_SIZE] __attribute__((aligned(4)));
```

---

## Implementation Examples in C/C++

### Example 1: CDC Virtual COM Port (TinyUSB)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tusb.h"

// Configuration
#define CDC_TASK_PRIORITY      3
#define CDC_TASK_STACK_SIZE    512
#define CDC_RX_QUEUE_SIZE      32

// Globals
static TaskHandle_t cdc_task_handle;
static QueueHandle_t cdc_rx_queue;

// USB Device Descriptors
tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_CDC,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCAFE,
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// Configuration Descriptor
uint8_t const desc_configuration[] = {
    // Configuration: Interface count, string index, total length, attributes, power
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    
    // CDC Interface: Interface number, string index, EP notification, EP data out, EP data in
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, 
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

// TinyUSB Callbacks
void tud_cdc_rx_cb(uint8_t itf) {
    // Called when data arrives
    uint8_t buf[64];
    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));
    
    // Send to processing queue
    for (uint32_t i = 0; i < count; i++) {
        xQueueSend(cdc_rx_queue, &buf[i], 0);
    }
}

void tud_mount_cb(void) {
    // Device mounted (enumerated)
    configPRINTF(("USB CDC Mounted\r\n"));
}

void tud_umount_cb(void) {
    // Device unmounted
    configPRINTF(("USB CDC Unmounted\r\n"));
}

// CDC Processing Task
void cdc_task(void *params) {
    uint8_t rx_char;
    uint8_t tx_buffer[128];
    int tx_index = 0;
    
    while (1) {
        // Process received characters
        if (xQueueReceive(cdc_rx_queue, &rx_char, pdMS_TO_TICKS(10))) {
            // Echo character
            tud_cdc_write_char(rx_char);
            tud_cdc_write_flush();
            
            // Buffer for line processing
            if (rx_char == '\n' || rx_char == '\r') {
                tx_buffer[tx_index] = '\0';
                // Process command in tx_buffer
                process_command((char*)tx_buffer);
                tx_index = 0;
            } else if (tx_index < sizeof(tx_buffer) - 1) {
                tx_buffer[tx_index++] = rx_char;
            }
        }
        
        // TinyUSB device task
        tud_task();
    }
}

// Initialization
void usb_cdc_init(void) {
    // Create queue
    cdc_rx_queue = xQueueCreate(CDC_RX_QUEUE_SIZE, sizeof(uint8_t));
    configASSERT(cdc_rx_queue);
    
    // Initialize TinyUSB
    tusb_init();
    
    // Create task
    xTaskCreate(cdc_task, "CDC", CDC_TASK_STACK_SIZE, 
                NULL, CDC_TASK_PRIORITY, &cdc_task_handle);
}

// Command processor example
void process_command(char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        const char* help = "Available commands:\r\n"
                           "  help  - Show this message\r\n"
                           "  stats - Show system stats\r\n";
        tud_cdc_write_str(help);
        tud_cdc_write_flush();
    } else if (strcmp(cmd, "stats") == 0) {
        TaskStatus_t *tasks;
        UBaseType_t task_count;
        char stats_buffer[512];
        
        task_count = uxTaskGetNumberOfTasks();
        tasks = pvPortMalloc(task_count * sizeof(TaskStatus_t));
        
        if (tasks) {
            task_count = uxTaskGetSystemState(tasks, task_count, NULL);
            snprintf(stats_buffer, sizeof(stats_buffer),
                     "Tasks: %lu, Heap free: %u bytes\r\n",
                     task_count, xPortGetFreeHeapSize());
            tud_cdc_write_str(stats_buffer);
            vPortFree(tasks);
        }
        tud_cdc_write_flush();
    }
}
```

### Example 2: HID Keyboard (STM32 HAL)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "usbd_hid.h"

// HID Report Descriptor for keyboard
uint8_t HID_ReportDesc[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x05, 0x07,  //   Usage Page (Key Codes)
    0x19, 0xE0,  //   Usage Minimum (224)
    0x29, 0xE7,  //   Usage Maximum (231)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x01,  //   Logical Maximum (1)
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x08,  //   Report Count (8)
    0x81, 0x02,  //   Input (Data, Variable, Absolute) - Modifier byte
    0x95, 0x01,  //   Report Count (1)
    0x75, 0x08,  //   Report Size (8)
    0x81, 0x01,  //   Input (Constant) - Reserved byte
    0x95, 0x06,  //   Report Count (6)
    0x75, 0x08,  //   Report Size (8)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x65,  //   Logical Maximum (101)
    0x05, 0x07,  //   Usage Page (Key codes)
    0x19, 0x00,  //   Usage Minimum (0)
    0x29, 0x65,  //   Usage Maximum (101)
    0x81, 0x00,  //   Input (Data, Array) - Key arrays
    0xC0         // End Collection
};

// HID Report structure
typedef struct {
    uint8_t modifiers;  // Ctrl, Shift, Alt, GUI
    uint8_t reserved;   // Always 0
    uint8_t keys[6];    // Up to 6 simultaneous keys
} HID_KeyboardReport_t;

// Globals
static SemaphoreHandle_t hid_mutex;
static HID_KeyboardReport_t keyboard_report = {0};
extern USBD_HandleTypeDef hUsbDeviceFS;

// HID Send Report
HAL_StatusTypeDef HID_SendKeyboard(uint8_t modifier, uint8_t key) {
    HAL_StatusTypeDef status;
    
    // Take mutex
    if (xSemaphoreTake(hid_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return HAL_TIMEOUT;
    }
    
    // Prepare report
    keyboard_report.modifiers = modifier;
    keyboard_report.keys[0] = key;
    
    // Send report
    status = USBD_HID_SendReport(&hUsbDeviceFS, 
                                 (uint8_t*)&keyboard_report, 
                                 sizeof(keyboard_report));
    
    // Small delay for host to process
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Send key release
    memset(&keyboard_report, 0, sizeof(keyboard_report));
    status = USBD_HID_SendReport(&hUsbDeviceFS, 
                                 (uint8_t*)&keyboard_report, 
                                 sizeof(keyboard_report));
    
    xSemaphoreGive(hid_mutex);
    return status;
}

// HID Task - sends periodic reports
void hid_keyboard_task(void *params) {
    const char* message = "Hello World!";
    uint8_t key_map[256] = {0}; // ASCII to HID keycode mapping (simplified)
    
    // Initialize key map (simplified example)
    key_map['A'] = 0x04; key_map['a'] = 0x04;
    key_map['B'] = 0x05; key_map['b'] = 0x05;
    // ... (full mapping needed for production)
    key_map[' '] = 0x2C;
    
    while (1) {
        // Wait for USB configured
        while (!USBD_HID_IsConfigured(&hUsbDeviceFS)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Type message
        for (int i = 0; message[i] != '\0'; i++) {
            uint8_t modifier = 0;
            uint8_t key = key_map[(uint8_t)message[i]];
            
            // Check for shift needed (uppercase)
            if (message[i] >= 'A' && message[i] <= 'Z') {
                modifier = 0x02; // Left Shift
            }
            
            if (key != 0) {
                HID_SendKeyboard(modifier, key);
            }
            
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Wait before repeating
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Initialize HID
void usb_hid_init(void) {
    hid_mutex = xSemaphoreCreateMutex();
    configASSERT(hid_mutex);
    
    xTaskCreate(hid_keyboard_task, "HID_KB", 512, 
                NULL, 3, NULL);
}
```

### Example 3: MSC (Mass Storage) with SD Card

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "ff.h"  // FatFS
#include "usbd_msc.h"

// MSC Configuration
#define MSC_BLOCK_SIZE        512
#define MSC_BLOCK_COUNT       (SD_CARD_SIZE / MSC_BLOCK_SIZE)
#define MSC_VENDOR_ID         "MyVendor"
#define MSC_PRODUCT_ID        "USB Storage"
#define MSC_REVISION          "1.0 "

// Storage interface
static SemaphoreHandle_t storage_mutex;
extern SD_HandleTypeDef hsd;

// SCSI Inquiry Data
int8_t STORAGE_GetCapacity(uint8_t lun, uint32_t *block_num, 
                           uint16_t *block_size) {
    *block_num = MSC_BLOCK_COUNT;
    *block_size = MSC_BLOCK_SIZE;
    return 0;
}

int8_t STORAGE_IsReady(uint8_t lun) {
    // Check if SD card is present and ready
    if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER) {
        return 0; // Ready
    }
    return -1; // Not ready
}

int8_t STORAGE_IsWriteProtected(uint8_t lun) {
    return 0; // Not write protected
}

int8_t STORAGE_Read(uint8_t lun, uint8_t *buf, uint32_t blk_addr, 
                    uint16_t blk_len) {
    HAL_StatusTypeDef status;
    
    // Take mutex to prevent concurrent access
    if (xSemaphoreTake(storage_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return -1;
    }
    
    // Read from SD card
    status = HAL_SD_ReadBlocks(&hsd, buf, blk_addr, blk_len, 1000);
    
    xSemaphoreGive(storage_mutex);
    
    return (status == HAL_OK) ? 0 : -1;
}

int8_t STORAGE_Write(uint8_t lun, uint8_t *buf, uint32_t blk_addr, 
                     uint16_t blk_len) {
    HAL_StatusTypeDef status;
    
    if (xSemaphoreTake(storage_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return -1;
    }
    
    // Write to SD card
    status = HAL_SD_WriteBlocks(&hsd, buf, blk_addr, blk_len, 1000);
    
    xSemaphoreGive(storage_mutex);
    
    return (status == HAL_OK) ? 0 : -1;
}

int8_t STORAGE_GetMaxLun(void) {
    return 0; // Single LUN
}

// Storage operations structure
USBD_StorageTypeDef USBD_Storage_fops = {
    STORAGE_GetCapacity,
    STORAGE_IsReady,
    STORAGE_IsWriteProtected,
    STORAGE_Read,
    STORAGE_Write,
    STORAGE_GetMaxLun,
    (int8_t*)MSC_VENDOR_ID,
    (int8_t*)MSC_PRODUCT_ID,
    (int8_t*)MSC_REVISION
};

// Data logging task (writes to filesystem)
void data_logger_task(void *params) {
    FATFS fs;
    FIL file;
    FRESULT res;
    char buffer[128];
    uint32_t log_counter = 0;
    
    // Wait for USB to be disconnected (can't access while MSC active)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        // Check if USB is disconnected
        if (!USBD_MSC_IsConnected()) {
            // Mount filesystem
            res = f_mount(&fs, "", 1);
            if (res == FR_OK) {
                // Open log file
                res = f_open(&file, "datalog.txt", 
                            FA_WRITE | FA_CREATE_ALWAYS);
                if (res == FR_OK) {
                    // Write data
                    snprintf(buffer, sizeof(buffer), 
                            "Log entry %lu: Free heap %u bytes\n",
                            log_counter++, xPortGetFreeHeapSize());
                    
                    UINT bytes_written;
                    f_write(&file, buffer, strlen(buffer), &bytes_written);
                    f_close(&file);
                }
                f_mount(NULL, "", 0); // Unmount
            }
        }
    }
}

// Initialize MSC
void usb_msc_init(void) {
    storage_mutex = xSemaphoreCreateMutex();
    configASSERT(storage_mutex);
    
    // Initialize SD card
    HAL_SD_Init(&hsd);
    
    // Create logging task
    xTaskCreate(data_logger_task, "Logger", 512, NULL, 2, NULL);
}
```

### Example 4: USB Host - Reading Flash Drive

```c
#include "FreeRTOS.h"
#include "task.h"
#include "usbh_core.h"
#include "usbh_msc.h"
#include "ff.h"

// USB Host Handle
USBH_HandleTypeDef hUsbHostFS;

// Application States
typedef enum {
    APP_IDLE,
    APP_CONNECTED,
    APP_READY,
    APP_READING,
    APP_DISCONNECTED
} AppState_t;

static volatile AppState_t app_state = APP_IDLE;

// USB Host Callbacks
void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id) {
    switch(id) {
        case HOST_USER_SELECT_CONFIGURATION:
            break;
            
        case HOST_USER_CLASS_ACTIVE:
            app_state = APP_READY;
            configPRINTF(("MSC Device Connected\r\n"));
            break;
            
        case HOST_USER_CONNECTION:
            app_state = APP_CONNECTED;
            configPRINTF(("Device Attached\r\n"));
            break;
            
        case HOST_USER_DISCONNECTION:
            app_state = APP_DISCONNECTED;
            configPRINTF(("Device Disconnected\r\n"));
            break;
            
        default:
            break;
    }
}

// USB Host Task
void usb_host_task(void *params) {
    FATFS fs;
    FIL file;
    DIR dir;
    FILINFO fno;
    char buffer[256];
    UINT bytes_read;
    
    while (1) {
        // Process USB events
        USBH_Process(&hUsbHostFS);
        
        if (app_state == APP_READY) {
            // Mount USB filesystem
            if (f_mount(&fs, "USB:", 1) == FR_OK) {
                // List files
                if (f_opendir(&dir, "USB:") == FR_OK) {
                    configPRINTF(("Files on USB drive:\r\n"));
                    
                    while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                        configPRINTF(("  %s (%lu bytes)\r\n", 
                                    fno.fname, fno.fsize));
                        
                        // Read config.txt if exists
                        if (strcmp(fno.fname, "config.txt") == 0) {
                            if (f_open(&file, "USB:/config.txt", FA_READ) == FR_OK) {
                                f_read(&file, buffer, sizeof(buffer) - 1, &bytes_read);
                                buffer[bytes_read] = '\0';
                                configPRINTF(("Config file content:\r\n%s\r\n", buffer));
                                f_close(&file);
                            }
                        }
                    }
                    f_closedir(&dir);
                }
                f_mount(NULL, "USB:", 0);
            }
            
            app_state = APP_IDLE; // Prevent repeated operations
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Initialize USB Host
void usb_host_init(void) {
    // Initialize USB Host
    USBH_Init(&hUsbHostFS, USBH_UserProcess, 0);
    
    // Register MSC class
    USBH_RegisterClass(&hUsbHostFS, USBH_MSC_CLASS);
    
    // Start USB Host
    USBH_Start(&hUsbHostFS);
    
    // Create task
    xTaskCreate(usb_host_task, "USB_Host", 1024, NULL, 3, NULL);
}
```

---

## Implementation Examples in Rust

### Example 1: CDC Virtual COM with Embassy (Async)

```rust
#![no_std]
#![no_main]

use embassy_executor::Spawner;
use embassy_stm32::usb::{Driver, Instance};
use embassy_usb::class::cdc_acm::{CdcAcmClass, State};
use embassy_usb::{Builder, Config};
use embassy_sync::blocking_mutex::raw::ThreadModeRawMutex;
use embassy_sync::channel::Channel;

// Static channel for received data
static CDC_CHANNEL: Channel<ThreadModeRawMutex, u8, 64> = Channel::new();

// USB Configuration
fn usb_config() -> Config<'static> {
    let mut config = Config::new(0xcafe, 0x4001);
    config.manufacturer = Some("MyCompany");
    config.product = Some("CDC Serial");
    config.serial_number = Some("12345678");
    config.max_power = 100;
    config.max_packet_size_0 = 64;
    config
}

#[embassy_executor::task]
async fn usb_task(mut usb: embassy_usb::UsbDevice<'static, Driver<'static, USB>>) {
    usb.run().await;
}

#[embassy_executor::task]
async fn cdc_task(class: CdcAcmClass<'static, Driver<'static, USB>>) {
    let (mut sender, mut receiver) = class.split();
    
    // RX handler
    let rx_fut = async {
        loop {
            let mut buf = [0u8; 64];
            match receiver.read_packet(&mut buf).await {
                Ok(n) => {
                    // Echo received data
                    let _ = sender.write_packet(&buf[..n]).await;
                    
                    // Send to channel for processing
                    for &byte in &buf[..n] {
                        CDC_CHANNEL.send(byte).await;
                    }
                }
                Err(_) => {}
            }
        }
    };
    
    // TX handler (periodic status messages)
    let tx_fut = async {
        let mut counter = 0u32;
        loop {
            embassy_time::Timer::after(embassy_time::Duration::from_secs(5)).await;
            
            let msg = format!("Status update {}\r\n", counter);
            let _ = sender.write_packet(msg.as_bytes()).await;
            counter += 1;
        }
    };
    
    embassy_futures::join::join(rx_fut, tx_fut).await;
}

#[embassy_executor::task]
async fn command_processor_task() {
    let mut buffer = [0u8; 128];
    let mut index = 0;
    
    loop {
        let byte = CDC_CHANNEL.receive().await;
        
        if byte == b'\n' || byte == b'\r' {
            // Process command
            if index > 0 {
                process_command(&buffer[..index]).await;
                index = 0;
            }
        } else if index < buffer.len() {
            buffer[index] = byte;
            index += 1;
        }
    }
}

async fn process_command(cmd: &[u8]) {
    match cmd {
        b"help" => {
            defmt::info!("Help command received");
        }
        b"reset" => {
            defmt::info!("Reset command received");
            cortex_m::peripheral::SCB::sys_reset();
        }
        _ => {
            defmt::info!("Unknown command: {:?}", cmd);
        }
    }
}

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    let p = embassy_stm32::init(Default::default());
    
    // Create USB driver
    let driver = Driver::new(p.USB, Irqs, p.PA12, p.PA11);
    
    // USB builder
    let config = usb_config();
    let mut config_descriptor = [0; 256];
    let mut bos_descriptor = [0; 256];
    let mut control_buf = [0; 64];
    let mut state = State::new();
    
    let mut builder = Builder::new(
        driver,
        config,
        &mut config_descriptor,
        &mut bos_descriptor,
        &mut [], // No MSOS descriptors
        &mut control_buf,
    );
    
    // Create CDC-ACM class
    let class = CdcAcmClass::new(&mut builder, &mut state, 64);
    
    // Build USB device
    let usb = builder.build();
    
    // Spawn tasks
    spawner.spawn(usb_task(usb)).unwrap();
    spawner.spawn(cdc_task(class)).unwrap();
    spawner.spawn(command_processor_task()).unwrap();
}
```

### Example 2: HID Mouse with USB-Device Crate

```rust
#![no_std]
#![no_main]

use cortex_m::peripheral::NVIC;
use cortex_m_rt::entry;
use stm32f4xx_hal::{
    pac::{interrupt, Interrupt},
    prelude::*,
    usb::{Peripheral, UsbBus},
};
use usb_device::prelude::*;
use usbd_hid::descriptor::{MouseReport, SerializedDescriptor};
use usbd_hid::hid_class::HIDClass;
use cortex_m::interrupt::Mutex;
use core::cell::RefCell;

// Global USB device
static USB_DEVICE: Mutex<RefCell<Option<UsbDevice<UsbBus<Peripheral>>>>> = 
    Mutex::new(RefCell::new(None));
static USB_HID: Mutex<RefCell<Option<HIDClass<UsbBus<Peripheral>>>>> = 
    Mutex::new(RefCell::new(None));

#[entry]
fn main() -> ! {
    let dp = stm32f4xx_hal::pac::Peripherals::take().unwrap();
    let cp = cortex_m::Peripherals::take().unwrap();
    
    // Clock configuration
    let rcc = dp.RCC.constrain();
    let clocks = rcc.cfgr.use_hse(25.MHz()).sysclk(84.MHz()).require_pll48clk().freeze();
    
    // USB peripheral
    let gpioa = dp.GPIOA.split();
    let usb = Peripheral {
        usb_global: dp.OTG_FS_GLOBAL,
        usb_device: dp.OTG_FS_DEVICE,
        usb_pwrclk: dp.OTG_FS_PWRCLK,
        pin_dm: gpioa.pa11.into_alternate(),
        pin_dp: gpioa.pa12.into_alternate(),
        hclk: clocks.hclk(),
    };
    
    let usb_bus = UsbBus::new(usb, unsafe { &mut EP_MEMORY });
    
    // HID class
    let usb_hid = HIDClass::new(&usb_bus, MouseReport::desc(), 10);
    
    // USB device
    let usb_dev = UsbDeviceBuilder::new(&usb_bus, UsbVidPid(0x16c0, 0x27dd))
        .manufacturer("MyCompany")
        .product("HID Mouse")
        .serial_number("12345")
        .device_class(0)
        .build();
    
    // Store in globals
    cortex_m::interrupt::free(|cs| {
        USB_DEVICE.borrow(cs).replace(Some(usb_dev));
        USB_HID.borrow(cs).replace(Some(usb_hid));
    });
    
    // Enable USB interrupt
    unsafe {
        NVIC::unmask(Interrupt::OTG_FS);
    }
    
    let mut delay = cp.SYST.delay(&clocks);
    let mut x: i8 = 0;
    let mut y: i8 = 0;
    let mut direction = 1i8;
    
    loop {
        // Move mouse in square pattern
        for _ in 0..20 {
            x += direction;
            send_mouse_report(x, 0, 0);
            delay.delay_ms(50u32);
        }
        
        for _ in 0..20 {
            y += direction;
            send_mouse_report(0, y, 0);
            delay.delay_ms(50u32);
        }
        
        for _ in 0..20 {
            x -= direction;
            send_mouse_report(-x, 0, 0);
            delay.delay_ms(50u32);
        }
        
        for _ in 0..20 {
            y -= direction;
            send_mouse_report(0, -y, 0);
            delay.delay_ms(50u32);
        }
    }
}

fn send_mouse_report(x: i8, y: i8, buttons: u8) {
    let report = MouseReport {
        x,
        y,
        buttons,
        wheel: 0,
        pan: 0,
    };
    
    cortex_m::interrupt::free(|cs| {
        if let Some(ref mut hid) = USB_HID.borrow(cs).borrow_mut().as_mut() {
            hid.push_input(&report).ok();
        }
    });
}

#[interrupt]
fn OTG_FS() {
    cortex_m::interrupt::free(|cs| {
        if let Some(ref mut usb_dev) = USB_DEVICE.borrow(cs).borrow_mut().as_mut() {
            if let Some(ref mut hid) = USB_HID.borrow(cs).borrow_mut().as_mut() {
                usb_dev.poll(&mut [hid]);
            }
        }
    });
}

static mut EP_MEMORY: [u32; 1024] = [0; 1024];

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
```

### Example 3: Async MSC with RTIC

```rust
#![no_std]
#![no_main]

use rtic::app;
use stm32f4xx_hal::{
    pac,
    prelude::*,
    usb::{Peripheral, UsbBus},
};
use usb_device::prelude::*;
use usbd_mass_storage::{MassStorage, MassStorageClass};

const BLOCK_SIZE: usize = 512;
const BLOCK_COUNT: u32 = 1024; // 512KB storage

// Simple RAM-based storage
struct RamStorage {
    data: [u8; BLOCK_SIZE * BLOCK_COUNT as usize],
}

impl RamStorage {
    const fn new() -> Self {
        Self {
            data: [0; BLOCK_SIZE * BLOCK_COUNT as usize],
        }
    }
}

impl usbd_mass_storage::BlockDevice for RamStorage {
    const BLOCK_BYTES: usize = BLOCK_SIZE;
    
    fn read_block(&self, lba: u32, block: &mut [u8]) -> Result<(), ()> {
        if lba >= BLOCK_COUNT {
            return Err(());
        }
        
        let offset = (lba as usize) * BLOCK_SIZE;
        block.copy_from_slice(&self.data[offset..offset + BLOCK_SIZE]);
        Ok(())
    }
    
    fn write_block(&mut self, lba: u32, block: &[u8]) -> Result<(), ()> {
        if lba >= BLOCK_COUNT {
            return Err(());
        }
        
        let offset = (lba as usize) * BLOCK_SIZE;
        self.data[offset..offset + BLOCK_SIZE].copy_from_slice(block);
        Ok(())
    }
    
    fn max_lba(&self) -> u32 {
        BLOCK_COUNT - 1
    }
}

#[app(device = stm32f4xx_hal::pac, peripherals = true, dispatchers = [EXTI0])]
mod app {
    use super::*;
    
    #[shared]
    struct Shared {
        usb_dev: UsbDevice<'static, UsbBus<Peripheral>>,
        mass_storage: MassStorageClass<'static, UsbBus<Peripheral>, RamStorage>,
    }
    
    #[local]
    struct Local {}
    
    #[init]
    fn init(ctx: init::Context) -> (Shared, Local, init::Monotonics) {
        static mut USB_BUS: Option<usb_device::bus::UsbBusAllocator<UsbBus<Peripheral>>> = None;
        static mut STORAGE: RamStorage = RamStorage::new();
        
        let dp = ctx.device;
        
        // Clock setup
        let rcc = dp.RCC.constrain();
        let clocks = rcc.cfgr.use_hse(25.MHz())
            .sysclk(84.MHz())
            .require_pll48clk()
            .freeze();
        
        // USB setup
        let gpioa = dp.GPIOA.split();
        let usb = Peripheral {
            usb_global: dp.OTG_FS_GLOBAL,
            usb_device: dp.OTG_FS_DEVICE,
            usb_pwrclk: dp.OTG_FS_PWRCLK,
            pin_dm: gpioa.pa11.into_alternate(),
            pin_dp: gpioa.pa12.into_alternate(),
            hclk: clocks.hclk(),
        };
        
        unsafe {
            USB_BUS = Some(UsbBus::new(usb, &mut EP_MEMORY));
        }
        
        let mass_storage = MassStorageClass::new(
            unsafe { USB_BUS.as_ref().unwrap() },
            unsafe { &mut *core::ptr::addr_of_mut!(STORAGE) },
        );
        
        let usb_dev = UsbDeviceBuilder::new(
            unsafe { USB_BUS.as_ref().unwrap() },
            UsbVidPid(0x16c0, 0x27dd),
        )
        .manufacturer("MyCompany")
        .product("USB Storage")
        .serial_number("12345")
        .device_class(0)
        .build();
        
        (
            Shared {
                usb_dev,
                mass_storage,
            },
            Local {},
            init::Monotonics(),
        )
    }
    
    #[task(binds = OTG_FS, shared = [usb_dev, mass_storage])]
    fn usb_interrupt(ctx: usb_interrupt::Context) {
        let mut usb_dev = ctx.shared.usb_dev;
        let mut mass_storage = ctx.shared.mass_storage;
        
        (usb_dev, mass_storage).lock(|usb_dev, mass_storage| {
            usb_dev.poll(&mut [mass_storage]);
        });
    }
}

static mut EP_MEMORY: [u32; 1024] = [0; 1024];
```

---

## Best Practices

### 1. Interrupt Priority Configuration

```c
// Configure interrupt priorities correctly
void configure_usb_interrupts(void) {
    // USB interrupt should be above FreeRTOS max syscall priority
    HAL_NVIC_SetPriority(OTG_FS_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
    
    // Ensure FreeRTOS can mask it
    configASSERT(5 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
}
```

### 2. Buffer Alignment and DMA

```c
// Ensure buffers are properly aligned for DMA
#define USB_BUFFER_ALIGN __attribute__((aligned(4)))

USB_BUFFER_ALIGN static uint8_t usb_rx_buffer[USB_MAX_PACKET_SIZE];
USB_BUFFER_ALIGN static uint8_t usb_tx_buffer[USB_MAX_PACKET_SIZE];

// For STM32, ensure buffers are in specific RAM sections
#define USB_DMA_SECTION __attribute__((section(".dma_buffer")))
```

### 3. Error Handling and Recovery

```c
// USB error recovery
void usb_error_handler(USB_ErrorCode_t error) {
    switch (error) {
        case USB_ERR_TIMEOUT:
            // Reset USB stack
            USB_DeInit();
            vTaskDelay(pdMS_TO_TICKS(100));
            USB_Init();
            break;
            
        case USB_ERR_STALL:
            // Clear stall condition
            USB_ClearStall();
            break;
            
        case USB_ERR_NAK:
            // Retry operation
            break;
            
        default:
            // Log error
            log_error("USB Error: %d", error);
            break;
    }
}
```

### 4. Power Management

```c
// USB suspend/resume handling
void usb_suspend_callback(void) {
    // Enter low power mode
    USB_EnterLowPowerMode();
    
    // Suspend FreeRTOS scheduler or use tickless idle
    vTaskSuspendAll();
    
    // Wait for resume
    __WFI();
}

void usb_resume_callback(void) {
    // Exit low power mode
    USB_ExitLowPowerMode();
    
    // Resume scheduler
    xTaskResumeAll();
}
```

### 5. Thread Safety

```c
// Protect USB operations with mutex
SemaphoreHandle_t usb_mutex;

void usb_safe_send(const uint8_t *data, size_t len) {
    if (xSemaphoreTake(usb_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        USB_Transmit(data, len);
        xSemaphoreGive(usb_mutex);
    }
}
```

### 6. Enumeration Timeout Handling

```c
// Wait for enumeration with timeout
bool wait_for_usb_configured(uint32_t timeout_ms) {
    TickType_t start = xTaskGetTickCount();
    
    while (!USB_IsConfigured()) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(timeout_ms)) {
            return false; // Timeout
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    return true;
}
```

### 7. Debugging and Logging

```c
// USB event logging
#define USB_LOG_EVENTS 1

#if USB_LOG_EVENTS
void log_usb_event(const char *event) {
    TickType_t timestamp = xTaskGetTickCount();
    printf("[%lu] USB: %s\n", timestamp, event);
}
#else
#define log_usb_event(x) ((void)0)
#endif
```

---

## Summary

USB integration in FreeRTOS systems requires careful attention to:

1. **Stack Selection**: Choose appropriate USB device/host libraries (TinyUSB, vendor-specific)

2. **Device Classes**: 
   - **CDC**: Virtual serial ports for debug/communication
   - **HID**: User input devices, custom controls
   - **MSC**: Mass storage for data logging, firmware updates

3. **RTOS Integration**:
   - Use task notifications or queues for ISR-to-task communication
   - Set appropriate interrupt priorities (must work with FreeRTOS)
   - Protect shared resources with mutexes
   - Size buffers and queues appropriately

4. **Task Priority Design**:
   - USB service task: High priority (4-5)
   - Class-specific tasks: Medium priority (3)
   - Application tasks: Lower priority (2)
   - Critical: USB ISR ≤ `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`

5. **Best Practices**:
   - Ensure DMA buffer alignment
   - Implement robust error handling and recovery
   - Handle suspend/resume for power management
   - Use thread-safe operations with mutexes
   - Implement enumeration timeouts
   - Enable comprehensive logging for debugging

6. **Language Considerations**:
   - **C/C++**: Mature ecosystem, extensive vendor libraries, direct hardware control
   - **Rust**: Memory safety, async/await patterns, growing embedded support (Embassy, RTIC)

USB in embedded systems bridges the gap between microcontrollers and the PC/peripheral ecosystem, enabling debug interfaces, data transfer, human interaction, and storage capabilities essential for modern IoT and embedded applications.