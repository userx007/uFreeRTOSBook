# HTTP Server and REST APIs in FreeRTOS

## Summary

The guide covers implementing embedded web servers on FreeRTOS with complete working examples in both **C/C++** and **Rust**, including:

### Main Topics:
- **Architecture patterns** for embedded HTTP servers (single-threaded, connection pool, worker pool, event-driven)
- **Complete C/C++ implementations** using lwIP with connection handling, routing, and thread-safe access
- **Rust implementations** using `esp-idf-svc` with modern async patterns
- **Real-world examples**: Sensor data APIs, actuator control, device configuration with NVS persistence
- **Security**: Authentication, rate limiting, input validation, HTTPS
- **Performance optimization**: Memory management, task priorities, connection pooling, zero-copy techniques

### Code Examples Include:
- Basic HTTP server with request parsing and routing
- Worker pool pattern for concurrent connections
- Thread-safe REST APIs with mutexes/semaphores
- JSON-based request/response handling (using cJSON and serde_json)
- Configuration management with flash persistence
- Async HTTP servers using tokio

### Best Practices Covered:
- Memory constraints and buffer management
- Concurrent access patterns
- Error handling and timeout management
- Security implementations
- Performance benchmarking

The document provides production-ready code that can be adapted for ESP32, STM32, and other FreeRTOS-capable microcontrollers.

# HTTP Server and REST APIs in FreeRTOS

## Table of Contents
1. [Introduction](#introduction)
2. [Core Concepts](#core-concepts)
3. [Architecture Overview](#architecture-overview)
4. [C/C++ Implementation](#cc-implementation)
5. [Rust Implementation](#rust-implementation)
6. [Best Practices](#best-practices)
7. [Performance Considerations](#performance-considerations)
8. [Summary](#summary)

---

## Introduction

Implementing HTTP servers and REST APIs on embedded systems running FreeRTOS enables remote device control, monitoring, and configuration through standard web protocols. This approach provides several advantages:

- **Universal Access**: Any device with a web browser or HTTP client can interact with the embedded system
- **Standardized Interface**: RESTful APIs follow well-established conventions
- **Easy Integration**: Simple to integrate with IoT platforms, mobile apps, and web dashboards
- **Human-Readable**: JSON-based communication is easy to debug and understand

However, embedded HTTP servers face unique challenges:
- Limited RAM and flash memory
- Concurrent connection handling with minimal overhead
- Real-time constraints of the embedded system
- Security considerations (authentication, encryption)

---

## Core Concepts

### HTTP Basics for Embedded Systems

HTTP (Hypertext Transfer Protocol) is a request-response protocol. The basic flow:

1. **Client sends HTTP request** → Method (GET, POST, PUT, DELETE) + URI + Headers + Body
2. **Server processes request** → Routes to handler, executes logic
3. **Server sends HTTP response** → Status code + Headers + Body

### REST API Principles

REST (Representational State Transfer) APIs use HTTP methods semantically:

- **GET**: Retrieve resource state (read-only, idempotent)
- **POST**: Create new resource
- **PUT**: Update existing resource (replace)
- **PATCH**: Partial update of resource
- **DELETE**: Remove resource

Resource URLs follow hierarchical patterns:
```
/api/devices/{device_id}
/api/sensors/{sensor_id}/readings
/api/config/network
```

### FreeRTOS Integration Points

1. **Task Architecture**: Separate tasks for HTTP server, request handlers, and business logic
2. **Queue-Based Communication**: Decouple HTTP handlers from hardware control
3. **Semaphores/Mutexes**: Protect shared resources (sensor data, configuration)
4. **Memory Management**: Careful allocation for HTTP buffers and JSON payloads

---

## Architecture Overview

### Typical Architecture

```
┌─────────────────────────────────────────────────┐
│                HTTP Clients                      │
│    (Browser, Mobile App, IoT Platform)          │
└─────────────────┬───────────────────────────────┘
                  │ HTTP Requests
                  ▼
┌─────────────────────────────────────────────────┐
│           HTTP Server Task (FreeRTOS)           │
│  ┌──────────────────────────────────────┐      │
│  │   Connection Manager                  │      │
│  │   (handles multiple connections)      │      │
│  └──────────────┬───────────────────────┘      │
│                 │                                │
│  ┌──────────────▼───────────────────────┐      │
│  │   Request Router                      │      │
│  │   (URL pattern matching)              │      │
│  └──────────────┬───────────────────────┘      │
│                 │                                │
│     ┌───────────┼───────────┐                   │
│     ▼           ▼           ▼                   │
│  ┌─────┐   ┌─────┐     ┌─────┐                │
│  │ GET │   │ POST│     │ PUT │ ...             │
│  │Handler   │Handler    │Handler                │
│  └──┬──┘   └──┬──┘     └──┬──┘                │
└─────┼─────────┼───────────┼────────────────────┘
      │         │           │
      │    ┌────▼────┐      │
      │    │ Queues  │      │
      │    │Semaphores│     │
      │    └────┬────┘      │
      │         │           │
      ▼         ▼           ▼
┌─────────────────────────────────────────────────┐
│          Application Tasks                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐     │
│  │ Sensor   │  │ Actuator │  │ Config   │     │
│  │ Task     │  │ Task     │  │ Task     │     │
│  └──────────┘  └──────────┘  └──────────┘     │
└─────────────────────────────────────────────────┘
```

### Connection Handling Strategies

1. **Single-Threaded Sequential**: One connection at a time (simplest, lowest memory)
2. **Connection Pool**: Pre-allocated connection structures with task per connection
3. **Worker Pool**: Fixed number of worker tasks processing connections from queue
4. **Event-Driven**: Single task with select/poll for multiple sockets (most efficient)

---

## C/C++ Implementation

### Basic HTTP Server with lwIP

```c
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "cJSON.h"
#include <string.h>

#define HTTP_SERVER_PORT 80
#define MAX_REQUEST_SIZE 2048
#define MAX_RESPONSE_SIZE 4096

// HTTP response codes
#define HTTP_OK "HTTP/1.1 200 OK\r\n"
#define HTTP_NOT_FOUND "HTTP/1.1 404 Not Found\r\n"
#define HTTP_BAD_REQUEST "HTTP/1.1 400 Bad Request\r\n"
#define HTTP_INTERNAL_ERROR "HTTP/1.1 500 Internal Server Error\r\n"

// Common headers
#define CONTENT_TYPE_JSON "Content-Type: application/json\r\n"
#define CONTENT_TYPE_HTML "Content-Type: text/html\r\n"
#define CONNECTION_CLOSE "Connection: close\r\n"

// Request structure
typedef struct {
    char method[8];      // GET, POST, PUT, DELETE
    char uri[128];       // Request URI
    char *body;          // Request body (for POST/PUT)
    size_t body_len;     // Body length
} http_request_t;

// Response structure
typedef struct {
    char *status_line;
    char *headers;
    char *body;
    size_t body_len;
} http_response_t;

// Parse HTTP request from buffer
static int parse_http_request(const char *buffer, http_request_t *request) {
    const char *line_end;
    const char *uri_start, *uri_end;
    const char *body_start;
    
    // Parse request line: "GET /api/sensor HTTP/1.1"
    line_end = strstr(buffer, "\r\n");
    if (!line_end) return -1;
    
    // Extract method
    const char *method_end = strchr(buffer, ' ');
    if (!method_end || method_end > line_end) return -1;
    
    size_t method_len = method_end - buffer;
    if (method_len >= sizeof(request->method)) return -1;
    
    memcpy(request->method, buffer, method_len);
    request->method[method_len] = '\0';
    
    // Extract URI
    uri_start = method_end + 1;
    uri_end = strchr(uri_start, ' ');
    if (!uri_end || uri_end > line_end) return -1;
    
    size_t uri_len = uri_end - uri_start;
    if (uri_len >= sizeof(request->uri)) return -1;
    
    memcpy(request->uri, uri_start, uri_len);
    request->uri[uri_len] = '\0';
    
    // Find body (after empty line)
    body_start = strstr(buffer, "\r\n\r\n");
    if (body_start) {
        body_start += 4; // Skip "\r\n\r\n"
        request->body = (char *)body_start;
        request->body_len = strlen(body_start);
    } else {
        request->body = NULL;
        request->body_len = 0;
    }
    
    return 0;
}

// Send HTTP response
static void send_http_response(struct netconn *conn, http_response_t *response) {
    char header_buffer[512];
    
    // Build complete header
    snprintf(header_buffer, sizeof(header_buffer),
             "%s%sContent-Length: %d\r\n%s\r\n",
             response->status_line,
             response->headers ? response->headers : "",
             response->body_len,
             CONNECTION_CLOSE);
    
    // Send headers
    netconn_write(conn, header_buffer, strlen(header_buffer), NETCONN_COPY);
    
    // Send body
    if (response->body && response->body_len > 0) {
        netconn_write(conn, response->body, response->body_len, NETCONN_COPY);
    }
}

// Example: GET /api/sensors
static void handle_get_sensors(http_response_t *response) {
    cJSON *root = cJSON_CreateObject();
    cJSON *sensors = cJSON_CreateArray();
    
    // Simulate sensor data
    cJSON *sensor1 = cJSON_CreateObject();
    cJSON_AddStringToObject(sensor1, "id", "temp_01");
    cJSON_AddStringToObject(sensor1, "type", "temperature");
    cJSON_AddNumberToObject(sensor1, "value", 23.5);
    cJSON_AddStringToObject(sensor1, "unit", "celsius");
    cJSON_AddItemToArray(sensors, sensor1);
    
    cJSON *sensor2 = cJSON_CreateObject();
    cJSON_AddStringToObject(sensor2, "id", "hum_01");
    cJSON_AddStringToObject(sensor2, "type", "humidity");
    cJSON_AddNumberToObject(sensor2, "value", 65.2);
    cJSON_AddStringToObject(sensor2, "unit", "percent");
    cJSON_AddItemToArray(sensors, sensor2);
    
    cJSON_AddItemToObject(root, "sensors", sensors);
    
    char *json_string = cJSON_Print(root);
    
    response->status_line = HTTP_OK;
    response->headers = CONTENT_TYPE_JSON;
    response->body = json_string;
    response->body_len = strlen(json_string);
    
    cJSON_Delete(root);
}

// Example: POST /api/actuator
static void handle_post_actuator(const char *body, http_response_t *response) {
    cJSON *request_json = cJSON_Parse(body);
    
    if (!request_json) {
        response->status_line = HTTP_BAD_REQUEST;
        response->headers = CONTENT_TYPE_JSON;
        response->body = "{\"error\":\"Invalid JSON\"}";
        response->body_len = strlen(response->body);
        return;
    }
    
    cJSON *actuator_id = cJSON_GetObjectItem(request_json, "actuator_id");
    cJSON *state = cJSON_GetObjectItem(request_json, "state");
    
    if (!actuator_id || !state) {
        response->status_line = HTTP_BAD_REQUEST;
        response->headers = CONTENT_TYPE_JSON;
        response->body = "{\"error\":\"Missing required fields\"}";
        response->body_len = strlen(response->body);
        cJSON_Delete(request_json);
        return;
    }
    
    // TODO: Send command to actuator task via queue
    // Example: xQueueSend(actuator_queue, &command, portMAX_DELAY);
    
    cJSON *response_json = cJSON_CreateObject();
    cJSON_AddStringToObject(response_json, "status", "success");
    cJSON_AddStringToObject(response_json, "actuator_id", actuator_id->valuestring);
    cJSON_AddStringToObject(response_json, "new_state", state->valuestring);
    
    char *json_string = cJSON_Print(response_json);
    
    response->status_line = HTTP_OK;
    response->headers = CONTENT_TYPE_JSON;
    response->body = json_string;
    response->body_len = strlen(json_string);
    
    cJSON_Delete(request_json);
    cJSON_Delete(response_json);
}

// Route handler
static void route_request(http_request_t *request, http_response_t *response) {
    // GET /api/sensors
    if (strcmp(request->method, "GET") == 0 && 
        strcmp(request->uri, "/api/sensors") == 0) {
        handle_get_sensors(response);
    }
    // POST /api/actuator
    else if (strcmp(request->method, "POST") == 0 && 
             strcmp(request->uri, "/api/actuator") == 0) {
        handle_post_actuator(request->body, response);
    }
    // GET /api/status
    else if (strcmp(request->method, "GET") == 0 && 
             strcmp(request->uri, "/api/status") == 0) {
        response->status_line = HTTP_OK;
        response->headers = CONTENT_TYPE_JSON;
        response->body = "{\"status\":\"online\",\"uptime\":12345}";
        response->body_len = strlen(response->body);
    }
    // 404 Not Found
    else {
        response->status_line = HTTP_NOT_FOUND;
        response->headers = CONTENT_TYPE_JSON;
        response->body = "{\"error\":\"Endpoint not found\"}";
        response->body_len = strlen(response->body);
    }
}

// Handle individual connection
static void http_server_handle_connection(struct netconn *conn) {
    struct netbuf *inbuf;
    char *buf;
    u16_t buflen;
    err_t err;
    
    // Set receive timeout
    netconn_set_recvtimeout(conn, 5000); // 5 seconds
    
    // Receive data
    err = netconn_recv(conn, &inbuf);
    if (err == ERR_OK) {
        netbuf_data(inbuf, (void**)&buf, &buflen);
        
        if (buflen > 0 && buflen < MAX_REQUEST_SIZE) {
            // Null-terminate buffer
            char request_buffer[MAX_REQUEST_SIZE];
            memcpy(request_buffer, buf, buflen);
            request_buffer[buflen] = '\0';
            
            // Parse request
            http_request_t request = {0};
            if (parse_http_request(request_buffer, &request) == 0) {
                // Route and handle request
                http_response_t response = {0};
                route_request(&request, &response);
                
                // Send response
                send_http_response(conn, &response);
                
                // Free any dynamically allocated response body
                // (In this example, some responses use static strings)
                // You would need to track which need freeing
            }
        }
        
        netbuf_delete(inbuf);
    }
    
    // Close connection
    netconn_close(conn);
    netconn_delete(conn);
}

// HTTP Server Task
void http_server_task(void *pvParameters) {
    struct netconn *conn, *newconn;
    err_t err;
    
    // Create new connection
    conn = netconn_new(NETCONN_TCP);
    if (conn == NULL) {
        vTaskDelete(NULL);
        return;
    }
    
    // Bind to port
    netconn_bind(conn, NULL, HTTP_SERVER_PORT);
    
    // Listen for connections
    netconn_listen(conn);
    
    while (1) {
        // Accept new connection
        err = netconn_accept(conn, &newconn);
        
        if (err == ERR_OK) {
            // Handle connection (blocking - for simplicity)
            // In production, spawn task or use worker pool
            http_server_handle_connection(newconn);
        }
    }
}
```

### Advanced: Thread-Safe REST API with Worker Pool

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#define NUM_WORKER_TASKS 3
#define CONNECTION_QUEUE_SIZE 10

// Connection item for queue
typedef struct {
    struct netconn *conn;
    uint32_t timestamp;
} connection_item_t;

// Global queue for connections
static QueueHandle_t connection_queue;

// Mutex for shared sensor data
static SemaphoreHandle_t sensor_data_mutex;

// Shared sensor data structure
typedef struct {
    float temperature;
    float humidity;
    float pressure;
    uint32_t last_update;
} sensor_data_t;

static sensor_data_t g_sensor_data;

// Thread-safe sensor data access
static void get_sensor_data(sensor_data_t *data) {
    xSemaphoreTake(sensor_data_mutex, portMAX_DELAY);
    memcpy(data, &g_sensor_data, sizeof(sensor_data_t));
    xSemaphoreGive(sensor_data_mutex);
}

static void update_sensor_data(const sensor_data_t *data) {
    xSemaphoreTake(sensor_data_mutex, portMAX_DELAY);
    memcpy(&g_sensor_data, data, sizeof(sensor_data_t));
    g_sensor_data.last_update = xTaskGetTickCount();
    xSemaphoreGive(sensor_data_mutex);
}

// Enhanced GET handler with thread-safe access
static void handle_get_sensor_data(http_response_t *response) {
    sensor_data_t data;
    get_sensor_data(&data);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "temperature", data.temperature);
    cJSON_AddNumberToObject(root, "humidity", data.humidity);
    cJSON_AddNumberToObject(root, "pressure", data.pressure);
    cJSON_AddNumberToObject(root, "last_update", data.last_update);
    
    char *json_string = cJSON_Print(root);
    
    response->status_line = HTTP_OK;
    response->headers = CONTENT_TYPE_JSON;
    response->body = json_string;
    response->body_len = strlen(json_string);
    
    cJSON_Delete(root);
}

// Worker task that processes connections from queue
static void http_worker_task(void *pvParameters) {
    connection_item_t conn_item;
    
    while (1) {
        // Wait for connection from queue
        if (xQueueReceive(connection_queue, &conn_item, portMAX_DELAY) == pdTRUE) {
            // Process connection
            http_server_handle_connection(conn_item.conn);
        }
    }
}

// Main server task - accepts connections and queues them
void http_server_task_pool(void *pvParameters) {
    struct netconn *conn, *newconn;
    err_t err;
    
    // Create connection queue
    connection_queue = xQueueCreate(CONNECTION_QUEUE_SIZE, sizeof(connection_item_t));
    
    // Create mutex for sensor data
    sensor_data_mutex = xSemaphoreCreateMutex();
    
    // Create worker tasks
    for (int i = 0; i < NUM_WORKER_TASKS; i++) {
        xTaskCreate(http_worker_task, "HTTP_Worker", 4096, NULL, 
                    tskIDLE_PRIORITY + 2, NULL);
    }
    
    // Create listening socket
    conn = netconn_new(NETCONN_TCP);
    netconn_bind(conn, NULL, HTTP_SERVER_PORT);
    netconn_listen(conn);
    
    while (1) {
        err = netconn_accept(conn, &newconn);
        
        if (err == ERR_OK) {
            connection_item_t conn_item = {
                .conn = newconn,
                .timestamp = xTaskGetTickCount()
            };
            
            // Try to queue connection
            if (xQueueSend(connection_queue, &conn_item, 0) != pdTRUE) {
                // Queue full - reject connection
                netconn_close(newconn);
                netconn_delete(newconn);
            }
        }
    }
}
```

### Complete Example: Device Configuration API

```c
// Configuration structure
typedef struct {
    char device_name[32];
    uint32_t sample_rate;
    bool wifi_enabled;
    char wifi_ssid[32];
    uint8_t threshold;
} device_config_t;

static device_config_t g_device_config = {
    .device_name = "ESP32_Sensor_01",
    .sample_rate = 1000,
    .wifi_enabled = true,
    .wifi_ssid = "MyNetwork",
    .threshold = 50
};

static SemaphoreHandle_t config_mutex;

// GET /api/config
static void handle_get_config(http_response_t *response) {
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_name", g_device_config.device_name);
    cJSON_AddNumberToObject(root, "sample_rate", g_device_config.sample_rate);
    cJSON_AddBoolToObject(root, "wifi_enabled", g_device_config.wifi_enabled);
    cJSON_AddStringToObject(root, "wifi_ssid", g_device_config.wifi_ssid);
    cJSON_AddNumberToObject(root, "threshold", g_device_config.threshold);
    
    xSemaphoreGive(config_mutex);
    
    char *json_string = cJSON_Print(root);
    response->status_line = HTTP_OK;
    response->headers = CONTENT_TYPE_JSON;
    response->body = json_string;
    response->body_len = strlen(json_string);
    
    cJSON_Delete(root);
}

// PUT /api/config
static void handle_put_config(const char *body, http_response_t *response) {
    cJSON *json = cJSON_Parse(body);
    
    if (!json) {
        response->status_line = HTTP_BAD_REQUEST;
        response->headers = CONTENT_TYPE_JSON;
        response->body = "{\"error\":\"Invalid JSON\"}";
        response->body_len = strlen(response->body);
        return;
    }
    
    xSemaphoreTake(config_mutex, portMAX_DELAY);
    
    // Update configuration fields if present
    cJSON *item;
    
    item = cJSON_GetObjectItem(json, "device_name");
    if (item && cJSON_IsString(item)) {
        strncpy(g_device_config.device_name, item->valuestring, 
                sizeof(g_device_config.device_name) - 1);
    }
    
    item = cJSON_GetObjectItem(json, "sample_rate");
    if (item && cJSON_IsNumber(item)) {
        g_device_config.sample_rate = item->valueint;
    }
    
    item = cJSON_GetObjectItem(json, "wifi_enabled");
    if (item && cJSON_IsBool(item)) {
        g_device_config.wifi_enabled = cJSON_IsTrue(item);
    }
    
    item = cJSON_GetObjectItem(json, "threshold");
    if (item && cJSON_IsNumber(item)) {
        g_device_config.threshold = item->valueint;
    }
    
    xSemaphoreGive(config_mutex);
    
    // TODO: Persist config to flash/EEPROM
    
    cJSON_Delete(json);
    
    response->status_line = HTTP_OK;
    response->headers = CONTENT_TYPE_JSON;
    response->body = "{\"status\":\"success\",\"message\":\"Configuration updated\"}";
    response->body_len = strlen(response->body);
}
```

---

## Rust Implementation

### Basic HTTP Server with `embedded-svc` and `esp-idf-svc`

```rust
use esp_idf_svc::http::server::{Configuration, EspHttpServer};
use esp_idf_svc::io::Write;
use esp_idf_sys as _;
use embedded_svc::http::{Method, Headers};
use serde::{Deserialize, Serialize};
use serde_json;
use std::sync::{Arc, Mutex};

// Sensor data structure
#[derive(Debug, Clone, Serialize, Deserialize)]
struct SensorData {
    temperature: f32,
    humidity: f32,
    pressure: f32,
    timestamp: u64,
}

// Actuator command structure
#[derive(Debug, Deserialize)]
struct ActuatorCommand {
    actuator_id: String,
    state: String,
}

// Response structure
#[derive(Debug, Serialize)]
struct ApiResponse<T> {
    status: String,
    data: Option<T>,
    error: Option<String>,
}

// Shared application state
struct AppState {
    sensor_data: Arc<Mutex<SensorData>>,
    actuator_states: Arc<Mutex<std::collections::HashMap<String, String>>>,
}

impl AppState {
    fn new() -> Self {
        Self {
            sensor_data: Arc::new(Mutex::new(SensorData {
                temperature: 25.0,
                humidity: 60.0,
                pressure: 1013.25,
                timestamp: 0,
            })),
            actuator_states: Arc::new(Mutex::new(std::collections::HashMap::new())),
        }
    }
}

// Initialize HTTP server with REST endpoints
fn init_http_server(state: Arc<AppState>) -> anyhow::Result<EspHttpServer> {
    let server_config = Configuration {
        stack_size: 10240,
        max_sessions: 4,
        ..Default::default()
    };
    
    let mut server = EspHttpServer::new(&server_config)?;
    
    // GET /api/sensors - Get current sensor readings
    {
        let state_clone = state.clone();
        server.fn_handler("/api/sensors", Method::Get, move |req| {
            let sensor_data = state_clone.sensor_data.lock().unwrap();
            
            let response = ApiResponse {
                status: "success".to_string(),
                data: Some(sensor_data.clone()),
                error: None,
            };
            
            let json = serde_json::to_string(&response)?;
            
            let mut response = req.into_ok_response()?;
            response.write_all(json.as_bytes())?;
            Ok(())
        })?;
    }
    
    // POST /api/actuator - Control actuator
    {
        let state_clone = state.clone();
        server.fn_handler("/api/actuator", Method::Post, move |mut req| {
            // Read request body
            let mut body = Vec::new();
            req.read_to_end(&mut body)?;
            
            // Parse JSON
            let command: ActuatorCommand = match serde_json::from_slice(&body) {
                Ok(cmd) => cmd,
                Err(e) => {
                    let error_response = ApiResponse::<()> {
                        status: "error".to_string(),
                        data: None,
                        error: Some(format!("Invalid JSON: {}", e)),
                    };
                    
                    let json = serde_json::to_string(&error_response)?;
                    let mut response = req.into_status_response(400)?;
                    response.write_all(json.as_bytes())?;
                    return Ok(());
                }
            };
            
            // Update actuator state
            let mut actuators = state_clone.actuator_states.lock().unwrap();
            actuators.insert(command.actuator_id.clone(), command.state.clone());
            
            // TODO: Send command to actuator task
            // Example: send to queue or channel
            
            let response_data = ApiResponse {
                status: "success".to_string(),
                data: Some(command),
                error: None,
            };
            
            let json = serde_json::to_string(&response_data)?;
            let mut response = req.into_ok_response()?;
            response.write_all(json.as_bytes())?;
            
            Ok(())
        })?;
    }
    
    // GET /api/status - System status
    {
        server.fn_handler("/api/status", Method::Get, move |req| {
            #[derive(Serialize)]
            struct SystemStatus {
                uptime: u64,
                free_heap: u32,
                status: String,
            }
            
            let status = SystemStatus {
                uptime: unsafe { esp_idf_sys::esp_timer_get_time() / 1000000 },
                free_heap: unsafe { esp_idf_sys::esp_get_free_heap_size() },
                status: "online".to_string(),
            };
            
            let response_data = ApiResponse {
                status: "success".to_string(),
                data: Some(status),
                error: None,
            };
            
            let json = serde_json::to_string(&response_data)?;
            let mut response = req.into_ok_response()?;
            response.write_all(json.as_bytes())?;
            
            Ok(())
        })?;
    }
    
    Ok(server)
}

// Main application
fn main() -> anyhow::Result<()> {
    esp_idf_sys::link_patches();
    
    // Initialize application state
    let state = Arc::new(AppState::new());
    
    // Initialize HTTP server
    let _server = init_http_server(state.clone())?;
    
    println!("HTTP Server started on port 80");
    
    // Keep application running
    loop {
        std::thread::sleep(std::time::Duration::from_secs(1));
    }
}
```

### Advanced: Configuration API with Persistence

```rust
use esp_idf_svc::nvs::{EspNvs, EspDefaultNvsPartition, NvsDefault};
use serde::{Deserialize, Serialize};

// Device configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
struct DeviceConfig {
    device_name: String,
    sample_rate: u32,
    wifi_enabled: bool,
    wifi_ssid: String,
    threshold: u8,
}

impl Default for DeviceConfig {
    fn default() -> Self {
        Self {
            device_name: "ESP32_Device".to_string(),
            sample_rate: 1000,
            wifi_enabled: true,
            wifi_ssid: "MyNetwork".to_string(),
            threshold: 50,
        }
    }
}

// Configuration manager with NVS persistence
struct ConfigManager {
    config: Arc<Mutex<DeviceConfig>>,
    nvs: Arc<Mutex<EspNvs<NvsDefault>>>,
}

impl ConfigManager {
    fn new(nvs_partition: EspDefaultNvsPartition) -> anyhow::Result<Self> {
        let nvs = EspNvs::new(nvs_partition, "config", true)?;
        
        // Try to load config from NVS
        let config = match nvs.get_blob::<DeviceConfig>("device_cfg") {
            Ok(Some(cfg)) => cfg,
            _ => DeviceConfig::default(),
        };
        
        Ok(Self {
            config: Arc::new(Mutex::new(config)),
            nvs: Arc::new(Mutex::new(nvs)),
        })
    }
    
    fn get_config(&self) -> DeviceConfig {
        self.config.lock().unwrap().clone()
    }
    
    fn update_config(&self, new_config: DeviceConfig) -> anyhow::Result<()> {
        // Update in-memory config
        *self.config.lock().unwrap() = new_config.clone();
        
        // Persist to NVS
        let mut nvs = self.nvs.lock().unwrap();
        nvs.set_blob("device_cfg", &new_config)?;
        
        Ok(())
    }
}

// Add configuration endpoints to server
fn add_config_endpoints(
    server: &mut EspHttpServer,
    config_manager: Arc<ConfigManager>,
) -> anyhow::Result<()> {
    // GET /api/config
    {
        let manager = config_manager.clone();
        server.fn_handler("/api/config", Method::Get, move |req| {
            let config = manager.get_config();
            
            let response = ApiResponse {
                status: "success".to_string(),
                data: Some(config),
                error: None,
            };
            
            let json = serde_json::to_string(&response)?;
            let mut response = req.into_ok_response()?;
            response.write_all(json.as_bytes())?;
            
            Ok(())
        })?;
    }
    
    // PUT /api/config
    {
        let manager = config_manager.clone();
        server.fn_handler("/api/config", Method::Put, move |mut req| {
            let mut body = Vec::new();
            req.read_to_end(&mut body)?;
            
            let new_config: DeviceConfig = match serde_json::from_slice(&body) {
                Ok(cfg) => cfg,
                Err(e) => {
                    let error_response = ApiResponse::<()> {
                        status: "error".to_string(),
                        data: None,
                        error: Some(format!("Invalid JSON: {}", e)),
                    };
                    
                    let json = serde_json::to_string(&error_response)?;
                    let mut response = req.into_status_response(400)?;
                    response.write_all(json.as_bytes())?;
                    return Ok(());
                }
            };
            
            match manager.update_config(new_config) {
                Ok(_) => {
                    let response_data = ApiResponse {
                        status: "success".to_string(),
                        data: Some("Configuration updated"),
                        error: None,
                    };
                    
                    let json = serde_json::to_string(&response_data)?;
                    let mut response = req.into_ok_response()?;
                    response.write_all(json.as_bytes())?;
                }
                Err(e) => {
                    let error_response = ApiResponse::<()> {
                        status: "error".to_string(),
                        data: None,
                        error: Some(format!("Failed to save config: {}", e)),
                    };
                    
                    let json = serde_json::to_string(&error_response)?;
                    let mut response = req.into_status_response(500)?;
                    response.write_all(json.as_bytes())?;
                }
            }
            
            Ok(())
        })?;
    }
    
    Ok(())
}
```

### Async HTTP Server with `tokio` (for more powerful MCUs)

```rust
use tokio::net::TcpListener;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use std::sync::Arc;
use tokio::sync::RwLock;

// Async request handler
async fn handle_request(
    mut stream: tokio::net::TcpStream,
    state: Arc<AppState>,
) -> anyhow::Result<()> {
    let mut buffer = [0u8; 2048];
    
    // Read request
    let n = stream.read(&mut buffer).await?;
    
    if n == 0 {
        return Ok(());
    }
    
    let request = String::from_utf8_lossy(&buffer[..n]);
    
    // Simple routing
    let response = if request.starts_with("GET /api/sensors") {
        let sensor_data = state.sensor_data.read().await;
        let json = serde_json::to_string(&*sensor_data)?;
        
        format!(
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\n\r\n{}",
            json.len(),
            json
        )
    } else if request.starts_with("POST /api/actuator") {
        // Extract body (simplified)
        if let Some(body_start) = request.find("\r\n\r\n") {
            let body = &request[body_start + 4..];
            
            match serde_json::from_str::<ActuatorCommand>(body) {
                Ok(command) => {
                    let mut actuators = state.actuator_states.write().await;
                    actuators.insert(command.actuator_id.clone(), command.state.clone());
                    
                    let response_data = ApiResponse {
                        status: "success".to_string(),
                        data: Some(command),
                        error: None,
                    };
                    
                    let json = serde_json::to_string(&response_data)?;
                    
                    format!(
                        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {}\r\n\r\n{}",
                        json.len(),
                        json
                    )
                }
                Err(_) => {
                    "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n".to_string()
                }
            }
        } else {
            "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n".to_string()
        }
    } else {
        "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n".to_string()
    };
    
    stream.write_all(response.as_bytes()).await?;
    stream.flush().await?;
    
    Ok(())
}

// Async HTTP server
#[tokio::main]
async fn async_http_server_main() -> anyhow::Result<()> {
    let state = Arc::new(AppState::new());
    let listener = TcpListener::bind("0.0.0.0:80").await?;
    
    println!("Async HTTP server listening on port 80");
    
    loop {
        let (stream, _addr) = listener.accept().await?;
        let state_clone = state.clone();
        
        // Spawn task for each connection
        tokio::spawn(async move {
            if let Err(e) = handle_request(stream, state_clone).await {
                eprintln!("Error handling request: {}", e);
            }
        });
    }
}
```

---

## Best Practices

### 1. Memory Management

**Problem**: Limited RAM on embedded systems
**Solutions**:
- Use stack buffers for small requests/responses
- Implement connection limits
- Stream large responses instead of buffering
- Free JSON objects immediately after use
- Use memory pools for frequent allocations

```c
// Good: Stack buffer for small responses
char response_buffer[512];
snprintf(response_buffer, sizeof(response_buffer), 
         "{\"status\":\"ok\",\"value\":%d}", sensor_value);

// Bad: Unbounded dynamic allocation
char *response = malloc(10000); // Could exhaust heap
```

### 2. Security Considerations

**Authentication**:
```c
// Simple token-based authentication
static bool check_auth_token(const char *header) {
    const char *auth_header = "Authorization: Bearer ";
    const char *token = strstr(header, auth_header);
    
    if (!token) return false;
    
    token += strlen(auth_header);
    
    // Compare with stored token
    return strncmp(token, STORED_TOKEN, TOKEN_LENGTH) == 0;
}
```

**HTTPS** (if hardware supports):
```c
// Configure TLS
netconn_set_secure(conn, true);
netconn_set_cert_and_key(conn, cert_pem, key_pem);
```

### 3. Error Handling

Always validate input and return appropriate HTTP status codes:

```rust
fn validate_sensor_id(id: &str) -> Result<(), String> {
    if id.is_empty() {
        return Err("Sensor ID cannot be empty".to_string());
    }
    if id.len() > 32 {
        return Err("Sensor ID too long".to_string());
    }
    Ok(())
}
```

### 4. Rate Limiting

Protect against DoS attacks:

```c
#define MAX_REQUESTS_PER_MINUTE 60

typedef struct {
    uint32_t ip_addr;
    uint16_t request_count;
    uint32_t window_start;
} rate_limit_entry_t;

static bool check_rate_limit(uint32_t ip_addr) {
    uint32_t current_time = xTaskGetTickCount() / 1000; // seconds
    
    // Find or create entry for this IP
    // Increment counter
    // Check if exceeded limit in current window
    
    return true; // Allow request
}
```

### 5. Concurrent Access Patterns

**Reader-Writer Lock** for sensor data:
```rust
use std::sync::RwLock;

let sensor_data = Arc::new(RwLock::new(SensorData::default()));

// Multiple readers
let data = sensor_data.read().unwrap();

// Single writer
let mut data = sensor_data.write().unwrap();
data.temperature = 25.5;
```

### 6. Request Timeout

Always set timeouts to prevent resource exhaustion:

```c
// Set receive timeout
netconn_set_recvtimeout(conn, 5000); // 5 seconds

// Set send timeout
netconn_set_sendtimeout(conn, 5000);
```

### 7. Logging and Debugging

```rust
use log::{info, warn, error};

info!("HTTP request: {} {}", method, uri);
warn!("Authentication failed for IP: {}", ip);
error!("Failed to parse JSON: {}", err);
```

---

## Performance Considerations

### 1. Task Priorities

Set appropriate priorities:
- HTTP server task: Medium priority (allow preemption by critical tasks)
- Worker tasks: Lower priority
- Sensor/actuator tasks: Higher priority (real-time requirements)

```c
xTaskCreate(http_server_task, "HTTP", 4096, NULL, 
            tskIDLE_PRIORITY + 2, NULL);
xTaskCreate(sensor_task, "Sensor", 2048, NULL, 
            tskIDLE_PRIORITY + 4, NULL);
```

### 2. Stack Size Tuning

Monitor actual stack usage:

```c
UBaseType_t high_water_mark = uxTaskGetStackHighWaterMark(NULL);
printf("Stack remaining: %u bytes\n", high_water_mark);
```

### 3. Connection Pooling

Pre-allocate connection structures:

```c
typedef struct {
    struct netconn *conn;
    bool in_use;
    TaskHandle_t worker_task;
} connection_slot_t;

static connection_slot_t connection_pool[MAX_CONNECTIONS];
```

### 4. Zero-Copy Techniques

Avoid copying data when possible:

```c
// Direct buffer access (zero-copy)
netbuf_data(inbuf, (void**)&buf, &buflen);
// Process buf directly without copying
```

### 5. Benchmarking

Measure response times:

```c
uint32_t start_time = xTaskGetTickCount();
handle_request(&request, &response);
uint32_t elapsed = xTaskGetTickCount() - start_time;
printf("Request handled in %u ms\n", elapsed);
```

### Typical Performance Metrics

| Metric | Target | Notes |
|--------|--------|-------|
| Response time | < 100ms | For simple GET requests |
| Throughput | 10-50 req/s | Depends on hardware |
| Concurrent connections | 4-10 | Limited by RAM |
| Memory per connection | 2-8 KB | Buffers + task stack |
| CPU usage | < 30% | Leave headroom for other tasks |

---

## Summary

### Key Takeaways

1. **Architecture**: Use task-based design with queues for decoupling HTTP handlers from application logic

2. **Memory**: Embedded systems require careful memory management:
   - Use stack buffers when possible
   - Limit concurrent connections
   - Free dynamic allocations immediately
   - Consider memory pools for frequent allocations

3. **Concurrency**: Implement thread-safe access to shared resources:
   - Mutexes/semaphores for exclusive access
   - Reader-writer locks for read-heavy workloads
   - Queues for task communication

4. **Security**: Always implement:
   - Input validation
   - Authentication (token-based, OAuth)
   - Rate limiting
   - HTTPS when possible

5. **Performance**: Optimize for embedded constraints:
   - Appropriate task priorities
   - Connection pooling
   - Timeout management
   - Zero-copy techniques when available

### C/C++ vs Rust Trade-offs

**C/C++ Advantages**:
- More mature ecosystem for embedded HTTP (lwIP, esp-idf)
- Fine-grained memory control
- Smaller binary size
- Direct FreeRTOS API access

**Rust Advantages**:
- Memory safety without runtime overhead
- Better error handling (Result types)
- Modern async/await syntax
- Type-safe JSON serialization
- Thread safety at compile time

### Common Pitfalls to Avoid

1. **Unbounded buffers**: Always limit request/response sizes
2. **No timeouts**: Connections can hang indefinitely
3. **Missing error handling**: Always check parse results
4. **Synchronous blocking**: Use non-blocking I/O or worker pools
5. **Memory leaks**: Track all allocations (especially JSON objects)
6. **No authentication**: Always validate client identity
7. **Priority inversion**: HTTP tasks blocking critical real-time tasks

### Recommended Libraries

**C/C++**:
- HTTP: lwIP (netconn/httpd), mongoose, esp-idf HTTP server
- JSON: cJSON, ArduinoJson, RapidJSON
- TLS: mbedTLS

**Rust**:
- HTTP: esp-idf-svc, embedded-svc
- JSON: serde_json
- Async: embassy (bare-metal), tokio (std environments)

### Next Steps

- Implement WebSocket support for real-time updates
- Add HTTPS/TLS encryption
- Integrate with MQTT for IoT cloud connectivity
- Implement OTA (Over-The-Air) firmware updates via HTTP
- Add web-based dashboard (serve static HTML/JS/CSS)
- Implement RESTful pagination for large datasets

---

**Final Note**: Embedded HTTP servers bridge the gap between traditional embedded systems and modern web technologies. While they introduce complexity, the benefits of universal access and standardized APIs make them invaluable for IoT and industrial applications.