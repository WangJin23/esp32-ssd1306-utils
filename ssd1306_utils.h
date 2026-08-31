#ifndef EXTRA_H
#define EXTRA_H

#include <Arduino.h>
#include <U8g2lib.h>

// 声明外部屏幕对象（在 extra.cpp 中定义）
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// 按键引脚定义
#define KEY_UP    16
#define KEY_DOWN  18
#define KEY_HASH  19
#define KEY_STAR  23

// 输入法函数声明
String getInputFromUser();

struct WiFiConfigResult {
    String ssid;
    String password;
};

WiFiConfigResult startWiFiConfig();

#endif
