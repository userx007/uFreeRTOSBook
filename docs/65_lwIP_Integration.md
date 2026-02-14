# lwIP Integration with FreeRTOS

## Overview

**lwIP** (lightweight IP) is an open-source TCP/IP stack designed for embedded systems with limited resources. It provides a complete implementation of the TCP/IP protocol suite while maintaining a small memory footprint. Integrating lwIP with FreeRTOS enables robust network communication in resource-constrained embedded applications.

## Key Concepts

### 1. **API Choices**

lwIP offers two primary API paradigms:

- **Raw API**: Event-driven, callback-based, runs in a single thread (typically interrupt context or a dedicated task)
- **Sequential API (Netconn/Socket)**: Thread-safe, blocking calls, more familiar to traditional socket programming

### 2. **Threading Models**

- **Raw API**: No operating system required, but can be integrated with FreeRTOS for the application layer
- **Sequential API**: Requires an OS (FreeRTOS) with multiple tasks for concurrent connections

### 3. **Memory Management**

lwIP uses memory pools (MEMP) and packet buffers (PBUF) which must be configured appropriately for FreeRTOS heap management.

---

## Detailed Explanation

### Raw API

The Raw API is callback-driven and executes in the lwIP core thread (tcpip_thread). All protocol processing happens in this context, making it very efficient but requiring careful synchronization.

**Advantages:**
- Lower memory overhead
- Faster execution (no context switching)
- Direct control over timing

**Disadvantages:**
- More complex programming model
- Callbacks must execute quickly
- Harder to manage multiple connections

### Sequential API

The Sequential API (Netconn API or BSD Socket API) provides a more traditional, blocking interface. Each connection typically runs in its own FreeRTOS task.

**Advantages:**
- Familiar programming model
- Easier to write and maintain
- Natural for multiple concurrent connections

**Disadvantages:**
- Higher memory usage (stack per task)
- Context switching overhead
- Slightly higher latency

---

## C/C++ Code Examples

### Example 1: lwIP Initialization with FreeRTOS

```c
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "netif/ethernet.h"

// Network interface structure
struct netif gnetif;

// Callback after TCP/IP stack initialization
static void tcpip_init_done(void *arg)
{
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;
    
    // Static IP configuration (or use DHCP)
    IP4_ADDR(&ipaddr, 192, 168, 1, 100);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 1, 1);
    
    // Add network interface
    netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, 
              &ethernetif_init, &tcpip_input);
    
    // Set as default interface
    netif_set_default(&gnetif);
    
    if (netif_is_link_up(&gnetif)) {
        netif_set_up(&gnetif);
    }
    
    // Optional: Start DHCP
    // dhcp_start(&gnetif);
    
    printf("TCP/IP stack initialized\n");
}

void network_init(void)
{
    // Initialize lwIP with FreeRTOS
    tcpip_init(tcpip_init_done, NULL);
}
```

### Example 2: Raw API - TCP Echo Server

```c
#include "lwip/tcp.h"
#include "lwip/pbuf.h"

#define ECHO_PORT 7

struct echo_state {
    struct tcp_pcb *pcb;
    struct pbuf *p;
};

// Callback when data is received
static err_t echo_recv(void *arg, struct tcp_pcb *tpcb, 
                       struct pbuf *p, err_t err)
{
    struct echo_state *es = (struct echo_state *)arg;
    
    if (p == NULL) {
        // Connection closed by client
        tcp_close(tpcb);
        mem_free(es);
        return ERR_OK;
    }
    
    // Echo data back
    tcp_write(tpcb, p->payload, p->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    
    // Acknowledge received data
    tcp_recved(tpcb, p->tot_len);
    
    // Free the pbuf
    pbuf_free(p);
    
    return ERR_OK;
}

// Callback when connection error occurs
static void echo_error(void *arg, err_t err)
{
    struct echo_state *es = (struct echo_state *)arg;
    if (es != NULL) {
        mem_free(es);
    }
}

// Callback when new connection is accepted
static err_t echo_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    struct echo_state *es;
    
    // Allocate state structure
    es = (struct echo_state *)mem_malloc(sizeof(struct echo_state));
    if (es == NULL) {
        tcp_close(newpcb);
        return ERR_MEM;
    }
    
    es->pcb = newpcb;
    es->p = NULL;
    
    // Set up callbacks
    tcp_arg(newpcb, es);
    tcp_recv(newpcb, echo_recv);
    tcp_err(newpcb, echo_error);
    
    return ERR_OK;
}

void echo_server_init(void)
{
    struct tcp_pcb *pcb;
    
    // Create new TCP PCB
    pcb = tcp_new();
    if (pcb == NULL) {
        return;
    }
    
    // Bind to port
    err_t err = tcp_bind(pcb, IP_ADDR_ANY, ECHO_PORT);
    if (err != ERR_OK) {
        tcp_close(pcb);
        return;
    }
    
    // Listen for connections
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, echo_accept);
}
```

### Example 3: Sequential API (Netconn) - TCP Echo Server

```c
#include "lwip/api.h"
#include "FreeRTOS.h"
#include "task.h"

#define ECHO_PORT 7
#define ECHO_THREAD_PRIO (tskIDLE_PRIORITY + 2)

static void echo_thread(void *arg)
{
    struct netconn *conn, *newconn;
    err_t err;
    
    // Create a new connection handle
    conn = netconn_new(NETCONN_TCP);
    
    if (conn == NULL) {
        vTaskDelete(NULL);
        return;
    }
    
    // Bind to port
    netconn_bind(conn, IP_ADDR_ANY, ECHO_PORT);
    
    // Listen for connections
    netconn_listen(conn);
    
    while (1) {
        // Accept new connection
        err = netconn_accept(conn, &newconn);
        
        if (err == ERR_OK) {
            struct netbuf *buf;
            void *data;
            u16_t len;
            
            // Receive data
            while (netconn_recv(newconn, &buf) == ERR_OK) {
                do {
                    netbuf_data(buf, &data, &len);
                    
                    // Echo data back
                    netconn_write(newconn, data, len, NETCONN_COPY);
                    
                } while (netbuf_next(buf) >= 0);
                
                netbuf_delete(buf);
            }
            
            // Close connection
            netconn_close(newconn);
            netconn_delete(newconn);
        }
    }
}

void echo_server_netconn_init(void)
{
    xTaskCreate(echo_thread, "echo", 
                configMINIMAL_STACK_SIZE * 2, 
                NULL, ECHO_THREAD_PRIO, NULL);
}
```

### Example 4: Socket API - TCP Client

```c
#include "lwip/sockets.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define SERVER_IP "192.168.1.50"
#define SERVER_PORT 8080

static void tcp_client_task(void *pvParameters)
{
    int sock;
    struct sockaddr_in server_addr;
    char send_buf[128];
    char recv_buf[128];
    
    while (1) {
        // Create socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        // Configure server address
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);
        
        // Connect to server
        if (connect(sock, (struct sockaddr *)&server_addr, 
                   sizeof(server_addr)) < 0) {
            close(sock);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        printf("Connected to server\n");
        
        // Send data
        strcpy(send_buf, "Hello from FreeRTOS!");
        if (send(sock, send_buf, strlen(send_buf), 0) < 0) {
            close(sock);
            continue;
        }
        
        // Receive response
        int len = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (len > 0) {
            recv_buf[len] = '\0';
            printf("Received: %s\n", recv_buf);
        }
        
        close(sock);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void tcp_client_init(void)
{
    xTaskCreate(tcp_client_task, "tcp_client",
                configMINIMAL_STACK_SIZE * 3,
                NULL, tskIDLE_PRIORITY + 1, NULL);
}
```

### Example 5: Memory Pool Configuration (lwipopts.h)

```c
// Memory management options for FreeRTOS integration
#define NO_SYS                      0  // Use OS
#define SYS_LIGHTWEIGHT_PROT        1

// Memory pools
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    (16 * 1024)  // Heap size
#define MEMP_NUM_PBUF               16           // Number of pbufs
#define MEMP_NUM_TCP_PCB            10           // TCP connections
#define MEMP_NUM_TCP_PCB_LISTEN     8
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_NETBUF             8
#define MEMP_NUM_NETCONN            8

// PBUF options
#define PBUF_POOL_SIZE              24
#define PBUF_POOL_BUFSIZE           512

// TCP options
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (4 * TCP_MSS)
#define TCP_WND                     (4 * TCP_MSS)

// Threading options
#define TCPIP_THREAD_STACKSIZE      1024
#define TCPIP_THREAD_PRIO           (tskIDLE_PRIORITY + 4)
#define TCPIP_MBOX_SIZE             16

// API options
#define LWIP_NETCONN                1  // Enable Netconn API
#define LWIP_SOCKET                 1  // Enable Socket API
```

### Example 6: Thread-Safe API Access

```c
#include "lwip/tcpip.h"
#include "FreeRTOS.h"
#include "semphr.h"

// Using tcpip_callback for thread-safe operations
static void raw_api_callback(void *arg)
{
    // This executes in the tcpip_thread context
    struct tcp_pcb *pcb = (struct tcp_pcb *)arg;
    
    // Perform lwIP operations safely
    tcp_connect(pcb, /* ... */);
}

void safe_tcp_connect_from_task(struct tcp_pcb *pcb)
{
    // Call from any FreeRTOS task
    tcpip_callback(raw_api_callback, pcb);
}

// Using tcpip_callback_with_block for synchronous operations
static void sync_operation(void *arg)
{
    int *result = (int *)arg;
    // Perform operation
    *result = 42;
}

int call_sync_operation(void)
{
    int result = 0;
    tcpip_callback_with_block(sync_operation, &result, 1);
    return result;
}
```

---

## Rust Code Examples

### Example 1: lwIP Bindings Setup (Build Configuration)

```rust
// build.rs - FFI bindings generation
use std::env;
use std::path::PathBuf;

fn main() {
    // Tell cargo to look for shared libraries
    println!("cargo:rustc-link-search=native=/path/to/lwip/lib");
    println!("cargo:rustc-link-lib=static=lwip");
    
    // Generate bindings
    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .use_core()
        .ctypes_prefix("cty")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .generate()
        .expect("Unable to generate bindings");
    
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings
        .write_to_file(out_path.join("lwip_bindings.rs"))
        .expect("Couldn't write bindings!");
}
```

### Example 2: Raw API Wrapper in Rust

```rust
// lwip_raw.rs
#![no_std]

use core::ptr;
use core::ffi::c_void;

// Include generated bindings
include!(concat!(env!("OUT_DIR"), "/lwip_bindings.rs"));

pub struct TcpPcb {
    pcb: *mut tcp_pcb,
}

impl TcpPcb {
    pub fn new() -> Option<Self> {
        unsafe {
            let pcb = tcp_new();
            if pcb.is_null() {
                None
            } else {
                Some(TcpPcb { pcb })
            }
        }
    }
    
    pub fn bind(&mut self, port: u16) -> Result<(), i8> {
        unsafe {
            let err = tcp_bind(self.pcb, &ip_addr_any as *const _, port);
            if err == err_enum_t_ERR_OK as i8 {
                Ok(())
            } else {
                Err(err)
            }
        }
    }
    
    pub fn listen(mut self) -> Result<TcpListenPcb, i8> {
        unsafe {
            let listen_pcb = tcp_listen(self.pcb);
            if listen_pcb.is_null() {
                Err(err_enum_t_ERR_MEM as i8)
            } else {
                // Prevent double-free
                self.pcb = ptr::null_mut();
                Ok(TcpListenPcb { pcb: listen_pcb })
            }
        }
    }
}

impl Drop for TcpPcb {
    fn drop(&mut self) {
        if !self.pcb.is_null() {
            unsafe {
                tcp_close(self.pcb);
            }
        }
    }
}

pub struct TcpListenPcb {
    pcb: *mut tcp_pcb,
}

impl TcpListenPcb {
    pub fn set_accept_callback<F>(&mut self, callback: F)
    where
        F: FnMut(*mut tcp_pcb) -> Result<(), i8> + 'static,
    {
        // Store callback in heap
        let boxed = Box::new(callback);
        let raw = Box::into_raw(boxed);
        
        unsafe {
            tcp_arg(self.pcb, raw as *mut c_void);
            tcp_accept(self.pcb, Some(accept_callback::<F>));
        }
    }
}

// Trampoline function for accept callback
unsafe extern "C" fn accept_callback<F>(
    arg: *mut c_void,
    newpcb: *mut tcp_pcb,
    _err: i8,
) -> i8
where
    F: FnMut(*mut tcp_pcb) -> Result<(), i8>,
{
    let callback = &mut *(arg as *mut F);
    match callback(newpcb) {
        Ok(()) => err_enum_t_ERR_OK as i8,
        Err(e) => e,
    }
}
```

### Example 3: Netconn API Wrapper

```rust
// netconn_wrapper.rs
#![no_std]

extern crate alloc;
use alloc::vec::Vec;
use core::ptr;

include!(concat!(env!("OUT_DIR"), "/lwip_bindings.rs"));

pub struct NetConn {
    conn: *mut netconn,
}

impl NetConn {
    pub fn new_tcp() -> Option<Self> {
        unsafe {
            let conn = netconn_new(netconn_type_NETCONN_TCP);
            if conn.is_null() {
                None
            } else {
                Some(NetConn { conn })
            }
        }
    }
    
    pub fn bind(&mut self, port: u16) -> Result<(), i8> {
        unsafe {
            let err = netconn_bind(self.conn, &ip_addr_any as *const _, port);
            if err == err_enum_t_ERR_OK as i8 {
                Ok(())
            } else {
                Err(err)
            }
        }
    }
    
    pub fn listen(&mut self) -> Result<(), i8> {
        unsafe {
            let err = netconn_listen(self.conn);
            if err == err_enum_t_ERR_OK as i8 {
                Ok(())
            } else {
                Err(err)
            }
        }
    }
    
    pub fn accept(&mut self) -> Result<NetConn, i8> {
        unsafe {
            let mut newconn: *mut netconn = ptr::null_mut();
            let err = netconn_accept(self.conn, &mut newconn);
            
            if err == err_enum_t_ERR_OK as i8 && !newconn.is_null() {
                Ok(NetConn { conn: newconn })
            } else {
                Err(err)
            }
        }
    }
    
    pub fn recv(&mut self) -> Result<Vec<u8>, i8> {
        unsafe {
            let mut buf: *mut netbuf = ptr::null_mut();
            let err = netconn_recv(self.conn, &mut buf);
            
            if err != err_enum_t_ERR_OK as i8 || buf.is_null() {
                return Err(err);
            }
            
            let mut data: *mut c_void = ptr::null_mut();
            let mut len: u16 = 0;
            
            netbuf_data(buf, &mut data, &mut len);
            
            let slice = core::slice::from_raw_parts(data as *const u8, len as usize);
            let result = Vec::from(slice);
            
            netbuf_delete(buf);
            Ok(result)
        }
    }
    
    pub fn write(&mut self, data: &[u8]) -> Result<(), i8> {
        unsafe {
            let err = netconn_write(
                self.conn,
                data.as_ptr() as *const c_void,
                data.len(),
                NETCONN_COPY as u8,
            );
            
            if err == err_enum_t_ERR_OK as i8 {
                Ok(())
            } else {
                Err(err)
            }
        }
    }
    
    pub fn close(&mut self) -> Result<(), i8> {
        unsafe {
            let err = netconn_close(self.conn);
            if err == err_enum_t_ERR_OK as i8 {
                Ok(())
            } else {
                Err(err)
            }
        }
    }
}

impl Drop for NetConn {
    fn drop(&mut self) {
        if !self.conn.is_null() {
            unsafe {
                netconn_delete(self.conn);
            }
        }
    }
}
```

### Example 4: Echo Server in Rust (Using Netconn Wrapper)

```rust
// echo_server.rs
#![no_std]
#![no_main]

extern crate alloc;

mod netconn_wrapper;
use netconn_wrapper::NetConn;

// Assuming FreeRTOS Rust bindings are available
extern crate freertos_rust;
use freertos_rust::*;

fn echo_server_task(_: FreeRtosTaskHandle) {
    let mut listener = NetConn::new_tcp().expect("Failed to create TCP connection");
    
    listener.bind(7).expect("Failed to bind");
    listener.listen().expect("Failed to listen");
    
    loop {
        match listener.accept() {
            Ok(mut client) => {
                // Handle client in this task or spawn new task
                loop {
                    match client.recv() {
                        Ok(data) => {
                            if data.is_empty() {
                                break; // Connection closed
                            }
                            
                            // Echo back
                            if client.write(&data).is_err() {
                                break;
                            }
                        }
                        Err(_) => break,
                    }
                }
                
                let _ = client.close();
            }
            Err(_) => {
                // Error accepting connection
                Task::delay(Duration::ms(100));
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn main() -> ! {
    // Initialize lwIP (assuming C initialization function)
    unsafe {
        network_init();
    }
    
    // Create echo server task
    Task::new()
        .name("echo_server")
        .stack_size(2048)
        .priority(TaskPriority(3))
        .start(echo_server_task)
        .expect("Failed to create task");
    
    // Start FreeRTOS scheduler
    FreeRtosUtils::start_scheduler();
}
```

### Example 5: UDP Communication in Rust

```rust
// udp_client.rs
use core::ptr;
use core::ffi::c_void;

include!(concat!(env!("OUT_DIR"), "/lwip_bindings.rs"));

pub struct UdpPcb {
    pcb: *mut udp_pcb,
}

impl UdpPcb {
    pub fn new() -> Option<Self> {
        unsafe {
            let pcb = udp_new();
            if pcb.is_null() {
                None
            } else {
                Some(UdpPcb { pcb })
            }
        }
    }
    
    pub fn bind(&mut self, port: u16) -> Result<(), i8> {
        unsafe {
            let err = udp_bind(self.pcb, &ip_addr_any as *const _, port);
            if err == err_enum_t_ERR_OK as i8 {
                Ok(())
            } else {
                Err(err)
            }
        }
    }
    
    pub fn send_to(&mut self, data: &[u8], dest_ip: &ip_addr_t, dest_port: u16) 
        -> Result<(), i8> 
    {
        unsafe {
            // Allocate pbuf
            let p = pbuf_alloc(
                pbuf_layer_PBUF_TRANSPORT,
                data.len() as u16,
                pbuf_type_PBUF_RAM,
            );
            
            if p.is_null() {
                return Err(err_enum_t_ERR_MEM as i8);
            }
            
            // Copy data to pbuf
            pbuf_take(p, data.as_ptr() as *const c_void, data.len() as u16);
            
            // Send
            let err = udp_sendto(self.pcb, p, dest_ip, dest_port);
            
            // Free pbuf
            pbuf_free(p);
            
            if err == err_enum_t_ERR_OK as i8 {
                Ok(())
            } else {
                Err(err)
            }
        }
    }
    
    pub fn set_recv_callback<F>(&mut self, callback: F)
    where
        F: FnMut(&[u8], &ip_addr_t, u16) + 'static,
    {
        let boxed = Box::new(callback);
        let raw = Box::into_raw(boxed);
        
        unsafe {
            udp_recv(self.pcb, Some(udp_recv_callback::<F>), raw as *mut c_void);
        }
    }
}

unsafe extern "C" fn udp_recv_callback<F>(
    arg: *mut c_void,
    _pcb: *mut udp_pcb,
    p: *mut pbuf,
    addr: *const ip_addr_t,
    port: u16,
) where
    F: FnMut(&[u8], &ip_addr_t, u16),
{
    if p.is_null() || addr.is_null() {
        return;
    }
    
    let callback = &mut *(arg as *mut F);
    let data = core::slice::from_raw_parts((*p).payload as *const u8, (*p).len as usize);
    
    callback(data, &*addr, port);
    
    pbuf_free(p);
}

impl Drop for UdpPcb {
    fn drop(&mut self) {
        if !self.pcb.is_null() {
            unsafe {
                udp_remove(self.pcb);
            }
        }
    }
}
```

---

## Thread and Memory Management Best Practices

### 1. **Task Priorities**
```c
// Recommended priority hierarchy
#define ETHERNET_INPUT_TASK_PRIO  (tskIDLE_PRIORITY + 5)  // Highest
#define TCPIP_THREAD_PRIO         (tskIDLE_PRIORITY + 4)
#define APP_NETWORK_TASK_PRIO     (tskIDLE_PRIORITY + 2)  // Application tasks
```

### 2. **Memory Pool Sizing**
- Size memory pools based on expected concurrent connections
- Monitor usage with `stats_display()` during development
- Account for both transmit and receive buffers

### 3. **Thread Safety**
- Use `LOCK_TCPIP_CORE()` / `UNLOCK_TCPIP_CORE()` when accessing lwIP from multiple tasks
- Or use `tcpip_callback()` for asynchronous operations
- Sequential API handles locking automatically

### 4. **Zero-Copy Optimization**
```c
// Use PBUF_REF or PBUF_ROM for zero-copy transmission
struct pbuf *p = pbuf_alloc(PBUF_RAW, 0, PBUF_REF);
p->payload = my_static_buffer;
p->len = p->tot_len = buffer_size;
```

---

## Summary

**lwIP integration with FreeRTOS** enables robust TCP/IP networking in embedded systems with careful attention to API selection, threading model, and memory management:

**Key Takeaways:**

1. **API Selection**: Choose Raw API for performance-critical, single-connection scenarios; use Sequential API (Netconn/Socket) for easier development with multiple concurrent connections

2. **Threading**: The tcpip_thread is the core of lwIP operation in RTOS mode—all Raw API callbacks execute here, while Sequential API creates separate tasks per connection

3. **Memory Management**: Properly configure memory pools (MEMP) and packet buffers (PBUF) in `lwipopts.h` based on application requirements

4. **Thread Safety**: Use `tcpip_callback()` or locking macros when accessing lwIP from FreeRTOS tasks outside the tcpip_thread

5. **Language Support**: While C is native, Rust can interface with lwIP through FFI bindings, enabling memory-safe wrappers with compile-time guarantees

6. **Performance Optimization**: Consider zero-copy techniques, appropriate task priorities, and memory pool sizing for optimal performance in resource-constrained environments

The integration provides a production-ready networking stack suitable for IoT devices, industrial automation, and embedded web servers while maintaining the small footprint required for microcontroller-based systems.