# FreeRTOS+TCP Stack - Comprehensive Guide

## What's Included:

**Core Topics:**
- Architecture overview and key components (IP Task, Network Buffers, Socket API, DHCP)
- Detailed configuration parameters in FreeRTOSIPConfig.h
- Network interface driver implementation
- Socket programming (TCP/UDP, client/server patterns)

**Code Examples in C/C++:**
- TCP client and server implementations
- UDP socket programming
- HTTP client example
- Multi-threaded server with connection pooling
- Network initialization and event handling
- Complete network interface driver example

**Code Examples in Rust:**
- FFI bindings for FreeRTOS+TCP
- Type-safe TCP client and server
- UDP socket implementation
- RAII-based resource management

**Additional Content:**
- Best practices for buffer management, error handling, and thread safety
- Performance tuning recommendations
- Network monitoring and link detection
- Complete working examples ready for adaptation

The guide provides production-ready code patterns suitable for IoT devices, industrial control systems, medical devices, and other embedded applications requiring robust TCP/IP networking.

---

## Table of Contents
1. [Introduction](#introduction)
2. [Architecture Overview](#architecture-overview)
3. [Key Components](#key-components)
4. [Configuration](#configuration)
5. [Network Interface Implementation](#network-interface-implementation)
6. [Socket Programming](#socket-programming)
7. [TCP/IP Applications](#tcpip-applications)
8. [Code Examples - C/C++](#code-examples-cc)
9. [Code Examples - Rust](#code-examples-rust)
10. [Best Practices](#best-practices)
11. [Summary](#summary)

---

## Introduction

**FreeRTOS+TCP** is a scalable, open-source TCP/IP stack designed specifically for FreeRTOS-based embedded systems. It provides a complete network protocol implementation with the following characteristics:

- **Thread-safe**: All API functions are designed for multi-threaded environments
- **Zero-copy architecture**: Minimizes memory copies for improved performance
- **Scalable**: Configurable features allow optimization for resource-constrained devices
- **Berkeley sockets compatible**: Familiar API for network programming
- **Integrated with FreeRTOS**: Leverages FreeRTOS primitives for scheduling and synchronization

### Key Features
- Full TCP/IP stack (IPv4/IPv6 support)
- UDP and TCP protocols
- DHCP client
- DNS client
- ARP protocol
- ICMP (ping) support
- Multiple network interface support
- Network buffer management
- Hardware checksum offloading support

---

## Architecture Overview

The FreeRTOS+TCP stack consists of several layers:

```
┌─────────────────────────────────────┐
│   Application Layer (Sockets API)   │
├─────────────────────────────────────┤
│   Transport Layer (TCP/UDP)         │
├─────────────────────────────────────┤
│   Network Layer (IP/ICMP/ARP)       │
├─────────────────────────────────────┤
│   Network Interface (Driver Layer)  │
├─────────────────────────────────────┤
│   Physical Layer (Ethernet/WiFi)    │
└─────────────────────────────────────┘
```

### Core Components

1. **IP Task**: Central processing task that handles all network events
2. **Network Buffers**: Zero-copy buffer management system
3. **Network Interface**: Hardware abstraction layer
4. **Protocol Handlers**: TCP, UDP, ICMP, ARP, DHCP, DNS implementations
5. **Socket Layer**: Berkeley-sockets compatible API

---

## Key Components

### 1. IP Task (FreeRTOS_IP.c)

The IP task is the heart of the TCP/IP stack. It:
- Receives packets from network interfaces
- Processes incoming packets
- Handles protocol state machines
- Manages timers for retransmissions and timeouts
- Coordinates with application tasks via queues

### 2. Network Buffers (FreeRTOS_IP_Utils.c)

Network buffers are the fundamental data structure for packet handling:
- **NetworkBufferDescriptor_t**: Metadata structure
- **Zero-copy design**: Buffers passed by reference
- **Buffer pools**: Pre-allocated for deterministic behavior

### 3. Socket API (FreeRTOS_Sockets.c)

Provides BSD-compatible socket functions:
- `FreeRTOS_socket()`: Create socket
- `FreeRTOS_bind()`: Bind to address
- `FreeRTOS_connect()`: Connect to remote endpoint
- `FreeRTOS_send()` / `FreeRTOS_recv()`: Data transfer
- `FreeRTOS_listen()` / `FreeRTOS_accept()`: Server operations

### 4. DHCP Client (FreeRTOS_DHCP.c)

Automatic IP address configuration:
- Discovers DHCP servers
- Obtains IP address, subnet mask, gateway
- Handles lease renewals
- Supports static fallback

---

## Configuration

FreeRTOS+TCP is configured via `FreeRTOSIPConfig.h`. Key configuration parameters:

### Essential Configuration Parameters

```c
/* Network Configuration */
#define ipconfigUSE_DHCP                    1
#define ipconfigUSE_DNS                     1
#define ipconfigUSE_TCP                     1
#define ipconfigUSE_UDP                     1

/* IP Configuration (if DHCP disabled) */
#define configIP_ADDR0                      192
#define configIP_ADDR1                      168
#define configIP_ADDR2                      1
#define configIP_ADDR3                      100

#define configNET_MASK0                     255
#define configNET_MASK1                     255
#define configNET_MASK2                     255
#define configNET_MASK3                     0

#define configGATEWAY_ADDR0                 192
#define configGATEWAY_ADDR1                 168
#define configGATEWAY_ADDR2                 1
#define configGATEWAY_ADDR3                 1

#define configDNS_SERVER_ADDR0              8
#define configDNS_SERVER_ADDR1              8
#define configDNS_SERVER_ADDR2              8
#define configDNS_SERVER_ADDR3              8

/* MAC Address */
#define configMAC_ADDR0                     0x00
#define configMAC_ADDR1                     0x11
#define configMAC_ADDR2                     0x22
#define configMAC_ADDR3                     0x33
#define configMAC_ADDR4                     0x44
#define configMAC_ADDR5                     0x55

/* Buffer Configuration */
#define ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS  60
#define ipconfigNETWORK_MTU                 1500
#define ipconfigTCP_MSS                     1460

/* Task Priorities and Sizes */
#define ipconfigIP_TASK_PRIORITY            (configMAX_PRIORITIES - 2)
#define ipconfigIP_TASK_STACK_SIZE_WORDS    1024

/* TCP Configuration */
#define ipconfigTCP_HANG_PROTECTION         1
#define ipconfigTCP_KEEP_ALIVE              1
#define ipconfigTCP_TIME_TO_LIVE            128
#define ipconfigTCP_TX_BUFFER_LENGTH        (4 * ipconfigTCP_MSS)
#define ipconfigTCP_RX_BUFFER_LENGTH        (4 * ipconfigTCP_MSS)

/* Socket Options */
#define ipconfigSOCKET_HAS_USER_SEMAPHORE   1
#define ipconfigSOCKET_HAS_USER_WAKE_CALLBACK 1
#define ipconfigSUPPORT_SELECT_FUNCTION     1

/* Zero Copy */
#define ipconfigZERO_COPY_RX_DRIVER         1
#define ipconfigZERO_COPY_TX_DRIVER         1

/* Driver-specific optimizations */
#define ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM  1
#define ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM  1
```

### Performance Tuning Parameters

```c
/* Increase for high-throughput applications */
#define ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS  100

/* TCP Window Scaling */
#define ipconfigUSE_TCP_WIN                 1
#define ipconfigTCP_WIN_SEG_COUNT           256

/* Event processing */
#define ipconfigPROCESS_CUSTOM_ETHERNET_FRAMES  0

/* Logging */
#define ipconfigHAS_DEBUG_PRINTF            1
#define ipconfigHAS_PRINTF                  1
```

---

## Network Interface Implementation

Every hardware platform requires a network interface implementation. The interface provides hardware abstraction.

### Network Interface Structure

```c
typedef struct xNetworkInterface
{
    const char *pcName;                          /* Interface name */
    BaseType_t (*pfInitialise)(struct xNetworkInterface *pxInterface);
    BaseType_t (*pfOutput)(struct xNetworkInterface *pxInterface,
                          NetworkBufferDescriptor_t * const pxNetworkBuffer,
                          BaseType_t xReleaseAfterSend);
    BaseType_t (*pfGetPhyLinkStatus)(struct xNetworkInterface *pxInterface);
} NetworkInterface_t;
```

### Example Network Interface Implementation (C)

```c
#include "FreeRTOS.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"

/* Ethernet driver headers */
#include "eth_driver.h"

/* Interface handle */
static NetworkInterface_t *pxMyInterface = NULL;

/* MAC address */
static uint8_t ucMACAddress[6] = 
{
    configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2,
    configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5
};

/* Initialization function */
BaseType_t xNetworkInterfaceInitialise(NetworkInterface_t *pxInterface)
{
    BaseType_t xReturn = pdFALSE;
    
    if (pxMyInterface == NULL)
    {
        pxMyInterface = pxInterface;
        
        /* Initialize hardware */
        if (ETH_Init() == ETH_SUCCESS)
        {
            /* Set MAC address */
            ETH_SetMACAddress(ucMACAddress);
            
            /* Enable interrupts */
            ETH_EnableInterrupts();
            
            /* Start reception */
            ETH_StartReception();
            
            xReturn = pdTRUE;
        }
    }
    
    return xReturn;
}

/* Output function - transmit packet */
BaseType_t xNetworkInterfaceOutput(NetworkInterface_t *pxInterface,
                                   NetworkBufferDescriptor_t * const pxNetworkBuffer,
                                   BaseType_t xReleaseAfterSend)
{
    BaseType_t xReturn = pdFALSE;
    
    if (pxNetworkBuffer != NULL)
    {
        /* Transmit the packet */
        if (ETH_Transmit(pxNetworkBuffer->pucEthernetBuffer,
                        pxNetworkBuffer->xDataLength) == ETH_SUCCESS)
        {
            xReturn = pdTRUE;
            
            /* Update statistics */
            iptraceNETWORK_INTERFACE_TRANSMIT();
        }
        
        /* Release buffer if requested */
        if (xReleaseAfterSend != pdFALSE)
        {
            vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
        }
    }
    
    return xReturn;
}

/* Get link status */
BaseType_t xGetPhyLinkStatus(NetworkInterface_t *pxInterface)
{
    return ETH_GetLinkStatus() ? pdTRUE : pdFALSE;
}

/* Ethernet receive interrupt handler */
void ETH_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    NetworkBufferDescriptor_t *pxNetworkBuffer;
    uint32_t ulLength;
    
    while (ETH_IsPacketAvailable())
    {
        /* Get packet length */
        ulLength = ETH_GetReceivedPacketLength();
        
        /* Allocate network buffer */
        pxNetworkBuffer = pxGetNetworkBufferWithDescriptor(ulLength, 0);
        
        if (pxNetworkBuffer != NULL)
        {
            /* Copy packet data */
            ETH_ReceivePacket(pxNetworkBuffer->pucEthernetBuffer, ulLength);
            pxNetworkBuffer->xDataLength = ulLength;
            pxNetworkBuffer->pxInterface = pxMyInterface;
            pxNetworkBuffer->pxEndPoint = FreeRTOS_MatchingEndpoint(
                pxMyInterface, pxNetworkBuffer->pucEthernetBuffer);
            
            /* Send to IP task */
            if (xSendEventStructToIPTask(pxNetworkBuffer,
                                        eNetworkRxEvent) == pdFALSE)
            {
                /* Failed to send, release buffer */
                vReleaseNetworkBufferAndDescriptor(pxNetworkBuffer);
                iptraceETHERNET_RX_EVENT_LOST();
            }
            else
            {
                iptraceNETWORK_INTERFACE_RECEIVE();
            }
        }
        else
        {
            /* No buffer available, discard packet */
            ETH_DiscardPacket();
            iptraceETHERNET_RX_EVENT_LOST();
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* Fill in interface structure */
NetworkInterface_t *pxMyNetworkInterface(void)
{
    static NetworkInterface_t xInterface;
    
    memset(&xInterface, 0, sizeof(xInterface));
    
    xInterface.pcName = "eth0";
    xInterface.pfInitialise = xNetworkInterfaceInitialise;
    xInterface.pfOutput = xNetworkInterfaceOutput;
    xInterface.pfGetPhyLinkStatus = xGetPhyLinkStatus;
    
    FreeRTOS_FillInterfaceDescriptor(&xInterface);
    
    return &xInterface;
}
```

---

## Socket Programming

FreeRTOS+TCP provides a Berkeley sockets-compatible API.

### Socket Types

```c
/* Stream socket (TCP) */
Socket_t xSocket = FreeRTOS_socket(FREERTOS_AF_INET,
                                   FREERTOS_SOCK_STREAM,
                                   FREERTOS_IPPROTO_TCP);

/* Datagram socket (UDP) */
Socket_t xSocket = FreeRTOS_socket(FREERTOS_AF_INET,
                                   FREERTOS_SOCK_DGRAM,
                                   FREERTOS_IPPROTO_UDP);
```

### Socket Options

```c
/* Set timeout */
TickType_t xReceiveTimeout = pdMS_TO_TICKS(5000);
FreeRTOS_setsockopt(xSocket,
                   0,
                   FREERTOS_SO_RCVTIMEO,
                   &xReceiveTimeout,
                   sizeof(xReceiveTimeout));

/* Set send timeout */
TickType_t xSendTimeout = pdMS_TO_TICKS(5000);
FreeRTOS_setsockopt(xSocket,
                   0,
                   FREERTOS_SO_SNDTIMEO,
                   &xSendTimeout,
                   sizeof(xSendTimeout));

/* Reuse address */
BaseType_t xReuseSocket = pdTRUE;
FreeRTOS_setsockopt(xSocket,
                   0,
                   FREERTOS_SO_REUSE_LISTEN_SOCKET,
                   &xReuseSocket,
                   sizeof(xReuseSocket));

/* Set receive buffer size */
size_t uxBufferSize = 8192;
FreeRTOS_setsockopt(xSocket,
                   0,
                   FREERTOS_SO_RCVBUF,
                   &uxBufferSize,
                   sizeof(uxBufferSize));
```

---

## TCP/IP Applications

### 1. TCP Client Example (C)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#define SERVER_IP_ADDR      "192.168.1.10"
#define SERVER_PORT         8080
#define BUFFER_SIZE         512

void vTCPClientTask(void *pvParameters)
{
    Socket_t xSocket;
    struct freertos_sockaddr xServerAddress;
    BaseType_t xStatus;
    uint8_t ucBuffer[BUFFER_SIZE];
    int32_t lBytesReceived, lBytesSent;
    TickType_t xTimeout = pdMS_TO_TICKS(10000);
    
    /* Create socket */
    xSocket = FreeRTOS_socket(FREERTOS_AF_INET,
                             FREERTOS_SOCK_STREAM,
                             FREERTOS_IPPROTO_TCP);
    
    if (xSocket != FREERTOS_INVALID_SOCKET)
    {
        /* Set timeouts */
        FreeRTOS_setsockopt(xSocket, 0, FREERTOS_SO_RCVTIMEO,
                           &xTimeout, sizeof(xTimeout));
        FreeRTOS_setsockopt(xSocket, 0, FREERTOS_SO_SNDTIMEO,
                           &xTimeout, sizeof(xTimeout));
        
        /* Set server address */
        xServerAddress.sin_family = FREERTOS_AF_INET;
        xServerAddress.sin_port = FreeRTOS_htons(SERVER_PORT);
        xServerAddress.sin_addr = FreeRTOS_inet_addr(SERVER_IP_ADDR);
        
        /* Connect to server */
        xStatus = FreeRTOS_connect(xSocket,
                                  &xServerAddress,
                                  sizeof(xServerAddress));
        
        if (xStatus == 0)
        {
            const char *pcMessage = "Hello from FreeRTOS+TCP!";
            
            /* Send data */
            lBytesSent = FreeRTOS_send(xSocket,
                                      pcMessage,
                                      strlen(pcMessage),
                                      0);
            
            if (lBytesSent > 0)
            {
                /* Receive response */
                lBytesReceived = FreeRTOS_recv(xSocket,
                                              ucBuffer,
                                              BUFFER_SIZE - 1,
                                              0);
                
                if (lBytesReceived > 0)
                {
                    ucBuffer[lBytesReceived] = '\0';
                    printf("Received: %s\n", ucBuffer);
                }
            }
        }
        
        /* Close socket */
        FreeRTOS_closesocket(xSocket);
    }
    
    vTaskDelete(NULL);
}
```

### 2. TCP Server Example (C)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#define SERVER_PORT         8080
#define BUFFER_SIZE         1024
#define MAX_BACKLOG         5

void vTCPServerTask(void *pvParameters)
{
    Socket_t xListeningSocket, xConnectedSocket;
    struct freertos_sockaddr xBindAddress, xClientAddress;
    socklen_t xClientAddressLength = sizeof(xClientAddress);
    BaseType_t xStatus;
    uint8_t ucBuffer[BUFFER_SIZE];
    int32_t lBytesReceived;
    TickType_t xTimeout = pdMS_TO_TICKS(20000);
    BaseType_t xReuseSocket = pdTRUE;
    
    /* Create listening socket */
    xListeningSocket = FreeRTOS_socket(FREERTOS_AF_INET,
                                      FREERTOS_SOCK_STREAM,
                                      FREERTOS_IPPROTO_TCP);
    
    if (xListeningSocket != FREERTOS_INVALID_SOCKET)
    {
        /* Set socket options */
        FreeRTOS_setsockopt(xListeningSocket, 0,
                           FREERTOS_SO_REUSE_LISTEN_SOCKET,
                           &xReuseSocket, sizeof(xReuseSocket));
        
        /* Bind to port */
        xBindAddress.sin_family = FREERTOS_AF_INET;
        xBindAddress.sin_port = FreeRTOS_htons(SERVER_PORT);
        xBindAddress.sin_addr = FreeRTOS_INADDR_ANY;
        
        xStatus = FreeRTOS_bind(xListeningSocket,
                               &xBindAddress,
                               sizeof(xBindAddress));
        
        if (xStatus == 0)
        {
            /* Listen for connections */
            xStatus = FreeRTOS_listen(xListeningSocket, MAX_BACKLOG);
            
            if (xStatus == 0)
            {
                printf("Server listening on port %d\n", SERVER_PORT);
                
                /* Accept connections loop */
                for (;;)
                {
                    /* Accept connection */
                    xConnectedSocket = FreeRTOS_accept(
                        xListeningSocket,
                        &xClientAddress,
                        &xClientAddressLength);
                    
                    if (xConnectedSocket != FREERTOS_INVALID_SOCKET)
                    {
                        /* Set timeout for connected socket */
                        FreeRTOS_setsockopt(xConnectedSocket, 0,
                                           FREERTOS_SO_RCVTIMEO,
                                           &xTimeout, sizeof(xTimeout));
                        
                        /* Handle client communication */
                        do
                        {
                            lBytesReceived = FreeRTOS_recv(
                                xConnectedSocket,
                                ucBuffer,
                                BUFFER_SIZE - 1,
                                0);
                            
                            if (lBytesReceived > 0)
                            {
                                ucBuffer[lBytesReceived] = '\0';
                                
                                /* Echo back to client */
                                FreeRTOS_send(xConnectedSocket,
                                            ucBuffer,
                                            lBytesReceived,
                                            0);
                            }
                        } while (lBytesReceived > 0);
                        
                        /* Close client socket */
                        FreeRTOS_closesocket(xConnectedSocket);
                    }
                }
            }
        }
        
        /* Close listening socket */
        FreeRTOS_closesocket(xListeningSocket);
    }
    
    vTaskDelete(NULL);
}
```

### 3. UDP Socket Example (C)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#define UDP_PORT            9000
#define BUFFER_SIZE         512

void vUDPEchoTask(void *pvParameters)
{
    Socket_t xSocket;
    struct freertos_sockaddr xBindAddress, xClientAddress;
    socklen_t xClientAddressLength = sizeof(xClientAddress);
    uint8_t ucBuffer[BUFFER_SIZE];
    int32_t lBytesReceived;
    
    /* Create UDP socket */
    xSocket = FreeRTOS_socket(FREERTOS_AF_INET,
                             FREERTOS_SOCK_DGRAM,
                             FREERTOS_IPPROTO_UDP);
    
    if (xSocket != FREERTOS_INVALID_SOCKET)
    {
        /* Bind to port */
        xBindAddress.sin_family = FREERTOS_AF_INET;
        xBindAddress.sin_port = FreeRTOS_htons(UDP_PORT);
        xBindAddress.sin_addr = FreeRTOS_INADDR_ANY;
        
        if (FreeRTOS_bind(xSocket, &xBindAddress,
                         sizeof(xBindAddress)) == 0)
        {
            printf("UDP server listening on port %d\n", UDP_PORT);
            
            /* Receive and echo loop */
            for (;;)
            {
                lBytesReceived = FreeRTOS_recvfrom(
                    xSocket,
                    ucBuffer,
                    BUFFER_SIZE,
                    0,
                    &xClientAddress,
                    &xClientAddressLength);
                
                if (lBytesReceived > 0)
                {
                    /* Echo back to sender */
                    FreeRTOS_sendto(xSocket,
                                   ucBuffer,
                                   lBytesReceived,
                                   0,
                                   &xClientAddress,
                                   xClientAddressLength);
                }
            }
        }
        
        FreeRTOS_closesocket(xSocket);
    }
    
    vTaskDelete(NULL);
}
```

### 4. HTTP Client Example (C)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

void vHTTPClientTask(void *pvParameters)
{
    Socket_t xSocket;
    struct freertos_sockaddr xServerAddress;
    uint32_t ulIPAddress;
    const char *pcHostName = "www.example.com";
    const char *pcHTTPRequest = 
        "GET / HTTP/1.1\r\n"
        "Host: www.example.com\r\n"
        "Connection: close\r\n\r\n";
    uint8_t ucBuffer[2048];
    int32_t lBytesReceived, lBytesSent;
    TickType_t xTimeout = pdMS_TO_TICKS(10000);
    
    /* Resolve hostname */
    ulIPAddress = FreeRTOS_gethostbyname(pcHostName);
    
    if (ulIPAddress != 0)
    {
        /* Create socket */
        xSocket = FreeRTOS_socket(FREERTOS_AF_INET,
                                 FREERTOS_SOCK_STREAM,
                                 FREERTOS_IPPROTO_TCP);
        
        if (xSocket != FREERTOS_INVALID_SOCKET)
        {
            /* Set timeout */
            FreeRTOS_setsockopt(xSocket, 0, FREERTOS_SO_RCVTIMEO,
                               &xTimeout, sizeof(xTimeout));
            
            /* Connect to server */
            xServerAddress.sin_family = FREERTOS_AF_INET;
            xServerAddress.sin_port = FreeRTOS_htons(80);
            xServerAddress.sin_addr = ulIPAddress;
            
            if (FreeRTOS_connect(xSocket, &xServerAddress,
                                sizeof(xServerAddress)) == 0)
            {
                /* Send HTTP request */
                lBytesSent = FreeRTOS_send(xSocket,
                                          pcHTTPRequest,
                                          strlen(pcHTTPRequest),
                                          0);
                
                if (lBytesSent > 0)
                {
                    /* Receive response */
                    do
                    {
                        lBytesReceived = FreeRTOS_recv(xSocket,
                                                      ucBuffer,
                                                      sizeof(ucBuffer) - 1,
                                                      0);
                        
                        if (lBytesReceived > 0)
                        {
                            ucBuffer[lBytesReceived] = '\0';
                            printf("%s", ucBuffer);
                        }
                    } while (lBytesReceived > 0);
                }
            }
            
            FreeRTOS_closesocket(xSocket);
        }
    }
    
    vTaskDelete(NULL);
}
```

---

## Code Examples - C/C++

### Advanced Example: Multi-threaded TCP Server with Connection Pool

```c
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#define MAX_CONNECTIONS     10
#define WORKER_STACK_SIZE   2048
#define BUFFER_SIZE         1024
#define SERVER_PORT         8080

/* Connection context */
typedef struct
{
    Socket_t xSocket;
    TaskHandle_t xTaskHandle;
    uint8_t ucBuffer[BUFFER_SIZE];
    BaseType_t xInUse;
} ConnectionContext_t;

/* Global connection pool */
static ConnectionContext_t xConnections[MAX_CONNECTIONS];
static SemaphoreHandle_t xConnectionMutex;

/* Worker task to handle individual connection */
void vConnectionWorkerTask(void *pvParameters)
{
    ConnectionContext_t *pxContext = (ConnectionContext_t *)pvParameters;
    int32_t lBytesReceived;
    TickType_t xTimeout = pdMS_TO_TICKS(30000);
    
    /* Set socket timeout */
    FreeRTOS_setsockopt(pxContext->xSocket, 0,
                       FREERTOS_SO_RCVTIMEO,
                       &xTimeout, sizeof(xTimeout));
    
    printf("Worker task started for socket %p\n", pxContext->xSocket);
    
    /* Handle client requests */
    do
    {
        lBytesReceived = FreeRTOS_recv(pxContext->xSocket,
                                      pxContext->ucBuffer,
                                      BUFFER_SIZE - 1,
                                      0);
        
        if (lBytesReceived > 0)
        {
            pxContext->ucBuffer[lBytesReceived] = '\0';
            
            /* Process request (example: simple echo) */
            printf("Received %d bytes: %s\n", lBytesReceived,
                   pxContext->ucBuffer);
            
            /* Send response */
            FreeRTOS_send(pxContext->xSocket,
                         pxContext->ucBuffer,
                         lBytesReceived,
                         0);
        }
    } while (lBytesReceived > 0);
    
    printf("Connection closed\n");
    
    /* Close socket */
    FreeRTOS_closesocket(pxContext->xSocket);
    
    /* Mark connection as available */
    xSemaphoreTake(xConnectionMutex, portMAX_DELAY);
    pxContext->xInUse = pdFALSE;
    pxContext->xTaskHandle = NULL;
    xSemaphoreGive(xConnectionMutex);
    
    /* Delete task */
    vTaskDelete(NULL);
}

/* Main server task */
void vTCPServerPoolTask(void *pvParameters)
{
    Socket_t xListeningSocket, xConnectedSocket;
    struct freertos_sockaddr xBindAddress, xClientAddress;
    socklen_t xClientAddressLength = sizeof(xClientAddress);
    BaseType_t xStatus, xIndex;
    BaseType_t xReuseSocket = pdTRUE;
    
    /* Initialize connection pool */
    memset(xConnections, 0, sizeof(xConnections));
    xConnectionMutex = xSemaphoreCreateMutex();
    
    /* Create listening socket */
    xListeningSocket = FreeRTOS_socket(FREERTOS_AF_INET,
                                      FREERTOS_SOCK_STREAM,
                                      FREERTOS_IPPROTO_TCP);
    
    if (xListeningSocket != FREERTOS_INVALID_SOCKET)
    {
        /* Configure socket */
        FreeRTOS_setsockopt(xListeningSocket, 0,
                           FREERTOS_SO_REUSE_LISTEN_SOCKET,
                           &xReuseSocket, sizeof(xReuseSocket));
        
        /* Bind */
        xBindAddress.sin_family = FREERTOS_AF_INET;
        xBindAddress.sin_port = FreeRTOS_htons(SERVER_PORT);
        xBindAddress.sin_addr = FreeRTOS_INADDR_ANY;
        
        xStatus = FreeRTOS_bind(xListeningSocket,
                               &xBindAddress,
                               sizeof(xBindAddress));
        
        if (xStatus == 0)
        {
            /* Listen */
            xStatus = FreeRTOS_listen(xListeningSocket, MAX_CONNECTIONS);
            
            if (xStatus == 0)
            {
                printf("Server pool ready on port %d\n", SERVER_PORT);
                
                /* Accept loop */
                for (;;)
                {
                    xConnectedSocket = FreeRTOS_accept(
                        xListeningSocket,
                        &xClientAddress,
                        &xClientAddressLength);
                    
                    if (xConnectedSocket != FREERTOS_INVALID_SOCKET)
                    {
                        /* Find available connection slot */
                        xSemaphoreTake(xConnectionMutex, portMAX_DELAY);
                        
                        xIndex = -1;
                        for (BaseType_t i = 0; i < MAX_CONNECTIONS; i++)
                        {
                            if (xConnections[i].xInUse == pdFALSE)
                            {
                                xIndex = i;
                                xConnections[i].xInUse = pdTRUE;
                                xConnections[i].xSocket = xConnectedSocket;
                                break;
                            }
                        }
                        
                        xSemaphoreGive(xConnectionMutex);
                        
                        if (xIndex >= 0)
                        {
                            /* Create worker task */
                            char pcTaskName[16];
                            snprintf(pcTaskName, sizeof(pcTaskName),
                                    "Worker%d", xIndex);
                            
                            xTaskCreate(vConnectionWorkerTask,
                                       pcTaskName,
                                       WORKER_STACK_SIZE,
                                       &xConnections[xIndex],
                                       tskIDLE_PRIORITY + 2,
                                       &xConnections[xIndex].xTaskHandle);
                        }
                        else
                        {
                            /* No slot available, reject connection */
                            printf("Connection pool full, rejecting\n");
                            FreeRTOS_closesocket(xConnectedSocket);
                        }
                    }
                }
            }
        }
        
        FreeRTOS_closesocket(xListeningSocket);
    }
    
    vTaskDelete(NULL);
}
```

### Network Initialization (C)

```c
#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DHCP.h"

/* Network event hook */
void vApplicationIPNetworkEventHook(eIPCallbackEvent_t eNetworkEvent)
{
    static BaseType_t xTasksAlreadyCreated = pdFALSE;
    uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
    char cBuffer[16];
    
    if (eNetworkEvent == eNetworkUp)
    {
        /* Network is up and ready */
        if (xTasksAlreadyCreated == pdFALSE)
        {
            /* Get network configuration */
            FreeRTOS_GetAddressConfiguration(&ulIPAddress,
                                            &ulNetMask,
                                            &ulGatewayAddress,
                                            &ulDNSServerAddress);
            
            /* Print configuration */
            FreeRTOS_inet_ntoa(ulIPAddress, cBuffer);
            printf("IP Address: %s\n", cBuffer);
            
            FreeRTOS_inet_ntoa(ulNetMask, cBuffer);
            printf("Subnet Mask: %s\n", cBuffer);
            
            FreeRTOS_inet_ntoa(ulGatewayAddress, cBuffer);
            printf("Gateway: %s\n", cBuffer);
            
            FreeRTOS_inet_ntoa(ulDNSServerAddress, cBuffer);
            printf("DNS Server: %s\n", cBuffer);
            
            /* Create application tasks */
            xTaskCreate(vTCPServerTask, "TCPServer",
                       2048, NULL, tskIDLE_PRIORITY + 3, NULL);
            
            xTasksAlreadyCreated = pdTRUE;
        }
    }
    else if (eNetworkEvent == eNetworkDown)
    {
        printf("Network is down\n");
    }
}

/* Initialize TCP/IP stack */
BaseType_t xApplicationNetworkInit(void)
{
    BaseType_t xReturn;
    uint8_t ucMACAddress[6] = 
    {
        configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2,
        configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5
    };
    
    #if (ipconfigUSE_DHCP == 0)
    static const uint8_t ucIPAddress[4] = 
    {
        configIP_ADDR0, configIP_ADDR1,
        configIP_ADDR2, configIP_ADDR3
    };
    static const uint8_t ucNetMask[4] = 
    {
        configNET_MASK0, configNET_MASK1,
        configNET_MASK2, configNET_MASK3
    };
    static const uint8_t ucGatewayAddress[4] = 
    {
        configGATEWAY_ADDR0, configGATEWAY_ADDR1,
        configGATEWAY_ADDR2, configGATEWAY_ADDR3
    };
    static const uint8_t ucDNSServerAddress[4] = 
    {
        configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1,
        configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3
    };
    #endif
    
    /* Get network interface */
    NetworkInterface_t *pxInterface = pxMyNetworkInterface();
    
    /* Initialize the TCP/IP stack */
    #if (ipconfigUSE_DHCP == 0)
    xReturn = FreeRTOS_IPInit(ucIPAddress,
                             ucNetMask,
                             ucGatewayAddress,
                             ucDNSServerAddress,
                             ucMACAddress);
    #else
    xReturn = FreeRTOS_IPInit_Multi();
    
    /* Add endpoint with DHCP */
    FreeRTOS_AddEndpoint(pxInterface,
                        NULL,  /* DHCP will assign */
                        NULL,
                        NULL,
                        NULL,
                        ucMACAddress);
    #endif
    
    return xReturn;
}
```

---

## Code Examples - Rust

FreeRTOS+TCP can be used from Rust through FFI bindings. Here are examples using the `freertos-rust` crate and custom bindings.

### Rust FFI Bindings Setup

```rust
// freertos_tcp_bindings.rs
use core::ffi::{c_char, c_int, c_uint, c_void};

// Socket types
pub const FREERTOS_AF_INET: c_int = 2;
pub const FREERTOS_SOCK_STREAM: c_int = 1;
pub const FREERTOS_SOCK_DGRAM: c_int = 2;
pub const FREERTOS_IPPROTO_TCP: c_int = 6;
pub const FREERTOS_IPPROTO_UDP: c_int = 17;

// Socket options
pub const FREERTOS_SO_RCVTIMEO: c_int = 0x1006;
pub const FREERTOS_SO_SNDTIMEO: c_int = 0x1005;
pub const FREERTOS_SO_REUSE_LISTEN_SOCKET: c_int = 0x0016;

// Invalid socket
pub const FREERTOS_INVALID_SOCKET: Socket_t = core::ptr::null_mut();

// Type definitions
pub type Socket_t = *mut c_void;
pub type TickType_t = u32;
pub type BaseType_t = i32;
pub type socklen_t = u32;

#[repr(C)]
pub struct freertos_sockaddr {
    pub sin_family: u8,
    pub sin_port: u16,
    pub sin_addr: u32,
    pub sin_zero: [u8; 8],
}

// External C functions
extern "C" {
    pub fn FreeRTOS_socket(
        domain: BaseType_t,
        sock_type: BaseType_t,
        protocol: BaseType_t,
    ) -> Socket_t;
    
    pub fn FreeRTOS_closesocket(socket: Socket_t) -> BaseType_t;
    
    pub fn FreeRTOS_bind(
        socket: Socket_t,
        address: *const freertos_sockaddr,
        address_len: socklen_t,
    ) -> BaseType_t;
    
    pub fn FreeRTOS_listen(
        socket: Socket_t,
        backlog: BaseType_t,
    ) -> BaseType_t;
    
    pub fn FreeRTOS_accept(
        socket: Socket_t,
        address: *mut freertos_sockaddr,
        address_len: *mut socklen_t,
    ) -> Socket_t;
    
    pub fn FreeRTOS_connect(
        socket: Socket_t,
        address: *const freertos_sockaddr,
        address_len: socklen_t,
    ) -> BaseType_t;
    
    pub fn FreeRTOS_send(
        socket: Socket_t,
        buffer: *const c_void,
        len: usize,
        flags: BaseType_t,
    ) -> BaseType_t;
    
    pub fn FreeRTOS_recv(
        socket: Socket_t,
        buffer: *mut c_void,
        len: usize,
        flags: BaseType_t,
    ) -> BaseType_t;
    
    pub fn FreeRTOS_sendto(
        socket: Socket_t,
        buffer: *const c_void,
        len: usize,
        flags: BaseType_t,
        dest_addr: *const freertos_sockaddr,
        dest_len: socklen_t,
    ) -> BaseType_t;
    
    pub fn FreeRTOS_recvfrom(
        socket: Socket_t,
        buffer: *mut c_void,
        len: usize,
        flags: BaseType_t,
        from_addr: *mut freertos_sockaddr,
        from_len: *mut socklen_t,
    ) -> BaseType_t;
    
    pub fn FreeRTOS_setsockopt(
        socket: Socket_t,
        level: BaseType_t,
        option_name: BaseType_t,
        option_value: *const c_void,
        option_len: socklen_t,
    ) -> BaseType_t;
    
    pub fn FreeRTOS_htons(host_short: u16) -> u16;
    pub fn FreeRTOS_htonl(host_long: u32) -> u32;
    pub fn FreeRTOS_inet_addr(cp: *const c_char) -> u32;
    pub fn FreeRTOS_gethostbyname(hostname: *const c_char) -> u32;
}
```

### TCP Client in Rust

```rust
// tcp_client.rs
use freertos_rust::{Task, Duration};
use crate::freertos_tcp_bindings::*;
use core::ffi::CString;

pub struct TcpClient {
    socket: Socket_t,
}

impl TcpClient {
    pub fn new() -> Result<Self, &'static str> {
        unsafe {
            let socket = FreeRTOS_socket(
                FREERTOS_AF_INET,
                FREERTOS_SOCK_STREAM,
                FREERTOS_IPPROTO_TCP,
            );
            
            if socket == FREERTOS_INVALID_SOCKET {
                return Err("Failed to create socket");
            }
            
            Ok(TcpClient { socket })
        }
    }
    
    pub fn set_timeout(&self, timeout_ms: u32) -> Result<(), &'static str> {
        unsafe {
            let timeout: TickType_t = timeout_ms;
            
            let result = FreeRTOS_setsockopt(
                self.socket,
                0,
                FREERTOS_SO_RCVTIMEO,
                &timeout as *const _ as *const _,
                core::mem::size_of::<TickType_t>() as socklen_t,
            );
            
            if result != 0 {
                return Err("Failed to set receive timeout");
            }
            
            let result = FreeRTOS_setsockopt(
                self.socket,
                0,
                FREERTOS_SO_SNDTIMEO,
                &timeout as *const _ as *const _,
                core::mem::size_of::<TickType_t>() as socklen_t,
            );
            
            if result != 0 {
                return Err("Failed to set send timeout");
            }
            
            Ok(())
        }
    }
    
    pub fn connect(&self, ip: &str, port: u16) -> Result<(), &'static str> {
        unsafe {
            let ip_cstring = CString::new(ip).unwrap();
            let ip_addr = FreeRTOS_inet_addr(ip_cstring.as_ptr());
            
            let server_addr = freertos_sockaddr {
                sin_family: FREERTOS_AF_INET as u8,
                sin_port: FreeRTOS_htons(port),
                sin_addr: ip_addr,
                sin_zero: [0; 8],
            };
            
            let result = FreeRTOS_connect(
                self.socket,
                &server_addr as *const _,
                core::mem::size_of::<freertos_sockaddr>() as socklen_t,
            );
            
            if result != 0 {
                return Err("Failed to connect");
            }
            
            Ok(())
        }
    }
    
    pub fn send(&self, data: &[u8]) -> Result<usize, &'static str> {
        unsafe {
            let bytes_sent = FreeRTOS_send(
                self.socket,
                data.as_ptr() as *const _,
                data.len(),
                0,
            );
            
            if bytes_sent < 0 {
                return Err("Send failed");
            }
            
            Ok(bytes_sent as usize)
        }
    }
    
    pub fn recv(&self, buffer: &mut [u8]) -> Result<usize, &'static str> {
        unsafe {
            let bytes_received = FreeRTOS_recv(
                self.socket,
                buffer.as_mut_ptr() as *mut _,
                buffer.len(),
                0,
            );
            
            if bytes_received < 0 {
                return Err("Receive failed");
            }
            
            Ok(bytes_received as usize)
        }
    }
}

impl Drop for TcpClient {
    fn drop(&mut self) {
        unsafe {
            FreeRTOS_closesocket(self.socket);
        }
    }
}

// Task function
pub fn tcp_client_task(_: ()) {
    match TcpClient::new() {
        Ok(client) => {
            // Set 10 second timeout
            if let Err(e) = client.set_timeout(10000) {
                println!("Failed to set timeout: {}", e);
                return;
            }
            
            // Connect to server
            if let Err(e) = client.connect("192.168.1.10", 8080) {
                println!("Failed to connect: {}", e);
                return;
            }
            
            println!("Connected to server");
            
            // Send message
            let message = b"Hello from Rust!";
            match client.send(message) {
                Ok(sent) => println!("Sent {} bytes", sent),
                Err(e) => println!("Send error: {}", e),
            }
            
            // Receive response
            let mut buffer = [0u8; 512];
            match client.recv(&mut buffer) {
                Ok(received) => {
                    let response = core::str::from_utf8(&buffer[..received])
                        .unwrap_or("<invalid UTF-8>");
                    println!("Received: {}", response);
                }
                Err(e) => println!("Receive error: {}", e),
            }
        }
        Err(e) => println!("Failed to create client: {}", e),
    }
}
```

### TCP Server in Rust

```rust
// tcp_server.rs
use freertos_rust::Task;
use crate::freertos_tcp_bindings::*;
use alloc::vec::Vec;

pub struct TcpServer {
    socket: Socket_t,
}

impl TcpServer {
    pub fn new(port: u16) -> Result<Self, &'static str> {
        unsafe {
            let socket = FreeRTOS_socket(
                FREERTOS_AF_INET,
                FREERTOS_SOCK_STREAM,
                FREERTOS_IPPROTO_TCP,
            );
            
            if socket == FREERTOS_INVALID_SOCKET {
                return Err("Failed to create socket");
            }
            
            // Set reuse option
            let reuse: BaseType_t = 1;
            FreeRTOS_setsockopt(
                socket,
                0,
                FREERTOS_SO_REUSE_LISTEN_SOCKET,
                &reuse as *const _ as *const _,
                core::mem::size_of::<BaseType_t>() as socklen_t,
            );
            
            // Bind
            let bind_addr = freertos_sockaddr {
                sin_family: FREERTOS_AF_INET as u8,
                sin_port: FreeRTOS_htons(port),
                sin_addr: 0, // INADDR_ANY
                sin_zero: [0; 8],
            };
            
            let result = FreeRTOS_bind(
                socket,
                &bind_addr as *const _,
                core::mem::size_of::<freertos_sockaddr>() as socklen_t,
            );
            
            if result != 0 {
                FreeRTOS_closesocket(socket);
                return Err("Failed to bind");
            }
            
            // Listen
            let result = FreeRTOS_listen(socket, 5);
            
            if result != 0 {
                FreeRTOS_closesocket(socket);
                return Err("Failed to listen");
            }
            
            Ok(TcpServer { socket })
        }
    }
    
    pub fn accept(&self) -> Result<TcpConnection, &'static str> {
        unsafe {
            let mut client_addr = core::mem::zeroed::<freertos_sockaddr>();
            let mut addr_len = core::mem::size_of::<freertos_sockaddr>() as socklen_t;
            
            let client_socket = FreeRTOS_accept(
                self.socket,
                &mut client_addr as *mut _,
                &mut addr_len as *mut _,
            );
            
            if client_socket == FREERTOS_INVALID_SOCKET {
                return Err("Accept failed");
            }
            
            Ok(TcpConnection {
                socket: client_socket,
            })
        }
    }
}

impl Drop for TcpServer {
    fn drop(&mut self) {
        unsafe {
            FreeRTOS_closesocket(self.socket);
        }
    }
}

pub struct TcpConnection {
    socket: Socket_t,
}

impl TcpConnection {
    pub fn set_timeout(&self, timeout_ms: u32) -> Result<(), &'static str> {
        unsafe {
            let timeout: TickType_t = timeout_ms;
            
            let result = FreeRTOS_setsockopt(
                self.socket,
                0,
                FREERTOS_SO_RCVTIMEO,
                &timeout as *const _ as *const _,
                core::mem::size_of::<TickType_t>() as socklen_t,
            );
            
            if result != 0 {
                return Err("Failed to set timeout");
            }
            
            Ok(())
        }
    }
    
    pub fn send(&self, data: &[u8]) -> Result<usize, &'static str> {
        unsafe {
            let bytes_sent = FreeRTOS_send(
                self.socket,
                data.as_ptr() as *const _,
                data.len(),
                0,
            );
            
            if bytes_sent < 0 {
                return Err("Send failed");
            }
            
            Ok(bytes_sent as usize)
        }
    }
    
    pub fn recv(&self, buffer: &mut [u8]) -> Result<usize, &'static str> {
        unsafe {
            let bytes_received = FreeRTOS_recv(
                self.socket,
                buffer.as_mut_ptr() as *mut _,
                buffer.len(),
                0,
            );
            
            if bytes_received < 0 {
                return Err("Receive failed");
            }
            
            Ok(bytes_received as usize)
        }
    }
}

impl Drop for TcpConnection {
    fn drop(&mut self) {
        unsafe {
            FreeRTOS_closesocket(self.socket);
        }
    }
}

// Server task
pub fn tcp_server_task(_: ()) {
    match TcpServer::new(8080) {
        Ok(server) => {
            println!("Server listening on port 8080");
            
            loop {
                match server.accept() {
                    Ok(connection) => {
                        println!("Client connected");
                        
                        // Set timeout
                        let _ = connection.set_timeout(30000);
                        
                        // Echo loop
                        let mut buffer = [0u8; 1024];
                        loop {
                            match connection.recv(&mut buffer) {
                                Ok(0) => break, // Connection closed
                                Ok(n) => {
                                    println!("Received {} bytes", n);
                                    let _ = connection.send(&buffer[..n]);
                                }
                                Err(_) => break,
                            }
                        }
                        
                        println!("Client disconnected");
                    }
                    Err(e) => println!("Accept error: {}", e),
                }
            }
        }
        Err(e) => println!("Failed to create server: {}", e),
    }
}
```

### UDP Socket in Rust

```rust
// udp_socket.rs
use crate::freertos_tcp_bindings::*;

pub struct UdpSocket {
    socket: Socket_t,
}

impl UdpSocket {
    pub fn new() -> Result<Self, &'static str> {
        unsafe {
            let socket = FreeRTOS_socket(
                FREERTOS_AF_INET,
                FREERTOS_SOCK_DGRAM,
                FREERTOS_IPPROTO_UDP,
            );
            
            if socket == FREERTOS_INVALID_SOCKET {
                return Err("Failed to create socket");
            }
            
            Ok(UdpSocket { socket })
        }
    }
    
    pub fn bind(&self, port: u16) -> Result<(), &'static str> {
        unsafe {
            let bind_addr = freertos_sockaddr {
                sin_family: FREERTOS_AF_INET as u8,
                sin_port: FreeRTOS_htons(port),
                sin_addr: 0, // INADDR_ANY
                sin_zero: [0; 8],
            };
            
            let result = FreeRTOS_bind(
                self.socket,
                &bind_addr as *const _,
                core::mem::size_of::<freertos_sockaddr>() as socklen_t,
            );
            
            if result != 0 {
                return Err("Bind failed");
            }
            
            Ok(())
        }
    }
    
    pub fn send_to(&self, data: &[u8], ip: u32, port: u16) 
        -> Result<usize, &'static str> 
    {
        unsafe {
            let dest_addr = freertos_sockaddr {
                sin_family: FREERTOS_AF_INET as u8,
                sin_port: FreeRTOS_htons(port),
                sin_addr: ip,
                sin_zero: [0; 8],
            };
            
            let bytes_sent = FreeRTOS_sendto(
                self.socket,
                data.as_ptr() as *const _,
                data.len(),
                0,
                &dest_addr as *const _,
                core::mem::size_of::<freertos_sockaddr>() as socklen_t,
            );
            
            if bytes_sent < 0 {
                return Err("Send failed");
            }
            
            Ok(bytes_sent as usize)
        }
    }
    
    pub fn recv_from(&self, buffer: &mut [u8]) 
        -> Result<(usize, u32, u16), &'static str> 
    {
        unsafe {
            let mut from_addr = core::mem::zeroed::<freertos_sockaddr>();
            let mut from_len = core::mem::size_of::<freertos_sockaddr>() as socklen_t;
            
            let bytes_received = FreeRTOS_recvfrom(
                self.socket,
                buffer.as_mut_ptr() as *mut _,
                buffer.len(),
                0,
                &mut from_addr as *mut _,
                &mut from_len as *mut _,
            );
            
            if bytes_received < 0 {
                return Err("Receive failed");
            }
            
            let port = FreeRTOS_htons(from_addr.sin_port);
            
            Ok((bytes_received as usize, from_addr.sin_addr, port))
        }
    }
}

impl Drop for UdpSocket {
    fn drop(&mut self) {
        unsafe {
            FreeRTOS_closesocket(self.socket);
        }
    }
}

// UDP echo server task
pub fn udp_echo_task(_: ()) {
    match UdpSocket::new() {
        Ok(socket) => {
            if let Err(e) = socket.bind(9000) {
                println!("Failed to bind: {}", e);
                return;
            }
            
            println!("UDP server listening on port 9000");
            
            let mut buffer = [0u8; 512];
            
            loop {
                match socket.recv_from(&mut buffer) {
                    Ok((n, ip, port)) => {
                        println!("Received {} bytes from {}:{}", n, ip, port);
                        let _ = socket.send_to(&buffer[..n], ip, port);
                    }
                    Err(e) => println!("Receive error: {}", e),
                }
            }
        }
        Err(e) => println!("Failed to create socket: {}", e),
    }
}
```

---

## Best Practices

### 1. Buffer Management

```c
/* Always check buffer allocation */
NetworkBufferDescriptor_t *pxBuffer = 
    pxGetNetworkBufferWithDescriptor(ulLength, 0);

if (pxBuffer != NULL)
{
    /* Use buffer */
    /* ... */
    
    /* Release when done */
    vReleaseNetworkBufferAndDescriptor(pxBuffer);
}
```

### 2. Error Handling

```c
/* Always check return values */
Socket_t xSocket = FreeRTOS_socket(...);
if (xSocket == FREERTOS_INVALID_SOCKET)
{
    /* Handle error */
}

BaseType_t xResult = FreeRTOS_connect(...);
if (xResult != 0)
{
    /* Handle connection failure */
    FreeRTOS_closesocket(xSocket);
}
```

### 3. Timeout Configuration

```c
/* Always set timeouts to prevent blocking indefinitely */
TickType_t xTimeout = pdMS_TO_TICKS(5000);
FreeRTOS_setsockopt(xSocket, 0, FREERTOS_SO_RCVTIMEO,
                   &xTimeout, sizeof(xTimeout));
```

### 4. Resource Cleanup

```c
/* Always close sockets */
if (xSocket != FREERTOS_INVALID_SOCKET)
{
    FreeRTOS_closesocket(xSocket);
}

/* Release network buffers */
if (pxBuffer != NULL)
{
    vReleaseNetworkBufferAndDescriptor(pxBuffer);
}
```

### 5. Thread Safety

```c
/* Use mutexes for shared resources */
SemaphoreHandle_t xSocketMutex = xSemaphoreCreateMutex();

xSemaphoreTake(xSocketMutex, portMAX_DELAY);
/* Access shared socket */
xSemaphoreGive(xSocketMutex);
```

### 6. Memory Efficiency

```c
/* Use appropriate buffer sizes */
#define SMALL_BUFFER    256
#define MEDIUM_BUFFER   1024
#define LARGE_BUFFER    4096

/* Allocate based on expected data size */
uint8_t *pucBuffer = pvPortMalloc(MEDIUM_BUFFER);
```

### 7. Network Monitoring

```c
/* Implement link change detection */
void vNetworkMonitorTask(void *pvParameters)
{
    BaseType_t xLastLinkStatus = pdFALSE;
    
    for (;;)
    {
        BaseType_t xLinkStatus = xGetPhyLinkStatus(pxInterface);
        
        if (xLinkStatus != xLastLinkStatus)
        {
            if (xLinkStatus == pdTRUE)
            {
                printf("Link up\n");
                FreeRTOS_NetworkDown(pxInterface);
                vTaskDelay(pdMS_TO_TICKS(100));
                FreeRTOS_NetworkUp(pxInterface);
            }
            else
            {
                printf("Link down\n");
                FreeRTOS_NetworkDown(pxInterface);
            }
            
            xLastLinkStatus = xLinkStatus;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

---

## Summary

**FreeRTOS+TCP** is a comprehensive, production-ready TCP/IP stack designed for embedded systems running FreeRTOS. Key takeaways:

### Core Strengths
- **Integrated Design**: Seamlessly integrates with FreeRTOS kernel, leveraging tasks, queues, and semaphores
- **Zero-Copy Architecture**: Minimizes memory overhead and improves performance
- **BSD Socket API**: Familiar programming interface for developers
- **Highly Configurable**: Scalable from small IoT devices to complex networked systems
- **Thread-Safe**: All APIs designed for multi-threaded environments

### Essential Components
1. **IP Task**: Central event processing loop
2. **Network Buffers**: Efficient packet management system
3. **Socket Layer**: Application programming interface
4. **Protocol Stack**: Complete TCP/UDP/IP/ICMP/ARP implementation
5. **Network Interface**: Hardware abstraction layer

### Configuration Highlights
- Flexible compile-time configuration via `FreeRTOSIPConfig.h`
- Support for DHCP, static IP, DNS
- Tunable buffer pools and TCP windows
- Optional features for resource optimization

### Programming Models
- **C/C++**: Native API with direct hardware access
- **Rust**: Safe abstractions through FFI bindings
- **Socket Programming**: TCP clients/servers, UDP communication
- **Asynchronous I/O**: Non-blocking operations with timeouts

### Best Practices
- Always check return values and handle errors
- Set appropriate timeouts to prevent deadlocks
- Properly manage network buffers (allocation/deallocation)
- Implement link monitoring for robust operation
- Use thread-safe patterns for shared resources
- Configure buffer pools based on application needs

### Use Cases
- **IoT Devices**: Sensor networks, smart home devices
- **Industrial Control**: Factory automation, SCADA systems
- **Medical Devices**: Connected healthcare equipment
- **Consumer Electronics**: Smart appliances, wearables
- **Automotive**: Connected car systems

FreeRTOS+TCP provides embedded developers with a reliable, efficient networking solution that balances features, performance, and resource usage for real-time embedded applications.