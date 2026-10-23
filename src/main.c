#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_efuse.h"
#include "esp_mac.h"

static const char *TAG = "TX_BARE";

#define PIN_LORA_SS       5
#define PIN_LORA_RST      12
#define PIN_LORA_DIO0     25
#define PIN_LORA_SCK      18
#define PIN_LORA_MISO     19
#define PIN_LORA_MOSI     23

#define PIN_GPS_RX        16
#define PIN_GPS_TX        17
#define PIN_GPS_EN        21

#define PIN_MAGNETIC      33

// ADC Channels
#define ADC_CH_VIB        ADC_CHANNEL_6 // GPIO34
#define ADC_CH_MQ7        ADC_CHANNEL_7 // GPIO35
#define ADC_CH_MQ135      ADC_CHANNEL_4 // GPIO32

#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FR_MSB               0x06
#define REG_FR_MID               0x07
#define REG_FR_LSB               0x08
#define REG_PA_CONFIG            0x09
#define REG_OCP                  0x0B
#define REG_LNA                  0x0C
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_TX_BASE_ADDR    0x0E
#define REG_IRQ_FLAGS            0x12
#define REG_MODEM_CONFIG_1       0x1D
#define REG_MODEM_CONFIG_2       0x1E
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_PAYLOAD_LENGTH       0x22
#define REG_MODEM_CONFIG_3       0x26
#define REG_SYNC_WORD            0x39
#define REG_PA_DAC               0x4D
#define REG_VERSION              0x42

#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_LONG_RANGE_MODE     0x80
#define IRQ_TX_DONE_MASK         0x08

#pragma pack(push, 1)
typedef struct {
    uint8_t  packetCounter;
    uint32_t deviceID;
    double   latitude;
    double   longitude;
    double   altitude;
    double   speed;
    uint8_t  satellites;
    char     timeUTC[20];
    bool     doorValue;
    char     tujuan[14];
    char     perusahaan[20];
    char     isi[16];
} TelemetryPacket;
#pragma pack(pop)

TelemetryPacket dataToSend = {0};

char g_tujuan[14] = "";
char g_perusahaan[20] = "";
char g_isi[16] = "";

spi_device_handle_t spi_lora;
adc_oneshot_unit_handle_t adc1_handle;

esp_err_t lora_write_reg(uint8_t reg, uint8_t val) {
    spi_transaction_t t = {
        .length = 16,
        .flags = SPI_TRANS_USE_TXDATA,
    };
    t.tx_data[0] = reg | 0x80;
    t.tx_data[1] = val;
    return spi_device_polling_transmit(spi_lora, &t);
}

uint8_t lora_read_reg(uint8_t reg) {
    spi_transaction_t t = {
        .length = 16,
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    };
    t.tx_data[0] = reg & 0x7F;
    t.tx_data[1] = 0x00;
    esp_err_t ret = spi_device_polling_transmit(spi_lora, &t);
    if (ret != ESP_OK) return 0;
    return t.rx_data[1];
}

void lora_init_hw(long frequency) {
    // Reset sequence
    gpio_set_direction(PIN_LORA_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_LORA_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_LORA_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    // SPI Bus Configuration - CS handled by driver
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_LORA_MISO,
        .mosi_io_num = PIN_LORA_MOSI,
        .sclk_io_num = PIN_LORA_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = PIN_LORA_SS,    // Driver handles CS automatically!
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &spi_lora));

    // Verify Chip Version
    uint8_t ver = lora_read_reg(REG_VERSION);
    ESP_LOGI(TAG, "RFM95 Version: 0x%02X (expect 0x12)", ver);
    if (ver != 0x12) {
        ESP_LOGE(TAG, "LoRa module not detected!");
        return;
    }

    // Config RF Mode & Freq
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint64_t frf = ((uint64_t)frequency << 19) / 32000000;
    lora_write_reg(REG_FR_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(REG_FR_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FR_LSB, (uint8_t)(frf >> 0));

    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0x00);

    lora_write_reg(REG_MODEM_CONFIG_1, 0x78); // BW=125kHz, CR=4/8, Explicit Header
    lora_write_reg(REG_MODEM_CONFIG_2, 0xC4); // SF=12, CRC On
    lora_write_reg(REG_MODEM_CONFIG_3, 0x0C); // AGC Auto + LDRO=1

    lora_write_reg(REG_PREAMBLE_MSB, 0x00);
    lora_write_reg(REG_PREAMBLE_LSB, 12);
    lora_write_reg(REG_SYNC_WORD, 0x34);

    lora_write_reg(REG_LNA, 0x23);       // LNA Gain Max + Boost
    lora_write_reg(REG_OCP, 0x2B);       // OCP 100mA
    lora_write_reg(REG_PA_CONFIG, 0x8F); // PA_BOOST Max Power (20dBm)
    lora_write_reg(REG_PA_DAC, 0x87);    // +20dBm boost

    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
    ESP_LOGI(TAG, "LoRa HW Initialized.");

    // Preamble
    lora_write_reg(REG_PREAMBLE_MSB, 0x00);
    lora_write_reg(REG_PREAMBLE_LSB, 12);

    // Sync word
    lora_write_reg(REG_SYNC_WORD, 0x34);

    // OCP (Over Current Protection) - 100mA
    lora_write_reg(REG_OCP, 0x2B);

    // PA config for 14dBm
    lora_write_reg(REG_PA_CONFIG, 0x8C);  // PA_BOOST | 12
    lora_write_reg(REG_PA_DAC, 0x84);     // No boost

    // Set LDRO for SF12@125kHz (symbol duration > 16ms)
    // Symbol duration = 1000 / (125000 / 4096) = 32.7ms > 16ms -> LDRO=1
    lora_write_reg(REG_MODEM_CONFIG_3, 0x0C);  // auto AGC + LDRO=1

    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
    ESP_LOGI(TAG, "LoRa HW Initialized.");
    ESP_LOGI(TAG, "REG_MODEM_CONFIG_1: 0x%02X (expect 0x78)", lora_read_reg(REG_MODEM_CONFIG_1));
    ESP_LOGI(TAG, "REG_MODEM_CONFIG_2: 0x%02X (expect 0xC4)", lora_read_reg(REG_MODEM_CONFIG_2));
    ESP_LOGI(TAG, "REG_MODEM_CONFIG_3: 0x%02X (expect 0x0C)", lora_read_reg(REG_MODEM_CONFIG_3));
    ESP_LOGI(TAG, "REG_SYNC_WORD: 0x%02X (expect 0x34)", lora_read_reg(REG_SYNC_WORD));
    ESP_LOGI(TAG, "REG_PREAMBLE_LSB: 0x%02X (expect 0x0C)", lora_read_reg(REG_PREAMBLE_LSB));
    ESP_LOGI(TAG, "REG_LNA: 0x%02X (expect 0x03)", lora_read_reg(REG_LNA));
    ESP_LOGI(TAG, "REG_OCP: 0x%02X (expect 0x2B)", lora_read_reg(REG_OCP));
    ESP_LOGI(TAG, "REG_PA_CONFIG: 0x%02X (expect 0x8F)", lora_read_reg(REG_PA_CONFIG));
    ESP_LOGI(TAG, "REG_PA_DAC: 0x%02X (expect 0x87)", lora_read_reg(REG_PA_DAC));
}

bool lora_send_packet(const uint8_t *buf, size_t len) {
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0);
    lora_write_reg(REG_PAYLOAD_LENGTH, len);

    uint8_t *fifo_buf = malloc(len + 1);
    fifo_buf[0] = REG_FIFO | 0x80;
    memcpy(&fifo_buf[1], buf, len);

    spi_transaction_t wr = {
        .length = (len + 1) * 8,
        .tx_buffer = fifo_buf,
    };
    esp_err_t ret = spi_device_polling_transmit(spi_lora, &wr);
    free(fifo_buf);
    if (ret != ESP_OK) return false;

    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_TX);

    int timeout = 0;
    while (!(lora_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE_MASK)) {
        vTaskDelay(pdMS_TO_TICKS(2));
        if (++timeout > 1500) {
            ESP_LOGE(TAG, "TX Timeout");
            lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
            return false;
        }
    }

    lora_write_reg(REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
    lora_write_reg(REG_OP_MODE, MODE_LONG_RANGE_MODE | MODE_STDBY);
    return true;
}

typedef struct {
    double lat, lon;
    float alt, spd;
    uint8_t sats;
    char timeUTC[20];
    bool valid;
} GPSData;

GPSData gpsData = {0};

double nmea_to_deg(const char *s, char dir) {
    if (!s || strlen(s) < 4) return 0.0;
    double raw = atof(s);
    int deg = (int)(raw / 100);
    double min = raw - deg * 100;
    double res = deg + min / 60.0;
    if (dir == 'S' || dir == 'W') res = -res;
    return res;
}

void parse_nmea_line(char *line) {
    if (strncmp(line, "$GNRMC", 6) == 0 || strncmp(line, "$GPRMC", 6) == 0) {
        char *tok = strtok(line, ",");
        int idx = 0; 
        char timeStr[10]="", dateStr[10]="", latStr[16]="", lonStr[16]="", spdStr[10]="";
        char latDir='N', lonDir='E', status='V';

        while(tok) {
            if (idx==1) strncpy(timeStr, tok, 9);
            else if (idx==2) status = tok[0];
            else if (idx==3) strncpy(latStr, tok, 15);
            else if (idx==4) latDir = tok[0];
            else if (idx==5) strncpy(lonStr, tok, 15);
            else if (idx==6) lonDir = tok[0];
            else if (idx==7) strncpy(spdStr, tok, 9);
            else if (idx==9) strncpy(dateStr, tok, 9);
            tok = strtok(NULL, ",");
            idx++;
        }
        if (status == 'A') {
            gpsData.valid = true;
            gpsData.lat = nmea_to_deg(latStr, latDir);
            gpsData.lon = nmea_to_deg(lonStr, lonDir);
            gpsData.spd = atof(spdStr) * 1.852f;

            if (strlen(dateStr)==6 && strlen(timeStr)>=6) {
                int d = (dateStr[0]-'0')*10 + (dateStr[1]-'0');
                int m = (dateStr[2]-'0')*10 + (dateStr[3]-'0');
                int y = 2000 + (dateStr[4]-'0')*10 + (dateStr[5]-'0');
                int hh = (timeStr[0]-'0')*10 + (timeStr[1]-'0') + 7;
                int mm = (timeStr[2]-'0')*10 + (timeStr[3]-'0');
                int ss = (timeStr[4]-'0')*10 + (timeStr[5]-'0');
                if (hh >= 24) { hh -= 24; d++; }
                // Clamp to valid range
                if (y < 2000 || y > 2099) y = 2026;
                if (m < 1 || m > 12) m = 1;
                if (d < 1 || d > 31) d = 1;
                if (hh < 0 || hh > 23) hh = 0;
                if (mm < 0 || mm > 59) mm = 0;
                if (ss < 0 || ss > 59) ss = 0;
                snprintf(gpsData.timeUTC, sizeof(gpsData.timeUTC), "%04d/%02d/%02d,%02d:%02d:%02d", y,m,d,hh,mm,ss);
            }
        }
    } else if (strncmp(line, "$GNGGA", 6) == 0 || strncmp(line, "$GPGGA", 6) == 0) {
        char *tok = strtok(line, ",");
        int idx = 0;
        while(tok) {
            if (idx == 7) gpsData.sats = (uint8_t)atoi(tok);
            else if (idx == 9) gpsData.alt = atof(tok);
            tok = strtok(NULL, ",");
            idx++;
        }
    }
}

void gps_uart_task(void *arg) {
    uint8_t *data = malloc(256);
    char line_buf[256];
    int line_idx = 0;

    while (1) {
        int len = uart_read_bytes(UART_NUM_2, data, 255, pdMS_TO_TICKS(100));
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                char c = data[i];
                if (c == '\n' || c == '\r') {
                    if (line_idx > 5) {
                        line_buf[line_idx] = '\0';
                        parse_nmea_line(line_buf);
                    }
                    line_idx = 0;
                } else if (line_idx < 255) {
                    line_buf[line_idx++] = c;
                }
            }
        }
    }
}

void load_config_nvs() {
    nvs_handle_t handle;
    if (nvs_open("cfg", NVS_READONLY, &handle) == ESP_OK) {
        size_t len;
        len = sizeof(g_tujuan);
        nvs_get_str(handle, "tujuan", g_tujuan, &len);
        len = sizeof(g_perusahaan);
        nvs_get_str(handle, "perusahaan", g_perusahaan, &len);
        len = sizeof(g_isi);
        nvs_get_str(handle, "isi", g_isi, &len);
        nvs_close(handle);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32 Bare Metal Transmitter Starting ===");

    ESP_ERROR_CHECK(nvs_flash_init());
    load_config_nvs();

    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    dataToSend.deviceID = (mac[3] << 16) | (mac[4] << 8) | mac[5];

    gpio_set_direction(PIN_GPS_EN, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_GPS_EN, 0);
    gpio_set_direction(PIN_MAGNETIC, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_MAGNETIC, GPIO_PULLUP_ONLY);

    // ADC Oneshot Init (ESP-IDF v6.0)
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CH_VIB, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CH_MQ7, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CH_MQ135, &config));

    uart_config_t uart_cfg = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, PIN_GPS_TX, PIN_GPS_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 1024, 0, 0, NULL, 0));

    lora_init_hw(923000000);

    xTaskCreatePinnedToCore(gps_uart_task, "gps_uart", 4096, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "System Ready. DeviceID: %lu", (unsigned long)dataToSend.deviceID);

    uint8_t packetCounter = 0;
    int64_t lastSend = 0;
    int lastDoor = 1;

    while (1) {
        int64_t now = esp_timer_get_time() / 1000;

        int vibVal = 0;
        adc_oneshot_read(adc1_handle, ADC_CH_VIB, &vibVal);
        bool vibDetected = (vibVal < 2000);

        int doorNow = gpio_get_level(PIN_MAGNETIC);
        bool doorTrigger = (doorNow != lastDoor);
        lastDoor = doorNow;

        // DEBUG: Print sensor values every 2 seconds
        static int64_t lastDebug = 0;
        if (now - lastDebug >= 2000) {
            ESP_LOGI(TAG, "DEBUG: vibVal=%d vibDetected=%d doorNow=%d doorTrigger=%d GPS_sats=%d", 
                     vibVal, vibDetected, doorNow, doorTrigger, gpsData.sats);
            lastDebug = now;
        }

        // Periodic test send every 10 seconds (for link verification)
        static int64_t lastPeriodicSend = 0;
        bool periodicSend = (now - lastPeriodicSend >= 10000);
        if (periodicSend) {
            lastPeriodicSend = now;
            ESP_LOGI(TAG, "Periodic test send...");
        }

        if (((vibDetected && (now - lastSend >= 5000)) || doorTrigger || periodicSend)) {
            packetCounter++;
            dataToSend.packetCounter = packetCounter;
            dataToSend.latitude = gpsData.lat;
            dataToSend.longitude = gpsData.lon;
            dataToSend.altitude = gpsData.alt;
            dataToSend.speed = gpsData.spd;
            dataToSend.satellites = gpsData.sats;
            strncpy(dataToSend.timeUTC, gpsData.timeUTC, sizeof(dataToSend.timeUTC)-1);
            dataToSend.timeUTC[sizeof(dataToSend.timeUTC)-1] = '\0';
            strncpy(dataToSend.tujuan, g_tujuan, sizeof(dataToSend.tujuan)-1);
            dataToSend.tujuan[sizeof(dataToSend.tujuan)-1] = '\0';
            strncpy(dataToSend.perusahaan, g_perusahaan, sizeof(dataToSend.perusahaan)-1);
            dataToSend.perusahaan[sizeof(dataToSend.perusahaan)-1] = '\0';
            strncpy(dataToSend.isi, g_isi, sizeof(dataToSend.isi)-1);
            dataToSend.isi[sizeof(dataToSend.isi)-1] = '\0';

            ESP_LOGI(TAG, "TX #%u | Lat: %.6f Lon: %.6f Sats: %d Door: %d",
                     packetCounter, gpsData.lat, gpsData.lon, gpsData.sats, dataToSend.doorValue);

            if (lora_send_packet((uint8_t*)&dataToSend, sizeof(dataToSend))) {
                ESP_LOGI(TAG, "LoRa TX OK");
            } else {
                ESP_LOGE(TAG, "LoRa TX FAIL");
            }
            lastSend = now;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
