# FreeRTOS+FAT File System

## Introduction

FreeRTOS+FAT is a thread-aware FAT file system implementation designed specifically for embedded systems running FreeRTOS. It provides a complete FAT12/FAT16/FAT32 file system with long filename support (LFN) that integrates seamlessly with FreeRTOS's multi-tasking environment. Unlike traditional embedded file systems, FreeRTOS+FAT is specifically engineered to handle concurrent access from multiple tasks safely through its built-in mutex protection and thread-aware design.

The file system supports standard file operations (open, read, write, close, seek) while maintaining data integrity in multi-threaded environments. It's particularly well-suited for applications requiring persistent storage on SD cards, eMMC, NAND/NOR flash, or other block-based storage media.

## Core Architecture and Features

FreeRTOS+FAT implements a layered architecture consisting of three main components: the high-level API layer (providing POSIX-like file operations), the FAT logic layer (handling file allocation tables, directory structures, and metadata), and the I/O manager layer (interfacing with physical storage devices through a portable abstraction).

The file system provides automatic mutex protection for all file operations, ensuring thread safety without requiring explicit locking in application code. It supports multiple simultaneous file handles, allowing different tasks to work with different files or even the same file concurrently (with appropriate caution). Long filename support enables files up to 255 characters with mixed case, while maintaining compatibility with 8.3 format.

Directory operations include creation, removal, and traversal with support for both absolute and relative paths. The implementation includes wear leveling considerations, caching for improved performance, and configurable buffer sizes to balance memory usage against speed.

## Configuration and Setup

### Basic Configuration (FreeRTOSFATConfig.h)

```c
// Maximum number of concurrently open files
#define ffconfigMAX_FILE_SYS        1
#define ffconfigMAX_PARTITIONS      1

// Long filename support
#define ffconfigLFN_SUPPORT         1
#define ffconfigINCLUDE_SHORT_NAME  1

// Path cache for performance
#define ffconfigPATH_CACHE_DEPTH    5

// Enable/disable features
#define ffconfigTIME_SUPPORT        1
#define ffconfigREMOVABLE_MEDIA     1
#define ffconfigMOUNT_FIND_FREE     1

// Buffer sizes
#define ffconfigSECTOR_BUFFER_COUNT 2
#define ffconfig512_BUFFER_COUNT    2

// Thread safety
#define ffconfigUSE_RECURSIVE_MUTEX 1

// Hash table for faster lookups
#define ffconfigHASH_TABLE_SIZE     128
```

## C/C++ Implementation Examples

### 1. SD Card Interface Implementation

```c
#include "ff_headers.h"
#include "ff_stdio.h"
#include "sd_card_driver.h"

// Disk I/O structure for SD card
typedef struct {
    SD_CardInfo cardInfo;
    uint32_t sectorSize;
    uint32_t sectorCount;
    SemaphoreHandle_t accessMutex;
} SDCardDevice_t;

// Read sectors from SD card
static int32_t sdcard_read(uint8_t *pucBuffer, 
                           uint32_t ulSectorNumber,
                           uint32_t ulSectorCount, 
                           FF_Disk_t *pxDisk)
{
    SDCardDevice_t *pxDevice = (SDCardDevice_t *)pxDisk->pvTag;
    int32_t lResult = 0;
    
    // Take mutex for thread safety
    if (xSemaphoreTake(pxDevice->accessMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        // Read multiple sectors
        if (SD_ReadBlocks(pucBuffer, 
                         ulSectorNumber, 
                         pxDevice->sectorSize,
                         ulSectorCount) == SD_OK)
        {
            lResult = ulSectorCount;
        }
        else
        {
            lResult = FF_ERR_DEVICE_DRIVER_FAILED;
        }
        
        xSemaphoreGive(pxDevice->accessMutex);
    }
    else
    {
        lResult = FF_ERR_DEVICE_DRIVER_FAILED;
    }
    
    return lResult;
}

// Write sectors to SD card
static int32_t sdcard_write(uint8_t *pucBuffer,
                            uint32_t ulSectorNumber,
                            uint32_t ulSectorCount,
                            FF_Disk_t *pxDisk)
{
    SDCardDevice_t *pxDevice = (SDCardDevice_t *)pxDisk->pvTag;
    int32_t lResult = 0;
    
    if (xSemaphoreTake(pxDevice->accessMutex, pdMS_TO_TICKS(1000)) == pdTRUE)
    {
        if (SD_WriteBlocks(pucBuffer,
                          ulSectorNumber,
                          pxDevice->sectorSize,
                          ulSectorCount) == SD_OK)
        {
            lResult = ulSectorCount;
        }
        else
        {
            lResult = FF_ERR_DEVICE_DRIVER_FAILED;
        }
        
        xSemaphoreGive(pxDevice->accessMutex);
    }
    else
    {
        lResult = FF_ERR_DEVICE_DRIVER_FAILED;
    }
    
    return lResult;
}

// Initialize SD card and mount file system
FF_Disk_t* sdcard_init_and_mount(void)
{
    FF_Disk_t *pxDisk = NULL;
    FF_Error_t xError;
    SDCardDevice_t *pxDevice;
    
    // Allocate device structure
    pxDevice = pvPortMalloc(sizeof(SDCardDevice_t));
    if (pxDevice == NULL)
        return NULL;
    
    // Initialize SD card hardware
    if (SD_Init(&pxDevice->cardInfo) != SD_OK)
    {
        vPortFree(pxDevice);
        return NULL;
    }
    
    pxDevice->sectorSize = 512;
    pxDevice->sectorCount = pxDevice->cardInfo.CardCapacity / 512;
    pxDevice->accessMutex = xSemaphoreCreateMutex();
    
    // Create FreeRTOS+FAT disk structure
    pxDisk = FF_SDDiskInit("/sd", 
                           pxDevice->sectorSize,
                           pxDevice->sectorCount,
                           pxDevice);
    
    if (pxDisk != NULL)
    {
        // Register read/write functions
        pxDisk->fnpReadBlocks = sdcard_read;
        pxDisk->fnpWriteBlocks = sdcard_write;
        
        // Mount the partition
        xError = FF_Mount(pxDisk, 0);
        
        if (FF_isERR(xError))
        {
            FF_SDDiskDelete(pxDisk);
            vSemaphoreDelete(pxDevice->accessMutex);
            vPortFree(pxDevice);
            return NULL;
        }
    }
    
    return pxDisk;
}
```

### 2. File Operations in Multi-tasking Environment

```c
// Data logging task - demonstrates safe concurrent file access
void vDataLoggingTask(void *pvParameters)
{
    FF_FILE *pxFile;
    char pcFileName[64];
    char pcLogData[128];
    TickType_t xLastLogTime = xTaskGetTickCount();
    
    while (1)
    {
        // Generate timestamped filename
        snprintf(pcFileName, sizeof(pcFileName), 
                 "/sd/logs/data_%08lu.txt", 
                 (unsigned long)xTaskGetTickCount());
        
        // Open file for appending (creates if doesn't exist)
        pxFile = ff_fopen(pcFileName, "a");
        
        if (pxFile != NULL)
        {
            // Generate log entry
            snprintf(pcLogData, sizeof(pcLogData),
                    "%lu,Temperature:%d,Pressure:%d\r\n",
                    (unsigned long)xTaskGetTickCount(),
                    read_temperature(),
                    read_pressure());
            
            // Write to file - internally protected by FreeRTOS+FAT
            size_t bytesWritten = ff_fwrite(pcLogData, 
                                           1, 
                                           strlen(pcLogData),
                                           pxFile);
            
            // Ensure data is flushed to disk
            ff_fflush(pxFile);
            
            // Close file
            ff_fclose(pxFile);
            
            if (bytesWritten != strlen(pcLogData))
            {
                // Handle write error
                printf("Error writing to log file\n");
            }
        }
        else
        {
            printf("Failed to open file: %s\n", pcFileName);
        }
        
        vTaskDelayUntil(&xLastLogTime, pdMS_TO_TICKS(1000));
    }
}

// Configuration file reader task
void vConfigReaderTask(void *pvParameters)
{
    FF_FILE *pxFile;
    char pcLine[256];
    
    // Open configuration file
    pxFile = ff_fopen("/sd/config/settings.ini", "r");
    
    if (pxFile != NULL)
    {
        // Read file line by line
        while (ff_fgets(pcLine, sizeof(pcLine), pxFile) != NULL)
        {
            // Parse configuration line
            parse_config_line(pcLine);
        }
        
        ff_fclose(pxFile);
    }
    else
    {
        // Create default configuration
        create_default_config();
    }
    
    vTaskDelete(NULL);
}
```

### 3. Directory Operations and File Management

```c
// Directory listing and file search
typedef struct {
    char fileName[256];
    uint32_t fileSize;
    FF_SystemTime_t modifiedTime;
} FileInfo_t;

int list_directory(const char *pcPath, FileInfo_t *pxFileList, int maxFiles)
{
    FF_FindData_t xFindData;
    int fileCount = 0;
    
    // Find first file in directory
    if (ff_findfirst(pcPath, &xFindData) == 0)
    {
        do
        {
            if (fileCount >= maxFiles)
                break;
            
            // Skip "." and ".." entries
            if (strcmp(xFindData.pcFileName, ".") == 0 ||
                strcmp(xFindData.pcFileName, "..") == 0)
                continue;
            
            // Copy file information
            strncpy(pxFileList[fileCount].fileName, 
                   xFindData.pcFileName,
                   sizeof(pxFileList[fileCount].fileName) - 1);
            
            pxFileList[fileCount].fileSize = xFindData.ulFileSize;
            pxFileList[fileCount].modifiedTime = xFindData.xModifiedTime;
            
            fileCount++;
            
        } while (ff_findnext(&xFindData) == 0);
    }
    
    return fileCount;
}

// Safe file copy with progress reporting
FF_Error_t copy_file_with_progress(const char *pcSource, 
                                   const char *pcDest,
                                   void (*progress_callback)(uint32_t))
{
    FF_FILE *pxSource, *pxDest;
    uint8_t pucBuffer[4096];
    size_t bytesRead, bytesWritten;
    uint32_t totalBytes = 0;
    FF_Error_t xError = FF_ERR_NONE;
    
    pxSource = ff_fopen(pcSource, "rb");
    if (pxSource == NULL)
        return FF_ERR_FILE_NOT_FOUND;
    
    pxDest = ff_fopen(pcDest, "wb");
    if (pxDest == NULL)
    {
        ff_fclose(pxSource);
        return FF_ERR_FILE_COULD_NOT_CREATE_FILE;
    }
    
    // Copy in chunks
    while ((bytesRead = ff_fread(pucBuffer, 1, sizeof(pucBuffer), pxSource)) > 0)
    {
        bytesWritten = ff_fwrite(pucBuffer, 1, bytesRead, pxDest);
        
        if (bytesWritten != bytesRead)
        {
            xError = FF_ERR_DEVICE_DRIVER_FAILED;
            break;
        }
        
        totalBytes += bytesWritten;
        
        if (progress_callback)
            progress_callback(totalBytes);
    }
    
    ff_fclose(pxSource);
    ff_fclose(pxDest);
    
    return xError;
}
```

### 4. Advanced: Circular Log Buffer on Flash

```c
// Circular logging system for wear leveling
#define MAX_LOG_FILES 10
#define LOG_FILE_SIZE (100 * 1024)  // 100KB per file

typedef struct {
    uint32_t currentFileIndex;
    uint32_t currentFilePosition;
    SemaphoreHandle_t logMutex;
    char basePath[64];
} CircularLogger_t;

CircularLogger_t* create_circular_logger(const char *pcBasePath)
{
    CircularLogger_t *pxLogger = pvPortMalloc(sizeof(CircularLogger_t));
    
    if (pxLogger != NULL)
    {
        pxLogger->currentFileIndex = 0;
        pxLogger->currentFilePosition = 0;
        pxLogger->logMutex = xSemaphoreCreateMutex();
        strncpy(pxLogger->basePath, pcBasePath, sizeof(pxLogger->basePath) - 1);
        
        // Create log directory if it doesn't exist
        ff_mkdir(pcBasePath);
    }
    
    return pxLogger;
}

FF_Error_t circular_log_write(CircularLogger_t *pxLogger, 
                              const char *pcData,
                              size_t dataLen)
{
    FF_FILE *pxFile;
    char pcFileName[128];
    FF_Error_t xError = FF_ERR_NONE;
    
    if (xSemaphoreTake(pxLogger->logMutex, portMAX_DELAY) == pdTRUE)
    {
        // Check if we need to rotate to next file
        if (pxLogger->currentFilePosition + dataLen > LOG_FILE_SIZE)
        {
            pxLogger->currentFileIndex = 
                (pxLogger->currentFileIndex + 1) % MAX_LOG_FILES;
            pxLogger->currentFilePosition = 0;
        }
        
        // Generate filename
        snprintf(pcFileName, sizeof(pcFileName),
                "%s/log_%02lu.dat",
                pxLogger->basePath,
                (unsigned long)pxLogger->currentFileIndex);
        
        // Open or create file
        pxFile = ff_fopen(pcFileName, 
                         pxLogger->currentFilePosition == 0 ? "wb" : "ab");
        
        if (pxFile != NULL)
        {
            size_t written = ff_fwrite(pcData, 1, dataLen, pxFile);
            
            if (written == dataLen)
            {
                pxLogger->currentFilePosition += written;
                ff_fflush(pxFile);
            }
            else
            {
                xError = FF_ERR_DEVICE_DRIVER_FAILED;
            }
            
            ff_fclose(pxFile);
        }
        else
        {
            xError = FF_ERR_FILE_COULD_NOT_CREATE_FILE;
        }
        
        xSemaphoreGive(pxLogger->logMutex);
    }
    
    return xError;
}
```

## Rust Implementation Examples

Rust doesn't have official FreeRTOS+FAT bindings, but here's how you would create safe wrappers:

### 1. Safe Rust Wrapper for FreeRTOS+FAT

```rust
// Rust FFI bindings to FreeRTOS+FAT
#![no_std]

use core::ffi::{c_char, c_int, c_void};
use core::ptr;

// FFI declarations
extern "C" {
    fn ff_fopen(path: *const c_char, mode: *const c_char) -> *mut c_void;
    fn ff_fclose(file: *mut c_void) -> c_int;
    fn ff_fread(ptr: *mut u8, size: usize, count: usize, 
                file: *mut c_void) -> usize;
    fn ff_fwrite(ptr: *const u8, size: usize, count: usize, 
                 file: *mut c_void) -> usize;
    fn ff_fseek(file: *mut c_void, offset: i32, whence: c_int) -> c_int;
    fn ff_ftell(file: *mut c_void) -> i32;
    fn ff_fflush(file: *mut c_void) -> c_int;
    fn ff_mkdir(path: *const c_char) -> c_int;
}

// Seek positions
pub enum SeekFrom {
    Start = 0,
    Current = 1,
    End = 2,
}

// Safe wrapper for file handle
pub struct File {
    handle: *mut c_void,
}

impl File {
    /// Open a file with specified mode
    pub fn open(path: &str, mode: &str) -> Result<Self, &'static str> {
        let path_cstr = make_cstring(path)?;
        let mode_cstr = make_cstring(mode)?;
        
        unsafe {
            let handle = ff_fopen(path_cstr.as_ptr(), mode_cstr.as_ptr());
            if handle.is_null() {
                Err("Failed to open file")
            } else {
                Ok(File { handle })
            }
        }
    }
    
    /// Read data from file
    pub fn read(&mut self, buffer: &mut [u8]) -> Result<usize, &'static str> {
        unsafe {
            let bytes_read = ff_fread(
                buffer.as_mut_ptr(),
                1,
                buffer.len(),
                self.handle
            );
            Ok(bytes_read)
        }
    }
    
    /// Write data to file
    pub fn write(&mut self, buffer: &[u8]) -> Result<usize, &'static str> {
        unsafe {
            let bytes_written = ff_fwrite(
                buffer.as_ptr(),
                1,
                buffer.len(),
                self.handle
            );
            
            if bytes_written != buffer.len() {
                Err("Write error")
            } else {
                Ok(bytes_written)
            }
        }
    }
    
    /// Seek to position in file
    pub fn seek(&mut self, pos: SeekFrom, offset: i32) -> Result<i32, &'static str> {
        unsafe {
            let result = ff_fseek(self.handle, offset, pos as c_int);
            if result != 0 {
                Err("Seek failed")
            } else {
                Ok(ff_ftell(self.handle))
            }
        }
    }
    
    /// Flush file buffers
    pub fn flush(&mut self) -> Result<(), &'static str> {
        unsafe {
            if ff_fflush(self.handle) == 0 {
                Ok(())
            } else {
                Err("Flush failed")
            }
        }
    }
}

impl Drop for File {
    fn drop(&mut self) {
        unsafe {
            ff_fclose(self.handle);
        }
    }
}

// Helper function to create null-terminated strings
fn make_cstring(s: &str) -> Result<heapless::Vec<u8, 256>, &'static str> {
    use heapless::Vec;
    
    let mut vec = Vec::new();
    for byte in s.bytes() {
        vec.push(byte).map_err(|_| "Path too long")?;
    }
    vec.push(0).map_err(|_| "Path too long")?;
    Ok(vec)
}

// Directory creation
pub fn create_dir(path: &str) -> Result<(), &'static str> {
    let path_cstr = make_cstring(path)?;
    
    unsafe {
        if ff_mkdir(path_cstr.as_ptr()) == 0 {
            Ok(())
        } else {
            Err("Failed to create directory")
        }
    }
}
```

### 2. Rust Application Example - Data Logger

```rust
use freertos_rust::*;
use freertos_fat::*;

// Data logger task
fn data_logger_task(sensor_queue: Queue<SensorData>) {
    let mut file = match File::open("/sd/logs/data.csv", "a") {
        Ok(f) => f,
        Err(e) => {
            println!("Failed to open log file: {}", e);
            return;
        }
    };
    
    loop {
        // Wait for sensor data
        if let Some(data) = sensor_queue.receive(Duration::ms(1000)) {
            // Format log entry
            let log_entry = format_log_entry(&data);
            
            // Write to file
            match file.write(log_entry.as_bytes()) {
                Ok(_) => {
                    // Flush periodically
                    let _ = file.flush();
                }
                Err(e) => {
                    println!("Write error: {}", e);
                }
            }
        }
    }
}

fn format_log_entry(data: &SensorData) -> heapless::String<128> {
    use core::fmt::Write;
    
    let mut s = heapless::String::new();
    write!(&mut s, "{},{},{}\n", 
           data.timestamp,
           data.temperature,
           data.humidity).unwrap();
    s
}

// Configuration reader with zero-copy parsing
fn read_config(path: &str) -> Result<Config, &'static str> {
    let mut file = File::open(path, "r")?;
    let mut buffer = [0u8; 1024];
    
    let bytes_read = file.read(&mut buffer)?;
    let config_str = core::str::from_utf8(&buffer[..bytes_read])
        .map_err(|_| "Invalid UTF-8")?;
    
    parse_config(config_str)
}
```

### 3. Thread-Safe Circular Buffer Logger in Rust

```rust
use freertos_rust::Mutex;
use core::sync::atomic::{AtomicU32, Ordering};

pub struct CircularLogger {
    current_index: AtomicU32,
    current_position: AtomicU32,
    mutex: Mutex<()>,
    base_path: &'static str,
    max_files: u32,
    max_file_size: u32,
}

impl CircularLogger {
    pub fn new(base_path: &'static str, max_files: u32, max_file_size: u32) 
        -> Result<Self, &'static str> 
    {
        // Create base directory
        create_dir(base_path)?;
        
        Ok(CircularLogger {
            current_index: AtomicU32::new(0),
            current_position: AtomicU32::new(0),
            mutex: Mutex::new(()).unwrap(),
            base_path,
            max_files,
            max_file_size,
        })
    }
    
    pub fn write(&self, data: &[u8]) -> Result<(), &'static str> {
        let _guard = self.mutex.lock(Duration::infinite()).unwrap();
        
        let mut pos = self.current_position.load(Ordering::Relaxed);
        
        // Check if rotation needed
        if pos + data.len() as u32 > self.max_file_size {
            let mut idx = self.current_index.load(Ordering::Relaxed);
            idx = (idx + 1) % self.max_files;
            self.current_index.store(idx, Ordering::Relaxed);
            pos = 0;
            self.current_position.store(0, Ordering::Relaxed);
        }
        
        // Build filename
        let idx = self.current_index.load(Ordering::Relaxed);
        let mut filename = heapless::String::<64>::new();
        use core::fmt::Write;
        write!(&mut filename, "{}/log_{:02}.dat", self.base_path, idx).unwrap();
        
        // Write data
        let mode = if pos == 0 { "wb" } else { "ab" };
        let mut file = File::open(filename.as_str(), mode)?;
        
        let written = file.write(data)?;
        file.flush()?;
        
        pos += written as u32;
        self.current_position.store(pos, Ordering::Relaxed);
        
        Ok(())
    }
}

// Usage in FreeRTOS task
fn logging_task(logger: &'static CircularLogger) {
    loop {
        let data = collect_sensor_data();
        
        match logger.write(&data) {
            Ok(_) => { /* Success */ }
            Err(e) => println!("Log error: {}", e),
        }
        
        CurrentTask::delay(Duration::ms(100));
    }
}
```

## Thread Safety and Best Practices

### Concurrent Access Patterns

```c
// SAFE: Multiple tasks writing to different files
void task_a(void *params) {
    FF_FILE *file = ff_fopen("/sd/task_a.log", "a");
    ff_fwrite("Task A data\n", 1, 12, file);
    ff_fclose(file);
}

void task_b(void *params) {
    FF_FILE *file = ff_fopen("/sd/task_b.log", "a");
    ff_fwrite("Task B data\n", 1, 12, file);
    ff_fclose(file);
}

// CAUTION: Multiple tasks accessing same file
// Use application-level synchronization
SemaphoreHandle_t xSharedFileMutex;

void shared_file_writer(const char *data) {
    xSemaphoreTake(xSharedFileMutex, portMAX_DELAY);
    
    FF_FILE *file = ff_fopen("/sd/shared.log", "a");
    if (file) {
        ff_fwrite(data, 1, strlen(data), file);
        ff_fflush(file);  // Ensure data is written
        ff_fclose(file);
    }
    
    xSemaphoreGive(xSharedFileMutex);
}
```

### Error Handling and Recovery

```c
FF_Error_t safe_write_with_retry(const char *path, 
                                 const void *data,
                                 size_t size)
{
    FF_FILE *file;
    FF_Error_t error = FF_ERR_NONE;
    int retries = 3;
    
    while (retries > 0)
    {
        file = ff_fopen(path, "wb");
        
        if (file != NULL)
        {
            size_t written = ff_fwrite(data, 1, size, file);
            
            if (written == size)
            {
                // Verify flush succeeded
                if (ff_fflush(file) == 0)
                {
                    ff_fclose(file);
                    return FF_ERR_NONE;
                }
            }
            
            ff_fclose(file);
        }
        
        retries--;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    return FF_ERR_DEVICE_DRIVER_FAILED;
}
```

## Summary

FreeRTOS+FAT provides a robust, thread-safe file system solution for embedded systems running FreeRTOS. Its key strengths include automatic mutex protection for all operations, comprehensive FAT12/16/32 support with long filenames, and a clean POSIX-like API that integrates seamlessly with multi-tasking applications.

The file system excels in scenarios requiring persistent storage on SD cards, eMMC, or flash memory while maintaining data integrity across concurrent task access. Implementation involves configuring the disk driver interface (read/write functions), mounting the partition, and then using standard file operations that are automatically protected against race conditions.

Critical considerations include proper error handling and retry logic, appropriate use of flush operations to ensure data persistence, and application-level synchronization when multiple tasks need coordinated access to shared files. The circular logging pattern demonstrated is particularly valuable for flash-based systems where wear leveling is important.

Whether implementing data loggers, configuration managers, or complex file-based protocols, FreeRTOS+FAT provides the foundation for reliable file I/O in resource-constrained embedded environments while maintaining the thread safety guarantees essential for real-time systems.