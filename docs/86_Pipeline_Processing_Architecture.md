# FreeRTOS Pipeline Processing Architecture

## Overview

Pipeline processing architecture in FreeRTOS is a design pattern that decomposes complex data processing into multiple sequential stages, where each stage is handled by a dedicated task. Data flows through the pipeline from stage to stage, with each stage performing a specific transformation or operation. This architecture is particularly effective for continuous data stream processing in embedded systems.

## Core Concepts

### Multi-Stage Processing Pipeline

A pipeline consists of:
- **Input Stage**: Receives raw data from sensors, communication interfaces, or other sources
- **Processing Stages**: Transform, filter, analyze, or enhance the data
- **Output Stage**: Delivers processed data to actuators, displays, or storage

Each stage operates concurrently, allowing for parallel processing and improved throughput.

### Key Design Considerations

1. **Inter-Stage Communication**: Queues, stream buffers, or direct task notifications
2. **Backpressure Handling**: Managing flow when stages operate at different speeds
3. **Priority Assignment**: Balancing responsiveness with throughput
4. **Buffer Sizing**: Optimizing memory usage while preventing bottlenecks

---

## C/C++ Implementation Examples

### Example 1: Basic Three-Stage Image Processing Pipeline

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#define PIPELINE_QUEUE_LENGTH 5
#define IMAGE_BUFFER_SIZE 640*480

// Pipeline data structure
typedef struct {
    uint8_t data[IMAGE_BUFFER_SIZE];
    uint32_t timestamp;
    uint16_t sequence_number;
} ImageFrame_t;

// Queue handles for inter-stage communication
QueueHandle_t xAcquisitionQueue;
QueueHandle_t xProcessingQueue;
QueueHandle_t xOutputQueue;

// Stage 1: Image Acquisition
void vImageAcquisitionTask(void *pvParameters)
{
    ImageFrame_t frame;
    uint16_t sequence = 0;
    
    for(;;)
    {
        // Simulate image capture from camera
        // In real implementation, this would interface with camera hardware
        vTaskDelay(pdMS_TO_TICKS(33)); // ~30 FPS
        
        frame.timestamp = xTaskGetTickCount();
        frame.sequence_number = sequence++;
        
        // Simulate image data capture
        for(int i = 0; i < IMAGE_BUFFER_SIZE; i++) {
            frame.data[i] = (uint8_t)(i & 0xFF);
        }
        
        // Send to next stage
        if(xQueueSend(xAcquisitionQueue, &frame, pdMS_TO_TICKS(10)) != pdPASS) {
            // Handle queue full - possibly drop frame or signal error
            printf("Acquisition queue full - frame dropped\n");
        }
    }
}

// Stage 2: Image Processing (Edge Detection)
void vImageProcessingTask(void *pvParameters)
{
    ImageFrame_t inputFrame, outputFrame;
    
    for(;;)
    {
        // Wait for data from previous stage
        if(xQueueReceive(xAcquisitionQueue, &inputFrame, portMAX_DELAY) == pdPASS)
        {
            outputFrame.timestamp = inputFrame.timestamp;
            outputFrame.sequence_number = inputFrame.sequence_number;
            
            // Perform edge detection (simplified Sobel operator)
            for(int i = 1; i < IMAGE_BUFFER_SIZE - 1; i++) {
                int gx = inputFrame.data[i+1] - inputFrame.data[i-1];
                int gy = inputFrame.data[i+640] - inputFrame.data[i-640];
                outputFrame.data[i] = (uint8_t)sqrt(gx*gx + gy*gy);
            }
            
            // Send to next stage
            if(xQueueSend(xProcessingQueue, &outputFrame, pdMS_TO_TICKS(10)) != pdPASS) {
                printf("Processing queue full - frame dropped\n");
            }
        }
    }
}

// Stage 3: Output/Display
void vImageOutputTask(void *pvParameters)
{
    ImageFrame_t frame;
    
    for(;;)
    {
        if(xQueueReceive(xProcessingQueue, &frame, portMAX_DELAY) == pdPASS)
        {
            // Output processed image to display or storage
            // Calculate processing latency
            TickType_t latency = xTaskGetTickCount() - frame.timestamp;
            printf("Frame %d processed with latency: %lu ms\n", 
                   frame.sequence_number, latency);
            
            // Simulate display update
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// Pipeline initialization
void vInitializePipeline(void)
{
    // Create inter-stage queues
    xAcquisitionQueue = xQueueCreate(PIPELINE_QUEUE_LENGTH, sizeof(ImageFrame_t));
    xProcessingQueue = xQueueCreate(PIPELINE_QUEUE_LENGTH, sizeof(ImageFrame_t));
    
    // Create pipeline tasks with appropriate priorities
    xTaskCreate(vImageAcquisitionTask, "Acquisition", 
                configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);
    xTaskCreate(vImageProcessingTask, "Processing", 
                configMINIMAL_STACK_SIZE * 4, NULL, 2, NULL);
    xTaskCreate(vImageOutputTask, "Output", 
                configMINIMAL_STACK_SIZE * 2, NULL, 1, NULL);
}
```

### Example 2: Audio Processing Pipeline with Dynamic Priority Balancing

```c
#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"

#define AUDIO_SAMPLE_RATE 48000
#define FRAME_SIZE 512
#define STREAM_BUFFER_SIZE (FRAME_SIZE * 10)

typedef struct {
    int16_t samples[FRAME_SIZE];
    uint32_t sample_count;
} AudioFrame_t;

StreamBufferHandle_t xInputStreamBuffer;
StreamBufferHandle_t xFilteredStreamBuffer;

// Performance monitoring structure
typedef struct {
    uint32_t frames_processed;
    uint32_t drops;
    TickType_t max_processing_time;
    TickType_t avg_processing_time;
} StageMetrics_t;

StageMetrics_t acquisitionMetrics = {0};
StageMetrics_t filterMetrics = {0};
StageMetrics_t outputMetrics = {0};

// Audio acquisition task
void vAudioAcquisitionTask(void *pvParameters)
{
    AudioFrame_t frame;
    TickType_t start_time;
    
    for(;;)
    {
        start_time = xTaskGetTickCount();
        
        // Simulate ADC read
        for(int i = 0; i < FRAME_SIZE; i++) {
            frame.samples[i] = (int16_t)(rand() % 65536 - 32768);
        }
        frame.sample_count = FRAME_SIZE;
        
        // Send to stream buffer
        size_t bytes_sent = xStreamBufferSend(
            xInputStreamBuffer,
            &frame,
            sizeof(AudioFrame_t),
            pdMS_TO_TICKS(5)
        );
        
        if(bytes_sent == sizeof(AudioFrame_t)) {
            acquisitionMetrics.frames_processed++;
        } else {
            acquisitionMetrics.drops++;
        }
        
        // Update metrics
        TickType_t processing_time = xTaskGetTickCount() - start_time;
        if(processing_time > acquisitionMetrics.max_processing_time) {
            acquisitionMetrics.max_processing_time = processing_time;
        }
        
        // Wait for next sample period
        vTaskDelay(pdMS_TO_TICKS(FRAME_SIZE * 1000 / AUDIO_SAMPLE_RATE));
    }
}

// FIR filter implementation
void applyFIRFilter(int16_t *input, int16_t *output, int length)
{
    // Simple low-pass FIR filter coefficients
    const float coeffs[] = {0.1, 0.2, 0.4, 0.2, 0.1};
    const int numTaps = 5;
    
    for(int i = 0; i < length; i++) {
        float sum = 0.0f;
        for(int j = 0; j < numTaps && (i - j) >= 0; j++) {
            sum += coeffs[j] * input[i - j];
        }
        output[i] = (int16_t)sum;
    }
}

// Audio filtering task
void vAudioFilterTask(void *pvParameters)
{
    AudioFrame_t inputFrame, outputFrame;
    TickType_t start_time;
    
    for(;;)
    {
        start_time = xTaskGetTickCount();
        
        // Receive from input stream
        size_t bytes_received = xStreamBufferReceive(
            xInputStreamBuffer,
            &inputFrame,
            sizeof(AudioFrame_t),
            portMAX_DELAY
        );
        
        if(bytes_received == sizeof(AudioFrame_t))
        {
            // Apply FIR filter
            applyFIRFilter(inputFrame.samples, outputFrame.samples, FRAME_SIZE);
            outputFrame.sample_count = FRAME_SIZE;
            
            // Send to output stream
            size_t bytes_sent = xStreamBufferSend(
                xFilteredStreamBuffer,
                &outputFrame,
                sizeof(AudioFrame_t),
                pdMS_TO_TICKS(5)
            );
            
            if(bytes_sent == sizeof(AudioFrame_t)) {
                filterMetrics.frames_processed++;
            } else {
                filterMetrics.drops++;
            }
            
            // Update metrics
            TickType_t processing_time = xTaskGetTickCount() - start_time;
            if(processing_time > filterMetrics.max_processing_time) {
                filterMetrics.max_processing_time = processing_time;
            }
        }
    }
}

// Audio output task (DAC/I2S)
void vAudioOutputTask(void *pvParameters)
{
    AudioFrame_t frame;
    TickType_t start_time;
    
    for(;;)
    {
        start_time = xTaskGetTickCount();
        
        // Receive filtered audio
        size_t bytes_received = xStreamBufferReceive(
            xFilteredStreamBuffer,
            &frame,
            sizeof(AudioFrame_t),
            portMAX_DELAY
        );
        
        if(bytes_received == sizeof(AudioFrame_t))
        {
            // Output to DAC/I2S
            for(int i = 0; i < FRAME_SIZE; i++) {
                // Simulate DAC write
                volatile int16_t sample = frame.samples[i];
            }
            
            outputMetrics.frames_processed++;
            
            // Update metrics
            TickType_t processing_time = xTaskGetTickCount() - start_time;
            if(processing_time > outputMetrics.max_processing_time) {
                outputMetrics.max_processing_time = processing_time;
            }
        }
    }
}

// Monitoring task for dynamic priority adjustment
void vPipelineMonitorTask(void *pvParameters)
{
    TaskHandle_t hAcqTask, hFilterTask, hOutputTask;
    
    // Get task handles (would need to store these during creation)
    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Check for bottlenecks
        if(filterMetrics.drops > 10) {
            // Filter stage is dropping frames - boost priority
            printf("Boosting filter task priority\n");
            // vTaskPrioritySet(hFilterTask, 4);
        }
        
        if(acquisitionMetrics.max_processing_time > pdMS_TO_TICKS(20)) {
            printf("Warning: Acquisition stage slow\n");
        }
        
        // Print pipeline statistics
        printf("\n=== Pipeline Statistics ===\n");
        printf("Acquisition: %lu frames, %lu drops\n", 
               acquisitionMetrics.frames_processed, acquisitionMetrics.drops);
        printf("Filter: %lu frames, %lu drops\n", 
               filterMetrics.frames_processed, filterMetrics.drops);
        printf("Output: %lu frames\n", outputMetrics.frames_processed);
    }
}

// Initialize audio pipeline
void vInitializeAudioPipeline(void)
{
    xInputStreamBuffer = xStreamBufferCreate(STREAM_BUFFER_SIZE, sizeof(AudioFrame_t));
    xFilteredStreamBuffer = xStreamBufferCreate(STREAM_BUFFER_SIZE, sizeof(AudioFrame_t));
    
    // Priority: Acquisition (highest) > Filter > Output
    xTaskCreate(vAudioAcquisitionTask, "AudioAcq", 2048, NULL, 4, NULL);
    xTaskCreate(vAudioFilterTask, "AudioFilter", 4096, NULL, 3, NULL);
    xTaskCreate(vAudioOutputTask, "AudioOut", 2048, NULL, 2, NULL);
    xTaskCreate(vPipelineMonitorTask, "Monitor", 1024, NULL, 1, NULL);
}
```

### Example 3: Packet Processing Pipeline with Backpressure

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#define MAX_PACKET_SIZE 1500
#define PIPELINE_DEPTH 3

typedef enum {
    PACKET_TYPE_DATA,
    PACKET_TYPE_CONTROL,
    PACKET_TYPE_ERROR
} PacketType_t;

typedef struct {
    PacketType_t type;
    uint16_t length;
    uint8_t data[MAX_PACKET_SIZE];
    uint32_t timestamp;
} Packet_t;

// Pipeline queues with different depths for backpressure handling
QueueHandle_t xRxQueue;
QueueHandle_t xDecryptQueue;
QueueHandle_t xProcessQueue;

SemaphoreHandle_t xBackpressureSemaphore;

// Stage 1: Packet Reception
void vPacketReceptionTask(void *pvParameters)
{
    Packet_t packet;
    UBaseType_t queueSpace;
    
    for(;;)
    {
        // Check available space in queue
        queueSpace = uxQueueSpacesAvailable(xRxQueue);
        
        if(queueSpace < 2) {
            // Apply backpressure - slow down reception
            printf("Backpressure: Slowing reception\n");
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        // Simulate packet reception
        packet.type = PACKET_TYPE_DATA;
        packet.length = (rand() % 1000) + 100;
        packet.timestamp = xTaskGetTickCount();
        
        for(int i = 0; i < packet.length; i++) {
            packet.data[i] = (uint8_t)(rand() & 0xFF);
        }
        
        xQueueSend(xRxQueue, &packet, portMAX_DELAY);
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Simulate reception rate
    }
}

// Stage 2: Decryption
void vDecryptionTask(void *pvParameters)
{
    Packet_t packet;
    
    for(;;)
    {
        if(xQueueReceive(xRxQueue, &packet, portMAX_DELAY) == pdPASS)
        {
            // Simulate decryption (CPU-intensive)
            for(int i = 0; i < packet.length; i++) {
                packet.data[i] ^= 0xAA; // Simple XOR "encryption"
            }
            
            // Simulated processing time
            vTaskDelay(pdMS_TO_TICKS(15));
            
            xQueueSend(xDecryptQueue, &packet, portMAX_DELAY);
        }
    }
}

// Stage 3: Protocol Processing
void vProtocolProcessingTask(void *pvParameters)
{
    Packet_t packet;
    
    for(;;)
    {
        if(xQueueReceive(xDecryptQueue, &packet, portMAX_DELAY) == pdPASS)
        {
            // Process packet based on type
            switch(packet.type) {
                case PACKET_TYPE_DATA:
                    // Handle data packet
                    printf("Processed data packet, latency: %lu ms\n",
                           xTaskGetTickCount() - packet.timestamp);
                    break;
                    
                case PACKET_TYPE_CONTROL:
                    // Handle control packet with higher priority
                    break;
                    
                case PACKET_TYPE_ERROR:
                    // Handle error packet
                    break;
            }
            
            xQueueSend(xProcessQueue, &packet, portMAX_DELAY);
        }
    }
}

// Stage 4: Final Output
void vOutputTask(void *pvParameters)
{
    Packet_t packet;
    
    for(;;)
    {
        if(xQueueReceive(xProcessQueue, &packet, portMAX_DELAY) == pdPASS)
        {
            // Send packet to network or storage
            printf("Output packet size: %d bytes\n", packet.length);
        }
    }
}

void vInitializePacketPipeline(void)
{
    // Create queues with different depths
    xRxQueue = xQueueCreate(5, sizeof(Packet_t));
    xDecryptQueue = xQueueCreate(3, sizeof(Packet_t));
    xProcessQueue = xQueueCreate(3, sizeof(Packet_t));
    
    xBackpressureSemaphore = xSemaphoreCreateBinary();
    
    // Create tasks with priority ordering
    xTaskCreate(vPacketReceptionTask, "PktRx", 2048, NULL, 4, NULL);
    xTaskCreate(vDecryptionTask, "Decrypt", 4096, NULL, 3, NULL);
    xTaskCreate(vProtocolProcessingTask, "Protocol", 2048, NULL, 2, NULL);
    xTaskCreate(vOutputTask, "Output", 2048, NULL, 1, NULL);
}
```

---

## Rust Implementation Examples

### Example 1: FreeRTOS Pipeline in Rust (using freertos-rust crate)

```rust
use freertos_rust::*;
use core::time::Duration;

const QUEUE_SIZE: usize = 5;
const IMAGE_WIDTH: usize = 640;
const IMAGE_HEIGHT: usize = 480;

#[derive(Clone, Copy)]
struct ImageFrame {
    data: [u8; IMAGE_WIDTH * IMAGE_HEIGHT],
    timestamp: u32,
    sequence: u16,
}

impl ImageFrame {
    fn new() -> Self {
        ImageFrame {
            data: [0; IMAGE_WIDTH * IMAGE_HEIGHT],
            timestamp: 0,
            sequence: 0,
        }
    }
}

// Stage 1: Image Acquisition Task
fn image_acquisition_task(queue: Arc<Queue<ImageFrame>>) {
    let mut sequence: u16 = 0;
    
    loop {
        let mut frame = ImageFrame::new();
        frame.timestamp = FreeRtosUtils::get_tick_count();
        frame.sequence = sequence;
        sequence = sequence.wrapping_add(1);
        
        // Simulate image capture
        for i in 0..frame.data.len() {
            frame.data[i] = (i & 0xFF) as u8;
        }
        
        // Send to next stage
        match queue.send(frame, Duration::from_millis(10)) {
            Ok(_) => {},
            Err(_) => {
                println!("Acquisition queue full - frame dropped");
            }
        }
        
        CurrentTask::delay(Duration::from_millis(33)); // ~30 FPS
    }
}

// Stage 2: Image Processing Task
fn image_processing_task(
    input_queue: Arc<Queue<ImageFrame>>,
    output_queue: Arc<Queue<ImageFrame>>,
) {
    loop {
        if let Ok(mut frame) = input_queue.receive(Duration::max()) {
            // Simple edge detection (Sobel-like)
            let mut processed_data = [0u8; IMAGE_WIDTH * IMAGE_HEIGHT];
            
            for y in 1..(IMAGE_HEIGHT - 1) {
                for x in 1..(IMAGE_WIDTH - 1) {
                    let idx = y * IMAGE_WIDTH + x;
                    
                    let gx = frame.data[idx + 1] as i32 - frame.data[idx - 1] as i32;
                    let gy = frame.data[idx + IMAGE_WIDTH] as i32 
                           - frame.data[idx - IMAGE_WIDTH] as i32;
                    
                    let magnitude = ((gx * gx + gy * gy) as f32).sqrt() as u8;
                    processed_data[idx] = magnitude;
                }
            }
            
            frame.data = processed_data;
            
            match output_queue.send(frame, Duration::from_millis(10)) {
                Ok(_) => {},
                Err(_) => {
                    println!("Processing queue full - frame dropped");
                }
            }
        }
    }
}

// Stage 3: Output Task
fn image_output_task(queue: Arc<Queue<ImageFrame>>) {
    loop {
        if let Ok(frame) = queue.receive(Duration::max()) {
            let current_tick = FreeRtosUtils::get_tick_count();
            let latency = current_tick - frame.timestamp;
            
            println!("Frame {} processed with latency: {} ms",
                     frame.sequence, latency);
            
            CurrentTask::delay(Duration::from_millis(10));
        }
    }
}

// Pipeline initialization
pub fn initialize_image_pipeline() {
    let acq_queue = Arc::new(Queue::<ImageFrame>::new(QUEUE_SIZE).unwrap());
    let proc_queue = Arc::new(Queue::<ImageFrame>::new(QUEUE_SIZE).unwrap());
    
    let acq_queue_clone = acq_queue.clone();
    Task::new()
        .name("Acquisition")
        .stack_size(2048)
        .priority(TaskPriority(3))
        .start(move || image_acquisition_task(acq_queue_clone))
        .unwrap();
    
    let acq_queue_clone = acq_queue.clone();
    let proc_queue_clone = proc_queue.clone();
    Task::new()
        .name("Processing")
        .stack_size(4096)
        .priority(TaskPriority(2))
        .start(move || image_processing_task(acq_queue_clone, proc_queue_clone))
        .unwrap();
    
    let proc_queue_clone = proc_queue.clone();
    Task::new()
        .name("Output")
        .stack_size(2048)
        .priority(TaskPriority(1))
        .start(move || image_output_task(proc_queue_clone))
        .unwrap();
}
```

### Example 2: Audio Pipeline with Performance Monitoring

```rust
use freertos_rust::*;
use core::sync::atomic::{AtomicU32, AtomicU64, Ordering};
use core::time::Duration;

const FRAME_SIZE: usize = 512;
const SAMPLE_RATE: u32 = 48000;

#[derive(Clone, Copy)]
struct AudioFrame {
    samples: [i16; FRAME_SIZE],
    sample_count: usize,
}

impl AudioFrame {
    fn new() -> Self {
        AudioFrame {
            samples: [0; FRAME_SIZE],
            sample_count: FRAME_SIZE,
        }
    }
}

// Performance metrics (using atomics for thread-safe access)
struct StageMetrics {
    frames_processed: AtomicU32,
    drops: AtomicU32,
    max_processing_time: AtomicU32,
}

impl StageMetrics {
    fn new() -> Self {
        StageMetrics {
            frames_processed: AtomicU32::new(0),
            drops: AtomicU32::new(0),
            max_processing_time: AtomicU32::new(0),
        }
    }
    
    fn increment_frames(&self) {
        self.frames_processed.fetch_add(1, Ordering::Relaxed);
    }
    
    fn increment_drops(&self) {
        self.drops.fetch_add(1, Ordering::Relaxed);
    }
    
    fn update_max_time(&self, time: u32) {
        let mut current = self.max_processing_time.load(Ordering::Relaxed);
        while time > current {
            match self.max_processing_time.compare_exchange_weak(
                current,
                time,
                Ordering::Relaxed,
                Ordering::Relaxed
            ) {
                Ok(_) => break,
                Err(x) => current = x,
            }
        }
    }
}

static ACQUISITION_METRICS: StageMetrics = StageMetrics {
    frames_processed: AtomicU32::new(0),
    drops: AtomicU32::new(0),
    max_processing_time: AtomicU32::new(0),
};

static FILTER_METRICS: StageMetrics = StageMetrics {
    frames_processed: AtomicU32::new(0),
    drops: AtomicU32::new(0),
    max_processing_time: AtomicU32::new(0),
};

// FIR filter implementation
fn apply_fir_filter(input: &[i16; FRAME_SIZE]) -> [i16; FRAME_SIZE] {
    const COEFFS: [f32; 5] = [0.1, 0.2, 0.4, 0.2, 0.1];
    let mut output = [0i16; FRAME_SIZE];
    
    for i in 0..FRAME_SIZE {
        let mut sum = 0.0f32;
        for j in 0..COEFFS.len() {
            if i >= j {
                sum += COEFFS[j] * input[i - j] as f32;
            }
        }
        output[i] = sum as i16;
    }
    
    output
}

// Audio acquisition task
fn audio_acquisition_task(queue: Arc<Queue<AudioFrame>>) {
    loop {
        let start_time = FreeRtosUtils::get_tick_count();
        let mut frame = AudioFrame::new();
        
        // Simulate ADC acquisition
        for i in 0..FRAME_SIZE {
            frame.samples[i] = ((i as i32 * 100) & 0xFFFF) as i16;
        }
        
        match queue.send(frame, Duration::from_millis(5)) {
            Ok(_) => {
                ACQUISITION_METRICS.increment_frames();
            }
            Err(_) => {
                ACQUISITION_METRICS.increment_drops();
            }
        }
        
        let processing_time = FreeRtosUtils::get_tick_count() - start_time;
        ACQUISITION_METRICS.update_max_time(processing_time);
        
        let delay_ms = (FRAME_SIZE as u32 * 1000) / SAMPLE_RATE;
        CurrentTask::delay(Duration::from_millis(delay_ms as u64));
    }
}

// Audio filter task
fn audio_filter_task(
    input_queue: Arc<Queue<AudioFrame>>,
    output_queue: Arc<Queue<AudioFrame>>,
) {
    loop {
        if let Ok(input_frame) = input_queue.receive(Duration::max()) {
            let start_time = FreeRtosUtils::get_tick_count();
            
            let mut output_frame = AudioFrame::new();
            output_frame.samples = apply_fir_filter(&input_frame.samples);
            output_frame.sample_count = FRAME_SIZE;
            
            match output_queue.send(output_frame, Duration::from_millis(5)) {
                Ok(_) => {
                    FILTER_METRICS.increment_frames();
                }
                Err(_) => {
                    FILTER_METRICS.increment_drops();
                }
            }
            
            let processing_time = FreeRtosUtils::get_tick_count() - start_time;
            FILTER_METRICS.update_max_time(processing_time);
        }
    }
}

// Audio output task
fn audio_output_task(queue: Arc<Queue<AudioFrame>>) {
    loop {
        if let Ok(frame) = queue.receive(Duration::max()) {
            // Simulate DAC output
            for sample in &frame.samples {
                // Hardware DAC write would go here
                let _ = sample;
            }
        }
    }
}

// Monitor task
fn pipeline_monitor_task() {
    loop {
        CurrentTask::delay(Duration::from_secs(1));
        
        let acq_frames = ACQUISITION_METRICS.frames_processed.load(Ordering::Relaxed);
        let acq_drops = ACQUISITION_METRICS.drops.load(Ordering::Relaxed);
        let filter_frames = FILTER_METRICS.frames_processed.load(Ordering::Relaxed);
        let filter_drops = FILTER_METRICS.drops.load(Ordering::Relaxed);
        
        println!("\n=== Pipeline Statistics ===");
        println!("Acquisition: {} frames, {} drops", acq_frames, acq_drops);
        println!("Filter: {} frames, {} drops", filter_frames, filter_drops);
        
        if filter_drops > 10 {
            println!("Warning: Filter stage experiencing drops!");
        }
    }
}

// Initialize audio pipeline
pub fn initialize_audio_pipeline() {
    let input_queue = Arc::new(Queue::<AudioFrame>::new(10).unwrap());
    let filtered_queue = Arc::new(Queue::<AudioFrame>::new(10).unwrap());
    
    let input_queue_clone = input_queue.clone();
    Task::new()
        .name("AudioAcq")
        .stack_size(2048)
        .priority(TaskPriority(4))
        .start(move || audio_acquisition_task(input_queue_clone))
        .unwrap();
    
    let input_queue_clone = input_queue.clone();
    let filtered_queue_clone = filtered_queue.clone();
    Task::new()
        .name("AudioFilter")
        .stack_size(4096)
        .priority(TaskPriority(3))
        .start(move || audio_filter_task(input_queue_clone, filtered_queue_clone))
        .unwrap();
    
    let filtered_queue_clone = filtered_queue.clone();
    Task::new()
        .name("AudioOut")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || audio_output_task(filtered_queue_clone))
        .unwrap();
    
    Task::new()
        .name("Monitor")
        .stack_size(1024)
        .priority(TaskPriority(1))
        .start(|| pipeline_monitor_task())
        .unwrap();
}
```

---

## Priority Balancing Strategies

### 1. **Stage-Based Priority Assignment**

- **Input Stage**: Highest priority (prevents data loss)
- **Processing Stages**: Medium priority (balanced throughput)
- **Output Stage**: Lower priority (can buffer results)

### 2. **Dynamic Priority Adjustment**

```c
void vAdaptivePriorityManager(void *pvParameters)
{
    TaskHandle_t hStage1, hStage2, hStage3;
    UBaseType_t queueDepth1, queueDepth2;
    
    for(;;)
    {
        queueDepth1 = uxQueueMessagesWaiting(xQueue1);
        queueDepth2 = uxQueueMessagesWaiting(xQueue2);
        
        // If stage 2 queue is filling up, boost stage 2 priority
        if(queueDepth2 > QUEUE_LENGTH * 0.75) {
            vTaskPrioritySet(hStage2, 4);
        } else {
            vTaskPrioritySet(hStage2, 2);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 3. **Rate-Based Priority**

Assign priorities based on required processing rates:
- Real-time constraints → Highest priority
- Soft deadlines → Medium priority
- Best-effort processing → Lowest priority

---

## Throughput Optimization Techniques

### 1. **Queue Sizing**

```c
// Calculate optimal queue depth based on processing rates
uint32_t calculate_queue_depth(uint32_t producer_rate_hz, 
                                 uint32_t consumer_rate_hz,
                                 uint32_t max_latency_ms)
{
    if(consumer_rate_hz >= producer_rate_hz) {
        return 2; // Minimal buffering needed
    }
    
    // Buffer size to handle bursts
    uint32_t burst_capacity = (producer_rate_hz - consumer_rate_hz) 
                             * max_latency_ms / 1000;
    return burst_capacity + 2; // +2 for margin
}
```

### 2. **Zero-Copy Optimization**

```c
// Use pointers instead of copying large data structures
typedef struct {
    uint8_t *pData;
    uint32_t length;
    uint32_t timestamp;
} DataDescriptor_t;

void vZeroCopyProducer(void *pvParameters)
{
    DataDescriptor_t descriptor;
    
    for(;;)
    {
        // Allocate from memory pool
        descriptor.pData = pvPortMalloc(LARGE_BUFFER_SIZE);
        
        // Fill data
        // ... process data ...
        
        descriptor.length = LARGE_BUFFER_SIZE;
        descriptor.timestamp = xTaskGetTickCount();
        
        // Send descriptor (pointer), not data
        xQueueSend(xQueue, &descriptor, portMAX_DELAY);
    }
}

void vZeroCopyConsumer(void *pvParameters)
{
    DataDescriptor_t descriptor;
    
    for(;;)
    {
        xQueueReceive(xQueue, &descriptor, portMAX_DELAY);
        
        // Process data in place
        // ... use descriptor.pData ...
        
        // Free when done
        vPortFree(descriptor.pData);
    }
}
```

### 3. **Batch Processing**

```c
#define BATCH_SIZE 10

void vBatchProcessingStage(void *pvParameters)
{
    DataItem_t batch[BATCH_SIZE];
    uint32_t batch_count = 0;
    
    for(;;)
    {
        // Collect items into batch
        while(batch_count < BATCH_SIZE)
        {
            if(xQueueReceive(xInputQueue, &batch[batch_count], 
                            pdMS_TO_TICKS(10)) == pdPASS)
            {
                batch_count++;
            } else {
                break; // Timeout - process partial batch
            }
        }
        
        if(batch_count > 0)
        {
            // Process entire batch at once (more efficient)
            process_batch(batch, batch_count);
            
            // Send results
            for(uint32_t i = 0; i < batch_count; i++) {
                xQueueSend(xOutputQueue, &batch[i], portMAX_DELAY);
            }
            
            batch_count = 0;
        }
    }
}
```

---

## Summary

**Pipeline Processing Architecture** in FreeRTOS is a powerful design pattern for decomposing complex data processing into sequential, concurrent stages. Key takeaways include:

### Benefits
- **Parallelism**: Multiple stages execute simultaneously, improving throughput
- **Modularity**: Each stage is independent and testable
- **Scalability**: Easy to add, remove, or modify stages
- **Resource Efficiency**: Balanced CPU utilization across stages

### Design Principles
1. **Inter-stage Communication**: Use queues or stream buffers for data flow
2. **Priority Assignment**: Input stages typically have highest priority to prevent data loss
3. **Backpressure Handling**: Monitor queue depths and adjust processing rates
4. **Performance Monitoring**: Track metrics (throughput, latency, drops) for optimization

### Optimization Strategies
- **Zero-copy operations** for large data structures
- **Batch processing** to reduce context switching overhead
- **Dynamic priority adjustment** based on queue occupancy
- **Optimal queue sizing** based on producer/consumer rates

### Common Applications
- Image/video processing pipelines
- Audio DSP chains
- Network packet processing
- Sensor data fusion
- Protocol stacks

The pipeline architecture is ideal for continuous data stream processing in embedded systems where predictable latency and high throughput are required. Proper balancing of task priorities and queue depths is essential for maximizing system performance while maintaining real-time responsiveness.