// Standard library includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Pico SDK headers
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"

// FreeRTOS headers
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Hardware configuration
#include "DEV_Config.h"

// Sensor driver libraries
#include "SHTC3.h"
#include "SGP40.h"
#include "QMI8658.h"

// Network stack (lwIP)
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/errno.h"
#include "lwip/inet.h"


// Network configuration - update with your local settings
static const char WIFI_SSID[] = "test-wifi";
static const char WIFI_PASSWORD[] = "test-password";
static const char SERVER_HOST[] = "192.168.1.100";  // Local development server IP
static const char SERVER_ENDPOINT[] = "/your/api/path";

// Hardware pin definitions
#define LIGHT_SENSOR_PIN 26
#define SOUND_SENSOR_PIN 27

// Synchronization primitives
static SemaphoreHandle_t i2c_bus_lock;
static SemaphoreHandle_t sensor_mutex;

// Sensor data structure
typedef struct {
    float temperature;
    float humidity;
    uint32_t voc_index;
    float accelerometer[3];
    float gyroscope[3];
    uint16_t light_level;
    uint16_t sound_level;
} sensor_readings_t;

// Global sensor readings storage
static sensor_readings_t sensor_readings = {0};

// Establish TCP connection to specified host and port
static int establish_tcp_connection(const char *hostname, const char *port_str) {
    struct addrinfo addr_hints = {0};
    struct addrinfo *addr_result = NULL;
    struct addrinfo *current_addr = NULL;
    
    addr_hints.ai_family = AF_UNSPEC;
    addr_hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(hostname, port_str, &addr_hints, &addr_result);
    if (status != 0 || addr_result == NULL) {
        printf("DNS resolution failed: %d\n", status);
        return -1;
    }

    int socket_fd = -1;
    for (current_addr = addr_result; current_addr != NULL; current_addr = current_addr->ai_next) {
        socket_fd = lwip_socket(current_addr->ai_family, current_addr->ai_socktype, current_addr->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        if (lwip_connect(socket_fd, current_addr->ai_addr, (socklen_t)current_addr->ai_addrlen) == 0) {
            break;
        }
        lwip_close(socket_fd);
        socket_fd = -1;
    }
    freeaddrinfo(addr_result);

    if (socket_fd < 0) {
        printf("TCP connection failed\n");
    }
    return socket_fd;
}

// Send HTTP POST request to local development server
static void send_http_request(const char *server_host, const char *endpoint, const char *json_data) {
    int connection_fd = -1;
    char http_request[1024];
    char server_response[1024];

    printf("Sending HTTP POST to %s%s\n", server_host, endpoint);

    connection_fd = establish_tcp_connection(server_host, "8000");
    if (connection_fd < 0) {
        printf("Connection error: %s:8000\n", server_host);
        return;
    }

    const int data_length = (int)strlen(json_data);
    int request_size = snprintf(http_request, sizeof(http_request),
                                 "POST %s HTTP/1.1\r\n"
                                 "Host: %s:8000\r\n"
                                 "Content-Type: application/json\r\n"
                                 "Content-Length: %d\r\n"
                                 "Connection: close\r\n"
                                 "\r\n"
                                 "%s",
                                 endpoint, server_host, data_length, json_data);
    if (request_size < 0 || request_size >= (int)sizeof(http_request)) {
        printf("HTTP request buffer overflow\n");
        lwip_close(connection_fd);
        return;
    }

    int bytes_sent = lwip_write(connection_fd, http_request, request_size);
    if (bytes_sent < 0) {
        printf("Write error: %d\n", errno);
        lwip_close(connection_fd);
        return;
    }
    printf("Transmitted %d bytes\n", bytes_sent);

    memset(server_response, 0, sizeof(server_response));
    int bytes_received = lwip_read(connection_fd, server_response, sizeof(server_response) - 1);
    if (bytes_received < 0) {
        printf("Read error: %d\n", errno);
    } else if (bytes_received == 0) {
        printf("Connection terminated by server\n");
    } else {
        printf("Response (%d bytes):\n--- START ---\n%s\n--- END ---\n", bytes_received, server_response);
    }

    lwip_close(connection_fd);
}

// FreeRTOS task implementations

void vLightSensorTask(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        adc_select_input(0);
        uint16_t reading = adc_read();
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensor_readings.light_level = reading;
            xSemaphoreGive(sensor_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vSoundSensorTask(void *pvParameters) {
    (void)pvParameters;
    for (;;) {
        adc_select_input(1);
        uint16_t reading = adc_read();
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensor_readings.sound_level = reading;
            xSemaphoreGive(sensor_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vSHTC3Task(void *pvParameters) {
    (void)pvParameters;
    float temp_reading = 0.0f;
    float hum_reading = 0.0f;
    for (;;) {
        if (xSemaphoreTake(i2c_bus_lock, portMAX_DELAY) == pdTRUE) {
            SHTC3_Measurement(&temp_reading, &hum_reading);
            xSemaphoreGive(i2c_bus_lock);
        }
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensor_readings.temperature = temp_reading;
            sensor_readings.humidity = hum_reading;
            xSemaphoreGive(sensor_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vSGP40Task(void *pvParameters) {
    (void)pvParameters;
    uint32_t voc_value = 0;
    for (;;) {
        if (xSemaphoreTake(i2c_bus_lock, portMAX_DELAY) == pdTRUE) {
            voc_value = SGP40_MeasureVOC(25, 50);
            xSemaphoreGive(i2c_bus_lock);
        }
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sensor_readings.voc_index = voc_value;
            xSemaphoreGive(sensor_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vQMI8658Task(void *pvParameters) {
    (void)pvParameters;
    float accel_data[3] = {0.0f};
    float gyro_data[3] = {0.0f};
    unsigned int timestamp = 0;
    for (;;) {
        if (xSemaphoreTake(i2c_bus_lock, portMAX_DELAY) == pdTRUE) {
            QMI8658_read_xyz(accel_data, gyro_data, &timestamp);
            xSemaphoreGive(i2c_bus_lock);
        }
        if (xSemaphoreTake(sensor_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            memcpy(sensor_readings.accelerometer, accel_data, sizeof(accel_data));
            memcpy(sensor_readings.gyroscope, gyro_data, sizeof(gyro_data));
            xSemaphoreGive(sensor_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vAPISendTask(void *pvParameters) {
    (void)pvParameters;
    static char json_payload[1024];
    sensor_readings_t current_readings;

    while (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) {
        printf("Waiting for network connection...\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    printf("Network ready. Beginning data transmission.\n");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        if (xSemaphoreTake(sensor_mutex, portMAX_DELAY) == pdTRUE) {
            memcpy(&current_readings, &sensor_readings, sizeof(sensor_readings_t));
            xSemaphoreGive(sensor_mutex);
        }

        snprintf(json_payload, sizeof(json_payload),
                 "{"
                 "\"temperature\":%.2f,"
                 "\"humidity\":%.2f,"
                 "\"voc\":%lu,"
                 "\"light\":%u,"
                 "\"sound\":%u,"
                 "\"accelerometer\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f},"
                 "\"gyroscope\":{\"x\":%.2f,\"y\":%.2f,\"z\":%.2f}"
                 "}",
                 current_readings.temperature, current_readings.humidity,
                 (unsigned long)current_readings.voc_index,
                 (unsigned)current_readings.light_level, (unsigned)current_readings.sound_level,
                 current_readings.accelerometer[0], current_readings.accelerometer[1], current_readings.accelerometer[2],
                 current_readings.gyroscope[0], current_readings.gyroscope[1], current_readings.gyroscope[2]);

        printf("Transmitting data:\n%s\n", json_payload);
        send_http_request(SERVER_HOST, SERVER_ENDPOINT, json_payload);
    }
}

int main(void) {
    stdio_init_all();
    printf("Initializing system...\n");

    if (cyw43_arch_init() != 0) {
        printf("Wi-Fi initialization error\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Connecting to network: %s\n", WIFI_SSID);

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000) != 0) {
        printf("Network connection failed\n");
    } else {
        printf("Network connected successfully\n");
    }

    if (DEV_Module_Init() != 0) {
        printf("Hardware initialization failed\n");
        while (1) { tight_loop_contents(); }
    }
    printf("Hardware initialized\n");

    adc_init();
    adc_gpio_init(LIGHT_SENSOR_PIN);
    adc_gpio_init(SOUND_SENSOR_PIN);

    i2c_bus_lock = xSemaphoreCreateMutex();
    sensor_mutex = xSemaphoreCreateMutex();

    xSemaphoreTake(i2c_bus_lock, portMAX_DELAY);
    SHTC3_Init();
    SGP40_init();
    QMI8658_init();
    xSemaphoreGive(i2c_bus_lock);
    printf("Sensor initialization complete\n");

    xTaskCreate(vSHTC3Task, "SHTC3Task", 256, NULL, 1, NULL);
    xTaskCreate(vSGP40Task, "SGP40Task", 256, NULL, 1, NULL);
    xTaskCreate(vQMI8658Task, "QMI8658Task", 256, NULL, 1, NULL);
    xTaskCreate(vLightSensorTask, "LightTask", 256, NULL, 1, NULL);
    xTaskCreate(vSoundSensorTask, "SoundTask", 256, NULL, 1, NULL);
    xTaskCreate(vAPISendTask, "APITask", 8192, NULL, 3, NULL);

    printf("Starting FreeRTOS scheduler\n");
    vTaskStartScheduler();

    while (1) { }
}
