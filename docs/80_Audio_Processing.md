# FreeRTOS Audio Processing: Real-Time Audio with I2S, DMA, and Task Pipelines

## Overview

Audio processing in FreeRTOS involves handling time-critical audio data streams, typically using I2S (Inter-IC Sound) interfaces, Direct Memory Access (DMA) for efficient data transfer, and task-based pipelines for processing. The real-time nature of FreeRTOS makes it ideal for audio applications where timing and latency are critical.

## Key Concepts

### 1. I2S Interface
I2S is a serial bus interface standard for connecting digital audio devices. It consists of:
- **SCK (Serial Clock)**: Bit clock
- **WS (Word Select)**: Left/right channel indicator
- **SD (Serial Data)**: Audio data line

### 2. DMA for Audio Buffers
DMA enables peripheral-to-memory transfers without CPU intervention, essential for:
- Minimizing CPU overhead
- Reducing jitter and latency
- Maintaining real-time performance

### 3. Double/Circular Buffering
Prevents buffer overruns/underruns by using multiple buffers that alternate between filling and processing.

---

## C/C++ Implementation

### Basic I2S Audio Setup with DMA

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// Audio configuration
#define SAMPLE_RATE         48000
#define BITS_PER_SAMPLE     16
#define CHANNELS            2
#define DMA_BUFFER_SIZE     512
#define NUM_BUFFERS         2

// Audio buffer structure
typedef struct {
    int16_t data[DMA_BUFFER_SIZE];
    uint16_t size;
    uint32_t timestamp;
} AudioBuffer_t;

// Global handles
static QueueHandle_t xAudioQueue;
static SemaphoreHandle_t xDMASemaphore;
static AudioBuffer_t audioBuffers[NUM_BUFFERS];
static volatile uint8_t currentBuffer = 0;

// I2S configuration structure (hardware specific)
typedef struct {
    uint32_t sampleRate;
    uint8_t bitsPerSample;
    uint8_t channels;
    void (*dmaCallback)(void);
} I2S_Config_t;

// Initialize I2S with DMA
void I2S_Init(I2S_Config_t *config) {
    // Hardware-specific initialization
    // Configure I2S peripheral
    // Configure DMA for circular buffer mode
    // Enable interrupts
    
    // Example pseudo-code:
    // I2S->CTRL = I2S_ENABLE | I2S_MASTER_MODE;
    // I2S->SAMPLE_RATE = config->sampleRate;
    // DMA->SRC = (uint32_t)&I2S->DATA;
    // DMA->DST = (uint32_t)audioBuffers[0].data;
    // DMA->COUNT = DMA_BUFFER_SIZE;
    // DMA->CTRL = DMA_ENABLE | DMA_CIRCULAR | DMA_HALF_COMPLETE_INT;
}

// DMA Half Complete Callback
void DMA_HalfComplete_Callback(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Signal that first half of buffer is ready
    xSemaphoreGiveFromISR(xDMASemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// DMA Complete Callback
void DMA_Complete_Callback(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Signal that second half of buffer is ready
    xSemaphoreGiveFromISR(xDMASemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Audio Input Task
void vAudioInputTask(void *pvParameters) {
    AudioBuffer_t buffer;
    
    while (1) {
        // Wait for DMA to fill buffer
        if (xSemaphoreTake(xDMASemaphore, portMAX_DELAY) == pdTRUE) {
            // Copy DMA buffer to processing buffer
            memcpy(buffer.data, 
                   audioBuffers[currentBuffer].data, 
                   DMA_BUFFER_SIZE * sizeof(int16_t));
            buffer.size = DMA_BUFFER_SIZE;
            buffer.timestamp = xTaskGetTickCount();
            
            // Send to processing queue
            if (xQueueSend(xAudioQueue, &buffer, 0) != pdTRUE) {
                // Buffer overrun - log error
            }
            
            // Switch buffer
            currentBuffer = (currentBuffer + 1) % NUM_BUFFERS;
        }
    }
}
```

### Audio Processing Pipeline

```c
// Processing stages
#define STAGE_INPUT      0
#define STAGE_FILTER     1
#define STAGE_EFFECT     2
#define STAGE_OUTPUT     3

// Queue handles for pipeline
static QueueHandle_t xInputQueue;
static QueueHandle_t xFilterQueue;
static QueueHandle_t xEffectQueue;
static QueueHandle_t xOutputQueue;

// High-pass filter example
void applyHighPassFilter(int16_t *input, int16_t *output, uint16_t size) {
    static int16_t previousInput = 0;
    static int16_t previousOutput = 0;
    
    // Simple RC high-pass filter
    const float RC = 0.05f;
    const float alpha = RC / (RC + 1.0f);
    
    for (uint16_t i = 0; i < size; i++) {
        output[i] = (int16_t)(alpha * (previousOutput + input[i] - previousInput));
        previousInput = input[i];
        previousOutput = output[i];
    }
}

// Filter Task
void vAudioFilterTask(void *pvParameters) {
    AudioBuffer_t inputBuffer, outputBuffer;
    
    while (1) {
        if (xQueueReceive(xInputQueue, &inputBuffer, portMAX_DELAY) == pdTRUE) {
            // Apply filtering
            applyHighPassFilter(inputBuffer.data, 
                              outputBuffer.data, 
                              inputBuffer.size);
            
            outputBuffer.size = inputBuffer.size;
            outputBuffer.timestamp = inputBuffer.timestamp;
            
            // Send to next stage
            xQueueSend(xFilterQueue, &outputBuffer, portMAX_DELAY);
        }
    }
}

// Echo effect example
typedef struct {
    int16_t buffer[SAMPLE_RATE];  // 1 second delay buffer
    uint32_t writeIndex;
    uint32_t readIndex;
    float feedback;
    float mix;
} EchoEffect_t;

static EchoEffect_t echoEffect = {
    .writeIndex = 0,
    .readIndex = 0,
    .feedback = 0.5f,
    .mix = 0.3f
};

void applyEcho(int16_t *input, int16_t *output, uint16_t size, EchoEffect_t *echo) {
    for (uint16_t i = 0; i < size; i++) {
        // Read delayed sample
        int16_t delayed = echo->buffer[echo->readIndex];
        
        // Mix input with delayed signal
        int32_t mixed = input[i] + (int32_t)(delayed * echo->mix);
        
        // Clamp to prevent overflow
        if (mixed > 32767) mixed = 32767;
        if (mixed < -32768) mixed = -32768;
        
        output[i] = (int16_t)mixed;
        
        // Write to delay buffer with feedback
        echo->buffer[echo->writeIndex] = input[i] + (int16_t)(delayed * echo->feedback);
        
        // Advance indices
        echo->writeIndex = (echo->writeIndex + 1) % SAMPLE_RATE;
        echo->readIndex = (echo->readIndex + 1) % SAMPLE_RATE;
    }
}

// Effect Task
void vAudioEffectTask(void *pvParameters) {
    AudioBuffer_t inputBuffer, outputBuffer;
    
    while (1) {
        if (xQueueReceive(xFilterQueue, &inputBuffer, portMAX_DELAY) == pdTRUE) {
            // Apply effect
            applyEcho(inputBuffer.data, 
                     outputBuffer.data, 
                     inputBuffer.size, 
                     &echoEffect);
            
            outputBuffer.size = inputBuffer.size;
            outputBuffer.timestamp = inputBuffer.timestamp;
            
            // Send to output
            xQueueSend(xOutputQueue, &outputBuffer, portMAX_DELAY);
        }
    }
}

// Output Task
void vAudioOutputTask(void *pvParameters) {
    AudioBuffer_t buffer;
    
    while (1) {
        if (xQueueReceive(xOutputQueue, &buffer, portMAX_DELAY) == pdTRUE) {
            // Wait for DMA to be ready
            // Configure DMA for transmission
            // Start I2S output
            
            // Example: Write to I2S TX DMA buffer
            // memcpy(i2s_tx_buffer, buffer.data, buffer.size * sizeof(int16_t));
            // I2S_StartTransmission();
        }
    }
}

// Initialize audio processing system
void AudioProcessing_Init(void) {
    // Create queues
    xInputQueue = xQueueCreate(4, sizeof(AudioBuffer_t));
    xFilterQueue = xQueueCreate(4, sizeof(AudioBuffer_t));
    xEffectQueue = xQueueCreate(4, sizeof(AudioBuffer_t));
    xOutputQueue = xQueueCreate(4, sizeof(AudioBuffer_t));
    
    // Create semaphore for DMA
    xDMASemaphore = xSemaphoreCreateBinary();
    
    // Create tasks with appropriate priorities
    xTaskCreate(vAudioInputTask, "AudioIn", 256, NULL, 4, NULL);
    xTaskCreate(vAudioFilterTask, "Filter", 512, NULL, 3, NULL);
    xTaskCreate(vAudioEffectTask, "Effect", 512, NULL, 3, NULL);
    xTaskCreate(vAudioOutputTask, "AudioOut", 256, NULL, 4, NULL);
    
    // Initialize I2S hardware
    I2S_Config_t i2sConfig = {
        .sampleRate = SAMPLE_RATE,
        .bitsPerSample = BITS_PER_SAMPLE,
        .channels = CHANNELS,
        .dmaCallback = DMA_HalfComplete_Callback
    };
    I2S_Init(&i2sConfig);
}
```

---

## Rust Implementation

### Using `freertos-rust` Crate

```rust
use freertos_rust::*;
use core::sync::atomic::{AtomicBool, Ordering};

// Audio configuration constants
const SAMPLE_RATE: u32 = 48000;
const BUFFER_SIZE: usize = 512;
const NUM_BUFFERS: usize = 2;

// Audio buffer type
#[derive(Clone, Copy)]
struct AudioBuffer {
    data: [i16; BUFFER_SIZE],
    size: usize,
    timestamp: u32,
}

impl Default for AudioBuffer {
    fn default() -> Self {
        AudioBuffer {
            data: [0; BUFFER_SIZE],
            size: 0,
            timestamp: 0,
        }
    }
}

// I2S peripheral wrapper (hardware-specific)
struct I2S {
    // Hardware registers would be mapped here
}

impl I2S {
    fn init(sample_rate: u32) -> Self {
        // Initialize I2S peripheral
        // Configure DMA
        I2S {}
    }
    
    fn start_rx_dma(&mut self, buffer: &mut [i16]) {
        // Start DMA reception
    }
    
    fn start_tx_dma(&mut self, buffer: &[i16]) {
        // Start DMA transmission
    }
}

// Audio input task
fn audio_input_task(
    dma_semaphore: Arc<Semaphore>,
    output_queue: Arc<Queue<AudioBuffer>>,
) {
    let mut buffer = AudioBuffer::default();
    
    loop {
        // Wait for DMA interrupt
        if dma_semaphore.take(Duration::infinite()).is_ok() {
            // Copy DMA buffer data
            // In real implementation, access DMA buffer here
            buffer.size = BUFFER_SIZE;
            buffer.timestamp = FreeRtosUtils::get_tick_count();
            
            // Send to processing queue
            let _ = output_queue.send(buffer, Duration::ms(10));
        }
    }
}

// High-pass filter
struct HighPassFilter {
    previous_input: i16,
    previous_output: i16,
    rc: f32,
}

impl HighPassFilter {
    fn new(cutoff_freq: f32, sample_rate: f32) -> Self {
        let rc = 1.0 / (2.0 * core::f32::consts::PI * cutoff_freq);
        HighPassFilter {
            previous_input: 0,
            previous_output: 0,
            rc,
        }
    }
    
    fn process(&mut self, input: &[i16], output: &mut [i16]) {
        let alpha = self.rc / (self.rc + 1.0 / SAMPLE_RATE as f32);
        
        for i in 0..input.len() {
            let filtered = alpha * (self.previous_output as f32 
                         + input[i] as f32 
                         - self.previous_input as f32);
            
            output[i] = filtered as i16;
            self.previous_input = input[i];
            self.previous_output = output[i];
        }
    }
}

// Audio filter task
fn audio_filter_task(
    input_queue: Arc<Queue<AudioBuffer>>,
    output_queue: Arc<Queue<AudioBuffer>>,
) {
    let mut filter = HighPassFilter::new(80.0, SAMPLE_RATE as f32);
    let mut output_buffer = AudioBuffer::default();
    
    loop {
        if let Ok(input_buffer) = input_queue.receive(Duration::infinite()) {
            // Process audio
            filter.process(&input_buffer.data[..input_buffer.size], 
                         &mut output_buffer.data[..input_buffer.size]);
            
            output_buffer.size = input_buffer.size;
            output_buffer.timestamp = input_buffer.timestamp;
            
            let _ = output_queue.send(output_buffer, Duration::infinite());
        }
    }
}

// Echo effect
struct EchoEffect {
    delay_buffer: [i16; SAMPLE_RATE as usize],
    write_index: usize,
    read_index: usize,
    feedback: f32,
    mix: f32,
}

impl EchoEffect {
    fn new(delay_samples: usize, feedback: f32, mix: f32) -> Self {
        EchoEffect {
            delay_buffer: [0; SAMPLE_RATE as usize],
            write_index: 0,
            read_index: 0,
            feedback,
            mix,
        }
    }
    
    fn process(&mut self, input: &[i16], output: &mut [i16]) {
        for i in 0..input.len() {
            let delayed = self.delay_buffer[self.read_index];
            
            // Mix input with delayed signal
            let mixed = input[i] as i32 + (delayed as f32 * self.mix) as i32;
            output[i] = mixed.clamp(-32768, 32767) as i16;
            
            // Write to delay buffer with feedback
            let feedback_sample = input[i] as f32 + delayed as f32 * self.feedback;
            self.delay_buffer[self.write_index] = feedback_sample.clamp(-32768.0, 32767.0) as i16;
            
            // Advance circular buffer indices
            self.write_index = (self.write_index + 1) % self.delay_buffer.len();
            self.read_index = (self.read_index + 1) % self.delay_buffer.len();
        }
    }
}

// Audio effect task
fn audio_effect_task(
    input_queue: Arc<Queue<AudioBuffer>>,
    output_queue: Arc<Queue<AudioBuffer>>,
) {
    let mut echo = EchoEffect::new(SAMPLE_RATE as usize / 4, 0.5, 0.3);
    let mut output_buffer = AudioBuffer::default();
    
    loop {
        if let Ok(input_buffer) = input_queue.receive(Duration::infinite()) {
            echo.process(&input_buffer.data[..input_buffer.size],
                        &mut output_buffer.data[..input_buffer.size]);
            
            output_buffer.size = input_buffer.size;
            output_buffer.timestamp = input_buffer.timestamp;
            
            let _ = output_queue.send(output_buffer, Duration::infinite());
        }
    }
}

// Audio output task
fn audio_output_task(input_queue: Arc<Queue<AudioBuffer>>) {
    // Initialize I2S for output
    let mut i2s = I2S::init(SAMPLE_RATE);
    
    loop {
        if let Ok(buffer) = input_queue.receive(Duration::infinite()) {
            // Start DMA transmission
            i2s.start_tx_dma(&buffer.data[..buffer.size]);
            
            // In real implementation, wait for transmission complete
        }
    }
}

// Main initialization
pub fn audio_processing_init() {
    // Create queues
    let input_queue = Arc::new(Queue::<AudioBuffer>::new(4).unwrap());
    let filter_queue = Arc::new(Queue::<AudioBuffer>::new(4).unwrap());
    let effect_queue = Arc::new(Queue::<AudioBuffer>::new(4).unwrap());
    let output_queue = Arc::new(Queue::<AudioBuffer>::new(4).unwrap());
    
    // Create DMA semaphore
    let dma_semaphore = Arc::new(Semaphore::new_binary().unwrap());
    
    // Create tasks
    Task::new()
        .name("AudioIn")
        .stack_size(1024)
        .priority(TaskPriority(4))
        .start({
            let sem = dma_semaphore.clone();
            let queue = input_queue.clone();
            move || audio_input_task(sem, queue)
        })
        .unwrap();
    
    Task::new()
        .name("Filter")
        .stack_size(2048)
        .priority(TaskPriority(3))
        .start({
            let in_q = input_queue.clone();
            let out_q = filter_queue.clone();
            move || audio_filter_task(in_q, out_q)
        })
        .unwrap();
    
    Task::new()
        .name("Effect")
        .stack_size(2048)
        .priority(TaskPriority(3))
        .start({
            let in_q = filter_queue.clone();
            let out_q = effect_queue.clone();
            move || audio_effect_task(in_q, out_q)
        })
        .unwrap();
    
    Task::new()
        .name("AudioOut")
        .stack_size(1024)
        .priority(TaskPriority(4))
        .start({
            let queue = effect_queue.clone();
            move || audio_output_task(queue)
        })
        .unwrap();
}
```

### Advanced: Zero-Copy Audio Processing in Rust

```rust
use core::mem::MaybeUninit;

// Pool-based buffer management for zero-copy
struct BufferPool<T, const N: usize, const SIZE: usize> {
    buffers: [MaybeUninit<T>; N],
    available: [AtomicBool; N],
}

impl<T, const N: usize, const SIZE: usize> BufferPool<T, N, SIZE> 
where
    T: Default + Copy,
{
    fn new() -> Self {
        BufferPool {
            buffers: [MaybeUninit::uninit(); N],
            available: [const { AtomicBool::new(true) }; N],
        }
    }
    
    fn acquire(&self) -> Option<(&mut T, usize)> {
        for (i, available) in self.available.iter().enumerate() {
            if available.compare_exchange(
                true, 
                false, 
                Ordering::Acquire, 
                Ordering::Relaxed
            ).is_ok() {
                unsafe {
                    let ptr = &self.buffers[i] as *const _ as *mut T;
                    return Some((&mut *ptr, i));
                }
            }
        }
        None
    }
    
    fn release(&self, index: usize) {
        self.available[index].store(true, Ordering::Release);
    }
}
```

---

## Summary

**Audio Processing in FreeRTOS** combines hardware interfaces (I2S), efficient data transfer mechanisms (DMA), and task-based concurrent processing to handle real-time audio streams:

### Key Takeaways:

1. **I2S + DMA**: Offloads audio data transfer from CPU, using circular buffers and interrupts for efficient streaming

2. **Double Buffering**: Prevents overruns/underruns by alternating between fill and process operations

3. **Task Pipeline**: Separates concerns (input → filter → effect → output) with appropriate priorities and queue-based communication

4. **Priority Management**: Input/output tasks typically run at higher priority (4) than processing tasks (3) to ensure timing constraints

5. **Buffer Management**: Use queues for inter-task communication; consider pool allocators for zero-copy in Rust

6. **Real-Time Constraints**: 
   - At 48kHz with 512-sample buffers: ~10.7ms processing window
   - Tasks must complete before next buffer arrives
   - Use task priorities and preemption to meet deadlines

7. **Language Considerations**:
   - **C/C++**: Direct hardware access, mature ecosystem, manual memory management
   - **Rust**: Memory safety, zero-cost abstractions, requires FFI for hardware access

This architecture scales to complex audio systems including multi-channel processing, DSP algorithms, and adaptive effects while maintaining deterministic real-time performance.