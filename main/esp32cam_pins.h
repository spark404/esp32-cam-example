/**
* Header with the pin definitions for the ESP32 CAM connected peripherals
*/

// These pins are connected to the onboard card reader
#define HS2_CLK_PIN     14
#define HS2_CMD_PIN     15
#define HS2_DATA0_PIN    2
#define HS2_DATA1_PIN    4
#define HS2_DATA2_PIN   12
#define HS2_DATA3_PIN   13

// Onboard FLASH
// This is combined with the SD Card DATA1 pin!
#define FLASH_LED_PIN    4

// Connected to the ESP UART 0
#define U0TXD_PIN        1
#define U0RXD_PIN        3

// Built in LED (inverted logic)
#define BUILTIN_LED_PIN 33

// Camera
#define CAM_Y2_PIN       5
#define CAM_Y3_PIN      18
#define CAM_Y4_PIN      19
#define CAM_Y5_PIN      21
#define CAM_Y6_PIN      36
#define CAM_Y7_PIN      39
#define CAM_Y8_PIN      34
#define CAM_Y9_PIN      35
#define CAM_XCLK_PIN     0
#define CAM_PCLK_PIN    22
#define CAM_VSYNC_PIN   25
#define CAM_HREF_PIN    23
#define CAM_SDA_PIN     26
#define CAM_SCL_PIN     27
#define CAM_PWR_PIN     32