# Memory Corruption Debugging in FreeRTOS

Memory corruption is one of the most challenging classes of bugs in embedded systems, particularly in multi-tasking environments like FreeRTOS. These issues can manifest as subtle, intermittent failures that are difficult to reproduce and diagnose. This guide covers comprehensive techniques for identifying and resolving memory corruption issues.

## Understanding Memory Corruption in FreeRTOS

Memory corruption occurs when code writes to memory locations it shouldn't access, potentially overwriting:
- Task stacks
- Heap allocations
- Global/static variables
- Kernel data structures
- Memory-mapped peripheral registers

In FreeRTOS systems, common causes include:
- **Stack overflow**: Tasks using more stack than allocated
- **Buffer overruns**: Writing beyond array bounds
- **Heap corruption**: Mismanaged dynamic memory
- **Dangling pointers**: Using freed memory
- **Race conditions**: Unsynchronized access to shared resources

## Detection Techniques

### 1. Stack Overflow Detection

FreeRTOS provides built-in stack checking mechanisms:

**C/C++ Implementation:**

```c
// FreeRTOSConfig.h - Enable stack overflow checking
#define configCHECK_FOR_STACK_OVERFLOW 2  // Method 2: more thorough

// Application hook called when overflow detected
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    // Disable interrupts to prevent further corruption
    taskDISABLE_INTERRUPTS();
    
    // Log the error
    printf("STACK OVERFLOW in task: %s\n", pcTaskName);
    
    // Trigger debugger breakpoint
    __asm volatile("bkpt #0");
    
    // Infinite loop for post-mortem debugging
    for(;;);
}

// Runtime stack monitoring
void vTaskMonitorStacks(void)
{
    TaskHandle_t xHandle;
    UBaseType_t uxHighWaterMark;
    
    // Check specific task
    xHandle = xTaskGetHandle("MyTask");
    uxHighWaterMark = uxTaskGetStackHighWaterMark(xHandle);
    
    printf("Task: MyTask, Remaining stack: %u words\n", uxHighWaterMark);
    
    // Warning threshold
    if(uxHighWaterMark < 50) {
        printf("WARNING: Low stack space!\n");
    }
}

// Periodic monitoring task
void vStackMonitorTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;)
    {
        // Check all tasks
        char *pcWriteBuffer = pvPortMalloc(1024);
        if(pcWriteBuffer != NULL)
        {
            vTaskList(pcWriteBuffer);
            printf("Task List:\n%s\n", pcWriteBuffer);
            vPortFree(pcWriteBuffer);
        }
        
        vTaskMonitorStacks();
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000));
    }
}
```

**Rust Implementation:**

```rust
use freertos_rust::*;
use core::ffi::c_char;

// Stack overflow hook (called from C)
#[no_mangle]
pub extern "C" fn vApplicationStackOverflowHook(
    _task: FreeRtosTaskHandle,
    task_name: *const c_char
) {
    unsafe {
        // Convert C string to Rust
        let name = core::ffi::CStr::from_ptr(task_name);
        
        // Log error (using panic in no_std environment)
        panic!("Stack overflow in task: {:?}", name);
    }
}

// Stack monitoring wrapper
pub struct StackMonitor {
    warning_threshold: usize,
}

impl StackMonitor {
    pub fn new(threshold: usize) -> Self {
        Self {
            warning_threshold: threshold,
        }
    }
    
    pub fn check_task_stack(&self, task_name: &str) -> Result<usize, FreeRtosError> {
        // Get task handle by name
        let handle = Task::get_by_name(task_name)?;
        let remaining = handle.get_stack_high_water_mark();
        
        if remaining < self.warning_threshold {
            defmt::warn!(
                "Task {} has only {} words of stack remaining!",
                task_name,
                remaining
            );
        }
        
        Ok(remaining)
    }
}

// Monitoring task
fn stack_monitor_task(monitor: StackMonitor) {
    let mut delay = Duration::ms(5000);
    
    loop {
        // Check multiple tasks
        let tasks = ["Task1", "Task2", "Task3"];
        
        for task_name in &tasks {
            match monitor.check_task_stack(task_name) {
                Ok(remaining) => {
                    defmt::info!("{}: {} words remaining", task_name, remaining);
                }
                Err(e) => {
                    defmt::error!("Failed to check {}: {:?}", task_name, e);
                }
            }
        }
        
        CurrentTask::delay(delay);
    }
}
```

### 2. Heap Corruption Detection

**C/C++ Implementation:**

```c
// Custom heap implementation with guards
#define HEAP_GUARD_VALUE 0xDEADBEEF

typedef struct HeapBlock {
    uint32_t frontGuard;
    size_t size;
    struct HeapBlock *next;
    uint8_t data[];  // Flexible array member
    // backGuard follows data
} HeapBlock_t;

void* pvGuardedMalloc(size_t xSize)
{
    // Allocate extra space for guards
    size_t totalSize = sizeof(HeapBlock_t) + xSize + sizeof(uint32_t);
    HeapBlock_t *pxBlock = pvPortMalloc(totalSize);
    
    if(pxBlock != NULL)
    {
        pxBlock->frontGuard = HEAP_GUARD_VALUE;
        pxBlock->size = xSize;
        
        // Place back guard after data
        uint32_t *pxBackGuard = (uint32_t*)(&pxBlock->data[xSize]);
        *pxBackGuard = HEAP_GUARD_VALUE;
        
        return pxBlock->data;
    }
    
    return NULL;
}

BaseType_t xCheckGuards(void *pvData)
{
    HeapBlock_t *pxBlock = (HeapBlock_t*)((uint8_t*)pvData - offsetof(HeapBlock_t, data));
    uint32_t *pxBackGuard = (uint32_t*)(&pxBlock->data[pxBlock->size]);
    
    if(pxBlock->frontGuard != HEAP_GUARD_VALUE)
    {
        printf("HEAP CORRUPTION: Front guard corrupted at %p\n", pvData);
        return pdFALSE;
    }
    
    if(*pxBackGuard != HEAP_GUARD_VALUE)
    {
        printf("HEAP CORRUPTION: Back guard corrupted at %p\n", pvData);
        return pdFALSE;
    }
    
    return pdTRUE;
}

void vGuardedFree(void *pvData)
{
    if(pvData != NULL)
    {
        // Check guards before freeing
        if(xCheckGuards(pvData) == pdTRUE)
        {
            HeapBlock_t *pxBlock = (HeapBlock_t*)((uint8_t*)pvData - 
                                                   offsetof(HeapBlock_t, data));
            vPortFree(pxBlock);
        }
        else
        {
            // Corruption detected - halt system
            taskDISABLE_INTERRUPTS();
            for(;;);
        }
    }
}

// Heap statistics and monitoring
void vPrintHeapStats(void)
{
    HeapStats_t xHeapStats;
    
    vPortGetHeapStats(&xHeapStats);
    
    printf("Heap Statistics:\n");
    printf("  Available: %u bytes\n", xHeapStats.xAvailableHeapSpaceInBytes);
    printf("  Largest free block: %u bytes\n", 
           xHeapStats.xSizeOfLargestFreeBlockInBytes);
    printf("  Smallest free block: %u bytes\n", 
           xHeapStats.xSizeOfSmallestFreeBlockInBytes);
    printf("  Number of free blocks: %u\n", 
           xHeapStats.xNumberOfFreeBlocks);
    printf("  Minimum ever free: %u bytes\n", 
           xHeapStats.xMinimumEverFreeBytesRemaining);
    printf("  Successful allocations: %u\n", 
           xHeapStats.xNumberOfSuccessfulAllocations);
    printf("  Successful frees: %u\n", 
           xHeapStats.xNumberOfSuccessfulFrees);
}
```

**Rust Implementation:**

```rust
use core::alloc::{GlobalAlloc, Layout};
use core::ptr::NonNull;

const GUARD_VALUE: u32 = 0xDEADBEEF;

#[repr(C)]
struct GuardedBlock {
    front_guard: u32,
    size: usize,
    layout: Layout,
    // data follows
    // back_guard follows data
}

pub struct GuardedAllocator<A: GlobalAlloc> {
    inner: A,
}

impl<A: GlobalAlloc> GuardedAllocator<A> {
    pub const fn new(inner: A) -> Self {
        Self { inner }
    }
    
    fn check_guards(&self, ptr: *mut u8, size: usize) -> bool {
        unsafe {
            let block = ptr.sub(core::mem::size_of::<GuardedBlock>()) 
                as *const GuardedBlock;
            
            if (*block).front_guard != GUARD_VALUE {
                defmt::error!("Front guard corrupted at {:?}", ptr);
                return false;
            }
            
            let back_guard_ptr = ptr.add(size) as *const u32;
            if *back_guard_ptr != GUARD_VALUE {
                defmt::error!("Back guard corrupted at {:?}", ptr);
                return false;
            }
            
            true
        }
    }
}

unsafe impl<A: GlobalAlloc> GlobalAlloc for GuardedAllocator<A> {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let guard_size = core::mem::size_of::<GuardedBlock>() + 
                        core::mem::size_of::<u32>();
        
        let total_size = layout.size() + guard_size;
        let new_layout = Layout::from_size_align_unchecked(
            total_size,
            layout.align()
        );
        
        let ptr = self.inner.alloc(new_layout);
        if ptr.is_null() {
            return ptr;
        }
        
        let block = ptr as *mut GuardedBlock;
        (*block).front_guard = GUARD_VALUE;
        (*block).size = layout.size();
        (*block).layout = layout;
        
        let data_ptr = ptr.add(core::mem::size_of::<GuardedBlock>());
        let back_guard = data_ptr.add(layout.size()) as *mut u32;
        *back_guard = GUARD_VALUE;
        
        data_ptr
    }
    
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        // Check guards before deallocation
        if !self.check_guards(ptr, layout.size()) {
            panic!("Heap corruption detected during dealloc");
        }
        
        let block_ptr = ptr.sub(core::mem::size_of::<GuardedBlock>());
        let block = block_ptr as *const GuardedBlock;
        
        self.inner.dealloc(block_ptr, (*block).layout);
    }
}
```

### 3. Buffer Overrun Detection

**C/C++ Implementation:**

```c
// Safe buffer with bounds checking
typedef struct {
    uint8_t *pucData;
    size_t xSize;
    size_t xUsed;
    uint32_t ulChecksum;
} SafeBuffer_t;

SafeBuffer_t* pxCreateSafeBuffer(size_t xSize)
{
    SafeBuffer_t *pxBuffer = pvPortMalloc(sizeof(SafeBuffer_t));
    if(pxBuffer == NULL) return NULL;
    
    pxBuffer->pucData = pvPortMalloc(xSize);
    if(pxBuffer->pucData == NULL) {
        vPortFree(pxBuffer);
        return NULL;
    }
    
    pxBuffer->xSize = xSize;
    pxBuffer->xUsed = 0;
    pxBuffer->ulChecksum = 0;
    
    return pxBuffer;
}

BaseType_t xSafeBufferWrite(SafeBuffer_t *pxBuffer, 
                            const uint8_t *pucData, 
                            size_t xLength)
{
    // Bounds check
    if(pxBuffer->xUsed + xLength > pxBuffer->xSize) {
        printf("ERROR: Buffer overrun prevented! Used: %u, Size: %u, Write: %u\n",
               pxBuffer->xUsed, pxBuffer->xSize, xLength);
        return pdFALSE;
    }
    
    // Critical section for thread safety
    taskENTER_CRITICAL();
    {
        memcpy(&pxBuffer->pucData[pxBuffer->xUsed], pucData, xLength);
        pxBuffer->xUsed += xLength;
        
        // Update checksum
        for(size_t i = 0; i < xLength; i++) {
            pxBuffer->ulChecksum += pucData[i];
        }
    }
    taskEXIT_CRITICAL();
    
    return pdTRUE;
}

// Array bounds checking macro
#define SAFE_ARRAY_ACCESS(array, index, size) \
    ({ \
        if((index) >= (size)) { \
            printf("Array bounds violation: index %u >= size %u at %s:%d\n", \
                   (index), (size), __FILE__, __LINE__); \
            configASSERT(0); \
        } \
        (array)[(index)]; \
    })

// Usage example
void vTaskWithBoundsChecking(void *pvParameters)
{
    uint8_t ucArray[10];
    
    for(uint32_t i = 0; i < 20; i++) {
        // This will trigger assertion when i >= 10
        SAFE_ARRAY_ACCESS(ucArray, i, 10) = i;
    }
}
```

**Rust Implementation:**

```rust
// Rust naturally prevents buffer overruns through bounds checking
use heapless::Vec;

pub struct SafeBuffer {
    data: Vec<u8, 1024>,  // Fixed-size buffer
    checksum: u32,
}

impl SafeBuffer {
    pub fn new() -> Self {
        Self {
            data: Vec::new(),
            checksum: 0,
        }
    }
    
    // Safe write with automatic bounds checking
    pub fn write(&mut self, bytes: &[u8]) -> Result<(), BufferError> {
        // Rust's push will return error if capacity exceeded
        for &byte in bytes {
            self.data.push(byte)
                .map_err(|_| BufferError::Overflow)?;
            self.checksum = self.checksum.wrapping_add(byte as u32);
        }
        
        Ok(())
    }
    
    // Safe indexed access
    pub fn get(&self, index: usize) -> Option<u8> {
        self.data.get(index).copied()
    }
    
    // Verify integrity
    pub fn verify_checksum(&self) -> bool {
        let calculated: u32 = self.data.iter()
            .map(|&b| b as u32)
            .sum();
        
        calculated == self.checksum
    }
}

#[derive(Debug)]
pub enum BufferError {
    Overflow,
    InvalidChecksum,
}

// Task demonstrating safe buffer usage
fn safe_buffer_task() {
    let mut buffer = SafeBuffer::new();
    
    let data = b"Hello, FreeRTOS!";
    
    match buffer.write(data) {
        Ok(_) => {
            defmt::info!("Data written successfully");
            
            // Safe indexed access
            if let Some(byte) = buffer.get(0) {
                defmt::info!("First byte: {}", byte);
            }
        }
        Err(BufferError::Overflow) => {
            defmt::error!("Buffer overflow prevented!");
        }
        Err(e) => {
            defmt::error!("Write error: {:?}", e);
        }
    }
    
    // This will compile but not panic - returns None
    let out_of_bounds = buffer.get(9999);
    assert!(out_of_bounds.is_none());
}
```

### 4. Memory Poisoning and Pattern Detection

**C/C++ Implementation:**

```c
// Fill freed memory with pattern to detect use-after-free
#define FREED_MEMORY_PATTERN 0xDD
#define UNINITIALIZED_PATTERN 0xCD

void vPoisonedFree(void *pvMemory, size_t xSize)
{
    if(pvMemory != NULL) {
        // Fill with poison pattern
        memset(pvMemory, FREED_MEMORY_PATTERN, xSize);
        vPortFree(pvMemory);
    }
}

void* pvInitializedMalloc(size_t xSize)
{
    void *pvMemory = pvPortMalloc(xSize);
    
    if(pvMemory != NULL) {
        // Fill with uninitialized pattern
        memset(pvMemory, UNINITIALIZED_PATTERN, xSize);
    }
    
    return pvMemory;
}

// Detect use-after-free
BaseType_t xDetectUseAfterFree(const void *pvMemory, size_t xSize)
{
    const uint8_t *pucBytes = (const uint8_t*)pvMemory;
    size_t xPoisonedBytes = 0;
    
    for(size_t i = 0; i < xSize; i++) {
        if(pucBytes[i] == FREED_MEMORY_PATTERN) {
            xPoisonedBytes++;
        }
    }
    
    // If >50% of memory matches poison pattern, likely use-after-free
    if(xPoisonedBytes > (xSize / 2)) {
        printf("Possible use-after-free detected: %u/%u bytes poisoned\n",
               xPoisonedBytes, xSize);
        return pdTRUE;
    }
    
    return pdFALSE;
}
```

### 5. MPU (Memory Protection Unit) Configuration

**C/C++ Implementation:**

```c
// Configure MPU for task isolation
void vSetupTaskMPU(void)
{
    #if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
    
    // Enable MPU with default memory map as background
    MPU->CTRL = MPU_CTRL_ENABLE_Msk | MPU_CTRL_PRIVDEFENA_Msk;
    
    // Region 0: Flash (read-only, executable)
    MPU->RBAR = 0x08000000 | MPU_RBAR_VALID_Msk | 0;
    MPU->RASR = MPU_RASR_ENABLE_Msk |
                MPU_RASR_SIZE_Msk |  // Determine size based on device
                (0x06 << MPU_RASR_AP_Pos) |  // Read-only
                MPU_RASR_XN_Msk;
    
    // Region 1: RAM (read-write, no execute)
    MPU->RBAR = 0x20000000 | MPU_RBAR_VALID_Msk | 1;
    MPU->RASR = MPU_RASR_ENABLE_Msk |
                (0x03 << MPU_RASR_AP_Pos) |  // Full access
                MPU_RASR_XN_Msk;  // Execute never
    
    // Region 2: Peripheral region (device memory)
    MPU->RBAR = 0x40000000 | MPU_RBAR_VALID_Msk | 2;
    MPU->RASR = MPU_RASR_ENABLE_Msk |
                (0x03 << MPU_RASR_AP_Pos) |
                (0x05 << MPU_RASR_TEX_Pos) |  // Device memory
                MPU_RASR_B_Msk;
    
    __DSB();
    __ISB();
    
    #endif
}

// MPU fault handler
void MemManage_Handler(void)
{
    uint32_t cfsr = SCB->CFSR;
    uint32_t mmfar = SCB->MMFAR;
    
    printf("MPU FAULT!\n");
    printf("CFSR: 0x%08lx\n", cfsr);
    
    if(cfsr & SCB_CFSR_MMARVALID_Msk) {
        printf("Fault address: 0x%08lx\n", mmfar);
    }
    
    if(cfsr & SCB_CFSR_DACCVIOL_Msk) {
        printf("Data access violation\n");
    }
    
    if(cfsr & SCB_CFSR_IACCVIOL_Msk) {
        printf("Instruction access violation\n");
    }
    
    taskDISABLE_INTERRUPTS();
    for(;;);
}
```

## Advanced Debugging Techniques

**C/C++ - Memory Tracing:**

```c
#define MAX_ALLOC_RECORDS 100

typedef struct {
    void *pvAddress;
    size_t xSize;
    const char *pcFile;
    uint32_t ulLine;
    TickType_t xTimestamp;
    BaseType_t xAllocated;  // 1=allocated, 0=freed
} AllocRecord_t;

static AllocRecord_t xAllocRecords[MAX_ALLOC_RECORDS];
static uint32_t ulRecordIndex = 0;
static SemaphoreHandle_t xRecordMutex;

void vInitMemoryTracing(void)
{
    xRecordMutex = xSemaphoreCreateMutex();
    memset(xAllocRecords, 0, sizeof(xAllocRecords));
}

void* pvTracedMalloc(size_t xSize, const char *pcFile, uint32_t ulLine)
{
    void *pvMemory = pvPortMalloc(xSize);
    
    if(pvMemory != NULL && xRecordMutex != NULL) {
        xSemaphoreTake(xRecordMutex, portMAX_DELAY);
        {
            uint32_t ulIndex = ulRecordIndex % MAX_ALLOC_RECORDS;
            
            xAllocRecords[ulIndex].pvAddress = pvMemory;
            xAllocRecords[ulIndex].xSize = xSize;
            xAllocRecords[ulIndex].pcFile = pcFile;
            xAllocRecords[ulIndex].ulLine = ulLine;
            xAllocRecords[ulIndex].xTimestamp = xTaskGetTickCount();
            xAllocRecords[ulIndex].xAllocated = pdTRUE;
            
            ulRecordIndex++;
        }
        xSemaphoreGive(xRecordMutex);
    }
    
    return pvMemory;
}

void vTracedFree(void *pvMemory)
{
    if(pvMemory != NULL && xRecordMutex != NULL) {
        xSemaphoreTake(xRecordMutex, portMAX_DELAY);
        {
            // Find allocation record
            for(uint32_t i = 0; i < MAX_ALLOC_RECORDS; i++) {
                if(xAllocRecords[i].pvAddress == pvMemory && 
                   xAllocRecords[i].xAllocated == pdTRUE) {
                    xAllocRecords[i].xAllocated = pdFALSE;
                    break;
                }
            }
        }
        xSemaphoreGive(xRecordMutex);
    }
    
    vPortFree(pvMemory);
}

void vPrintMemoryLeaks(void)
{
    printf("=== Memory Leak Report ===\n");
    
    xSemaphoreTake(xRecordMutex, portMAX_DELAY);
    {
        for(uint32_t i = 0; i < MAX_ALLOC_RECORDS; i++) {
            if(xAllocRecords[i].xAllocated == pdTRUE) {
                printf("LEAK: %u bytes at %p\n", 
                       xAllocRecords[i].xSize,
                       xAllocRecords[i].pvAddress);
                printf("  Allocated: %s:%lu (tick %lu)\n",
                       xAllocRecords[i].pcFile,
                       xAllocRecords[i].ulLine,
                       xAllocRecords[i].xTimestamp);
            }
        }
    }
    xSemaphoreGive(xRecordMutex);
}

// Macro for traced allocations
#define TRACED_MALLOC(size) pvTracedMalloc((size), __FILE__, __LINE__)
#define TRACED_FREE(ptr) vTracedFree(ptr)
```

## Summary

Memory corruption debugging in FreeRTOS requires a multi-layered approach combining preventive measures, detection mechanisms, and diagnostic tools. Key strategies include:

**Prevention:**
- Proper stack sizing and monitoring
- Bounds checking on all array accesses
- Thread-safe memory management with mutexes
- Using safe data structures and APIs

**Detection:**
- Stack overflow hooks (configCHECK_FOR_STACK_OVERFLOW)
- Guard bytes around heap allocations
- Memory poisoning patterns
- Checksum validation
- MPU for hardware-level protection

**Diagnosis:**
- Memory tracing with allocation tracking
- Runtime statistics monitoring
- Post-mortem debugging with preserved state
- Pattern analysis for corruption types

**Language Considerations:**
- C/C++ requires explicit bounds checking, guard patterns, and careful pointer management
- Rust provides compile-time memory safety but still needs runtime monitoring in embedded contexts
- Both benefit from FreeRTOS's built-in debugging features when properly configured

The most effective approach combines multiple techniques: enable all FreeRTOS debugging features during development, implement guard bytes and checksums for critical data structures, use memory tracing to identify leaks, and leverage hardware MPU when available. Regular monitoring of stack usage and heap statistics helps catch issues before they cause system failures.