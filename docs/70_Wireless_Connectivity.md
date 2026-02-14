# Wireless Connectivity (WiFi/BLE) with FreeRTOS

## What's Covered:

**Core Topics:**
- Architecture overview with layered approach
- WiFi integration (state machines, event handling, reconnection)
- BLE integration (GAP/GATT layers, peripheral/central roles)
- Connection state management patterns
- Communication patterns (request-response, pub-sub, streaming)

**Code Examples in C/C++:**
1. WiFi task with event handling (ESP32-style)
2. BLE peripheral with GATT server (Nordic NRF52)
3. MQTT over WiFi with connection management
4. BLE central scanning and connecting

**Code Examples in Rust:**
1. WiFi manager with Embassy async framework
2. BLE peripheral with GATT services
3. Async MQTT client
4. WiFi/BLE coexistence manager

**Best Practices:**
- Task design and priority management
- State machine implementation
- Resource and power optimization
- Security considerations
- Testing and debugging strategies

The document provides production-ready examples showing how to integrate wireless stacks with FreeRTOS tasks, handle events, manage connection states, and implement reliable communication patterns for IoT devices.


## Overview

Wireless connectivity is a critical component in modern embedded systems, enabling IoT devices, wearables, and smart products to communicate with cloud services, smartphones, and other devices. FreeRTOS provides an excellent foundation for integrating wireless stacks like WiFi and Bluetooth Low Energy (BLE) through its task-based architecture, synchronization primitives, and resource management capabilities.

This document explores the integration of wireless connectivity stacks with FreeRTOS, covering architecture patterns, state management, and practical implementations.

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [WiFi Integration](#wifi-integration)
3. [Bluetooth Low Energy (BLE) Integration](#bluetooth-low-energy-ble-integration)
4. [Connection State Management](#connection-state-management)
5. [Communication Patterns](#communication-patterns)
6. [Code Examples in C/C++](#code-examples-in-cc)
7. [Code Examples in Rust](#code-examples-in-rust)
8. [Best Practices](#best-practices)
9. [Summary](#summary)

---

## Architecture Overview

### Key Components

Wireless connectivity in FreeRTOS typically involves several layers:

```
┌─────────────────────────────────────────┐
│     Application Layer                   │
│  (Business Logic, Cloud Communication)  │
└─────────────────────────────────────────┘
              ↕
┌─────────────────────────────────────────┐
│     Protocol Layer                      │
│  (MQTT, HTTP, CoAP, GATT Services)      │
└─────────────────────────────────────────┘
              ↕
┌─────────────────────────────────────────┐
│     Network Stack                       │
│  (TCP/IP Stack, BLE Host Stack)         │
└─────────────────────────────────────────┘
              ↕
┌─────────────────────────────────────────┐
│     Driver Layer                        │
│  (WiFi Driver, BLE Controller)          │
└─────────────────────────────────────────┘
              ↕
┌─────────────────────────────────────────┐
│     Hardware Layer                      │
│  (WiFi Radio, BLE Radio)                │
└─────────────────────────────────────────┘
```

### FreeRTOS Task Architecture

A typical wireless application uses multiple tasks:

- **WiFi Task**: Manages WiFi connection, reconnection, and events
- **BLE Task**: Handles BLE advertising, scanning, and connections
- **Network Task**: Processes incoming/outgoing network packets
- **Application Tasks**: Business logic, sensor reading, data processing
- **Event Handler Task**: Coordinates wireless events across subsystems

---

## WiFi Integration

### WiFi State Machine

WiFi connectivity follows a state machine pattern:

```
        ┌──────────┐
        │   IDLE   │
        └────┬─────┘
             │ init
        ┌────▼──────┐
        │   READY   │◄────────────┐
        └────┬──────┘             │
             │ connect            │
        ┌────▼──────────┐         │
        │  CONNECTING   │         │
        └───┬───────┬───┘         │
            │       │             │
         ┌──▼──────┐│ timeout     │
         │CONNECTED││             │
         └──┬──────┘│             │
            │       │             │
            └───────┴─────────────┘
                 disconnect
```

### WiFi Task Structure

The WiFi task typically:
1. Initializes the WiFi driver
2. Monitors connection state
3. Handles reconnection logic
4. Processes WiFi events (scan results, connection status)
5. Manages power saving modes

### Key Considerations

- **Event-Driven Architecture**: Use event groups or queues for WiFi events
- **Reconnection Strategy**: Implement exponential backoff for failed connections
- **Power Management**: Balance between always-on and power-save modes
- **Credential Management**: Securely store and retrieve WiFi credentials
- **Multi-Network Support**: Handle roaming and network switching

---

## Bluetooth Low Energy (BLE) Integration

### BLE Roles

FreeRTOS devices can operate in different BLE roles:

- **Peripheral (Slave)**: Advertises services, accepts connections
- **Central (Master)**: Scans for peripherals, initiates connections
- **Broadcaster**: Sends advertisement data without accepting connections
- **Observer**: Scans for advertisements without connecting

### BLE Architecture Layers

```
┌──────────────────────────────────────┐
│  GAP (Generic Access Profile)       │
│  - Advertising, Scanning, Connection │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│  GATT (Generic Attribute Profile)    │
│  - Services, Characteristics         │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│  ATT (Attribute Protocol)            │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│  L2CAP (Logical Link Control)        │
└──────────────────────────────────────┘
┌──────────────────────────────────────┐
│  HCI (Host Controller Interface)     │
└──────────────────────────────────────┘
```

### BLE Connection States

```
┌──────────────┐
│ Standby      │
└──────┬───────┘
       │
┌──────▼───────┐
│ Advertising  │
└──────┬───────┘
       │
┌──────▼───────┐     ┌────────────┐
│ Connected    │◄────┤ Initiating │
└──────┬───────┘     └────────────┘
       │
┌──────▼───────┐
│ Disconnected │
└──────────────┘
```

---

## Connection State Management

### State Machine Implementation Pattern

Effective state management is crucial for reliable wireless connectivity:

1. **Use Enumerations**: Define clear states
2. **Event Queues**: Receive state change events
3. **State Transition Table**: Define valid transitions
4. **Timeout Handling**: Implement watchdogs for stuck states
5. **Callback Registration**: Allow multiple subscribers to state changes

### Synchronization Primitives

- **Mutexes**: Protect shared wireless stack state
- **Semaphores**: Signal completion of wireless operations
- **Event Groups**: Coordinate multiple wireless events
- **Queues**: Pass data between wireless and application tasks

---

## Communication Patterns

### 1. Request-Response Pattern

Application sends request, waits for response:
- HTTP REST API calls
- CoAP transactions
- BLE Read/Write operations

### 2. Publish-Subscribe Pattern

Decoupled communication:
- MQTT topics
- BLE Notifications/Indications
- Event broadcasting

### 3. Streaming Pattern

Continuous data flow:
- Sensor data streaming
- Audio/Video over BLE
- Real-time telemetry

### 4. Connection Pooling

Manage multiple simultaneous connections:
- Multiple BLE peripheral connections
- Concurrent HTTP sessions
- WebSocket connections

---

## Code Examples in C/C++

### Example 1: WiFi Task with Event Handling (ESP32-style)

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

// Event bits
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_DISCONNECTED_BIT BIT1
#define WIFI_FAIL_BIT         BIT2

static const char *TAG = "WiFi";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#define MAX_RETRY 5

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started, connecting...");
                esp_wifi_connect();
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                if (s_retry_num < MAX_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "Retry connecting to AP, attempt %d", s_retry_num);
                } else {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    ESP_LOGE(TAG, "Failed to connect to AP");
                }
                xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Connected to AP");
                s_retry_num = 0;
                break;
                
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// WiFi initialization
void wifi_init_sta(const char* ssid, const char* password)
{
    s_wifi_event_group = xEventGroupCreate();
    
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                                ESP_EVENT_ANY_ID,
                                                &wifi_event_handler,
                                                NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                                IP_EVENT_STA_GOT_IP,
                                                &wifi_event_handler,
                                                NULL));
    
    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialization finished");
    
    // Wait for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", ssid);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID: %s", ssid);
    }
}

// WiFi monitoring task
void wifi_monitor_task(void *pvParameters)
{
    TickType_t last_check = xTaskGetTickCount();
    const TickType_t check_interval = pdMS_TO_TICKS(30000); // 30 seconds
    
    while (1) {
        vTaskDelayUntil(&last_check, check_interval);
        
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "RSSI: %d dBm, Channel: %d",
                     ap_info.rssi, ap_info.primary);
            
            // Implement roaming logic if needed
            if (ap_info.rssi < -80) {
                ESP_LOGW(TAG, "Weak signal, consider roaming");
            }
        } else {
            ESP_LOGW(TAG, "Not connected to AP");
        }
    }
}
```

### Example 2: BLE Peripheral with GATT Server (Nordic NRF52-style)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "ble_advertising.h"

#define DEVICE_NAME                     "FreeRTOS_BLE"
#define APP_BLE_CONN_CFG_TAG           1
#define APP_ADV_INTERVAL               300  // 187.5 ms
#define APP_ADV_DURATION               0    // Continuous

// Custom service UUID (128-bit)
#define CUSTOM_SERVICE_UUID_BASE  {0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15, \
                                   0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00}
#define CUSTOM_SERVICE_UUID        0x1400
#define CUSTOM_CHAR_UUID           0x1401

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;
static uint16_t m_service_handle;
static ble_gatts_char_handles_t m_char_handles;

// Queue for BLE events
static QueueHandle_t ble_event_queue;

typedef struct {
    uint16_t conn_handle;
    ble_evt_t event;
} ble_event_msg_t;

// BLE event handler
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_event_msg_t msg;
    msg.conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    memcpy(&msg.event, p_ble_evt, sizeof(ble_evt_t));
    
    xQueueSend(ble_event_queue, &msg, 0);
}

// GAP initialization
static void gap_params_init(void)
{
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    // Set device name
    sd_ble_gap_device_name_set(&sec_mode,
                               (const uint8_t *)DEVICE_NAME,
                               strlen(DEVICE_NAME));

    // Set connection parameters
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));
    gap_conn_params.min_conn_interval = MSEC_TO_UNITS(100, UNIT_1_25_MS);
    gap_conn_params.max_conn_interval = MSEC_TO_UNITS(200, UNIT_1_25_MS);
    gap_conn_params.slave_latency     = 0;
    gap_conn_params.conn_sup_timeout  = MSEC_TO_UNITS(4000, UNIT_10_MS);

    sd_ble_gap_ppcp_set(&gap_conn_params);
}

// GATT service initialization
static void services_init(void)
{
    uint32_t err_code;
    ble_uuid_t service_uuid;
    ble_uuid128_t base_uuid = {CUSTOM_SERVICE_UUID_BASE};
    
    // Add custom UUID base
    sd_ble_uuid_vs_add(&base_uuid, &service_uuid.type);
    service_uuid.uuid = CUSTOM_SERVICE_UUID;
    
    // Add service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &service_uuid,
                                        &m_service_handle);
    
    // Add characteristic
    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          char_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&cccd_md, 0, sizeof(cccd_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;

    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read   = 1;
    char_md.char_props.write  = 1;
    char_md.char_props.notify = 1;
    char_md.p_cccd_md         = &cccd_md;

    char_uuid.type = service_uuid.type;
    char_uuid.uuid = CUSTOM_CHAR_UUID;

    memset(&attr_md, 0, sizeof(attr_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc    = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth = 0;
    attr_md.wr_auth = 0;
    attr_md.vlen    = 1;

    memset(&attr_char_value, 0, sizeof(attr_char_value));
    attr_char_value.p_uuid    = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(uint8_t);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = 20;

    err_code = sd_ble_gatts_characteristic_add(m_service_handle,
                                                &char_md,
                                                &attr_char_value,
                                                &m_char_handles);
}

// Advertising initialization
static void advertising_init(void)
{
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));
    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance      = false;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.config.ble_adv_fast_enabled     = true;
    init.config.ble_adv_fast_interval    = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout     = APP_ADV_DURATION;

    ble_advertising_init(&init);
}

// BLE task processing events
void ble_task(void *pvParameters)
{
    ble_event_msg_t msg;
    
    while (1) {
        if (xQueueReceive(ble_event_queue, &msg, portMAX_DELAY)) {
            switch (msg.event.header.evt_id) {
                case BLE_GAP_EVT_CONNECTED:
                    m_conn_handle = msg.conn_handle;
                    printf("BLE Connected\n");
                    break;

                case BLE_GAP_EVT_DISCONNECTED:
                    m_conn_handle = BLE_CONN_HANDLE_INVALID;
                    printf("BLE Disconnected\n");
                    // Restart advertising
                    ble_advertising_start(BLE_ADV_MODE_FAST);
                    break;

                case BLE_GATTS_EVT_WRITE:
                {
                    ble_gatts_evt_write_t const * p_evt_write = 
                        &msg.event.evt.gatts_evt.params.write;
                    
                    if (p_evt_write->handle == m_char_handles.value_handle) {
                        printf("Received write: %d bytes\n", p_evt_write->len);
                        // Process received data
                    }
                    break;
                }

                default:
                    break;
            }
        }
    }
}

// Send notification
void ble_send_notification(uint8_t *data, uint16_t length)
{
    if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
        ble_gatts_hvx_params_t params;
        uint16_t len = length;

        memset(&params, 0, sizeof(params));
        params.type   = BLE_GATT_HVX_NOTIFICATION;
        params.handle = m_char_handles.value_handle;
        params.p_data = data;
        params.p_len  = &len;

        sd_ble_gatts_hvx(m_conn_handle, &params);
    }
}

// Initialize BLE stack
void ble_stack_init(void)
{
    // Create event queue
    ble_event_queue = xQueueCreate(10, sizeof(ble_event_msg_t));
    
    // Initialize SoftDevice
    nrf_sdh_enable_request();
    
    // Configure BLE stack
    uint32_t ram_start = 0;
    nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    nrf_sdh_ble_enable(&ram_start);
    
    // Register BLE event handler
    NRF_SDH_BLE_OBSERVER(m_ble_observer, 3, ble_evt_handler, NULL);
    
    // Initialize GAP, GATT, advertising
    gap_params_init();
    services_init();
    advertising_init();
    
    // Start advertising
    ble_advertising_start(BLE_ADV_MODE_FAST);
    
    // Create BLE processing task
    xTaskCreate(ble_task, "BLE", 512, NULL, 3, NULL);
}
```

### Example 3: MQTT over WiFi with Connection Management

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "mqtt_client.h"

#define MQTT_BROKER_URL    "mqtt://broker.hivemq.com"
#define MQTT_CLIENT_ID     "freertos_device_001"
#define MQTT_TOPIC_PUBLISH "sensor/temperature"
#define MQTT_TOPIC_SUBSCRIBE "device/command"

static esp_mqtt_client_handle_t mqtt_client;
static SemaphoreHandle_t mqtt_connected_sem;

typedef enum {
    MQTT_STATE_DISCONNECTED,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_ERROR
} mqtt_state_t;

static mqtt_state_t mqtt_state = MQTT_STATE_DISCONNECTED;

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            printf("MQTT Connected\n");
            mqtt_state = MQTT_STATE_CONNECTED;
            xSemaphoreGive(mqtt_connected_sem);
            
            // Subscribe to topics
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_SUBSCRIBE, 1);
            break;

        case MQTT_EVENT_DISCONNECTED:
            printf("MQTT Disconnected\n");
            mqtt_state = MQTT_STATE_DISCONNECTED;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            printf("MQTT Subscribed, msg_id=%d\n", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            printf("MQTT Data received\n");
            printf("Topic: %.*s\n", event->topic_len, event->topic);
            printf("Data: %.*s\n", event->data_len, event->data);
            
            // Process received command
            if (strncmp(event->topic, MQTT_TOPIC_SUBSCRIBE, event->topic_len) == 0) {
                // Handle command
            }
            break;

        case MQTT_EVENT_ERROR:
            printf("MQTT Error\n");
            mqtt_state = MQTT_STATE_ERROR;
            break;

        default:
            break;
    }
}

// Initialize MQTT
void mqtt_app_start(void)
{
    mqtt_connected_sem = xSemaphoreCreateBinary();
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.keepalive = 60,
        .network.reconnect_timeout_ms = 10000,
        .network.timeout_ms = 10000,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

// Publish sensor data task
void mqtt_publish_task(void *pvParameters)
{
    char payload[64];
    int msg_id;
    float temperature;
    
    // Wait for MQTT connection
    xSemaphoreTake(mqtt_connected_sem, portMAX_DELAY);
    
    while (1) {
        if (mqtt_state == MQTT_STATE_CONNECTED) {
            // Read sensor (simulated)
            temperature = 20.0 + (rand() % 100) / 10.0;
            
            // Format payload
            snprintf(payload, sizeof(payload), "{\"temp\":%.1f}", temperature);
            
            // Publish message
            msg_id = esp_mqtt_client_publish(mqtt_client,
                                             MQTT_TOPIC_PUBLISH,
                                             payload,
                                             0,
                                             1,  // QoS
                                             0); // Retain
            
            if (msg_id >= 0) {
                printf("Published: %s (msg_id=%d)\n", payload, msg_id);
            } else {
                printf("Failed to publish\n");
            }
        } else {
            printf("MQTT not connected, state=%d\n", mqtt_state);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // Publish every 5 seconds
    }
}
```

### Example 4: BLE Central Scanning and Connecting

```c
#include "FreeRTOS.h"
#include "task.h"
#include "ble_gap.h"

#define SCAN_INTERVAL  0x00A0  // 100 ms
#define SCAN_WINDOW    0x0050  // 50 ms
#define SCAN_TIMEOUT   0       // Continuous

typedef struct {
    ble_gap_addr_t address;
    int8_t rssi;
    char name[32];
    uint8_t data[31];
    uint8_t data_len;
} scanned_device_t;

static scanned_device_t scanned_devices[10];
static uint8_t device_count = 0;

// Scan parameters
static ble_gap_scan_params_t const m_scan_params = {
    .active        = 1,
    .interval      = SCAN_INTERVAL,
    .window        = SCAN_WINDOW,
    .timeout       = SCAN_TIMEOUT,
    .scan_phys     = BLE_GAP_PHY_1MBPS,
    .filter_policy = BLE_GAP_SCAN_FP_ACCEPT_ALL,
};

// Parse advertising data
static bool parse_adv_name(const uint8_t *adv_data, uint8_t adv_len, char *name, uint8_t name_len)
{
    uint8_t offset = 0;
    
    while (offset < adv_len) {
        uint8_t field_len = adv_data[offset];
        uint8_t field_type = adv_data[offset + 1];
        
        if (field_type == BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME ||
            field_type == BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME) {
            uint8_t copy_len = (field_len - 1 < name_len - 1) ? 
                               field_len - 1 : name_len - 1;
            memcpy(name, &adv_data[offset + 2], copy_len);
            name[copy_len] = '\0';
            return true;
        }
        
        offset += field_len + 1;
    }
    
    return false;
}

// BLE scan event handler
static void ble_scan_evt_handler(ble_evt_t const * p_ble_evt)
{
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_ADV_REPORT:
        {
            ble_gap_evt_adv_report_t const * p_adv_report = 
                &p_gap_evt->params.adv_report;
            
            // Add to scanned devices list
            if (device_count < 10) {
                scanned_device_t *dev = &scanned_devices[device_count];
                
                memcpy(&dev->address, &p_adv_report->peer_addr, 
                       sizeof(ble_gap_addr_t));
                dev->rssi = p_adv_report->rssi;
                dev->data_len = p_adv_report->data.len;
                memcpy(dev->data, p_adv_report->data.p_data, dev->data_len);
                
                // Parse device name
                if (parse_adv_name(p_adv_report->data.p_data,
                                  p_adv_report->data.len,
                                  dev->name,
                                  sizeof(dev->name))) {
                    printf("Found device: %s, RSSI: %d dBm\n", 
                           dev->name, dev->rssi);
                }
                
                device_count++;
            }
            
            // Resume scanning
            sd_ble_gap_scan_start(NULL, NULL);
            break;
        }

        case BLE_GAP_EVT_TIMEOUT:
            if (p_gap_evt->params.timeout.src == BLE_GAP_TIMEOUT_SRC_SCAN) {
                printf("Scan timeout\n");
            }
            break;

        default:
            break;
    }
}

// Connect to device
void ble_connect_to_device(ble_gap_addr_t *peer_addr)
{
    ble_gap_conn_params_t conn_params = {
        .min_conn_interval = MSEC_TO_UNITS(7.5, UNIT_1_25_MS),
        .max_conn_interval = MSEC_TO_UNITS(30, UNIT_1_25_MS),
        .slave_latency     = 0,
        .conn_sup_timeout  = MSEC_TO_UNITS(4000, UNIT_10_MS),
    };

    ble_gap_scan_params_t scan_params = {
        .interval    = SCAN_INTERVAL,
        .window      = SCAN_WINDOW,
        .timeout     = 0,
        .scan_phys   = BLE_GAP_PHY_1MBPS,
    };

    uint32_t err_code = sd_ble_gap_connect(peer_addr,
                                           &scan_params,
                                           &conn_params,
                                           APP_BLE_CONN_CFG_TAG);
    
    if (err_code == NRF_SUCCESS) {
        printf("Connecting to device...\n");
    }
}

// Start BLE scanning
void ble_start_scan(void)
{
    device_count = 0;
    memset(scanned_devices, 0, sizeof(scanned_devices));
    
    uint32_t err_code = sd_ble_gap_scan_start(&m_scan_params, NULL);
    if (err_code == NRF_SUCCESS) {
        printf("BLE scan started\n");
    }
}

// BLE scanning task
void ble_scan_task(void *pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for system initialization
    
    ble_start_scan();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // Scan for 10 seconds
        
        // Process found devices
        printf("Found %d devices:\n", device_count);
        for (uint8_t i = 0; i < device_count; i++) {
            printf("  %d: %s (RSSI: %d)\n", 
                   i, scanned_devices[i].name, scanned_devices[i].rssi);
        }
        
        // Optionally connect to strongest signal
        if (device_count > 0) {
            int8_t max_rssi = -100;
            uint8_t best_idx = 0;
            
            for (uint8_t i = 0; i < device_count; i++) {
                if (scanned_devices[i].rssi > max_rssi) {
                    max_rssi = scanned_devices[i].rssi;
                    best_idx = i;
                }
            }
            
            // Connect to best device
            // ble_connect_to_device(&scanned_devices[best_idx].address);
        }
        
        // Restart scan
        device_count = 0;
        ble_start_scan();
    }
}
```

---

## Code Examples in Rust

### Example 1: WiFi Manager with Embassy (Rust Async)

```rust
#![no_std]
#![no_main]

use embassy_executor::Spawner;
use embassy_net::{Config, Stack, StackResources};
use embassy_time::{Duration, Timer};
use esp_wifi::{
    initialize,
    wifi::{
        ClientConfiguration, Configuration, WifiController, WifiDevice,
        WifiEvent, WifiStaDevice, WifiState,
    },
    EspWifiInitFor,
};
use esp_backtrace as _;
use esp_hal::{
    clock::ClockControl,
    peripherals::Peripherals,
    prelude::*,
    rng::Rng,
    system::SystemControl,
    timer::timg::TimerGroup,
};

const SSID: &str = "YourWiFiSSID";
const PASSWORD: &str = "YourPassword";

// WiFi task
#[embassy_executor::task]
async fn wifi_task(
    mut controller: WifiController<'static>,
) {
    log::info!("Starting WiFi controller");
    
    loop {
        match controller.wait_for_event().await {
            WifiEvent::StaStart => {
                log::info!("WiFi started, connecting...");
                controller.connect().await.ok();
            }
            WifiEvent::StaConnected => {
                log::info!("WiFi connected");
            }
            WifiEvent::StaDisconnected => {
                log::warn!("WiFi disconnected, reconnecting...");
                Timer::after(Duration::from_secs(5)).await;
                controller.connect().await.ok();
            }
            WifiEvent::StaIpAssigned => {
                log::info!("IP address assigned");
            }
            _ => {}
        }
    }
}

// Network task
#[embassy_executor::task]
async fn net_task(stack: &'static Stack<WifiDevice<'static, WifiStaDevice>>) {
    stack.run().await
}

// Connection monitor task
#[embassy_executor::task]
async fn connection_monitor(stack: &'static Stack<WifiDevice<'static, WifiStaDevice>>) {
    loop {
        if stack.is_link_up() {
            if let Some(config) = stack.config_v4() {
                log::info!("IP: {}", config.address);
                log::info!("Gateway: {}", config.gateway.unwrap());
            }
        } else {
            log::warn!("Network link is down");
        }
        
        Timer::after(Duration::from_secs(30)).await;
    }
}

#[main]
async fn main(spawner: Spawner) {
    esp_println::logger::init_logger_from_env();
    
    let peripherals = Peripherals::take();
    let system = SystemControl::new(peripherals.SYSTEM);
    let clocks = ClockControl::max(system.clock_control).freeze();
    
    let timer = TimerGroup::new(peripherals.TIMG0, &clocks);
    let init = initialize(
        EspWifiInitFor::Wifi,
        timer.timer0,
        Rng::new(peripherals.RNG),
        peripherals.RADIO_CLK,
        &clocks,
    )
    .unwrap();

    let wifi = peripherals.WIFI;
    let (wifi_interface, controller) =
        esp_wifi::wifi::new_with_mode(&init, wifi, WifiStaDevice).unwrap();

    // Configure WiFi
    let wifi_config = Configuration::Client(ClientConfiguration {
        ssid: SSID.try_into().unwrap(),
        password: PASSWORD.try_into().unwrap(),
        ..Default::default()
    });
    controller.set_configuration(&wifi_config).unwrap();

    // Setup network stack
    let seed = 1234; // Use hardware RNG in production
    let config = Config::dhcpv4(Default::default());
    
    let stack = &*singleton!(
        Stack<WifiDevice<'static, WifiStaDevice>>,
        Stack::new(
            wifi_interface,
            config,
            singleton!(StackResources<3>, StackResources::<3>::new()),
            seed
        )
    );

    // Spawn tasks
    spawner.spawn(wifi_task(controller)).ok();
    spawner.spawn(net_task(stack)).ok();
    spawner.spawn(connection_monitor(stack)).ok();
    
    // Application task
    application_task(stack).await;
}

// Application logic
async fn application_task(stack: &'static Stack<WifiDevice<'static, WifiStaDevice>>) {
    loop {
        if stack.is_link_up() {
            log::info!("Network is up, can do work");
            // Perform network operations
        }
        
        Timer::after(Duration::from_secs(10)).await;
    }
}

// Helper macro for static allocation
macro_rules! singleton {
    ($val:expr) => {{
        type T = impl Sized;
        static STATIC_CELL: StaticCell<T> = StaticCell::new();
        STATIC_CELL.init_with(|| $val)
    }};
}
```

### Example 2: BLE Peripheral with GATT in Rust

```rust
#![no_std]
#![no_main]

use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};
use nrf_softdevice::{
    ble::{
        gatt_server, peripheral,
        Connection,
    },
    Softdevice,
};
use defmt::*;

// Define GATT service
#[nrf_softdevice::gatt_service(uuid = "180f")] // Battery Service
struct BatteryService {
    #[characteristic(uuid = "2a19", read, notify)]
    battery_level: u8,
}

// BLE Server
#[nrf_softdevice::gatt_server]
struct Server {
    bas: BatteryService,
}

// Advertising data
const DEVICE_NAME: &[u8] = b"FreeRTOS-BLE";

#[embassy_executor::task]
async fn softdevice_task(sd: &'static Softdevice) -> ! {
    sd.run().await
}

// BLE peripheral task
#[embassy_executor::task]
async fn bluetooth_task(sd: &'static Softdevice, server: &'static Server) {
    let adv_data = [
        0x02, 0x01, 0x06, // Flags
        0x0d, 0x09, // Complete local name
        b'F', b'r', b'e', b'e', b'R', b'T', b'O', b'S', b'-', b'B', b'L', b'E',
    ];
    
    let scan_data = [
        0x03, 0x03, 0x0f, 0x18, // Battery service UUID
    ];

    loop {
        let config = peripheral::Config::default();
        let adv = peripheral::ConnectableAdvertisement::ScannableUndirected {
            adv_data: &adv_data,
            scan_data: &scan_data,
        };
        
        info!("Advertising...");
        let conn = peripheral::advertise_connectable(sd, adv, &config)
            .await
            .unwrap();
        
        info!("BLE connected");
        
        let res = gatt_server::run(&conn, server, |e| match e {
            ServerEvent::Bas(e) => match e {
                BatteryServiceEvent::BatteryLevelCccdWrite { notifications } => {
                    info!("Battery notifications: {}", notifications);
                }
            },
        })
        .await;
        
        if let Err(e) = res {
            warn!("GATT server error: {:?}", e);
        }
        
        info!("BLE disconnected");
    }
}

// Battery notification task
#[embassy_executor::task]
async fn battery_task(sd: &'static Softdevice, server: &'static Server) {
    let mut battery_level: u8 = 100;
    
    loop {
        Timer::after(Duration::from_secs(5)).await;
        
        // Simulate battery drain
        if battery_level > 0 {
            battery_level -= 1;
        } else {
            battery_level = 100;
        }
        
        // Update characteristic
        if let Err(e) = server.bas.battery_level_set(&battery_level) {
            warn!("Failed to update battery: {:?}", e);
        }
        
        // Notify if someone is connected
        if let Err(e) = server.bas.battery_level_notify(sd, &battery_level) {
            // No one connected or notifications disabled
        } else {
            info!("Battery level notified: {}%", battery_level);
        }
    }
}

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    info!("Starting BLE application");
    
    let config = nrf_softdevice::Config {
        clock: Some(raw::nrf_clock_lf_cfg_t {
            source: raw::NRF_CLOCK_LF_SRC_XTAL as u8,
            rc_ctiv: 0,
            rc_temp_ctiv: 0,
            accuracy: raw::NRF_CLOCK_LF_ACCURACY_20_PPM as u8,
        }),
        conn_gap: Some(raw::ble_gap_conn_cfg_t {
            conn_count: 1,
            event_length: 24,
        }),
        conn_gatt: Some(raw::ble_gatt_conn_cfg_t { att_mtu: 256 }),
        gatts_attr_tab_size: Some(raw::ble_gatts_cfg_attr_tab_size_t {
            attr_tab_size: 32768,
        }),
        gap_role_count: Some(raw::ble_gap_cfg_role_count_t {
            adv_set_count: 1,
            periph_role_count: 1,
            central_role_count: 0,
            central_sec_count: 0,
            _bitfield_1: raw::ble_gap_cfg_role_count_t::new_bitfield_1(0),
        }),
        gap_device_name: Some(raw::ble_gap_cfg_device_name_t {
            p_value: DEVICE_NAME.as_ptr() as *const u8 as _,
            current_len: DEVICE_NAME.len() as u16,
            max_len: DEVICE_NAME.len() as u16,
            write_perm: unsafe { core::mem::zeroed() },
            _bitfield_1: raw::ble_gap_cfg_device_name_t::new_bitfield_1(
                raw::BLE_GATTS_VLOC_STACK as u8,
            ),
        }),
        ..Default::default()
    };

    let sd = Softdevice::enable(&config);
    let server = Server::new(sd).unwrap();
    
    spawner.spawn(softdevice_task(sd)).unwrap();
    spawner.spawn(bluetooth_task(sd, server)).unwrap();
    spawner.spawn(battery_task(sd, server)).unwrap();
}
```

### Example 3: Async MQTT Client in Rust

```rust
use embassy_executor::Spawner;
use embassy_net::tcp::TcpSocket;
use embassy_time::{Duration, Timer};
use embedded_io_async::{Read, Write};
use rust_mqtt::{
    client::{client::MqttClient, client_config::ClientConfig},
    packet::v5::reason_codes::ReasonCode,
    utils::rng_generator::CountingRng,
};

const BROKER_HOST: &str = "broker.hivemq.com";
const BROKER_PORT: u16 = 1883;
const CLIENT_ID: &str = "freertos_rust_001";
const TOPIC_PUBLISH: &str = "sensor/temperature";
const TOPIC_SUBSCRIBE: &str = "device/command";

#[embassy_executor::task]
async fn mqtt_task(
    stack: &'static Stack<WifiDevice<'static, WifiStaDevice>>,
) {
    // Wait for network
    loop {
        if stack.is_link_up() {
            break;
        }
        Timer::after(Duration::from_millis(500)).await;
    }
    
    info!("Network ready, connecting to MQTT broker");
    
    let mut rx_buffer = [0; 4096];
    let mut tx_buffer = [0; 4096];
    let mut socket = TcpSocket::new(stack, &mut rx_buffer, &mut tx_buffer);
    socket.set_timeout(Some(Duration::from_secs(10)));
    
    loop {
        // Resolve broker address
        let remote_endpoint = (BROKER_HOST, BROKER_PORT);
        
        info!("Connecting to {}:{}", BROKER_HOST, BROKER_PORT);
        
        match socket.connect(remote_endpoint).await {
            Ok(()) => {
                info!("TCP connected to MQTT broker");
                
                // Run MQTT client
                if let Err(e) = run_mqtt_client(&mut socket).await {
                    error!("MQTT error: {:?}", e);
                }
            }
            Err(e) => {
                error!("TCP connect failed: {:?}", e);
            }
        }
        
        info!("Reconnecting in 5 seconds...");
        Timer::after(Duration::from_secs(5)).await;
    }
}

async fn run_mqtt_client<T: Read + Write>(
    socket: &mut T,
) -> Result<(), &'static str> {
    let mut config = ClientConfig::new(
        rust_mqtt::client::client_config::MqttVersion::MQTTv5,
        CountingRng(20000),
    );
    config.add_max_subscribe_qos(rust_mqtt::packet::v5::publish_packet::QualityOfService::QoS1);
    config.add_client_id(CLIENT_ID);
    config.max_packet_size = 100;
    
    let mut recv_buffer = [0; 4096];
    let mut write_buffer = [0; 4096];
    
    let mut client = MqttClient::<_, 5, _>::new(
        socket,
        &mut write_buffer,
        4096,
        &mut recv_buffer,
        4096,
        config,
    );
    
    // Connect to broker
    match client.connect_to_broker().await {
        Ok(()) => {
            info!("MQTT connected");
        }
        Err(e) => {
            error!("MQTT connect failed: {:?}", e);
            return Err("Connect failed");
        }
    }
    
    // Subscribe to command topic
    match client
        .subscribe_to_topic(TOPIC_SUBSCRIBE)
        .await
    {
        Ok(()) => {
            info!("Subscribed to {}", TOPIC_SUBSCRIBE);
        }
        Err(e) => {
            error!("Subscribe failed: {:?}", e);
        }
    }
    
    let mut publish_timer = Timer::after(Duration::from_secs(0));
    let mut sequence = 0u32;
    
    loop {
        // Check for incoming messages (non-blocking)
        match client.receive_message().await {
            Ok((topic, payload)) => {
                info!("Received on {}: {:?}", topic, payload);
                
                // Process command
                if topic == TOPIC_SUBSCRIBE {
                    handle_command(payload);
                }
            }
            Err(ReasonCode::NetworkError) => {
                error!("Network error, disconnecting");
                return Err("Network error");
            }
            Err(_) => {
                // No message or other error, continue
            }
        }
        
        // Publish sensor data periodically
        if publish_timer.is_expired() {
            let temperature = 20.0 + (sequence % 10) as f32;
            let payload = format!("{{\"temp\":{:.1},\"seq\":{}}}", temperature, sequence);
            
            match client
                .send_message(
                    TOPIC_PUBLISH,
                    payload.as_bytes(),
                    rust_mqtt::packet::v5::publish_packet::QualityOfService::QoS1,
                    false,
                )
                .await
            {
                Ok(()) => {
                    info!("Published: {}", payload);
                    sequence += 1;
                }
                Err(e) => {
                    error!("Publish failed: {:?}", e);
                }
            }
            
            publish_timer = Timer::after(Duration::from_secs(5));
        }
        
        // Small delay to yield
        Timer::after(Duration::from_millis(10)).await;
    }
}

fn handle_command(payload: &[u8]) {
    // Parse and handle command
    if let Ok(cmd) = core::str::from_utf8(payload) {
        info!("Processing command: {}", cmd);
        
        // Example: {"action":"reboot"}
        if cmd.contains("reboot") {
            info!("Reboot command received");
            // Handle reboot
        }
    }
}
```

### Example 4: WiFi + BLE Coexistence Manager in Rust

```rust
use embassy_sync::{blocking_mutex::raw::CriticalSectionRawMutex, channel::Channel};
use embassy_executor::Spawner;
use embassy_time::{Duration, Timer};

// Wireless resource allocation
#[derive(Debug, Clone, Copy)]
enum WirelessResource {
    WiFi,
    BLE,
}

#[derive(Debug, Clone, Copy)]
enum ResourceRequest {
    Acquire(WirelessResource, u32), // Resource, priority
    Release(WirelessResource),
}

static RESOURCE_CHANNEL: Channel<CriticalSectionRawMutex, ResourceRequest, 10> = 
    Channel::new();

// Resource manager task
#[embassy_executor::task]
async fn wireless_resource_manager() {
    let mut wifi_owner: Option<u32> = None;
    let mut ble_owner: Option<u32> = None;
    
    loop {
        let request = RESOURCE_CHANNEL.receive().await;
        
        match request {
            ResourceRequest::Acquire(WirelessResource::WiFi, priority) => {
                if wifi_owner.is_none() || wifi_owner.unwrap() < priority {
                    // Grant WiFi access
                    wifi_owner = Some(priority);
                    info!("WiFi granted to priority {}", priority);
                    
                    // If BLE is active, may need to coordinate
                    if ble_owner.is_some() {
                        // Implement coexistence algorithm
                        coexistence_balance();
                    }
                }
            }
            
            ResourceRequest::Release(WirelessResource::WiFi) => {
                wifi_owner = None;
                info!("WiFi released");
            }
            
            ResourceRequest::Acquire(WirelessResource::BLE, priority) => {
                if ble_owner.is_none() || ble_owner.unwrap() < priority {
                    ble_owner = Some(priority);
                    info!("BLE granted to priority {}", priority);
                    
                    if wifi_owner.is_some() {
                        coexistence_balance();
                    }
                }
            }
            
            ResourceRequest::Release(WirelessResource::BLE) => {
                ble_owner = None;
                info!("BLE released");
            }
        }
    }
}

fn coexistence_balance() {
    // Implement time-division multiplexing or priority-based arbitration
    info!("Balancing WiFi and BLE coexistence");
    
    // Example: Adjust WiFi scan intervals when BLE is active
    // Or adjust BLE connection intervals when WiFi needs bandwidth
}

// WiFi task requesting resource
#[embassy_executor::task]
async fn wifi_application_task() {
    loop {
        // Request WiFi resource
        RESOURCE_CHANNEL
            .send(ResourceRequest::Acquire(WirelessResource::WiFi, 10))
            .await;
        
        // Perform WiFi operations
        info!("WiFi: Performing network operations");
        Timer::after(Duration::from_secs(2)).await;
        
        // Release resource
        RESOURCE_CHANNEL
            .send(ResourceRequest::Release(WirelessResource::WiFi))
            .await;
        
        Timer::after(Duration::from_secs(3)).await;
    }
}

// BLE task requesting resource
#[embassy_executor::task]
async fn ble_application_task() {
    loop {
        // Request BLE resource
        RESOURCE_CHANNEL
            .send(ResourceRequest::Acquire(WirelessResource::BLE, 8))
            .await;
        
        // Perform BLE operations
        info!("BLE: Handling connections");
        Timer::after(Duration::from_secs(1)).await;
        
        // Release resource
        RESOURCE_CHANNEL
            .send(ResourceRequest::Release(WirelessResource::BLE))
            .await;
        
        Timer::after(Duration::from_secs(4)).await;
    }
}

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    spawner.spawn(wireless_resource_manager()).unwrap();
    spawner.spawn(wifi_application_task()).unwrap();
    spawner.spawn(ble_application_task()).unwrap();
}
```

---

## Best Practices

### 1. **Task Design**

- **Separate Concerns**: Use dedicated tasks for WiFi, BLE, network protocol, and application logic
- **Priority Assignment**: Network stack tasks should have higher priority than application tasks
- **Stack Sizing**: Allocate sufficient stack for wireless libraries (typically 4KB-8KB)

### 2. **State Management**

- **Clear State Machines**: Define explicit states and transitions
- **Event-Driven**: Use queues and event groups for state changes
- **Timeout Protection**: Implement watchdogs for each state
- **Atomic Operations**: Use mutexes for concurrent access to state variables

### 3. **Resource Management**

- **Buffer Pools**: Pre-allocate network buffers to avoid fragmentation
- **Connection Limits**: Set maximum concurrent connections based on memory
- **Graceful Degradation**: Handle resource exhaustion elegantly
- **Memory Monitoring**: Track heap usage in wireless stack

### 4. **Power Optimization**

- **Dynamic Power Modes**: Switch between active and sleep modes
- **Connection Parameters**: Optimize BLE connection intervals and latency
- **WiFi Power Save**: Use DTIM-based power save when appropriate
- **Wake Locks**: Prevent sleep during critical operations

### 5. **Error Handling**

- **Reconnection Logic**: Implement exponential backoff
- **Error Logging**: Track connection failures for debugging
- **Recovery Strategies**: Reset wireless stack if needed
- **Fallback Mechanisms**: Provide degraded functionality on errors

### 6. **Security**

- **Secure Credentials**: Never hardcode WiFi passwords
- **TLS/SSL**: Use encryption for network communication
- **BLE Bonding**: Implement pairing and encryption
- **Certificate Validation**: Verify server certificates
- **Secure Storage**: Use hardware security for keys

### 7. **Testing**

- **Connection Stress Tests**: Test rapid connect/disconnect cycles
- **Range Testing**: Verify behavior at edge of connectivity
- **Interference Testing**: Test with multiple wireless devices
- **Power Cycle Testing**: Ensure recovery from power loss
- **Long-Duration Testing**: Run for days to catch memory leaks

### 8. **Debugging**

- **Logging Levels**: Implement configurable debug output
- **State Tracking**: Log all state transitions
- **Packet Inspection**: Capture and analyze network traffic
- **Performance Metrics**: Monitor connection time, RSSI, throughput
- **Memory Profiling**: Track allocation patterns

---

## Summary

Wireless connectivity integration with FreeRTOS enables powerful IoT and embedded applications by combining reliable real-time task management with modern communication protocols. This document covered:

### Key Takeaways

1. **Architecture**: Wireless stacks integrate naturally with FreeRTOS's task-based model, with separate tasks for connection management, protocol handling, and application logic.

2. **WiFi Integration**: Requires careful state machine management, event-driven architecture, and robust reconnection strategies. Modern platforms like ESP32 provide good integration with FreeRTOS.

3. **BLE Integration**: Involves understanding GAP/GATT layers, managing connection states, and implementing peripheral or central roles. BLE's event-driven nature maps well to FreeRTOS primitives.

4. **State Management**: Critical for reliable wireless operation. Use well-defined state machines, event groups for synchronization, and timeout protection.

5. **Communication Patterns**: Different patterns (request-response, publish-subscribe, streaming) suit different applications. Choose based on latency, bandwidth, and reliability requirements.

6. **Code Examples**: Both C/C++ and Rust provide robust tools for wireless development:
   - **C/C++**: Mature ecosystems with vendor SDKs (ESP-IDF, Nordic SDK)
   - **Rust**: Type safety and async/await patterns with Embassy framework

7. **Best Practices**: Focus on proper task design, resource management, power optimization, security, and thorough testing to build reliable wireless embedded systems.

8. **Coexistence**: When using both WiFi and BLE, implement resource arbitration and time-division multiplexing to minimize interference.

### Practical Considerations

- Start with vendor examples and SDK documentation
- Understand your platform's wireless architecture
- Monitor resource usage (heap, stack, CPU)
- Test thoroughly in real-world conditions
- Plan for failure modes and recovery
- Keep wireless firmware updated
- Document your state machines and event flows

### Next Steps

For deeper learning:
- Study your specific wireless chip's technical reference manual
- Experiment with different connection parameters
- Implement OTA (Over-The-Air) updates
- Explore mesh networking protocols
- Integrate with cloud IoT platforms (AWS IoT, Azure IoT, Google Cloud IoT)
- Implement device provisioning and fleet management

Wireless connectivity transforms embedded devices into connected products, but requires careful integration, robust error handling, and thorough testing. FreeRTOS provides the foundation for building reliable wireless applications through its proven real-time capabilities and rich ecosystem.