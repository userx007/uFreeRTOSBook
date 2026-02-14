# TLS/SSL and Secure Sockets in FreeRTOS

## Overview

TLS/SSL (Transport Layer Security/Secure Sockets Layer) provides encrypted communications for embedded systems running FreeRTOS. This is critical for IoT devices that transmit sensitive data over networks. The most common approach is integrating **mbedTLS** (formerly PolarSSL), a lightweight cryptographic library designed for resource-constrained devices.

## Key Concepts

### 1. **Why TLS/SSL in FreeRTOS?**
- **Data Encryption**: Protect data in transit from eavesdropping
- **Authentication**: Verify server/client identity using certificates
- **Integrity**: Detect tampering with cryptographic hashes
- **Compliance**: Meet security standards (GDPR, HIPAA, etc.)

### 2. **mbedTLS Architecture**
- **Modular design**: Only include needed cryptographic primitives
- **Small footprint**: Configurable to fit limited RAM/Flash
- **RTOS integration**: Thread-safe with proper locking mechanisms
- **Hardware acceleration**: Can leverage crypto accelerators

### 3. **Challenges in RTOS Context**
- **Memory constraints**: TLS handshakes require significant buffers
- **Stack usage**: Deep call stacks during crypto operations
- **Timing**: Real-time requirements vs. crypto overhead
- **Entropy sources**: Generating cryptographically secure random numbers

---

## C/C++ Implementation Examples

### Example 1: Basic mbedTLS Integration with FreeRTOS

```c
/* mbedtls_config.h - Custom configuration for embedded systems */
#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

/* System support */
#define MBEDTLS_HAVE_TIME
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY

/* mbedTLS feature selection */
#define MBEDTLS_SSL_TLS_C
#define MBEDTLS_SSL_CLI_C
#define MBEDTLS_SSL_SRV_C
#define MBEDTLS_NET_C
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_PEM_PARSE_C
#define MBEDTLS_RSA_C
#define MBEDTLS_AES_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_CTR_DRBG_C

/* Memory optimization */
#define MBEDTLS_SSL_MAX_CONTENT_LEN 4096

/* Threading support for FreeRTOS */
#define MBEDTLS_THREADING_C
#define MBEDTLS_THREADING_ALT

#endif /* MBEDTLS_CONFIG_H */
```

```c
/* freertos_threading.c - Thread safety implementation */
#include "mbedtls/threading.h"
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct {
    SemaphoreHandle_t mutex;
    StaticSemaphore_t mutex_buffer;
} freertos_mutex_t;

void freertos_mutex_init(mbedtls_threading_mutex_t *mutex)
{
    if (mutex == NULL || mutex->mutex != NULL)
        return;
    
    freertos_mutex_t *fm = pvPortMalloc(sizeof(freertos_mutex_t));
    if (fm != NULL) {
        fm->mutex = xSemaphoreCreateMutexStatic(&fm->mutex_buffer);
        mutex->mutex = fm;
    }
}

void freertos_mutex_free(mbedtls_threading_mutex_t *mutex)
{
    if (mutex == NULL || mutex->mutex == NULL)
        return;
    
    freertos_mutex_t *fm = (freertos_mutex_t *)mutex->mutex;
    vSemaphoreDelete(fm->mutex);
    vPortFree(fm);
    mutex->mutex = NULL;
}

int freertos_mutex_lock(mbedtls_threading_mutex_t *mutex)
{
    if (mutex == NULL || mutex->mutex == NULL)
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    
    freertos_mutex_t *fm = (freertos_mutex_t *)mutex->mutex;
    if (xSemaphoreTake(fm->mutex, portMAX_DELAY) != pdTRUE)
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    
    return 0;
}

int freertos_mutex_unlock(mbedtls_threading_mutex_t *mutex)
{
    if (mutex == NULL || mutex->mutex == NULL)
        return MBEDTLS_ERR_THREADING_BAD_INPUT_DATA;
    
    freertos_mutex_t *fm = (freertos_mutex_t *)mutex->mutex;
    if (xSemaphoreGive(fm->mutex) != pdTRUE)
        return MBEDTLS_ERR_THREADING_MUTEX_ERROR;
    
    return 0;
}

/* Initialize threading for mbedTLS */
void mbedtls_threading_setup(void)
{
    mbedtls_threading_set_alt(
        freertos_mutex_init,
        freertos_mutex_free,
        freertos_mutex_lock,
        freertos_mutex_unlock
    );
}
```

### Example 2: Secure TLS Client Implementation

```c
/* tls_client.c - Secure HTTPS client task */
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "FreeRTOS.h"
#include "task.h"

#define SERVER_PORT "443"
#define SERVER_NAME "api.example.com"
#define GET_REQUEST "GET /data HTTP/1.1\r\nHost: api.example.com\r\n\r\n"

/* Root CA certificate (PEM format) */
const char *ca_cert = 
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n"
    /* ... certificate data ... */
    "-----END CERTIFICATE-----\n";

typedef struct {
    mbedtls_net_context server_fd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
} tls_client_context_t;

int tls_client_init(tls_client_context_t *ctx)
{
    int ret;
    const char *pers = "tls_client";
    
    /* Initialize structures */
    mbedtls_net_init(&ctx->server_fd);
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_x509_crt_init(&ctx->cacert);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
    mbedtls_entropy_init(&ctx->entropy);
    
    /* Seed random number generator */
    ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func,
                                 &ctx->entropy, (const unsigned char *)pers,
                                 strlen(pers));
    if (ret != 0) {
        return ret;
    }
    
    /* Parse CA certificate */
    ret = mbedtls_x509_crt_parse(&ctx->cacert, 
                                  (const unsigned char *)ca_cert,
                                  strlen(ca_cert) + 1);
    if (ret < 0) {
        return ret;
    }
    
    /* Setup SSL/TLS configuration */
    ret = mbedtls_ssl_config_defaults(&ctx->conf,
                                       MBEDTLS_SSL_IS_CLIENT,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        return ret;
    }
    
    /* Configure authentication and CA chain */
    mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->cacert, NULL);
    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    
    /* Setup SSL context */
    ret = mbedtls_ssl_setup(&ctx->ssl, &ctx->conf);
    if (ret != 0) {
        return ret;
    }
    
    /* Set hostname for SNI */
    ret = mbedtls_ssl_set_hostname(&ctx->ssl, SERVER_NAME);
    if (ret != 0) {
        return ret;
    }
    
    return 0;
}

int tls_client_connect(tls_client_context_t *ctx)
{
    int ret;
    char error_buf[100];
    
    /* Connect to server */
    ret = mbedtls_net_connect(&ctx->server_fd, SERVER_NAME,
                               SERVER_PORT, MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) {
        return ret;
    }
    
    /* Set BIO callbacks */
    mbedtls_ssl_set_bio(&ctx->ssl, &ctx->server_fd,
                        mbedtls_net_send, mbedtls_net_recv, NULL);
    
    /* Perform TLS handshake */
    while ((ret = mbedtls_ssl_handshake(&ctx->ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && 
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            mbedtls_strerror(ret, error_buf, sizeof(error_buf));
            printf("TLS handshake failed: %s\n", error_buf);
            return ret;
        }
        /* Yield to other tasks during handshake */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    /* Verify server certificate */
    uint32_t flags = mbedtls_ssl_get_verify_result(&ctx->ssl);
    if (flags != 0) {
        char vrfy_buf[512];
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), 
                                      "  ! ", flags);
        printf("Certificate verification failed:\n%s\n", vrfy_buf);
        return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    }
    
    printf("TLS connection established successfully\n");
    return 0;
}

int tls_client_send_request(tls_client_context_t *ctx)
{
    int ret, len;
    unsigned char buf[1024];
    
    /* Send HTTP request */
    len = sprintf((char *)buf, GET_REQUEST);
    while ((ret = mbedtls_ssl_write(&ctx->ssl, buf, len)) <= 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && 
            ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            return ret;
        }
    }
    
    printf("Sent %d bytes\n", ret);
    
    /* Read response */
    do {
        len = sizeof(buf) - 1;
        memset(buf, 0, sizeof(buf));
        ret = mbedtls_ssl_read(&ctx->ssl, buf, len);
        
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || 
            ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }
        
        if (ret <= 0) {
            break;
        }
        
        printf("Received %d bytes:\n%s\n", ret, buf);
    } while (1);
    
    return 0;
}

void tls_client_cleanup(tls_client_context_t *ctx)
{
    mbedtls_ssl_close_notify(&ctx->ssl);
    mbedtls_net_free(&ctx->server_fd);
    mbedtls_x509_crt_free(&ctx->cacert);
    mbedtls_ssl_free(&ctx->ssl);
    mbedtls_ssl_config_free(&ctx->conf);
    mbedtls_ctr_drbg_free(&ctx->ctr_drbg);
    mbedtls_entropy_free(&ctx->entropy);
}

/* FreeRTOS task */
void tls_client_task(void *pvParameters)
{
    tls_client_context_t ctx;
    int ret;
    
    /* Initialize mbedTLS threading support */
    mbedtls_threading_setup();
    
    while (1) {
        ret = tls_client_init(&ctx);
        if (ret == 0) {
            ret = tls_client_connect(&ctx);
            if (ret == 0) {
                tls_client_send_request(&ctx);
            }
        }
        
        tls_client_cleanup(&ctx);
        
        /* Wait before next connection */
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}
```

### Example 3: Certificate Management and Storage

```c
/* cert_manager.c - Certificate storage in flash */
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "esp_partition.h"  // Example: ESP32 flash partition
#include <string.h>

#define CERT_PARTITION_LABEL "certs"
#define MAX_CERT_SIZE 4096

typedef struct {
    char device_cert[MAX_CERT_SIZE];
    char device_key[MAX_CERT_SIZE];
    char ca_cert[MAX_CERT_SIZE];
    uint32_t magic;
    uint32_t crc;
} cert_storage_t;

/* Store certificates in flash */
int cert_manager_store(const char *device_cert, const char *device_key,
                       const char *ca_cert)
{
    const esp_partition_t *partition;
    cert_storage_t cert_data = {0};
    
    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                          ESP_PARTITION_SUBTYPE_ANY,
                                          CERT_PARTITION_LABEL);
    if (partition == NULL) {
        return -1;
    }
    
    /* Copy certificate data */
    strncpy(cert_data.device_cert, device_cert, MAX_CERT_SIZE - 1);
    strncpy(cert_data.device_key, device_key, MAX_CERT_SIZE - 1);
    strncpy(cert_data.ca_cert, ca_cert, MAX_CERT_SIZE - 1);
    cert_data.magic = 0xDEADBEEF;
    
    /* Calculate CRC (simplified) */
    cert_data.crc = calculate_crc32((uint8_t *)&cert_data, 
                                     sizeof(cert_data) - sizeof(uint32_t));
    
    /* Erase and write partition */
    esp_partition_erase_range(partition, 0, partition->size);
    esp_partition_write(partition, 0, &cert_data, sizeof(cert_data));
    
    return 0;
}

/* Load certificates from flash */
int cert_manager_load(mbedtls_x509_crt *device_cert, mbedtls_pk_context *device_key,
                      mbedtls_x509_crt *ca_cert)
{
    const esp_partition_t *partition;
    cert_storage_t cert_data;
    int ret;
    
    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                          ESP_PARTITION_SUBTYPE_ANY,
                                          CERT_PARTITION_LABEL);
    if (partition == NULL) {
        return -1;
    }
    
    /* Read certificate data */
    esp_partition_read(partition, 0, &cert_data, sizeof(cert_data));
    
    /* Verify magic and CRC */
    if (cert_data.magic != 0xDEADBEEF) {
        return -2;
    }
    
    uint32_t calculated_crc = calculate_crc32((uint8_t *)&cert_data,
                                               sizeof(cert_data) - sizeof(uint32_t));
    if (calculated_crc != cert_data.crc) {
        return -3;
    }
    
    /* Parse certificates */
    ret = mbedtls_x509_crt_parse(device_cert,
                                  (const unsigned char *)cert_data.device_cert,
                                  strlen(cert_data.device_cert) + 1);
    if (ret != 0) {
        return ret;
    }
    
    ret = mbedtls_pk_parse_key(device_key,
                                (const unsigned char *)cert_data.device_key,
                                strlen(cert_data.device_key) + 1,
                                NULL, 0);
    if (ret != 0) {
        return ret;
    }
    
    ret = mbedtls_x509_crt_parse(ca_cert,
                                  (const unsigned char *)cert_data.ca_cert,
                                  strlen(cert_data.ca_cert) + 1);
    
    return ret;
}
```

### Example 4: Hardware Entropy Source

```c
/* entropy_hardware.c - Hardware RNG integration */
#include "mbedtls/entropy.h"
#include "driver/rtc_io.h"  // Example: ESP32 hardware RNG

/* Hardware entropy callback for mbedTLS */
int hardware_entropy_source(void *data, unsigned char *output, 
                            size_t len, size_t *olen)
{
    uint32_t random_value;
    size_t i;
    
    for (i = 0; i < len; i += sizeof(uint32_t)) {
        /* Read hardware random number generator */
        random_value = esp_random();  // ESP32 hardware RNG
        
        size_t copy_len = (len - i) < sizeof(uint32_t) ? 
                          (len - i) : sizeof(uint32_t);
        memcpy(output + i, &random_value, copy_len);
    }
    
    *olen = len;
    return 0;
}

/* Register hardware entropy source */
void entropy_init_hardware(mbedtls_entropy_context *entropy)
{
    mbedtls_entropy_add_source(entropy, hardware_entropy_source,
                               NULL, 32, MBEDTLS_ENTROPY_SOURCE_STRONG);
}
```

### Example 5: TLS Server with Client Authentication

```c
/* tls_server.c - Mutual TLS authentication */
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"

typedef struct {
    mbedtls_net_context listen_fd;
    mbedtls_net_context client_fd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt srvcert;
    mbedtls_pk_context pkey;
    mbedtls_x509_crt cacert;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
} tls_server_context_t;

int tls_server_setup(tls_server_context_t *ctx)
{
    int ret;
    const char *pers = "tls_server";
    
    mbedtls_net_init(&ctx->listen_fd);
    mbedtls_net_init(&ctx->client_fd);
    mbedtls_ssl_init(&ctx->ssl);
    mbedtls_ssl_config_init(&ctx->conf);
    mbedtls_x509_crt_init(&ctx->srvcert);
    mbedtls_pk_init(&ctx->pkey);
    mbedtls_x509_crt_init(&ctx->cacert);
    mbedtls_entropy_init(&ctx->entropy);
    mbedtls_ctr_drbg_init(&ctx->ctr_drbg);
    
    /* Seed RNG */
    ret = mbedtls_ctr_drbg_seed(&ctx->ctr_drbg, mbedtls_entropy_func,
                                 &ctx->entropy, (const unsigned char *)pers,
                                 strlen(pers));
    if (ret != 0) return ret;
    
    /* Load server certificate and private key */
    ret = cert_manager_load(&ctx->srvcert, &ctx->pkey, &ctx->cacert);
    if (ret != 0) return ret;
    
    /* Configure SSL/TLS */
    ret = mbedtls_ssl_config_defaults(&ctx->conf,
                                       MBEDTLS_SSL_IS_SERVER,
                                       MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) return ret;
    
    /* Require client authentication */
    mbedtls_ssl_conf_authmode(&ctx->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&ctx->conf, &ctx->cacert, NULL);
    mbedtls_ssl_conf_own_cert(&ctx->conf, &ctx->srvcert, &ctx->pkey);
    mbedtls_ssl_conf_rng(&ctx->conf, mbedtls_ctr_drbg_random, &ctx->ctr_drbg);
    
    /* Bind to port */
    ret = mbedtls_net_bind(&ctx->listen_fd, NULL, "8443", 
                           MBEDTLS_NET_PROTO_TCP);
    if (ret != 0) return ret;
    
    return 0;
}

void tls_server_task(void *pvParameters)
{
    tls_server_context_t ctx;
    int ret;
    unsigned char buf[1024];
    
    tls_server_setup(&ctx);
    
    while (1) {
        /* Accept client connection */
        ret = mbedtls_net_accept(&ctx.listen_fd, &ctx.client_fd,
                                  NULL, 0, NULL);
        if (ret != 0) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        /* Setup SSL context for this client */
        mbedtls_ssl_session_reset(&ctx.ssl);
        mbedtls_ssl_set_bio(&ctx.ssl, &ctx.client_fd,
                            mbedtls_net_send, mbedtls_net_recv, NULL);
        
        /* Perform handshake */
        while ((ret = mbedtls_ssl_handshake(&ctx.ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && 
                ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                printf("TLS handshake failed: %d\n", ret);
                break;
            }
        }
        
        if (ret == 0) {
            /* Verify client certificate */
            const mbedtls_x509_crt *client_cert = mbedtls_ssl_get_peer_cert(&ctx.ssl);
            if (client_cert != NULL) {
                printf("Client authenticated: %s\n", 
                       client_cert->subject.val.p);
                
                /* Handle client communication */
                while (1) {
                    ret = mbedtls_ssl_read(&ctx.ssl, buf, sizeof(buf) - 1);
                    if (ret <= 0) break;
                    
                    buf[ret] = '\0';
                    printf("Received: %s\n", buf);
                    
                    /* Echo response */
                    mbedtls_ssl_write(&ctx.ssl, buf, ret);
                }
            }
        }
        
        mbedtls_ssl_close_notify(&ctx.ssl);
        mbedtls_net_free(&ctx.client_fd);
    }
}
```

---

## Rust Implementation Examples

### Example 1: Rust TLS Client with embedded-tls

```rust
// Cargo.toml dependencies
// [dependencies]
// embedded-tls = "0.17"
// embassy-net = "0.4"
// embassy-executor = "0.5"

use embedded_tls::{TlsConfig, TlsConnection, TlsContext, Aes128GcmSha256};
use embassy_net::tcp::TcpSocket;
use embassy_executor::Spawner;
use embedded_io_async::{Read, Write};

const CA_CERT: &[u8] = include_bytes!("../certs/ca.der");
const SERVER_NAME: &str = "api.example.com";

#[embassy_executor::task]
async fn tls_client_task(spawner: Spawner) {
    let mut rx_buffer = [0; 4096];
    let mut tx_buffer = [0; 4096];
    
    // Create TCP socket
    let mut socket = TcpSocket::new(
        stack,
        &mut rx_buffer,
        &mut tx_buffer
    );
    
    // Connect to server
    let remote_endpoint = (SERVER_NAME, 443);
    socket.connect(remote_endpoint).await.unwrap();
    
    // Setup TLS configuration
    let mut read_record_buffer = [0; 16384];
    let mut write_record_buffer = [0; 16384];
    
    let config = TlsConfig::new()
        .with_ca(CA_CERT)
        .with_server_name(SERVER_NAME);
    
    let mut tls = TlsConnection::<_, Aes128GcmSha256>::new(
        socket,
        &mut read_record_buffer,
        &mut write_record_buffer,
    );
    
    // Perform TLS handshake
    tls.open(TlsContext::new(&config, &mut OsRng))
        .await
        .unwrap();
    
    // Send HTTPS request
    let request = b"GET /data HTTP/1.1\r\nHost: api.example.com\r\n\r\n";
    tls.write_all(request).await.unwrap();
    
    // Read response
    let mut buffer = [0u8; 1024];
    loop {
        match tls.read(&mut buffer).await {
            Ok(0) => break, // Connection closed
            Ok(n) => {
                let response = core::str::from_utf8(&buffer[..n]).unwrap();
                defmt::info!("Received: {}", response);
            }
            Err(e) => {
                defmt::error!("Read error: {:?}", e);
                break;
            }
        }
    }
    
    tls.close().await.unwrap();
}
```

### Example 2: Rust Certificate Verification

```rust
use embedded_tls::webpki::{EndEntityCert, TrustAnchor};
use embedded_tls::parse_buffer::ParseBuffer;

struct CertificateValidator {
    trust_anchors: &'static [TrustAnchor<'static>],
}

impl CertificateValidator {
    pub fn new(ca_certs: &'static [&'static [u8]]) -> Self {
        let mut anchors = heapless::Vec::<TrustAnchor, 8>::new();
        
        for cert_der in ca_certs {
            if let Ok(ta) = webpki::trust_anchor_util::cert_der_as_trust_anchor(cert_der) {
                let _ = anchors.push(ta);
            }
        }
        
        Self {
            trust_anchors: anchors.leak(),
        }
    }
    
    pub fn verify_server_cert(
        &self,
        server_cert_der: &[u8],
        server_name: &str,
        timestamp: u64,
    ) -> Result<(), CertError> {
        let cert = EndEntityCert::try_from(server_cert_der)
            .map_err(|_| CertError::ParseError)?;
        
        let time = webpki::Time::from_seconds_since_unix_epoch(timestamp);
        
        cert.verify_is_valid_tls_server_cert(
            SUPPORTED_SIG_ALGS,
            &webpki::TlsServerTrustAnchors(self.trust_anchors),
            &[],
            time,
        )
        .map_err(|_| CertError::VerificationFailed)?;
        
        cert.verify_is_valid_for_dns_name(
            webpki::DnsNameRef::try_from_ascii_str(server_name)
                .map_err(|_| CertError::InvalidServerName)?
        )
        .map_err(|_| CertError::NameMismatch)?;
        
        Ok(())
    }
}

#[derive(Debug)]
enum CertError {
    ParseError,
    VerificationFailed,
    InvalidServerName,
    NameMismatch,
}

static SUPPORTED_SIG_ALGS: &[&webpki::SignatureAlgorithm] = &[
    &webpki::RSA_PKCS1_2048_8192_SHA256,
    &webpki::ECDSA_P256_SHA256,
    &webpki::ECDSA_P384_SHA384,
];
```

### Example 3: Rust Secure Storage

```rust
use embassy_sync::blocking_mutex::raw::CriticalSectionRawMutex;
use embassy_sync::mutex::Mutex;
use embedded_storage::nor_flash::{NorFlash, ReadNorFlash};

const CERT_FLASH_OFFSET: u32 = 0x10000;
const MAX_CERT_SIZE: usize = 4096;

pub struct SecureCertStore<F: NorFlash> {
    flash: Mutex<CriticalSectionRawMutex, F>,
}

impl<F: NorFlash> SecureCertStore<F> {
    pub fn new(flash: F) -> Self {
        Self {
            flash: Mutex::new(flash),
        }
    }
    
    pub async fn store_certificate(
        &self,
        cert_type: CertType,
        cert_data: &[u8],
    ) -> Result<(), StoreError> {
        if cert_data.len() > MAX_CERT_SIZE {
            return Err(StoreError::CertTooLarge);
        }
        
        let offset = CERT_FLASH_OFFSET + (cert_type as u32 * MAX_CERT_SIZE as u32);
        
        let mut flash = self.flash.lock().await;
        
        // Erase sector
        flash.erase(offset, offset + MAX_CERT_SIZE as u32)
            .map_err(|_| StoreError::FlashError)?;
        
        // Write certificate with length header
        let len_bytes = (cert_data.len() as u32).to_le_bytes();
        flash.write(offset, &len_bytes)
            .map_err(|_| StoreError::FlashError)?;
        flash.write(offset + 4, cert_data)
            .map_err(|_| StoreError::FlashError)?;
        
        Ok(())
    }
    
    pub async fn load_certificate(
        &self,
        cert_type: CertType,
        buffer: &mut [u8],
    ) -> Result<usize, StoreError> {
        let offset = CERT_FLASH_OFFSET + (cert_type as u32 * MAX_CERT_SIZE as u32);
        
        let mut flash = self.flash.lock().await;
        
        // Read length header
        let mut len_bytes = [0u8; 4];
        flash.read(offset, &mut len_bytes)
            .map_err(|_| StoreError::FlashError)?;
        
        let cert_len = u32::from_le_bytes(len_bytes) as usize;
        
        if cert_len > buffer.len() || cert_len > MAX_CERT_SIZE {
            return Err(StoreError::BufferTooSmall);
        }
        
        // Read certificate data
        flash.read(offset + 4, &mut buffer[..cert_len])
            .map_err(|_| StoreError::FlashError)?;
        
        Ok(cert_len)
    }
}

#[derive(Copy, Clone)]
#[repr(u32)]
pub enum CertType {
    DeviceCert = 0,
    DeviceKey = 1,
    CaCert = 2,
}

#[derive(Debug)]
pub enum StoreError {
    CertTooLarge,
    BufferTooSmall,
    FlashError,
}
```

### Example 4: Rust Hardware Random Number Generator

```rust
use rand_core::{RngCore, CryptoRng};
use embassy_stm32::rng::Rng;

pub struct HardwareRng<'d> {
    rng: Rng<'d>,
}

impl<'d> HardwareRng<'d> {
    pub fn new(rng: Rng<'d>) -> Self {
        Self { rng }
    }
}

impl<'d> RngCore for HardwareRng<'d> {
    fn next_u32(&mut self) -> u32 {
        let mut bytes = [0u8; 4];
        self.fill_bytes(&mut bytes);
        u32::from_le_bytes(bytes)
    }
    
    fn next_u64(&mut self) -> u64 {
        let mut bytes = [0u8; 8];
        self.fill_bytes(&mut bytes);
        u64::from_le_bytes(bytes)
    }
    
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        for chunk in dest.chunks_mut(4) {
            let random = self.rng.next_u32();
            let bytes = random.to_le_bytes();
            chunk.copy_from_slice(&bytes[..chunk.len()]);
        }
    }
    
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), rand_core::Error> {
        self.fill_bytes(dest);
        Ok(())
    }
}

// Marker trait to indicate this is cryptographically secure
impl<'d> CryptoRng for HardwareRng<'d> {}

// Usage example
async fn generate_tls_random(rng: &mut HardwareRng<'_>) -> [u8; 32] {
    let mut random_bytes = [0u8; 32];
    rng.fill_bytes(&mut random_bytes);
    random_bytes
}
```

### Example 5: Complete Rust HTTPS Client with Embassy

```rust
#![no_std]
#![no_main]

use defmt::*;
use embassy_executor::Spawner;
use embassy_net::{Stack, StackResources};
use embassy_stm32::eth::{Ethernet, PacketQueue};
use embassy_stm32::peripherals::ETH;
use embassy_stm32::rng::Rng;
use embassy_time::{Duration, Timer};
use embedded_io_async::Write;
use embedded_tls::*;
use static_cell::StaticCell;

static CA_CERT: &[u8] = include_bytes!("../certs/ca.der");

#[embassy_executor::task]
async fn net_task(stack: &'static Stack<Ethernet<'static, ETH>>) -> ! {
    stack.run().await
}

#[embassy_executor::main]
async fn main(spawner: Spawner) {
    let p = embassy_stm32::init(Default::default());
    
    // Initialize RNG
    let mut rng = Rng::new(p.RNG);
    
    // Setup Ethernet
    static PACKETS: StaticCell<PacketQueue<16, 16>> = StaticCell::new();
    let device = Ethernet::new(
        PACKETS.init(PacketQueue::new()),
        p.ETH,
        // ... Ethernet pin configuration
    );
    
    // Network stack
    let config = embassy_net::Config::dhcpv4(Default::default());
    static STACK: StaticCell<Stack<Ethernet<'static, ETH>>> = StaticCell::new();
    static RESOURCES: StaticCell<StackResources<3>> = StaticCell::new();
    let stack = &*STACK.init(Stack::new(
        device,
        config,
        RESOURCES.init(StackResources::new()),
        rng.next_u64(),
    ));
    
    spawner.spawn(net_task(stack)).unwrap();
    
    // Wait for network
    stack.wait_config_up().await;
    info!("Network configured!");
    
    loop {
        if let Err(e) = https_request(stack, &mut rng).await {
            error!("HTTPS request failed: {:?}", e);
        }
        
        Timer::after(Duration::from_secs(60)).await;
    }
}

async fn https_request<'a>(
    stack: &Stack<Ethernet<'static, ETH>>,
    rng: &mut Rng<'a>,
) -> Result<(), TlsError> {
    let mut rx_buffer = [0; 4096];
    let mut tx_buffer = [0; 4096];
    
    let mut socket = embassy_net::tcp::TcpSocket::new(
        stack,
        &mut rx_buffer,
        &mut tx_buffer,
    );
    
    socket.set_timeout(Some(Duration::from_secs(10)));
    
    info!("Connecting to server...");
    socket.connect((SERVER_IP, 443)).await
        .map_err(|_| TlsError::ConnectionFailed)?;
    
    let mut read_buf = [0; 16384];
    let mut write_buf = [0; 16384];
    
    let config = TlsConfig::new()
        .with_ca(CA_CERT)
        .with_server_name("api.example.com");
    
    let mut tls = TlsConnection::<_, Aes128GcmSha256>::new(
        socket,
        &mut read_buf,
        &mut write_buf,
    );
    
    info!("Starting TLS handshake...");
    tls.open(TlsContext::new(&config, rng))
        .await
        .map_err(|_| TlsError::HandshakeFailed)?;
    
    info!("TLS established, sending request...");
    let request = b"GET /api/data HTTP/1.1\r\n\
                     Host: api.example.com\r\n\
                     Connection: close\r\n\r\n";
    
    tls.write_all(request).await
        .map_err(|_| TlsError::WriteError)?;
    
    let mut response = [0u8; 2048];
    let mut total_read = 0;
    
    loop {
        match tls.read(&mut response[total_read..]).await {
            Ok(0) => break,
            Ok(n) => {
                total_read += n;
                if total_read >= response.len() {
                    break;
                }
            }
            Err(_) => return Err(TlsError::ReadError),
        }
    }
    
    info!("Received {} bytes", total_read);
    info!("Response: {}", 
          core::str::from_utf8(&response[..total_read]).unwrap_or("<invalid>"));
    
    tls.close().await.ok();
    
    Ok(())
}

#[derive(Debug)]
enum TlsError {
    ConnectionFailed,
    HandshakeFailed,
    WriteError,
    ReadError,
}
```

---

## Performance Considerations

### Memory Requirements
```c
/* Typical memory footprint for TLS connection */
- SSL context: ~64 KB (configurable)
- Certificate chain: 2-8 KB per certificate
- Crypto operations: 4-16 KB stack
- Buffers: 4-32 KB (depends on MBEDTLS_SSL_MAX_CONTENT_LEN)

/* Memory optimization techniques */
#define MBEDTLS_SSL_MAX_CONTENT_LEN 4096  // Reduce buffer size
#define MBEDTLS_MPI_MAX_SIZE 256          // Limit bignum size
#define MBEDTLS_SSL_IN_CONTENT_LEN 4096   // Input buffer
#define MBEDTLS_SSL_OUT_CONTENT_LEN 4096  // Output buffer
```

### Task Stack Sizing
```c
/* Recommended stack sizes */
#define TLS_CLIENT_STACK_SIZE (8192)   // 8 KB minimum
#define TLS_SERVER_STACK_SIZE (12288)  // 12 KB for server with client auth

xTaskCreate(tls_client_task, "TLS_Client", 
            TLS_CLIENT_STACK_SIZE / sizeof(StackType_t),
            NULL, 5, NULL);
```

### Timing Considerations
```c
/* TLS operations and expected timing */
- Full handshake: 500-2000 ms (RSA 2048)
- Session resumption: 50-200 ms
- Encryption/decryption: 1-5 ms per record
- Certificate verification: 100-500 ms
```

---

## Summary

**TLS/SSL integration in FreeRTOS** enables secure communications for IoT and embedded devices through:

1. **mbedTLS Library**: Lightweight, modular cryptographic library optimized for embedded systems
2. **Certificate Management**: Secure storage in flash, automated provisioning, and runtime validation
3. **Thread Safety**: FreeRTOS mutex integration for multi-threaded access
4. **Hardware Acceleration**: Leveraging crypto accelerators and hardware RNG sources
5. **Memory Optimization**: Careful configuration to fit within resource constraints
6. **Mutual Authentication**: Client and server certificate verification for IoT security

**Key Implementation Aspects**:
- Properly configure mbedTLS for minimal footprint while maintaining security
- Implement secure random number generation using hardware entropy
- Store certificates securely in flash with integrity checks
- Size task stacks appropriately for deep crypto call chains
- Handle TLS operations asynchronously to avoid blocking critical tasks
- Implement certificate renewal and revocation checking for production systems

**Security Best Practices**:
- Always verify certificates (avoid `MBEDTLS_SSL_VERIFY_NONE`)
- Use strong cipher suites (AES-GCM, ChaCha20-Poly1305)
- Implement certificate pinning for known servers
- Regularly update mbedTLS to patch vulnerabilities
- Protect private keys with hardware security modules when possible
- Use session resumption to reduce handshake overhead

TLS/SSL is essential for modern IoT deployments, providing confidentiality, integrity, and authentication for device communications in increasingly hostile network environments.