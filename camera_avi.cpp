#include "camera_avi.h"
#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"

#include <WiFi.h>

// Time
#include "time.h"

// define the number of bytes you want to access
#define EEPROM_SIZE 1

#define TIMEZONE "GMT+8"             // your timezone  -  this is GMT

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// EDIT ssid and password
const char ssid[] = "DLWiFiNo";           // your wireless network name (SSID)
const char password[] = "13572468";  // your Wi-Fi network password
static const char devname[] = "desklens";         // name of your camera for mDNS, Router, and filenames

char localip[20];
char strftime_buf[64];

bool internet_connected = false;

struct tm timeinfo;

bool init_wifi()
{
  int connAttempts = 0;

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  WiFi.setHostname(devname);
  //WiFi.printDiag(Serial);
  WiFi.begin(ssid, password);
  delay(1000);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (connAttempts == 10) {
      Serial.println("Cannot connect - try again");
      WiFi.begin(ssid, password);
      WiFi.printDiag(Serial);
    }
    if (connAttempts == 20) {
      Serial.println("Cannot connect - fail");

      WiFi.printDiag(Serial);
      return false;
    }
    connAttempts++;
  }

  Serial.println("Internet connected");

  //WiFi.printDiag(Serial);

  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", TIMEZONE, 1);  // mountain time zone from #define at top
  tzset();

  time_t now ;
  timeinfo = { 0 };
  int retry = 0;
  const int retry_count = 10;
  delay(1000);
  time(&now);
  localtime_r(&now, &timeinfo);

  while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
    Serial.printf("Waiting for system time to be set... (%d/%d) -- %d\n", retry, retry_count, timeinfo.tm_year);
    delay(1000);
    time(&now);
    localtime_r(&now, &timeinfo);
  }

  Serial.println(ctime(&now));
  sprintf(localip, "%s", WiFi.localIP().toString().c_str());

  return true;

}

void camera_setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);
  //Serial.setDebugOutput(true);
  //Serial.println();
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 

  if (init_wifi()) { // Connected to WiFi
    internet_connected = true;
  }
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Init Camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  
  //Serial.println("Starting SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }
    
  camera_fb_t * fb = NULL;
  
  // Take Picture with Camera
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  strftime(strftime_buf, sizeof(strftime_buf), "%F_%H.%M.%S", &timeinfo);

  // Path where new picture will be saved in SD Card
  char path[100];
  sprintf(path, "/picture%s_%s.jpg", devname, strftime_buf);

  fs::FS &fs = SD_MMC; 
  Serial.printf("Picture file name: %s\n", path);
  
  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
  } 
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Saved file to path: %s\n", path);
  }
  file.close();
  esp_camera_fb_return(fb); 
  
  // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

}
