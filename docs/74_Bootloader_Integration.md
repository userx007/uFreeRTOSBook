# FreeRTOS Bootloader Integration

## Overview

Bootloader integration is a critical component in embedded systems running FreeRTOS, enabling secure firmware updates, recovery mechanisms, and system reliability. A bootloader is a small program that runs before the main application, responsible for validating, loading, and executing the correct firmware image.

## Key Concepts

### 1. **Dual-Bank Architecture**

Dual-bank (or dual-slot) bootloaders maintain two separate flash memory regions for firmware:
- **Bank A (Primary)**: Currently running firmware
- **Bank B (Secondary)**: New firmware for updates

This architecture enables:
- Atomic updates (switch between banks)
- Rollback capability if new firmware fails
- No downtime during updates (for some implementations)

### 2. **Bootloader Stages**

Typical boot sequence:
1. **Stage 0**: Hardware initialization
2. **Stage 1**: Bootloader execution (validation, decision)
3. **Stage 2**: Application firmware execution

---

## Detailed Implementation

### C/C++ Implementation

#### 1. Basic Bootloader Structure

```c
// bootloader.h
#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <stdint.h>
#include <stdbool.h>

// Flash memory layout
#define BOOTLOADER_START    0x08000000
#define BOOTLOADER_SIZE     0x00008000  // 32KB
#define APP_BANK_A_START    0x08008000
#define APP_BANK_A_SIZE     0x00074000  // 464KB
#define APP_BANK_B_START    0x0807C000
#define APP_BANK_B_SIZE     0x00074000  // 464KB
#define METADATA_START      0x080F0000
#define METADATA_SIZE       0x00001000  // 4KB

// Firmware metadata structure
typedef struct {
    uint32_t magic;           // Magic number for validation
    uint32_t version;         // Firmware version
    uint32_t size;            // Firmware size in bytes
    uint32_t crc32;           // CRC32 checksum
    uint32_t timestamp;       // Build timestamp
    uint8_t  signature[256];  // Digital signature (RSA-2048)
    uint32_t flags;           // Status flags
} FirmwareMetadata_t;

// Boot status flags
#define FIRMWARE_FLAG_VALID         (1 << 0)
#define FIRMWARE_FLAG_ACTIVE        (1 << 1)
#define FIRMWARE_FLAG_UPDATE_READY  (1 << 2)
#define FIRMWARE_FLAG_ROLLBACK      (1 << 3)

// Boot bank selection
typedef enum {
    BOOT_BANK_A = 0,
    BOOT_BANK_B = 1,
    BOOT_BANK_INVALID = 0xFF
} BootBank_t;

// Function prototypes
bool Bootloader_Init(void);
bool Bootloader_ValidateFirmware(uint32_t bank_addr, FirmwareMetadata_t *metadata);
void Bootloader_JumpToApplication(uint32_t app_addr);
BootBank_t Bootloader_SelectBootBank(void);
bool Bootloader_SwapBanks(void);
bool Bootloader_InstallUpdate(uint8_t *firmware_data, uint32_t size);

#endif // BOOTLOADER_H
```

#### 2. Bootloader Main Logic

```c
// bootloader.c
#include "bootloader.h"
#include "flash_driver.h"
#include "crypto.h"
#include <string.h>

// Magic number for firmware validation
#define FIRMWARE_MAGIC 0xDEADBEEF

// Boot attempt counter
static uint8_t boot_attempt_count = 0;
#define MAX_BOOT_ATTEMPTS 3

/**
 * @brief Initialize bootloader
 */
bool Bootloader_Init(void) {
    // Initialize hardware (clocks, flash, peripherals)
    SystemClock_Config();
    Flash_Init();
    
    // Read boot attempt counter from non-volatile storage
    boot_attempt_count = NVS_ReadBootAttempts();
    
    return true;
}

/**
 * @brief Validate firmware using CRC and signature
 */
bool Bootloader_ValidateFirmware(uint32_t bank_addr, FirmwareMetadata_t *metadata) {
    // Read metadata from flash
    memcpy(metadata, (void*)(bank_addr + APP_BANK_A_SIZE - sizeof(FirmwareMetadata_t)), 
           sizeof(FirmwareMetadata_t));
    
    // Check magic number
    if (metadata->magic != FIRMWARE_MAGIC) {
        return false;
    }
    
    // Verify CRC32
    uint32_t calculated_crc = CRC32_Calculate((uint8_t*)bank_addr, metadata->size);
    if (calculated_crc != metadata->crc32) {
        return false;
    }
    
    // Verify digital signature (optional, for secure boot)
    #ifdef SECURE_BOOT_ENABLED
    if (!Crypto_VerifySignature((uint8_t*)bank_addr, metadata->size, 
                                metadata->signature, sizeof(metadata->signature))) {
        return false;
    }
    #endif
    
    return true;
}

/**
 * @brief Select which bank to boot from
 */
BootBank_t Bootloader_SelectBootBank(void) {
    FirmwareMetadata_t metadata_a, metadata_b;
    bool valid_a, valid_b;
    
    // Validate both banks
    valid_a = Bootloader_ValidateFirmware(APP_BANK_A_START, &metadata_a);
    valid_b = Bootloader_ValidateFirmware(APP_BANK_B_START, &metadata_b);
    
    // Check for update flag in Bank B
    if (valid_b && (metadata_b.flags & FIRMWARE_FLAG_UPDATE_READY)) {
        // Attempt to boot new firmware
        if (boot_attempt_count < MAX_BOOT_ATTEMPTS) {
            boot_attempt_count++;
            NVS_WriteBootAttempts(boot_attempt_count);
            return BOOT_BANK_B;
        } else {
            // Too many failed attempts, rollback to Bank A
            metadata_b.flags &= ~FIRMWARE_FLAG_UPDATE_READY;
            metadata_b.flags |= FIRMWARE_FLAG_ROLLBACK;
            Flash_Write(APP_BANK_B_START + APP_BANK_B_SIZE - sizeof(FirmwareMetadata_t),
                       (uint8_t*)&metadata_b, sizeof(FirmwareMetadata_t));
            boot_attempt_count = 0;
            NVS_WriteBootAttempts(0);
        }
    }
    
    // Boot from Bank A if valid
    if (valid_a) {
        boot_attempt_count = 0;
        NVS_WriteBootAttempts(0);
        return BOOT_BANK_A;
    }
    
    // Boot from Bank B if Bank A is invalid
    if (valid_b) {
        boot_attempt_count = 0;
        NVS_WriteBootAttempts(0);
        return BOOT_BANK_B;
    }
    
    // No valid firmware found
    return BOOT_BANK_INVALID;
}

/**
 * @brief Jump to application firmware
 */
void Bootloader_JumpToApplication(uint32_t app_addr) {
    // Function pointer to application reset handler
    typedef void (*pFunction)(void);
    pFunction JumpToApplication;
    
    // Get the application stack pointer (first entry in vector table)
    uint32_t JumpAddress = *(__IO uint32_t*)(app_addr + 4);
    
    // Get application reset handler address
    JumpToApplication = (pFunction) JumpAddress;
    
    // Disable all interrupts
    __disable_irq();
    
    // Deinitialize peripherals
    HAL_DeInit();
    
    // Set vector table offset
    SCB->VTOR = app_addr;
    
    // Set stack pointer
    __set_MSP(*(__IO uint32_t*)app_addr);
    
    // Jump to application
    JumpToApplication();
    
    // Should never reach here
    while(1);
}

/**
 * @brief Swap firmware banks (make Bank B the active bank)
 */
bool Bootloader_SwapBanks(void) {
    FirmwareMetadata_t metadata_a, metadata_b;
    
    // Read both metadata
    memcpy(&metadata_a, (void*)(APP_BANK_A_START + APP_BANK_A_SIZE - sizeof(FirmwareMetadata_t)),
           sizeof(FirmwareMetadata_t));
    memcpy(&metadata_b, (void*)(APP_BANK_B_START + APP_BANK_B_SIZE - sizeof(FirmwareMetadata_t)),
           sizeof(FirmwareMetadata_t));
    
    // Mark Bank A as inactive
    metadata_a.flags &= ~FIRMWARE_FLAG_ACTIVE;
    metadata_a.flags &= ~FIRMWARE_FLAG_UPDATE_READY;
    
    // Mark Bank B as active
    metadata_b.flags |= FIRMWARE_FLAG_ACTIVE;
    metadata_b.flags &= ~FIRMWARE_FLAG_UPDATE_READY;
    
    // Write updated metadata
    Flash_Erase(APP_BANK_A_START + APP_BANK_A_SIZE - FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    Flash_Write(APP_BANK_A_START + APP_BANK_A_SIZE - sizeof(FirmwareMetadata_t),
               (uint8_t*)&metadata_a, sizeof(FirmwareMetadata_t));
    
    Flash_Erase(APP_BANK_B_START + APP_BANK_B_SIZE - FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    Flash_Write(APP_BANK_B_START + APP_BANK_B_SIZE - sizeof(FirmwareMetadata_t),
               (uint8_t*)&metadata_b, sizeof(FirmwareMetadata_t));
    
    return true;
}

/**
 * @brief Main bootloader entry point
 */
int main(void) {
    BootBank_t boot_bank;
    uint32_t app_address;
    
    // Initialize bootloader
    Bootloader_Init();
    
    // Select boot bank
    boot_bank = Bootloader_SelectBootBank();
    
    if (boot_bank == BOOT_BANK_A) {
        app_address = APP_BANK_A_START;
    } else if (boot_bank == BOOT_BANK_B) {
        app_address = APP_BANK_B_START;
    } else {
        // No valid firmware, enter recovery mode
        Bootloader_EnterRecoveryMode();
        while(1); // Should not reach here
    }
    
    // Jump to application
    Bootloader_JumpToApplication(app_address);
    
    // Should never reach here
    while(1);
    return 0;
}
```

#### 3. OTA Update Implementation (FreeRTOS Task)

```c
// ota_update.c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "bootloader.h"
#include "network.h"

#define OTA_CHUNK_SIZE 4096
#define OTA_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define OTA_TASK_STACK_SIZE 2048

typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_DOWNLOADING,
    OTA_STATE_VERIFYING,
    OTA_STATE_INSTALLING,
    OTA_STATE_REBOOTING,
    OTA_STATE_ERROR
} OTAState_t;

typedef struct {
    OTAState_t state;
    uint32_t total_size;
    uint32_t downloaded_size;
    uint32_t flash_address;
    uint8_t progress_percent;
} OTAContext_t;

static OTAContext_t ota_context = {0};
static QueueHandle_t ota_event_queue;

/**
 * @brief Download firmware chunk over network
 */
static bool OTA_DownloadChunk(uint8_t *buffer, uint32_t offset, uint32_t size) {
    // Implementation depends on network stack (WiFi, Ethernet, cellular)
    // Example: HTTP/HTTPS download
    return HTTP_Download("https://update.server.com/firmware.bin", 
                        buffer, offset, size);
}

/**
 * @brief Install firmware update to Bank B
 */
static bool OTA_InstallUpdate(void) {
    uint8_t chunk_buffer[OTA_CHUNK_SIZE];
    uint32_t offset = 0;
    uint32_t flash_addr = APP_BANK_B_START;
    
    // Erase Bank B
    Flash_EraseBank(APP_BANK_B_START, APP_BANK_B_SIZE);
    
    while (offset < ota_context.total_size) {
        uint32_t chunk_size = (ota_context.total_size - offset) > OTA_CHUNK_SIZE ?
                              OTA_CHUNK_SIZE : (ota_context.total_size - offset);
        
        // Download chunk
        if (!OTA_DownloadChunk(chunk_buffer, offset, chunk_size)) {
            return false;
        }
        
        // Write to flash
        if (!Flash_Write(flash_addr, chunk_buffer, chunk_size)) {
            return false;
        }
        
        offset += chunk_size;
        flash_addr += chunk_size;
        ota_context.downloaded_size = offset;
        ota_context.progress_percent = (offset * 100) / ota_context.total_size;
        
        // Yield to other tasks
        taskYIELD();
    }
    
    return true;
}

/**
 * @brief Verify and finalize update
 */
static bool OTA_FinalizeUpdate(void) {
    FirmwareMetadata_t metadata;
    
    // Validate new firmware
    if (!Bootloader_ValidateFirmware(APP_BANK_B_START, &metadata)) {
        return false;
    }
    
    // Mark as ready for installation
    metadata.flags |= FIRMWARE_FLAG_UPDATE_READY;
    metadata.flags |= FIRMWARE_FLAG_VALID;
    
    // Write metadata
    Flash_Write(APP_BANK_B_START + APP_BANK_B_SIZE - sizeof(FirmwareMetadata_t),
               (uint8_t*)&metadata, sizeof(FirmwareMetadata_t));
    
    return true;
}

/**
 * @brief OTA update task
 */
void OTA_UpdateTask(void *pvParameters) {
    while (1) {
        switch (ota_context.state) {
            case OTA_STATE_IDLE:
                // Wait for update trigger
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
                
            case OTA_STATE_DOWNLOADING:
                if (OTA_InstallUpdate()) {
                    ota_context.state = OTA_STATE_VERIFYING;
                } else {
                    ota_context.state = OTA_STATE_ERROR;
                }
                break;
                
            case OTA_STATE_VERIFYING:
                if (OTA_FinalizeUpdate()) {
                    ota_context.state = OTA_STATE_REBOOTING;
                } else {
                    ota_context.state = OTA_STATE_ERROR;
                }
                break;
                
            case OTA_STATE_REBOOTING:
                // Gracefully shutdown FreeRTOS tasks
                vTaskSuspendAll();
                
                // Perform system reset
                NVIC_SystemReset();
                break;
                
            case OTA_STATE_ERROR:
                // Handle error, notify user
                ota_context.state = OTA_STATE_IDLE;
                break;
        }
    }
}

/**
 * @brief Start OTA update process
 */
bool OTA_StartUpdate(uint32_t firmware_size) {
    if (ota_context.state != OTA_STATE_IDLE) {
        return false;
    }
    
    ota_context.total_size = firmware_size;
    ota_context.downloaded_size = 0;
    ota_context.progress_percent = 0;
    ota_context.state = OTA_STATE_DOWNLOADING;
    
    return true;
}

/**
 * @brief Initialize OTA subsystem
 */
void OTA_Init(void) {
    // Create OTA task
    xTaskCreate(OTA_UpdateTask, "OTA", OTA_TASK_STACK_SIZE, NULL, 
                OTA_TASK_PRIORITY, NULL);
}
```

#### 4. Watchdog-based Recovery

```c
// watchdog_recovery.c
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"

IWDG_HandleTypeDef hiwdg;

/**
 * @brief Initialize independent watchdog
 */
void Watchdog_Init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Reload = 4095; // ~10 seconds timeout
    
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief Watchdog refresh task
 */
void Watchdog_Task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        // Refresh watchdog every 5 seconds
        HAL_IWDG_Refresh(&hiwdg);
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000));
    }
}

/**
 * @brief Check if reset was caused by watchdog
 */
bool Watchdog_WasResetByWatchdog(void) {
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return true;
    }
    return false;
}
```

---

### Rust Implementation

#### 1. Bootloader Core (Rust with embedded-hal)

```rust
// bootloader.rs
#![no_std]
#![no_main]

use core::ptr;
use cortex_m_rt::entry;
use embedded_hal::blocking::delay::DelayMs;

// Flash memory layout constants
const BOOTLOADER_START: u32 = 0x0800_0000;
const BOOTLOADER_SIZE: u32 = 0x0000_8000;
const APP_BANK_A_START: u32 = 0x0800_8000;
const APP_BANK_A_SIZE: u32 = 0x0007_4000;
const APP_BANK_B_START: u32 = 0x0807_C000;
const APP_BANK_B_SIZE: u32 = 0x0007_4000;

const FIRMWARE_MAGIC: u32 = 0xDEAD_BEEF;
const MAX_BOOT_ATTEMPTS: u8 = 3;

#[repr(C)]
#[derive(Clone, Copy)]
struct FirmwareMetadata {
    magic: u32,
    version: u32,
    size: u32,
    crc32: u32,
    timestamp: u32,
    signature: [u8; 256],
    flags: u32,
}

#[repr(u32)]
enum FirmwareFlags {
    Valid = 1 << 0,
    Active = 1 << 1,
    UpdateReady = 1 << 2,
    Rollback = 1 << 3,
}

#[derive(Debug, PartialEq)]
enum BootBank {
    BankA,
    BankB,
    Invalid,
}

struct Bootloader {
    boot_attempt_count: u8,
}

impl Bootloader {
    fn new() -> Self {
        Self {
            boot_attempt_count: 0,
        }
    }

    /// Validate firmware using CRC32
    fn validate_firmware(&self, bank_addr: u32) -> Option<FirmwareMetadata> {
        // Read metadata from end of flash bank
        let metadata_addr = bank_addr + APP_BANK_A_SIZE - 
                           core::mem::size_of::<FirmwareMetadata>() as u32;
        
        let metadata = unsafe {
            ptr::read_volatile(metadata_addr as *const FirmwareMetadata)
        };

        // Validate magic number
        if metadata.magic != FIRMWARE_MAGIC {
            return None;
        }

        // Calculate and verify CRC32
        let calculated_crc = self.calculate_crc32(bank_addr, metadata.size);
        if calculated_crc != metadata.crc32 {
            return None;
        }

        Some(metadata)
    }

    /// Calculate CRC32 checksum
    fn calculate_crc32(&self, addr: u32, size: u32) -> u32 {
        let mut crc: u32 = 0xFFFF_FFFF;
        
        for i in 0..size {
            let byte = unsafe {
                ptr::read_volatile((addr + i) as *const u8)
            };
            
            crc ^= byte as u32;
            for _ in 0..8 {
                if crc & 1 != 0 {
                    crc = (crc >> 1) ^ 0xEDB8_8320;
                } else {
                    crc >>= 1;
                }
            }
        }
        
        !crc
    }

    /// Select which bank to boot from
    fn select_boot_bank(&mut self) -> BootBank {
        let metadata_a = self.validate_firmware(APP_BANK_A_START);
        let metadata_b = self.validate_firmware(APP_BANK_B_START);

        // Check for update flag in Bank B
        if let Some(meta_b) = metadata_b {
            if meta_b.flags & (FirmwareFlags::UpdateReady as u32) != 0 {
                if self.boot_attempt_count < MAX_BOOT_ATTEMPTS {
                    self.boot_attempt_count += 1;
                    // Save boot attempt count to NVS
                    self.save_boot_attempts();
                    return BootBank::BankB;
                } else {
                    // Too many failed attempts, rollback
                    self.boot_attempt_count = 0;
                    self.save_boot_attempts();
                    self.mark_rollback(APP_BANK_B_START);
                }
            }
        }

        // Boot from Bank A if valid
        if metadata_a.is_some() {
            self.boot_attempt_count = 0;
            self.save_boot_attempts();
            return BootBank::BankA;
        }

        // Boot from Bank B if Bank A invalid
        if metadata_b.is_some() {
            self.boot_attempt_count = 0;
            self.save_boot_attempts();
            return BootBank::BankB;
        }

        BootBank::Invalid
    }

    /// Jump to application firmware
    unsafe fn jump_to_application(&self, app_addr: u32) -> ! {
        // Disable interrupts
        cortex_m::interrupt::disable();

        // Get stack pointer and reset handler from vector table
        let stack_ptr = ptr::read_volatile(app_addr as *const u32);
        let reset_handler = ptr::read_volatile((app_addr + 4) as *const u32);

        // Set vector table offset
        const SCB_VTOR: *mut u32 = 0xE000_ED08 as *mut u32;
        ptr::write_volatile(SCB_VTOR, app_addr);

        // Set stack pointer
        cortex_m::register::msp::write(stack_ptr);

        // Jump to application
        let app_entry: extern "C" fn() -> ! = 
            core::mem::transmute(reset_handler);
        app_entry();
    }

    fn save_boot_attempts(&self) {
        // Implementation depends on NVS driver
        // Example: write to backup registers or EEPROM
    }

    fn mark_rollback(&self, bank_addr: u32) {
        // Mark firmware as rolled back
        // Implementation depends on flash driver
    }
}

#[entry]
fn main() -> ! {
    let mut bootloader = Bootloader::new();
    
    // Select boot bank
    let boot_bank = bootloader.select_boot_bank();
    
    let app_address = match boot_bank {
        BootBank::BankA => APP_BANK_A_START,
        BootBank::BankB => APP_BANK_B_START,
        BootBank::Invalid => {
            // Enter recovery mode
            enter_recovery_mode();
            loop {}
        }
    };

    // Jump to application
    unsafe {
        bootloader.jump_to_application(app_address);
    }
}

fn enter_recovery_mode() -> ! {
    // Recovery mode implementation (USB DFU, UART bootloader, etc.)
    loop {
        // Wait for recovery commands
    }
}

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
```

#### 2. OTA Update Module (Rust with FreeRTOS bindings)

```rust
// ota_update.rs
use freertos_rust::*;
use embedded_hal::blocking::spi::Transfer;
use core::sync::atomic::{AtomicU8, Ordering};

const OTA_CHUNK_SIZE: usize = 4096;

#[derive(Clone, Copy, PartialEq)]
#[repr(u8)]
enum OTAState {
    Idle = 0,
    Downloading = 1,
    Verifying = 2,
    Installing = 3,
    Rebooting = 4,
    Error = 5,
}

struct OTAContext {
    state: AtomicU8,
    total_size: u32,
    downloaded_size: u32,
    progress_percent: u8,
}

impl OTAContext {
    fn new() -> Self {
        Self {
            state: AtomicU8::new(OTAState::Idle as u8),
            total_size: 0,
            downloaded_size: 0,
            progress_percent: 0,
        }
    }

    fn get_state(&self) -> OTAState {
        match self.state.load(Ordering::Relaxed) {
            0 => OTAState::Idle,
            1 => OTAState::Downloading,
            2 => OTAState::Verifying,
            3 => OTAState::Installing,
            4 => OTAState::Rebooting,
            _ => OTAState::Error,
        }
    }

    fn set_state(&self, state: OTAState) {
        self.state.store(state as u8, Ordering::Relaxed);
    }
}

struct OTAManager {
    context: OTAContext,
    flash_driver: FlashDriver,
    network: NetworkInterface,
}

impl OTAManager {
    fn new(flash: FlashDriver, network: NetworkInterface) -> Self {
        Self {
            context: OTAContext::new(),
            flash_driver: flash,
            network,
        }
    }

    /// Download firmware chunk from server
    fn download_chunk(&mut self, buffer: &mut [u8], offset: u32, size: u32) 
        -> Result<(), OTAError> {
        // HTTP(S) download implementation
        self.network.download_chunk(
            "https://update.server.com/firmware.bin",
            buffer,
            offset,
            size
        )
    }

    /// Install firmware update to Bank B
    fn install_update(&mut self) -> Result<(), OTAError> {
        let mut chunk_buffer = [0u8; OTA_CHUNK_SIZE];
        let mut offset: u32 = 0;
        let mut flash_addr = APP_BANK_B_START;

        // Erase Bank B
        self.flash_driver.erase_bank(APP_BANK_B_START, APP_BANK_B_SIZE)?;

        while offset < self.context.total_size {
            let chunk_size = core::cmp::min(
                OTA_CHUNK_SIZE as u32,
                self.context.total_size - offset
            );

            // Download chunk
            self.download_chunk(
                &mut chunk_buffer[..chunk_size as usize],
                offset,
                chunk_size
            )?;

            // Write to flash
            self.flash_driver.write(
                flash_addr,
                &chunk_buffer[..chunk_size as usize]
            )?;

            offset += chunk_size;
            flash_addr += chunk_size;
            self.context.downloaded_size = offset;
            self.context.progress_percent = 
                ((offset * 100) / self.context.total_size) as u8;

            // Yield to other tasks
            CurrentTask::delay(Duration::ms(10));
        }

        Ok(())
    }

    /// Verify and finalize update
    fn finalize_update(&mut self) -> Result<(), OTAError> {
        // Validate new firmware
        let bootloader = Bootloader::new();
        let metadata = bootloader.validate_firmware(APP_BANK_B_START)
            .ok_or(OTAError::ValidationFailed)?;

        // Mark as ready for installation
        let mut new_metadata = metadata;
        new_metadata.flags |= FirmwareFlags::UpdateReady as u32;
        new_metadata.flags |= FirmwareFlags::Valid as u32;

        // Write updated metadata
        let metadata_addr = APP_BANK_B_START + APP_BANK_B_SIZE - 
                           core::mem::size_of::<FirmwareMetadata>() as u32;
        
        self.flash_driver.write(
            metadata_addr,
            unsafe {
                core::slice::from_raw_parts(
                    &new_metadata as *const _ as *const u8,
                    core::mem::size_of::<FirmwareMetadata>()
                )
            }
        )?;

        Ok(())
    }

    /// Start OTA update process
    fn start_update(&mut self, firmware_size: u32) -> Result<(), OTAError> {
        if self.context.get_state() != OTAState::Idle {
            return Err(OTAError::AlreadyInProgress);
        }

        self.context.total_size = firmware_size;
        self.context.downloaded_size = 0;
        self.context.progress_percent = 0;
        self.context.set_state(OTAState::Downloading);

        Ok(())
    }
}

/// OTA update task
fn ota_task(manager: &mut OTAManager) {
    loop {
        match manager.context.get_state() {
            OTAState::Idle => {
                CurrentTask::delay(Duration::ms(1000));
            }

            OTAState::Downloading => {
                match manager.install_update() {
                    Ok(_) => manager.context.set_state(OTAState::Verifying),
                    Err(_) => manager.context.set_state(OTAState::Error),
                }
            }

            OTAState::Verifying => {
                match manager.finalize_update() {
                    Ok(_) => manager.context.set_state(OTAState::Rebooting),
                    Err(_) => manager.context.set_state(OTAState::Error),
                }
            }

            OTAState::Rebooting => {
                // Gracefully shutdown
                Task::suspend_all();
                
                // System reset
                cortex_m::peripheral::SCB::sys_reset();
            }

            OTAState::Error => {
                // Handle error
                manager.context.set_state(OTAState::Idle);
            }
        }
    }
}

#[derive(Debug)]
enum OTAError {
    AlreadyInProgress,
    ValidationFailed,
    FlashError,
    NetworkError,
}

// Placeholder types (would be implemented by HAL drivers)
struct FlashDriver;
struct NetworkInterface;

impl FlashDriver {
    fn erase_bank(&mut self, _addr: u32, _size: u32) -> Result<(), OTAError> {
        Ok(())
    }
    
    fn write(&mut self, _addr: u32, _data: &[u8]) -> Result<(), OTAError> {
        Ok(())
    }
}

impl NetworkInterface {
    fn download_chunk(&mut self, _url: &str, _buffer: &mut [u8], 
                     _offset: u32, _size: u32) -> Result<(), OTAError> {
        Ok(())
    }
}
```

---

## Best Practices

### 1. **Security Considerations**

```c
// Secure boot with RSA signature verification
bool SecureBoot_VerifySignature(uint8_t *firmware, uint32_t size, 
                                uint8_t *signature, uint32_t sig_len) {
    // Use hardware crypto accelerator if available
    mbedtls_rsa_context rsa;
    mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);
    
    // Load public key
    mbedtls_rsa_import_raw(&rsa, 
        public_modulus, RSA_KEY_SIZE,
        NULL, 0, NULL, 0, NULL, 0,
        public_exponent, 3);
    
    // Calculate SHA-256 hash
    uint8_t hash[32];
    mbedtls_sha256(firmware, size, hash, 0);
    
    // Verify signature
    int ret = mbedtls_rsa_pkcs1_verify(&rsa, NULL, NULL,
                                      MBEDTLS_RSA_PUBLIC,
                                      MBEDTLS_MD_SHA256,
                                      32, hash, signature);
    
    mbedtls_rsa_free(&rsa);
    return (ret == 0);
}
```

### 2. **Power-Fail Safety**

```c
// Atomic flash operations with wear leveling
typedef struct {
    uint32_t sequence_number;
    uint32_t checksum;
    uint8_t  data[SECTOR_SIZE - 8];
} FlashSector_t;

bool Flash_WriteAtomic(uint32_t addr, uint8_t *data, uint32_t size) {
    static uint32_t sequence = 0;
    FlashSector_t sector;
    
    // Prepare sector with sequence number
    sector.sequence_number = ++sequence;
    memcpy(sector.data, data, size);
    sector.checksum = CRC32_Calculate((uint8_t*)&sector, sizeof(sector) - 4);
    
    // Write to flash
    Flash_Erase(addr, SECTOR_SIZE);
    Flash_Write(addr, (uint8_t*)&sector, sizeof(sector));
    
    // Verify
    FlashSector_t verify;
    memcpy(&verify, (void*)addr, sizeof(verify));
    
    return (verify.sequence_number == sector.sequence_number &&
            verify.checksum == sector.checksum);
}
```

### 3. **Rollback Protection**

```c
// Anti-rollback using version monotonic counters
bool Bootloader_CheckVersion(uint32_t new_version) {
    uint32_t min_version = NVS_ReadMinimumVersion();
    
    if (new_version < min_version) {
        // Prevent downgrade attacks
        return false;
    }
    
    // Update minimum version after successful boot
    NVS_WriteMinimumVersion(new_version);
    return true;
}
```

---

## Summary

**Bootloader Integration in FreeRTOS** is essential for robust embedded systems requiring field updates and recovery capabilities. Key aspects include:

### Core Components:
1. **Dual-Bank Architecture**: Enables safe, atomic firmware updates with fallback capability
2. **Firmware Validation**: CRC32 checksums and digital signatures ensure integrity
3. **OTA Updates**: Network-based firmware delivery integrated with FreeRTOS tasks
4. **Recovery Mechanisms**: Watchdog timers and rollback logic prevent bricking

### Critical Features:
- **Boot Selection Logic**: Intelligently chooses between firmware banks based on validity and flags
- **Atomic Operations**: Power-fail-safe flash writes prevent corruption
- **Security**: RSA signature verification, secure boot, and anti-rollback protection
- **Graceful Degradation**: Multiple boot attempts with automatic rollback on failure

### Implementation Strategies:
- **C/C++**: Direct hardware access, optimal for resource-constrained systems
- **Rust**: Memory safety guarantees, preventing common bootloader vulnerabilities
- **FreeRTOS Integration**: Non-blocking OTA updates running as background tasks

### Best Practices:
✓ Always validate firmware before execution  
✓ Implement watchdog-based recovery  
✓ Use cryptographic signatures for secure boot  
✓ Design for power-loss during updates  
✓ Maintain boot attempt counters  
✓ Provide fallback/recovery modes  
✓ Log boot events for diagnostics  

Proper bootloader design is the foundation of reliable, maintainable embedded systems, enabling long-term field support and security updates without physical access to devices.