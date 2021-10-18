#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32cam_pins.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_camera.h"

#define TAG "app_main"

#define GPIO_HIGH 1
#define GPIO_LOW 0

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
        .frame_size = FRAMESIZE_SVGA,//QQVGA-QXGA Do not use sizes above QVGA when not JPEG

        .jpeg_quality = 12, //0-63 lower number means higher quality
        .fb_count = 1 //if more than one, i2s runs in continuous mode. Use only with JPEG
};

esp_err_t esp32cam_camera_init() {
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
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_set_level(BUILTIN_LED_PIN, GPIO_HIGH));

    // Initialize Camera
    ESP_ERROR_CHECK(esp32cam_camera_init());

    int i = 0;
    while (1) {
        printf("[%d] Hello world!\n", i);
        i++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
