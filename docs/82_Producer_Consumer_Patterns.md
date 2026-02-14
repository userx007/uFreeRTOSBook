# Producer-Consumer Patterns in FreeRTOS

## Detailed Description

Producer-Consumer patterns are fundamental concurrent programming paradigms where one or more tasks (producers) generate data and one or more tasks (consumers) process that data. In FreeRTOS, these patterns are typically implemented using **queues**, **stream buffers**, or **message buffers** to decouple producers from consumers, allowing them to operate at different rates while maintaining thread-safe communication.

### Core Concepts

**1. Multi-Producer Multi-Consumer (MPMC) Queues**
- Multiple tasks can write to the same queue
- Multiple tasks can read from the same queue
- FreeRTOS queues are inherently thread-safe
- Items are typically processed in FIFO order

**2. Backpressure Handling**
- Managing scenarios where producers generate data faster than consumers can process
- Strategies include: blocking producers, dropping data, dynamic throttling, or expanding buffer capacity
- Critical for preventing memory overflow and system instability

**3. Data Pipeline Design**
- Chaining multiple producer-consumer stages
- Each stage processes data and passes it to the next
- Enables parallel processing and modular architecture
- Helps distribute workload across multiple tasks

---

## C/C++ Implementation Examples

### Example 1: Basic Producer-Consumer with Single Queue

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

#define QUEUE_LENGTH 10
#define ITEM_SIZE sizeof(uint32_t)

// Shared queue handle
QueueHandle_t xDataQueue;

// Producer task - generates data
void vProducerTask(void *pvParameters)
{
    uint32_t ulValueToSend = 0;
    BaseType_t xStatus;
    
    for(;;)
    {
        // Generate data (simulated sensor reading)
        ulValueToSend++;
        
        // Send data to queue with 100ms timeout
        xStatus = xQueueSend(xDataQueue, &ulValueToSend, pdMS_TO_TICKS(100));
        
        if(xStatus != pdPASS)
        {
            printf("Producer: Queue full, data lost: %lu\n", ulValueToSend);
        }
        else
        {
            printf("Producer: Sent %lu\n", ulValueToSend);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200)); // Produce every 200ms
    }
}

// Consumer task - processes data
void vConsumerTask(void *pvParameters)
{
    uint32_t ulReceivedValue;
    BaseType_t xStatus;
    
    for(;;)
    {
        // Wait for data with infinite timeout
        xStatus = xQueueReceive(xDataQueue, &ulReceivedValue, portMAX_DELAY);
        
        if(xStatus == pdPASS)
        {
            printf("Consumer: Processing %lu\n", ulReceivedValue);
            
            // Simulate processing time
            vTaskDelay(pdMS_TO_TICKS(300));
            
            printf("Consumer: Finished %lu\n", ulReceivedValue);
        }
    }
}

int main(void)
{
    // Create queue
    xDataQueue = xQueueCreate(QUEUE_LENGTH, ITEM_SIZE);
    
    if(xDataQueue != NULL)
    {
        // Create producer and consumer tasks
        xTaskCreate(vProducerTask, "Producer", 1000, NULL, 2, NULL);
        xTaskCreate(vConsumerTask, "Consumer", 1000, NULL, 2, NULL);
        
        // Start scheduler
        vTaskStartScheduler();
    }
    
    // Should never reach here
    for(;;);
    return 0;
}
```

### Example 2: Multi-Producer Multi-Consumer with Backpressure

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdio.h>

#define QUEUE_LENGTH 5
#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2

typedef struct {
    uint32_t producerId;
    uint32_t dataValue;
    TickType_t timestamp;
} DataItem_t;

QueueHandle_t xDataQueue;
SemaphoreHandle_t xConsoleMutex;

// Producer task with backpressure handling
void vProducerTask(void *pvParameters)
{
    uint32_t producerId = (uint32_t)pvParameters;
    DataItem_t xItem;
    BaseType_t xStatus;
    uint32_t ulDroppedCount = 0;
    uint32_t ulSentCount = 0;
    
    for(;;)
    {
        // Prepare data item
        xItem.producerId = producerId;
        xItem.dataValue = ulSentCount + ulDroppedCount;
        xItem.timestamp = xTaskGetTickCount();
        
        // Try to send with short timeout (backpressure handling)
        xStatus = xQueueSend(xDataQueue, &xItem, pdMS_TO_TICKS(50));
        
        if(xStatus == pdPASS)
        {
            ulSentCount++;
            xSemaphoreTake(xConsoleMutex, portMAX_DELAY);
            printf("Producer %lu: Sent item %lu (Queue: %d/%d)\n", 
                   producerId, xItem.dataValue,
                   uxQueueMessagesWaiting(xDataQueue), QUEUE_LENGTH);
            xSemaphoreGive(xConsoleMutex);
        }
        else
        {
            // Backpressure: Queue full, implement strategy
            ulDroppedCount++;
            xSemaphoreTake(xConsoleMutex, portMAX_DELAY);
            printf("Producer %lu: BACKPRESSURE - Dropped item %lu (Total dropped: %lu)\n",
                   producerId, xItem.dataValue, ulDroppedCount);
            xSemaphoreGive(xConsoleMutex);
            
            // Optional: Back off when experiencing backpressure
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // Variable production rate
        vTaskDelay(pdMS_TO_TICKS(100 + (producerId * 50)));
    }
}

// Consumer task
void vConsumerTask(void *pvParameters)
{
    uint32_t consumerId = (uint32_t)pvParameters;
    DataItem_t xItem;
    BaseType_t xStatus;
    
    for(;;)
    {
        // Wait for data
        xStatus = xQueueReceive(xDataQueue, &xItem, portMAX_DELAY);
        
        if(xStatus == pdPASS)
        {
            TickType_t processingDelay = xTaskGetTickCount() - xItem.timestamp;
            
            xSemaphoreTake(xConsoleMutex, portMAX_DELAY);
            printf("Consumer %lu: Processing from Producer %lu, Value: %lu, Delay: %lums\n",
                   consumerId, xItem.producerId, xItem.dataValue, processingDelay);
            xSemaphoreGive(xConsoleMutex);
            
            // Simulate variable processing time
            vTaskDelay(pdMS_TO_TICKS(200 + (xItem.dataValue % 100)));
        }
    }
}

int main(void)
{
    // Create queue and mutex
    xDataQueue = xQueueCreate(QUEUE_LENGTH, sizeof(DataItem_t));
    xConsoleMutex = xSemaphoreCreateMutex();
    
    if(xDataQueue != NULL && xConsoleMutex != NULL)
    {
        // Create multiple producers
        for(uint32_t i = 0; i < NUM_PRODUCERS; i++)
        {
            char taskName[16];
            snprintf(taskName, sizeof(taskName), "Producer%lu", i);
            xTaskCreate(vProducerTask, taskName, 1000, (void*)i, 2, NULL);
        }
        
        // Create multiple consumers
        for(uint32_t i = 0; i < NUM_CONSUMERS; i++)
        {
            char taskName[16];
            snprintf(taskName, sizeof(taskName), "Consumer%lu", i);
            xTaskCreate(vConsumerTask, taskName, 1000, (void*)i, 2, NULL);
        }
        
        vTaskStartScheduler();
    }
    
    for(;;);
    return 0;
}
```

### Example 3: Data Pipeline with Multiple Stages

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <math.h>

#define QUEUE_LENGTH 8

// Pipeline stages
typedef struct {
    uint32_t rawData;
    uint32_t stage;
} PipelineData_t;

// Queue handles for pipeline stages
QueueHandle_t xRawDataQueue;    // Stage 1 -> Stage 2
QueueHandle_t xProcessedQueue;  // Stage 2 -> Stage 3
QueueHandle_t xResultQueue;     // Stage 3 -> Output

// Stage 1: Data Acquisition
void vAcquisitionTask(void *pvParameters)
{
    PipelineData_t xData;
    uint32_t sensorValue = 0;
    
    for(;;)
    {
        // Simulate sensor reading
        sensorValue = (sensorValue + 17) % 1000;
        
        xData.rawData = sensorValue;
        xData.stage = 1;
        
        if(xQueueSend(xRawDataQueue, &xData, pdMS_TO_TICKS(100)) == pdPASS)
        {
            printf("Stage 1 (Acquisition): Generated %lu\n", sensorValue);
        }
        
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

// Stage 2: Data Filtering
void vFilterTask(void *pvParameters)
{
    PipelineData_t xInput, xOutput;
    uint32_t filterBuffer[3] = {0};
    uint8_t bufferIndex = 0;
    
    for(;;)
    {
        if(xQueueReceive(xRawDataQueue, &xInput, portMAX_DELAY) == pdPASS)
        {
            // Simple moving average filter
            filterBuffer[bufferIndex] = xInput.rawData;
            bufferIndex = (bufferIndex + 1) % 3;
            
            uint32_t average = (filterBuffer[0] + filterBuffer[1] + filterBuffer[2]) / 3;
            
            xOutput.rawData = average;
            xOutput.stage = 2;
            
            printf("Stage 2 (Filter): %lu -> %lu (filtered)\n", 
                   xInput.rawData, average);
            
            xQueueSend(xProcessedQueue, &xOutput, portMAX_DELAY);
        }
    }
}

// Stage 3: Data Analysis
void vAnalysisTask(void *pvParameters)
{
    PipelineData_t xInput, xOutput;
    
    for(;;)
    {
        if(xQueueReceive(xProcessedQueue, &xInput, portMAX_DELAY) == pdPASS)
        {
            // Perform analysis (e.g., threshold detection)
            uint32_t analysisResult = (xInput.rawData > 500) ? 1 : 0;
            
            xOutput.rawData = analysisResult;
            xOutput.stage = 3;
            
            printf("Stage 3 (Analysis): %lu -> Result: %lu\n",
                   xInput.rawData, analysisResult);
            
            xQueueSend(xResultQueue, &xOutput, portMAX_DELAY);
        }
    }
}

// Stage 4: Output/Logging
void vOutputTask(void *pvParameters)
{
    PipelineData_t xResult;
    uint32_t alertCount = 0;
    
    for(;;)
    {
        if(xQueueReceive(xResultQueue, &xResult, portMAX_DELAY) == pdPASS)
        {
            if(xResult.rawData == 1)
            {
                alertCount++;
                printf("Stage 4 (Output): *** ALERT #%lu ***\n", alertCount);
            }
            else
            {
                printf("Stage 4 (Output): Normal operation\n");
            }
            
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

int main(void)
{
    // Create pipeline queues
    xRawDataQueue = xQueueCreate(QUEUE_LENGTH, sizeof(PipelineData_t));
    xProcessedQueue = xQueueCreate(QUEUE_LENGTH, sizeof(PipelineData_t));
    xResultQueue = xQueueCreate(QUEUE_LENGTH, sizeof(PipelineData_t));
    
    if(xRawDataQueue && xProcessedQueue && xResultQueue)
    {
        // Create pipeline tasks
        xTaskCreate(vAcquisitionTask, "Acquisition", 1000, NULL, 2, NULL);
        xTaskCreate(vFilterTask, "Filter", 1000, NULL, 2, NULL);
        xTaskCreate(vAnalysisTask, "Analysis", 1000, NULL, 2, NULL);
        xTaskCreate(vOutputTask, "Output", 1000, NULL, 2, NULL);
        
        vTaskStartScheduler();
    }
    
    for(;;);
    return 0;
}
```

---

## Rust Implementation Examples

### Example 1: Basic Producer-Consumer in Rust with FreeRTOS Bindings

```rust
// Using freertos-rust crate
use freertos_rust::*;
use core::time::Duration;

const QUEUE_SIZE: usize = 10;

// Producer task
fn producer_task(queue: Arc<Queue<u32>>) {
    let mut value: u32 = 0;
    
    loop {
        value = value.wrapping_add(1);
        
        match queue.send(value, Duration::from_millis(100)) {
            Ok(_) => {
                println!("Producer: Sent {}", value);
            }
            Err(_) => {
                println!("Producer: Queue full, dropped {}", value);
            }
        }
        
        CurrentTask::delay(Duration::from_millis(200));
    }
}

// Consumer task
fn consumer_task(queue: Arc<Queue<u32>>) {
    loop {
        match queue.receive(Duration::max_value()) {
            Ok(value) => {
                println!("Consumer: Processing {}", value);
                
                // Simulate processing
                CurrentTask::delay(Duration::from_millis(300));
                
                println!("Consumer: Finished {}", value);
            }
            Err(_) => {
                println!("Consumer: Receive error");
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn main() {
    // Create queue
    let queue = Arc::new(Queue::new(QUEUE_SIZE).unwrap());
    
    // Clone Arc for tasks
    let producer_queue = queue.clone();
    let consumer_queue = queue.clone();
    
    // Create tasks
    Task::new()
        .name("Producer")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || producer_task(producer_queue))
        .unwrap();
    
    Task::new()
        .name("Consumer")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || consumer_task(consumer_queue))
        .unwrap();
    
    FreeRtosUtils::start_scheduler();
}
```

### Example 2: Multi-Producer Multi-Consumer with Backpressure in Rust

```rust
use freertos_rust::*;
use core::time::Duration;
use core::sync::atomic::{AtomicU32, Ordering};

const QUEUE_SIZE: usize = 5;
const NUM_PRODUCERS: usize = 3;
const NUM_CONSUMERS: usize = 2;

#[derive(Clone, Copy)]
struct DataItem {
    producer_id: u32,
    data_value: u32,
    timestamp: Duration,
}

// Shared statistics
struct ProducerStats {
    sent: AtomicU32,
    dropped: AtomicU32,
}

impl ProducerStats {
    fn new() -> Self {
        Self {
            sent: AtomicU32::new(0),
            dropped: AtomicU32::new(0),
        }
    }
}

// Producer task with backpressure handling
fn producer_task(
    producer_id: u32,
    queue: Arc<Queue<DataItem>>,
    stats: Arc<ProducerStats>,
    mutex: Arc<Mutex<()>>,
) {
    let mut counter: u32 = 0;
    
    loop {
        let item = DataItem {
            producer_id,
            data_value: counter,
            timestamp: Duration::from_millis(
                CurrentTask::get_tick_count() as u64
            ),
        };
        
        // Try to send with timeout (backpressure handling)
        match queue.send(item, Duration::from_millis(50)) {
            Ok(_) => {
                stats.sent.fetch_add(1, Ordering::Relaxed);
                
                let _guard = mutex.lock(Duration::max_value());
                println!(
                    "Producer {}: Sent item {} (Queue size: {})",
                    producer_id,
                    item.data_value,
                    queue.messages_waiting()
                );
            }
            Err(_) => {
                // Backpressure: Queue full
                stats.dropped.fetch_add(1, Ordering::Relaxed);
                
                let _guard = mutex.lock(Duration::max_value());
                let dropped = stats.dropped.load(Ordering::Relaxed);
                println!(
                    "Producer {}: BACKPRESSURE - Dropped {} (Total: {})",
                    producer_id, item.data_value, dropped
                );
                
                // Back off during backpressure
                CurrentTask::delay(Duration::from_millis(100));
            }
        }
        
        counter = counter.wrapping_add(1);
        
        // Variable production rate
        CurrentTask::delay(Duration::from_millis(
            (100 + producer_id * 50) as u64
        ));
    }
}

// Consumer task
fn consumer_task(
    consumer_id: u32,
    queue: Arc<Queue<DataItem>>,
    mutex: Arc<Mutex<()>>,
) {
    loop {
        match queue.receive(Duration::max_value()) {
            Ok(item) => {
                let current_tick = CurrentTask::get_tick_count();
                let processing_delay = current_tick - 
                    (item.timestamp.as_millis() as u32);
                
                {
                    let _guard = mutex.lock(Duration::max_value());
                    println!(
                        "Consumer {}: Processing from Producer {}, \
                         Value: {}, Delay: {}ms",
                        consumer_id,
                        item.producer_id,
                        item.data_value,
                        processing_delay
                    );
                }
                
                // Simulate variable processing time
                CurrentTask::delay(Duration::from_millis(
                    (200 + (item.data_value % 100)) as u64
                ));
            }
            Err(_) => {
                println!("Consumer {}: Receive error", consumer_id);
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn main() {
    // Create shared resources
    let queue = Arc::new(Queue::new(QUEUE_SIZE).unwrap());
    let console_mutex = Arc::new(Mutex::new(()).unwrap());
    
    // Create producer statistics
    let mut producer_stats = Vec::new();
    for _ in 0..NUM_PRODUCERS {
        producer_stats.push(Arc::new(ProducerStats::new()));
    }
    
    // Create producer tasks
    for i in 0..NUM_PRODUCERS {
        let queue_clone = queue.clone();
        let mutex_clone = console_mutex.clone();
        let stats_clone = producer_stats[i].clone();
        
        Task::new()
            .name(&format!("Producer{}", i))
            .stack_size(2048)
            .priority(TaskPriority(2))
            .start(move || {
                producer_task(i as u32, queue_clone, stats_clone, mutex_clone)
            })
            .unwrap();
    }
    
    // Create consumer tasks
    for i in 0..NUM_CONSUMERS {
        let queue_clone = queue.clone();
        let mutex_clone = console_mutex.clone();
        
        Task::new()
            .name(&format!("Consumer{}", i))
            .stack_size(2048)
            .priority(TaskPriority(2))
            .start(move || consumer_task(i as u32, queue_clone, mutex_clone))
            .unwrap();
    }
    
    FreeRtosUtils::start_scheduler();
}
```

### Example 3: Type-Safe Pipeline in Rust

```rust
use freertos_rust::*;
use core::time::Duration;

const QUEUE_SIZE: usize = 8;

// Pipeline data types with type safety
#[derive(Clone, Copy)]
struct RawData {
    value: u32,
}

#[derive(Clone, Copy)]
struct FilteredData {
    value: u32,
    filtered_value: u32,
}

#[derive(Clone, Copy)]
struct AnalyzedData {
    original: u32,
    filtered: u32,
    alert: bool,
}

// Stage 1: Data acquisition
fn acquisition_stage(output_queue: Arc<Queue<RawData>>) {
    let mut sensor_value: u32 = 0;
    
    loop {
        sensor_value = (sensor_value + 17) % 1000;
        
        let raw_data = RawData {
            value: sensor_value,
        };
        
        if output_queue.send(raw_data, Duration::from_millis(100)).is_ok() {
            println!("Acquisition: Generated {}", sensor_value);
        }
        
        CurrentTask::delay(Duration::from_millis(150));
    }
}

// Stage 2: Filtering
fn filter_stage(
    input_queue: Arc<Queue<RawData>>,
    output_queue: Arc<Queue<FilteredData>>,
) {
    let mut filter_buffer = [0u32; 3];
    let mut buffer_index = 0usize;
    
    loop {
        if let Ok(raw_data) = input_queue.receive(Duration::max_value()) {
            // Moving average filter
            filter_buffer[buffer_index] = raw_data.value;
            buffer_index = (buffer_index + 1) % 3;
            
            let average = (filter_buffer[0] + filter_buffer[1] + 
                          filter_buffer[2]) / 3;
            
            let filtered_data = FilteredData {
                value: raw_data.value,
                filtered_value: average,
            };
            
            println!(
                "Filter: {} -> {} (filtered)",
                raw_data.value, average
            );
            
            output_queue.send(filtered_data, Duration::max_value()).ok();
        }
    }
}

// Stage 3: Analysis
fn analysis_stage(
    input_queue: Arc<Queue<FilteredData>>,
    output_queue: Arc<Queue<AnalyzedData>>,
) {
    loop {
        if let Ok(filtered_data) = input_queue.receive(Duration::max_value()) {
            let alert = filtered_data.filtered_value > 500;
            
            let analyzed_data = AnalyzedData {
                original: filtered_data.value,
                filtered: filtered_data.filtered_value,
                alert,
            };
            
            println!(
                "Analysis: {} -> Alert: {}",
                filtered_data.filtered_value, alert
            );
            
            output_queue.send(analyzed_data, Duration::max_value()).ok();
        }
    }
}

// Stage 4: Output
fn output_stage(input_queue: Arc<Queue<AnalyzedData>>) {
    let mut alert_count = 0u32;
    
    loop {
        if let Ok(analyzed_data) = input_queue.receive(Duration::max_value()) {
            if analyzed_data.alert {
                alert_count += 1;
                println!("Output: *** ALERT #{} ***", alert_count);
            } else {
                println!("Output: Normal operation");
            }
            
            CurrentTask::delay(Duration::from_millis(50));
        }
    }
}

#[no_mangle]
pub extern "C" fn main() {
    // Create pipeline queues
    let raw_queue = Arc::new(Queue::<RawData>::new(QUEUE_SIZE).unwrap());
    let filtered_queue = Arc::new(Queue::<FilteredData>::new(QUEUE_SIZE).unwrap());
    let analyzed_queue = Arc::new(Queue::<AnalyzedData>::new(QUEUE_SIZE).unwrap());
    
    // Stage 1
    let raw_queue_clone = raw_queue.clone();
    Task::new()
        .name("Acquisition")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || acquisition_stage(raw_queue_clone))
        .unwrap();
    
    // Stage 2
    let raw_queue_clone = raw_queue.clone();
    let filtered_queue_clone = filtered_queue.clone();
    Task::new()
        .name("Filter")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || filter_stage(raw_queue_clone, filtered_queue_clone))
        .unwrap();
    
    // Stage 3
    let filtered_queue_clone = filtered_queue.clone();
    let analyzed_queue_clone = analyzed_queue.clone();
    Task::new()
        .name("Analysis")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || analysis_stage(filtered_queue_clone, analyzed_queue_clone))
        .unwrap();
    
    // Stage 4
    let analyzed_queue_clone = analyzed_queue.clone();
    Task::new()
        .name("Output")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || output_stage(analyzed_queue_clone))
        .unwrap();
    
    FreeRtosUtils::start_scheduler();
}
```

---

## Summary

**Producer-Consumer Patterns** in FreeRTOS are essential for building robust, scalable embedded systems. Key takeaways:

### Core Principles
- **Decoupling**: Producers and consumers operate independently, communicating only through queues
- **Thread Safety**: FreeRTOS queues provide built-in synchronization without explicit locking
- **Flexibility**: Multiple producers and consumers can share the same queue simultaneously

### Backpressure Strategies
1. **Blocking**: Producers wait until space is available (can cause delays)
2. **Dropping**: Discard data when queue is full (data loss but system remains responsive)
3. **Throttling**: Reduce production rate dynamically based on queue occupancy
4. **Buffering**: Use larger queues or ring buffers for burst handling

### Pipeline Design Benefits
- **Parallelism**: Multiple stages execute concurrently on different cores/tasks
- **Modularity**: Each stage has a single, well-defined responsibility
- **Scalability**: Easy to add/remove stages or adjust processing capacity
- **Load Distribution**: Workload spreads across multiple tasks

### Best Practices
- Choose appropriate queue sizes based on data rates and processing times
- Monitor queue occupancy to detect bottlenecks early
- Implement proper error handling for queue operations
- Use priority-based task scheduling when timing is critical
- Consider using stream buffers for high-throughput byte streams
- Profile your system to identify optimal buffer sizes and task priorities

### Language Considerations
- **C/C++**: Direct access to FreeRTOS APIs, maximum performance, manual memory management
- **Rust**: Type safety, zero-cost abstractions, compile-time guarantees against data races, modern ergonomics through crates like `freertos-rust`

This pattern is fundamental for sensor data processing, communication protocols, logging systems, and any scenario requiring asynchronous data flow in embedded systems.