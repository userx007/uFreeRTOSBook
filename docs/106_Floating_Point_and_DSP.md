# Floating Point and DSP in FreeRTOS

## Overview

Floating-point operations and Digital Signal Processing (DSP) in embedded systems running FreeRTOS require careful consideration of hardware support, context switching overhead, and optimization techniques. This topic covers how to properly manage floating-point units (FPUs), integrate DSP libraries, and design efficient signal processing tasks.

## Floating-Point Context Management

### Hardware Context

Modern ARM Cortex-M processors (M4F, M7, M33) include dedicated FPUs with 32 single-precision registers (S0-S31) or 16 double-precision registers (D0-D15). When a task uses floating-point operations, the FPU context must be saved and restored during context switches.

### FreeRTOS Configuration

FreeRTOS provides configuration options to manage FPU context:

**C/C++ Configuration (FreeRTOSConfig.h):**

```c
// Enable lazy stacking - saves FPU context only when necessary
#define configUSE_TASK_FPU_SUPPORT 1

// For ARM Cortex-M4F/M7
#define configENABLE_FPU 1
#define configENABLE_MPU 0
```

### Lazy Context Switching

Lazy stacking defers saving the FPU context until another task attempts to use the FPU, reducing overhead when consecutive tasks don't use floating-point.

**C Example - Task with FPU Usage:**

```c
#include "FreeRTOS.h"
#include "task.h"
#include <math.h>

void vSignalProcessingTask(void *pvParameters)
{
    float sample, filtered;
    float alpha = 0.1f; // Low-pass filter coefficient
    float previous = 0.0f;
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100 Hz
    
    for(;;)
    {
        // Simulate ADC reading
        sample = readADCSample();
        
        // Simple exponential moving average filter
        filtered = alpha * sample + (1.0f - alpha) * previous;
        previous = filtered;
        
        // Process filtered signal
        float magnitude = sqrtf(filtered * filtered);
        
        // Output result
        outputDAC(filtered);
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void setupFPUTask(void)
{
    xTaskCreate(
        vSignalProcessingTask,
        "DSP_Task",
        256, // Stack size
        NULL,
        3,   // Priority
        NULL
    );
}
```

**C++ Example with Class-Based Design:**

```cpp
#include "FreeRTOS.h"
#include "task.h"
#include <cmath>
#include <array>

class DSPProcessor {
private:
    static constexpr size_t BUFFER_SIZE = 64;
    std::array<float, BUFFER_SIZE> buffer;
    size_t writeIndex;
    float sampleRate;
    
public:
    DSPProcessor(float fs) : writeIndex(0), sampleRate(fs) {
        buffer.fill(0.0f);
    }
    
    // FIR filter implementation
    float processFIR(float input, const float* coeffs, size_t numTaps) {
        buffer[writeIndex] = input;
        
        float output = 0.0f;
        size_t idx = writeIndex;
        
        for(size_t i = 0; i < numTaps; i++) {
            output += buffer[idx] * coeffs[i];
            idx = (idx == 0) ? (BUFFER_SIZE - 1) : (idx - 1);
        }
        
        writeIndex = (writeIndex + 1) % BUFFER_SIZE;
        return output;
    }
    
    // Calculate RMS value
    float calculateRMS(size_t samples) {
        float sum = 0.0f;
        for(size_t i = 0; i < samples && i < BUFFER_SIZE; i++) {
            sum += buffer[i] * buffer[i];
        }
        return std::sqrt(sum / samples);
    }
};

extern "C" void vDSPTask(void* pvParameters) {
    DSPProcessor* processor = static_cast<DSPProcessor*>(pvParameters);
    
    // Low-pass FIR coefficients (example)
    static const float firCoeffs[] = {
        0.05f, 0.10f, 0.15f, 0.20f, 0.20f, 0.15f, 0.10f, 0.05f
    };
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    for(;;) {
        float input = getAudioSample();
        float output = processor->processFIR(input, firCoeffs, 8);
        
        sendProcessedSample(output);
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1));
    }
}
```

**Rust Example:**

```rust
#![no_std]

use freertos_rust::*;
use core::f32;

// Exponential Moving Average Filter
struct EMAFilter {
    alpha: f32,
    previous: f32,
}

impl EMAFilter {
    fn new(alpha: f32) -> Self {
        EMAFilter {
            alpha,
            previous: 0.0,
        }
    }
    
    fn process(&mut self, input: f32) -> f32 {
        let output = self.alpha * input + (1.0 - self.alpha) * self.previous;
        self.previous = output;
        output
    }
}

// Biquad IIR Filter
struct BiquadFilter {
    b0: f32, b1: f32, b2: f32,
    a1: f32, a2: f32,
    x1: f32, x2: f32,
    y1: f32, y2: f32,
}

impl BiquadFilter {
    fn new_lowpass(fc: f32, fs: f32, q: f32) -> Self {
        let omega = 2.0 * f32::consts::PI * fc / fs;
        let sin_omega = omega.sin();
        let cos_omega = omega.cos();
        let alpha = sin_omega / (2.0 * q);
        
        let b0 = (1.0 - cos_omega) / 2.0;
        let b1 = 1.0 - cos_omega;
        let b2 = (1.0 - cos_omega) / 2.0;
        let a0 = 1.0 + alpha;
        let a1 = -2.0 * cos_omega;
        let a2 = 1.0 - alpha;
        
        BiquadFilter {
            b0: b0 / a0,
            b1: b1 / a0,
            b2: b2 / a0,
            a1: a1 / a0,
            a2: a2 / a0,
            x1: 0.0,
            x2: 0.0,
            y1: 0.0,
            y2: 0.0,
        }
    }
    
    fn process(&mut self, input: f32) -> f32 {
        let output = self.b0 * input + self.b1 * self.x1 + self.b2 * self.x2
                   - self.a1 * self.y1 - self.a2 * self.y2;
        
        self.x2 = self.x1;
        self.x1 = input;
        self.y2 = self.y1;
        self.y1 = output;
        
        output
    }
}

// DSP Task
fn dsp_task(_: TaskParameters) {
    let mut filter = BiquadFilter::new_lowpass(1000.0, 48000.0, 0.707);
    
    loop {
        // Read from ADC (mock function)
        let input = read_adc_sample();
        
        // Process through filter
        let output = filter.process(input);
        
        // Output to DAC (mock function)
        write_dac_sample(output);
        
        CurrentTask::delay(Duration::ms(1));
    }
}

// Helper functions (would be implemented elsewhere)
fn read_adc_sample() -> f32 { 0.0 }
fn write_dac_sample(_value: f32) {}

pub fn create_dsp_task() {
    Task::new()
        .name("DSP")
        .stack_size(512)
        .priority(TaskPriority(3))
        .start(dsp_task)
        .unwrap();
}
```

## ARM CMSIS-DSP Integration

The CMSIS-DSP library provides optimized signal processing functions for ARM Cortex-M processors, leveraging SIMD instructions and hardware acceleration.

**C Example with CMSIS-DSP:**

```c
#include "FreeRTOS.h"
#include "task.h"
#include "arm_math.h"

#define FFT_SIZE 256
#define SAMPLE_RATE 16000

typedef struct {
    arm_rfft_fast_instance_f32 fftInstance;
    float32_t inputBuffer[FFT_SIZE];
    float32_t outputBuffer[FFT_SIZE];
    float32_t magnitudeBuffer[FFT_SIZE / 2];
    uint16_t bufferIndex;
} FFTProcessor_t;

void initFFTProcessor(FFTProcessor_t *processor)
{
    // Initialize FFT structure
    arm_rfft_fast_init_f32(&processor->fftInstance, FFT_SIZE);
    processor->bufferIndex = 0;
    
    // Clear buffers
    memset(processor->inputBuffer, 0, sizeof(processor->inputBuffer));
    memset(processor->outputBuffer, 0, sizeof(processor->outputBuffer));
}

void vFFTTask(void *pvParameters)
{
    FFTProcessor_t processor;
    initFFTProcessor(&processor);
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(16); // ~62.5 Hz update rate
    
    for(;;)
    {
        // Collect samples
        for(uint16_t i = 0; i < FFT_SIZE; i++)
        {
            processor.inputBuffer[i] = (float32_t)readADC();
            vTaskDelay(pdMS_TO_TICKS(1000 / SAMPLE_RATE));
        }
        
        // Apply windowing (Hanning)
        for(uint16_t i = 0; i < FFT_SIZE; i++)
        {
            float32_t window = 0.5f * (1.0f - arm_cos_f32(
                2.0f * PI * i / (FFT_SIZE - 1)));
            processor.inputBuffer[i] *= window;
        }
        
        // Compute FFT
        arm_rfft_fast_f32(&processor.fftInstance,
                         processor.inputBuffer,
                         processor.outputBuffer,
                         0); // Forward FFT
        
        // Calculate magnitude spectrum
        arm_cmplx_mag_f32(processor.outputBuffer,
                         processor.magnitudeBuffer,
                         FFT_SIZE / 2);
        
        // Find peak frequency
        float32_t maxValue;
        uint32_t maxIndex;
        arm_max_f32(processor.magnitudeBuffer, FFT_SIZE / 2,
                   &maxValue, &maxIndex);
        
        float32_t peakFreq = (float32_t)maxIndex * SAMPLE_RATE / FFT_SIZE;
        
        // Process results
        handleFrequencyPeak(peakFreq, maxValue);
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```

**C++ Advanced CMSIS-DSP Wrapper:**

```cpp
#include "arm_math.h"
#include <array>
#include <functional>

template<size_t N>
class FFTAnalyzer {
private:
    arm_rfft_fast_instance_f32 fftInstance;
    std::array<float32_t, N> inputBuffer;
    std::array<float32_t, N> outputBuffer;
    std::array<float32_t, N/2> magnitudeBuffer;
    
public:
    FFTAnalyzer() {
        arm_rfft_fast_init_f32(&fftInstance, N);
    }
    
    void setInput(const float32_t* data, size_t length) {
        size_t copyLen = (length < N) ? length : N;
        memcpy(inputBuffer.data(), data, copyLen * sizeof(float32_t));
        
        // Zero pad if necessary
        if(copyLen < N) {
            memset(&inputBuffer[copyLen], 0, (N - copyLen) * sizeof(float32_t));
        }
    }
    
    void applyWindow(std::function<float32_t(size_t, size_t)> windowFunc) {
        for(size_t i = 0; i < N; i++) {
            inputBuffer[i] *= windowFunc(i, N);
        }
    }
    
    void computeFFT() {
        arm_rfft_fast_f32(&fftInstance, inputBuffer.data(),
                         outputBuffer.data(), 0);
    }
    
    const std::array<float32_t, N/2>& getMagnitudeSpectrum() {
        arm_cmplx_mag_f32(outputBuffer.data(),
                         magnitudeBuffer.data(), N/2);
        return magnitudeBuffer;
    }
    
    struct PeakInfo {
        size_t index;
        float32_t magnitude;
        float32_t frequency;
    };
    
    PeakInfo findPeak(float32_t sampleRate) {
        float32_t maxVal;
        uint32_t maxIdx;
        arm_max_f32(magnitudeBuffer.data(), N/2, &maxVal, &maxIdx);
        
        return {
            maxIdx,
            maxVal,
            static_cast<float32_t>(maxIdx) * sampleRate / N
        };
    }
};

// Hanning window function
inline float32_t hanningWindow(size_t n, size_t N) {
    return 0.5f * (1.0f - arm_cos_f32(2.0f * PI * n / (N - 1)));
}

extern "C" void vAdvancedFFTTask(void* pvParameters) {
    FFTAnalyzer<256> analyzer;
    constexpr float SAMPLE_RATE = 16000.0f;
    
    float32_t samples[256];
    
    for(;;) {
        // Acquire samples
        acquireSamples(samples, 256);
        
        // Process
        analyzer.setInput(samples, 256);
        analyzer.applyWindow(hanningWindow);
        analyzer.computeFFT();
        
        auto spectrum = analyzer.getMagnitudeSpectrum();
        auto peak = analyzer.findPeak(SAMPLE_RATE);
        
        // Report findings
        reportSpectralAnalysis(peak.frequency, peak.magnitude);
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

**Rust with DSP Operations:**

```rust
use micromath::F32Ext; // For embedded-friendly math

const FFT_SIZE: usize = 128;

struct CircularBuffer<const N: usize> {
    data: [f32; N],
    write_idx: usize,
}

impl<const N: usize> CircularBuffer<N> {
    fn new() -> Self {
        CircularBuffer {
            data: [0.0; N],
            write_idx: 0,
        }
    }
    
    fn push(&mut self, value: f32) {
        self.data[self.write_idx] = value;
        self.write_idx = (self.write_idx + 1) % N;
    }
    
    fn get(&self, index: usize) -> f32 {
        let idx = (self.write_idx + N - 1 - index) % N;
        self.data[idx]
    }
}

// FIR Filter implementation
struct FIRFilter<const TAPS: usize> {
    coefficients: [f32; TAPS],
    buffer: CircularBuffer<TAPS>,
}

impl<const TAPS: usize> FIRFilter<TAPS> {
    fn new(coeffs: [f32; TAPS]) -> Self {
        FIRFilter {
            coefficients: coeffs,
            buffer: CircularBuffer::new(),
        }
    }
    
    fn process(&mut self, input: f32) -> f32 {
        self.buffer.push(input);
        
        let mut output = 0.0;
        for i in 0..TAPS {
            output += self.coefficients[i] * self.buffer.get(i);
        }
        output
    }
}

// Decimator for downsampling
struct Decimator<const TAPS: usize> {
    filter: FIRFilter<TAPS>,
    decimation_factor: usize,
    counter: usize,
}

impl<const TAPS: usize> Decimator<TAPS> {
    fn new(coeffs: [f32; TAPS], factor: usize) -> Self {
        Decimator {
            filter: FIRFilter::new(coeffs),
            decimation_factor: factor,
            counter: 0,
        }
    }
    
    fn process(&mut self, input: f32) -> Option<f32> {
        let filtered = self.filter.process(input);
        self.counter += 1;
        
        if self.counter >= self.decimation_factor {
            self.counter = 0;
            Some(filtered)
        } else {
            None
        }
    }
}

fn audio_processing_task(_: TaskParameters) {
    // Low-pass FIR coefficients for decimation
    let lpf_coeffs: [f32; 16] = [
        0.0018, 0.0089, 0.0244, 0.0489, 0.0791,
        0.1080, 0.1278, 0.1340, 0.1278, 0.1080,
        0.0791, 0.0489, 0.0244, 0.0089, 0.0018, 0.0
    ];
    
    let mut decimator = Decimator::new(lpf_coeffs, 4);
    
    loop {
        let sample = read_audio_sample();
        
        if let Some(downsampled) = decimator.process(sample) {
            process_downsampled_audio(downsampled);
        }
        
        CurrentTask::delay(Duration::us(20)); // 50 kHz sample rate
    }
}
```

## Optimization Techniques

### 1. **Memory Alignment**

```c
// Ensure buffers are aligned for optimal SIMD access
__attribute__((aligned(16))) float32_t audioBuffer[256];

// Or using C11
_Alignas(16) float32_t signalBuffer[512];
```

### 2. **Fixed-Point Arithmetic** (when FPU is limited)

```c
#include "arm_math.h"

// Q15 format (1 sign bit, 15 fractional bits)
q15_t fixedInput[256];
q15_t fixedOutput[256];

// Convert float to Q15
arm_float_to_q15(floatBuffer, fixedInput, 256);

// Perform fixed-point FFT
arm_cfft_q15(&fftInstance_q15, fixedInput, 0, 1);

// Convert back if needed
arm_q15_to_float(fixedOutput, floatBuffer, 256);
```

### 3. **DMA Integration**

```c
void setupDMAForDSP(void)
{
    // Configure DMA to transfer ADC samples to buffer
    // This reduces CPU overhead for data acquisition
    
    DMA_InitTypeDef DMA_InitStruct;
    DMA_InitStruct.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStruct.DMA_MemoryBaseAddr = (uint32_t)audioBuffer;
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStruct.DMA_BufferSize = BUFFER_SIZE;
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;
    
    // Trigger task from DMA complete interrupt
}

void DMA_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if(DMA_GetITStatus(DMA_IT_TC))
    {
        // Notify DSP task that buffer is ready
        xTaskNotifyFromISR(xDSPTaskHandle, 0,
                          eNoAction, &xHigherPriorityTaskWoken);
        DMA_ClearITPendingBit(DMA_IT_TC);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

## Summary

**Key Points:**

1. **FPU Context Management**: Enable `configUSE_TASK_FPU_SUPPORT` for automatic lazy stacking, which saves FPU registers only when necessary, reducing context switch overhead.

2. **CMSIS-DSP Integration**: Leverage ARM's optimized library for FFT, filtering, matrix operations, and statistical functions—significantly faster than generic implementations.

3. **Design Patterns**: Use circular buffers for continuous signal processing, separate acquisition and processing tasks, and employ DMA to reduce CPU load during data transfer.

4. **Optimization**: Align data structures for SIMD operations, consider fixed-point arithmetic for resource-constrained systems, and use hardware accelerators (DMA, DSP extensions) when available.

5. **Task Priorities**: DSP tasks typically require higher priorities to meet real-time constraints, especially for audio processing or control systems with tight timing requirements.

6. **Memory Considerations**: DSP operations often require substantial stack and heap space for buffers and intermediate results—allocate appropriately and monitor usage.

The combination of FreeRTOS's real-time capabilities with hardware FPU support and optimized DSP libraries enables sophisticated signal processing on embedded systems while maintaining deterministic timing and efficient resource utilization.