# CAN Bus Integration with FreeRTOS

## Detailed Description

### Overview of CAN Bus
Controller Area Network (CAN) is a robust vehicle bus standard designed for microcontrollers and devices to communicate without a host computer. It's widely used in automotive, industrial automation, and embedded systems requiring reliable multi-master communication.

**Key CAN Characteristics:**
- **Multi-master**: Any node can initiate transmission
- **Message-based**: Uses identifiers instead of addresses
- **Priority-based arbitration**: Lower IDs have higher priority
- **Error detection**: CRC, frame check, acknowledgment
- **Fault confinement**: Bus-off state for faulty nodes
- **Bit rates**: Standard (up to 1 Mbps), CAN FD (up to 8 Mbps data phase)

### FreeRTOS Integration Challenges

Integrating CAN with FreeRTOS involves:

1. **Interrupt-driven reception**: CAN messages arrive asynchronously
2. **Real-time constraints**: Time-critical messages need prioritization
3. **Buffer management**: Preventing overflow with message queues
4. **Error handling**: Bus-off, error passive states
5. **Task synchronization**: Multiple tasks may send/receive CAN messages

### Message Queuing Strategies

**Strategy 1: Single Queue with Filtering**
- All messages go to one queue
- Tasks filter messages by ID

**Strategy 2: Multiple Queues by Priority**
- High-priority messages to dedicated queue
- Low-priority messages to separate queue

**Strategy 3: Per-Message-ID Queues**
- Each message type has its own queue
- Maximum decoupling, higher memory usage

### CAN Error States and Recovery

CAN controllers have three error states:
- **Error Active**: Normal operation, can send error frames
- **Error Passive**: Elevated error count, limited error signaling
- **Bus-Off**: Critical error count, disconnected from bus

## C/C++ Implementation

### Basic CAN Driver Integration

```c
/* can_freertos.h */
#ifndef CAN_FREERTOS_H
#define CAN_FREERTOS_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// CAN frame structure
typedef struct {
    uint32_t id;           // CAN identifier (11-bit or 29-bit)
    uint8_t  dlc;          // Data length code (0-8)
    uint8_t  data[8];      // Payload
    uint8_t  extended;     // Extended ID flag
    uint8_t  rtr;          // Remote transmission request
    uint32_t timestamp;    // Reception timestamp
} CAN_Frame_t;

// CAN error information
typedef struct {
    uint8_t  tx_error_count;
    uint8_t  rx_error_count;
    uint8_t  error_state;  // 0=Active, 1=Passive, 2=Bus-off
    uint32_t last_error_code;
    uint32_t error_count;
} CAN_ErrorInfo_t;

// CAN configuration
typedef struct {
    uint32_t baudrate;
    uint8_t  rx_queue_size;
    uint8_t  tx_queue_size;
    UBaseType_t rx_task_priority;
    UBaseType_t tx_task_priority;
} CAN_Config_t;

// Function prototypes
BaseType_t CAN_Init(const CAN_Config_t* config);
BaseType_t CAN_SendFrame(const CAN_Frame_t* frame, TickType_t timeout);
BaseType_t CAN_ReceiveFrame(CAN_Frame_t* frame, TickType_t timeout);
void CAN_GetErrorInfo(CAN_ErrorInfo_t* error_info);
BaseType_t CAN_RecoverFromBusOff(void);

#endif
```

### CAN Driver Implementation with FreeRTOS

```c
/* can_freertos.c */
#include "can_freertos.h"
#include "stm32f4xx_hal.h" // Example for STM32

// FreeRTOS objects
static QueueHandle_t can_rx_queue = NULL;
static QueueHandle_t can_tx_queue = NULL;
static SemaphoreHandle_t can_tx_semaphore = NULL;
static TaskHandle_t can_rx_task_handle = NULL;
static TaskHandle_t can_tx_task_handle = NULL;

// CAN hardware handle (example for STM32 HAL)
static CAN_HandleTypeDef hcan1;

// Error tracking
static CAN_ErrorInfo_t error_info = {0};

// Initialize CAN with FreeRTOS integration
BaseType_t CAN_Init(const CAN_Config_t* config) {
    // Create FreeRTOS queues
    can_rx_queue = xQueueCreate(config->rx_queue_size, sizeof(CAN_Frame_t));
    can_tx_queue = xQueueCreate(config->tx_queue_size, sizeof(CAN_Frame_t));
    can_tx_semaphore = xSemaphoreCreateBinary();
    
    if (!can_rx_queue || !can_tx_queue || !can_tx_semaphore) {
        return pdFAIL;
    }
    
    // Initialize CAN hardware (STM32 example)
    hcan1.Instance = CAN1;
    hcan1.Init.Prescaler = 6;  // Configure for desired baudrate
    hcan1.Init.Mode = CAN_MODE_NORMAL;
    hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
    hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
    hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
    hcan1.Init.TimeTriggeredMode = DISABLE;
    hcan1.Init.AutoBusOff = ENABLE;  // Automatic recovery
    hcan1.Init.AutoWakeUp = DISABLE;
    hcan1.Init.AutoRetransmission = ENABLE;
    hcan1.Init.ReceiveFifoLocked = DISABLE;
    hcan1.Init.TransmitFifoPriority = ENABLE;
    
    if (HAL_CAN_Init(&hcan1) != HAL_OK) {
        return pdFAIL;
    }
    
    // Configure CAN filter to accept all messages
    CAN_FilterTypeDef filter_config;
    filter_config.FilterBank = 0;
    filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
    filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
    filter_config.FilterIdHigh = 0x0000;
    filter_config.FilterIdLow = 0x0000;
    filter_config.FilterMaskIdHigh = 0x0000;
    filter_config.FilterMaskIdLow = 0x0000;
    filter_config.FilterFIFOAssignment = CAN_RX_FIFO0;
    filter_config.FilterActivation = ENABLE;
    filter_config.SlaveStartFilterBank = 14;
    
    if (HAL_CAN_ConfigFilter(&hcan1, &filter_config) != HAL_OK) {
        return pdFAIL;
    }
    
    // Start CAN peripheral
    if (HAL_CAN_Start(&hcan1) != HAL_OK) {
        return pdFAIL;
    }
    
    // Enable interrupts
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_TX_MAILBOX_EMPTY);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_ERROR);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_BUSOFF);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_ERROR_PASSIVE);
    
    // Create FreeRTOS tasks
    xTaskCreate(CAN_RxTask, "CAN_RX", 256, NULL, 
                config->rx_task_priority, &can_rx_task_handle);
    xTaskCreate(CAN_TxTask, "CAN_TX", 256, NULL, 
                config->tx_task_priority, &can_tx_task_handle);
    
    return pdPASS;
}

// CAN RX Task - processes received messages
static void CAN_RxTask(void* pvParameters) {
    CAN_Frame_t frame;
    
    while (1) {
        // Wait for message in queue (populated by ISR)
        if (xQueueReceive(can_rx_queue, &frame, portMAX_DELAY) == pdPASS) {
            // Process message based on ID
            switch (frame.id) {
                case 0x100:
                    // Handle specific message type
                    process_sensor_data(&frame);
                    break;
                    
                case 0x200:
                    // Handle control message
                    process_control_message(&frame);
                    break;
                    
                default:
                    // Unknown message
                    break;
            }
        }
    }
}

// CAN TX Task - sends queued messages
static void CAN_TxTask(void* pvParameters) {
    CAN_Frame_t frame;
    CAN_TxHeaderTypeDef tx_header;
    uint32_t tx_mailbox;
    
    while (1) {
        // Wait for message to send
        if (xQueueReceive(can_tx_queue, &frame, portMAX_DELAY) == pdPASS) {
            // Prepare CAN header
            if (frame.extended) {
                tx_header.IDE = CAN_ID_EXT;
                tx_header.ExtId = frame.id;
            } else {
                tx_header.IDE = CAN_ID_STD;
                tx_header.StdId = frame.id;
            }
            
            tx_header.RTR = frame.rtr ? CAN_RTR_REMOTE : CAN_RTR_DATA;
            tx_header.DLC = frame.dlc;
            tx_header.TransmitGlobalTime = DISABLE;
            
            // Send message (non-blocking)
            if (HAL_CAN_AddTxMessage(&hcan1, &tx_header, frame.data, 
                                     &tx_mailbox) == HAL_OK) {
                // Wait for transmission complete
                xSemaphoreTake(can_tx_semaphore, portMAX_DELAY);
            } else {
                // TX mailbox full, retry
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }
}

// Send a CAN frame (called by application)
BaseType_t CAN_SendFrame(const CAN_Frame_t* frame, TickType_t timeout) {
    return xQueueSend(can_tx_queue, frame, timeout);
}

// Receive a CAN frame (called by application)
BaseType_t CAN_ReceiveFrame(CAN_Frame_t* frame, TickType_t timeout) {
    return xQueueReceive(can_rx_queue, frame, timeout);
}

// RX interrupt callback
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    CAN_RxHeaderTypeDef rx_header;
    CAN_Frame_t frame;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, frame.data) == HAL_OK) {
        // Fill frame structure
        frame.id = (rx_header.IDE == CAN_ID_EXT) ? rx_header.ExtId : rx_header.StdId;
        frame.dlc = rx_header.DLC;
        frame.extended = (rx_header.IDE == CAN_ID_EXT);
        frame.rtr = (rx_header.RTR == CAN_RTR_REMOTE);
        frame.timestamp = xTaskGetTickCountFromISR();
        
        // Add to queue from ISR
        xQueueSendFromISR(can_rx_queue, &frame, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// TX complete callback
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef* hcan) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(can_tx_semaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Error callback
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef* hcan) {
    uint32_t error = HAL_CAN_GetError(hcan);
    
    error_info.last_error_code = error;
    error_info.error_count++;
    
    // Handle specific errors
    if (error & HAL_CAN_ERROR_BOF) {
        error_info.error_state = 2; // Bus-off
        // Trigger recovery if needed
    }
}

// Bus-off recovery
BaseType_t CAN_RecoverFromBusOff(void) {
    // Reset CAN peripheral
    HAL_CAN_Stop(&hcan1);
    HAL_CAN_ResetError(&hcan1);
    
    if (HAL_CAN_Start(&hcan1) == HAL_OK) {
        error_info.error_state = 0;
        error_info.tx_error_count = 0;
        error_info.rx_error_count = 0;
        return pdPASS;
    }
    
    return pdFAIL;
}

// Get error information
void CAN_GetErrorInfo(CAN_ErrorInfo_t* info) {
    taskENTER_CRITICAL();
    *info = error_info;
    taskEXIT_CRITICAL();
}
```

### Advanced Message Filtering Example

```c
/* can_filter.c - Message routing with multiple queues */

#define NUM_MESSAGE_TYPES 3

typedef struct {
    uint32_t id_start;
    uint32_t id_end;
    QueueHandle_t queue;
    const char* name;
} CAN_MessageRoute_t;

static CAN_MessageRoute_t message_routes[NUM_MESSAGE_TYPES] = {
    {0x100, 0x1FF, NULL, "Sensors"},    // High priority sensors
    {0x200, 0x2FF, NULL, "Control"},    // Control messages
    {0x300, 0x3FF, NULL, "Diagnostic"}, // Diagnostic data
};

void CAN_InitMessageRouting(void) {
    // Create separate queues for each message type
    message_routes[0].queue = xQueueCreate(20, sizeof(CAN_Frame_t)); // Large for sensors
    message_routes[1].queue = xQueueCreate(10, sizeof(CAN_Frame_t)); // Medium for control
    message_routes[2].queue = xQueueCreate(5, sizeof(CAN_Frame_t));  // Small for diagnostics
}

void CAN_RouteMessage(const CAN_Frame_t* frame) {
    BaseType_t routed = pdFALSE;
    
    for (int i = 0; i < NUM_MESSAGE_TYPES; i++) {
        if (frame->id >= message_routes[i].id_start && 
            frame->id <= message_routes[i].id_end) {
            
            // Try to add to appropriate queue
            if (xQueueSend(message_routes[i].queue, frame, 0) == pdPASS) {
                routed = pdTRUE;
            } else {
                // Queue full - log overflow
                log_queue_overflow(message_routes[i].name);
            }
            break;
        }
    }
    
    if (!routed) {
        // Unknown message ID
        log_unknown_message(frame->id);
    }
}
```

### Application Example

```c
/* main.c - Application using CAN */

void app_main(void) {
    CAN_Config_t can_config = {
        .baudrate = 500000,
        .rx_queue_size = 32,
        .tx_queue_size = 16,
        .rx_task_priority = 3,
        .tx_task_priority = 2
    };
    
    // Initialize CAN
    CAN_Init(&can_config);
    
    // Create application tasks
    xTaskCreate(SensorTask, "Sensor", 256, NULL, 2, NULL);
    xTaskCreate(ControlTask, "Control", 256, NULL, 4, NULL);
    xTaskCreate(DiagnosticTask, "Diag", 256, NULL, 1, NULL);
    
    vTaskStartScheduler();
}

void SensorTask(void* param) {
    CAN_Frame_t frame;
    
    while (1) {
        // Read sensor
        uint16_t temperature = read_temperature_sensor();
        
        // Prepare CAN message
        frame.id = 0x150;
        frame.dlc = 2;
        frame.extended = 0;
        frame.rtr = 0;
        frame.data[0] = (temperature >> 8) & 0xFF;
        frame.data[1] = temperature & 0xFF;
        
        // Send via CAN
        CAN_SendFrame(&frame, pdMS_TO_TICKS(100));
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void ControlTask(void* param) {
    CAN_Frame_t frame;
    CAN_ErrorInfo_t error_info;
    
    while (1) {
        // Receive control messages
        if (CAN_ReceiveFrame(&frame, pdMS_TO_TICKS(1000)) == pdPASS) {
            if (frame.id == 0x200) {
                // Process motor speed command
                uint16_t speed = (frame.data[0] << 8) | frame.data[1];
                set_motor_speed(speed);
            }
        }
        
        // Periodic error monitoring
        CAN_GetErrorInfo(&error_info);
        if (error_info.error_state == 2) {
            // Bus-off detected
            CAN_RecoverFromBusOff();
        }
    }
}
```

## Rust Implementation

### Rust CAN Driver with FreeRTOS Bindings

```rust
// can_freertos.rs
use freertos_rust::{Queue, Semaphore, Task, Duration};
use core::sync::atomic::{AtomicU32, Ordering};

// CAN frame structure
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct CanFrame {
    pub id: u32,
    pub dlc: u8,
    pub data: [u8; 8],
    pub extended: bool,
    pub rtr: bool,
    pub timestamp: u32,
}

impl CanFrame {
    pub fn new(id: u32, data: &[u8]) -> Self {
        let mut frame = CanFrame {
            id,
            dlc: data.len().min(8) as u8,
            data: [0; 8],
            extended: id > 0x7FF,
            rtr: false,
            timestamp: 0,
        };
        frame.data[..frame.dlc as usize].copy_from_slice(&data[..frame.dlc as usize]);
        frame
    }
    
    pub fn new_rtr(id: u32, dlc: u8) -> Self {
        CanFrame {
            id,
            dlc,
            data: [0; 8],
            extended: id > 0x7FF,
            rtr: true,
            timestamp: 0,
        }
    }
}

// Error information
#[derive(Clone, Copy, Debug)]
pub struct CanErrorInfo {
    pub tx_error_count: u8,
    pub rx_error_count: u8,
    pub error_state: CanErrorState,
    pub last_error_code: u32,
    pub total_errors: u32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CanErrorState {
    ErrorActive = 0,
    ErrorPassive = 1,
    BusOff = 2,
}

// CAN Bus Controller
pub struct CanBus {
    rx_queue: Queue<CanFrame>,
    tx_queue: Queue<CanFrame>,
    tx_semaphore: Semaphore,
    error_count: AtomicU32,
    error_state: AtomicU32,
}

impl CanBus {
    pub fn new(rx_queue_size: usize, tx_queue_size: usize) -> Result<Self, &'static str> {
        Ok(CanBus {
            rx_queue: Queue::new(rx_queue_size)
                .ok_or("Failed to create RX queue")?,
            tx_queue: Queue::new(tx_queue_size)
                .ok_or("Failed to create TX queue")?,
            tx_semaphore: Semaphore::new_binary()
                .ok_or("Failed to create semaphore")?,
            error_count: AtomicU32::new(0),
            error_state: AtomicU32::new(CanErrorState::ErrorActive as u32),
        })
    }
    
    pub fn send(&self, frame: CanFrame, timeout: Duration) -> Result<(), &'static str> {
        self.tx_queue.send(frame, timeout)
            .map_err(|_| "TX queue full")
    }
    
    pub fn receive(&self, timeout: Duration) -> Result<CanFrame, &'static str> {
        self.rx_queue.receive(timeout)
            .ok_or("No message received")
    }
    
    pub fn get_error_state(&self) -> CanErrorState {
        match self.error_state.load(Ordering::Relaxed) {
            0 => CanErrorState::ErrorActive,
            1 => CanErrorState::ErrorPassive,
            _ => CanErrorState::BusOff,
        }
    }
    
    pub fn get_error_count(&self) -> u32 {
        self.error_count.load(Ordering::Relaxed)
    }
    
    // Called from interrupt context
    pub fn handle_rx_interrupt(&self, frame: CanFrame) {
        let _ = self.rx_queue.send_from_isr(frame);
    }
    
    pub fn handle_tx_complete_interrupt(&self) {
        let _ = self.tx_semaphore.give_from_isr();
    }
    
    pub fn handle_error_interrupt(&self, error_code: u32) {
        self.error_count.fetch_add(1, Ordering::Relaxed);
        
        // Update error state based on error code
        if error_code & 0x04 != 0 { // Bus-off flag
            self.error_state.store(CanErrorState::BusOff as u32, Ordering::Relaxed);
        }
    }
    
    pub fn recover_from_bus_off(&self) -> Result<(), &'static str> {
        // Hardware-specific recovery code would go here
        unsafe {
            can_hw_reset();
            can_hw_start();
        }
        
        self.error_state.store(CanErrorState::ErrorActive as u32, Ordering::Relaxed);
        self.error_count.store(0, Ordering::Relaxed);
        
        Ok(())
    }
}

// External hardware interface (would be implemented for specific MCU)
extern "C" {
    fn can_hw_init(baudrate: u32) -> bool;
    fn can_hw_send(id: u32, data: *const u8, len: u8, extended: bool) -> bool;
    fn can_hw_reset();
    fn can_hw_start();
}

// CAN RX Task
pub fn can_rx_task(can_bus: &'static CanBus) {
    loop {
        match can_bus.receive(Duration::infinite()) {
            Ok(frame) => {
                // Route message based on ID
                match frame.id {
                    0x100..=0x1FF => handle_sensor_message(frame),
                    0x200..=0x2FF => handle_control_message(frame),
                    0x300..=0x3FF => handle_diagnostic_message(frame),
                    _ => handle_unknown_message(frame),
                }
            }
            Err(_) => {
                // Timeout or error
            }
        }
    }
}

// CAN TX Task
pub fn can_tx_task(can_bus: &'static CanBus) {
    loop {
        match can_bus.tx_queue.receive(Duration::infinite()) {
            Some(frame) => {
                // Send to hardware
                unsafe {
                    if can_hw_send(
                        frame.id,
                        frame.data.as_ptr(),
                        frame.dlc,
                        frame.extended
                    ) {
                        // Wait for transmission complete
                        let _ = can_bus.tx_semaphore.take(Duration::infinite());
                    } else {
                        // TX failed, could retry or log error
                    }
                }
            }
            None => {}
        }
    }
}

// Message handlers
fn handle_sensor_message(frame: CanFrame) {
    match frame.id {
        0x150 => {
            // Temperature sensor
            let temp = u16::from_be_bytes([frame.data[0], frame.data[1]]);
            process_temperature(temp);
        }
        0x151 => {
            // Pressure sensor
            let pressure = u16::from_be_bytes([frame.data[0], frame.data[1]]);
            process_pressure(pressure);
        }
        _ => {}
    }
}

fn handle_control_message(frame: CanFrame) {
    match frame.id {
        0x200 => {
            // Motor speed command
            let speed = u16::from_be_bytes([frame.data[0], frame.data[1]]);
            set_motor_speed(speed);
        }
        _ => {}
    }
}

fn handle_diagnostic_message(frame: CanFrame) {
    // Log diagnostic data
}

fn handle_unknown_message(frame: CanFrame) {
    // Log unknown message
}

// Placeholder functions
fn process_temperature(_temp: u16) {}
fn process_pressure(_pressure: u16) {}
fn set_motor_speed(_speed: u16) {}
```

### Advanced Rust Example with Type-Safe Message Routing

```rust
// can_router.rs - Type-safe message routing
use freertos_rust::{Queue, Duration};
use core::marker::PhantomData;

// Trait for CAN messages
pub trait CanMessage: Sized {
    const ID: u32;
    const DLC: u8;
    
    fn from_frame(frame: &CanFrame) -> Option<Self>;
    fn to_frame(&self) -> CanFrame;
}

// Example message types
#[derive(Debug, Clone, Copy)]
pub struct TemperatureMessage {
    pub value: u16,
    pub sensor_id: u8,
}

impl CanMessage for TemperatureMessage {
    const ID: u32 = 0x150;
    const DLC: u8 = 3;
    
    fn from_frame(frame: &CanFrame) -> Option<Self> {
        if frame.id == Self::ID && frame.dlc >= Self::DLC {
            Some(TemperatureMessage {
                value: u16::from_be_bytes([frame.data[0], frame.data[1]]),
                sensor_id: frame.data[2],
            })
        } else {
            None
        }
    }
    
    fn to_frame(&self) -> CanFrame {
        let mut frame = CanFrame::new(Self::ID, &[]);
        frame.dlc = Self::DLC;
        let temp_bytes = self.value.to_be_bytes();
        frame.data[0] = temp_bytes[0];
        frame.data[1] = temp_bytes[1];
        frame.data[2] = self.sensor_id;
        frame
    }
}

#[derive(Debug, Clone, Copy)]
pub struct MotorSpeedCommand {
    pub speed: u16,
    pub motor_id: u8,
}

impl CanMessage for MotorSpeedCommand {
    const ID: u32 = 0x200;
    const DLC: u8 = 3;
    
    fn from_frame(frame: &CanFrame) -> Option<Self> {
        if frame.id == Self::ID && frame.dlc >= Self::DLC {
            Some(MotorSpeedCommand {
                speed: u16::from_be_bytes([frame.data[0], frame.data[1]]),
                motor_id: frame.data[2],
            })
        } else {
            None
        }
    }
    
    fn to_frame(&self) -> CanFrame {
        let mut frame = CanFrame::new(Self::ID, &[]);
        frame.dlc = Self::DLC;
        let speed_bytes = self.speed.to_be_bytes();
        frame.data[0] = speed_bytes[0];
        frame.data[1] = speed_bytes[1];
        frame.data[2] = self.motor_id;
        frame
    }
}

// Type-safe message queue wrapper
pub struct TypedCanQueue<T: CanMessage> {
    queue: Queue<T>,
    _phantom: PhantomData<T>,
}

impl<T: CanMessage> TypedCanQueue<T> {
    pub fn new(size: usize) -> Option<Self> {
        Queue::new(size).map(|queue| TypedCanQueue {
            queue,
            _phantom: PhantomData,
        })
    }
    
    pub fn send(&self, msg: T, timeout: Duration) -> Result<(), &'static str> {
        self.queue.send(msg, timeout).map_err(|_| "Queue full")
    }
    
    pub fn receive(&self, timeout: Duration) -> Option<T> {
        self.queue.receive(timeout)
    }
}

// Message router
pub struct CanRouter {
    temperature_queue: TypedCanQueue<TemperatureMessage>,
    motor_queue: TypedCanQueue<MotorSpeedCommand>,
}

impl CanRouter {
    pub fn new() -> Option<Self> {
        Some(CanRouter {
            temperature_queue: TypedCanQueue::new(20)?,
            motor_queue: TypedCanQueue::new(10)?,
        })
    }
    
    pub fn route_frame(&self, frame: CanFrame) {
        match frame.id {
            TemperatureMessage::ID => {
                if let Some(msg) = TemperatureMessage::from_frame(&frame) {
                    let _ = self.temperature_queue.send(msg, Duration::ms(0));
                }
            }
            MotorSpeedCommand::ID => {
                if let Some(msg) = MotorSpeedCommand::from_frame(&frame) {
                    let _ = self.motor_queue.send(msg, Duration::ms(0));
                }
            }
            _ => {}
        }
    }
    
    pub fn get_temperature_queue(&self) -> &TypedCanQueue<TemperatureMessage> {
        &self.temperature_queue
    }
    
    pub fn get_motor_queue(&self) -> &TypedCanQueue<MotorSpeedCommand> {
        &self.motor_queue
    }
}
```

### Complete Rust Application Example

```rust
// main.rs
#![no_std]
#![no_main]

use freertos_rust::{Task, Duration};

static CAN_BUS: Option<CanBus> = None;
static ROUTER: Option<CanRouter> = None;

#[no_mangle]
pub extern "C" fn main() -> ! {
    // Initialize CAN bus
    let can_bus = CanBus::new(32, 16).expect("Failed to create CAN bus");
    let can_bus: &'static CanBus = Box::leak(Box::new(can_bus));
    
    // Initialize hardware
    unsafe {
        can_hw_init(500_000);
    }
    
    // Create router
    let router = CanRouter::new().expect("Failed to create router");
    let router: &'static CanRouter = Box::leak(Box::new(router));
    
    // Create tasks
    Task::new()
        .name("CAN_RX")
        .stack_size(512)
        .priority(3)
        .start(move || can_rx_task(can_bus))
        .expect("Failed to create RX task");
    
    Task::new()
        .name("CAN_TX")
        .stack_size(512)
        .priority(2)
        .start(move || can_tx_task(can_bus))
        .expect("Failed to create TX task");
    
    Task::new()
        .name("Sensor")
        .stack_size(512)
        .priority(2)
        .start(move || sensor_task(can_bus, router))
        .expect("Failed to create sensor task");
    
    Task::new()
        .name("Control")
        .stack_size(512)
        .priority(4)
        .start(move || control_task(router))
        .expect("Failed to create control task");
    
    freertos_rust::start_scheduler();
}

fn sensor_task(can_bus: &'static CanBus, _router: &'static CanRouter) {
    loop {
        // Read temperature sensor
        let temp = read_temperature() as u16;
        
        let msg = TemperatureMessage {
            value: temp,
            sensor_id: 1,
        };
        
        let _ = can_bus.send(msg.to_frame(), Duration::ms(100));
        
        Task::delay(Duration::ms(100));
    }
}

fn control_task(router: &'static CanRouter) {
    let motor_queue = router.get_motor_queue();
    
    loop {
        if let Some(cmd) = motor_queue.receive(Duration::ms(1000)) {
            // Set motor speed
            set_motor_speed_hw(cmd.motor_id, cmd.speed);
        }
        
        // Check for bus-off and recover if needed
        if CAN_BUS.as_ref().unwrap().get_error_state() == CanErrorState::BusOff {
            let _ = CAN_BUS.as_ref().unwrap().recover_from_bus_off();
        }
    }
}

// Hardware interface stubs
fn read_temperature() -> f32 { 25.0 }
fn set_motor_speed_hw(_id: u8, _speed: u16) {}
```

## Summary

**CAN Bus Integration with FreeRTOS** enables reliable multi-master communication in embedded systems through:

### Key Concepts:
1. **Interrupt-Driven Architecture**: CAN RX/TX interrupts populate FreeRTOS queues for task processing
2. **Message Queuing Strategies**: Single queue with filtering, priority-based queues, or per-ID queues
3. **Error Handling**: State machine (Error Active → Error Passive → Bus-Off) with automatic recovery
4. **Task Synchronization**: Semaphores coordinate TX completion, queues decouple reception from processing

### Implementation Patterns:
- **C/C++**: HAL-based driver with ISR-to-task communication via queues and semaphores
- **Rust**: Type-safe message routing with zero-cost abstractions and compile-time ID validation
- **Best Practices**: 
  - Separate RX/TX tasks for priority management
  - Hardware filters reduce interrupt load
  - Bounded queues prevent memory exhaustion
  - Periodic error monitoring enables proactive recovery

### Critical Considerations:
- **Real-time constraints**: High-priority CAN tasks (3-4) ensure timely message processing
- **Bus-off recovery**: Automatic or manual reset after critical error thresholds
- **Buffer sizing**: Balance memory usage vs. message burst handling
- **Filter configuration**: Hardware acceptance filters minimize CPU load

This architecture provides deterministic CAN communication suitable for automotive (J1939, CANopen), industrial (DeviceNet), and aerospace (CANaerospace) applications.