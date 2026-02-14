# Hierarchical Task Design in FreeRTOS

## Detailed Description

Hierarchical task design is an architectural pattern in FreeRTOS applications where tasks are organized in a parent-child or supervisor-worker relationship. This design pattern helps manage complexity in large embedded systems by establishing clear responsibilities, communication paths, and control hierarchies between tasks.

### Core Concepts

**Supervisor Tasks** act as managers that:
- Create and delete worker tasks
- Monitor worker task health and status
- Coordinate workflow between multiple workers
- Handle resource allocation and deallocation
- Make high-level decisions about system behavior

**Worker Tasks** are specialized units that:
- Perform specific, well-defined operations
- Report status to their supervisor
- Request resources or services from supervisors
- Focus on a single responsibility

### Benefits of Hierarchical Design

1. **Separation of Concerns**: Each task has a clear, focused responsibility
2. **Scalability**: Easy to add new worker tasks without restructuring the entire system
3. **Maintainability**: Isolates changes to specific parts of the hierarchy
4. **Resource Management**: Supervisors can dynamically create/destroy workers based on system load
5. **Fault Isolation**: Failures in worker tasks can be contained and handled by supervisors
6. **Testability**: Individual components can be tested in isolation

### Implementation Strategies

**Static Hierarchy**: All tasks created at startup with fixed relationships
**Dynamic Hierarchy**: Tasks created and destroyed at runtime based on system needs
**Hybrid Approach**: Core supervisors are static, workers are dynamic

## C/C++ Implementation

### Example 1: Basic Supervisor-Worker Pattern

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Command structure for supervisor-worker communication
typedef enum {
    CMD_START_PROCESSING,
    CMD_STOP_PROCESSING,
    CMD_STATUS_REQUEST,
    CMD_SHUTDOWN
} WorkerCommand_t;

typedef struct {
    WorkerCommand_t command;
    void* data;
    uint32_t dataLength;
} CommandMessage_t;

// Worker status structure
typedef struct {
    uint32_t taskId;
    uint32_t itemsProcessed;
    TickType_t lastActivityTime;
    uint8_t isHealthy;
} WorkerStatus_t;

// Global handles
static QueueHandle_t xWorkerCommandQueue;
static QueueHandle_t xSupervisorStatusQueue;
static TaskHandle_t xSupervisorHandle;
static TaskHandle_t xWorkerHandles[3];

// Worker task implementation
void vWorkerTask(void* pvParameters) {
    uint32_t workerId = (uint32_t)pvParameters;
    CommandMessage_t command;
    WorkerStatus_t status = {0};
    uint8_t isRunning = pdFALSE;
    
    status.taskId = workerId;
    
    printf("Worker %lu: Started\n", workerId);
    
    for(;;) {
        // Wait for commands from supervisor
        if(xQueueReceive(xWorkerCommandQueue, &command, pdMS_TO_TICKS(100)) == pdPASS) {
            switch(command.command) {
                case CMD_START_PROCESSING:
                    printf("Worker %lu: Starting processing\n", workerId);
                    isRunning = pdTRUE;
                    break;
                    
                case CMD_STOP_PROCESSING:
                    printf("Worker %lu: Stopping processing\n", workerId);
                    isRunning = pdFALSE;
                    break;
                    
                case CMD_STATUS_REQUEST:
                    status.lastActivityTime = xTaskGetTickCount();
                    status.isHealthy = pdTRUE;
                    xQueueSend(xSupervisorStatusQueue, &status, portMAX_DELAY);
                    break;
                    
                case CMD_SHUTDOWN:
                    printf("Worker %lu: Shutting down\n", workerId);
                    vTaskDelete(NULL);
                    break;
            }
        }
        
        // Perform work if running
        if(isRunning) {
            // Simulate processing work
            vTaskDelay(pdMS_TO_TICKS(50));
            status.itemsProcessed++;
            
            // Periodic status update to supervisor
            if((status.itemsProcessed % 10) == 0) {
                status.lastActivityTime = xTaskGetTickCount();
                status.isHealthy = pdTRUE;
                xQueueSend(xSupervisorStatusQueue, &status, 0);
            }
        }
    }
}

// Supervisor task implementation
void vSupervisorTask(void* pvParameters) {
    CommandMessage_t command;
    WorkerStatus_t workerStatus;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint8_t workerCount = 3;
    
    printf("Supervisor: Starting worker management\n");
    
    // Create worker command queue
    xWorkerCommandQueue = xQueueCreate(10, sizeof(CommandMessage_t));
    xSupervisorStatusQueue = xQueueCreate(10, sizeof(WorkerStatus_t));
    
    // Create worker tasks
    for(uint8_t i = 0; i < workerCount; i++) {
        char taskName[16];
        sprintf(taskName, "Worker_%d", i);
        xTaskCreate(vWorkerTask, taskName, 256, (void*)i, 2, &xWorkerHandles[i]);
    }
    
    // Allow workers to initialize
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Start all workers
    command.command = CMD_START_PROCESSING;
    command.data = NULL;
    for(uint8_t i = 0; i < workerCount; i++) {
        xQueueSend(xWorkerCommandQueue, &command, portMAX_DELAY);
    }
    
    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
        
        // Request status from all workers
        command.command = CMD_STATUS_REQUEST;
        for(uint8_t i = 0; i < workerCount; i++) {
            xQueueSend(xWorkerCommandQueue, &command, portMAX_DELAY);
        }
        
        // Collect and analyze worker status
        for(uint8_t i = 0; i < workerCount; i++) {
            if(xQueueReceive(xSupervisorStatusQueue, &workerStatus, pdMS_TO_TICKS(500)) == pdPASS) {
                printf("Supervisor: Worker %lu processed %lu items, healthy: %d\n",
                       workerStatus.taskId, 
                       workerStatus.itemsProcessed,
                       workerStatus.isHealthy);
                       
                // Check for worker health issues
                TickType_t currentTime = xTaskGetTickCount();
                if((currentTime - workerStatus.lastActivityTime) > pdMS_TO_TICKS(5000)) {
                    printf("Supervisor: Worker %lu appears unresponsive!\n", workerStatus.taskId);
                    // Could restart worker here
                }
            } else {
                printf("Supervisor: Worker %d did not respond to status request\n", i);
            }
        }
    }
}
```

### Example 2: Dynamic Worker Creation with Lifecycle Management

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdlib.h>

#define MAX_WORKERS 5

typedef struct WorkerContext {
    TaskHandle_t taskHandle;
    uint32_t workerId;
    uint8_t isActive;
    void* workData;
    SemaphoreHandle_t completionSemaphore;
} WorkerContext_t;

// Supervisor state
typedef struct {
    WorkerContext_t workers[MAX_WORKERS];
    uint8_t activeWorkerCount;
    SemaphoreHandle_t workerPoolMutex;
} SupervisorState_t;

static SupervisorState_t gSupervisorState = {0};

// Worker lifecycle management
void vDynamicWorkerTask(void* pvParameters) {
    WorkerContext_t* ctx = (WorkerContext_t*)pvParameters;
    
    printf("Dynamic Worker %lu: Started\n", ctx->workerId);
    
    // Perform assigned work
    if(ctx->workData != NULL) {
        // Process data (simulated)
        vTaskDelay(pdMS_TO_TICKS(500 + (rand() % 1000)));
        printf("Dynamic Worker %lu: Work completed\n", ctx->workerId);
    }
    
    // Signal completion
    xSemaphoreGive(ctx->completionSemaphore);
    
    // Worker self-terminates
    vTaskDelete(NULL);
}

// Supervisor creates worker on demand
BaseType_t xSupervisorSpawnWorker(void* workData) {
    BaseType_t result = pdFAIL;
    
    // Lock worker pool
    if(xSemaphoreTake(gSupervisorState.workerPoolMutex, portMAX_DELAY) == pdPASS) {
        // Find free worker slot
        for(uint8_t i = 0; i < MAX_WORKERS; i++) {
            if(!gSupervisorState.workers[i].isActive) {
                WorkerContext_t* ctx = &gSupervisorState.workers[i];
                
                // Initialize worker context
                ctx->workerId = i;
                ctx->workData = workData;
                ctx->isActive = pdTRUE;
                
                if(ctx->completionSemaphore == NULL) {
                    ctx->completionSemaphore = xSemaphoreCreateBinary();
                }
                
                // Create worker task
                char taskName[16];
                sprintf(taskName, "DynWorker_%d", i);
                
                if(xTaskCreate(vDynamicWorkerTask, taskName, 512, ctx, 2, 
                              &ctx->taskHandle) == pdPASS) {
                    gSupervisorState.activeWorkerCount++;
                    printf("Supervisor: Spawned worker %d (total active: %d)\n", 
                           i, gSupervisorState.activeWorkerCount);
                    result = pdPASS;
                } else {
                    ctx->isActive = pdFALSE;
                }
                break;
            }
        }
        xSemaphoreGive(gSupervisorState.workerPoolMutex);
    }
    
    return result;
}

// Supervisor monitors and cleans up completed workers
void vSupervisorMonitorTask(void* pvParameters) {
    gSupervisorState.workerPoolMutex = xSemaphoreCreateMutex();
    
    printf("Supervisor Monitor: Started\n");
    
    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        
        // Check for completed workers
        if(xSemaphoreTake(gSupervisorState.workerPoolMutex, portMAX_DELAY) == pdPASS) {
            for(uint8_t i = 0; i < MAX_WORKERS; i++) {
                WorkerContext_t* ctx = &gSupervisorState.workers[i];
                
                if(ctx->isActive && ctx->completionSemaphore != NULL) {
                    // Check if worker completed (non-blocking)
                    if(xSemaphoreTake(ctx->completionSemaphore, 0) == pdPASS) {
                        printf("Supervisor: Worker %d completed, cleaning up\n", i);
                        
                        // Clean up worker resources
                        if(ctx->workData != NULL) {
                            free(ctx->workData);
                            ctx->workData = NULL;
                        }
                        
                        ctx->isActive = pdFALSE;
                        ctx->taskHandle = NULL;
                        gSupervisorState.activeWorkerCount--;
                        
                        printf("Supervisor: Active workers: %d\n", 
                               gSupervisorState.activeWorkerCount);
                    }
                }
            }
            xSemaphoreGive(gSupervisorState.workerPoolMutex);
        }
    }
}
```

## Rust Implementation

### Example 1: Hierarchical Task with Message Passing

```rust
#![no_std]
#![no_main]

use freertos_rust::*;
use core::time::Duration;

#[derive(Clone, Copy)]
enum WorkerCommand {
    ProcessData(u32),
    GetStatus,
    Shutdown,
}

#[derive(Clone, Copy)]
enum StatusReport {
    WorkerStatus {
        worker_id: u32,
        items_processed: u32,
        is_healthy: bool,
    },
}

struct WorkerContext {
    worker_id: u32,
    command_queue: Queue<WorkerCommand>,
    status_queue: Queue<StatusReport>,
}

impl WorkerContext {
    fn new(
        worker_id: u32,
        command_queue: Queue<WorkerCommand>,
        status_queue: Queue<StatusReport>,
    ) -> Self {
        Self {
            worker_id,
            command_queue,
            status_queue,
        }
    }
    
    fn run(&mut self) {
        let mut items_processed = 0u32;
        
        println!("Worker {}: Started", self.worker_id);
        
        loop {
            // Wait for commands with timeout
            match self.command_queue.receive(Duration::from_millis(100)) {
                Ok(command) => {
                    match command {
                        WorkerCommand::ProcessData(data) => {
                            println!("Worker {}: Processing data {}", self.worker_id, data);
                            
                            // Simulate work
                            CurrentTask::delay(Duration::from_millis(50));
                            items_processed += 1;
                            
                            // Periodic status update
                            if items_processed % 10 == 0 {
                                self.send_status(items_processed);
                            }
                        }
                        
                        WorkerCommand::GetStatus => {
                            self.send_status(items_processed);
                        }
                        
                        WorkerCommand::Shutdown => {
                            println!("Worker {}: Shutting down", self.worker_id);
                            break;
                        }
                    }
                }
                
                Err(_) => {
                    // Timeout - continue processing
                }
            }
        }
    }
    
    fn send_status(&self, items_processed: u32) {
        let status = StatusReport::WorkerStatus {
            worker_id: self.worker_id,
            items_processed,
            is_healthy: true,
        };
        
        let _ = self.status_queue.send(status, Duration::from_millis(10));
    }
}

struct Supervisor {
    worker_command_queues: [Queue<WorkerCommand>; 3],
    status_queue: Queue<StatusReport>,
    worker_count: usize,
}

impl Supervisor {
    fn new() -> Self {
        let command_queue_0 = Queue::new(10).unwrap();
        let command_queue_1 = Queue::new(10).unwrap();
        let command_queue_2 = Queue::new(10).unwrap();
        let status_queue = Queue::new(10).unwrap();
        
        Self {
            worker_command_queues: [command_queue_0, command_queue_1, command_queue_2],
            status_queue,
            worker_count: 3,
        }
    }
    
    fn spawn_workers(&self) {
        for i in 0..self.worker_count {
            let mut context = WorkerContext::new(
                i as u32,
                self.worker_command_queues[i].clone(),
                self.status_queue.clone(),
            );
            
            Task::new()
                .name(&format!("Worker_{}", i))
                .stack_size(2048)
                .priority(TaskPriority(2))
                .start(move || {
                    context.run();
                })
                .unwrap();
        }
    }
    
    fn run(&mut self) {
        println!("Supervisor: Starting");
        
        self.spawn_workers();
        
        // Give workers time to start
        CurrentTask::delay(Duration::from_millis(100));
        
        // Start all workers with initial data
        for i in 0..self.worker_count {
            let _ = self.worker_command_queues[i].send(
                WorkerCommand::ProcessData(i as u32 * 100),
                Duration::from_millis(100),
            );
        }
        
        let mut work_counter = 0u32;
        
        loop {
            CurrentTask::delay(Duration::from_secs(1));
            
            // Request status from all workers
            for i in 0..self.worker_count {
                let _ = self.worker_command_queues[i].send(
                    WorkerCommand::GetStatus,
                    Duration::from_millis(100),
                );
            }
            
            // Collect status reports
            for _ in 0..self.worker_count {
                match self.status_queue.receive(Duration::from_millis(500)) {
                    Ok(StatusReport::WorkerStatus { 
                        worker_id, 
                        items_processed, 
                        is_healthy 
                    }) => {
                        println!(
                            "Supervisor: Worker {} - Processed: {}, Healthy: {}",
                            worker_id, items_processed, is_healthy
                        );
                    }
                    
                    Err(_) => {
                        println!("Supervisor: Worker did not respond");
                    }
                }
            }
            
            // Send new work periodically
            work_counter += 1;
            if work_counter % 5 == 0 {
                for i in 0..self.worker_count {
                    let _ = self.worker_command_queues[i].send(
                        WorkerCommand::ProcessData(work_counter + i as u32),
                        Duration::from_millis(100),
                    );
                }
            }
        }
    }
}
```

### Example 2: Dynamic Worker Pool with Lifetime Management

```rust
use freertos_rust::*;
use core::time::Duration;
use core::sync::atomic::{AtomicU32, Ordering};

static WORKER_ID_COUNTER: AtomicU32 = AtomicU32::new(0);

#[derive(Clone, Copy)]
struct WorkItem {
    data: u32,
    priority: u8,
}

struct DynamicWorker {
    worker_id: u32,
    work_item: WorkItem,
    completion_signal: Arc<Semaphore>,
}

impl DynamicWorker {
    fn new(work_item: WorkItem, completion_signal: Arc<Semaphore>) -> Self {
        let worker_id = WORKER_ID_COUNTER.fetch_add(1, Ordering::Relaxed);
        
        Self {
            worker_id,
            work_item,
            completion_signal,
        }
    }
    
    fn run(self) {
        println!("Dynamic Worker {}: Started with data {}", 
                 self.worker_id, self.work_item.data);
        
        // Simulate variable work duration
        let work_duration = 500 + (self.work_item.data % 1000);
        CurrentTask::delay(Duration::from_millis(work_duration as u64));
        
        println!("Dynamic Worker {}: Completed", self.worker_id);
        
        // Signal completion
        self.completion_signal.release();
        
        // Worker task self-terminates
    }
}

struct WorkerPool {
    max_workers: usize,
    active_count: Arc<Mutex<usize>>,
    work_queue: Queue<WorkItem>,
}

impl WorkerPool {
    fn new(max_workers: usize, queue_size: usize) -> Self {
        Self {
            max_workers,
            active_count: Arc::new(Mutex::new(0).unwrap()),
            work_queue: Queue::new(queue_size).unwrap(),
        }
    }
    
    fn submit_work(&self, work_item: WorkItem) -> Result<(), &'static str> {
        self.work_queue
            .send(work_item, Duration::from_millis(100))
            .map_err(|_| "Work queue full")
    }
    
    fn spawn_worker_if_available(&self, work_item: WorkItem) -> bool {
        let mut count = self.active_count.lock(Duration::from_millis(100)).unwrap();
        
        if *count < self.max_workers {
            let completion_signal = Arc::new(Semaphore::new_binary().unwrap());
            let active_count_clone = self.active_count.clone();
            let signal_clone = completion_signal.clone();
            
            let worker = DynamicWorker::new(work_item, completion_signal);
            
            // Spawn worker task
            Task::new()
                .name(&format!("DynWorker_{}", worker.worker_id))
                .stack_size(1024)
                .priority(TaskPriority(2))
                .start(move || {
                    worker.run();
                    
                    // Cleanup: decrement active count
                    if let Ok(mut count) = active_count_clone.lock(Duration::from_millis(100)) {
                        *count -= 1;
                        println!("Worker pool: Active workers: {}", *count);
                    }
                })
                .unwrap();
            
            *count += 1;
            println!("Worker pool: Spawned worker (active: {})", *count);
            true
        } else {
            false
        }
    }
}

struct PoolSupervisor {
    pool: Arc<WorkerPool>,
}

impl PoolSupervisor {
    fn new(max_workers: usize) -> Self {
        Self {
            pool: Arc::new(WorkerPool::new(max_workers, 20)),
        }
    }
    
    fn run(&mut self) {
        println!("Pool Supervisor: Started");
        
        loop {
            // Try to spawn workers for queued work
            if let Ok(work_item) = self.pool.work_queue.receive(Duration::from_millis(100)) {
                if !self.pool.spawn_worker_if_available(work_item) {
                    // No worker slots available, re-queue
                    let _ = self.pool.work_queue.send(work_item, Duration::from_millis(10));
                }
            }
            
            CurrentTask::delay(Duration::from_millis(50));
        }
    }
    
    fn get_pool_handle(&self) -> Arc<WorkerPool> {
        self.pool.clone()
    }
}
```

## Summary

**Hierarchical task design** is a powerful architectural pattern for organizing complex FreeRTOS applications. By establishing clear supervisor-worker relationships, applications gain better modularity, maintainability, and scalability.

### Key Takeaways:

1. **Clear Responsibilities**: Supervisors manage lifecycle and coordination; workers execute specific tasks
2. **Communication Patterns**: Use queues for command/status exchange and semaphores for synchronization
3. **Lifecycle Management**: Supervisors control when workers are created, monitored, and destroyed
4. **Dynamic vs Static**: Choose based on resource constraints and application requirements
5. **Fault Tolerance**: Supervisors can detect and recover from worker failures
6. **Resource Efficiency**: Dynamic workers free resources when idle; static workers have predictable memory usage

### Best Practices:

- Keep worker tasks focused on single responsibilities
- Implement health monitoring and timeout detection
- Use proper synchronization for shared resources
- Design clear command and status message protocols
- Consider priority inheritance for supervisor tasks
- Balance between dynamic flexibility and static predictability
- Document task hierarchies and communication flows

This pattern is especially valuable in applications like data processing pipelines, sensor networks, communication protocols, and any system where work can be decomposed into manageable, coordinated units.