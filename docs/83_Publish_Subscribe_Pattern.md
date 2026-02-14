# FreeRTOS Publish-Subscribe Pattern

## Overview

The **Publish-Subscribe Pattern** (Pub-Sub) is a messaging paradigm where publishers send messages without knowing who the receivers are, and subscribers receive messages based on their interests rather than directly from specific senders. In FreeRTOS, this pattern decouples tasks and enables flexible, scalable event-driven architectures.

## Core Concepts

### Key Components

1. **Publisher**: Tasks that generate events/data
2. **Subscriber**: Tasks that consume events/data
3. **Event Broker**: Middleware that routes messages (implemented using queues, event groups, or custom structures)
4. **Topics**: Categories or channels for organizing messages

### Advantages

- **Decoupling**: Publishers and subscribers don't need direct references to each other
- **Scalability**: Easy to add/remove subscribers without modifying publishers
- **Flexibility**: Dynamic subscription management at runtime
- **One-to-Many**: Single publisher can notify multiple subscribers

---

## Implementation Approaches

### 1. Queue-Based Publish-Subscribe (C/C++)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>

// Maximum subscribers per topic
#define MAX_SUBSCRIBERS 10
#define MAX_TOPICS 5
#define MESSAGE_QUEUE_SIZE 10

// Event message structure
typedef struct {
    char topic[32];
    uint8_t data[64];
    size_t dataLen;
} EventMessage_t;

// Subscriber structure
typedef struct {
    QueueHandle_t queue;
    char topic[32];
    TaskHandle_t taskHandle;
} Subscriber_t;

// Topic broker structure
typedef struct {
    char name[32];
    Subscriber_t subscribers[MAX_SUBSCRIBERS];
    uint8_t subscriberCount;
} Topic_t;

// Global topic registry
static Topic_t topics[MAX_TOPICS];
static uint8_t topicCount = 0;
static SemaphoreHandle_t registryMutex;

// Initialize the pub-sub system
void PubSub_Init(void) {
    registryMutex = xSemaphoreCreateMutex();
    topicCount = 0;
    memset(topics, 0, sizeof(topics));
}

// Find or create a topic
static Topic_t* FindOrCreateTopic(const char* topicName) {
    // Find existing topic
    for (uint8_t i = 0; i < topicCount; i++) {
        if (strcmp(topics[i].name, topicName) == 0) {
            return &topics[i];
        }
    }
    
    // Create new topic
    if (topicCount < MAX_TOPICS) {
        strncpy(topics[topicCount].name, topicName, 31);
        topics[topicCount].subscriberCount = 0;
        return &topics[topicCount++];
    }
    
    return NULL;
}

// Subscribe to a topic
BaseType_t PubSub_Subscribe(const char* topicName, QueueHandle_t queue, 
                           TaskHandle_t taskHandle) {
    BaseType_t result = pdFALSE;
    
    if (xSemaphoreTake(registryMutex, portMAX_DELAY) == pdTRUE) {
        Topic_t* topic = FindOrCreateTopic(topicName);
        
        if (topic && topic->subscriberCount < MAX_SUBSCRIBERS) {
            Subscriber_t* sub = &topic->subscribers[topic->subscriberCount];
            sub->queue = queue;
            sub->taskHandle = taskHandle;
            strncpy(sub->topic, topicName, 31);
            topic->subscriberCount++;
            result = pdTRUE;
        }
        
        xSemaphoreGive(registryMutex);
    }
    
    return result;
}

// Unsubscribe from a topic
BaseType_t PubSub_Unsubscribe(const char* topicName, TaskHandle_t taskHandle) {
    BaseType_t result = pdFALSE;
    
    if (xSemaphoreTake(registryMutex, portMAX_DELAY) == pdTRUE) {
        for (uint8_t i = 0; i < topicCount; i++) {
            if (strcmp(topics[i].name, topicName) == 0) {
                Topic_t* topic = &topics[i];
                
                for (uint8_t j = 0; j < topic->subscriberCount; j++) {
                    if (topic->subscribers[j].taskHandle == taskHandle) {
                        // Remove subscriber by shifting array
                        for (uint8_t k = j; k < topic->subscriberCount - 1; k++) {
                            topic->subscribers[k] = topic->subscribers[k + 1];
                        }
                        topic->subscriberCount--;
                        result = pdTRUE;
                        break;
                    }
                }
                break;
            }
        }
        
        xSemaphoreGive(registryMutex);
    }
    
    return result;
}

// Publish a message to a topic
BaseType_t PubSub_Publish(const char* topicName, const uint8_t* data, 
                         size_t dataLen) {
    BaseType_t result = pdFALSE;
    
    if (xSemaphoreTake(registryMutex, portMAX_DELAY) == pdTRUE) {
        for (uint8_t i = 0; i < topicCount; i++) {
            if (strcmp(topics[i].name, topicName) == 0) {
                Topic_t* topic = &topics[i];
                
                // Create event message
                EventMessage_t msg;
                strncpy(msg.topic, topicName, 31);
                msg.dataLen = (dataLen < 64) ? dataLen : 64;
                memcpy(msg.data, data, msg.dataLen);
                
                // Send to all subscribers
                for (uint8_t j = 0; j < topic->subscriberCount; j++) {
                    xQueueSend(topic->subscribers[j].queue, &msg, 0);
                }
                
                result = pdTRUE;
                break;
            }
        }
        
        xSemaphoreGive(registryMutex);
    }
    
    return result;
}

// Example: Temperature Publisher Task
void TemperaturePublisherTask(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint16_t temperature;
    
    while (1) {
        // Simulate temperature reading
        temperature = (rand() % 100) + 200; // 20.0 to 30.0 degrees
        
        // Publish to "temperature" topic
        PubSub_Publish("temperature", (uint8_t*)&temperature, 
                      sizeof(temperature));
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
    }
}

// Example: Display Subscriber Task
void DisplaySubscriberTask(void* pvParameters) {
    QueueHandle_t displayQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, 
                                               sizeof(EventMessage_t));
    EventMessage_t msg;
    
    // Subscribe to temperature topic
    PubSub_Subscribe("temperature", displayQueue, xTaskGetCurrentTaskHandle());
    
    while (1) {
        if (xQueueReceive(displayQueue, &msg, portMAX_DELAY) == pdTRUE) {
            if (strcmp(msg.topic, "temperature") == 0) {
                uint16_t temp = *(uint16_t*)msg.data;
                printf("Display: Temperature = %d.%d C\n", 
                       temp / 10, temp % 10);
            }
        }
    }
}

// Example: Logging Subscriber Task
void LoggingSubscriberTask(void* pvParameters) {
    QueueHandle_t logQueue = xQueueCreate(MESSAGE_QUEUE_SIZE, 
                                          sizeof(EventMessage_t));
    EventMessage_t msg;
    
    // Subscribe to multiple topics
    PubSub_Subscribe("temperature", logQueue, xTaskGetCurrentTaskHandle());
    PubSub_Subscribe("pressure", logQueue, xTaskGetCurrentTaskHandle());
    PubSub_Subscribe("humidity", logQueue, xTaskGetCurrentTaskHandle());
    
    while (1) {
        if (xQueueReceive(logQueue, &msg, portMAX_DELAY) == pdTRUE) {
            printf("LOG [%s]: Data received (%d bytes)\n", 
                   msg.topic, msg.dataLen);
        }
    }
}
```

### 2. Event Group-Based Publish-Subscribe (C/C++)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

// Event bits for different topics
#define EVENT_TEMPERATURE   (1 << 0)
#define EVENT_PRESSURE      (1 << 1)
#define EVENT_HUMIDITY      (1 << 2)
#define EVENT_MOTION        (1 << 3)
#define EVENT_ALARM         (1 << 4)

// Shared event group
static EventGroupHandle_t sensorEventGroup;

// Shared data structure (protected by mutex)
typedef struct {
    float temperature;
    float pressure;
    float humidity;
    bool motionDetected;
} SensorData_t;

static SensorData_t sharedSensorData;
static SemaphoreHandle_t dataMutex;

void EventBasedPubSub_Init(void) {
    sensorEventGroup = xEventGroupCreate();
    dataMutex = xSemaphoreCreateMutex();
}

// Publisher: Temperature Sensor
void TemperatureSensorTask(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    while (1) {
        float temp = 25.0f + ((rand() % 100) / 10.0f);
        
        // Update shared data
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            sharedSensorData.temperature = temp;
            xSemaphoreGive(dataMutex);
        }
        
        // Notify subscribers
        xEventGroupSetBits(sensorEventGroup, EVENT_TEMPERATURE);
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
    }
}

// Subscriber: Display Task (waits for multiple events)
void DisplayTask(void* pvParameters) {
    EventBits_t uxBits;
    const EventBits_t uxBitsToWaitFor = EVENT_TEMPERATURE | EVENT_PRESSURE;
    
    while (1) {
        // Wait for any sensor event
        uxBits = xEventGroupWaitBits(
            sensorEventGroup,
            uxBitsToWaitFor,
            pdTRUE,  // Clear bits on exit
            pdFALSE, // Wait for any bit (OR)
            portMAX_DELAY
        );
        
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            if (uxBits & EVENT_TEMPERATURE) {
                printf("Display: Temp = %.1f C\n", 
                       sharedSensorData.temperature);
            }
            
            if (uxBits & EVENT_PRESSURE) {
                printf("Display: Pressure = %.1f hPa\n", 
                       sharedSensorData.pressure);
            }
            
            xSemaphoreGive(dataMutex);
        }
    }
}

// Subscriber: Alarm Task (waits for specific condition)
void AlarmTask(void* pvParameters) {
    EventBits_t uxBits;
    
    while (1) {
        // Wait for motion or alarm events
        uxBits = xEventGroupWaitBits(
            sensorEventGroup,
            EVENT_MOTION | EVENT_ALARM,
            pdTRUE,
            pdFALSE,
            portMAX_DELAY
        );
        
        if (uxBits & EVENT_MOTION) {
            printf("ALARM: Motion detected!\n");
        }
        
        if (uxBits & EVENT_ALARM) {
            printf("ALARM: Critical condition!\n");
        }
    }
}
```

### 3. Advanced Topic-Based Routing with Wildcards (C++)

```cpp
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string>
#include <vector>
#include <memory>

class Message {
public:
    std::string topic;
    std::vector<uint8_t> payload;
    uint32_t timestamp;
    
    Message(const std::string& t, const std::vector<uint8_t>& p) 
        : topic(t), payload(p), timestamp(xTaskGetTickCount()) {}
};

class Subscriber {
public:
    virtual void onMessage(const Message& msg) = 0;
    virtual ~Subscriber() = default;
};

class TopicMatcher {
public:
    static bool matches(const std::string& pattern, const std::string& topic) {
        // Simple wildcard matching
        // "sensor/+" matches "sensor/temp" but not "sensor/temp/room1"
        // "sensor/#" matches "sensor/temp/room1" and all sub-topics
        
        size_t patPos = 0, topPos = 0;
        
        while (patPos < pattern.length() && topPos < topic.length()) {
            if (pattern[patPos] == '#') {
                return true; // Match everything from here
            }
            else if (pattern[patPos] == '+') {
                // Match one level
                while (topPos < topic.length() && topic[topPos] != '/') {
                    topPos++;
                }
                patPos++;
                if (patPos < pattern.length() && pattern[patPos] == '/') {
                    patPos++;
                    topPos++;
                }
            }
            else if (pattern[patPos] == topic[topPos]) {
                patPos++;
                topPos++;
            }
            else {
                return false;
            }
        }
        
        return patPos == pattern.length() && topPos == topic.length();
    }
};

class MessageBroker {
private:
    struct Subscription {
        std::string pattern;
        Subscriber* subscriber;
        QueueHandle_t queue;
    };
    
    std::vector<Subscription> subscriptions;
    SemaphoreHandle_t mutex;
    
public:
    MessageBroker() {
        mutex = xSemaphoreCreateMutex();
    }
    
    void subscribe(const std::string& pattern, Subscriber* subscriber) {
        QueueHandle_t queue = xQueueCreate(10, sizeof(Message*));
        
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            subscriptions.push_back({pattern, subscriber, queue});
            xSemaphoreGive(mutex);
        }
        
        // Start subscriber task
        xTaskCreate([](void* param) {
            auto* sub = static_cast<Subscription*>(param);
            Message* msg;
            
            while (true) {
                if (xQueueReceive(sub->queue, &msg, portMAX_DELAY) == pdTRUE) {
                    sub->subscriber->onMessage(*msg);
                    delete msg;
                }
            }
        }, "Subscriber", 2048, &subscriptions.back(), 1, nullptr);
    }
    
    void publish(const std::string& topic, const std::vector<uint8_t>& payload) {
        Message* msg = new Message(topic, payload);
        
        if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
            for (auto& sub : subscriptions) {
                if (TopicMatcher::matches(sub.pattern, topic)) {
                    Message* msgCopy = new Message(*msg);
                    xQueueSend(sub.queue, &msgCopy, 0);
                }
            }
            xSemaphoreGive(mutex);
        }
        
        delete msg;
    }
};

// Example usage
class TemperatureSubscriber : public Subscriber {
public:
    void onMessage(const Message& msg) override {
        if (msg.payload.size() >= sizeof(float)) {
            float temp = *reinterpret_cast<const float*>(msg.payload.data());
            printf("Temp from %s: %.1f C\n", msg.topic.c_str(), temp);
        }
    }
};

void ExampleCppTask(void* pvParameters) {
    MessageBroker broker;
    TemperatureSubscriber tempSub;
    
    // Subscribe to all temperature sensors
    broker.subscribe("sensor/temperature/+", &tempSub);
    
    // Publish messages
    float temp1 = 25.5f;
    broker.publish("sensor/temperature/room1", 
                  std::vector<uint8_t>((uint8_t*)&temp1, 
                                       (uint8_t*)&temp1 + sizeof(float)));
    
    vTaskDelete(NULL);
}
```

### 4. Rust Implementation with FreeRTOS Bindings

```rust
// Using freertos-rust crate
use freertos_rust::*;
use alloc::string::String;
use alloc::vec::Vec;
use alloc::sync::Arc;
use core::sync::atomic::{AtomicBool, Ordering};

#[derive(Clone, Debug)]
struct Message {
    topic: String,
    payload: Vec<u8>,
    timestamp: Duration,
}

trait Subscriber: Send {
    fn on_message(&mut self, msg: &Message);
}

struct Subscription {
    pattern: String,
    queue: Queue<Message>,
}

struct MessageBroker {
    subscriptions: Arc<Mutex<Vec<Subscription>>>,
}

impl MessageBroker {
    fn new() -> Self {
        Self {
            subscriptions: Arc::new(Mutex::new(Vec::new()).unwrap()),
        }
    }
    
    fn subscribe(&self, pattern: String, queue: Queue<Message>) {
        let mut subs = self.subscriptions.lock(Duration::infinite()).unwrap();
        subs.push(Subscription { pattern, queue });
    }
    
    fn publish(&self, topic: String, payload: Vec<u8>) {
        let msg = Message {
            topic: topic.clone(),
            payload,
            timestamp: CurrentTask::get_tick_count(),
        };
        
        let subs = self.subscriptions.lock(Duration::infinite()).unwrap();
        
        for sub in subs.iter() {
            if Self::topic_matches(&sub.pattern, &topic) {
                let _ = sub.queue.send(msg.clone(), Duration::ms(0));
            }
        }
    }
    
    fn topic_matches(pattern: &str, topic: &str) -> bool {
        // Simple wildcard matching
        if pattern == "#" {
            return true;
        }
        
        let pat_parts: Vec<&str> = pattern.split('/').collect();
        let top_parts: Vec<&str> = topic.split('/').collect();
        
        if pat_parts.len() > top_parts.len() {
            return false;
        }
        
        for (i, pat) in pat_parts.iter().enumerate() {
            if *pat == "#" {
                return true;
            } else if *pat == "+" {
                continue;
            } else if i >= top_parts.len() || *pat != top_parts[i] {
                return false;
            }
        }
        
        pat_parts.len() == top_parts.len()
    }
}

// Temperature Publisher Task
fn temperature_publisher_task(broker: Arc<MessageBroker>) {
    Task::new()
        .name("TempPublisher")
        .stack_size(2048)
        .priority(TaskPriority(2))
        .start(move || {
            let mut temp: f32 = 20.0;
            
            loop {
                temp += 0.1;
                if temp > 30.0 {
                    temp = 20.0;
                }
                
                let payload = temp.to_le_bytes().to_vec();
                broker.publish(
                    "sensor/temperature/room1".to_string(),
                    payload
                );
                
                CurrentTask::delay(Duration::ms(1000));
            }
        })
        .unwrap();
}

// Display Subscriber Task
fn display_subscriber_task(broker: Arc<MessageBroker>) {
    let queue = Queue::new(10).unwrap();
    broker.subscribe("sensor/temperature/+".to_string(), queue.clone());
    
    Task::new()
        .name("Display")
        .stack_size(2048)
        .priority(TaskPriority(1))
        .start(move || {
            loop {
                if let Some(msg) = queue.receive(Duration::infinite()) {
                    if msg.payload.len() >= 4 {
                        let temp = f32::from_le_bytes([
                            msg.payload[0],
                            msg.payload[1],
                            msg.payload[2],
                            msg.payload[3],
                        ]);
                        println!("Display: {} = {:.1}°C", msg.topic, temp);
                    }
                }
            }
        })
        .unwrap();
}

// Logger Subscriber Task (subscribes to all topics)
fn logger_subscriber_task(broker: Arc<MessageBroker>) {
    let queue = Queue::new(20).unwrap();
    broker.subscribe("#".to_string(), queue.clone());
    
    Task::new()
        .name("Logger")
        .stack_size(2048)
        .priority(TaskPriority(1))
        .start(move || {
            loop {
                if let Some(msg) = queue.receive(Duration::infinite()) {
                    println!("LOG [{}]: {} bytes @ {}ms",
                             msg.topic,
                             msg.payload.len(),
                             msg.timestamp.to_ms());
                }
            }
        })
        .unwrap();
}

// Main application
pub fn start_pubsub_system() {
    let broker = Arc::new(MessageBroker::new());
    
    temperature_publisher_task(broker.clone());
    display_subscriber_task(broker.clone());
    logger_subscriber_task(broker.clone());
    
    FreeRtosUtils::start_scheduler();
}
```

### 5. Dynamic Subscriber Management (C)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Subscriber callback function type
typedef void (*SubscriberCallback_t)(const char* topic, const uint8_t* data, 
                                     size_t len, void* context);

typedef struct {
    char topicPattern[32];
    SubscriberCallback_t callback;
    void* context;
    bool active;
    TaskHandle_t taskHandle;
} DynamicSubscriber_t;

#define MAX_DYNAMIC_SUBSCRIBERS 20
static DynamicSubscriber_t dynamicSubscribers[MAX_DYNAMIC_SUBSCRIBERS];
static SemaphoreHandle_t subscriberMutex;

void DynamicPubSub_Init(void) {
    subscriberMutex = xSemaphoreCreateMutex();
    memset(dynamicSubscribers, 0, sizeof(dynamicSubscribers));
}

// Register a callback-based subscriber
uint8_t DynamicPubSub_Register(const char* pattern, 
                               SubscriberCallback_t callback,
                               void* context) {
    uint8_t handle = 0xFF;
    
    if (xSemaphoreTake(subscriberMutex, portMAX_DELAY) == pdTRUE) {
        for (uint8_t i = 0; i < MAX_DYNAMIC_SUBSCRIBERS; i++) {
            if (!dynamicSubscribers[i].active) {
                strncpy(dynamicSubscribers[i].topicPattern, pattern, 31);
                dynamicSubscribers[i].callback = callback;
                dynamicSubscribers[i].context = context;
                dynamicSubscribers[i].active = true;
                dynamicSubscribers[i].taskHandle = xTaskGetCurrentTaskHandle();
                handle = i;
                break;
            }
        }
        xSemaphoreGive(subscriberMutex);
    }
    
    return handle;
}

// Unregister a subscriber
void DynamicPubSub_Unregister(uint8_t handle) {
    if (handle < MAX_DYNAMIC_SUBSCRIBERS) {
        if (xSemaphoreTake(subscriberMutex, portMAX_DELAY) == pdTRUE) {
            dynamicSubscribers[handle].active = false;
            xSemaphoreGive(subscriberMutex);
        }
    }
}

// Publish to dynamic subscribers
void DynamicPubSub_Publish(const char* topic, const uint8_t* data, size_t len) {
    if (xSemaphoreTake(subscriberMutex, portMAX_DELAY) == pdTRUE) {
        for (uint8_t i = 0; i < MAX_DYNAMIC_SUBSCRIBERS; i++) {
            if (dynamicSubscribers[i].active) {
                // Simple pattern match (could be enhanced with wildcards)
                if (strstr(topic, dynamicSubscribers[i].topicPattern) != NULL) {
                    dynamicSubscribers[i].callback(topic, data, len, 
                                                  dynamicSubscribers[i].context);
                }
            }
        }
        xSemaphoreGive(subscriberMutex);
    }
}

// Example callback function
void TemperatureAlertCallback(const char* topic, const uint8_t* data, 
                              size_t len, void* context) {
    if (len >= sizeof(float)) {
        float* threshold = (float*)context;
        float temp = *(float*)data;
        
        if (temp > *threshold) {
            printf("ALERT: Temperature %.1f exceeds threshold %.1f!\n", 
                   temp, *threshold);
        }
    }
}

// Usage example
void DynamicSubscriberExample(void) {
    static float alertThreshold = 28.0f;
    
    uint8_t handle = DynamicPubSub_Register("temperature", 
                                            TemperatureAlertCallback,
                                            &alertThreshold);
    
    // Later, can unregister
    // DynamicPubSub_Unregister(handle);
}
```

---

## Summary

The **Publish-Subscribe Pattern** in FreeRTOS provides a powerful mechanism for building event-driven, loosely-coupled embedded systems. Key takeaways:

### Implementation Strategies

1. **Queue-Based**: Best for reliable message delivery with buffering, suitable when subscribers need guaranteed message reception
2. **Event Group-Based**: Ideal for lightweight signaling where data is shared separately, minimal memory overhead
3. **Hybrid Approach**: Combines events for notification with shared memory or queues for data transfer

### Best Practices

- **Use mutexes** to protect shared subscriber registries and topic lists
- **Implement topic wildcards** (+ for single level, # for multi-level) for flexible subscriptions
- **Consider memory constraints**: Event groups use less RAM than queue-per-subscriber approaches
- **Dynamic management**: Allow runtime subscription/unsubscription for flexible system reconfiguration
- **Callback vs. Queue**: Callbacks execute in publisher context (faster but blocking), queues provide task isolation

### Common Use Cases

- **Sensor networks**: Multiple tasks interested in sensor data
- **Logging systems**: Centralized logger subscribing to all events
- **Alarm systems**: Multiple conditions triggering various responses
- **UI updates**: Display tasks subscribing to data changes
- **Data distribution**: One producer, multiple consumers with different processing requirements

### Performance Considerations

- Queue-based: Higher memory usage, better isolation, non-blocking publishers
- Event-based: Lower memory, immediate notification, requires shared data protection
- Limit subscribers per topic to avoid excessive memory consumption
- Use appropriate queue depths based on message rates and processing speeds

The pub-sub pattern is essential for scalable FreeRTOS applications, enabling clean architecture and maintainable code in complex embedded systems.