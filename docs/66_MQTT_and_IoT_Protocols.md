# MQTT and IoT Protocols in FreeRTOS

## Detailed Description

**MQTT (Message Queuing Telemetry Transport)** is a lightweight publish/subscribe messaging protocol designed for constrained devices and low-bandwidth, high-latency, or unreliable networks. It's the de facto standard for IoT communication due to its minimal overhead and efficient use of network resources.

### Key Concepts

**1. Publish/Subscribe Architecture**
- **Publishers** send messages to specific topics
- **Subscribers** receive messages from topics they've subscribed to
- **Broker** acts as intermediary, routing messages between publishers and subscribers
- Decouples senders from receivers

**2. Quality of Service (QoS) Levels**
- **QoS 0**: At most once (fire and forget)
- **QoS 1**: At least once (acknowledged delivery)
- **QoS 2**: Exactly once (assured delivery)

**3. Key Features**
- Small code footprint (~30KB)
- Minimal network bandwidth usage
- Session persistence
- Last Will and Testament (LWT) for ungraceful disconnections
- Retained messages for latest state

**4. FreeRTOS Integration**
FreeRTOS provides ideal infrastructure for MQTT implementations:
- Tasks for connection management, publish, and subscribe handlers
- Queues for message buffering
- Semaphores for thread-safe operations
- Timers for keepalive and reconnection logic

---

## C/C++ Implementation Examples

### Example 1: Basic MQTT Client with FreeRTOS

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "mqtt_client.h"
#include <string.h>

// MQTT Configuration
#define MQTT_BROKER_URL "mqtt://broker.hivemq.com"
#define MQTT_BROKER_PORT 1883
#define MQTT_CLIENT_ID "freertos_device_001"
#define MQTT_KEEPALIVE_INTERVAL 60

// Topic definitions
#define TOPIC_TELEMETRY "devices/freertos001/telemetry"
#define TOPIC_COMMAND "devices/freertos001/command"
#define TOPIC_STATUS "devices/freertos001/status"

// Task priorities
#define MQTT_TASK_PRIORITY (tskIDLE_PRIORITY + 3)
#define PUBLISH_TASK_PRIORITY (tskIDLE_PRIORITY + 2)

// Global handles
static QueueHandle_t xMqttMessageQueue;
static SemaphoreHandle_t xMqttMutex;
static esp_mqtt_client_handle_t client;

// Message structure for queue
typedef struct {
    char topic[128];
    char payload[256];
    int qos;
} MqttMessage_t;

// MQTT Event Handler
static void mqtt_event_handler(void *handler_args, 
                               esp_event_base_t base,
                               int32_t event_id, 
                               void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            printf("MQTT Connected to broker\n");
            
            // Subscribe to command topic
            int msg_id = esp_mqtt_client_subscribe(client, 
                                                   TOPIC_COMMAND, 
                                                   1);
            printf("Subscribed to %s, msg_id=%d\n", TOPIC_COMMAND, msg_id);
            
            // Publish online status
            esp_mqtt_client_publish(client, 
                                   TOPIC_STATUS, 
                                   "online", 
                                   0, 
                                   1, 
                                   1); // retained
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            printf("MQTT Disconnected\n");
            break;
            
        case MQTT_EVENT_SUBSCRIBED:
            printf("MQTT_EVENT_SUBSCRIBED, msg_id=%d\n", event->msg_id);
            break;
            
        case MQTT_EVENT_DATA:
            printf("MQTT_EVENT_DATA\n");
            printf("Topic: %.*s\n", event->topic_len, event->topic);
            printf("Data: %.*s\n", event->data_len, event->data);
            
            // Handle incoming command
            if (strncmp(event->topic, TOPIC_COMMAND, event->topic_len) == 0) {
                handle_command(event->data, event->data_len);
            }
            break;
            
        case MQTT_EVENT_ERROR:
            printf("MQTT_EVENT_ERROR\n");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                printf("Last error code: 0x%x\n", 
                       event->error_handle->esp_transport_sock_errno);
            }
            break;
            
        default:
            break;
    }
}

// Command handler function
static void handle_command(const char *data, int len)
{
    char command[256];
    snprintf(command, sizeof(command), "%.*s", len, data);
    
    if (strcmp(command, "REBOOT") == 0) {
        printf("Reboot command received\n");
        esp_restart();
    }
    else if (strncmp(command, "SET_INTERVAL:", 13) == 0) {
        int interval = atoi(command + 13);
        printf("Setting telemetry interval to %d seconds\n", interval);
        // Update telemetry interval logic here
    }
}

// MQTT Initialization
void mqtt_app_start(void)
{
    // Create mutex for thread-safe MQTT operations
    xMqttMutex = xSemaphoreCreateMutex();
    
    // Create message queue for publish operations
    xMqttMessageQueue = xQueueCreate(10, sizeof(MqttMessage_t));
    
    // MQTT client configuration
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
        .credentials.client_id = MQTT_CLIENT_ID,
        .session.keepalive = MQTT_KEEPALIVE_INTERVAL,
        .session.last_will = {
            .topic = TOPIC_STATUS,
            .msg = "offline",
            .qos = 1,
            .retain = 1
        }
    };
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, 
                                   ESP_EVENT_ANY_ID, 
                                   mqtt_event_handler, 
                                   NULL);
    esp_mqtt_client_start(client);
}

// Publish Task - reads from queue and publishes
static void vPublishTask(void *pvParameters)
{
    MqttMessage_t message;
    
    for (;;) {
        // Wait for messages in queue
        if (xQueueReceive(xMqttMessageQueue, &message, portMAX_DELAY)) {
            
            // Take mutex for thread-safe operation
            if (xSemaphoreTake(xMqttMutex, pdMS_TO_TICKS(1000))) {
                
                int msg_id = esp_mqtt_client_publish(client,
                                                     message.topic,
                                                     message.payload,
                                                     0,
                                                     message.qos,
                                                     0);
                
                if (msg_id >= 0) {
                    printf("Published to %s: %s (msg_id=%d)\n",
                           message.topic, message.payload, msg_id);
                } else {
                    printf("Failed to publish message\n");
                }
                
                xSemaphoreGive(xMqttMutex);
            }
        }
    }
}

// Telemetry Task - publishes sensor data periodically
static void vTelemetryTask(void *pvParameters)
{
    MqttMessage_t message;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10000); // 10 seconds
    
    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        // Simulate sensor readings
        float temperature = 25.0 + (rand() % 50) / 10.0;
        float humidity = 40.0 + (rand() % 400) / 10.0;
        
        // Format JSON payload
        snprintf(message.topic, sizeof(message.topic), "%s", TOPIC_TELEMETRY);
        snprintf(message.payload, sizeof(message.payload),
                 "{\"temperature\":%.1f,\"humidity\":%.1f,\"uptime\":%lu}",
                 temperature, humidity, xTaskGetTickCount() * portTICK_PERIOD_MS);
        message.qos = 0;
        
        // Send to publish queue
        xQueueSend(xMqttMessageQueue, &message, 0);
    }
}

// Application entry point
void app_main(void)
{
    // Initialize WiFi (implementation depends on platform)
    // wifi_init();
    
    // Start MQTT client
    mqtt_app_start();
    
    // Create tasks
    xTaskCreate(vPublishTask, 
                "MQTT_Publish", 
                4096, 
                NULL, 
                PUBLISH_TASK_PRIORITY, 
                NULL);
                
    xTaskCreate(vTelemetryTask, 
                "Telemetry", 
                4096, 
                NULL, 
                tskIDLE_PRIORITY + 1, 
                NULL);
}
```

### Example 2: AWS IoT Core Integration

```c
#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "transport_secure_sockets.h"
#include "aws_iot_config.h"

// AWS IoT Configuration
#define AWS_IOT_ENDPOINT "your-endpoint.iot.region.amazonaws.com"
#define AWS_IOT_PORT 8883
#define THING_NAME "MyFreeRTOSDevice"

// Certificate and key buffers (load from secure storage)
extern const char root_ca_pem[];
extern const char certificate_pem[];
extern const char private_key_pem[];

// Shadow topics
#define SHADOW_UPDATE_TOPIC "$aws/things/" THING_NAME "/shadow/update"
#define SHADOW_UPDATE_ACCEPTED "$aws/things/" THING_NAME "/shadow/update/accepted"
#define SHADOW_UPDATE_REJECTED "$aws/things/" THING_NAME "/shadow/update/rejected"
#define SHADOW_GET_TOPIC "$aws/things/" THING_NAME "/shadow/get"

typedef struct {
    MQTTContext_t mqttContext;
    NetworkContext_t networkContext;
    MQTTFixedBuffer_t networkBuffer;
    TransportInterface_t transport;
} MQTTAgentContext_t;

static MQTTAgentContext_t xGlobalMqttAgentContext;

// TLS Connection Setup
static int32_t prvConnectToAWSIoT(NetworkContext_t *pNetworkContext)
{
    ServerInfo_t serverInfo = {0};
    SocketsConfig_t socketsConfig = {0};
    TlsTransportParams_t tlsParams = {0};
    
    // Configure server
    serverInfo.pHostName = AWS_IOT_ENDPOINT;
    serverInfo.hostNameLength = strlen(AWS_IOT_ENDPOINT);
    serverInfo.port = AWS_IOT_PORT;
    
    // Configure TLS
    tlsParams.pRootCa = root_ca_pem;
    tlsParams.rootCaSize = strlen(root_ca_pem);
    tlsParams.pClientCert = certificate_pem;
    tlsParams.clientCertSize = strlen(certificate_pem);
    tlsParams.pPrivateKey = private_key_pem;
    tlsParams.privateKeySize = strlen(private_key_pem);
    
    // Configure sockets
    socketsConfig.enableTls = true;
    socketsConfig.pAlpnProtos = NULL;
    socketsConfig.maxFragmentLength = 0;
    socketsConfig.disableSni = false;
    socketsConfig.sendTimeoutMs = 5000;
    socketsConfig.recvTimeoutMs = 5000;
    
    // Establish TLS connection
    return SecureSocketsTransport_Connect(pNetworkContext,
                                         &serverInfo,
                                         &socketsConfig,
                                         &tlsParams);
}

// MQTT Event Callback
static void prvEventCallback(MQTTContext_t *pMqttContext,
                            MQTTPacketInfo_t *pPacketInfo,
                            MQTTDeserializedInfo_t *pDeserializedInfo)
{
    uint16_t packetIdentifier = pDeserializedInfo->packetIdentifier;
    
    if (pPacketInfo->type == MQTT_PACKET_TYPE_PUBLISH) {
        MQTTPublishInfo_t *pPublishInfo = pDeserializedInfo->pPublishInfo;
        
        printf("Incoming PUBLISH on topic: %.*s\n",
               pPublishInfo->topicNameLength,
               pPublishInfo->pTopicName);
        printf("Payload: %.*s\n",
               pPublishInfo->payloadLength,
               (const char *)pPublishInfo->pPayload);
    }
    else if (pPacketInfo->type == MQTT_PACKET_TYPE_SUBACK) {
        printf("SUBACK received for packet ID %u\n", packetIdentifier);
    }
}

// Device Shadow Update
static void prvUpdateDeviceShadow(MQTTContext_t *pMqttContext)
{
    char shadowDocument[512];
    MQTTPublishInfo_t publishInfo = {0};
    
    // Create shadow document
    snprintf(shadowDocument, sizeof(shadowDocument),
             "{"
             "\"state\":{"
             "\"reported\":{"
             "\"temperature\":%.1f,"
             "\"humidity\":%.1f,"
             "\"connected\":true"
             "}"
             "}"
             "}",
             25.5, 60.2);
    
    publishInfo.qos = MQTTQoS1;
    publishInfo.pTopicName = SHADOW_UPDATE_TOPIC;
    publishInfo.topicNameLength = strlen(SHADOW_UPDATE_TOPIC);
    publishInfo.pPayload = shadowDocument;
    publishInfo.payloadLength = strlen(shadowDocument);
    
    MQTTStatus_t status = MQTT_Publish(pMqttContext, &publishInfo, 0);
    
    if (status == MQTTSuccess) {
        printf("Shadow update published\n");
    } else {
        printf("Failed to publish shadow update: %d\n", status);
    }
}

// AWS IoT MQTT Task
static void vAWSIoTTask(void *pvParameters)
{
    MQTTStatus_t mqttStatus;
    MQTTContext_t *pMqttContext = &xGlobalMqttAgentContext.mqttContext;
    NetworkContext_t *pNetworkContext = &xGlobalMqttAgentContext.networkContext;
    
    // Connect to AWS IoT
    if (prvConnectToAWSIoT(pNetworkContext) == 0) {
        printf("Connected to AWS IoT Core\n");
        
        // Initialize MQTT
        TransportInterface_t transport = {0};
        transport.pNetworkContext = pNetworkContext;
        transport.send = SecureSocketsTransport_Send;
        transport.recv = SecureSocketsTransport_Recv;
        
        MQTTFixedBuffer_t networkBuffer;
        static uint8_t buffer[2048];
        networkBuffer.pBuffer = buffer;
        networkBuffer.size = sizeof(buffer);
        
        mqttStatus = MQTT_Init(pMqttContext,
                              &transport,
                              Clock_GetTimeMs,
                              prvEventCallback,
                              &networkBuffer);
        
        if (mqttStatus == MQTTSuccess) {
            // Connect to MQTT broker
            MQTTConnectInfo_t connectInfo = {0};
            connectInfo.cleanSession = true;
            connectInfo.pClientIdentifier = THING_NAME;
            connectInfo.clientIdentifierLength = strlen(THING_NAME);
            connectInfo.keepAliveSeconds = 60;
            
            bool sessionPresent;
            mqttStatus = MQTT_Connect(pMqttContext,
                                     &connectInfo,
                                     NULL,
                                     5000,
                                     &sessionPresent);
            
            if (mqttStatus == MQTTSuccess) {
                printf("MQTT Connected to AWS IoT\n");
                
                // Subscribe to shadow topics
                MQTTSubscribeInfo_t subscriptions[2];
                subscriptions[0].qos = MQTTQoS1;
                subscriptions[0].pTopicFilter = SHADOW_UPDATE_ACCEPTED;
                subscriptions[0].topicFilterLength = strlen(SHADOW_UPDATE_ACCEPTED);
                
                subscriptions[1].qos = MQTTQoS1;
                subscriptions[1].pTopicFilter = SHADOW_UPDATE_REJECTED;
                subscriptions[1].topicFilterLength = strlen(SHADOW_UPDATE_REJECTED);
                
                MQTT_Subscribe(pMqttContext, subscriptions, 2, 1);
                
                // Main loop
                for (;;) {
                    // Update shadow every 30 seconds
                    prvUpdateDeviceShadow(pMqttContext);
                    
                    // Process incoming packets
                    MQTT_ProcessLoop(pMqttContext, 30000);
                }
            }
        }
    }
}
```

---

## Rust Implementation Examples

### Example 1: MQTT Client with FreeRTOS-Rust

```rust
use freertos_rust::{Task, Duration, Queue};
use rumqttc::{MqttOptions, Client, QoS, Event, Packet};
use std::sync::{Arc, Mutex};
use serde::{Serialize, Deserialize};

// Message structures
#[derive(Debug, Clone, Serialize, Deserialize)]
struct TelemetryData {
    temperature: f32,
    humidity: f32,
    uptime_ms: u64,
    timestamp: u64,
}

#[derive(Debug, Clone)]
struct MqttMessage {
    topic: String,
    payload: Vec<u8>,
    qos: QoS,
}

// MQTT Client Manager
struct MqttClientManager {
    client: Arc<Mutex<Client>>,
    publish_queue: Queue<MqttMessage>,
}

impl MqttClientManager {
    fn new(broker: &str, port: u16, client_id: &str) -> Self {
        let mut mqtt_options = MqttOptions::new(client_id, broker, port);
        mqtt_options
            .set_keep_alive(Duration::from_secs(60))
            .set_clean_session(true)
            .set_last_will(rumqttc::LastWill {
                topic: format!("devices/{}/status", client_id),
                message: b"offline".to_vec(),
                qos: QoS::AtLeastOnce,
                retain: true,
            });

        let (client, mut connection) = Client::new(mqtt_options, 10);
        let client = Arc::new(Mutex::new(client));
        
        let publish_queue = Queue::new(10).unwrap();

        // Spawn connection task
        let client_clone = Arc::clone(&client);
        Task::new()
            .name("mqtt_connection")
            .stack_size(4096)
            .priority(3)
            .start(move || {
                Self::connection_task(connection, client_clone);
            })
            .unwrap();

        Self {
            client,
            publish_queue,
        }
    }

    fn connection_task(
        mut connection: rumqttc::Connection,
        client: Arc<Mutex<Client>>,
    ) {
        loop {
            match connection.iter().next() {
                Some(Ok(Event::Incoming(Packet::ConnAck(_)))) => {
                    println!("MQTT Connected");
                    
                    // Subscribe to command topic
                    if let Ok(mut client) = client.lock() {
                        client
                            .subscribe("devices/+/command", QoS::AtLeastOnce)
                            .unwrap();
                        
                        // Publish online status
                        client
                            .publish(
                                "devices/freertos001/status",
                                QoS::AtLeastOnce,
                                true,
                                b"online",
                            )
                            .unwrap();
                    }
                }
                Some(Ok(Event::Incoming(Packet::Publish(publish)))) => {
                    Self::handle_message(&publish);
                }
                Some(Ok(Event::Incoming(Packet::SubAck(suback)))) => {
                    println!("Subscription acknowledged: {:?}", suback);
                }
                Some(Err(e)) => {
                    eprintln!("MQTT Connection error: {:?}", e);
                    Task::delay(Duration::from_secs(5));
                }
                _ => {}
            }
        }
    }

    fn handle_message(publish: &rumqttc::Publish) {
        println!("Received on topic '{}': {:?}", 
                 publish.topic, 
                 String::from_utf8_lossy(&publish.payload));

        // Parse and handle commands
        if publish.topic.contains("command") {
            match String::from_utf8_lossy(&publish.payload).as_ref() {
                "REBOOT" => {
                    println!("Reboot command received");
                    // Trigger reboot
                }
                cmd if cmd.starts_with("SET_INTERVAL:") => {
                    if let Ok(interval) = cmd[13..].parse::<u32>() {
                        println!("Setting interval to {} seconds", interval);
                    }
                }
                _ => println!("Unknown command"),
            }
        }
    }

    fn publish(&self, topic: String, payload: Vec<u8>, qos: QoS) {
        let message = MqttMessage { topic, payload, qos };
        self.publish_queue.send(message, Duration::ms(100)).ok();
    }

    fn start_publish_task(&self) {
        let client = Arc::clone(&self.client);
        let queue = self.publish_queue.clone();

        Task::new()
            .name("mqtt_publish")
            .stack_size(4096)
            .priority(2)
            .start(move || {
                loop {
                    if let Ok(message) = queue.receive(Duration::infinite()) {
                        if let Ok(mut client) = client.lock() {
                            match client.publish(
                                &message.topic,
                                message.qos,
                                false,
                                message.payload,
                            ) {
                                Ok(_) => println!("Published to {}", message.topic),
                                Err(e) => eprintln!("Publish error: {:?}", e),
                            }
                        }
                    }
                }
            })
            .unwrap();
    }
}

// Telemetry Publisher Task
fn telemetry_task(mqtt: Arc<Mutex<MqttClientManager>>) {
    Task::new()
        .name("telemetry")
        .stack_size(4096)
        .priority(1)
        .start(move || {
            let mut counter: u64 = 0;
            
            loop {
                // Simulate sensor readings
                let data = TelemetryData {
                    temperature: 25.0 + (counter % 10) as f32,
                    humidity: 60.0 + (counter % 20) as f32,
                    uptime_ms: freertos_rust::get_tick_count() as u64,
                    timestamp: counter,
                };

                // Serialize to JSON
                if let Ok(json) = serde_json::to_vec(&data) {
                    if let Ok(mqtt) = mqtt.lock() {
                        mqtt.publish(
                            "devices/freertos001/telemetry".to_string(),
                            json,
                            QoS::AtMostOnce,
                        );
                    }
                }

                counter += 1;
                Task::delay(Duration::from_secs(10));
            }
        })
        .unwrap();
}

// Main application entry
#[no_mangle]
pub extern "C" fn app_main() {
    // Initialize MQTT client
    let mqtt_manager = MqttClientManager::new(
        "broker.hivemq.com",
        1883,
        "freertos_rust_device_001",
    );
    
    mqtt_manager.start_publish_task();
    
    let mqtt = Arc::new(Mutex::new(mqtt_manager));
    
    // Start telemetry task
    telemetry_task(Arc::clone(&mqtt));
    
    // Keep main task alive
    loop {
        Task::delay(Duration::from_secs(1));
    }
}
```

### Example 2: AWS IoT with TLS in Rust

```rust
use freertos_rust::{Task, Duration};
use rumqttc::{MqttOptions, Client, QoS, TlsConfiguration, Transport};
use rustls::ClientConfig;
use std::sync::Arc;
use serde_json::json;

// AWS IoT Configuration
const AWS_IOT_ENDPOINT: &str = "your-endpoint.iot.us-east-1.amazonaws.com";
const AWS_IOT_PORT: u16 = 8883;
const THING_NAME: &str = "MyRustDevice";

// Shadow topics
const SHADOW_UPDATE: &str = "$aws/things/MyRustDevice/shadow/update";
const SHADOW_UPDATE_ACCEPTED: &str = "$aws/things/MyRustDevice/shadow/update/accepted";
const SHADOW_UPDATE_REJECTED: &str = "$aws/things/MyRustDevice/shadow/update/rejected";
const SHADOW_GET: &str = "$aws/things/MyRustDevice/shadow/get";

struct AWSIoTClient {
    client: Client,
}

impl AWSIoTClient {
    fn new(
        root_ca: &[u8],
        client_cert: &[u8],
        client_key: &[u8],
    ) -> Result<Self, Box<dyn std::error::Error>> {
        // Configure TLS
        let mut root_cert_store = rustls::RootCertStore::empty();
        root_cert_store.add_parsable_certificates(
            &rustls_pemfile::certs(&mut &root_ca[..])
                .collect::<Result<Vec<_>, _>>()?,
        );

        let client_certs = rustls_pemfile::certs(&mut &client_cert[..])
            .collect::<Result<Vec<_>, _>>()?;
        
        let client_key = rustls_pemfile::private_key(&mut &client_key[..])?
            .ok_or("No private key found")?;

        let client_config = ClientConfig::builder()
            .with_root_certificates(root_cert_store)
            .with_client_auth_cert(client_certs, client_key)?;

        // MQTT Options
        let mut mqtt_options = MqttOptions::new(
            THING_NAME,
            AWS_IOT_ENDPOINT,
            AWS_IOT_PORT,
        );
        
        mqtt_options
            .set_keep_alive(Duration::from_secs(60))
            .set_clean_session(true)
            .set_transport(Transport::Tls(TlsConfiguration::Rustls(
                Arc::new(client_config),
            )));

        let (client, connection) = Client::new(mqtt_options, 10);

        // Spawn connection handler
        Task::new()
            .name("aws_iot_connection")
            .stack_size(8192)
            .priority(3)
            .start(move || {
                Self::handle_connection(connection);
            })
            .unwrap();

        Ok(Self { client })
    }

    fn handle_connection(mut connection: rumqttc::Connection) {
        for notification in connection.iter() {
            match notification {
                Ok(rumqttc::Event::Incoming(rumqttc::Packet::ConnAck(_))) => {
                    println!("Connected to AWS IoT Core");
                }
                Ok(rumqttc::Event::Incoming(rumqttc::Packet::Publish(p))) => {
                    println!(
                        "Received on {}: {}",
                        p.topic,
                        String::from_utf8_lossy(&p.payload)
                    );
                    
                    Self::handle_shadow_response(&p.topic, &p.payload);
                }
                Err(e) => {
                    eprintln!("AWS IoT connection error: {:?}", e);
                    Task::delay(Duration::from_secs(5));
                }
                _ => {}
            }
        }
    }

    fn handle_shadow_response(topic: &str, payload: &[u8]) {
        if topic.contains("accepted") {
            println!("Shadow update accepted");
        } else if topic.contains("rejected") {
            eprintln!("Shadow update rejected: {}", 
                     String::from_utf8_lossy(payload));
        }
    }

    fn update_shadow(&mut self, temperature: f32, humidity: f32) {
        let shadow_doc = json!({
            "state": {
                "reported": {
                    "temperature": temperature,
                    "humidity": humidity,
                    "connected": true,
                    "timestamp": freertos_rust::get_tick_count()
                }
            }
        });

        let payload = serde_json::to_vec(&shadow_doc).unwrap();
        
        self.client
            .publish(SHADOW_UPDATE, QoS::AtLeastOnce, false, payload)
            .unwrap();
    }

    fn subscribe_to_shadow(&mut self) {
        self.client
            .subscribe(SHADOW_UPDATE_ACCEPTED, QoS::AtLeastOnce)
            .unwrap();
        
        self.client
            .subscribe(SHADOW_UPDATE_REJECTED, QoS::AtLeastOnce)
            .unwrap();
    }

    fn get_shadow(&mut self) {
        self.client
            .publish(SHADOW_GET, QoS::AtLeastOnce, false, b"")
            .unwrap();
    }
}

// Shadow update task
fn shadow_update_task(mut aws_client: AWSIoTClient) {
    Task::new()
        .name("shadow_update")
        .stack_size(8192)
        .priority(2)
        .start(move || {
            // Subscribe to shadow topics
            aws_client.subscribe_to_shadow();
            
            // Request current shadow
            Task::delay(Duration::from_secs(2));
            aws_client.get_shadow();
            
            let mut counter = 0;
            loop {
                // Update shadow with telemetry
                let temp = 20.0 + (counter as f32 * 0.5);
                let humidity = 50.0 + (counter as f32 * 0.3);
                
                aws_client.update_shadow(temp, humidity);
                
                counter = (counter + 1) % 100;
                Task::delay(Duration::from_secs(30));
            }
        })
        .unwrap();
}
```

---

## Summary

**MQTT and IoT Protocols** in FreeRTOS enable efficient, reliable machine-to-machine communication for embedded IoT devices. Key takeaways:

### Core Concepts
- **Lightweight Protocol**: Minimal overhead makes MQTT ideal for resource-constrained devices
- **Pub/Sub Pattern**: Decouples publishers from subscribers, enabling scalable architectures
- **QoS Levels**: Provides flexibility for reliability vs. performance trade-offs
- **Session Management**: Handles reconnections and message persistence

### FreeRTOS Integration
- **Task-based Architecture**: Separate tasks for connection management, publishing, and message handling
- **Queue-based Messaging**: Thread-safe message passing between tasks
- **Synchronization Primitives**: Mutexes and semaphores ensure thread safety
- **Timer Management**: Keepalive and reconnection logic using FreeRTOS timers

### AWS IoT Core
- **Secure Communication**: TLS 1.2/1.3 with mutual authentication
- **Device Shadow**: Virtual representation for device state synchronization
- **Thing Management**: Fleet management and provisioning
- **Rules Engine**: Real-time data processing and routing

### Implementation Best Practices
1. **Connection Resilience**: Implement automatic reconnection with exponential backoff
2. **Message Buffering**: Use queues to handle bursts and network interruptions
3. **Resource Management**: Monitor memory usage and task stack sizes
4. **Security**: Always use TLS for production, secure credential storage
5. **Error Handling**: Comprehensive error handling for network and protocol errors
6. **Testing**: Validate QoS behavior, connection recovery, and edge cases

### Language Comparison
- **C/C++**: Maximum control, widespread library support (ESP-IDF, AWS IoT SDK)
- **Rust**: Memory safety, zero-cost abstractions, growing ecosystem (rumqttc, embedded-hal)

MQTT remains the standard for IoT connectivity due to its efficiency, reliability, and extensive ecosystem support across platforms and programming languages.