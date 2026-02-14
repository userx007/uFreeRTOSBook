# Non-Volatile Storage Patterns in FreeRTOS

## Overview

Non-Volatile Storage (NVS) patterns in FreeRTOS-based systems are critical for maintaining configuration data, user settings, calibration values, and system state across power cycles. Unlike volatile RAM, NVS persists data even when power is removed, making it essential for embedded systems that need to preserve information.

## Key Concepts

### 1. **Configuration Storage**
Configuration storage involves persisting system parameters, user preferences, and calibration data in non-volatile memory (Flash, EEPROM, FRAM, or external storage).

### 2. **Power-Loss Scenarios**
Embedded systems must handle unexpected power loss gracefully, ensuring data integrity even during write operations.

### 3. **Data Integrity with CRC/Checksums**
Error detection mechanisms verify that stored data hasn't been corrupted due to power loss, bit flips, or wear.

### 4. **Settings Persistence**
Managing how and when settings are written to NVS to balance data safety with flash wear leveling.

---

## Common NVS Storage Types

| Type | Write Cycles | Speed | Use Case |
|------|-------------|-------|----------|
| **Flash** | 10K-100K | Slow | Firmware, large configs |
| **EEPROM** | 100K-1M | Medium | Frequent updates |
| **FRAM** | Unlimited | Fast | High-frequency logging |
| **External Flash** | 100K | Medium | Large datasets |

---

## Implementation Patterns

### Pattern 1: Simple Configuration Structure with CRC

**C/C++ Implementation:**

```c
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdint.h>

// Configuration structure
typedef struct {
    uint32_t magic_number;      // Validation marker
    uint32_t version;           // Schema version
    
    // Application settings
    uint8_t wifi_ssid[32];
    uint8_t wifi_password[64];
    uint32_t device_id;
    float calibration_offset;
    uint8_t enable_feature_flags;
    
    // Integrity check
    uint32_t crc32;
} SystemConfig_t;

#define CONFIG_MAGIC 0xDEADBEEF
#define CONFIG_VERSION 1
#define FLASH_CONFIG_ADDR 0x08080000  // Example flash address

// CRC-32 calculation (simple implementation)
uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    
    return ~crc;
}

// Save configuration to NVS
BaseType_t save_config(const SystemConfig_t* config) {
    SystemConfig_t temp_config;
    
    // Create a copy to calculate CRC
    memcpy(&temp_config, config, sizeof(SystemConfig_t));
    temp_config.magic_number = CONFIG_MAGIC;
    temp_config.version = CONFIG_VERSION;
    
    // Calculate CRC (exclude CRC field itself)
    temp_config.crc32 = calculate_crc32(
        (uint8_t*)&temp_config,
        sizeof(SystemConfig_t) - sizeof(uint32_t)
    );
    
    // Critical section to prevent interruption during write
    taskENTER_CRITICAL();
    
    // Flash write operation (platform-specific)
    // Example: HAL_FLASH_Unlock();
    // HAL_FLASH_Program(...);
    // HAL_FLASH_Lock();
    
    taskEXIT_CRITICAL();
    
    return pdPASS;
}

// Load configuration from NVS
BaseType_t load_config(SystemConfig_t* config) {
    // Read from flash (platform-specific)
    // Example: memcpy(config, (void*)FLASH_CONFIG_ADDR, sizeof(SystemConfig_t));
    
    // Validate magic number
    if (config->magic_number != CONFIG_MAGIC) {
        return pdFAIL;  // Invalid or uninitialized
    }
    
    // Validate version
    if (config->version != CONFIG_VERSION) {
        return pdFAIL;  // Version mismatch
    }
    
    // Verify CRC
    uint32_t calculated_crc = calculate_crc32(
        (uint8_t*)config,
        sizeof(SystemConfig_t) - sizeof(uint32_t)
    );
    
    if (calculated_crc != config->crc32) {
        return pdFAIL;  // Corruption detected
    }
    
    return pdPASS;
}

// Initialize with default configuration
void init_default_config(SystemConfig_t* config) {
    memset(config, 0, sizeof(SystemConfig_t));
    
    strcpy((char*)config->wifi_ssid, "DefaultSSID");
    strcpy((char*)config->wifi_password, "DefaultPassword");
    config->device_id = 12345;
    config->calibration_offset = 0.0f;
    config->enable_feature_flags = 0xFF;
}
```

### Pattern 2: Dual-Bank Storage (Power-Loss Protection)

```c
// Dual-bank approach for power-loss safety
typedef struct {
    uint32_t sequence_number;   // Increments with each write
    SystemConfig_t config;
    uint32_t crc32;
} ConfigBank_t;

#define CONFIG_BANK_A_ADDR 0x08080000
#define CONFIG_BANK_B_ADDR 0x08082000

// Save with dual-bank redundancy
BaseType_t save_config_safe(const SystemConfig_t* config) {
    static uint32_t sequence = 0;
    ConfigBank_t bank_a, bank_b;
    
    // Read both banks
    memcpy(&bank_a, (void*)CONFIG_BANK_A_ADDR, sizeof(ConfigBank_t));
    memcpy(&bank_b, (void*)CONFIG_BANK_B_ADDR, sizeof(ConfigBank_t));
    
    // Determine which bank to write (alternate)
    ConfigBank_t* target_bank;
    uint32_t target_addr;
    
    if (bank_a.sequence_number >= bank_b.sequence_number) {
        target_bank = &bank_b;
        target_addr = CONFIG_BANK_B_ADDR;
    } else {
        target_bank = &bank_a;
        target_addr = CONFIG_BANK_A_ADDR;
    }
    
    // Prepare new bank
    sequence++;
    target_bank->sequence_number = sequence;
    memcpy(&target_bank->config, config, sizeof(SystemConfig_t));
    
    // Calculate CRC
    target_bank->crc32 = calculate_crc32(
        (uint8_t*)target_bank,
        sizeof(ConfigBank_t) - sizeof(uint32_t)
    );
    
    // Write to flash
    taskENTER_CRITICAL();
    // Flash erase and program operations
    taskEXIT_CRITICAL();
    
    return pdPASS;
}

// Load most recent valid configuration
BaseType_t load_config_safe(SystemConfig_t* config) {
    ConfigBank_t bank_a, bank_b;
    
    memcpy(&bank_a, (void*)CONFIG_BANK_A_ADDR, sizeof(ConfigBank_t));
    memcpy(&bank_b, (void*)CONFIG_BANK_B_ADDR, sizeof(ConfigBank_t));
    
    // Validate both banks
    BaseType_t bank_a_valid = pdFAIL;
    BaseType_t bank_b_valid = pdFAIL;
    
    uint32_t crc_a = calculate_crc32(
        (uint8_t*)&bank_a,
        sizeof(ConfigBank_t) - sizeof(uint32_t)
    );
    if (crc_a == bank_a.crc32) bank_a_valid = pdPASS;
    
    uint32_t crc_b = calculate_crc32(
        (uint8_t*)&bank_b,
        sizeof(ConfigBank_t) - sizeof(uint32_t)
    );
    if (crc_b == bank_b.crc32) bank_b_valid = pdPASS;
    
    // Select most recent valid bank
    if (bank_a_valid == pdPASS && bank_b_valid == pdPASS) {
        if (bank_a.sequence_number > bank_b.sequence_number) {
            memcpy(config, &bank_a.config, sizeof(SystemConfig_t));
        } else {
            memcpy(config, &bank_b.config, sizeof(SystemConfig_t));
        }
        return pdPASS;
    } else if (bank_a_valid == pdPASS) {
        memcpy(config, &bank_a.config, sizeof(SystemConfig_t));
        return pdPASS;
    } else if (bank_b_valid == pdPASS) {
        memcpy(config, &bank_b.config, sizeof(SystemConfig_t));
        return pdPASS;
    }
    
    return pdFAIL;  // No valid configuration found
}
```

### Pattern 3: Deferred Write Pattern (Wear Leveling)

```c
// Configuration manager task
typedef struct {
    SystemConfig_t current_config;
    SystemConfig_t pending_config;
    BaseType_t dirty_flag;
    TickType_t last_save_time;
    SemaphoreHandle_t config_mutex;
} ConfigManager_t;

static ConfigManager_t config_mgr;

#define CONFIG_SAVE_DELAY_MS 5000  // Wait 5 seconds before persisting

void config_manager_task(void* params) {
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1000));
        
        // Check if we have pending changes
        if (xSemaphoreTake(config_mgr.config_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (config_mgr.dirty_flag == pdTRUE) {
                TickType_t current_time = xTaskGetTickCount();
                TickType_t elapsed = current_time - config_mgr.last_save_time;
                
                // Only save if enough time has passed (debouncing)
                if (elapsed >= pdMS_TO_TICKS(CONFIG_SAVE_DELAY_MS)) {
                    save_config_safe(&config_mgr.pending_config);
                    memcpy(&config_mgr.current_config, &config_mgr.pending_config,
                           sizeof(SystemConfig_t));
                    config_mgr.dirty_flag = pdFALSE;
                }
            }
            xSemaphoreGive(config_mgr.config_mutex);
        }
    }
}

// Update configuration (deferred write)
BaseType_t update_config(const SystemConfig_t* new_config) {
    if (xSemaphoreTake(config_mgr.config_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        memcpy(&config_mgr.pending_config, new_config, sizeof(SystemConfig_t));
        config_mgr.dirty_flag = pdTRUE;
        config_mgr.last_save_time = xTaskGetTickCount();
        xSemaphoreGive(config_mgr.config_mutex);
        return pdPASS;
    }
    return pdFAIL;
}

// Force immediate save
BaseType_t flush_config(void) {
    if (xSemaphoreTake(config_mgr.config_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (config_mgr.dirty_flag == pdTRUE) {
            save_config_safe(&config_mgr.pending_config);
            memcpy(&config_mgr.current_config, &config_mgr.pending_config,
                   sizeof(SystemConfig_t));
            config_mgr.dirty_flag = pdFALSE;
        }
        xSemaphoreGive(config_mgr.config_mutex);
        return pdPASS;
    }
    return pdFAIL;
}
```

---

## Rust Implementation

```rust
use crc::{Crc, CRC_32_ISO_HDLC};

const CONFIG_MAGIC: u32 = 0xDEADBEEF;
const CONFIG_VERSION: u32 = 1;
const CRC32: Crc<u32> = Crc::<u32>::new(&CRC_32_ISO_HDLC);

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct SystemConfig {
    magic_number: u32,
    version: u32,
    wifi_ssid: [u8; 32],
    wifi_password: [u8; 64],
    device_id: u32,
    calibration_offset: f32,
    enable_feature_flags: u8,
    crc32: u32,
}

impl SystemConfig {
    pub fn new() -> Self {
        Self {
            magic_number: CONFIG_MAGIC,
            version: CONFIG_VERSION,
            wifi_ssid: [0; 32],
            wifi_password: [0; 64],
            device_id: 0,
            calibration_offset: 0.0,
            enable_feature_flags: 0xFF,
            crc32: 0,
        }
    }

    pub fn calculate_crc(&self) -> u32 {
        let bytes = unsafe {
            core::slice::from_raw_parts(
                self as *const _ as *const u8,
                core::mem::size_of::<Self>() - core::mem::size_of::<u32>(),
            )
        };
        CRC32.checksum(bytes)
    }

    pub fn validate(&self) -> Result<(), ConfigError> {
        if self.magic_number != CONFIG_MAGIC {
            return Err(ConfigError::InvalidMagic);
        }
        
        if self.version != CONFIG_VERSION {
            return Err(ConfigError::VersionMismatch);
        }
        
        let calculated_crc = self.calculate_crc();
        if calculated_crc != self.crc32 {
            return Err(ConfigError::CrcMismatch);
        }
        
        Ok(())
    }

    pub fn save(&mut self) -> Result<(), ConfigError> {
        self.magic_number = CONFIG_MAGIC;
        self.version = CONFIG_VERSION;
        self.crc32 = self.calculate_crc();
        
        // Platform-specific flash write
        // Example: flash::write(FLASH_ADDR, self)?;
        
        Ok(())
    }

    pub fn load() -> Result<Self, ConfigError> {
        // Platform-specific flash read
        // Example: let config: SystemConfig = flash::read(FLASH_ADDR)?;
        
        // For demonstration
        let config = Self::new();
        config.validate()?;
        Ok(config)
    }
}

#[derive(Debug)]
pub enum ConfigError {
    InvalidMagic,
    VersionMismatch,
    CrcMismatch,
    FlashError,
}

// Dual-bank implementation
#[repr(C)]
pub struct ConfigBank {
    sequence_number: u32,
    config: SystemConfig,
    crc32: u32,
}

impl ConfigBank {
    pub fn calculate_crc(&self) -> u32 {
        let bytes = unsafe {
            core::slice::from_raw_parts(
                self as *const _ as *const u8,
                core::mem::size_of::<Self>() - core::mem::size_of::<u32>(),
            )
        };
        CRC32.checksum(bytes)
    }

    pub fn is_valid(&self) -> bool {
        self.calculate_crc() == self.crc32
    }
}

pub struct DualBankStorage {
    bank_a_addr: usize,
    bank_b_addr: usize,
    sequence: u32,
}

impl DualBankStorage {
    pub fn new(bank_a_addr: usize, bank_b_addr: usize) -> Self {
        Self {
            bank_a_addr,
            bank_b_addr,
            sequence: 0,
        }
    }

    pub fn save(&mut self, config: &SystemConfig) -> Result<(), ConfigError> {
        // Read both banks
        let bank_a = self.read_bank(self.bank_a_addr)?;
        let bank_b = self.read_bank(self.bank_b_addr)?;

        // Determine which bank to write
        let (target_addr, new_sequence) = if bank_a.sequence_number >= bank_b.sequence_number {
            (self.bank_b_addr, bank_a.sequence_number + 1)
        } else {
            (self.bank_a_addr, bank_b.sequence_number + 1)
        };

        // Create new bank
        let mut new_bank = ConfigBank {
            sequence_number: new_sequence,
            config: *config,
            crc32: 0,
        };
        new_bank.crc32 = new_bank.calculate_crc();

        // Write to flash (platform-specific)
        self.write_bank(target_addr, &new_bank)?;
        
        Ok(())
    }

    pub fn load(&self) -> Result<SystemConfig, ConfigError> {
        let bank_a = self.read_bank(self.bank_a_addr)?;
        let bank_b = self.read_bank(self.bank_b_addr)?;

        let bank_a_valid = bank_a.is_valid();
        let bank_b_valid = bank_b.is_valid();

        match (bank_a_valid, bank_b_valid) {
            (true, true) => {
                if bank_a.sequence_number > bank_b.sequence_number {
                    Ok(bank_a.config)
                } else {
                    Ok(bank_b.config)
                }
            }
            (true, false) => Ok(bank_a.config),
            (false, true) => Ok(bank_b.config),
            (false, false) => Err(ConfigError::CrcMismatch),
        }
    }

    fn read_bank(&self, addr: usize) -> Result<ConfigBank, ConfigError> {
        // Platform-specific implementation
        unimplemented!("Platform-specific flash read")
    }

    fn write_bank(&self, addr: usize, bank: &ConfigBank) -> Result<(), ConfigError> {
        // Platform-specific implementation
        unimplemented!("Platform-specific flash write")
    }
}

// Configuration manager with deferred writes
use core::sync::atomic::{AtomicBool, Ordering};

pub struct ConfigManager {
    current_config: SystemConfig,
    pending_config: SystemConfig,
    dirty: AtomicBool,
    last_save_time: u32,
}

impl ConfigManager {
    const SAVE_DELAY_MS: u32 = 5000;

    pub fn new() -> Self {
        Self {
            current_config: SystemConfig::new(),
            pending_config: SystemConfig::new(),
            dirty: AtomicBool::new(false),
            last_save_time: 0,
        }
    }

    pub fn update(&mut self, new_config: SystemConfig) {
        self.pending_config = new_config;
        self.dirty.store(true, Ordering::Release);
        self.last_save_time = Self::get_tick_count();
    }

    pub fn periodic_save(&mut self) -> Result<(), ConfigError> {
        if !self.dirty.load(Ordering::Acquire) {
            return Ok(());
        }

        let current_time = Self::get_tick_count();
        let elapsed = current_time - self.last_save_time;

        if elapsed >= Self::SAVE_DELAY_MS {
            self.pending_config.save()?;
            self.current_config = self.pending_config;
            self.dirty.store(false, Ordering::Release);
        }

        Ok(())
    }

    pub fn flush(&mut self) -> Result<(), ConfigError> {
        if self.dirty.load(Ordering::Acquire) {
            self.pending_config.save()?;
            self.current_config = self.pending_config;
            self.dirty.store(false, Ordering::Release);
        }
        Ok(())
    }

    fn get_tick_count() -> u32 {
        // FreeRTOS binding: xTaskGetTickCount()
        0 // Placeholder
    }
}
```

---

## Advanced Pattern: Key-Value Store with Wear Leveling

```c
// Key-Value NVS with wear leveling
#define NVS_SECTOR_SIZE 4096
#define NVS_MAX_ENTRIES 64

typedef struct {
    uint16_t key;
    uint16_t length;
    uint32_t crc32;
    uint8_t data[];  // Flexible array member
} NVS_Entry_t;

typedef struct {
    uint32_t magic;
    uint32_t erase_count;
    uint16_t active_entries;
    uint16_t reserved;
} NVS_SectorHeader_t;

// Write key-value pair
BaseType_t nvs_write_kv(uint16_t key, const void* data, uint16_t length) {
    // Find sector with lowest erase count
    // Append entry to sector
    // If sector full, compact and erase
    // Update erase count for wear leveling
    
    return pdPASS;
}

// Read key-value pair
BaseType_t nvs_read_kv(uint16_t key, void* data, uint16_t max_length) {
    // Search through sectors for most recent entry with key
    // Verify CRC
    // Copy data
    
    return pdPASS;
}
```

---

## Summary

**Non-Volatile Storage Patterns in FreeRTOS** provide robust mechanisms for persisting configuration and state across power cycles:

### Key Takeaways:

1. **Data Integrity**: Always use CRC32 or other checksums to detect corruption
2. **Power-Loss Safety**: Implement dual-bank or journaling strategies to prevent data loss during unexpected shutdowns
3. **Wear Leveling**: Minimize flash writes through deferred writes and wear-leveling algorithms
4. **Versioning**: Include magic numbers and version fields to handle schema migrations
5. **Atomic Operations**: Use critical sections or disable interrupts during write operations

### Best Practices:

- **Debounce writes**: Defer multiple rapid updates to reduce flash wear
- **Validate on boot**: Always check CRC and magic numbers when loading configuration
- **Provide defaults**: Have fallback default configurations for corrupted or missing data
- **Test power-loss**: Simulate power failures during writes to verify recovery mechanisms
- **Monitor wear**: Track erase counts and implement wear leveling for long-term reliability

### Common Pitfalls to Avoid:

- Writing to flash from ISRs (use queues to defer to tasks)
- Ignoring CRC validation failures
- Not handling flash erase failures
- Excessive flash writes causing premature wear
- Insufficient delay between rapid configuration updates

This pattern is essential for production embedded systems running FreeRTOS, ensuring reliability and data persistence in real-world deployment scenarios.