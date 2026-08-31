#ifndef EXTRA_H
#define EXTRA_H

#include <Arduino.h>
#include <U8g2lib.h>

// 声明外部屏幕对象
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// 引脚定义
#define SCLK_PIN  21
#define SDA_PIN   22
#define KEY_UP    16
#define KEY_DOWN  18
#define KEY_HASH  19
#define KEY_STAR  23

// 输入法函数声明
void ssd1306UtilsInit();
String getInputFromUser();
struct WiFiConfigResult {
    String ssid;
    String password;
};
WiFiConfigResult startWiFiConfig();

#endif