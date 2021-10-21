#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp32cam_pins.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_smartconfig.h"

// See COMPONENT_EMBED_FILES in CMakeList.txt
extern const uint8_t binary_pico_min_css_start[] asm("_binary_pico_min_css_start");
extern const uint8_t binary_pico_min_css_end[] asm("_binary_pico_min_css_end");
extern const uint8_t binary_index_html_start[] asm("_binary_index_html_start");
extern const uint8_t binary_index_html_end[] asm("_binary_index_html_end");

#define TAG "app_main"

#define GPIO_HIGH 1
#define GPIO_LOW 0

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;

static camera_config_t camera_config = {
        .pin_pwdn  = CAM_PWR_PIN,
        .pin_reset = -1, // Software reset
        .pin_xclk = CAM_XCLK_PIN,
        .pin_sscb_sda = CAM_SDA_PIN,
        .pin_sscb_scl = CAM_SCL_PIN,

        .pin_d7 = CAM_Y9_PIN,
        .pin_d6 = CAM_Y8_PIN,
        .pin_d5 = CAM_Y7_PIN,
        .pin_d4 = CAM_Y6_PIN,
        .pin_d3 = CAM_Y5_PIN,
        .pin_d2 = CAM_Y4_PIN,
        .pin_d1 = CAM_Y3_PIN,
        .pin_d0 = CAM_Y2_PIN,
        .pin_vsync = CAM_VSYNC_PIN,
        .pin_href = CAM_HREF_PIN,
        .pin_pclk = CAM_PCLK_PIN,

        //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = FRAMESIZE_HD,//QQVGA-QXGA Do not use sizes above QVGA when not JPEG

        .jpeg_quality = 12, //0-63 lower number means higher quality
        .fb_count = 1 //if more than one, i2s runs in continuous mode. Use only with JPEG
};

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

#ifdef CONFIG_ESPCAM_WIFI_CRED_SMARTCONFIG
static void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}
#endif

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
#ifdef CONFIG_ESPCAM_WIFI_CRED_STATIC
        ESP_LOGI(TAG, "Using preconfigured WiFi credentials");
        wifi_config_t wifi_config;
        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, CONFIG_ESPCAM_WIFI_SSID, strlen(CONFIG_ESPCAM_WIFI_SSID));
        memcpy(wifi_config.sta.password, CONFIG_ESPCAM_WIFI_PASSWORD, strlen(CONFIG_ESPCAM_WIFI_PASSWORD));
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
#elif defined CONFIG_ESPCAM_WIFI_CRED_SMARTCONFIG
        ESP_LOGI(TAG, "Using SmartConfig to obtain credentials");
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
#else
#error  No wifi credentials source defined
#endif
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

static esp_err_t esp32cam_initialise_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    EventBits_t uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
    if(!(uxBits & CONNECTED_BIT)) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t esp32cam_camera_init() {
    // Power up the camera by setting the power pin high
    if(CAM_PWR_PIN != -1){
        esp_err_t err = gpio_set_direction(CAM_PWR_PIN, GPIO_MODE_OUTPUT);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set direction for CAM_PWR_PIN");
            return err;
        }
        err = gpio_set_level(CAM_PWR_PIN, GPIO_HIGH);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to drive CAM_PWR_PIN to LOW");
            return err;
        }
    }

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    return ESP_OK;
}

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static esp_err_t stream_get_handler(httpd_req_t *req) {
    char *part_buf[64];
    httpd_resp_set_type(req, STREAM_CONTENT_TYPE);

    for(;;) {
        // Acquire a frame by requesting a frame buffer
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera Capture Failed");
            return ESP_FAIL;
        }

        ssize_t header_len = snprintf((char *) part_buf, 64, STREAM_PART, fb->len);
        esp_err_t err = httpd_resp_send_chunk(req, (const char *) part_buf, header_len);

        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, (const char *) fb->buf, (ssize_t)fb->len);
        }
        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(req, STREAM_BOUNDARY, (ssize_t)strlen(STREAM_BOUNDARY));
        }

        // Return the frame buffer back to the driver for reuse
        esp_camera_fb_return(fb);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send stream chunk");
            break;
        }
    }

    return ESP_OK;
}

static esp_err_t frame_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/jpeg");

    // Acquire a frame by requesting a frame buffer
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera Capture Failed");
        return ESP_FAIL;
    }

    esp_err_t err = httpd_resp_send(req, (char *)fb->buf, (ssize_t)fb->len);

    // Return the frame buffer back to the driver for reuse
    esp_camera_fb_return(fb);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send frame buffer");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t static_get_handler(httpd_req_t *req) {
    if (strcmp("/", req->uri) == 0) {
        httpd_resp_set_type(req, "text/html");
        return httpd_resp_send(req, (char *)binary_index_html_start, (ssize_t)(binary_index_html_end - binary_index_html_start));
    } else if (strcmp("/css/pico.min.css", req->uri) == 0) {
        httpd_resp_set_type(req, "text/html");
        return httpd_resp_send(req, (char *)binary_pico_min_css_start, (ssize_t)(binary_pico_min_css_end - binary_pico_min_css_start));
    }
    return ESP_FAIL;
}

static const httpd_uri_t index_get = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = static_get_handler,
        .user_ctx  = NULL
};

static const httpd_uri_t css_get = {
        .uri       = "/css/pico.min.css",
        .method    = HTTP_GET,
        .handler   = static_get_handler,
        .user_ctx  = NULL
};

static const httpd_uri_t frame_get = {
        .uri       = "/frame",
        .method    = HTTP_GET,
        .handler   = frame_get_handler,
        .user_ctx  = NULL
};

static const httpd_uri_t stream_get = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_get_handler,
        .user_ctx  = NULL
};

static esp_err_t esp32cam_start_http_server(httpd_handle_t *handle) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(handle, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed start webserver");
        return err;
    }

    err = httpd_register_uri_handler(*handle, &index_get);
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(*handle, &css_get);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(*handle, &frame_get);
    }
    if (err == ESP_OK) {
        err = httpd_register_uri_handler(*handle, &stream_get);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed set handler");
        httpd_stop(&handle);
        handle = NULL;
        return err;
    }

    return ESP_OK;
}

void app_main(void)
{
    // Initialize NVS.
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    // Enable the onboard LED
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_direction(BUILTIN_LED_PIN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(BUILTIN_LED_PIN, GPIO_LOW));

    // Init WiFi (with SmartConfig)
    ESP_ERROR_CHECK(esp32cam_initialise_wifi());

    // Initialize Camera
    ESP_ERROR_CHECK(esp32cam_camera_init());

    // Initialize HTTP Server
    httpd_handle_t server;
    ESP_ERROR_CHECK(esp32cam_start_http_server(&server));

    size_t clients = 0;
    int ids[16];
    while (1) {
        httpd_get_client_list(server, &clients, (int *)&ids);
        printf("Http clients connected: %d\n", clients);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
