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


/* ====================================================================
   --- Wi-Fi / API constants (use real values in your setup) ---
   These are concrete defaults (not placeholders) that compile.
   Change to your own network + endpoint when you flash.
   ==================================================================== */
static const char WIFI_SSID[]     = "test-wifi";
static const char WIFI_PASSWORD[] = "test-password";
static const char API_HOST[]      = "192.168.1.100";  // Change to your local PC's IP address
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

/* ====================================================================
   --- Simple HTTP POST using plain TCP socket ---
   ==================================================================== */
static void http_post(const char *host, const char *path, const char *json_payload) {
    int fd = -1;
    char request_buf[1024];
    char response_buf[1024];

    printf("Starting HTTP POST to %s%s\n", host, path);

    // TCP connect to port 8000 (local development)
    fd = tcp_connect_lwip(host, "8000");
    if (fd < 0) {
        printf("Failed to connect to %s:8000\n", host);
        return;
    }

    // Build HTTP request
    const int payload_len = (int)strlen(json_payload);
    int n = snprintf(request_buf, sizeof(request_buf),
                     "POST %s HTTP/1.1\r\n"
                     "Host: %s:8000\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %d\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "%s",
                     path, host, payload_len, json_payload);
    if (n < 0 || n >= (int)sizeof(request_buf)) {
        printf("Request too big\n");
        lwip_close(fd);
        return;
    }

    // Send HTTP request
    int sent = lwip_write(fd, request_buf, n);
    if (sent < 0) {
        printf("Failed to send request: %d\n", errno);
        lwip_close(fd);
        return;
    }
    printf("Sent %d bytes\n", sent);

    // Read response (single chunk)
    memset(response_buf, 0, sizeof(response_buf));
    int received = lwip_read(fd, response_buf, sizeof(response_buf) - 1);
    if (received < 0) {
        printf("Failed to read response: %d\n", errno);
    } else if (received == 0) {
        printf("... Server closed connection\n");
    } else {
        printf("... Received %d bytes:\n--- (BEGIN RESPONSE) ---\n%s\n--- (END RESPONSE) ---\n", received, response_buf);
    }

    lwip_close(fd);
}

/* ====================================================================
   --- FreeRTOS Tasks (sensors unchanged except small hygiene) ---
   ==================================================================== */

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
        http_post(API_HOST, API_PATH, json_buffer);
    }
}

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
