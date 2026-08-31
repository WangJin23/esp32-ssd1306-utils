#include "ssd1306_utils.h"
#include <WiFi.h>
#include <vector>
#include <map>
#include <string.h>

// ---- 屏幕对象（使用你的引脚） ----
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 21, 22);

// ---- 字符集定义 ----
const char* charset[] = {
  "abcdefghijklmnopqrstuvwxyz",
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
  "0123456789!@#$%?&*()-_=+[]",
  "<>,.|\\/{}`~"
};
const char* charsetName[] = { "小写", "大写", "符号", "符号" };
const int charsetCount = sizeof(charset) / sizeof(charset[0]);
int charsetLen[charsetCount];  // 运行时计算长度

// ---- 输入法状态变量 ----
int currentSet = 0;
int highlightIndex = 0;
String inputBuffer = "";

bool hashLongPress = false;
bool starLongPress = false;
unsigned long hashPressStart = 0;
unsigned long starPressStart = 0;

// ---- 绘制界面（内部函数） ----
static void drawInputUI() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.enableUTF8Print();

  // 输入框
  u8g2.setCursor(0, 12);
  u8g2.print("> ");
  u8g2.print(inputBuffer);
  int cursorX = u8g2.getStrWidth(("> " + inputBuffer).c_str());
  u8g2.drawLine(cursorX, 14, cursorX + 2, 14);

  // 分隔线
  u8g2.drawHLine(0, 16, 128);

  // 字符键盘（两行）
  const char* currentChars = charset[currentSet];
  int len = charsetLen[currentSet];
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

  // 底部提示
  u8g2.setCursor(2, 58);
  u8g2.print(charsetName[currentSet]);
  u8g2.setCursor(34, 58);
  u8g2.print("#输/切 *删/确");

  u8g2.sendBuffer();
}

// ---- 公开函数：阻塞式输入法 ----
String getInputFromUser() {
  // 重置状态
  inputBuffer = "";
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

    // # 键：短按输入，长按切换字符集
    if (hash == LOW && lastHash == HIGH) {
      hashPressStart = millis();
      hashLongPress = false;
    }
    if (hash == LOW && !hashLongPress) {
      if (millis() - hashPressStart > 500) {
        currentSet = (currentSet + 1) % charsetCount;
        highlightIndex = 0;
        hashLongPress = true;
      }
    }
    if (hash == HIGH && lastHash == LOW) {
      if (!hashLongPress) {
        const char* chars = charset[currentSet];
        inputBuffer += chars[highlightIndex];
      }
      hashLongPress = false;
    }

    // * 键：短按删除，长按确认（不再有双击取消）
    if (star == LOW && lastStar == HIGH) {
      starPressStart = millis();
      starLongPress = false;
    }

    if (star == LOW && !starLongPress) {
      if (millis() - starPressStart > 800) {
        // 长按确认，返回当前输入（可能为空）
        String result = inputBuffer;
        inputBuffer = "";
        u8g2.clearDisplay();
        return result;
      }
    }

    if (star == HIGH && lastStar == LOW) {
      if (!starLongPress) {
        // 短按删除一个字符
        if (inputBuffer.length() > 0) {
          inputBuffer.remove(inputBuffer.length() - 1);
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
    if (networks[i].encryptionType != WIFI_AUTH_OPEN) label += " 🔒";
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
  WiFi.mode(WIFI_STA);        // 重置为STA模式，确保扫描正常
  delay(100);                 // 等待模式切换

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.enableUTF8Print();
  u8g2.setCursor(10, 30);
  u8g2.print("扫描中...");
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
    // 如果已连接，记录结果并返回
    if (WiFi.status() == WL_CONNECTED) {
      result.ssid = WiFi.SSID();
      // 注意：WiFi.psk() 在某些 ESP32 版本可能不可用，改用用户输入的密码（见下方）
      // 所以我们在连接成功时直接记录 password，而不是从 WiFi 读取
      // 为安全起见，如果 WiFi.psk() 可用则使用，否则使用记录值
      #ifdef ESP32
        result.password = WiFi.psk();  // 部分版本支持
      #else
        result.password = "";  // 由调用者保存
      #endif
      // 显示成功信息
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
          if (password == INPUT_CANCEL || password.length() == 0) {
            drawWifiList();
            continue;
          }
        }

        bool connected = connectToWiFi(selected.ssid, password);
        if (connected) {
          result.ssid = selected.ssid;
          result.password = password;   // 记录用户输入的密码
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