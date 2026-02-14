# Flash Memory Management in FreeRTOS

## Overview

Flash memory management in FreeRTOS-based systems involves handling non-volatile storage while addressing unique challenges such as limited write cycles, block-based operations, and the need for wear leveling. This topic is critical for embedded systems that require persistent data storage, firmware updates, logging, or configuration management.

## Flash Memory Types

### NOR Flash
- **Characteristics**: Random access, byte-addressable reads, slower writes
- **Use Cases**: Code execution, bootloaders, small data storage
- **Typical Sizes**: 1MB - 256MB
- **Write Speed**: ~100-150 KB/s

### NAND Flash
- **Characteristics**: Page-based access, faster writes, higher density
- **Use Cases**: Large data storage, file systems, logging
- **Typical Sizes**: 128MB - several GB
- **Write Speed**: ~5-20 MB/s

## Flash Memory Constraints

1. **Limited Write/Erase Cycles**: 10,000 - 100,000 cycles for NOR; 100,000 - 1,000,000 for NAND
2. **Block Erase Requirement**: Must erase entire blocks before writing
3. **Bit Flipping**: Can only change bits from 1→0 without erase
4. **Write Amplification**: Small updates may require large block operations
5. **Bad Blocks**: Especially in NAND flash

---

## Code Examples

### C/C++ Implementation

#### 1. Basic Flash Driver Interface

```c
// flash_driver.h
#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "semphr.h"

// Flash configuration
#define FLASH_SECTOR_SIZE       4096
#define FLASH_PAGE_SIZE         256
#define FLASH_TOTAL_SIZE        (16 * 1024 * 1024)  // 16MB
#define FLASH_BASE_ADDRESS      0x08000000

// Flash commands
#define FLASH_CMD_READ          0x03
#define FLASH_CMD_WRITE         0x02
#define FLASH_CMD_ERASE_SECTOR  0x20
#define FLASH_CMD_ERASE_BLOCK   0xD8
#define FLASH_CMD_WRITE_ENABLE  0x06

typedef enum {
    FLASH_OK = 0,
    FLASH_ERROR,
    FLASH_BUSY,
    FLASH_TIMEOUT,
    FLASH_BAD_BLOCK
} FlashStatus_t;

typedef struct {
    uint32_t base_address;
    uint32_t total_size;
    uint32_t sector_size;
    uint32_t page_size;
    SemaphoreHandle_t mutex;
    bool initialized;
} FlashDriver_t;

// Function prototypes
FlashStatus_t Flash_Init(FlashDriver_t *driver);
FlashStatus_t Flash_Read(FlashDriver_t *driver, uint32_t address, 
                         uint8_t *buffer, size_t length);
FlashStatus_t Flash_Write(FlashDriver_t *driver, uint32_t address, 
                          const uint8_t *data, size_t length);
FlashStatus_t Flash_EraseSector(FlashDriver_t *driver, uint32_t sector_addr);
FlashStatus_t Flash_EraseBlock(FlashDriver_t *driver, uint32_t block_addr);

#endif // FLASH_DRIVER_H
```

```c
// flash_driver.c
#include "flash_driver.h"
#include "task.h"

static FlashDriver_t flash_driver;

FlashStatus_t Flash_Init(FlashDriver_t *driver) {
    if (driver->initialized) {
        return FLASH_OK;
    }
    
    // Create mutex for thread-safe access
    driver->mutex = xSemaphoreCreateMutex();
    if (driver->mutex == NULL) {
        return FLASH_ERROR;
    }
    
    driver->base_address = FLASH_BASE_ADDRESS;
    driver->total_size = FLASH_TOTAL_SIZE;
    driver->sector_size = FLASH_SECTOR_SIZE;
    driver->page_size = FLASH_PAGE_SIZE;
    driver->initialized = true;
    
    return FLASH_OK;
}

FlashStatus_t Flash_Read(FlashDriver_t *driver, uint32_t address, 
                         uint8_t *buffer, size_t length) {
    if (!driver->initialized || buffer == NULL) {
        return FLASH_ERROR;
    }
    
    if (xSemaphoreTake(driver->mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return FLASH_TIMEOUT;
    }
    
    // Validate address range
    if (address + length > driver->total_size) {
        xSemaphoreGive(driver->mutex);
        return FLASH_ERROR;
    }
    
    // Hardware-specific read operation
    // This is a placeholder - actual implementation depends on hardware
    uint32_t physical_addr = driver->base_address + address;
    memcpy(buffer, (void*)physical_addr, length);
    
    xSemaphoreGive(driver->mutex);
    return FLASH_OK;
}

FlashStatus_t Flash_Write(FlashDriver_t *driver, uint32_t address, 
                          const uint8_t *data, size_t length) {
    if (!driver->initialized || data == NULL) {
        return FLASH_ERROR;
    }
    
    if (xSemaphoreTake(driver->mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return FLASH_TIMEOUT;
    }
    
    // Validate alignment and range
    if ((address % driver->page_size) != 0 || 
        address + length > driver->total_size) {
        xSemaphoreGive(driver->mutex);
        return FLASH_ERROR;
    }
    
    // Write in page-sized chunks
    size_t bytes_written = 0;
    while (bytes_written < length) {
        size_t chunk_size = driver->page_size;
        if (bytes_written + chunk_size > length) {
            chunk_size = length - bytes_written;
        }
        
        // Enable write operation (hardware-specific)
        // Send WRITE_ENABLE command
        
        // Program page (hardware-specific)
        uint32_t page_addr = address + bytes_written;
        // HAL_Flash_Program(page_addr, &data[bytes_written], chunk_size);
        
        // Wait for completion
        vTaskDelay(pdMS_TO_TICKS(5));
        
        bytes_written += chunk_size;
    }
    
    xSemaphoreGive(driver->mutex);
    return FLASH_OK;
}

FlashStatus_t Flash_EraseSector(FlashDriver_t *driver, uint32_t sector_addr) {
    if (!driver->initialized) {
        return FLASH_ERROR;
    }
    
    if (xSemaphoreTake(driver->mutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
        return FLASH_TIMEOUT;
    }
    
    // Validate sector alignment
    if ((sector_addr % driver->sector_size) != 0) {
        xSemaphoreGive(driver->mutex);
        return FLASH_ERROR;
    }
    
    // Enable write operation
    // Send WRITE_ENABLE command
    
    // Erase sector (hardware-specific)
    // HAL_Flash_EraseSector(sector_addr);
    
    // Wait for erase completion (can take 100ms - 3s)
    vTaskDelay(pdMS_TO_TICKS(500));
    
    xSemaphoreGive(driver->mutex);
    return FLASH_OK;
}
```

#### 2. Wear Leveling Implementation

```c
// wear_leveling.h
#ifndef WEAR_LEVELING_H
#define WEAR_LEVELING_H

#include "flash_driver.h"

#define WL_SECTOR_COUNT     256
#define WL_MAX_ERASE_COUNT  100000

typedef struct {
    uint32_t physical_sector;
    uint32_t logical_sector;
    uint32_t erase_count;
    bool is_bad;
} WLSectorInfo_t;

typedef struct {
    FlashDriver_t *flash;
    WLSectorInfo_t sectors[WL_SECTOR_COUNT];
    uint32_t sector_count;
    SemaphoreHandle_t mutex;
    bool initialized;
} WearLevelingDriver_t;

FlashStatus_t WL_Init(WearLevelingDriver_t *wl, FlashDriver_t *flash);
FlashStatus_t WL_Read(WearLevelingDriver_t *wl, uint32_t logical_addr, 
                      uint8_t *buffer, size_t length);
FlashStatus_t WL_Write(WearLevelingDriver_t *wl, uint32_t logical_addr, 
                       const uint8_t *data, size_t length);
FlashStatus_t WL_Erase(WearLevelingDriver_t *wl, uint32_t logical_sector);
void WL_GetStatistics(WearLevelingDriver_t *wl, uint32_t *min_erase, 
                      uint32_t *max_erase, uint32_t *avg_erase);

#endif // WEAR_LEVELING_H
```

```c
// wear_leveling.c
#include "wear_leveling.h"
#include <string.h>

FlashStatus_t WL_Init(WearLevelingDriver_t *wl, FlashDriver_t *flash) {
    if (wl == NULL || flash == NULL) {
        return FLASH_ERROR;
    }
    
    wl->flash = flash;
    wl->sector_count = WL_SECTOR_COUNT;
    wl->mutex = xSemaphoreCreateMutex();
    
    if (wl->mutex == NULL) {
        return FLASH_ERROR;
    }
    
    // Initialize sector mapping (identity mapping initially)
    for (uint32_t i = 0; i < wl->sector_count; i++) {
        wl->sectors[i].physical_sector = i;
        wl->sectors[i].logical_sector = i;
        wl->sectors[i].erase_count = 0;
        wl->sectors[i].is_bad = false;
    }
    
    // Load wear leveling metadata from flash
    // (In production, this would read from a reserved area)
    
    wl->initialized = true;
    return FLASH_OK;
}

static uint32_t WL_LogicalToPhysical(WearLevelingDriver_t *wl, 
                                     uint32_t logical_sector) {
    for (uint32_t i = 0; i < wl->sector_count; i++) {
        if (wl->sectors[i].logical_sector == logical_sector && 
            !wl->sectors[i].is_bad) {
            return wl->sectors[i].physical_sector;
        }
    }
    return 0xFFFFFFFF; // Invalid
}

static FlashStatus_t WL_PerformWearLeveling(WearLevelingDriver_t *wl) {
    // Find sector with highest erase count
    uint32_t max_erase = 0;
    uint32_t max_idx = 0;
    
    // Find sector with lowest erase count
    uint32_t min_erase = WL_MAX_ERASE_COUNT;
    uint32_t min_idx = 0;
    
    for (uint32_t i = 0; i < wl->sector_count; i++) {
        if (wl->sectors[i].is_bad) continue;
        
        if (wl->sectors[i].erase_count > max_erase) {
            max_erase = wl->sectors[i].erase_count;
            max_idx = i;
        }
        if (wl->sectors[i].erase_count < min_erase) {
            min_erase = wl->sectors[i].erase_count;
            min_idx = i;
        }
    }
    
    // If difference is significant, swap sectors
    if (max_erase - min_erase > 100) {
        uint8_t temp_buffer[FLASH_SECTOR_SIZE];
        
        // Read data from highly-used sector
        uint32_t max_phys = wl->sectors[max_idx].physical_sector;
        Flash_Read(wl->flash, max_phys * FLASH_SECTOR_SIZE, 
                   temp_buffer, FLASH_SECTOR_SIZE);
        
        // Read data from lightly-used sector
        uint32_t min_phys = wl->sectors[min_idx].physical_sector;
        uint8_t temp_buffer2[FLASH_SECTOR_SIZE];
        Flash_Read(wl->flash, min_phys * FLASH_SECTOR_SIZE, 
                   temp_buffer2, FLASH_SECTOR_SIZE);
        
        // Swap physical sectors
        Flash_EraseSector(wl->flash, max_phys * FLASH_SECTOR_SIZE);
        Flash_Write(wl->flash, max_phys * FLASH_SECTOR_SIZE, 
                    temp_buffer2, FLASH_SECTOR_SIZE);
        
        Flash_EraseSector(wl->flash, min_phys * FLASH_SECTOR_SIZE);
        Flash_Write(wl->flash, min_phys * FLASH_SECTOR_SIZE, 
                    temp_buffer, FLASH_SECTOR_SIZE);
        
        // Update mapping
        uint32_t temp_logical = wl->sectors[max_idx].logical_sector;
        wl->sectors[max_idx].logical_sector = wl->sectors[min_idx].logical_sector;
        wl->sectors[min_idx].logical_sector = temp_logical;
    }
    
    return FLASH_OK;
}

FlashStatus_t WL_Write(WearLevelingDriver_t *wl, uint32_t logical_addr, 
                       const uint8_t *data, size_t length) {
    if (!wl->initialized) {
        return FLASH_ERROR;
    }
    
    if (xSemaphoreTake(wl->mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return FLASH_TIMEOUT;
    }
    
    uint32_t logical_sector = logical_addr / FLASH_SECTOR_SIZE;
    uint32_t physical_sector = WL_LogicalToPhysical(wl, logical_sector);
    
    if (physical_sector == 0xFFFFFFFF) {
        xSemaphoreGive(wl->mutex);
        return FLASH_ERROR;
    }
    
    uint32_t physical_addr = physical_sector * FLASH_SECTOR_SIZE + 
                             (logical_addr % FLASH_SECTOR_SIZE);
    
    FlashStatus_t status = Flash_Write(wl->flash, physical_addr, data, length);
    
    xSemaphoreGive(wl->mutex);
    return status;
}

FlashStatus_t WL_Erase(WearLevelingDriver_t *wl, uint32_t logical_sector) {
    if (!wl->initialized) {
        return FLASH_ERROR;
    }
    
    if (xSemaphoreTake(wl->mutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
        return FLASH_TIMEOUT;
    }
    
    uint32_t physical_sector = WL_LogicalToPhysical(wl, logical_sector);
    
    if (physical_sector == 0xFFFFFFFF) {
        xSemaphoreGive(wl->mutex);
        return FLASH_ERROR;
    }
    
    // Increment erase count
    for (uint32_t i = 0; i < wl->sector_count; i++) {
        if (wl->sectors[i].physical_sector == physical_sector) {
            wl->sectors[i].erase_count++;
            
            // Check if sector has reached end of life
            if (wl->sectors[i].erase_count >= WL_MAX_ERASE_COUNT) {
                wl->sectors[i].is_bad = true;
                // Remap to spare sector
            }
            break;
        }
    }
    
    FlashStatus_t status = Flash_EraseSector(wl->flash, 
                                             physical_sector * FLASH_SECTOR_SIZE);
    
    // Perform wear leveling if needed
    WL_PerformWearLeveling(wl);
    
    xSemaphoreGive(wl->mutex);
    return status;
}

void WL_GetStatistics(WearLevelingDriver_t *wl, uint32_t *min_erase, 
                      uint32_t *max_erase, uint32_t *avg_erase) {
    uint32_t min = WL_MAX_ERASE_COUNT;
    uint32_t max = 0;
    uint32_t total = 0;
    uint32_t count = 0;
    
    for (uint32_t i = 0; i < wl->sector_count; i++) {
        if (wl->sectors[i].is_bad) continue;
        
        if (wl->sectors[i].erase_count < min) {
            min = wl->sectors[i].erase_count;
        }
        if (wl->sectors[i].erase_count > max) {
            max = wl->sectors[i].erase_count;
        }
        total += wl->sectors[i].erase_count;
        count++;
    }
    
    *min_erase = min;
    *max_erase = max;
    *avg_erase = count > 0 ? total / count : 0;
}
```

#### 3. Flash File System Integration (LittleFS)

```c
// flash_filesystem.c
#include "lfs.h"
#include "flash_driver.h"

static FlashDriver_t flash_drv;

// LittleFS read callback
int lfs_flash_read(const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, void *buffer, lfs_size_t size) {
    uint32_t addr = block * c->block_size + off;
    FlashStatus_t status = Flash_Read(&flash_drv, addr, buffer, size);
    return (status == FLASH_OK) ? 0 : -1;
}

// LittleFS program callback
int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, const void *buffer, lfs_size_t size) {
    uint32_t addr = block * c->block_size + off;
    FlashStatus_t status = Flash_Write(&flash_drv, addr, buffer, size);
    return (status == FLASH_OK) ? 0 : -1;
}

// LittleFS erase callback
int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block) {
    uint32_t addr = block * c->block_size;
    FlashStatus_t status = Flash_EraseSector(&flash_drv, addr);
    return (status == FLASH_OK) ? 0 : -1;
}

// LittleFS sync callback
int lfs_flash_sync(const struct lfs_config *c) {
    return 0; // No buffering, always synced
}

// Configuration
const struct lfs_config lfs_cfg = {
    .read  = lfs_flash_read,
    .prog  = lfs_flash_prog,
    .erase = lfs_flash_erase,
    .sync  = lfs_flash_sync,
    
    .read_size = 256,
    .prog_size = 256,
    .block_size = 4096,
    .block_count = 1024,
    .cache_size = 256,
    .lookahead_size = 16,
    .block_cycles = 500,
};

// Usage example task
void vFlashFileSystemTask(void *pvParameters) {
    lfs_t lfs;
    lfs_file_t file;
    
    // Mount filesystem
    int err = lfs_mount(&lfs, &lfs_cfg);
    if (err) {
        // Format if mount fails
        lfs_format(&lfs, &lfs_cfg);
        lfs_mount(&lfs, &lfs_cfg);
    }
    
    // Write configuration
    err = lfs_file_open(&lfs, &file, "config.dat", 
                        LFS_O_RDWR | LFS_O_CREAT);
    if (err == 0) {
        uint8_t config_data[128];
        // Fill config_data
        lfs_file_write(&lfs, &file, config_data, sizeof(config_data));
        lfs_file_close(&lfs, &file);
    }
    
    // Read configuration
    err = lfs_file_open(&lfs, &file, "config.dat", LFS_O_RDONLY);
    if (err == 0) {
        uint8_t config_data[128];
        lfs_file_read(&lfs, &file, config_data, sizeof(config_data));
        lfs_file_close(&lfs, &file);
    }
    
    lfs_unmount(&lfs);
    vTaskDelete(NULL);
}
```

---

### Rust Implementation

#### 1. Flash Driver with Embedded HAL

```rust
// flash_driver.rs
use core::fmt;
use embedded_hal::spi::{SpiBus, SpiDevice};
use embedded_hal::digital::OutputPin;

#[derive(Debug, Clone, Copy)]
pub enum FlashError {
    SpiError,
    Timeout,
    InvalidAddress,
    WriteFailed,
    EraseFailed,
    NotInitialized,
}

impl fmt::Display for FlashError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            FlashError::SpiError => write!(f, "SPI communication error"),
            FlashError::Timeout => write!(f, "Operation timeout"),
            FlashError::InvalidAddress => write!(f, "Invalid address"),
            FlashError::WriteFailed => write!(f, "Write operation failed"),
            FlashError::EraseFailed => write!(f, "Erase operation failed"),
            FlashError::NotInitialized => write!(f, "Flash not initialized"),
        }
    }
}

const FLASH_CMD_WRITE_ENABLE: u8 = 0x06;
const FLASH_CMD_WRITE_DISABLE: u8 = 0x04;
const FLASH_CMD_READ_STATUS: u8 = 0x05;
const FLASH_CMD_READ_DATA: u8 = 0x03;
const FLASH_CMD_PAGE_PROGRAM: u8 = 0x02;
const FLASH_CMD_SECTOR_ERASE: u8 = 0x20;
const FLASH_CMD_BLOCK_ERASE: u8 = 0xD8;

const STATUS_BUSY: u8 = 0x01;
const PAGE_SIZE: usize = 256;
const SECTOR_SIZE: usize = 4096;
const TOTAL_SIZE: usize = 16 * 1024 * 1024; // 16MB

pub struct FlashDriver<SPI, CS> {
    spi: SPI,
    cs: CS,
    initialized: bool,
}

impl<SPI, CS> FlashDriver<SPI, CS>
where
    SPI: SpiDevice,
    CS: OutputPin,
{
    pub fn new(spi: SPI, cs: CS) -> Self {
        Self {
            spi,
            cs,
            initialized: false,
        }
    }
    
    pub fn init(&mut self) -> Result<(), FlashError> {
        self.initialized = true;
        Ok(())
    }
    
    fn wait_ready(&mut self) -> Result<(), FlashError> {
        const MAX_RETRIES: u32 = 10000;
        
        for _ in 0..MAX_RETRIES {
            let status = self.read_status()?;
            if (status & STATUS_BUSY) == 0 {
                return Ok(());
            }
        }
        Err(FlashError::Timeout)
    }
    
    fn read_status(&mut self) -> Result<u8, FlashError> {
        self.cs.set_low().map_err(|_| FlashError::SpiError)?;
        
        let cmd = [FLASH_CMD_READ_STATUS, 0x00];
        let mut buf = [0u8; 2];
        
        self.spi.transfer(&mut buf, &cmd)
            .map_err(|_| FlashError::SpiError)?;
        
        self.cs.set_high().map_err(|_| FlashError::SpiError)?;
        
        Ok(buf[1])
    }
    
    fn write_enable(&mut self) -> Result<(), FlashError> {
        self.cs.set_low().map_err(|_| FlashError::SpiError)?;
        
        self.spi.write(&[FLASH_CMD_WRITE_ENABLE])
            .map_err(|_| FlashError::SpiError)?;
        
        self.cs.set_high().map_err(|_| FlashError::SpiError)?;
        Ok(())
    }
    
    pub fn read(&mut self, address: u32, buffer: &mut [u8]) 
        -> Result<(), FlashError> {
        if !self.initialized {
            return Err(FlashError::NotInitialized);
        }
        
        if address as usize + buffer.len() > TOTAL_SIZE {
            return Err(FlashError::InvalidAddress);
        }
        
        self.wait_ready()?;
        
        self.cs.set_low().map_err(|_| FlashError::SpiError)?;
        
        let cmd = [
            FLASH_CMD_READ_DATA,
            (address >> 16) as u8,
            (address >> 8) as u8,
            address as u8,
        ];
        
        self.spi.write(&cmd).map_err(|_| FlashError::SpiError)?;
        self.spi.read(buffer).map_err(|_| FlashError::SpiError)?;
        
        self.cs.set_high().map_err(|_| FlashError::SpiError)?;
        
        Ok(())
    }
    
    pub fn write_page(&mut self, address: u32, data: &[u8]) 
        -> Result<(), FlashError> {
        if !self.initialized {
            return Err(FlashError::NotInitialized);
        }
        
        if data.len() > PAGE_SIZE {
            return Err(FlashError::InvalidAddress);
        }
        
        if address as usize + data.len() > TOTAL_SIZE {
            return Err(FlashError::InvalidAddress);
        }
        
        self.wait_ready()?;
        self.write_enable()?;
        
        self.cs.set_low().map_err(|_| FlashError::SpiError)?;
        
        let cmd = [
            FLASH_CMD_PAGE_PROGRAM,
            (address >> 16) as u8,
            (address >> 8) as u8,
            address as u8,
        ];
        
        self.spi.write(&cmd).map_err(|_| FlashError::SpiError)?;
        self.spi.write(data).map_err(|_| FlashError::SpiError)?;
        
        self.cs.set_high().map_err(|_| FlashError::SpiError)?;
        
        self.wait_ready()?;
        Ok(())
    }
    
    pub fn erase_sector(&mut self, address: u32) -> Result<(), FlashError> {
        if !self.initialized {
            return Err(FlashError::NotInitialized);
        }
        
        if address as usize >= TOTAL_SIZE {
            return Err(FlashError::InvalidAddress);
        }
        
        self.wait_ready()?;
        self.write_enable()?;
        
        self.cs.set_low().map_err(|_| FlashError::SpiError)?;
        
        let cmd = [
            FLASH_CMD_SECTOR_ERASE,
            (address >> 16) as u8,
            (address >> 8) as u8,
            address as u8,
        ];
        
        self.spi.write(&cmd).map_err(|_| FlashError::SpiError)?;
        
        self.cs.set_high().map_err(|_| FlashError::SpiError)?;
        
        self.wait_ready()?;
        Ok(())
    }
}
```

#### 2. Wear Leveling in Rust

```rust
// wear_leveling.rs
use alloc::vec::Vec;
use core::ops::Range;

const MAX_ERASE_COUNT: u32 = 100_000;
const SECTOR_COUNT: usize = 256;

#[derive(Debug, Clone, Copy)]
pub struct SectorInfo {
    pub physical_sector: u32,
    pub logical_sector: u32,
    pub erase_count: u32,
    pub is_bad: bool,
}

pub struct WearLevelingDriver<F> {
    flash: F,
    sectors: Vec<SectorInfo>,
    sector_size: usize,
}

impl<F> WearLevelingDriver<F>
where
    F: FlashOperations,
{
    pub fn new(flash: F, sector_size: usize) -> Self {
        let mut sectors = Vec::with_capacity(SECTOR_COUNT);
        
        // Initialize identity mapping
        for i in 0..SECTOR_COUNT {
            sectors.push(SectorInfo {
                physical_sector: i as u32,
                logical_sector: i as u32,
                erase_count: 0,
                is_bad: false,
            });
        }
        
        Self {
            flash,
            sectors,
            sector_size,
        }
    }
    
    fn logical_to_physical(&self, logical_sector: u32) -> Option<u32> {
        self.sectors.iter()
            .find(|s| s.logical_sector == logical_sector && !s.is_bad)
            .map(|s| s.physical_sector)
    }
    
    pub fn read(&mut self, logical_addr: u32, buffer: &mut [u8]) 
        -> Result<(), FlashError> {
        let logical_sector = logical_addr / self.sector_size as u32;
        let offset = logical_addr % self.sector_size as u32;
        
        let physical_sector = self.logical_to_physical(logical_sector)
            .ok_or(FlashError::InvalidAddress)?;
        
        let physical_addr = physical_sector * self.sector_size as u32 + offset;
        self.flash.read(physical_addr, buffer)
    }
    
    pub fn write(&mut self, logical_addr: u32, data: &[u8]) 
        -> Result<(), FlashError> {
        let logical_sector = logical_addr / self.sector_size as u32;
        let offset = logical_addr % self.sector_size as u32;
        
        let physical_sector = self.logical_to_physical(logical_sector)
            .ok_or(FlashError::InvalidAddress)?;
        
        let physical_addr = physical_sector * self.sector_size as u32 + offset;
        self.flash.write(physical_addr, data)
    }
    
    pub fn erase_sector(&mut self, logical_sector: u32) 
        -> Result<(), FlashError> {
        let physical_sector = self.logical_to_physical(logical_sector)
            .ok_or(FlashError::InvalidAddress)?;
        
        // Increment erase count
        if let Some(sector) = self.sectors.iter_mut()
            .find(|s| s.physical_sector == physical_sector) {
            sector.erase_count += 1;
            
            // Mark as bad if exceeds limit
            if sector.erase_count >= MAX_ERASE_COUNT {
                sector.is_bad = true;
                // TODO: Remap to spare sector
            }
        }
        
        let physical_addr = physical_sector * self.sector_size as u32;
        self.flash.erase_sector(physical_addr)?;
        
        // Perform wear leveling
        self.perform_wear_leveling()?;
        
        Ok(())
    }
    
    fn perform_wear_leveling(&mut self) -> Result<(), FlashError> {
        // Find sectors with max and min erase counts
        let (max_idx, max_count) = self.sectors.iter()
            .enumerate()
            .filter(|(_, s)| !s.is_bad)
            .max_by_key(|(_, s)| s.erase_count)
            .map(|(i, s)| (i, s.erase_count))
            .unwrap_or((0, 0));
        
        let (min_idx, min_count) = self.sectors.iter()
            .enumerate()
            .filter(|(_, s)| !s.is_bad)
            .min_by_key(|(_, s)| s.erase_count)
            .map(|(i, s)| (i, s.erase_count))
            .unwrap_or((0, 0));
        
        // If difference is significant, swap sectors
        if max_count - min_count > 100 {
            let max_phys = self.sectors[max_idx].physical_sector;
            let min_phys = self.sectors[min_idx].physical_sector;
            
            // Swap data between sectors
            let mut temp_buffer = alloc::vec![0u8; self.sector_size];
            
            // Read max sector
            self.flash.read(
                max_phys * self.sector_size as u32,
                &mut temp_buffer
            )?;
            
            let mut temp_buffer2 = alloc::vec![0u8; self.sector_size];
            
            // Read min sector
            self.flash.read(
                min_phys * self.sector_size as u32,
                &mut temp_buffer2
            )?;
            
            // Write swapped data
            self.flash.erase_sector(max_phys * self.sector_size as u32)?;
            self.flash.write(max_phys * self.sector_size as u32, &temp_buffer2)?;
            
            self.flash.erase_sector(min_phys * self.sector_size as u32)?;
            self.flash.write(min_phys * self.sector_size as u32, &temp_buffer)?;
            
            // Update logical mapping
            let temp_logical = self.sectors[max_idx].logical_sector;
            self.sectors[max_idx].logical_sector = 
                self.sectors[min_idx].logical_sector;
            self.sectors[min_idx].logical_sector = temp_logical;
        }
        
        Ok(())
    }
    
    pub fn get_statistics(&self) -> WearStatistics {
        let valid_sectors: Vec<_> = self.sectors.iter()
            .filter(|s| !s.is_bad)
            .collect();
        
        let min = valid_sectors.iter()
            .map(|s| s.erase_count)
            .min()
            .unwrap_or(0);
        
        let max = valid_sectors.iter()
            .map(|s| s.erase_count)
            .max()
            .unwrap_or(0);
        
        let sum: u32 = valid_sectors.iter()
            .map(|s| s.erase_count)
            .sum();
        
        let avg = if valid_sectors.is_empty() {
            0
        } else {
            sum / valid_sectors.len() as u32
        };
        
        WearStatistics { min, max, avg }
    }
}

pub trait FlashOperations {
    fn read(&mut self, address: u32, buffer: &mut [u8]) 
        -> Result<(), FlashError>;
    fn write(&mut self, address: u32, data: &[u8]) 
        -> Result<(), FlashError>;
    fn erase_sector(&mut self, address: u32) -> Result<(), FlashError>;
}

#[derive(Debug)]
pub struct WearStatistics {
    pub min: u32,
    pub max: u32,
    pub avg: u32,
}
```

#### 3. FreeRTOS Integration with Rust

```rust
// freertos_flash_task.rs
use freertos_rust::{Task, Duration};
use core::ptr;

pub struct FlashTask<F> {
    flash: F,
}

impl<F> FlashTask<F>
where
    F: FlashOperations + Send + 'static,
{
    pub fn new(flash: F) -> Self {
        Self { flash }
    }
    
    pub fn spawn(mut self) -> Result<(), freertos_rust::FreeRtosError> {
        Task::new()
            .name("FlashTask")
            .stack_size(2048)
            .priority(freertos_rust::TaskPriority(3))
            .start(move || {
                self.run();
            })
    }
    
    fn run(&mut self) {
        loop {
            // Example: Periodic data logging
            let timestamp = get_timestamp();
            let data = format!("Log entry at {}", timestamp);
            
            if let Err(e) = self.write_log(&data) {
                // Handle error
                log_error("Flash write failed", e);
            }
            
            // Wait before next write
            Task::delay(Duration::ms(1000));
        }
    }
    
    fn write_log(&mut self, data: &str) -> Result<(), FlashError> {
        const LOG_START_ADDR: u32 = 0x10000;
        const LOG_SECTOR_SIZE: u32 = 4096;
        
        static mut LOG_OFFSET: u32 = 0;
        
        unsafe {
            // Check if we need to erase sector
            if LOG_OFFSET >= LOG_SECTOR_SIZE {
                self.flash.erase_sector(LOG_START_ADDR)?;
                LOG_OFFSET = 0;
            }
            
            let addr = LOG_START_ADDR + LOG_OFFSET;
            self.flash.write(addr, data.as_bytes())?;
            
            LOG_OFFSET += data.len() as u32;
        }
        
        Ok(())
    }
}

// Helper functions
fn get_timestamp() -> u32 {
    // Get FreeRTOS tick count or RTC timestamp
    unsafe { freertos_rust::shim::xTaskGetTickCount() }
}

fn log_error(msg: &str, error: FlashError) {
    // Log to UART or other debug interface
    println!("{}: {:?}", msg, error);
}
```

---

## Summary

Flash memory management in FreeRTOS systems requires careful handling of unique constraints:

### Key Concepts:
1. **Flash Types**: NOR (code execution, random access) vs. NAND (data storage, higher density)
2. **Wear Leveling**: Critical for extending flash lifetime by distributing erase cycles evenly
3. **Block Erase**: Flash must be erased in blocks before writing
4. **Thread Safety**: Use mutexes/semaphores for multi-task access

### Implementation Strategies:
- **Driver Layer**: Hardware abstraction with thread-safe operations
- **Wear Leveling**: Logical-to-physical address mapping with erase count tracking
- **File Systems**: Integration with littleFS, FatFS, or SPIFFS for structured storage
- **Bad Block Management**: Detection and remapping of failed sectors

### Best Practices:
- Always erase before write operations
- Implement power-loss protection mechanisms
- Use CRC/ECC for data integrity
- Monitor erase counts and sector health
- Design for graceful degradation when sectors fail
- Cache frequently accessed data in RAM
- Batch writes to minimize erase cycles

### Performance Considerations:
- Erase operations are slow (100ms - 3s)
- Write speeds vary by page size and flash type
- Read operations are fast and don't wear the flash
- Block operations should be scheduled appropriately in RTOS tasks

Flash memory management is essential for reliable embedded systems requiring persistent storage, firmware updates, or data logging capabilities.