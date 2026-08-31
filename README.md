# esp32-ssd1306-utils
一个ESP32和SSD1306（带有四个按钮）基于**U8g2**的简单实用库，专为 **128x64 SSD1306 OLED屏幕 带四个按键**设计，包含简易输入法和简易联网功能。
感谢DeepSeek，贡献了99.9%的代码。

## 安装

这里以PlatformIO为例，把库直接放入lib文件夹即可
```text
your_project/
├── lib/
│   └── ssd1306_utils/
│       ├── ssd1306_utils.h
│       └── ssd1306_utils.cpp
├── src/
│   └── main.cpp
└── platformio.ini
```

## 建议ESP32与屏幕（带有四个按钮）的引脚连接

| OLED 引脚 | ESP32 引脚 |
|-----------|------------|
| VCC       | 3.3V      |
| GND       | GND       |
| SCL       | GPIO 21   |
| SDA       | GPIO 22   |
| 上        | GPIO 16   |
| 下        | GPIO 18   |
| #         | GPIO 19   |
| *         | GPIO 23   |

> 如果需要修改引脚，可以在ssd1306_utils.h中更改KEY_UP, KEY_DOWN, KEY_HASH, KEY_STAR这些定义

## 用法

主要有两个可用函数，其他都是内部函数，调用了可能会出错。

### `String getInputFromUser()`

- **功能**：启动图形化输入法，返回用户输入的字符串。
- **返回值**：用户输入的字符串。
  - 若用户未输入任何字符直接长按 `*` 确认，返回空字符串 `""`。
  - 若在输入过程中返回取消（当前未启用），预留 `INPUT_CANCEL` 标记（值为 `"__CANCEL__"`）。

### `WiFiConfigResult startWiFiConfig()`

- **功能**：启动 WiFi 配网界面，**阻塞直到连接成功**。
- **返回值**：结构体 `WiFiConfigResult`，定义如下：

```cpp
struct WiFiConfigResult {
  String ssid;
  String password;
};
```

## 界面用法

### 输入法
| 按钮 | 短按 | 长按 |
|------|------|------|
| 上 | 上一个字母 | / |
| 下 | 下一个字母 | / |
| #号 | 确认输入字母 | 切换字符集 |
| *号 | 删除上一个输入的字母 | 确定输入（如果为空则返回 ""） |

### WIFI连接界面
| 按钮 | 短按 | 长按 |
|------|------|------|
| 上 | 上一个 WiFi | / |
| 下 | 下一个 WiFi | / |
| #号 | 进入密码输入界面 | / |
| *号 | 刷新 WiFi 列表 | / |

## 需要的外部库

- **U8g2lib.h** 用于显示

## 示例代码

```cpp
#include <Arduino.h>
#include <ssd1306_utils.h>

void setup() {
  // 为调用做设置，否则会报错
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_HASH, INPUT_PULLUP);
  pinMode(KEY_STAR, INPUT_PULLUP);
  String result_1 = getInputFromUser(); // 这里会阻塞直到输入完成
  Serial.print("用户输入了: ");
  Serial.println(result_1);
  WiFiConfigResult result_2 = startWiFiConfig(); // 这里会阻塞直到连接成功
  Serial.println("WiFi 连接成功！");
  Serial.print("SSID: ");
  Serial.println(result_2.ssid);
  Serial.print("密码: ");
  Serial.println(result_2.password);
}

void loop() {
  delay(100);
}

```

## 修改字符集

如果你觉得字符集不符合你的要求，可以在ssd1306_utils.cpp更改，需要更改位置如下：
```cpp
const char* charset[] = {
  "abcdefghijklmnopqrstuvwxyz",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "0123456789!@#$%?&*()-_=+[]",
  "<>,.|\\/{}`~"
};
const char* charsetName[] = { "小写", "大写", "符号", "符号" };
```
charset列表中每个字符串都代表一页，分别对应着charsetName的页面名称。
