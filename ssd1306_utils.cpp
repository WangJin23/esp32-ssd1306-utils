//esp32-ssd1306-utils v1.0.1
#include "ssd1306_utils.h"
#include <WiFi.h>
#include <vector>
#include <map>
#include <string.h>

// ---- 屏幕对象（使用你的引脚） ----
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCLK_PIN, SDA_PIN);

// ---- 字符集定义（新增 Unicode 页） ----
const char* charset[] = {
  "abcdefghijklmnopqrstuvwxyz",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "0123456789!@#$%?&*()-_=+[]",
  "<>,.|\\/{}`~",
  "0123456789ABCDEF"   // Unicode 输入页
};
const char* charsetName[] = { "小写", "大写", "符号", "符号", "Unicode" };
const int charsetCount = sizeof(charset) / sizeof(charset[0]);
int charsetLen[charsetCount];  // 运行时计算长度

// ---- 输入法状态变量 ----
int currentSet = 0;
int highlightIndex = 0;
String inputBuffer = "";
String unicodeBuffer = "";   // 存放当前输入的十六进制码点（最多4位）

bool hashLongPress = false;
bool starLongPress = false;
unsigned long hashPressStart = 0;
unsigned long starPressStart = 0;

void ssd1306UtilsInit() {
  u8g2.begin();
  u8g2.enableUTF8Print();
  pinMode(KEY_UP, INPUT_PULLUP);
  pinMode(KEY_DOWN, INPUT_PULLUP);
  pinMode(KEY_HASH, INPUT_PULLUP);
  pinMode(KEY_STAR, INPUT_PULLUP);
}
// ---- Unicode 码点转 UTF-8 字符串 ----
String unicodeToUTF8(uint32_t codePoint) {
  String result = "";
  if (codePoint <= 0x7F) {
    // 1 字节: 0xxxxxxx
    result += (char)codePoint;
  } else if (codePoint <= 0x7FF) {
    // 2 字节: 110xxxxx 10xxxxxx
    result += (char)(0xC0 | ((codePoint >> 6) & 0x1F));
    result += (char)(0x80 | (codePoint & 0x3F));
  } else if (codePoint <= 0xFFFF) {
    // 3 字节: 1110xxxx 10xxxxxx 10xxxxxx
    result += (char)(0xE0 | ((codePoint >> 12) & 0x0F));
    result += (char)(0x80 | ((codePoint >> 6) & 0x3F));
    result += (char)(0x80 | (codePoint & 0x3F));
  }
  return result;
}
// ---- 删除字符串末尾一个完整的 UTF-8 字符 ----
// ---- 删除字符串末尾一个完整的 UTF-8 字符（如果可能） ----
static void removeLastUTF8Char(String &str) {
    if (str.length() == 0) return;
    int len = str.length();
    int pos = len - 1;
    // 如果最后一个字节是 ASCII（<0x80）或 UTF-8 起始字节（>=0xC0），则直接删除它
    // 否则它是连续字节（0x80-0xBF），需要向前找到起始字节
    while (pos > 0 && ((unsigned char)str[pos] & 0xC0) == 0x80) {
        pos--;
    }
    // 现在 pos 指向起始字节（或 ASCII），删除从 pos 到末尾的所有内容
    str.remove(pos);
}
// ---- 获取字符串末尾 UTF-8 字符的字节数 ----
static int getLastCharByteLen(const String &str) {
    if (str.length() == 0) return 0;
    unsigned char last = str[str.length() - 1];
    if (last >= 0xE0 && last <= 0xEF) return 3;      // 中文 3 字节
    if (last >= 0xC0 && last <= 0xDF) return 2;      // 少数 2 字节字符
    return 1;                                         // ASCII 或单字节
}
// ---- 判断是否为 Unicode 页 ----
static bool isUnicodePage() {
    return currentSet == (charsetCount - 1);
}

// ---- 绘制界面（内部函数） ----
static void drawInputUI() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.enableUTF8Print();

  // ---- 输入框（顶部） ----
  u8g2.setCursor(0, 12);
  
  if (isUnicodePage() && unicodeBuffer.length() > 0) {
    // Unicode 模式：显示 U+ 和码点，以及预览字符
    u8g2.print("U+");
    u8g2.print(unicodeBuffer);
    
    // 预览当前码点对应的字符（如果有效）
    if (unicodeBuffer.length() == 4) {
      long codePoint = strtol(unicodeBuffer.c_str(), NULL, 16);
      if (codePoint > 0 && codePoint <= 0xFFFF) {
        u8g2.print(" → ");
        u8g2.print(unicodeToUTF8(codePoint));
      }
    }
  } else {
    // 普通模式：显示已输入内容
    u8g2.print("> ");
    u8g2.print(inputBuffer);
  }
  
  // ---- 光标（下划线）：直接放在当前光标位置 ----
  int cursorX = u8g2.getCursorX();   // 获取当前文字末尾的 X 坐标
  u8g2.drawLine(cursorX, 14, cursorX + 2, 14);

  // ---- 分隔线 ----
  u8g2.drawHLine(0, 16, 128);

  // ---- 字符键盘 ----
  const char* currentChars = charset[currentSet];
  int len = charsetLen[currentSet];
  
  if (isUnicodePage()) {
    // Unicode 页特殊布局：三行十六进制键盘
    // 第一行: 0 1 2 3 4 5 6 7
    // 第二行: 8 9 A B C D E F
    // 第三行: U+ 显示 + 功能提示
    int spacing = 14;
    int xStart = (128 - 8 * spacing) / 2;
    if (xStart < 0) xStart = 0;
    
    // 第一行: 0-7
    int y1 = 26;
    for (int i = 0; i < 8; i++) {
      int x = xStart + i * spacing;
      char ch = currentChars[i];
      if (i == highlightIndex) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(x-1, y1-10, 12, 14);
        u8g2.setDrawColor(0);
        u8g2.setCursor(x, y1);
        u8g2.print(ch);
        u8g2.setDrawColor(1);
      } else {
        u8g2.setCursor(x, y1);
        u8g2.print(ch);
      }
    }
    
    // 第二行: 8-F
    int y2 = 40;
    for (int i = 8; i < 16; i++) {
      int idx = i - 8;
      int x = xStart + idx * spacing;
      char ch = currentChars[i];
      if (i == highlightIndex) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(x-1, y2-10, 12, 14);
        u8g2.setDrawColor(0);
        u8g2.setCursor(x, y2);
        u8g2.print(ch);
        u8g2.setDrawColor(1);
      } else {
        u8g2.setCursor(x, y2);
        u8g2.print(ch);
      }
    }
    
    // 底部提示
    u8g2.setCursor(2, 58);
    u8g2.print("U+");
    u8g2.print(unicodeBuffer);
    if (unicodeBuffer.length() == 4) {
      long codePoint = strtol(unicodeBuffer.c_str(), NULL, 16);
      if (codePoint > 0 && codePoint <= 0xFFFF) {
        u8g2.print(" → ");
        u8g2.print((char)codePoint);
      }
    }
    u8g2.setCursor(50, 58);
    u8g2.print("#输/切 *删/确");
    
  } else {
    // 普通页布局（原样）
    int half = (len + 1) / 2;
    int spacing = 10;
    int xStart = (128 - (half > 13 ? 13 : half) * spacing) / 2;
    if (xStart < 0) xStart = 0;

    int y1 = 26;
    for (int i = 0; i < half && i < len; i++) {
      int x = xStart + i * spacing;
      char ch = currentChars[i];
      if (i == highlightIndex) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(x-1, y1-10, 10, 14);
        u8g2.setDrawColor(0);
        u8g2.setCursor(x, y1);
        u8g2.print(ch);
        u8g2.setDrawColor(1);
      } else {
        u8g2.setCursor(x, y1);
        u8g2.print(ch);
      }
    }

    int y2 = 40;
    for (int i = half; i < len; i++) {
      int idx = i - half;
      int x = xStart + idx * spacing;
      char ch = currentChars[i];
      if (i == highlightIndex) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(x-1, y2-10, 10, 14);
        u8g2.setDrawColor(0);
        u8g2.setCursor(x, y2);
        u8g2.print(ch);
        u8g2.setDrawColor(1);
      } else {
        u8g2.setCursor(x, y2);
        u8g2.print(ch);
      }
    }

    // 底部提示（普通页）
    u8g2.setCursor(2, 58);
    u8g2.print(charsetName[currentSet]);
    u8g2.setCursor(34, 58);
    u8g2.print("#输/切 *删/确");
  }

  u8g2.sendBuffer();
}
// ---- 公开函数：阻塞式输入法 ----
String getInputFromUser() {
  // 重置状态
  inputBuffer = "";
  unicodeBuffer = "";
  highlightIndex = 0;
  currentSet = 0;
  hashLongPress = false;
  starLongPress = false;
  hashPressStart = 0;
  starPressStart = 0;

  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < charsetCount; i++) {
      charsetLen[i] = strlen(charset[i]);
    }
    initialized = true;
  }

  static bool lastUp = HIGH, lastDown = HIGH, lastHash = HIGH, lastStar = HIGH;
  unsigned long lastDrawTime = 0;

  while (true) {
    bool up = digitalRead(KEY_UP);
    bool down = digitalRead(KEY_DOWN);
    bool hash = digitalRead(KEY_HASH);
    bool star = digitalRead(KEY_STAR);

    // 上键
    if (up == LOW && lastUp == HIGH) {
      highlightIndex = (highlightIndex - 1 + charsetLen[currentSet]) % charsetLen[currentSet];
    }
    // 下键
    if (down == LOW && lastDown == HIGH) {
      highlightIndex = (highlightIndex + 1) % charsetLen[currentSet];
    }

    // # 键
    if (hash == LOW && lastHash == HIGH) {
      hashPressStart = millis();
      hashLongPress = false;
    }
    if (hash == LOW && !hashLongPress) {
      if (millis() - hashPressStart > 500) {
        // 长按 #：切换字符集
        // 如果在 Unicode 页且码点未满4位，禁止切换
        if (isUnicodePage() && unicodeBuffer.length() > 0 && unicodeBuffer.length() < 4) {
          // 禁止切换，忽略操作
        } else {
          currentSet = (currentSet + 1) % charsetCount;
          highlightIndex = 0;
          // 离开 Unicode 页时清空 unicodeBuffer
          if (!isUnicodePage()) {
            unicodeBuffer = "";
          }
        }
        hashLongPress = true;
      }
    }
    if (hash == HIGH && lastHash == LOW) {
      if (!hashLongPress) {
        // 短按 #：输入当前字符
        if (isUnicodePage()) {
          // Unicode 页：输入十六进制字符
          const char* chars = charset[currentSet];
          char ch = chars[highlightIndex];
          // 限制最多4位
          if (unicodeBuffer.length() < 4) {
            unicodeBuffer += ch;
            // 当输入满4位时，自动转换成字符并加入 inputBuffer
            if (unicodeBuffer.length() == 4) {
              long codePoint = strtol(unicodeBuffer.c_str(), NULL, 16);
              if (codePoint > 0 && codePoint <= 0xFFFF) {
                inputBuffer += unicodeToUTF8(codePoint);  // ✅ 正确
              }
              unicodeBuffer = "";
            }
          }
        } else {
          // 普通页：输入字符
          const char* chars = charset[currentSet];
          inputBuffer += chars[highlightIndex];
        }
      }
      hashLongPress = false;
    }

    // * 键：短按删除，长按确认
    if (star == LOW && lastStar == HIGH) {
      starPressStart = millis();
      starLongPress = false;
    }

    if (star == LOW && !starLongPress) {
      if (millis() - starPressStart > 800) {
        // 长按确认，返回当前输入（可能为空）
        // 如果在 Unicode 页且码点未满4位，禁止确认
        if (isUnicodePage() && unicodeBuffer.length() > 0 && unicodeBuffer.length() < 4) {
          // 禁止确认，忽略操作
        } else {
          String result = inputBuffer;
          inputBuffer = "";
          unicodeBuffer = "";
          u8g2.clearDisplay();
          return result;
        }
      }
    }

    if (star == HIGH && lastStar == LOW) {
      if (!starLongPress) {
        // 短按删除
        if (isUnicodePage()) {
          // Unicode 页：优先删 unicodeBuffer，否则删 inputBuffer 末尾字符
          if (unicodeBuffer.length() > 0) {
            unicodeBuffer.remove(unicodeBuffer.length() - 1);
          } else {
            removeLastUTF8Char(inputBuffer);
          }
        } else {
          removeLastUTF8Char(inputBuffer);
        }
      }
      starLongPress = false;
    }

    lastUp = up;
    lastDown = down;
    lastHash = hash;
    lastStar = star;

    if (millis() - lastDrawTime > 50) {
      drawInputUI();
      lastDrawTime = millis();
    }
    delay(10);
  }
}

// ---- 以下为 WiFi 相关代码（保持不变） ----
struct Network {
  String ssid;
  uint8_t encryptionType;
  int rssi;
};

static std::vector<Network> networks;
static int selectedIndex = 0;
static int displayStart = 0;
static const int DISPLAY_COUNT = 4;

static void drawWifiList() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.enableUTF8Print();

  u8g2.setCursor(0, 12);
  u8g2.print("WiFi 列表");
  u8g2.setCursor(85, 12);
  u8g2.print("#确 *刷");
  u8g2.drawHLine(0, 16, 128);

  if (networks.empty()) {
    u8g2.setCursor(20, 40);
    u8g2.print("无可用网络");
    u8g2.sendBuffer();
    return;
  }

  int total = networks.size();
  int end = min(displayStart + DISPLAY_COUNT, total);
  for (int i = displayStart; i < end; i++) {
    int y = 28 + (i - displayStart) * 10;
    String label = networks[i].ssid;
    if (i == selectedIndex) {
      u8g2.setDrawColor(1);
      u8g2.drawBox(0, y-8, 128, 10);
      u8g2.setDrawColor(0);
      u8g2.setCursor(2, y);
      u8g2.print(label);
      u8g2.setDrawColor(1);
    } else {
      u8g2.setCursor(2, y);
      u8g2.print(label);
    }
  }
  u8g2.sendBuffer();
}

static void scanWiFi() {
  WiFi.mode(WIFI_STA);
  delay(100);

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.enableUTF8Print();
  u8g2.setCursor(10, 30);
  u8g2.print("加载WiFi列表中...");
  u8g2.sendBuffer();

  networks.clear();
  int n = WiFi.scanNetworks();
  if (n > 0) {
    std::map<String, Network> unique;
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;
      if (unique.find(ssid) == unique.end() || WiFi.RSSI(i) > unique[ssid].rssi) {
        Network net;
        net.ssid = ssid;
        net.encryptionType = WiFi.encryptionType(i);
        net.rssi = WiFi.RSSI(i);
        unique[ssid] = net;
      }
    }
    for (auto &pair : unique) {
      networks.push_back(pair.second);
    }
  }
  WiFi.scanDelete();
  selectedIndex = 0;
  displayStart = 0;
}

static void showMessage(const String &msg, int y = 30) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.enableUTF8Print();
  u8g2.setCursor(0, y);
  u8g2.print(msg);
  u8g2.sendBuffer();
}

static bool connectToWiFi(const String &ssid, const String &password) {
  showMessage("正在连接...", 30);
  WiFi.begin(ssid.c_str(), password.c_str());
  int timeout = 15000;
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

static void showConnected() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.enableUTF8Print();
  u8g2.setCursor(0, 30);
  u8g2.print("连接成功！");
  u8g2.setCursor(0, 50);
  u8g2.print("IP: " + WiFi.localIP().toString());
  u8g2.sendBuffer();
  while (true) {
    delay(1000);
  }
}

WiFiConfigResult startWiFiConfig() {
  WiFiConfigResult result;
  result.ssid = "";
  result.password = "";

  scanWiFi();

  static bool lastUp = HIGH;
  static bool lastDown = HIGH;
  static bool lastHash = HIGH;
  static bool lastStar = HIGH;

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      result.ssid = WiFi.SSID();
      #ifdef ESP32
        result.password = WiFi.psk();
      #else
        result.password = "";
      #endif
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
      u8g2.enableUTF8Print();
      u8g2.setCursor(0, 30);
      u8g2.print("连接成功！");
      u8g2.setCursor(0, 50);
      u8g2.print("IP: " + WiFi.localIP().toString());
      u8g2.sendBuffer();
      delay(2000);
      return result;
    }

    static unsigned long lastKeyTime = 0;
    if (millis() - lastKeyTime < 200) {
      drawWifiList();
      continue;
    }

    bool up = digitalRead(KEY_UP);
    bool down = digitalRead(KEY_DOWN);
    bool hash = digitalRead(KEY_HASH);
    bool star = digitalRead(KEY_STAR);

    if (up == LOW && lastUp == HIGH) {
      if (selectedIndex > 0) {
        selectedIndex--;
        if (selectedIndex < displayStart) displayStart = selectedIndex;
      }
      lastKeyTime = millis();
    }
    if (down == LOW && lastDown == HIGH) {
      if (selectedIndex < (int)networks.size() - 1) {
        selectedIndex++;
        if (selectedIndex >= displayStart + DISPLAY_COUNT) {
          displayStart = selectedIndex - DISPLAY_COUNT + 1;
        }
      }
      lastKeyTime = millis();
    }

    if (hash == LOW && lastHash == HIGH) {
      lastKeyTime = millis();
      if (networks.empty()) {
        scanWiFi();
        drawWifiList();
      } else {
        Network selected = networks[selectedIndex];
        String password = "";
        bool needPassword = (selected.encryptionType != WIFI_AUTH_OPEN);

        if (needPassword) {
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
          u8g2.enableUTF8Print();
          u8g2.setCursor(0, 20);
          u8g2.print("两秒后输入密码");
          u8g2.setCursor(0, 32);
          u8g2.print("如果这不是你想要的WiFi");
          u8g2.setCursor(0, 44);
          u8g2.print("请直接输入空密码返回");
          u8g2.sendBuffer();
          delay(2000);
          password = getInputFromUser();
          if (password.length() == 0) {
            scanWiFi();
            drawWifiList();
            continue;
          }
        }

        bool connected = connectToWiFi(selected.ssid, password);
        if (connected) {
          result.ssid = selected.ssid;
          result.password = password;
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
          u8g2.enableUTF8Print();
          u8g2.setCursor(0, 30);
          u8g2.print("连接成功！");
          u8g2.setCursor(0, 50);
          u8g2.print("IP: " + WiFi.localIP().toString());
          u8g2.sendBuffer();
          delay(2000);
          return result;
        } else {
          showMessage("连接失败！", 30);
          delay(1500);
          drawWifiList();
        }
      }
    }

    if (star == LOW && lastStar == HIGH) {
      lastKeyTime = millis();
      scanWiFi();
      drawWifiList();
    }

    lastUp = up;
    lastDown = down;
    lastHash = hash;
    lastStar = star;

    drawWifiList();
    delay(10);
  }
}