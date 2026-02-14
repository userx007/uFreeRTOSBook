# Modbus Implementation in FreeRTOS

## Overview

Modbus is a widely-used communication protocol in industrial automation for connecting PLCs, sensors, and other devices. In FreeRTOS, implementing Modbus involves creating dedicated tasks for protocol handling, managing serial/network communication, and implementing robust error handling and timeout mechanisms.

## Modbus Variants

### 1. **Modbus RTU (Remote Terminal Unit)**
- Uses serial communication (RS-232, RS-485)
- Binary encoding
- CRC error checking
- Master-slave architecture

### 2. **Modbus TCP/IP**
- Uses Ethernet TCP/IP
- Same function codes as RTU
- MBAP (Modbus Application Protocol) header
- More reliable due to TCP error handling

## Key Concepts

### Task Architecture
- **Master Task**: Initiates requests and processes responses
- **Slave Task**: Listens for requests and sends responses
- **Timer Task**: Handles timeouts and retransmissions
- **Serial/Network Handler**: Manages low-level communication

### Timeout Mechanisms
- Character timeout (3.5 character times for RTU)
- Response timeout (configurable, typically 1-5 seconds)
- Inter-frame delay

---

## C/C++ Implementation

### Modbus RTU Slave Example

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"

// Modbus RTU Configuration
#define MODBUS_SLAVE_ADDR       1
#define MODBUS_BAUDRATE         9600
#define MODBUS_TIMEOUT_MS       1000
#define MODBUS_CHAR_TIMEOUT_MS  10
#define MAX_FRAME_SIZE          256

// Modbus Function Codes
#define MB_FC_READ_COILS            0x01
#define MB_FC_READ_DISCRETE_INPUTS  0x02
#define MB_FC_READ_HOLDING_REGS     0x03
#define MB_FC_READ_INPUT_REGS       0x04
#define MB_FC_WRITE_SINGLE_COIL     0x05
#define MB_FC_WRITE_SINGLE_REG      0x06
#define MB_FC_WRITE_MULTIPLE_REGS   0x10

// Modbus Exception Codes
#define MB_EX_ILLEGAL_FUNCTION      0x01
#define MB_EX_ILLEGAL_DATA_ADDRESS  0x02
#define MB_EX_ILLEGAL_DATA_VALUE    0x03

// Data structures
typedef struct {
    uint8_t address;
    uint8_t function;
    uint16_t reg_address;
    uint16_t reg_count;
    uint8_t data[MAX_FRAME_SIZE];
    uint16_t data_len;
} ModbusFrame_t;

typedef struct {
    uint16_t holding_registers[100];
    uint16_t input_registers[100];
    uint8_t coils[100];
    uint8_t discrete_inputs[100];
    SemaphoreHandle_t data_mutex;
} ModbusData_t;

// Global variables
static QueueHandle_t xModbusRxQueue;
static TimerHandle_t xCharTimeoutTimer;
static ModbusData_t modbusData;
static uint8_t rxBuffer[MAX_FRAME_SIZE];
static uint16_t rxIndex = 0;

// CRC16 calculation for Modbus RTU
uint16_t ModbusCRC16(uint8_t *buffer, uint16_t length) {
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < length; i++) {
        crc ^= buffer[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// Character timeout callback
void vCharTimeoutCallback(TimerHandle_t xTimer) {
    if (rxIndex > 0) {
        // Frame complete, send to processing task
        ModbusFrame_t frame;
        memcpy(frame.data, rxBuffer, rxIndex);
        frame.data_len = rxIndex;
        
        xQueueSend(xModbusRxQueue, &frame, 0);
        rxIndex = 0;
    }
}

// UART Receive ISR
void UART_RxISR(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t byte = UART_ReadByte();
    
    // Reset character timeout
    xTimerResetFromISR(xCharTimeoutTimer, &xHigherPriorityTaskWoken);
    
    if (rxIndex < MAX_FRAME_SIZE) {
        rxBuffer[rxIndex++] = byte;
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Process Read Holding Registers
void ProcessReadHoldingRegs(ModbusFrame_t *request, uint8_t *response, uint16_t *response_len) {
    uint16_t start_addr = (request->data[2] << 8) | request->data[3];
    uint16_t num_regs = (request->data[4] << 8) | request->data[5];
    
    // Validate request
    if (start_addr + num_regs > 100) {
        // Send exception response
        response[0] = MODBUS_SLAVE_ADDR;
        response[1] = request->function | 0x80;
        response[2] = MB_EX_ILLEGAL_DATA_ADDRESS;
        uint16_t crc = ModbusCRC16(response, 3);
        response[3] = crc & 0xFF;
        response[4] = (crc >> 8) & 0xFF;
        *response_len = 5;
        return;
    }
    
    // Build response
    response[0] = MODBUS_SLAVE_ADDR;
    response[1] = MB_FC_READ_HOLDING_REGS;
    response[2] = num_regs * 2; // Byte count
    
    xSemaphoreTake(modbusData.data_mutex, portMAX_DELAY);
    for (uint16_t i = 0; i < num_regs; i++) {
        uint16_t reg_val = modbusData.holding_registers[start_addr + i];
        response[3 + i * 2] = (reg_val >> 8) & 0xFF;
        response[4 + i * 2] = reg_val & 0xFF;
    }
    xSemaphoreGive(modbusData.data_mutex);
    
    uint16_t crc = ModbusCRC16(response, 3 + num_regs * 2);
    response[3 + num_regs * 2] = crc & 0xFF;
    response[4 + num_regs * 2] = (crc >> 8) & 0xFF;
    *response_len = 5 + num_regs * 2;
}

// Process Write Multiple Registers
void ProcessWriteMultipleRegs(ModbusFrame_t *request, uint8_t *response, uint16_t *response_len) {
    uint16_t start_addr = (request->data[2] << 8) | request->data[3];
    uint16_t num_regs = (request->data[4] << 8) | request->data[5];
    uint8_t byte_count = request->data[6];
    
    if (start_addr + num_regs > 100 || byte_count != num_regs * 2) {
        response[0] = MODBUS_SLAVE_ADDR;
        response[1] = request->function | 0x80;
        response[2] = MB_EX_ILLEGAL_DATA_VALUE;
        uint16_t crc = ModbusCRC16(response, 3);
        response[3] = crc & 0xFF;
        response[4] = (crc >> 8) & 0xFF;
        *response_len = 5;
        return;
    }
    
    // Write registers
    xSemaphoreTake(modbusData.data_mutex, portMAX_DELAY);
    for (uint16_t i = 0; i < num_regs; i++) {
        modbusData.holding_registers[start_addr + i] = 
            (request->data[7 + i * 2] << 8) | request->data[8 + i * 2];
    }
    xSemaphoreGive(modbusData.data_mutex);
    
    // Echo request (standard response)
    memcpy(response, request->data, 6);
    uint16_t crc = ModbusCRC16(response, 6);
    response[6] = crc & 0xFF;
    response[7] = (crc >> 8) & 0xFF;
    *response_len = 8;
}

// Modbus Slave Task
void vModbusSlaveTask(void *pvParameters) {
    ModbusFrame_t rxFrame;
    uint8_t txBuffer[MAX_FRAME_SIZE];
    uint16_t txLength;
    
    while (1) {
        if (xQueueReceive(xModbusRxQueue, &rxFrame, portMAX_DELAY) == pdTRUE) {
            // Validate CRC
            uint16_t received_crc = (rxFrame.data[rxFrame.data_len - 1] << 8) | 
                                    rxFrame.data[rxFrame.data_len - 2];
            uint16_t calculated_crc = ModbusCRC16(rxFrame.data, rxFrame.data_len - 2);
            
            if (received_crc != calculated_crc) {
                continue; // Invalid CRC, ignore frame
            }
            
            // Check if frame is for this slave
            if (rxFrame.data[0] != MODBUS_SLAVE_ADDR) {
                continue;
            }
            
            uint8_t function = rxFrame.data[1];
            
            // Process function
            switch (function) {
                case MB_FC_READ_HOLDING_REGS:
                    ProcessReadHoldingRegs(&rxFrame, txBuffer, &txLength);
                    break;
                    
                case MB_FC_WRITE_MULTIPLE_REGS:
                    ProcessWriteMultipleRegs(&rxFrame, txBuffer, &txLength);
                    break;
                    
                default:
                    // Unsupported function
                    txBuffer[0] = MODBUS_SLAVE_ADDR;
                    txBuffer[1] = function | 0x80;
                    txBuffer[2] = MB_EX_ILLEGAL_FUNCTION;
                    uint16_t crc = ModbusCRC16(txBuffer, 3);
                    txBuffer[3] = crc & 0xFF;
                    txBuffer[4] = (crc >> 8) & 0xFF;
                    txLength = 5;
                    break;
            }
            
            // Send response
            UART_WriteBytes(txBuffer, txLength);
        }
    }
}
```

### Modbus TCP Master Example

```c
#include "lwip/sockets.h"
#include "lwip/netdb.h"

#define MODBUS_TCP_PORT         502
#define MBAP_HEADER_SIZE        7
#define MODBUS_TCP_TIMEOUT_MS   3000

typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
} MBAPHeader_t;

// Modbus TCP Master - Read Holding Registers
int ModbusTCP_ReadHoldingRegisters(const char *slave_ip, uint8_t unit_id,
                                   uint16_t start_addr, uint16_t num_regs,
                                   uint16_t *registers, uint32_t timeout_ms) {
    int sock;
    struct sockaddr_in server_addr;
    uint8_t request[MBAP_HEADER_SIZE + 5];
    uint8_t response[MBAP_HEADER_SIZE + 256];
    static uint16_t transaction_id = 0;
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    // Set timeout
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    // Connect to slave
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MODBUS_TCP_PORT);
    inet_pton(AF_INET, slave_ip, &server_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        return -2;
    }
    
    // Build MBAP header
    transaction_id++;
    request[0] = (transaction_id >> 8) & 0xFF;
    request[1] = transaction_id & 0xFF;
    request[2] = 0; // Protocol ID
    request[3] = 0;
    request[4] = 0; // Length (6 bytes)
    request[5] = 6;
    request[6] = unit_id;
    
    // Build PDU
    request[7] = MB_FC_READ_HOLDING_REGS;
    request[8] = (start_addr >> 8) & 0xFF;
    request[9] = start_addr & 0xFF;
    request[10] = (num_regs >> 8) & 0xFF;
    request[11] = num_regs & 0xFF;
    
    // Send request
    if (send(sock, request, 12, 0) < 0) {
        close(sock);
        return -3;
    }
    
    // Receive response
    int bytes_received = recv(sock, response, sizeof(response), 0);
    close(sock);
    
    if (bytes_received < MBAP_HEADER_SIZE + 3) {
        return -4; // Timeout or incomplete response
    }
    
    // Validate response
    uint16_t resp_transaction_id = (response[0] << 8) | response[1];
    if (resp_transaction_id != transaction_id) {
        return -5; // Transaction ID mismatch
    }
    
    // Check for exception
    if (response[7] & 0x80) {
        return -(100 + response[8]); // Return exception code
    }
    
    // Extract register values
    uint8_t byte_count = response[8];
    for (uint16_t i = 0; i < num_regs; i++) {
        registers[i] = (response[9 + i * 2] << 8) | response[10 + i * 2];
    }
    
    return num_regs; // Success
}

// Modbus TCP Master Task
void vModbusTCPMasterTask(void *pvParameters) {
    uint16_t registers[10];
    int result;
    
    while (1) {
        // Read holding registers from slave
        result = ModbusTCP_ReadHoldingRegisters(
            "192.168.1.100",  // Slave IP
            1,                 // Unit ID
            0,                 // Start address
            10,                // Number of registers
            registers,         // Output buffer
            MODBUS_TCP_TIMEOUT_MS
        );
        
        if (result > 0) {
            // Process received data
            printf("Successfully read %d registers\n", result);
            for (int i = 0; i < result; i++) {
                printf("Register[%d] = %d\n", i, registers[i]);
            }
        } else {
            printf("Modbus error: %d\n", result);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## Rust Implementation

### Modbus RTU in Rust with FreeRTOS Bindings

```rust
#![no_std]
#![no_main]

use freertos_rust::*;
use core::sync::atomic::{AtomicU16, Ordering};

// Modbus constants
const MODBUS_SLAVE_ADDR: u8 = 1;
const MAX_FRAME_SIZE: usize = 256;
const CHAR_TIMEOUT_MS: u32 = 10;

// Function codes
const MB_FC_READ_HOLDING_REGS: u8 = 0x03;
const MB_FC_WRITE_SINGLE_REG: u8 = 0x06;
const MB_FC_WRITE_MULTIPLE_REGS: u8 = 0x10;

// Exception codes
const MB_EX_ILLEGAL_FUNCTION: u8 = 0x01;
const MB_EX_ILLEGAL_DATA_ADDRESS: u8 = 0x02;
const MB_EX_ILLEGAL_DATA_VALUE: u8 = 0x03;

#[derive(Clone, Copy)]
struct ModbusFrame {
    data: [u8; MAX_FRAME_SIZE],
    length: usize,
}

impl ModbusFrame {
    fn new() -> Self {
        ModbusFrame {
            data: [0u8; MAX_FRAME_SIZE],
            length: 0,
        }
    }
}

struct ModbusData {
    holding_registers: [u16; 100],
    input_registers: [u16; 100],
}

impl ModbusData {
    fn new() -> Self {
        ModbusData {
            holding_registers: [0u16; 100],
            input_registers: [0u16; 100],
        }
    }
}

// CRC16 calculation
fn modbus_crc16(buffer: &[u8]) -> u16 {
    let mut crc: u16 = 0xFFFF;
    
    for byte in buffer {
        crc ^= *byte as u16;
        for _ in 0..8 {
            if crc & 0x0001 != 0 {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    crc
}

// Modbus Slave implementation
struct ModbusSlave {
    address: u8,
    data: ModbusData,
    rx_queue: Queue<ModbusFrame>,
}

impl ModbusSlave {
    fn new(address: u8, queue_size: usize) -> Result<Self, FreeRtosError> {
        Ok(ModbusSlave {
            address,
            data: ModbusData::new(),
            rx_queue: Queue::new(queue_size)?,
        })
    }
    
    fn process_read_holding_registers(&self, request: &[u8], response: &mut [u8]) -> usize {
        let start_addr = ((request[2] as u16) << 8) | (request[3] as u16);
        let num_regs = ((request[4] as u16) << 8) | (request[5] as u16);
        
        // Validate address range
        if start_addr as usize + num_regs as usize > self.data.holding_registers.len() {
            return self.build_exception_response(
                MB_FC_READ_HOLDING_REGS,
                MB_EX_ILLEGAL_DATA_ADDRESS,
                response
            );
        }
        
        // Build response
        response[0] = self.address;
        response[1] = MB_FC_READ_HOLDING_REGS;
        response[2] = (num_regs * 2) as u8;
        
        let mut idx = 3;
        for i in 0..num_regs {
            let reg_val = self.data.holding_registers[(start_addr + i) as usize];
            response[idx] = ((reg_val >> 8) & 0xFF) as u8;
            response[idx + 1] = (reg_val & 0xFF) as u8;
            idx += 2;
        }
        
        let crc = modbus_crc16(&response[0..idx]);
        response[idx] = (crc & 0xFF) as u8;
        response[idx + 1] = ((crc >> 8) & 0xFF) as u8;
        
        idx + 2
    }
    
    fn process_write_multiple_registers(&mut self, request: &[u8], response: &mut [u8]) -> usize {
        let start_addr = ((request[2] as u16) << 8) | (request[3] as u16);
        let num_regs = ((request[4] as u16) << 8) | (request[5] as u16);
        let byte_count = request[6];
        
        if byte_count != (num_regs * 2) as u8 ||
           start_addr as usize + num_regs as usize > self.data.holding_registers.len() {
            return self.build_exception_response(
                MB_FC_WRITE_MULTIPLE_REGS,
                MB_EX_ILLEGAL_DATA_VALUE,
                response
            );
        }
        
        // Write registers
        let mut data_idx = 7;
        for i in 0..num_regs {
            let reg_val = ((request[data_idx] as u16) << 8) | (request[data_idx + 1] as u16);
            self.data.holding_registers[(start_addr + i) as usize] = reg_val;
            data_idx += 2;
        }
        
        // Echo request (standard response)
        response[0..6].copy_from_slice(&request[0..6]);
        let crc = modbus_crc16(&response[0..6]);
        response[6] = (crc & 0xFF) as u8;
        response[7] = ((crc >> 8) & 0xFF) as u8;
        
        8
    }
    
    fn build_exception_response(&self, function: u8, exception: u8, response: &mut [u8]) -> usize {
        response[0] = self.address;
        response[1] = function | 0x80;
        response[2] = exception;
        let crc = modbus_crc16(&response[0..3]);
        response[3] = (crc & 0xFF) as u8;
        response[4] = ((crc >> 8) & 0xFF) as u8;
        5
    }
    
    fn process_frame(&mut self, frame: &ModbusFrame) -> Option<Vec<u8>> {
        if frame.length < 4 {
            return None;
        }
        
        // Validate CRC
        let received_crc = ((frame.data[frame.length - 1] as u16) << 8) |
                          (frame.data[frame.length - 2] as u16);
        let calculated_crc = modbus_crc16(&frame.data[0..frame.length - 2]);
        
        if received_crc != calculated_crc || frame.data[0] != self.address {
            return None;
        }
        
        let mut response = vec![0u8; MAX_FRAME_SIZE];
        let function = frame.data[1];
        
        let response_len = match function {
            MB_FC_READ_HOLDING_REGS => {
                self.process_read_holding_registers(&frame.data, &mut response)
            }
            MB_FC_WRITE_MULTIPLE_REGS => {
                self.process_write_multiple_registers(&frame.data, &mut response)
            }
            _ => {
                self.build_exception_response(function, MB_EX_ILLEGAL_FUNCTION, &mut response)
            }
        };
        
        response.truncate(response_len);
        Some(response)
    }
}

// Modbus slave task
fn modbus_slave_task(slave: Arc<Mutex<ModbusSlave>>) {
    Task::new()
        .name("ModbusSlave")
        .stack_size(2048)
        .priority(TaskPriority(3))
        .start(move || {
            loop {
                let frame = {
                    let s = slave.lock().unwrap();
                    s.rx_queue.receive(Duration::infinite()).ok()
                };
                
                if let Some(frame) = frame {
                    let response = {
                        let mut s = slave.lock().unwrap();
                        s.process_frame(&frame)
                    };
                    
                    if let Some(resp_data) = response {
                        // Send response via UART
                        uart_send_bytes(&resp_data);
                    }
                }
            }
        })
        .unwrap();
}

// Modbus TCP Master
struct ModbusTCPMaster {
    transaction_id: AtomicU16,
}

impl ModbusTCPMaster {
    fn new() -> Self {
        ModbusTCPMaster {
            transaction_id: AtomicU16::new(0),
        }
    }
    
    fn read_holding_registers(
        &self,
        slave_ip: &str,
        unit_id: u8,
        start_addr: u16,
        num_regs: u16,
        timeout_ms: u32,
    ) -> Result<Vec<u16>, i32> {
        // Build request
        let tid = self.transaction_id.fetch_add(1, Ordering::SeqCst);
        let mut request = vec![0u8; 12];
        
        // MBAP Header
        request[0] = ((tid >> 8) & 0xFF) as u8;
        request[1] = (tid & 0xFF) as u8;
        request[2] = 0; // Protocol ID
        request[3] = 0;
        request[4] = 0; // Length
        request[5] = 6;
        request[6] = unit_id;
        
        // PDU
        request[7] = MB_FC_READ_HOLDING_REGS;
        request[8] = ((start_addr >> 8) & 0xFF) as u8;
        request[9] = (start_addr & 0xFF) as u8;
        request[10] = ((num_regs >> 8) & 0xFF) as u8;
        request[11] = (num_regs & 0xFF) as u8;
        
        // Send request and receive response via TCP socket
        // (Implementation depends on your TCP stack)
        
        Ok(vec![0u16; num_regs as usize])
    }
}

// Stub functions (hardware-specific)
fn uart_send_bytes(data: &[u8]) {
    // Hardware-specific UART transmission
}
```

---

## Summary

**Modbus Implementation in FreeRTOS** involves creating a robust task-based architecture that handles both RTU (serial) and TCP (Ethernet) variants of the protocol. Key implementation aspects include:

### Core Components:
1. **Task Structure**: Dedicated tasks for master/slave operations, timeout management, and communication handling
2. **Frame Processing**: CRC validation for RTU, MBAP header handling for TCP
3. **Timeout Management**: Character timeouts (3.5 char times) for RTU, configurable response timeouts for both variants
4. **Error Handling**: Exception responses, CRC validation, address range checking

### Best Practices:
- Use queues for inter-task communication
- Implement mutexes for data protection
- Utilize FreeRTOS timers for timeout detection
- Separate protocol logic from hardware abstraction
- Implement proper error recovery mechanisms

### Language Considerations:
- **C/C++**: Direct hardware access, mature libraries, widely used in industrial applications
- **Rust**: Memory safety, zero-cost abstractions, growing embedded ecosystem with crates like `modbus-core`

The implementation ensures reliable industrial communication with proper error handling, making it suitable for real-time control systems, SCADA applications, and industrial IoT devices.