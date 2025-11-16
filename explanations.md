# Загальний огляд проєкту

Цей проєкт — система збору даних з сенсорів на Raspberry Pi Pico W з передачею через HTTPS до сервера. Використовується FreeRTOS для багатозадачності.

### Що робить система

1. Збирає дані з 5 сенсорів
2. Обробляє їх паралельно завдяки FreeRTOS
3. Відправляє дані через HTTPS кожні 30 секунд
4. Забезпечує безпеку через TLS/SSL

---

## Архітектура системи

### Апаратні компоненти

- Raspberry Pi Pico W — мікроконтролер з Wi‑Fi
- SHTC3 — датчик температури та вологості (I2C)
- SGP40 — датчик VOC (I2C)
- QMI8658 — IMU (акселерометр, гіроскоп) (I2C)
- Датчик освітленості — аналоговий (ADC, GPIO26)
- Датчик звуку — аналоговий (ADC, GPIO27)

### Програмні компоненти

- FreeRTOS — RTOS для багатозадачності
- lwIP — TCP/IP стек для мережі
- mbedTLS — TLS/SSL для HTTPS
- Pico SDK — низькорівневий доступ до апаратури

---

## Детальний розбір коду

### 1. Головний файл: `personal-project.c`

#### Включення бібліотек (рядки 1-47)

```c
/* Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Pico SDK */
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h" // For Wi-Fi
#include "hardware/adc.h"    // For ADC

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h" // For Mutexes

/* Waveshare Libs (Hardware Init) */
#include "DEV_Config.h"

/* Sensor Libs */
#include "SHTC3.h"
#include "SGP40.h"
#include "QMI8658.h"

/* Network (lwIP) */
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/errno.h"
#include "lwip/inet.h"

/* mbedTLS (no desktop net_sockets!) */
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/error.h"

/* Map "net_* failed" error codes for builds without MBEDTLS_NET_C.
 * Values match mbedTLS 2.28.x so error strings remain meaningful via mbedtls_strerror().
 */
#ifndef MBEDTLS_ERR_NET_SEND_FAILED
#define MBEDTLS_ERR_NET_SEND_FAILED    -0x004E
#endif

#ifndef MBEDTLS_ERR_NET_RECV_FAILED
#define MBEDTLS_ERR_NET_RECV_FAILED    -0x004C
#endif
```

Пояснення:

- Стандартні C: `stdio`, `stdlib`, `string`, `errno`
- Pico SDK: доступ до GPIO, ADC, Wi‑Fi (CYW43)
- FreeRTOS: задачі, семафори/мʼютекси
- Драйвери сенсорів: SHTC3, SGP40, QMI8658
- Мережа: lwIP для TCP/IP, mbedTLS для TLS

#### Константи та конфігурація (рядки 50-77)

```c
/* ====================================================================
   --- Wi-Fi / API constants (use real values in your setup) ---
   These are concrete defaults (not placeholders) that compile.
   Change to your own network + endpoint when you flash.
   ==================================================================== */
static const char WIFI_SSID[]     = "test-wifi";
static const char WIFI_PASSWORD[] = "test-password";
static const char API_HOST[]      = "your-api-host.com";
static const char API_PATH[]      = "/your/api/path";

/* Sensor Defines */
#define LIGHT_SENSOR_PIN 26 // ADC 0
#define SOUND_SENSOR_PIN 27 // ADC 1

/* RTOS Handles */
static SemaphoreHandle_t i2c_mutex;             // Protects I2C bus
static SemaphoreHandle_t g_sensor_data_mutex;   // Protects g_sensor_data struct

/* Global struct for all sensor data */
typedef struct {
    float     temp;
    float     hum;
    uint32_t  voc;
    float     acc[3];
    float     gyro[3];
    uint16_t  light; // 0-4095
    uint16_t  sound; // 0-4095
} SensorData_t;

/* Global sensor data */
static SensorData_t g_sensor_data = {0};
```

Пояснення:

- Wi‑Fi/API: SSID, пароль, хост, шлях
- Піни: GPIO26 (ADC0) для світла, GPIO27 (ADC1) для звуку
- Мʼютекси: `i2c_mutex` — доступ до I2C, `g_sensor_data_mutex` — доступ до спільної структури даних
- Структура даних: `SensorData_t` — централізоване сховище для всіх сенсорів

#### TLS-допоміжні функції (рядки 82-144)

```c
/* ====================================================================
   --- TLS helpers: BIO callbacks using lwIP sockets (blocking) ---
   ==================================================================== */

typedef struct {
    int fd; // lwIP socket fd
} tls_net_ctx_t;

static int tls_net_send(void *ctx, const unsigned char *buf, size_t len) {
    tls_net_ctx_t *c = (tls_net_ctx_t *)ctx;
    int ret = lwip_write(c->fd, buf, (int)len);
    if (ret < 0) {
        // Map EWOULDBLOCK/AGAIN to WANT_WRITE if using non-blocking.
        if (errno == EWOULDBLOCK || errno == EAGAIN) return MBEDTLS_ERR_SSL_WANT_WRITE;
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }
    return ret;
}

static int tls_net_recv(void *ctx, unsigned char *buf, size_t len) {
    tls_net_ctx_t *c = (tls_net_ctx_t *)ctx;
    int ret = lwip_read(c->fd, buf, (int)len);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return MBEDTLS_ERR_SSL_WANT_READ;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    if (ret == 0) {
        // Peer closed connection
        return 0; 
    }
    return ret;
}

/* Resolve host and connect TCP socket (IPv4/IPv6) */
static int tcp_connect_lwip(const char *host, const char *port) {
    struct addrinfo hints = {0}, *res = NULL, *rp = NULL;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0 || !res) {
        printf("getaddrinfo failed: %d\n", err);
        return -1;
    }

    int fd = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        fd = lwip_socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;

        if (lwip_connect(fd, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0) {
            break; // connected
        }
        lwip_close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        printf("connect() failed\n");
    }
    return fd;
}
```

Пояснення:

- `tls_net_ctx_t`: контекст з дескриптором сокету
- `tls_net_send/recv`: callback-и для mbedTLS, що використовують lwIP замість стандартних `net_*`
- `tcp_connect_lwip`: DNS-резолюція (`getaddrinfo`) і TCP-підключення

#### HTTPS POST функція (рядки 156-267)

```c
/* ====================================================================
   --- HTTPS POST using mbedTLS over lwIP socket (no mbedtls_net_*) ---
   ==================================================================== */
static void https_post(const char *host, const char *path, const char *json_payload) {
    int ret = 0;
    tls_net_ctx_t net_ctx = { .fd = -1 };

    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    char request_buf[1024];
    char response_buf[1024];
    const char *pers = "pico_w_https_client";

    printf("Starting HTTPS POST to %s%s\n", host, path);

    // Init TLS objects
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    // Seed DRBG
    if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                     (const unsigned char *)pers, strlen(pers))) != 0) {
        print_mbedtls_err("ctr_drbg_seed", ret);
        goto cleanup;
    }

    // Load CA certs if you have them; otherwise keep VERIFY_OPTIONAL for now.
    // Example: mbedtls_x509_crt_parse(&cacert, ca_pem, ca_pem_len);

    if ((ret = mbedtls_ssl_config_defaults(&conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        print_mbedtls_err("ssl_config_defaults", ret);
        goto cleanup;
    }

    // For first bring-up you can do OPTIONAL; for production use REQUIRED and real CA.
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
        print_mbedtls_err("ssl_setup", ret);
        goto cleanup;
    }

    if ((ret = mbedtls_ssl_set_hostname(&ssl, host)) != 0) {
        print_mbedtls_err("ssl_set_hostname", ret);
        goto cleanup;
    }

    // TCP connect
    net_ctx.fd = tcp_connect_lwip(host, "443");
    if (net_ctx.fd < 0) goto cleanup;

    // BIO: plug our send/recv
    mbedtls_ssl_set_bio(&ssl, &net_ctx, tls_net_send, tls_net_recv, NULL);

    // Handshake
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            print_mbedtls_err("ssl_handshake", ret);
            goto cleanup;
        }
    }

    // Build HTTP request
    const int payload_len = (int)strlen(json_payload);
    int n = snprintf(request_buf, sizeof(request_buf),
                     "POST %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %d\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "%s",
                     path, host, payload_len, json_payload);
    if (n < 0 || n >= (int)sizeof(request_buf)) {
        printf("Request too big\n");
        goto cleanup;
    }

    // Send
    ret = mbedtls_ssl_write(&ssl, (const unsigned char *)request_buf, (size_t)n);
    if (ret <= 0) {
        print_mbedtls_err("ssl_write", ret);
        goto cleanup;
    }

    // Read (single chunk)
    memset(response_buf, 0, sizeof(response_buf));
    ret = mbedtls_ssl_read(&ssl, (unsigned char *)response_buf, sizeof(response_buf) - 1);
    if (ret <= 0) {
        if (ret == 0) printf("... Server closed connection\n");
        else print_mbedtls_err("ssl_read", ret);
    } else {
        printf("... Received %d bytes:\n--- (BEGIN RESPONSE) ---\n%s\n--- (END RESPONSE) ---\n", ret, response_buf);
    }

cleanup:
    if (net_ctx.fd >= 0) lwip_close(net_ctx.fd);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_x509_crt_free(&cacert);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}
```

Пояснення:

- Ініціалізація: SSL контекст, конфігурація, сертифікати, RNG
- RNG: `mbedtls_ctr_drbg_seed` — ініціалізація генератора випадкових чисел
- Налаштування SSL: клієнт, TLS 1.2, перевірка сертифіката (зараз OPTIONAL)
- Підключення: TCP на порт 443, підключення BIO до lwIP
- Handshake: TLS handshake
- HTTP запит: формування POST з JSON
- Відправка/отримання: `mbedtls_ssl_write/read`
- Cleanup: звільнення ресурсів

#### Задачі FreeRTOS для сенсорів (рядки 273-349)

```c
void vLightSensorTask(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        adc_select_input(0); // ADC0 (GPIO26)
        uint16_t light_val = adc_read();
        if (xSemaphoreTake(g_sensor_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_sensor_data.light = light_val;
            xSemaphoreGive(g_sensor_data_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

Пояснення:

- Безкінечний цикл: `for (;;)`
- Читання ADC: вибір каналу 0 (GPIO26), читання значення (0-4095)
- Захист мʼютексом: захоплення, запис, звільнення
- Затримка: 100 мс між вимірами

```c
void vSoundSensorTask(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        adc_select_input(1); // ADC1 (GPIO27)
        uint16_t sound_val = adc_read();
        if (xSemaphoreTake(g_sensor_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_sensor_data.sound = sound_val;
            xSemaphoreGive(g_sensor_data_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

Аналогічно до `vLightSensorTask`, але для ADC1 (GPIO27).

```c
void vSHTC3Task(void *pvParameters) {
    (void)pvParameters;
    float local_temp = 0.f, local_hum = 0.f;
    for (;;) {
        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
            SHTC3_Measurement(&local_temp, &local_hum);
            xSemaphoreGive(i2c_mutex);
        }
        if (xSemaphoreTake(g_sensor_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_sensor_data.temp = local_temp;
            g_sensor_data.hum  = local_hum;
            xSemaphoreGive(g_sensor_data_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

Пояснення:

- Локальні змінні: зберігають значення до запису в глобальну структуру
- I2C мʼютекс: `i2c_mutex` захищає шину I2C (один сенсор одночасно)
- Читання: `SHTC3_Measurement` виконує I2C-транзакцію
- Запис: після звільнення I2C мʼютекса запис у глобальну структуру

```c
void vSGP40Task(void *pvParameters) {
    (void)pvParameters;
    uint32_t voc_index = 0;
    for (;;) {
        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
            voc_index = SGP40_MeasureVOC(25, 50); // static T/H for now
            xSemaphoreGive(i2c_mutex);
        }
        if (xSemaphoreTake(g_sensor_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_sensor_data.voc = voc_index;
            xSemaphoreGive(g_sensor_data_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

Пояснення:

- SGP40 потребує температуру та вологість для компенсації; зараз використовуються статичні значення (25°C, 50%). Можна передавати реальні значення з `g_sensor_data`.

```c
void vQMI8658Task(void *pvParameters) {
    (void)pvParameters;
    float local_acc[3]  = {0};
    float local_gyro[3] = {0};
    unsigned int tim_count = 0;
    for (;;) {
        if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
            QMI8658_read_xyz(local_acc, local_gyro, &tim_count);
            xSemaphoreGive(i2c_mutex);
        }
        if (xSemaphoreTake(g_sensor_data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            memcpy(g_sensor_data.acc,  local_acc,  sizeof(local_acc));
            memcpy(g_sensor_data.gyro, local_gyro, sizeof(local_gyro));
            xSemaphoreGive(g_sensor_data_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

Пояснення:

- Масиви: `local_acc[3]`, `local_gyro[3]` для X, Y, Z
- `memcpy`: копіювання масивів у глобальну структуру

#### Задача відправки на API (рядки 351-391)

```c
void vAPISendTask(void *pvParameters) {
    (void)pvParameters;
    static char json_buffer[1024];
    SensorData_t local_data;

    // Wait until Wi-Fi is up
    while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) {
        printf("API Task waiting for Wi-Fi...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    printf("API Task: Wi-Fi connected. Starting send loop.\n");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // every 30s

        if (xSemaphoreTake(g_sensor_data_mutex, portMAX_DELAY) == pdTRUE) {
            memcpy(&local_data, &g_sensor_data, sizeof(SensorData_t));
            xSemaphoreGive(g_sensor_data_mutex);
        }

        // Build JSON
        snprintf(json_buffer, sizeof(json_buffer),
                 "{"
                 "\"temperature\":%.2f,"
                 "\"humidity\":%.2f,"
                 "\"voc\":%lu,"
                 "\"light\":%u,"
                 "\"sound\":%u,"
                 "\"accelerometer\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
                 "\"gyroscope\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}"
                 "}",
                 local_data.temp, local_data.hum,
                 (unsigned long)local_data.voc,
                 (unsigned)local_data.light, (unsigned)local_data.sound,
                 local_data.acc[0], local_data.acc[1], local_data.acc[2],
                 local_data.gyro[0], local_data.gyro[1], local_data.gyro[2]);

        printf("Sending JSON to API:\n%s\n", json_buffer);
        https_post(API_HOST, API_PATH, json_buffer);
    }
}
```

Пояснення:

- Очікування Wi‑Fi: цикл до підключення
- Інтервал: 30 секунд між відправками
- Копіювання даних: захоплення мʼютекса, копіювання в локальну змінну, звільнення
- Формування JSON: `snprintf` для безпечного форматування
- Відправка: виклик `https_post`

#### Функція main (рядки 393-449)

```c
int main(void) {
    stdio_init_all();
    printf("System Init...\n");

    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect to Wi-Fi.\n");
    } else {
        printf("Connected to Wi-Fi.\n");
    }

    // Sensors HW init
    if (DEV_Module_Init() != 0) {
        printf("DEV_Module_Init failed\r\n");
        while (1) { tight_loop_contents(); }
    }
    printf("DEV_Module_Init OK\r\n");

    // ADC init
    adc_init();
    adc_gpio_init(LIGHT_SENSOR_PIN);
    adc_gpio_init(SOUND_SENSOR_PIN);

    // Mutexes
    i2c_mutex           = xSemaphoreCreateMutex();
    g_sensor_data_mutex = xSemaphoreCreateMutex();

    // I2C sensors init (protected)
    xSemaphoreTake(i2c_mutex, portMAX_DELAY);
    SHTC3_Init();
    SGP40_init();
    QMI8658_init();
    xSemaphoreGive(i2c_mutex);
    printf("I2C Sensors Init OK\r\n");

    // Tasks
    xTaskCreate(vSHTC3Task,       "SHTC3Task",   256,  NULL, 1, NULL);
    xTaskCreate(vSGP40Task,       "SGP40Task",   256,  NULL, 1, NULL);
    xTaskCreate(vQMI8658Task,     "QMI8658Task", 256,  NULL, 1, NULL);
    xTaskCreate(vLightSensorTask, "LightTask",   256,  NULL, 1, NULL);
    xTaskCreate(vSoundSensorTask, "SoundTask",   256,  NULL, 1, NULL);

    // HTTPS task needs bigger stack
    xTaskCreate(vAPISendTask,     "APITask",    8192,  NULL, 3, NULL);

    printf("Starting Scheduler...\n");
    vTaskStartScheduler();

    while (1) { /* should not get here */ }
}
```

Пояснення:

- Ініціалізація: `stdio_init_all()` для UART/USB
- Wi‑Fi: ініціалізація CYW43, режим станції, підключення з таймаутом 30 с
- Апаратна ініціалізація: `DEV_Module_Init()` (I2C, SPI тощо)
- ADC: ініціалізація та налаштування пінів
- Мʼютекси: створення двох мʼютексів
- Ініціалізація I2C-сенсорів: під захистом мʼютекса
- Створення задач: `xTaskCreate` (імʼя, стек, параметри, пріоритет, handle)
- Запуск планувальника: `vTaskStartScheduler()` — перехід до багатозадачності

---

## Ключові концепції

### 1. FreeRTOS — Real-Time Operating System

Що це:

- RTOS для багатозадачності на мікроконтролерах
- Планувальник керує виконанням задач

Основні поняття:

- Задача (Task): функція, що виконується окремо
- Планувальник (Scheduler): перемикає задачі
- Пріоритет: вища задача виконується першою
- Стек: памʼять для локальних змінних задачі

У проєкті:

- 6 задач: 5 для сенсорів (пріоритет 1), 1 для API (пріоритет 3)
- Мʼютекси: синхронізація доступу до спільних ресурсів

### 2. Мʼютекси (Mutexes)

Навіщо:

- Захищають спільні ресурси від одночасного доступу

У проєкті:

- `i2c_mutex`: захищає I2C-шину (один сенсор одночасно)
- `g_sensor_data_mutex`: захищає глобальну структуру даних

Як працює:

```c
// Захоплення мʼютекса
xSemaphoreTake(mutex, timeout);

// Критична секція (робота з ресурсом)
// ... код ...

// Звільнення мʼютекса
xSemaphoreGive(mutex);
```

### 3. I2C (Inter-Integrated Circuit)

Що це:

- Протокол для підключення сенсорів

У проєкті:

- I2C0: SHTC3, SGP40, QMI8658
- Адреси: SHTC3 (0x70), SGP40 (0x59), QMI8658 (своя)

Чому мʼютекс:

- Один сенсор одночасно, щоб уникнути конфліктів

### 4. ADC (Analog-to-Digital Converter)

Що це:

- Перетворює аналоговий сигнал у цифровий

У проєкті:

- GPIO26 (ADC0): датчик світла
- GPIO27 (ADC1): датчик звуку
- Діапазон: 0-4095 (12 біт)

### 5. lwIP — Lightweight IP

Що це:

- Легкий TCP/IP стек для вбудованих систем

У проєкті:

- DNS, TCP, сокети
- Інтеграція з FreeRTOS (`NO_SYS=0`)

### 6. mbedTLS

Що це:

- Бібліотека для TLS/SSL

У проєкті:

- HTTPS-клієнт
- TLS 1.2, шифрування, перевірка сертифікатів

### 7. Структура даних `SensorData_t`

```c
typedef struct {
    float     temp;      // Температура (°C)
    float     hum;       // Вологість (%)
    uint32_t  voc;       // VOC індекс
    float     acc[3];    // Акселерометр [x, y, z] (g)
    float     gyro[3];   // Гіроскоп [x, y, z] (dps)
    uint16_t  light;     // Освітленість (0-4095)
    uint16_t  sound;     // Звук (0-4095)
} SensorData_t;
```

Чому глобальна:

- Доступна всім задачам
- Захищена мʼютексом

---

## Як працює система в цілому

### Послідовність запуску

1. `main()` виконується:
   - Ініціалізація UART/USB
   - Підключення Wi‑Fi
   - Ініціалізація апаратури (I2C, ADC)
   - Створення мʼютексів
   - Ініціалізація сенсорів
   - Створення задач
   - Запуск планувальника

2. Планувальник запускає задачі:
   - Всі задачі сенсорів виконуються паралельно
   - Кожна читає свій сенсор кожні 100 мс
   - Дані записуються в `g_sensor_data`

3. Задача API:
   - Очікує підключення Wi‑Fi
   - Кожні 30 секунд:
     - Копіює дані з `g_sensor_data`
     - Формує JSON
     - Відправляє через HTTPS

### Потік даних

```
Сенсори → Задачі FreeRTOS → g_sensor_data (захищена мʼютексом)
                                              ↓
                                    vAPISendTask (кожні 30с)
                                              ↓
                                    JSON формування
                                              ↓
                                    HTTPS POST
                                              ↓
                                    Backend API
```

---

## Конфігураційні файли

### `FreeRTOSConfig.h`

```44:48:FreeRTOSConfig.h
#define configUSE_PREEMPTION                    1
#define configUSE_TICKLESS_IDLE                 0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
```

Пояснення:

- `configUSE_PREEMPTION = 1`: витісняюча багатозадачність
- `configTICK_RATE_HZ = 1000`: 1000 тіків на секунду (1 мс)

```74:74:FreeRTOSConfig.h
#define configTOTAL_HEAP_SIZE                   (128*1024)
```

Пояснення:

- 128 КБ heap для динамічного виділення памʼяті

### `lwipopts.h`

```8:10:config/lwipopts.h
#define NO_SYS                         0
#define SYS_LIGHTWEIGHT_PROT           1
#define LWIP_TCPIP_CORE_LOCKING        1
```

Пояснення:

- `NO_SYS = 0`: використання ОС (FreeRTOS)
- `LWIP_TCPIP_CORE_LOCKING = 1`: блокування для потокобезпеки

### `mbedtls_config.h`

```12:12:config/mbedtls_config.h
#define MBEDTLS_PLATFORM_MS_TIME_ALT
```

Пояснення:

- Використання альтернативної функції часу (з `mbedtls_time_alt.c`)

---

## Важливі деталі для розуміння

### 1. Чому два мʼютекси?

- `i2c_mutex`: захищає I2C-шину (сенсори не можуть читати одночасно)
- `g_sensor_data_mutex`: захищає глобальну структуру (запобігає пошкодженню даних)

### 2. Чому локальні змінні в задачах?

```c
float local_temp = 0.f, local_hum = 0.f;
SHTC3_Measurement(&local_temp, &local_hum);
// Потім запис у g_sensor_data
```

Причина:

- Мінімізація часу утримання мʼютекса
- Зменшення блокувань інших задач

### 3. Чому великий стек для API-задачі?

```c
xTaskCreate(vAPISendTask, "APITask", 8192, NULL, 3, NULL);
//                                          ^^^^
//                                          8192 байт
```

Причина:

- HTTPS/TLS потребує більше памʼяті (буфери, контексти)

### 4. Чому `portMAX_DELAY` для I2C мʼютекса?

```c
xSemaphoreTake(i2c_mutex, portMAX_DELAY)
```

Причина:

- Задача чекає, поки I2C звільниться (без таймауту)

### 5. Чому `pdMS_TO_TICKS(100)` для затримок?

```c
vTaskDelay(pdMS_TO_TICKS(100));
```

Причина:

- FreeRTOS працює в тіках, а не мілісекундах
- `pdMS_TO_TICKS` перетворює мс у тіки

---

## Типові проблеми та рішення

### 1. Deadlock (взаємне блокування)

Як уникнути:

- Завжди звільняти мʼютекси
- Не брати кілька мʼютексів одночасно
- Використовувати таймаути

### 2. Stack overflow

Симптоми:

- Система зависає
- Непередбачувана поведінка

Рішення:

- Збільшити розмір стеку в `xTaskCreate`
- Перевірити `uxTaskGetStackHighWaterMark`

### 3. Wi‑Fi не підключається

Можливі причини:

- Неправильний SSID/пароль
- Слабкий сигнал
- Недостатньо часу на підключення

---

## Рекомендації для покращення

1. Додати обробку помилок у задачах сенсорів
2. Використовувати реальні T/H для SGP40 замість статичних
3. Додати перевірку сертифікатів (замість `VERIFY_OPTIONAL`)
4. Додати watchdog timer для моніторингу системи
5. Реалізувати чергу повідомлень замість глобальної структури
6. Додати OLED-дисплей для локального відображення даних

---

## Висновок

Цей проєкт демонструє:

- Багатозадачність на FreeRTOS
- Роботу з I2C та ADC
- Безпечну передачу даних через HTTPS
- Синхронізацію через мʼютекси
- Інтеграцію мережевого стеку (lwIP) та TLS (mbedTLS)

Кожна частина коду має свою роль у загальній системі, що працює як єдине ціле для збору та передачі даних сенсорів.

Якщо потрібні додаткові пояснення по конкретних частинах, напишіть.
