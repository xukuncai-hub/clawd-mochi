/*
 * ╔══════════════════════════════════════════════════════════════╗
 *   CLAWD MOCHI — ESP32-C3 Super Mini + ST7789 1.54" 240×240
 *
 *   Wiring:
 *     SDA → GPIO 10  (hardware SPI MOSI)
 *     SCL → GPIO 8   (hardware SPI SCK)
 *     RST → GPIO 2
 *     DC  → GPIO 1
 *     CS  → GPIO 4
 *     BL  → GPIO 3
 *     VCC → 3V3
 *     GND → GND
 *
 *   WiFi: "ClaWD-Mochi"  pw: clawd1234  → http://192.168.4.1
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <math.h>
#include <time.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <WebSocketsServer.h>
#include "dance_frames.h"

// ── Pins ──────────────────────────────────────────────────────
#define TFT_CS  4
#define TFT_DC  1
#define TFT_RST 2
#define TFT_BLK 3
#define BUZZER  6     // buzzer (GPIO 6, safe during boot)

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ── WiFi ──────────────────────────────────────────────────────
const char* AP_SSID = "ClaWD-Mochi";
const char* AP_PASS = "clawd1234";

// ── WiFi Station mode (optional) ───────────────────────────────
// Set your home WiFi credentials here so Clawd Mochi joins your
// network.  This lets clawd-on-desk reach it while your computer
// still has internet for Claude Code.
// Leave STA_SSID empty ("") to use AP-only mode.
const char* STA_SSID = "p15";
const char* STA_PASS = "123456789";
String staIP = "";   // set at runtime if STA connects

WebServer server(80);
WebSocketsServer wsServer(81);
Preferences prefs;

// ── Display ───────────────────────────────────────────────────
#define DISP_W 240
#define DISP_H 240

// ── Eye constants (shared by both eye views) ──────────────────
#define EYE_W   30
#define EYE_H   60
#define EYE_GAP 120
#define EYE_OX  0     // horizontal offset
#define EYE_OY  40    // vertical offset upward (subtracted from centre)

// ── Colours ───────────────────────────────────────────────────
uint16_t C_ORANGE, C_DARKBG, C_MUTED, C_GREEN;
#define C_WHITE ST77XX_WHITE
#define C_BLACK ST77XX_BLACK

// ── Clock & Weather ───────────────────────────────────────────
#define NTP_SERVER       "pool.ntp.org"
#define WEATHER_INTERVAL 900000   // 15 min

// ── Location ──────────────────────────────────────────────────
// Set MY_CITY to your city (English name).  Geocoding API turns
// it into exact lat/lon — 100% accurate, no IP guessing.
// Leave "" to auto-detect from public IP (can be inaccurate on mobile).
//
//   "Beijing"  "Shanghai"  "Chongqing"  "Chengdu"
//   "Shenzhen" "Guangzhou" "Hangzhou"   "Wuhan"
//   "Tokyo"    "Seoul"     "London"     "New York"
//
// Direct lat/lon (0 = use geocoding/IP auto-detect instead)
// Chongqing districts: Yubei(29.72,106.62) Jiangbei(29.73,106.64) Yuzhong(29.55,106.57)
//   Nan'an(29.52,106.56) Shapingba(29.56,106.46) Jiulongpo(29.50,106.51)
//   Dadukou(29.48,106.48) Ba'nan(29.38,106.52) Beibei(29.83,106.44) Hechuan(29.99,106.26)
#define MY_LAT          30.19     // ← set your latitude (0=auto)
#define MY_LON          105.84    // ← set your longitude (0=auto)
#define MY_CITY_NAME    "Chongqing"  // displayed on screen
#define MY_CITY         ""        // city name for geocoding (only used when MY_LAT=0)
#define MY_TZ_OFFSET    8         // UTC offset: China=8, Japan=9, UK=0, US/East=-5

float    wxLat           = MY_LAT ? MY_LAT : 29.56f;
float    wxLon           = MY_LON ? MY_LON : 106.55f;
String   wxCity          = MY_CITY_NAME;
String   wxTz            = "CST-8";   // set from MY_TZ_OFFSET at runtime

// WMO weather code buckets
#define WX_CLEAR          0   // 0
#define WX_PARTLY_CLOUDY  1   // 1-3
#define WX_OVERCAST       2
#define WX_FOG            3   // 45, 48
#define WX_DRIZZLE        4   // 51-57
#define WX_RAIN           5   // 61-67
#define WX_SNOW           6   // 71-77
#define WX_THUNDER        7   // 95-99

// ── State ─────────────────────────────────────────────────────
#define VIEW_EYES_NORMAL    0
#define VIEW_EYES_SQUISH    1
#define VIEW_CODE           2
#define VIEW_DRAW           3
#define VIEW_EYES_ANGRY     4
#define VIEW_EYES_HEART     5
#define VIEW_EYES_DIZZY     6
#define VIEW_EYES_SLEEPY    7
#define VIEW_EYES_SURPRISED 8
#define VIEW_WINK           9
#define VIEW_CLOCK_DIGITAL  10
#define VIEW_CLOCK_ANALOG   11
#define VIEW_WEATHER        12
#define VIEW_ALARM          14
#define VIEW_TIMER          15
#define VIEW_EYES_CRY       16
#define VIEW_EYES_STAR      17
#define VIEW_EYES_SHY       18
#define VIEW_EYES_PUPPY     19
#define VIEW_DANCE          20

uint8_t  currentView  = VIEW_EYES_NORMAL;
bool     busy         = false;
bool     backlightOn  = true;
uint8_t  animSpeed    = 1;   // 1=slow(default) 2=normal 3=fast

uint16_t animBgColor  = 0;   // background for eye/logo animations
uint16_t drawBgColor  = 0;   // background for canvas

// ── Auto‑switch ──────────────────────────────────────────────────
bool     autoSwitch     = false;
uint8_t  autoSwitchMode = 0;
uint32_t autoSwitchInterval = 5000;
uint32_t lastAutoSwitch = 0;
uint8_t  autoSwitchIdx  = 0;

// ── Idle blink ────────────────────────────────────────────────
uint32_t lastIdleBlink = 0;
uint32_t idleBlinkNext = 0;  // randomised each blink

// Auto-switch groups
const uint8_t AUTO_EXPR[] PROGMEM = {
  VIEW_EYES_NORMAL, VIEW_EYES_SQUISH, VIEW_EYES_ANGRY,
  VIEW_EYES_HEART, VIEW_EYES_DIZZY, VIEW_EYES_SLEEPY,
  VIEW_EYES_SURPRISED, VIEW_WINK,
  VIEW_EYES_CRY, VIEW_EYES_STAR, VIEW_EYES_SHY, VIEW_EYES_PUPPY
};
const uint8_t AUTO_INFO[] PROGMEM = {
  VIEW_CLOCK_DIGITAL, VIEW_CLOCK_ANALOG, VIEW_WEATHER
};
#define AUTO_EXPR_COUNT 12
#define AUTO_INFO_COUNT 3

// ── Clock / Weather state ─────────────────────────────────────
bool     timeSynced        = false;
time_t   nowEpoch          = 0;   // (unused, kept for compatibility)
struct tm timeInfo;
char     timeStr[16];
char     dateStr[32];

float    wxTemp            = 0;
uint8_t  wxHumidity        = 0;
uint8_t  wxCode            = 0;
float    wxWind            = 0;
bool     wxValid           = false;
uint32_t lastWeatherFetch  = 0;
uint32_t lastClockDraw     = 0;
uint32_t lastWxFetchStart  = 0;
bool     wxFetching        = false;
bool     clockDirty         = true;

// ── Alarms (dynamic, up to 8) ────────────────────────────────
#define MAX_ALARMS 8
struct Alarm {
  uint8_t  hour;
  uint8_t  min;
  bool     enabled;
  uint16_t duration;   // ring duration in seconds (5-300)
};
Alarm alarms[MAX_ALARMS] = {
  {7, 0, false, 30},
  {12, 0, false, 30},
  {0, 0, false, 30}
};
uint8_t  alarmCount      = 3;    // active alarm slots
uint8_t  activeAlarmIdx  = 255;  // which alarm is ringing (255=none)
uint8_t  prevView        = VIEW_CLOCK_DIGITAL;
uint32_t alarmRingStart  = 0;
bool     alarmRinging    = false;

// ── Countdown Timer ──────────────────────────────────────────
struct Countdown {
  uint32_t totalSec;
  uint32_t remaining;
  bool     running;
  uint32_t lastTick;
  uint8_t  buzzDuration;  // seconds
};
Countdown timer = {0, 0, false, 0, 10};

// ── Terminal ──────────────────────────────────────────────────
#define TERM_COLS      15
#define TERM_ROWS       8
#define TERM_CHAR_W    12
#define TERM_CHAR_H    20
#define TERM_PAD_X      8
#define TERM_PAD_Y     18

bool    termMode    = false;
String  termLines[TERM_ROWS];
uint8_t termRow     = 0;
uint8_t termCol     = 0;

// ── Logo animation ───────────────────────────────────────────
#define LOGO_CX 120
#define LOGO_CY 105

#define LOGO_TRI_COUNT 162
static const int16_t LOGO_TRIS[][6] PROGMEM = {
  {120,105,65,134,100,114},{120,105,100,114,101,113},{120,105,101,113,100,112},
  {120,105,100,112,99,112},{120,105,99,112,93,111},{120,105,93,111,73,111},
  {120,105,73,111,55,110},{120,105,55,110,38,109},{120,105,38,109,34,108},
  {120,105,34,108,30,103},{120,105,30,103,30,100},{120,105,30,100,34,98},
  {120,105,34,98,39,98},{120,105,39,98,50,99},{120,105,50,99,67,100},
  {120,105,67,100,80,101},{120,105,80,101,98,103},{120,105,98,103,101,103},
  {120,105,101,103,101,102},{120,105,101,102,100,101},{120,105,100,101,100,100},
  {120,105,100,100,82,88},{120,105,82,88,63,76},{120,105,63,76,53,69},
  {120,105,53,69,48,65},{120,105,48,65,45,61},{120,105,45,61,44,54},
  {120,105,44,54,49,49},{120,105,49,49,55,49},{120,105,55,49,57,49},
  {120,105,57,49,64,55},{120,105,64,55,78,66},{120,105,78,66,96,79},
  {120,105,96,79,99,81},{120,105,99,81,100,81},{120,105,100,81,100,80},
  {120,105,100,80,99,78},{120,105,99,78,89,60},{120,105,89,60,78,41},
  {120,105,78,41,73,34},{120,105,73,34,72,29},{120,105,72,29,72,28},
  {120,105,72,28,72,27},{120,105,72,27,71,26},{120,105,71,26,71,25},
  {120,105,71,25,71,24},{120,105,71,24,77,16},{120,105,77,16,80,15},
  {120,105,80,15,87,16},{120,105,87,16,91,19},{120,105,91,19,95,29},
  {120,105,95,29,103,46},{120,105,103,46,114,68},{120,105,114,68,118,75},
  {120,105,118,75,119,81},{120,105,119,81,120,83},{120,105,120,83,121,83},
  {120,105,121,83,121,82},{120,105,121,82,122,69},{120,105,122,69,124,54},
  {120,105,124,54,126,34},{120,105,126,34,126,28},{120,105,126,28,129,21},
  {120,105,129,21,135,18},{120,105,135,18,139,20},{120,105,139,20,143,25},
  {120,105,143,25,142,28},{120,105,142,28,140,42},{120,105,140,42,136,64},
  {120,105,136,64,133,78},{120,105,133,78,135,78},{120,105,135,78,136,76},
  {120,105,136,76,144,67},{120,105,144,67,156,51},{120,105,156,51,162,45},
  {120,105,162,45,168,38},{120,105,168,38,172,35},{120,105,172,35,180,35},
  {120,105,180,35,185,43},{120,105,185,43,183,52},{120,105,183,52,175,62},
  {120,105,175,62,168,71},{120,105,168,71,159,83},{120,105,159,83,153,94},
  {120,105,153,94,154,94},{120,105,154,94,155,94},{120,105,155,94,176,90},
  {120,105,176,90,188,88},{120,105,188,88,201,85},{120,105,201,85,208,88},
  {120,105,208,88,208,91},{120,105,208,91,206,97},{120,105,206,97,191,101},
  {120,105,191,101,174,104},{120,105,174,104,148,110},{120,105,148,110,148,111},
  {120,105,148,111,148,111},{120,105,148,111,160,112},{120,105,160,112,165,112},
  {120,105,165,112,177,112},{120,105,177,112,200,114},{120,105,200,114,205,118},
  {120,105,205,118,209,123},{120,105,209,123,208,126},{120,105,208,126,199,131},
  {120,105,199,131,187,128},{120,105,187,128,159,121},{120,105,159,121,149,119},
  {120,105,149,119,147,119},{120,105,147,119,147,120},{120,105,147,120,156,128},
  {120,105,156,128,170,141},{120,105,170,141,189,158},{120,105,189,158,190,163},
  {120,105,190,163,188,166},{120,105,188,166,185,166},{120,105,185,166,169,153},
  {120,105,169,153,162,148},{120,105,162,148,148,136},{120,105,148,136,147,136},
  {120,105,147,136,147,137},{120,105,147,137,150,142},{120,105,150,142,168,168},
  {120,105,168,168,169,176},{120,105,169,176,168,179},{120,105,168,179,163,180},
  {120,105,163,180,158,179},{120,105,158,179,148,165},{120,105,148,165,137,149},
  {120,105,137,149,129,134},{120,105,129,134,128,135},{120,105,128,135,123,189},
  {120,105,123,189,120,192},{120,105,120,192,115,194},{120,105,115,194,110,191},
  {120,105,110,191,108,185},{120,105,108,185,110,174},{120,105,110,174,113,160},
  {120,105,113,160,116,148},{120,105,116,148,118,134},{120,105,118,134,119,129},
  {120,105,119,129,119,129},{120,105,119,129,118,129},{120,105,118,129,107,144},
  {120,105,107,144,91,166},{120,105,91,166,78,180},{120,105,78,180,75,181},
  {120,105,75,181,70,178},{120,105,70,178,70,173},{120,105,70,173,73,169},
  {120,105,73,169,91,146},{120,105,91,146,102,132},{120,105,102,132,109,124},
  {120,105,109,124,109,123},{120,105,109,123,108,123},{120,105,108,123,61,153},
  {120,105,61,153,52,155},{120,105,52,155,49,151},{120,105,49,151,49,146},
  {120,105,49,146,51,144},{120,105,51,144,65,134},{120,105,65,134,65,134},
};

#define LOGO_SEG_COUNT 162
static const int16_t LOGO_SEGS[][4] PROGMEM = {
  {65,134,100,114},{100,114,101,113},{101,113,100,112},{100,112,99,112},
  {99,112,93,111},{93,111,73,111},{73,111,55,110},{55,110,38,109},
  {38,109,34,108},{34,108,30,103},{30,103,30,100},{30,100,34,98},
  {34,98,39,98},{39,98,50,99},{50,99,67,100},{67,100,80,101},
  {80,101,98,103},{98,103,101,103},{101,103,101,102},{101,102,100,101},
  {100,101,100,100},{100,100,82,88},{82,88,63,76},{63,76,53,69},
  {53,69,48,65},{48,65,45,61},{45,61,44,54},{44,54,49,49},
  {49,49,55,49},{55,49,57,49},{57,49,64,55},{64,55,78,66},
  {78,66,96,79},{96,79,99,81},{99,81,100,81},{100,81,100,80},
  {100,80,99,78},{99,78,89,60},{89,60,78,41},{78,41,73,34},
  {73,34,72,29},{72,29,72,28},{72,28,72,27},{72,27,71,26},
  {71,26,71,25},{71,25,71,24},{71,24,77,16},{77,16,80,15},
  {80,15,87,16},{87,16,91,19},{91,19,95,29},{95,29,103,46},
  {103,46,114,68},{114,68,118,75},{118,75,119,81},{119,81,120,83},
  {120,83,121,83},{121,83,121,82},{121,82,122,69},{122,69,124,54},
  {124,54,126,34},{126,34,126,28},{126,28,129,21},{129,21,135,18},
  {135,18,139,20},{139,20,143,25},{143,25,142,28},{142,28,140,42},
  {140,42,136,64},{136,64,133,78},{133,78,135,78},{135,78,136,76},
  {136,76,144,67},{144,67,156,51},{156,51,162,45},{162,45,168,38},
  {168,38,172,35},{172,35,180,35},{180,35,185,43},{185,43,183,52},
  {183,52,175,62},{175,62,168,71},{168,71,159,83},{159,83,153,94},
  {153,94,154,94},{154,94,155,94},{155,94,176,90},{176,90,188,88},
  {188,88,201,85},{201,85,208,88},{208,88,208,91},{208,91,206,97},
  {206,97,191,101},{191,101,174,104},{174,104,148,110},{148,110,148,111},
  {148,111,148,111},{148,111,160,112},{160,112,165,112},{165,112,177,112},
  {177,112,200,114},{200,114,205,118},{205,118,209,123},{209,123,208,126},
  {208,126,199,131},{199,131,187,128},{187,128,159,121},{159,121,149,119},
  {149,119,147,119},{147,119,147,120},{147,120,156,128},{156,128,170,141},
  {170,141,189,158},{189,158,190,163},{190,163,188,166},{188,166,185,166},
  {185,166,169,153},{169,153,162,148},{162,148,148,136},{148,136,147,136},
  {147,136,147,137},{147,137,150,142},{150,142,168,168},{168,168,169,176},
  {169,176,168,179},{168,179,163,180},{163,180,158,179},{158,179,148,165},
  {148,165,137,149},{137,149,129,134},{129,134,128,135},{128,135,123,189},
  {123,189,120,192},{120,192,115,194},{115,194,110,191},{110,191,108,185},
  {108,185,110,174},{110,174,113,160},{113,160,116,148},{116,148,118,134},
  {118,134,119,129},{119,129,119,129},{119,129,118,129},{118,129,107,144},
  {107,144,91,166},{91,166,78,180},{78,180,75,181},{75,181,70,178},
  {70,178,70,173},{70,173,73,169},{73,169,91,146},{91,146,102,132},
  {102,132,109,124},{109,124,109,123},{109,123,108,123},{108,123,61,153},
  {61,153,52,155},{52,155,49,151},{49,151,49,146},{49,146,51,144},
  {51,144,65,134},{65,134,65,134},
};

void drawLogoFilled(uint16_t bg, uint16_t fg) {
  tft.fillScreen(bg);
  for (uint16_t i = 0; i < LOGO_TRI_COUNT; i++) {
    tft.fillTriangle(
      pgm_read_word(&LOGO_TRIS[i][0]), pgm_read_word(&LOGO_TRIS[i][1]),
      pgm_read_word(&LOGO_TRIS[i][2]), pgm_read_word(&LOGO_TRIS[i][3]),
      pgm_read_word(&LOGO_TRIS[i][4]), pgm_read_word(&LOGO_TRIS[i][5]),
      fg);
  }
  tft.setTextColor(fg); tft.setTextSize(2);
  tft.setCursor(LOGO_CX - 54, 210); tft.print("Anthropic");
  tft.setCursor(LOGO_CX - 53, 210); tft.print("Anthropic");
}

void animLogoReveal() {
  busy = true;
  tft.fillScreen(animBgColor);
  for (uint16_t i = 0; i < LOGO_SEG_COUNT; i++) {
    int16_t x1 = pgm_read_word(&LOGO_SEGS[i][0]);
    int16_t y1 = pgm_read_word(&LOGO_SEGS[i][1]);
    int16_t x2 = pgm_read_word(&LOGO_SEGS[i][2]);
    int16_t y2 = pgm_read_word(&LOGO_SEGS[i][3]);
    tft.drawLine(x1, y1, x2, y2, C_WHITE);
    tft.drawLine(x1 + 1, y1, x2 + 1, y2, C_WHITE);
    if (i % 4 == 0) { server.handleClient(); safeDelay(speedMs(8)); }
  }
  drawLogoFilled(animBgColor, C_WHITE);
  safeDelay(1500);
  busy = false;
}

// ═════════════════════════════════════════════════════════════
//  HELPERS
// ═════════════════════════════════════════════════════════════

int speedMs(int ms) {
  if (animSpeed == 3) return ms / 2;
  if (animSpeed == 1) return ms * 2;
  return ms;
}

// Non-blocking delay: keeps the web server + WiFi stack alive
void safeDelay(int ms) {
  uint32_t start = millis();
  while (millis() - start < (uint32_t)ms) {
    server.handleClient();
    delay(1);  // yield to FreeRTOS, lets WiFi stack breathe
  }
}

uint16_t hexToRgb565(String hex) {
  hex.replace("#", "");
  if (hex.length() != 6) return C_WHITE;
  long v = strtol(hex.c_str(), nullptr, 16);
  return tft.color565((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

// Print Chinese text at (x, y_baseline).  Caller must clear the bg area first.

// Quick transition flash (50ms black) for smoother view switching
void transitionFlash() {
  tft.fillScreen(C_BLACK);
  delay(50);
}

// Check if a view is an eye-expression (supports idle blink)
bool isEyeView(uint8_t v) {
  return (v >= VIEW_EYES_NORMAL && v <= VIEW_EYES_PUPPY) || v == VIEW_WINK;
}

void setBacklight(bool on) {
  backlightOn = on;
  digitalWrite(TFT_BLK, on ? HIGH : LOW);
}

void initColours() {
  // C_ORANGE = tft.color565(170, 72, 28);
  C_ORANGE = tft.color565(218, 17, 0);
  C_DARKBG = tft.color565(10,  12,  16);
  C_MUTED  = tft.color565(90,  88,  86);
  C_GREEN  = tft.color565(80, 220, 130);
  animBgColor = C_ORANGE;
  drawBgColor = C_ORANGE;
}

// ── Preferences (NVS) ────────────────────────────────────────
// Save + broadcast to WebSocket clients in one call
void notifyChanged() {
  savePreferences();
  wsBroadcastState();
}

void savePreferences() {
  prefs.begin("clawd", false);  // read-write
  prefs.putUChar("spd", animSpeed);
  prefs.putUShort("bg",  animBgColor);
  prefs.putUShort("drbg", drawBgColor);
  prefs.putUChar("bl",  backlightOn ? 1 : 0);
  prefs.putUChar("view", currentView);
  prefs.putUChar("auto", autoSwitch ? 1 : 0);
  prefs.putUChar("aMode", autoSwitchMode);
  prefs.putUInt("aInt", autoSwitchInterval);
  prefs.putUChar("aCnt", alarmCount);
  for (uint8_t i = 0; i < alarmCount; i++) {
    char k[8];
    snprintf(k, sizeof(k), "ah%d", i); prefs.putUChar(k, alarms[i].hour);
    snprintf(k, sizeof(k), "am%d", i); prefs.putUChar(k, alarms[i].min);
    snprintf(k, sizeof(k), "ae%d", i); prefs.putUChar(k, alarms[i].enabled ? 1 : 0);
    snprintf(k, sizeof(k), "ad%d", i); prefs.putUShort(k, alarms[i].duration);
  }
  prefs.putUInt("tTot", timer.totalSec);
  prefs.putUInt("tBuzz", timer.buzzDuration);
  prefs.end();
}

void loadPreferences() {
  prefs.begin("clawd", true);  // read-only
  animSpeed          = prefs.getUChar("spd", animSpeed);
  animBgColor        = prefs.getUShort("bg", animBgColor);
  drawBgColor        = prefs.getUShort("drbg", drawBgColor);
  backlightOn        = prefs.getUChar("bl", 1);
  currentView        = prefs.getUChar("view", currentView);
  autoSwitch         = prefs.getUChar("auto", 0);
  autoSwitchMode     = prefs.getUChar("aMode", 0);
  autoSwitchInterval = prefs.getUInt("aInt", 5000);
  alarmCount         = prefs.getUChar("aCnt", alarmCount);
  if (alarmCount > MAX_ALARMS) alarmCount = MAX_ALARMS;
  for (uint8_t i = 0; i < alarmCount; i++) {
    char k[8];
    snprintf(k, sizeof(k), "ah%d", i); alarms[i].hour     = prefs.getUChar(k, alarms[i].hour);
    snprintf(k, sizeof(k), "am%d", i); alarms[i].min      = prefs.getUChar(k, alarms[i].min);
    snprintf(k, sizeof(k), "ae%d", i); alarms[i].enabled  = prefs.getUChar(k, 0);
    snprintf(k, sizeof(k), "ad%d", i); alarms[i].duration = prefs.getUShort(k, 30);
  }
  timer.totalSec     = prefs.getUInt("tTot", 0);
  timer.remaining    = timer.totalSec;
  timer.buzzDuration = prefs.getUInt("tBuzz", 10);
  prefs.end();
  setBacklight(backlightOn);
}

// ── Weather helpers ──────────────────────────────────────────
uint8_t classifyWxCode(uint8_t raw) {
  if (raw == 0)                     return WX_CLEAR;
  if (raw >= 1  && raw <= 3)       return WX_PARTLY_CLOUDY;
  if (raw == 45 || raw == 48)      return WX_FOG;
  if (raw >= 51 && raw <= 57)      return WX_DRIZZLE;
  if (raw >= 61 && raw <= 67)      return WX_RAIN;
  if (raw >= 71 && raw <= 77)      return WX_SNOW;
  if (raw == 80 || raw == 81 || raw == 82)  return WX_RAIN;   // rain showers
  if (raw == 85 || raw == 86)      return WX_SNOW;   // heavy snow
  if (raw >= 95 && raw <= 99)      return WX_THUNDER;
  return WX_OVERCAST;
}

const char* weatherDesc(uint8_t bucket) {
  switch (bucket) {
    case WX_CLEAR:          return "Sunny";
    case WX_PARTLY_CLOUDY:  return "Cloudy";
    case WX_OVERCAST:       return "Overcast";
    case WX_FOG:            return "Fog";
    case WX_DRIZZLE:        return "Drizzle";
    case WX_RAIN:           return "Rain";
    case WX_SNOW:           return "Snow";
    case WX_THUNDER:        return "Storm";
  }
  return "?";
}

// Returns a single emoji-like character descriptor for the weather
// (plain ASCII since the ST7789 font doesn't have emoji)
const char* weatherIconText(uint8_t bucket) {
  switch (bucket) {
    case WX_CLEAR:          return "*";   // sun-like
    case WX_PARTLY_CLOUDY:  return "o";
    case WX_OVERCAST:       return "=";
    case WX_FOG:            return "~";
    case WX_DRIZZLE:        return ".";
    case WX_RAIN:           return "/";
    case WX_SNOW:           return "s";
    case WX_THUNDER:        return "!";
  }
  return "?";
}

// Resolve location: use MY_LAT/MY_LON if set, else geocode, else IP
void resolveLocation() {
  if (WiFi.status() != WL_CONNECTED) return;

  // ── Direct coordinates (most reliable) ──────────────────────
  if (MY_LAT != 0 && MY_LON != 0) {
    wxLat = MY_LAT; wxLon = MY_LON; wxCity = MY_CITY_NAME;
    char ptz[16];
    snprintf(ptz, sizeof(ptz), "UTC%d", -MY_TZ_OFFSET);
    configTzTime(ptz, NTP_SERVER);
    Serial.println("Location (manual): " + wxCity +
                   " lat=" + String(wxLat,4) + " lon=" + String(wxLon,4));
    return;
  }

  if (strlen(MY_CITY) > 0) {
    // ── Manual mode: geocode the city name → exact coordinates ──
    HTTPClient http;
    http.setTimeout(5000);
    String cityEncoded = String(MY_CITY);
    cityEncoded.replace(" ", "%20");
    String url = "http://geocoding-api.open-meteo.com/v1/search?name="
                 + cityEncoded + "&count=1&language=en";
    http.begin(url);
    int code = http.GET();
    Serial.println("Geo HTTP: " + String(code) + "  city=" + MY_CITY);
    if (code == 200) {
      String payload = http.getString();
      Serial.println("Geo raw: " + payload);
      const char* s = payload.c_str();
      const char* p;
      p = strstr(s, "\"latitude\":");
      if (p) wxLat = atof(p + strlen("\"latitude\":"));
      p = strstr(s, "\"longitude\":");
      if (p) wxLon = atof(p + strlen("\"longitude\":"));
      if (wxLat != 0 && wxLon != 0) wxCity = MY_CITY;
    }
    http.end();
  } else {
    // ── Auto mode: detect from public IP (less accurate for mobile) ──
    HTTPClient http;
    http.setTimeout(5000);
    http.begin("http://ip-api.com/json/");
    int code = http.GET();
    Serial.println("GeoIP HTTP: " + String(code));
    if (code == 200) {
      String payload = http.getString();
      Serial.println("GeoIP raw: " + payload);
      const char* s = payload.c_str();
      const char* p;
      p = strstr(s, "\"lat\":");
      if (p) wxLat = atof(p + strlen("\"lat\":"));
      p = strstr(s, "\"lon\":");
      if (p) wxLon = atof(p + strlen("\"lon\":"));
      p = strstr(s, "\"city\":\"");
      if (p) {
        p += strlen("\"city\":\"");
        String enCity;
        while (*p && *p != '"') enCity += *p++;
        wxCity = enCity;
      }
      p = strstr(s, "\"timezone\":\"");
      if (p) {
        p += strlen("\"timezone\":\"");
        wxTz = "";
        while (*p && *p != '"') wxTz += *p++;
      }
    }
    http.end();
  }

  // ── Timezone via numeric offset (always reliable on ESP32) ──
  // POSIX TZ sign is REVERSED: UTC+8 → "UTC-8"
  char posixTz[16];
  snprintf(posixTz, sizeof(posixTz), "UTC%d", -MY_TZ_OFFSET);
  configTzTime(posixTz, NTP_SERVER);

  Serial.println("Location: " + wxCity +
                 " lat=" + String(wxLat, 4) +
                 " lon=" + String(wxLon, 4) +
                 " tz=" + posixTz);
}

void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;
  wxFetching = true;

  // Drain pending web requests before blocking HTTP call
  for (uint8_t i = 0; i < 8; i++) { server.handleClient(); delay(1); }

  HTTPClient http;
  http.setTimeout(2000);  // reduced timeout — safeDelay handles clients
  char url[256];
  snprintf(url, sizeof(url),
    "http://api.open-meteo.com/v1/forecast"
    "?latitude=%.2f&longitude=%.2f"
    "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
    "&timezone=auto",
    wxLat, wxLon);
  http.begin(url);
  Serial.println("Wx URL: " + String(url));
  int code = http.GET();
  Serial.println("Wx HTTP: " + String(code));
  if (code == 200) {
    String payload = http.getString();
    // Parse: keys appear in BOTH "current_units" (unit strings)
    // and "current" (actual values).  Search inside "current":{...} only.
    const char* s = payload.c_str();

    const char* cur = strstr(s, "\"current\":{");
    if (!cur) { http.end(); wxFetching = false; return; }
    cur += strlen("\"current\":{");  // now points into the "current" object

    // Helper: find key inside "current" block only
    auto parseVal = [&](const char* key) -> const char* {
      const char* found = strstr(cur, key);
      if (!found) return nullptr;
      return found + strlen(key);  // skip past "key":
    };

    const char* p;
    p = parseVal("\"temperature_2m\":");
    if (p) wxTemp = atof(p);

    p = parseVal("\"relative_humidity_2m\":");
    if (p) wxHumidity = (uint8_t)atoi(p);

    p = parseVal("\"wind_speed_10m\":");
    if (p) wxWind = atof(p);

    p = parseVal("\"weather_code\":");
    if (p) wxCode = classifyWxCode((uint8_t)atoi(p));

    wxValid = true;
    Serial.println("Weather OK: " + String(wxTemp) + "C " +
                   String(wxHumidity) + "% code=" + String(wxCode));
  } else {
    Serial.println("Weather HTTP err: " + String(code));
  }
  http.end();
  wxFetching = false;
  lastWeatherFetch = millis();

  // Drain any requests that queued up during the blocking HTTP call
  for (uint8_t i = 0; i < 8; i++) { server.handleClient(); delay(1); }
}

void syncTimeIfNeeded() {
  if (timeSynced) return;
  if (WiFi.status() != WL_CONNECTED) return;
  struct tm ti;
  if (getLocalTime(&ti, 5000)) {
    timeSynced = true;
    Serial.printf("NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                  ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                  ti.tm_hour, ti.tm_min, ti.tm_sec);
  }
}

const char* weekdayNameCN(uint8_t wday) {
  switch (wday) {
    case 0: return "Sun"; case 1: return "Mon";
    case 2: return "Tue"; case 3: return "Wed";
    case 4: return "Thu"; case 5: return "Fri";
    case 6: return "Sat";
  }
  return "";
}

// ═════════════════════════════════════════════════════════════
//  LOGO
// ═════════════════════════════════════════════════════════════

// ═════════════════════════════════════════════════════════════
//  VIEWS
// ═════════════════════════════════════════════════════════════

// Eye helpers — shared constants via #define EYE_*
inline int16_t eyeLX(int16_t ox) {
  return (DISP_W - (EYE_W * 2 + EYE_GAP)) / 2 + EYE_OX + ox;
}
inline int16_t eyeRX(int16_t ox) { return eyeLX(ox) + EYE_W + EYE_GAP; }
inline int16_t eyeY()            { return (DISP_H - EYE_H) / 2 - EYE_OY; }
inline int16_t eyeCY()           { return eyeY() + EYE_H / 2; }

void drawNormalEyes(int16_t ox = 0, bool blink = false) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(ox), rx = eyeRX(ox), ey = eyeY();
  if (!blink) {
    tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
    tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
    // Cute highlight dots
    tft.fillCircle(lx + EYE_W/2 - 6, ey + EYE_H/2 - 8, 3, C_WHITE);
    tft.fillCircle(rx + EYE_W/2 - 6, ey + EYE_H/2 - 8, 3, C_WHITE);
  } else {
    tft.fillRect(lx, ey + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
    tft.fillRect(rx, ey + EYE_H / 2 - 3, EYE_W, 6, C_BLACK);
  }
}

void drawSquishEyes(bool closed = false) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  if (!closed) {
    // White curved chevron smile inside black rect
    const int16_t arm   = 10;
    const int16_t reach = 12;
    const int16_t lcx   = lx + EYE_W / 2;
    const int16_t rcx   = rx + EYE_W / 2;
    for (int8_t t = -4; t <= 4; t++) {
      tft.drawLine(lcx - reach/2, cy - arm + t, lcx + reach/2, cy + t,      C_WHITE);
      tft.drawLine(lcx + reach/2, cy + t,       lcx - reach/2, cy + arm + t, C_WHITE);
      tft.drawLine(rcx + reach/2, cy - arm + t, rcx - reach/2, cy + t,      C_WHITE);
      tft.drawLine(rcx - reach/2, cy + t,       rcx + reach/2, cy + arm + t, C_WHITE);
    }
  } else {
    tft.fillRect(lx, cy - 5, EYE_W, 10, C_WHITE);
    tft.fillRect(rx, cy - 5, EYE_W, 10, C_WHITE);
  }
}

// ── Angry eyes — slanted inward, narrowed ────────────────────────
void drawAngryEyes() {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  // Black rect base
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  // Angry brow (orange, tilting down toward center)
  for (int8_t t = 0; t < 4; t++) {
    tft.drawLine(lx - 4, ey - 8 + t, lx + EYE_W + 4, ey + 4 + t, C_ORANGE);
    tft.drawLine(rx + EYE_W + 4, ey - 8 + t, rx - 4, ey + 4 + t, C_ORANGE);
  }
  // Narrowed white pupil slits
  tft.fillRect(lx + 4, cy - 3, EYE_W - 8, 6, C_WHITE);
  tft.fillRect(rx + 4, cy - 3, EYE_W - 8, 6, C_WHITE);
}

// ── Heart eyes ───────────────────────────────────────────────────
void drawHeartEyes() {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  // Draw white eye backgrounds
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  // Heart in left eye
  const int16_t lcx = lx + EYE_W / 2;
  const int16_t rcx = rx + EYE_W / 2;
  const int16_t hcy = eyeCY();
  // Small heart using filled circles + triangle
  const int16_t hs = 8;  // heart size
  tft.fillCircle(lcx - hs/2, hcy - hs/4, hs/2, C_ORANGE);
  tft.fillCircle(lcx + hs/2, hcy - hs/4, hs/2, C_ORANGE);
  tft.fillTriangle(lcx - hs, hcy - hs/4 + 1, lcx + hs, hcy - hs/4 + 1,
                   lcx, hcy + hs, C_ORANGE);
  // Heart in right eye
  tft.fillCircle(rcx - hs/2, hcy - hs/4, hs/2, C_ORANGE);
  tft.fillCircle(rcx + hs/2, hcy - hs/4, hs/2, C_ORANGE);
  tft.fillTriangle(rcx - hs, hcy - hs/4 + 1, rcx + hs, hcy - hs/4 + 1,
                   rcx, hcy + hs, C_ORANGE);
}

// ── Dizzy eyes — X pattern ──────────────────────────────────────
void drawDizzyEyes(bool spiral = false) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  const int16_t lcx = lx + EYE_W / 2;
  const int16_t rcx = rx + EYE_W / 2;
  const int16_t cy = eyeCY();
  if (!spiral) {
    // X eyes
    const int16_t xs = 10;
    for (int8_t t = -2; t <= 2; t++) {
      tft.drawLine(lcx - xs, cy - xs + t, lcx + xs, cy + xs + t, C_WHITE);
      tft.drawLine(lcx - xs, cy + xs + t, lcx + xs, cy - xs + t, C_WHITE);
      tft.drawLine(rcx - xs, cy - xs + t, rcx + xs, cy + xs + t, C_WHITE);
      tft.drawLine(rcx - xs, cy + xs + t, rcx + xs, cy - xs + t, C_WHITE);
    }
  } else {
    // Spiral eyes — concentric arcs
    for (int8_t r = 3; r <= 14; r += 3) {
      tft.drawCircle(lcx, cy, r, C_WHITE);
      tft.drawCircle(rcx, cy, r, C_WHITE);
    }
  }
}

// ── Sleepy eyes — half‑closed horizontal slits ───────────────────
void drawSleepyEyes() {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  // Thin white slits (half-closed)
  tft.fillRect(lx + 2, cy + 4, EYE_W - 4, 4, C_WHITE);
  tft.fillRect(rx + 2, cy + 4, EYE_W - 4, 4, C_WHITE);
  // Droopy upper lid — uses bg color to cut into black rect from top
  tft.fillTriangle(lx, ey, lx + EYE_W, ey, lx + EYE_W / 2, ey + 16, animBgColor);
  tft.fillTriangle(rx, ey, rx + EYE_W, ey, rx + EYE_W / 2, ey + 16, animBgColor);
  // Redraw slits over the droop
  tft.fillRect(lx + 2, cy + 4, EYE_W - 4, 4, C_WHITE);
  tft.fillRect(rx + 2, cy + 4, EYE_W - 4, 4, C_WHITE);
}

// ── Surprised eyes — wide circles, small pupils ──────────────────
void drawSurprisedEyes(bool extraWide = false) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  // Black rect base
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  // Large white circles inside the rect
  const int16_t rad = extraWide ? 13 : 10;
  tft.fillCircle(lx + EYE_W/2, cy, rad, C_WHITE);
  tft.fillCircle(rx + EYE_W/2, cy, rad, C_WHITE);
  if (!extraWide) {
    // Small black pupils in center
    tft.fillCircle(lx + EYE_W/2, cy, 3, C_BLACK);
    tft.fillCircle(rx + EYE_W/2, cy, 3, C_BLACK);
  }
}

// ── Wink — one normal eye, one blink ─────────────────────────────
void drawWink(bool leftWink = true) {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  if (leftWink) {
    // Left eye closed (blink line), right eye normal
    tft.fillRect(lx, cy - 3, EYE_W, 6, C_BLACK);
    tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  } else {
    tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
    tft.fillRect(rx, cy - 3, EYE_W, 6, C_BLACK);
  }
}

// ── Cry eyes — sad with tear drops ───────────────────────────
void drawCryEyes() {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  // Same base as normal: black rects
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  // Small white pupils at top (looking up pleading)
  tft.fillCircle(lx + EYE_W/2, cy - 6, 4, C_WHITE);
  tft.fillCircle(rx + EYE_W/2, cy - 6, 4, C_WHITE);
  // Tear drops (blue) falling from bottom
  uint16_t tear = tft.color565(100, 180, 255);
  tft.fillCircle(lx + EYE_W/2 - 4, ey + EYE_H + 2, 3, tear);
  tft.fillCircle(rx + EYE_W/2 - 4, ey + EYE_H + 2, 3, tear);
  tft.fillCircle(lx + EYE_W/2 - 6, ey + EYE_H + 8, 2, tear);
  tft.fillCircle(rx + EYE_W/2 - 6, ey + EYE_H + 8, 2, tear);
}

// ── Star eyes — sparkly excited ──────────────────────────────
void drawStarEyes() {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  // Simple plus-shaped sparkle inside each eye
  for (int8_t t = -2; t <= 2; t++) {
    tft.drawFastHLine(lx + EYE_W/2 - 6, cy + t, 12, C_ORANGE);
    tft.drawFastVLine(lx + EYE_W/2 + t, cy - 6, 12, C_ORANGE);
    tft.drawFastHLine(rx + EYE_W/2 - 6, cy + t, 12, C_ORANGE);
    tft.drawFastVLine(rx + EYE_W/2 + t, cy - 6, 12, C_ORANGE);
  }
  // Small white dot in center
  tft.fillCircle(lx + EYE_W/2, cy, 3, C_WHITE);
  tft.fillCircle(rx + EYE_W/2, cy, 3, C_WHITE);
}

// ── Shy eyes — looking aside with blush ──────────────────────
void drawShyEyes() {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  // White slit pupils shifted down and to the side
  tft.fillRect(lx + 6, cy + 2, EYE_W - 12, 5, C_WHITE);
  tft.fillRect(rx + 6, cy + 2, EYE_W - 12, 5, C_WHITE);
  // Blush marks: small squares below each eye
  uint16_t blush = tft.color565(255, 120, 150);
  tft.fillRect(lx + 2, ey + EYE_H + 2, 6, 6, blush);
  tft.fillRect(rx + EYE_W - 8, ey + EYE_H + 2, 6, 6, blush);
}

// ── Puppy eyes — big shiny pleading ──────────────────────────
void drawPuppyEyes() {
  tft.fillScreen(animBgColor);
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  // Same black rect base, but with large white oval pupils
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  // Large round white pupils (bigger than normal, filling eye)
  tft.fillCircle(lx + EYE_W/2, cy, 11, C_WHITE);
  tft.fillCircle(rx + EYE_W/2, cy, 11, C_WHITE);
  // Small dark center
  tft.fillCircle(lx + EYE_W/2 + 1, cy + 2, 4, C_BLACK);
  tft.fillCircle(rx + EYE_W/2 + 1, cy + 2, 4, C_BLACK);
  // Catch light highlight
  tft.fillCircle(lx + EYE_W/2 - 4, cy - 5, 3, C_WHITE);
  tft.fillCircle(rx + EYE_W/2 - 4, cy - 5, 3, C_WHITE);
}

// ── Dance (from b4.gif) ──────────────────────────────────────
void drawDance() {
  // Frame data is drawn in animDance
}

// ── Digital clock ────────────────────────────────────────────
void drawClockDigital() {
  if (currentView != VIEW_CLOCK_DIGITAL) return;
  static int8_t lastS = -1;
  time_t now; struct tm ti;
  time(&now); localtime_r(&now, &ti);

  if (clockDirty) {
    tft.fillScreen(C_DARKBG);
    tft.fillRect(0, 0, DISP_W, 1, C_ORANGE);

    char buf[20];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             ti.tm_hour, ti.tm_min, ti.tm_sec);
    tft.setTextColor(C_WHITE); tft.setTextSize(4);
    int16_t tw = 8 * 24;
    tft.setCursor((DISP_W - tw) / 2, 28);
    tft.print(buf);

    if (wxValid) {
      char wxbuf[32];
      snprintf(wxbuf, sizeof(wxbuf), "%s  %.0fC", weatherDesc(wxCode), wxTemp);
      tft.setTextColor(C_MUTED); tft.setTextSize(1);
      int16_t ww = strlen(wxbuf) * 6;
      tft.setCursor((DISP_W - ww) / 2, 68);
      tft.print(wxbuf);
    }

    tft.fillRect(50, 90, DISP_W - 100, 1, C_MUTED);

    tft.setTextColor(C_WHITE); tft.setTextSize(2);
    snprintf(dateStr, sizeof(dateStr), "%04d/%02d/%02d  %s",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
             weekdayNameCN(ti.tm_wday));
    int16_t dw = strlen(dateStr) * 12;
    tft.setCursor((DISP_W - dw) / 2, 108);
    tft.print(dateStr);

    tft.setTextColor(C_MUTED); tft.setTextSize(1);
    tft.setCursor(16, 150);
    tft.print(wxCity);

    tft.fillRect(60, 172, DISP_W - 120, 1, C_ORANGE);

    lastS = -1;
    clockDirty = false;
  }

  if (ti.tm_sec != lastS) {
    char buf[20];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             ti.tm_hour, ti.tm_min, ti.tm_sec);
    tft.fillRect((DISP_W - 192) / 2, 26, 200, 36, C_DARKBG);
    tft.setTextColor(C_WHITE); tft.setTextSize(4);
    tft.setCursor((DISP_W - 192) / 2, 28);
    tft.print(buf);
    lastS = ti.tm_sec;
  }
}

// ── Analog clock face ─────────────────────────────────────────
void drawClockAnalog() {
  if (currentView != VIEW_CLOCK_ANALOG) return;  // only draw on clock page
  static int16_t phx, phy, pmx, pmy, psx, psy;
  time_t now; struct tm ti;
  time(&now); localtime_r(&now, &ti);
  const int16_t cx = 120, cy = 108, r = 105;

  if (clockDirty) {
    tft.fillScreen(C_DARKBG);
    tft.fillRect(0, 0, DISP_W, 1, C_ORANGE);
    tft.drawCircle(cx, cy, r, C_MUTED);
    for (int8_t i = 0; i < 60; i++) {
      float a = (i - 15) * PI / 30.0;
      int16_t len = (i % 15 == 0) ? 16 : (i % 5 == 0) ? 10 : 4;
      int16_t thick = (i % 5 == 0) ? 2 : 1;
      uint16_t col = (i % 5 == 0) ? C_WHITE : C_MUTED;
      for (int8_t t = 0; t < thick; t++)
        tft.drawLine(cx+cos(a)*(r-len), cy+sin(a)*(r-len)+t,
                     cx+cos(a)*r, cy+sin(a)*r+t, col);
    }
    snprintf(dateStr, sizeof(dateStr), "%d/%02d/%02d  %s",
             ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
             weekdayNameCN(ti.tm_wday));
    tft.setTextColor(C_MUTED); tft.setTextSize(1);
    int16_t dw = strlen(dateStr) * 6;
    tft.setCursor((DISP_W - dw) / 2, 226);
    tft.print(dateStr);
    phx = phy = pmx = pmy = psx = psy = 0;
    clockDirty = false;
  }

  float ha = (ti.tm_hour % 12 + ti.tm_min / 60.0) * 30.0 - 90;
  float ma = (ti.tm_min + ti.tm_sec / 60.0) * 6.0 - 90;
  float sa = ti.tm_sec * 6.0 - 90;
  int16_t hx = cx + cos(ha*PI/180)*(r-52), hy = cy + sin(ha*PI/180)*(r-52);
  int16_t mx = cx + cos(ma*PI/180)*(r-26), my = cy + sin(ma*PI/180)*(r-26);
  int16_t sx = cx + cos(sa*PI/180)*(r-14), sy = cy + sin(sa*PI/180)*(r-14);

  // Erase old hands with THICK lines (no flicker, no ghost)
  if (phx) {
    for (int8_t t = -2; t <= 2; t++) {
      tft.drawLine(cx+t, cy, phx+t, phy, C_DARKBG);  // hour erase
      tft.drawLine(cx+t, cy, pmx+t, pmy, C_DARKBG);  // minute erase
    }
    for (int8_t t = -1; t <= 1; t++) {
      tft.drawLine(cx+t, cy, psx+t, psy, C_DARKBG);  // second erase
    }
    tft.fillCircle(cx, cy, 6, C_DARKBG);  // center erase

    // Redraw tick marks that may have been erased by hand erasure.
    // Without this, tick marks under old hand positions vanish permanently,
    // leaving "missing teeth" on the dial over time.
    for (int8_t i = 0; i < 60; i++) {
      float a = (i - 15) * PI / 30.0;
      int16_t len = (i % 15 == 0) ? 16 : (i % 5 == 0) ? 10 : 4;
      int16_t thick = (i % 5 == 0) ? 2 : 1;
      uint16_t col = (i % 5 == 0) ? C_WHITE : C_MUTED;
      for (int8_t t = 0; t < thick; t++)
        tft.drawLine(cx+cos(a)*(r-len), cy+sin(a)*(r-len)+t,
                     cx+cos(a)*r, cy+sin(a)*r+t, col);
    }
    // Redraw outline circle where hand tips may have nicked the rim
    tft.drawCircle(cx, cy, r, C_MUTED);
  }

  // Draw new hands
  for (int8_t t = -1; t <= 1; t++) {
    tft.drawLine(cx+t, cy, hx+t, hy, C_WHITE);
    tft.drawLine(cx+t, cy, mx+t, my, C_WHITE);
  }
  tft.drawLine(cx, cy, sx, sy, C_ORANGE);
  tft.fillCircle(cx, cy, 2, C_WHITE);
  tft.fillCircle(cx, cy, 1, C_ORANGE);

  phx = hx; phy = hy; pmx = mx; pmy = my; psx = sx; psy = sy;
}

// ── Weather view ──────────────────────────────────────────────
void drawWeather() {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 1, C_ORANGE);

  if (!wxValid) {
    tft.setTextColor(C_MUTED); tft.setTextSize(2);
    tft.setCursor(40, DISP_H/2-12); tft.print("No weather data");
    return;
  }

  tft.setTextColor(C_WHITE); tft.setTextSize(7);
  String ts = String(wxTemp,0);
  int16_t tw = ts.length() * 42;
  tft.setCursor((DISP_W - tw) / 2, 24);
  tft.print(ts);
  tft.setTextSize(3);
  tft.setCursor((DISP_W - tw) / 2 + tw + 4, 32);
  tft.print("C");

  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  const char* desc = weatherDesc(wxCode);
  tw = strlen(desc) * 12;
  tft.setCursor((DISP_W - tw) / 2, 80);
  tft.print(desc);

  tft.fillRect(40, 104, DISP_W - 80, 1, C_MUTED);

  int16_t cy = 120;
  tft.fillRect(24, cy, DISP_W - 48, 70, C_ORANGE);
  tft.fillRect(25, cy + 1, DISP_W - 50, 68, C_DARKBG);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(38, cy + 14); tft.print("Humidity");
  tft.setCursor(160, cy + 14); tft.print(String(wxHumidity) + "%");
  tft.setCursor(38, cy + 44); tft.print("Wind");
  tft.setCursor(160, cy + 44); tft.print(String(wxWind,1));

  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(16, 214); tft.print(wxCity);
  time_t n; struct tm t;
  time(&n); localtime_r(&n, &t);
  snprintf(timeStr, sizeof(timeStr), "Upd %02d:%02d", t.tm_hour, t.tm_min);
  tw = strlen(timeStr) * 6;
  tft.setCursor(DISP_W - tw - 12, 214);
  tft.print(timeStr);
}

void drawCodeView() {
  termMode = false;
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0,          DISP_W, 4, C_ORANGE);
  tft.fillRect(0, DISP_H - 4, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_ORANGE); tft.setTextSize(4);
  tft.setCursor((DISP_W - 144) / 2, DISP_H / 2 - 52); tft.print("Claude");
  tft.setTextColor(C_WHITE);  tft.setTextSize(4);
  tft.setCursor((DISP_W - 96) / 2,  DISP_H / 2 + 8);  tft.print("Code");
  tft.fillRect((DISP_W - 96) / 2, DISP_H / 2 + 52, 96, 3, C_ORANGE);
}

// ── Timer view ───────────────────────────────────────────────
void drawTimer() {
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);

  uint32_t sec = timer.remaining;
  uint8_t h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;

  tft.setTextColor(C_WHITE);
  char buf[16];
  if (h > 0) {
    tft.setTextSize(5);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
  } else {
    tft.setTextSize(6);
    snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
  }
  int16_t tw = strlen(buf) * (h > 0 ? 30 : 36);
  tft.setCursor((DISP_W - tw) / 2, 60);
  tft.print(buf);

  // Progress bar
  if (timer.totalSec > 0) {
    int16_t barW = DISP_W - 40, barH = 12, barX = 20, barY = 140;
    float pct = (float)timer.remaining / timer.totalSec;
    tft.drawRect(barX, barY, barW, barH, C_MUTED);
    tft.fillRect(barX + 2, barY + 2, (barW - 4) * pct, barH - 4, C_ORANGE);
    tft.setTextColor(C_MUTED); tft.setTextSize(1);
    tft.setCursor(barX, barY + 20);
    tft.print(String((int)(pct * 100)) + "%");
  }

  // Status
  tft.setTextColor(C_MUTED); tft.setTextSize(2);
  const char* st = timer.running ? "Running" : (timer.remaining > 0 ? "Paused" : "Done");
  tw = strlen(st) * 12;
  tft.setCursor((DISP_W - tw) / 2, 180);
  tft.print(st);

  tft.fillRect(40, 200, DISP_W - 80, 1, C_MUTED);
  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(40, 214);
  tft.print("Buzz: " + String(timer.buzzDuration) + "s");
}

// ── Alarm view (popup style — white bg, black text) ──────────
void drawAlarm() {
  tft.fillScreen(C_WHITE);

  uint8_t idx = activeAlarmIdx < MAX_ALARMS ? activeAlarmIdx : 0;
  tft.setTextColor(C_BLACK); tft.setTextSize(2);
  tft.setCursor(50, 30); tft.print("Alarm #");
  tft.print(idx + 1);

  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", alarms[idx].hour, alarms[idx].min);
  tft.setTextColor(C_BLACK); tft.setTextSize(6);
  tft.setCursor(42, 64);
  tft.print(buf);

  tft.setTextColor(C_ORANGE); tft.setTextSize(2);
  tft.setCursor(60, 140);
  tft.print("RINGING");

  tft.fillRect(40, 168, DISP_W - 80, 1, C_BLACK);

  tft.setTextColor(C_MUTED); tft.setTextSize(1);
  tft.setCursor(32, 188); tft.print("Dismiss from web controller");
  tft.setCursor(50, 212); tft.print(WiFi.softAPIP().toString().c_str());
}

// ═════════════════════════════════════════════════════════════
//  TERMINAL
// ═════════════════════════════════════════════════════════════

void termClear() {
  for (uint8_t i = 0; i < TERM_ROWS; i++) termLines[i] = "";
  termRow = 0; termCol = 0;
}

void termDrawHeader() {
  tft.fillRect(0, 0, DISP_W, TERM_PAD_Y + 1, C_DARKBG);
  tft.setTextColor(C_ORANGE); tft.setTextSize(1);
  tft.setCursor(TERM_PAD_X, 4); tft.print("clawd@mochi term");
  tft.drawFastHLine(0, TERM_PAD_Y, DISP_W, C_ORANGE);
}

void termDrawPrefix(int16_t yy) {
  tft.setTextColor(C_GREEN); tft.setTextSize(1);
  tft.setCursor(TERM_PAD_X, yy + 6);
  tft.print("clawd:~$ ");
}

#define PREFIX_PX 54

void termDrawLine(uint8_t r) {
  const int16_t yy = TERM_PAD_Y + 4 + r * TERM_CHAR_H;
  tft.fillRect(0, yy, DISP_W, TERM_CHAR_H, C_DARKBG);
  if (r == termRow) termDrawPrefix(yy);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(TERM_PAD_X + PREFIX_PX, yy + 1);
  tft.print(termLines[r]);
  if (r == termRow) {
    const int16_t cx = TERM_PAD_X + PREFIX_PX + termCol * TERM_CHAR_W;
    tft.fillRect(cx, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
  }
}

void termDrawLastChar() {
  if (termCol == 0) return;
  const int16_t yy    = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
  const int16_t baseX = TERM_PAD_X + PREFIX_PX;
  const uint8_t prev  = termCol - 1;
  tft.fillRect(baseX + prev * TERM_CHAR_W, yy + 1, TERM_CHAR_W, TERM_CHAR_H - 1, C_DARKBG);
  tft.setTextColor(C_WHITE); tft.setTextSize(2);
  tft.setCursor(baseX + prev * TERM_CHAR_W, yy + 1);
  tft.print(termLines[termRow][prev]);
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
}

void termDrawBackspace() {
  const int16_t yy    = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
  const int16_t baseX = TERM_PAD_X + PREFIX_PX;
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W * 2, TERM_CHAR_H - 1, C_DARKBG);
  tft.fillRect(baseX + termCol * TERM_CHAR_W, yy + 1, TERM_CHAR_W - 2, TERM_CHAR_H - 2, C_GREEN);
  if (termLines[termRow].length() == 0) {
    tft.fillRect(0, yy, TERM_PAD_X + PREFIX_PX, TERM_CHAR_H, C_DARKBG);
  }
}

void termFullRedraw() {
  tft.fillScreen(C_DARKBG);
  termDrawHeader();
  for (uint8_t r = 0; r < TERM_ROWS; r++) termDrawLine(r);
}

void termScroll() {
  for (uint8_t i = 0; i < TERM_ROWS - 1; i++) termLines[i] = termLines[i + 1];
  termLines[TERM_ROWS - 1] = "";
  termRow = TERM_ROWS - 1;
  termFullRedraw();
}

void termAddChar(char c) {
  if (c == '\n' || c == '\r') {
    const int16_t yy = TERM_PAD_Y + 4 + termRow * TERM_CHAR_H;
    tft.fillRect(TERM_PAD_X + PREFIX_PX + termCol * TERM_CHAR_W,
                 yy + 1, TERM_CHAR_W, TERM_CHAR_H - 1, C_DARKBG);
    termRow++; termCol = 0;
    if (termRow >= TERM_ROWS) { termScroll(); return; }
    termDrawLine(termRow);
  } else if (c == '\b' || c == 127) {
    if (termCol > 0) {
      termCol--;
      termLines[termRow].remove(termLines[termRow].length() - 1);
      termDrawBackspace();
    }
  } else if (c >= 32 && c < 127) {
    if (termCol >= TERM_COLS) {
      termRow++; termCol = 0;
      if (termRow >= TERM_ROWS) { termScroll(); return; }
    }
    if (termCol == 0) termDrawPrefix(TERM_PAD_Y + 4 + termRow * TERM_CHAR_H);
    termLines[termRow] += c;
    termCol++;
    termDrawLastChar();
  }
}

// ═════════════════════════════════════════════════════════════
//  ANIMATIONS
// ═════════════════════════════════════════════════════════════

void animNormalEyes() {
  busy = true;
  const int16_t offs[] = {-16, 16, -16, 16, 0};
  for (uint8_t i = 0; i < 5; i++) { drawNormalEyes(offs[i]); safeDelay(speedMs(80)); }
  drawNormalEyes(0, true);  safeDelay(speedMs(100));
  drawNormalEyes(0, false); safeDelay(speedMs(70));
  drawNormalEyes(0, true);  safeDelay(speedMs(70));
  drawNormalEyes(0, false);
  busy = false;
}

void animSquishEyes() {
  busy = true;
  for (uint8_t i = 0; i < 3; i++) {
    drawSquishEyes(false); safeDelay(speedMs(160));
    drawSquishEyes(true);  safeDelay(speedMs(100));
  }
  drawSquishEyes(false);
  busy = false;
}

void animAngryEyes() {
  busy = true;
  drawAngryEyes();
  for (uint8_t i = 0; i < 5; i++) {
    // vibrate effect — slight vertical shake
    const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
    tft.fillRect(lx, ey, EYE_W, EYE_H, animBgColor);
    tft.fillRect(rx, ey, EYE_W, EYE_H, animBgColor);
    safeDelay(speedMs(40));
    drawAngryEyes();
    safeDelay(speedMs(40));
  }
  busy = false;
}

void animHeartEyes() {
  busy = true;
  // Hearts pulse — grow and shrink
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  for (uint8_t p = 0; p < 3; p++) {
    drawHeartEyes(); safeDelay(speedMs(200));
    tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
    tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
    // draw slightly smaller hearts
    const int16_t lcx = lx + EYE_W / 2, rcx = rx + EYE_W / 2;
    const int16_t hcy = eyeCY(), hs = 5;
    tft.fillCircle(lcx - hs/2, hcy - hs/4, hs/2, C_ORANGE);
    tft.fillCircle(lcx + hs/2, hcy - hs/4, hs/2, C_ORANGE);
    tft.fillTriangle(lcx - hs, hcy - hs/4 + 1, lcx + hs, hcy - hs/4 + 1,
                     lcx, hcy + hs, C_ORANGE);
    tft.fillCircle(rcx - hs/2, hcy - hs/4, hs/2, C_ORANGE);
    tft.fillCircle(rcx + hs/2, hcy - hs/4, hs/2, C_ORANGE);
    tft.fillTriangle(rcx - hs, hcy - hs/4 + 1, rcx + hs, hcy - hs/4 + 1,
                     rcx, hcy + hs, C_ORANGE);
    safeDelay(speedMs(200));
  }
  drawHeartEyes();
  busy = false;
}

void animDizzyEyes() {
  busy = true;
  // Alternate X and spiral
  drawDizzyEyes(false); safeDelay(speedMs(400));
  drawDizzyEyes(true);  safeDelay(speedMs(400));
  drawDizzyEyes(false); safeDelay(speedMs(400));
  drawDizzyEyes(true);  safeDelay(speedMs(400));
  drawDizzyEyes(false);
  busy = false;
}

void animSleepyEyes() {
  busy = true;
  drawSleepyEyes(); safeDelay(speedMs(500));
  // Almost fully closed — thinner slit
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
  tft.fillScreen(animBgColor);
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(lx + 6, eyeCY(), EYE_W - 12, 2, C_WHITE);
  tft.fillRect(rx + 6, eyeCY(), EYE_W - 12, 2, C_WHITE);
  safeDelay(speedMs(600));
  drawSleepyEyes(); safeDelay(speedMs(400));
  busy = false;
}

void animSurprisedEyes() {
  busy = true;
  drawSurprisedEyes(false); safeDelay(speedMs(250));
  drawSurprisedEyes(true);  safeDelay(speedMs(200));
  drawSurprisedEyes(false); safeDelay(speedMs(250));
  drawSurprisedEyes(true);  safeDelay(speedMs(200));
  drawSurprisedEyes(false);
  busy = false;
}

void animWink() {
  busy = true;
  drawWink(true);  safeDelay(speedMs(200));
  drawWink(false); safeDelay(speedMs(200));
  drawWink(true);  safeDelay(speedMs(200));
  drawWink(false); safeDelay(speedMs(200));
  drawWink(true);  safeDelay(speedMs(300));
  drawWink(true);  // end with wink face
  busy = false;
}

void animCryEyes() {
  busy = true;
  for (uint8_t i = 0; i < 3; i++) {
    drawCryEyes(); safeDelay(speedMs(350));
    // Brief blink
    const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY();
    tft.fillRect(lx, ey + EYE_H/2 - 3, EYE_W, 8, animBgColor);
    tft.fillRect(rx, ey + EYE_H/2 - 3, EYE_W, 8, animBgColor);
    safeDelay(speedMs(80));
    drawCryEyes(); safeDelay(speedMs(200));
  }
  busy = false;
}

void animStarEyes() {
  busy = true;
  drawStarEyes(); safeDelay(speedMs(300));
  // Quick brightness flash
  tft.fillRect(eyeLX(0), eyeY(), EYE_W, EYE_H, C_ORANGE);
  tft.fillRect(eyeRX(0), eyeY(), EYE_W, EYE_H, C_ORANGE);
  safeDelay(speedMs(80));
  drawStarEyes(); safeDelay(speedMs(400));
  tft.fillRect(eyeLX(0), eyeY(), EYE_W, EYE_H, C_ORANGE);
  tft.fillRect(eyeRX(0), eyeY(), EYE_W, EYE_H, C_ORANGE);
  safeDelay(speedMs(80));
  drawStarEyes();
  busy = false;
}

void animShyEyes() {
  busy = true;
  drawShyEyes(); safeDelay(speedMs(500));
  // Quick glance up
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(lx + 6, cy - 4, EYE_W - 12, 5, C_WHITE);
  tft.fillRect(rx + 6, cy - 4, EYE_W - 12, 5, C_WHITE);
  safeDelay(speedMs(180));
  drawShyEyes(); safeDelay(speedMs(400));
  busy = false;
}

void animPuppyEyes() {
  busy = true;
  drawPuppyEyes(); safeDelay(speedMs(350));
  // Pupils shift right
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  tft.fillRect(lx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillRect(rx, ey, EYE_W, EYE_H, C_BLACK);
  tft.fillCircle(lx + EYE_W/2, cy, 11, C_WHITE);
  tft.fillCircle(rx + EYE_W/2, cy, 11, C_WHITE);
  tft.fillCircle(lx + EYE_W/2 + 4, cy + 2, 4, C_BLACK);
  tft.fillCircle(rx + EYE_W/2 + 4, cy + 2, 4, C_BLACK);
  tft.fillCircle(lx + EYE_W/2 - 1, cy - 5, 3, C_WHITE);
  tft.fillCircle(rx + EYE_W/2 - 1, cy - 5, 3, C_WHITE);
  safeDelay(speedMs(300));
  drawPuppyEyes(); safeDelay(speedMs(300));
  busy = false;
}

static uint16_t danceBuf[220*224];  // full screen buffer

void animDance() {
  busy = true;
  tft.fillScreen(C_BLACK);
  const int16_t ox = (DISP_W - DANCE_W) / 2;
  const int16_t oy = (DISP_H - DANCE_H) / 2;

  while (currentView == VIEW_DANCE) {
    for (uint8_t f = 0; f < DANCE_FRAMES && currentView == VIEW_DANCE; f++) {
      const uint8_t* src = dance_frames[f];
      const uint16_t* pal = dance_pals[f];
      uint16_t* dst = danceBuf;

      for (uint16_t i = 0; i < (DANCE_W * DANCE_H) / 2; i++) {
        uint8_t b = pgm_read_byte(src++);
        *dst++ = pgm_read_word(&pal[(b >> 4) & 0x0F]);
        *dst++ = pgm_read_word(&pal[b & 0x0F]);
      }

      tft.drawRGBBitmap(ox, oy, danceBuf, DANCE_W, DANCE_H);
      server.handleClient();
      if (currentView != VIEW_DANCE) break;
      safeDelay(speedMs(80));
    }
  }
  // Dance exited — clear and draw the new view
  transitionFlash();
  switch (currentView) {
    case VIEW_EYES_NORMAL:    drawNormalEyes();    break;
    case VIEW_EYES_SQUISH:    drawSquishEyes();    break;
    case VIEW_CLOCK_DIGITAL:  drawClockDigital();  break;
    case VIEW_CLOCK_ANALOG:   drawClockAnalog();   break;
    case VIEW_WEATHER:        drawWeather();       break;
    case VIEW_TIMER:          drawTimer();         break;
    default: break;
  }
  busy = false;
}

void animClockDigital() {
  busy = true;
  clockDirty = true;
  if (!wxValid && WiFi.status() == WL_CONNECTED && !wxFetching) {
    fetchWeather();
  }
  drawClockDigital();
  busy = false;
}

void animClockAnalog() {
  busy = true;
  clockDirty = true;
  drawClockAnalog();
  busy = false;
}

void animWeather() {
  busy = true;
  if (!wxValid && WiFi.status() == WL_CONNECTED && !wxFetching) {
    fetchWeather();
  }
  drawWeather();
  busy = false;
}

void animAlarm() {
  busy = true;
  clockDirty = true;
  drawAlarm();
  busy = false;
}

// ── Idle blink — quick natural blink when sitting on an eye view ──
void idleBlink() {
  const int16_t lx = eyeLX(0), rx = eyeRX(0), ey = eyeY(), cy = eyeCY();
  // Close eyes briefly
  tft.fillRect(lx, cy - 4, EYE_W, 8, C_BLACK);
  tft.fillRect(rx, cy - 4, EYE_W, 8, C_BLACK);
  safeDelay(speedMs(80));
  // Restore current expression
  switch (currentView) {
    case VIEW_EYES_NORMAL:    drawNormalEyes();    break;
    case VIEW_EYES_SQUISH:    drawSquishEyes();    break;
    case VIEW_EYES_ANGRY:     drawAngryEyes();     break;
    case VIEW_EYES_HEART:     drawHeartEyes();     break;
    case VIEW_EYES_DIZZY:     drawDizzyEyes();     break;
    case VIEW_EYES_SLEEPY:    drawSleepyEyes();    break;
    case VIEW_EYES_SURPRISED: drawSurprisedEyes(); break;
    case VIEW_WINK:           drawWink();          break;
    case VIEW_EYES_CRY:       drawCryEyes();       break;
    case VIEW_EYES_STAR:      drawStarEyes();      break;
    case VIEW_EYES_SHY:       drawShyEyes();       break;
    case VIEW_EYES_PUPPY:     drawPuppyEyes();     break;
    default: break;
  }
  lastIdleBlink = millis();
  idleBlinkNext = (esp_random() % 5000) + 3000;  // 3–8 seconds
}

// ═════════════════════════════════════════════════════════════
//  WEB PAGE
// ═════════════════════════════════════════════════════════════
const char INDEX_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Clawd Mochi</title>
<style>
:root{--bg:#0d0d14;--card:#18181f;--inp:#1c1c24;--bd:#24242e;--or:#c96a3e;--tx:#e8e4dc;--mu:#6a6a78;--gn:#28b878;--r:14px;--rs:10px}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{background:var(--bg);font-family:-apple-system,system-ui,sans-serif;color:var(--tx);display:flex;flex-direction:column;align-items:center;padding:12px 10px 48px;gap:8px;min-height:100vh}

/* Busy */
.busy{width:100%;max-width:400px;height:2px;background:var(--card);overflow:hidden;opacity:0;transition:opacity .3s}
.busy.show{opacity:1}
.busy-i{height:100%;width:40%;background:var(--or);animation:sl 1s ease infinite}
@keyframes sl{0%{margin-left:-40%}100%{margin-left:100%}}

/* Buttons */
.row{display:flex;gap:6px;width:100%;max-width:400px}
.btn{flex:1;background:var(--inp);border:1px solid var(--bd);border-radius:var(--rs);color:var(--tx);font-size:13px;font-weight:600;padding:10px 6px;cursor:pointer;text-align:center;transition:all .15s;user-select:none;min-height:42px;display:flex;align-items:center;justify-content:center}
.btn:active{transform:scale(.96);background:#252430}
.btn.on{background:#1c1008;border-color:var(--or);color:var(--or)}

/* Sections */
.sec{width:100%;max-width:400px;background:var(--card);border:1px solid var(--bd);border-radius:var(--r);overflow:hidden}
.sec-hdr{display:flex;justify-content:space-between;align-items:center;padding:14px 16px;cursor:pointer;user-select:none;font-size:13px;font-weight:700;color:var(--tx);letter-spacing:.5px}
.sec-hdr::after{content:'\25B6';font-size:10px;color:var(--mu);transition:transform .2s}
.sec-hdr.open::after{transform:rotate(90deg)}
.sec-body{display:none;padding:0 16px 14px;flex-direction:column;gap:8px}
.sec-body.open{display:flex}

/* Expression grid */
.expr-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:5px}
.expr-tile{background:var(--inp);border:2px solid var(--bd);border-radius:var(--rs);padding:8px 2px 6px;text-align:center;cursor:pointer;transition:all .15s;user-select:none}
.expr-tile:active{transform:scale(.95)}
.expr-tile.active{border-color:var(--or);box-shadow:0 0 14px rgba(201,106,62,.2)}
.expr-icon{font-size:22px;line-height:1.2;display:block}
.expr-label{font-size:9px;color:var(--mu);font-weight:600;margin-top:1px}

/* Controls */
select{background:var(--inp);border:1px solid var(--bd);border-radius:var(--rs);color:var(--tx);font-size:16px;padding:8px 6px;text-align:center;appearance:none;cursor:pointer;min-width:48px}
select.dur{font-size:12px;min-width:52px}
input[type=color]{width:38px;height:34px;border:2px solid var(--bd);border-radius:var(--rs);cursor:pointer;background:transparent;padding:2px}
input[type=color]::-webkit-color-swatch-wrapper{padding:0}
input[type=color]::-webkit-color-swatch{border-radius:6px;border:none}
input[type=range]{flex:1;accent-color:var(--or);height:4px}
.alarm-row{display:flex;gap:6px;width:100%;align-items:center}
.alarm-id{font-size:12px;font-weight:700;color:var(--or);min-width:22px}
.alarm-colon{font-size:18px;font-weight:700;color:var(--mu);line-height:1}

/* Timer */
.timer-display{font-size:38px;font-weight:200;font-family:'SF Mono','Fira Code',monospace;text-align:center;letter-spacing:2px;padding:8px 0}
.timer-inputs{display:flex;gap:6px;align-items:center;justify-content:center}
.timer-inputs input{width:52px;background:var(--inp);border:1px solid var(--bd);border-radius:var(--rs);color:var(--tx);font-size:20px;padding:8px;text-align:center;font-family:monospace}
.timer-inputs span{color:var(--mu);font-size:16px}

/* Toast */
.toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%);background:var(--card);border:1px solid var(--bd);border-radius:var(--rs);font-size:13px;color:var(--tx);padding:10px 24px;opacity:0;transition:opacity .25s;pointer-events:none;white-space:nowrap;z-index:99}
.toast.show{opacity:1}

/* Speed */
.spd-row{display:flex;gap:4px}
.spd-btn{flex:1;background:var(--inp);border:1px solid var(--bd);border-radius:var(--rs);color:var(--mu);font-size:11px;font-weight:700;padding:8px 2px;cursor:pointer;text-align:center;transition:all .15s}
.spd-btn.active{background:#1c1008;border-color:var(--or);color:var(--or)}

/* Terminal/Canvas */
#twrap{border:1px solid #1a4830;border-radius:var(--rs);background:#0c1018;padding:10px}
#twrap .thdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
#twrap .tttl{font-size:12px;color:var(--gn);font-weight:700;font-family:monospace}
#twrap .tx{background:transparent;border:1px solid #1a4830;border-radius:6px;color:var(--gn);font-size:11px;padding:4px 10px;cursor:pointer}
#twrap .trow{display:flex;gap:6px}
#twrap .tin{flex:1;background:transparent;border:1px solid #1a2820;border-radius:8px;color:#40d880;font-size:14px;padding:8px;outline:none;font-family:monospace}
#twrap .tin::placeholder{color:#1a3828}
#twrap .tgo{background:#1a6040;border:none;border-radius:8px;color:#fff;font-size:18px;padding:8px 14px;cursor:pointer}
#cwrap{border:1px solid var(--bd);border-radius:var(--rs);background:var(--card);padding:10px}
#cvs{width:100%;border-radius:6px;border:1px solid var(--bd);touch-action:none;cursor:crosshair;display:block}
</style>
</head>
<body>
<div class="busy" id="busy"><div class="busy-i"></div></div>

<!-- Status + Screen + Auto -->
<div style="display:flex;align-items:center;gap:8px;width:100%;max-width:400px;margin-bottom:2px">
  <span id="connDot" style="width:8px;height:8px;border-radius:50%;background:#6a6a78;flex-shrink:0" title="Connection"></span>
  <span id="connText" style="font-size:10px;color:var(--mu)">---</span>
  <span style="flex:1"></span>
  <button class="btn on" id="blBtn" onclick="toggleBL()" style="flex:1">Screen ON</button>
  <button class="btn" id="autoBtn" onclick="toggleAuto()" style="flex:1">Auto OFF</button>
</div>

<!-- TFT Display -->
<div class="sec" id="secDisplay">
  <div class="sec-hdr open" onclick="toggleSec('secDisplay')">TFT Display</div>
  <div class="sec-body open">
    <div class="row">
      <button class="btn" onclick="switchView('c')">&#128340; Clock</button>
      <button class="btn" onclick="switchView('C')">&#127761; Analog</button>
      <button class="btn" onclick="switchView('e')">&#127780; Weather</button>
    </div>
    <div class="row">
      <button class="btn" onclick="switchView('d')">&#9000; Terminal</button>
      <button class="btn" onclick="switchView('T')">&#9201; Timer</button>
      <button class="btn" onclick="switchView('j')">&#127936; Dance</button>
    </div>
  </div>
</div>

<!-- Eyes -->
<div class="sec" id="secEyes">
  <div class="sec-hdr open" onclick="toggleSec('secEyes')">Eyes</div>
  <div class="sec-body open">
    <div class="expr-grid" id="exprGrid">
      <div class="expr-tile active" data-cmd="w"><span class="expr-icon">&#128064;</span><span class="expr-label">Normal</span></div>
      <div class="expr-tile" data-cmd="s"><span class="expr-icon">&#128522;</span><span class="expr-label">Happy</span></div>
      <div class="expr-tile" data-cmd="g"><span class="expr-icon">&#128548;</span><span class="expr-label">Angry</span></div>
      <div class="expr-tile" data-cmd="h"><span class="expr-icon">&#128150;</span><span class="expr-label">Heart</span></div>
      <div class="expr-tile" data-cmd="x"><span class="expr-icon">&#128534;</span><span class="expr-label">Dizzy</span></div>
      <div class="expr-tile" data-cmd="z"><span class="expr-icon">&#128554;</span><span class="expr-label">Sleepy</span></div>
      <div class="expr-tile" data-cmd="u"><span class="expr-icon">&#128558;</span><span class="expr-label">Wow</span></div>
      <div class="expr-tile" data-cmd="k"><span class="expr-icon">&#128540;</span><span class="expr-label">Wink</span></div>
      <div class="expr-tile" data-cmd="y"><span class="expr-icon">&#128557;</span><span class="expr-label">Cry</span></div>
      <div class="expr-tile" data-cmd="r"><span class="expr-icon">&#127775;</span><span class="expr-label">Star</span></div>
      <div class="expr-tile" data-cmd="b"><span class="expr-icon">&#128524;</span><span class="expr-label">Shy</span></div>
      <div class="expr-tile" data-cmd="p"><span class="expr-icon">&#128054;</span><span class="expr-label">Puppy</span></div>
      <div class="expr-tile" data-cmd="a"><span class="expr-icon">&#11088;</span><span class="expr-label">Logo</span></div>
    </div>
    <div class="row" style="align-items:center">
      <input type="color" id="bgCol" value="#da1100" onchange="setBgColor(this.value)" title="BG Color">
      <div class="spd-row" style="flex:1">
        <button class="spd-btn" data-spd="1" onclick="setSpeed(1)">Slow</button>
        <button class="spd-btn active" data-spd="2" onclick="setSpeed(2)">Normal</button>
        <button class="spd-btn" data-spd="3" onclick="setSpeed(3)">Fast</button>
      </div>
    </div>
  </div>
</div>

<!-- Auto Switch -->
<div class="sec" id="secAuto">
  <div class="sec-hdr" onclick="toggleSec('secAuto')">Auto Switch</div>
  <div class="sec-body">
    <div class="row" id="autoModeRow">
      <button class="btn on" onclick="setAutoMode(0)">Faces</button>
      <button class="btn" onclick="setAutoMode(1)">Info</button>
    </div>
    <input type="range" id="autoInt" min="1500" max="30000" value="5000" step="500" oninput="setAutoInterval(this.value)">
    <div style="font-size:10px;color:var(--mu);text-align:center"><span id="autoIntV">5s</span></div>
  </div>
</div>

<!-- Alarms -->
<div class="sec" id="secAlarms">
  <div class="sec-hdr open" onclick="toggleSec('secAlarms')">Alarms</div>
  <div class="sec-body open">
    <div id="alarmList"></div>
    <div class="row">
      <button class="btn" onclick="addAlarm()" style="color:var(--gn)">+ Add Alarm</button>
    </div>
  </div>
</div>

<!-- Timer -->
<div class="sec" id="secTimer">
  <div class="sec-hdr open" onclick="toggleSec('secTimer')">Timer</div>
  <div class="sec-body open">
    <div class="timer-display" id="timerDisp">00:00</div>
    <div class="timer-inputs">
      <input type="number" id="tH" value="0" min="0" max="99" placeholder="H">
      <span>h</span>
      <input type="number" id="tM" value="5" min="0" max="59" placeholder="M">
      <span>m</span>
      <input type="number" id="tS" value="0" min="0" max="59" placeholder="S">
      <span>s</span>
    </div>
    <div class="row">
      <button class="btn on" onclick="timerStart()">Start</button>
      <button class="btn" onclick="timerPause()">Pause</button>
      <button class="btn" onclick="timerReset()">Reset</button>
    </div>
    <div style="display:flex;align-items:center;gap:8px;font-size:12px;color:var(--mu)">
      Buzz: <select id="tBuzz" onchange="timerSetBuzz()">
        <option value="3">3s</option><option value="5">5s</option><option value="10" selected>10s</option><option value="30">30s</option><option value="60">60s</option>
      </select>
      <button class="btn" onclick="timerSet()" style="flex:1">Apply</button>
    </div>
  </div>
</div>

<!-- Advanced -->
<div class="sec" id="secAdv">
  <div class="sec-hdr" onclick="toggleSec('secAdv')">Advanced</div>
  <div class="sec-body">
    <div id="twrap">
      <div class="thdr"><span class="tttl">clawd:~$</span><button class="tx" onclick="closeTerm()">Exit</button></div>
      <div class="trow">
        <input class="tin" id="tin" type="text" placeholder="Type command..." autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false">
        <button class="tgo" onclick="termEnter()">&crarr;</button>
      </div>
    </div>
    <div id="cwrap">
      <canvas id="cvs" width="240" height="240"></canvas>
      <div class="row" style="margin-top:6px">
        <button class="btn" onclick="clearCanvas()">Clear</button>
        <button class="btn" id="canvasToggleBtn" onclick="toggleCanvas()">Open Canvas</button>
      </div>
    </div>
  </div>
</div>

<div class="toast" id="toast"></div>

<script>
let tt,blOn=true,autoOn=false,canvasOpen=false,activeCmd='w',curSpeed=2,timerInterval=null;

function toast(m,ok=true){
  const e=document.getElementById('toast');e.textContent=m;
  e.style.borderColor=ok?'var(--gn)':'var(--or)';e.classList.add('show');
  clearTimeout(tt);tt=setTimeout(()=>e.classList.remove('show'),1500)}
let deviceBusy=false;
async function api(p){try{const r=await fetch(p);return r.ok}catch(e){return false}}
async function apiSafe(p){if(deviceBusy){toast('Device busy, wait...',false);return false}return await api(p)}
function pad(n){return String(n).padStart(2,'0')}

// === Collapsible sections ===
function toggleSec(id){
  const s=document.getElementById(id);
  const h=s.querySelector('.sec-hdr'),b=s.querySelector('.sec-body');
  h.classList.toggle('open');b.classList.toggle('open');
}

// === TFT View ===
async function switchView(cmd){
  const ok=await api('/cmd?k='+cmd);
  const labels={c:'Clock',C:'Analog',e:'Weather',d:'Terminal',T:'Timer',j:'Dance'};
  toast(ok?'TFT: '+(labels[cmd]||cmd):'No response, retry',ok);
}

// === Expressions ===
document.getElementById('exprGrid').addEventListener('click',async function(e){
  const tile=e.target.closest('.expr-tile');if(!tile)return;
  document.querySelectorAll('.expr-tile').forEach(t=>t.classList.remove('active'));
  tile.classList.add('active');activeCmd=tile.dataset.cmd;
  const ok=await api('/cmd?k='+tile.dataset.cmd);
  toast(ok?tile.querySelector('.expr-label').textContent+' eyes':'No response, retry',ok);
});

// === Speed & Color ===
function setSpeed(v){curSpeed=v;api('/speed?v='+v);document.querySelectorAll('.spd-btn').forEach(b=>b.classList.toggle('active',b.dataset.spd==v))}
function setBgColor(h){api('/redraw?bg='+encodeURIComponent(h))}

// === Backlight & Auto ===
async function toggleBL(){blOn=!blOn;await api('/backlight?on='+(blOn?1:0));const b=document.getElementById('blBtn');b.textContent=blOn?'Screen ON':'Screen OFF';b.classList.toggle('on',blOn)}
async function toggleAuto(){autoOn=!autoOn;await api('/auto?on='+(autoOn?1:0));const b=document.getElementById('autoBtn');b.textContent=autoOn?'Auto ON':'Auto OFF';b.classList.toggle('on',autoOn)}
async function setAutoMode(v){await api('/auto/mode?v='+v);document.querySelectorAll('#autoModeRow .btn').forEach((b,i)=>b.classList.toggle('on',i===v))}
function setAutoInterval(v){const s=Math.round(v/1000*10)/10;document.getElementById('autoIntV').textContent=(s>=1?s+'s':(v/1000).toFixed(1)+'s');api('/auto/interval?v='+v)}

// === Alarms ===
const DUR_OPTS=[5,10,15,30,60,120,300];
let alarmData=[];
function buildAlarmRows(alarms){
  alarmData=alarms||[{h:7,m:0,on:false,dur:30},{h:12,m:0,on:false,dur:30},{h:0,m:0,on:false,dur:30}];
  renderAlarms();
}
function renderAlarms(){
  const list=document.getElementById('alarmList');list.innerHTML='';
  alarmData.forEach((a,i)=>{
    const div=document.createElement('div');div.className='alarm-row';div.innerHTML=
      '<span class="alarm-id">#'+(i+1)+'</span>'+
      '<select id="alH'+i+'" onchange="saveAlarm('+i+')"></select>'+
      '<span class="alarm-colon">:</span>'+
      '<select id="alM'+i+'" onchange="saveAlarm('+i+')"></select>'+
      '<button class="btn" id="alBtn'+i+'" onclick="toggleAlarm('+i+')" style="flex:1">'+(a.on?'ON':'OFF')+'</button>'+
      '<select class="dur" id="alDur'+i+'" onchange="saveAlarm('+i+')">'+
        DUR_OPTS.map(d=>'<option value="'+d+'"'+(a.dur===d?' selected':'')+'>'+d+'s</option>').join('')+
      '</select>'+
      (alarmData.length>1?'<button class="btn" onclick="delAlarm('+i+')" style="min-width:28px;flex:0;color:var(--or);font-weight:700">&times;</button>':'');
    list.appendChild(div);
    const h=document.getElementById('alH'+i),m=document.getElementById('alM'+i);
    for(let j=0;j<24;j++){const o=document.createElement('option');o.value=j;o.text=pad(j);h.appendChild(o)}
    for(let j=0;j<60;j++){const o=document.createElement('option');o.value=j;o.text=pad(j);m.appendChild(o)}
    h.value=a.h||0;m.value=a.m||0;
    if(a.on){const b=document.getElementById('alBtn'+i);b.classList.add('on')}
  });
}
async function saveAlarm(id){
  const h=document.getElementById('alH'+id).value,m=document.getElementById('alM'+id).value;
  const d=document.getElementById('alDur'+id).value;
  await api('/alarm/set?id='+id+'&h='+h+'&m='+m+'&dur='+d);
}
async function toggleAlarm(id){
  const h=document.getElementById('alH'+id).value,m=document.getElementById('alM'+id).value;
  const d=document.getElementById('alDur'+id).value;
  const b=document.getElementById('alBtn'+id);
  const on=b.textContent.includes('OFF');
  await api('/alarm/set?id='+id+'&h='+h+'&m='+m+'&dur='+d+'&on='+(on?1:0));
  b.textContent=on?'ON':'OFF';b.classList.toggle('on',on);
  toast(on?'Alarm #'+(id+1)+' ON '+pad(h)+':'+pad(m):'Alarm #'+(id+1)+' OFF');
}
async function addAlarm(){
  try{const r=await fetch('/alarm/add');const j=await r.json();
    if(j.ok){alarmData.push({h:0,m:0,on:false,dur:30});renderAlarms();toast('Alarm added')}
    else{toast('Max 8 alarms','err')}
  }catch(e){}
}
async function delAlarm(id){
  if(!confirm('Delete alarm #'+(id+1)+'?'))return;
  try{const r=await fetch('/alarm/del?id='+id);const j=await r.json();
    if(j.ok){alarmData.splice(id,1);renderAlarms();toast('Alarm deleted')}
  }catch(e){}
}

// === Timer ===
async function timerSet(){
  const h=document.getElementById('tH').value,m=document.getElementById('tM').value;
  const s=document.getElementById('tS').value,b=document.getElementById('tBuzz').value;
  await api('/timer/set?h='+h+'&m='+m+'&s='+s+'&buzz='+b);
  timerPoll();
}
async function timerStart(){await api('/timer/start');timerPoll();startTimerPoll()}
async function timerPause(){await api('/timer/pause');timerPoll();stopTimerPoll()}
async function timerReset(){await api('/timer/reset');timerPoll();stopTimerPoll()}
async function timerSetBuzz(){const b=document.getElementById('tBuzz').value;await api('/timer/set?buzz='+b)}
function startTimerPoll(){stopTimerPoll();timerInterval=setInterval(timerPoll,1000)}
function stopTimerPoll(){if(timerInterval){clearInterval(timerInterval);timerInterval=null}}
async function timerPoll(){
  try{const r=await fetch('/timer/state');const t=await r.json();
    const d=document.getElementById('timerDisp');
    if(t.total>0){
      const h=Math.floor(t.remaining/3600),m=Math.floor((t.remaining%3600)/60),s=t.remaining%60;
      d.textContent=t.total>=3600?(pad(h)+':'+pad(m)+':'+pad(s)):(pad(m)+':'+pad(s));
    }else{d.textContent='00:00'}
    if(!t.running&&t.remaining===0)stopTimerPoll();
  }catch(e){}
}

// === Terminal ===
const tin=document.getElementById('tin');let lastVal='';
tin.addEventListener('input',async()=>{const c=tin.value,l=lastVal;if(c.length>l.length)await api('/char?c='+encodeURIComponent(c[c.length-1]));else if(c.length<l.length)await api('/char?c=%08');lastVal=c});
async function termEnter(){await api('/char?c=%0A');tin.value='';lastVal='';tin.focus()}
tin.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();termEnter()}})
function closeTerm(){tin.value='';lastVal='';toast('Terminal cleared')}

// === Canvas ===
let drawing=false,lastX=0,lastY=0;
const cvs=document.getElementById('cvs'),ctx=cvs.getContext('2d');
function getPos(e){const r=cvs.getBoundingClientRect();const sx=cvs.width/r.width,sy=cvs.height/r.height;const s=e.touches?e.touches[0]:e;return{x:(s.clientX-r.left)*sx,y:(s.clientY-r.top)*sy}}
function startDraw(e){if(!canvasOpen)return;e.preventDefault();drawing=true;const p=getPos(e);lastX=p.x;lastY=p.y;ctx.beginPath();ctx.arc(p.x,p.y,2.5,0,Math.PI*2);ctx.fillStyle='#fff';ctx.fill()}
function moveDraw(e){if(!drawing)return;e.preventDefault();const p=getPos(e);ctx.beginPath();ctx.moveTo(lastX,lastY);ctx.lineTo(p.x,p.y);ctx.strokeStyle='#fff';ctx.lineWidth=4;ctx.lineCap='round';ctx.stroke();lastX=p.x;lastY=p.y}
function endDraw(e){if(!drawing)return;drawing=false}
cvs.addEventListener('mousedown',startDraw);cvs.addEventListener('mousemove',moveDraw);
cvs.addEventListener('mouseup',endDraw);cvs.addEventListener('mouseleave',endDraw);
cvs.addEventListener('touchstart',startDraw,{passive:false});cvs.addEventListener('touchmove',moveDraw,{passive:false});
cvs.addEventListener('touchend',endDraw);
function toggleCanvas(){canvasOpen=!canvasOpen;const b=document.getElementById('canvasToggleBtn');b.textContent=canvasOpen?'Close Canvas':'Open Canvas';b.classList.toggle('on',canvasOpen);if(canvasOpen){ctx.fillStyle='#18181f';ctx.fillRect(0,0,cvs.width,cvs.height);toast('Canvas ready')}}
function clearCanvas(){ctx.fillStyle='#18181f';ctx.fillRect(0,0,cvs.width,cvs.height);toast('Cleared')}

// === Handle state update (from WebSocket or HTTP fallback) ===
function handleStateUpdate(j){
  if(j.speed!==undefined){
    curSpeed=j.speed;
    document.querySelectorAll('.spd-btn').forEach(b=>b.classList.toggle('active',parseInt(b.dataset.spd)===j.speed));
  }
  if(j.bl!==undefined){
    blOn=j.bl;const b=document.getElementById('blBtn');
    b.textContent=j.bl?'Screen ON':'Screen OFF';b.classList.toggle('on',j.bl);
  }
  if(j.auto!==undefined){
    autoOn=j.auto;const b=document.getElementById('autoBtn');
    b.textContent=j.auto?'Auto ON':'Auto OFF';b.classList.toggle('on',j.auto);
  }
  if(j.autoMode!==undefined){
    document.querySelectorAll('#autoModeRow .btn').forEach((b,i)=>b.classList.toggle('on',i===j.autoMode));
  }
  if(j.autoInt){
    document.getElementById('autoInt').value=j.autoInt;
    document.getElementById('autoIntV').textContent=Math.round(j.autoInt/100)/10+'s';
  }
  if(j.alarms&&j.alarms.length){
    const changed=JSON.stringify(j.alarms)!==JSON.stringify(alarmData.map(a=>({h:a.h,m:a.m,on:a.on,dur:a.dur})));
    if(changed){alarmData=j.alarms;renderAlarms();}
  }
  if(j.timer){
    const d=document.getElementById('timerDisp');
    if(j.timer.total>0){
      const h=Math.floor(j.timer.remaining/3600),m=Math.floor((j.timer.remaining%3600)/60),s=j.timer.remaining%60;
      d.textContent=j.timer.total>=3600?(pad(h)+':'+pad(m)+':'+pad(s)):(pad(m)+':'+pad(s));
    }else{d.textContent='00:00'}
    if(!j.timer.running&&j.timer.remaining===0)stopTimerPoll();
    document.getElementById('tBuzz').value=j.timer.buzz||10;
  }
  deviceBusy=j.busy;document.getElementById('busy').classList.toggle('show',j.busy);
}

// === Init ===
(async()=>{
  buildAlarmRows();
  // Bootstrap: fetch state once via HTTP before WebSocket connects
  try{
    const r=await fetch('/state');const j=await r.json();
    handleStateUpdate(j);
    if(j.timer&&j.timer.running)startTimerPoll();
  }catch(e){}
  ctx.fillStyle='#18181f';ctx.fillRect(0,0,cvs.width,cvs.height);

  // Connection status
  let connFail=0,wsPollInterval=null;
  const updateConn=(ok,busy)=>{
    const d=document.getElementById('connDot'),t=document.getElementById('connText');
    if(ok){connFail=0;d.style.background='var(--gn)';t.textContent=busy?'Busy':'Live'}
    else{connFail++;d.style.background=connFail>4?'var(--or)':'#e8b830';t.textContent=connFail>4?'Offline':'Slow'}
  };

  // Start HTTP fallback polling
  function startHttpPolling(){
    if(wsPollInterval)return;
    wsPollInterval=setInterval(async()=>{
      try{const r=await fetch('/state');const j=await r.json();handleStateUpdate(j);updateConn(true,j.busy)}catch(e){updateConn(false)}
    },15000);
  }
  function stopHttpPolling(){
    if(wsPollInterval){clearInterval(wsPollInterval);wsPollInterval=null}
  }

  // WebSocket connection
  function wsConnect(){
    const proto=location.protocol==='https:'?'wss:':'ws:';
    const ws=new WebSocket(proto+'//'+location.hostname+':81/');
    ws.onopen=()=>{updateConn(true,deviceBusy);stopHttpPolling()};
    ws.onmessage=(e)=>{
      try{const j=JSON.parse(e.data);handleStateUpdate(j);updateConn(true,j.busy)}catch(err){}
    };
    ws.onclose=()=>{updateConn(false);startHttpPolling();setTimeout(wsConnect,3000)};
    ws.onerror=()=>{ws.close()};
  }
  wsConnect();
})();
</script>
</body>
</html>
)rawhtml";

// ═════════════════════════════════════════════════════════════
//  WEB ROUTES
// ═════════════════════════════════════════════════════════════

void routeRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.send_P(200, "text/html", INDEX_HTML);
}

void routeCmd() {
  if (!server.hasArg("k") || server.arg("k").isEmpty()) {
    server.send(400, "application/json", "{\"e\":1}"); return;
  }
  const char c = server.arg("k")[0];

  if (termMode) {
    termMode = false;
    if (c == 'q') { drawCodeView(); server.send(200, "application/json", "{\"ok\":1}"); return; }
    // For any other key, exit terminal and let it fall through to switch view
  }

  server.send(200, "application/json", "{\"ok\":1}");
  currentView = VIEW_EYES_NORMAL;  // default, overwritten below
  switch (c) {
    case 'w': currentView = VIEW_EYES_NORMAL;    break;
    case 's': currentView = VIEW_EYES_SQUISH;    break;
    case 'd':
      currentView = VIEW_CODE; drawCodeView();
      termMode = true; termClear(); termFullRedraw(); break;
    case 'a': animLogoReveal(); return;
    case 'g': currentView = VIEW_EYES_ANGRY;     break;
    case 'h': currentView = VIEW_EYES_HEART;     break;
    case 'x': currentView = VIEW_EYES_DIZZY;     break;
    case 'z': currentView = VIEW_EYES_SLEEPY;    break;
    case 'u': currentView = VIEW_EYES_SURPRISED; break;
    case 'k': currentView = VIEW_WINK;           break;
    case 'y': currentView = VIEW_EYES_CRY;       break;
    case 'r': currentView = VIEW_EYES_STAR;      break;
    case 'b': currentView = VIEW_EYES_SHY;       break;
    case 'p': currentView = VIEW_EYES_PUPPY;     break;
    case 'c': currentView = VIEW_CLOCK_DIGITAL;  break;
    case 'C': currentView = VIEW_CLOCK_ANALOG;   break;
    case 'e': currentView = VIEW_WEATHER;        break;
    case 'A': currentView = VIEW_ALARM;          break;
    case 'T': currentView = VIEW_TIMER;          break;
    case 'j': currentView = VIEW_DANCE;          break;
  }
  notifyChanged();  // persist currentView
  idleBlinkNext = 0;   // reset idle blink timer on manual switch
  transitionFlash();
  // Run animation for the new view (unless dance, which loops itself)
  switch (currentView) {
    case VIEW_EYES_NORMAL:    animNormalEyes();    break;
    case VIEW_EYES_SQUISH:    animSquishEyes();    break;
    case VIEW_EYES_ANGRY:     animAngryEyes();     break;
    case VIEW_EYES_HEART:     animHeartEyes();     break;
    case VIEW_EYES_DIZZY:     animDizzyEyes();     break;
    case VIEW_EYES_SLEEPY:    animSleepyEyes();    break;
    case VIEW_EYES_SURPRISED: animSurprisedEyes(); break;
    case VIEW_WINK:           animWink();          break;
    case VIEW_EYES_CRY:       animCryEyes();       break;
    case VIEW_EYES_STAR:      animStarEyes();      break;
    case VIEW_EYES_SHY:       animShyEyes();       break;
    case VIEW_EYES_PUPPY:     animPuppyEyes();     break;
    case VIEW_CLOCK_DIGITAL:  animClockDigital();  break;
    case VIEW_CLOCK_ANALOG:   animClockAnalog();   break;
    case VIEW_WEATHER:        animWeather();       break;
    case VIEW_ALARM:          animAlarm();         break;
    case VIEW_TIMER:          drawTimer();         break;
    case VIEW_DANCE:          animDance();         break;
  }
}

void routeChar() {
  if (!termMode) { server.send(200, "application/json", "{\"ok\":1}"); return; }
  const String val = server.arg("c");
  if (val.length() > 0) termAddChar(val[0]);
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeSpeed() {
  if (server.hasArg("v")) animSpeed = constrain(server.arg("v").toInt(), 1, 3);
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

// /redraw?bg=hex — set animBg and immediately redraw current view
void routeRedraw() {
  if (server.hasArg("bg")) {
    animBgColor = hexToRgb565(server.arg("bg"));
    drawBgColor = animBgColor;
  }
  switch (currentView) {
    case VIEW_EYES_NORMAL:    drawNormalEyes();    break;
    case VIEW_EYES_SQUISH:    drawSquishEyes();    break;
    case VIEW_CODE:           drawCodeView();      break;
    case VIEW_DRAW:           tft.fillScreen(drawBgColor); break;
    case VIEW_EYES_ANGRY:     drawAngryEyes();     break;
    case VIEW_EYES_HEART:     drawHeartEyes();     break;
    case VIEW_EYES_DIZZY:     drawDizzyEyes();     break;
    case VIEW_EYES_SLEEPY:    drawSleepyEyes();    break;
    case VIEW_EYES_SURPRISED: drawSurprisedEyes(); break;
    case VIEW_WINK:           drawWink();          break;
    case VIEW_EYES_CRY:       drawCryEyes();       break;
    case VIEW_EYES_STAR:      drawStarEyes();      break;
    case VIEW_EYES_SHY:       drawShyEyes();       break;
    case VIEW_EYES_PUPPY:     drawPuppyEyes();     break;
    case VIEW_CLOCK_DIGITAL:  drawClockDigital();  break;
    case VIEW_CLOCK_ANALOG:   drawClockAnalog();   break;
    case VIEW_WEATHER:        drawWeather();       break;
    case VIEW_TIMER:          drawTimer();         break;
    case VIEW_DANCE:          drawDance();         break;
    case VIEW_ALARM:          drawAlarm();         break;
  }
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeCanvas() {
  const bool on = server.hasArg("on") && server.arg("on") == "1";
  if (on) { currentView = VIEW_DRAW; tft.fillScreen(drawBgColor); }
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeDrawClear() {
  const String bg = server.hasArg("bg") ? server.arg("bg") : "#aa4818";
  drawBgColor = hexToRgb565(bg);
  animBgColor = drawBgColor;
  currentView = VIEW_DRAW; termMode = false;
  tft.fillScreen(drawBgColor);
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeDrawStroke() {
  if (!server.hasArg("pts") || !server.hasArg("pen")) {
    server.send(200, "application/json", "{\"ok\":1}"); return;
  }
  const uint16_t color = hexToRgb565(server.arg("pen"));
  const String   data  = server.arg("pts");
  currentView = VIEW_DRAW;

  struct Pt { int16_t x, y; };
  Pt prev = {-1, -1};
  int start = 0;
  while (start < (int)data.length()) {
    int semi = data.indexOf(';', start);
    if (semi == -1) semi = data.length();
    String entry = data.substring(start, semi);
    const int comma = entry.indexOf(',');
    if (comma > 0) {
      const int16_t x = entry.substring(0, comma).toInt();
      const int16_t y = entry.substring(comma + 1).toInt();
      if (prev.x >= 0) {
        tft.drawLine(prev.x, prev.y, x, y, color);
        tft.drawLine(prev.x + 1, prev.y, x + 1, y, color);
        tft.drawLine(prev.x, prev.y + 1, x, y + 1, color);
      } else {
        tft.fillCircle(x, y, 2, color);
      }
      prev = {x, y};
    }
    start = semi + 1;
  }
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeBacklight() {
  setBacklight(server.hasArg("on") && server.arg("on") == "1");
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

// Convert RGB565 back to #RRGGBB for state endpoint
String rgb565ToHex(uint16_t c) {
  uint8_t r = ((c >> 11) & 0x1F) << 3;
  uint8_t g = ((c >> 5)  & 0x3F) << 2;
  uint8_t b = (c & 0x1F) << 3;
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
  return String(buf);
}

void routeState() {
  String j = "{\"view\":"; j += currentView;
  j += ",\"busy\":";   j += busy        ? "true" : "false";
  j += ",\"term\":";   j += termMode    ? "true" : "false";
  j += ",\"bl\":";     j += backlightOn ? "true" : "false";
  j += ",\"speed\":";  j += animSpeed;
  j += ",\"auto\":";   j += autoSwitch  ? "true" : "false";
  j += ",\"autoMode\":"; j += autoSwitchMode;
  j += ",\"autoInt\":";  j += autoSwitchInterval;
  // alarms array
  j += ",\"alarmCount\":"; j += alarmCount;
  j += ",\"alarms\":[";
  for (uint8_t i = 0; i < alarmCount; i++) {
    if (i > 0) j += ",";
    j += "{\"h\":"; j += alarms[i].hour;
    j += ",\"m\":"; j += alarms[i].min;
    j += ",\"on\":"; j += (alarms[i].enabled ? "true" : "false");
    j += ",\"dur\":"; j += alarms[i].duration;
    j += "}";
  }
  j += "]";
  // timer
  j += ",\"timer\":{";
  j += "\"total\":"; j += timer.totalSec;
  j += ",\"remaining\":"; j += timer.remaining;
  j += ",\"running\":"; j += (timer.running ? "true" : "false");
  j += ",\"buzz\":"; j += timer.buzzDuration;
  j += "}";
  server.send(200, "application/json", j);
}

void routeAuto() {
  if (server.hasArg("on")) {
    autoSwitch = (server.arg("on") == "1");
    lastAutoSwitch = millis();
    autoSwitchIdx = 0;
  }
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeAutoInterval() {
  if (server.hasArg("v")) {
    autoSwitchInterval = constrain(server.arg("v").toInt(), 1500L, 30000L);
  }
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeAutoMode() {
  if (server.hasArg("v")) {
    autoSwitchMode = constrain(server.arg("v").toInt(), 0, 1);
    autoSwitchIdx = 0;
  }
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeAlarmGet() {
  uint8_t id = server.hasArg("id") ? constrain(server.arg("id").toInt(), 0, MAX_ALARMS-1) : 0;
  String j = "{\"id\":"; j += id;
  j += ",\"h\":"; j += alarms[id].hour;
  j += ",\"m\":"; j += alarms[id].min;
  j += ",\"on\":"; j += (alarms[id].enabled ? "true" : "false");
  j += ",\"dur\":"; j += alarms[id].duration;
  j += "}";
  server.send(200, "application/json", j);
}

void routeAlarmSet() {
  uint8_t id = server.hasArg("id") ? constrain(server.arg("id").toInt(), 0, MAX_ALARMS-1) : 0;
  if (server.hasArg("h")) alarms[id].hour = constrain(server.arg("h").toInt(), 0, 23);
  if (server.hasArg("m")) alarms[id].min  = constrain(server.arg("m").toInt(), 0, 59);
  if (server.hasArg("dur")) alarms[id].duration = constrain(server.arg("dur").toInt(), 5, 300);
  if (server.hasArg("on")) {
    alarms[id].enabled = (server.arg("on") == "1");
    if (!alarms[id].enabled && activeAlarmIdx == id) {
      // Dismiss this ringing alarm
      alarmRinging   = false;
      activeAlarmIdx = 255;
      busy           = false;
      pinMode(BUZZER, INPUT);
      currentView = prevView; clockDirty = true;
      drawClockDigital();
    }
  }
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeAlarmAdd() {
  if (alarmCount >= MAX_ALARMS) {
    server.send(400, "application/json", "{\"e\":\"max\"}"); return;
  }
  alarms[alarmCount] = {0, 0, false, 30};
  alarmCount++;
  notifyChanged();
  server.send(200, "application/json", ("{\"ok\":1,\"count\":" + String(alarmCount) + "}"));
}

void routeAlarmDel() {
  uint8_t id = server.hasArg("id") ? server.arg("id").toInt() : 255;
  if (id >= alarmCount || alarmCount <= 1) {
    server.send(400, "application/json", "{\"e\":\"invalid\"}"); return;
  }
  if (activeAlarmIdx == id) {
    alarmRinging = false; activeAlarmIdx = 255; busy = false;
    pinMode(BUZZER, INPUT);
  }
  // Shift remaining alarms down
  for (uint8_t i = id; i < alarmCount - 1; i++) {
    alarms[i] = alarms[i + 1];
  }
  alarmCount--;
  if (activeAlarmIdx > id) activeAlarmIdx--;
  notifyChanged();
  server.send(200, "application/json", ("{\"ok\":1,\"count\":" + String(alarmCount) + "}"));
}

// ── Timer routes ─────────────────────────────────────────────
void routeTimerSet() {
  uint16_t h = server.hasArg("h") ? server.arg("h").toInt() : 0;
  uint8_t  m = server.hasArg("m") ? server.arg("m").toInt() : 0;
  uint8_t  s = server.hasArg("s") ? server.arg("s").toInt() : 0;
  timer.totalSec  = (uint32_t)h * 3600 + m * 60 + s;
  timer.remaining = timer.totalSec;
  timer.running   = false;
  timer.lastTick  = millis();
  if (server.hasArg("buzz")) timer.buzzDuration = constrain(server.arg("buzz").toInt(), 1, 120);
  if (currentView == VIEW_TIMER) drawTimer();
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeTimerStart() {
  if (timer.remaining > 0) {
    timer.running  = true;
    timer.lastTick = millis();
    if (currentView == VIEW_TIMER) drawTimer();
  }
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeTimerPause() {
  timer.running = false;
  if (currentView == VIEW_TIMER) drawTimer();
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeTimerReset() {
  timer.remaining = timer.totalSec;
  timer.running   = false;
  if (currentView == VIEW_TIMER) drawTimer();
  notifyChanged();
  server.send(200, "application/json", "{\"ok\":1}");
}

void routeTimerState() {
  uint32_t sec = timer.remaining;
  uint8_t h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
  String j = "{\"total\":"; j += timer.totalSec;
  j += ",\"remaining\":"; j += timer.remaining;
  j += ",\"running\":"; j += (timer.running ? "true" : "false");
  j += ",\"buzz\":"; j += timer.buzzDuration;
  j += ",\"h\":"; j += h; j += ",\"m\":"; j += m; j += ",\"s\":"; j += s;
  j += "}";
  server.send(200, "application/json", j);
}

void routeIP() {
  // Return device IP addresses so clawd-on-desk bridge can discover it
  String json = "{\"ap\":\"192.168.4.1\"";
  if (staIP.length() > 0) {
    json += ",\"sta\":\"" + staIP + "\"";
  }
  json += "}";
  server.send(200, "application/json", json);
}

void routeNotFound() { server.send(404, "text/plain", "not found"); }

// ═════════════════════════════════════════════════════════════
//  WebSocket
// ═════════════════════════════════════════════════════════════

void wsBroadcastState() {
  String j = "{\"view\":"; j += currentView;
  j += ",\"busy\":";   j += busy        ? "true" : "false";
  j += ",\"term\":";   j += termMode    ? "true" : "false";
  j += ",\"bl\":";     j += backlightOn ? "true" : "false";
  j += ",\"speed\":";  j += animSpeed;
  j += ",\"auto\":";   j += autoSwitch  ? "true" : "false";
  j += ",\"autoMode\":"; j += autoSwitchMode;
  j += ",\"autoInt\":";  j += autoSwitchInterval;
  j += ",\"alarms\":[";
  for (uint8_t i = 0; i < alarmCount; i++) {
    if (i > 0) j += ",";
    j += "{\"h\":"; j += alarms[i].hour;
    j += ",\"m\":"; j += alarms[i].min;
    j += ",\"on\":"; j += (alarms[i].enabled ? "true" : "false");
    j += ",\"dur\":"; j += alarms[i].duration;
    j += "}";
  }
  j += "],\"timer\":{";
  j += "\"total\":"; j += timer.totalSec;
  j += ",\"remaining\":"; j += timer.remaining;
  j += ",\"running\":"; j += (timer.running ? "true" : "false");
  j += ",\"buzz\":"; j += timer.buzzDuration;
  j += "}}";
  wsServer.broadcastTXT(j);
}

void wsEventHandler(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  if (type == WStype_CONNECTED) {
    wsBroadcastState();  // send current state to newly connected client
  }
  // ignore other events for now
}

// ═════════════════════════════════════════════════════════════
//  SETUP
// ═════════════════════════════════════════════════════════════

void setup() {
  // Buzzer: keep as INPUT (Hi‑Z) so module stays silent
  pinMode(BUZZER, INPUT);

  Serial.begin(115200);

  pinMode(TFT_BLK, OUTPUT);
  setBacklight(true);

  SPI.begin(8, -1, 10, TFT_CS);   // SCK=8, MOSI=10
  tft.init(240, 240);
  tft.setSPISpeed(40000000);
  tft.setRotation(1);
  initColours();

  // ── Boot splash ────────────────────────────────────────────
  tft.fillScreen(animBgColor);
  tft.setTextColor(C_WHITE); tft.setTextSize(3);
  tft.setCursor(DISP_W / 2 - 54, DISP_H / 2 - 22); tft.print("Clawd");
  tft.setCursor(DISP_W / 2 - 54, DISP_H / 2 + 14); tft.print("Mochi");

  // ── Start WiFi ─────────────────────────────────────────────
  if (strlen(STA_SSID) > 0) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    WiFi.begin(STA_SSID, STA_PASS);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
  }

  // ── Register routes + start server IMMEDIATELY ─────────────
  server.on("/",            HTTP_GET, routeRoot);
  server.on("/cmd",         HTTP_GET, routeCmd);
  server.on("/char",        HTTP_GET, routeChar);
  server.on("/speed",       HTTP_GET, routeSpeed);
  server.on("/redraw",      HTTP_GET, routeRedraw);
  server.on("/canvas",      HTTP_GET, routeCanvas);
  server.on("/draw/clear",  HTTP_GET, routeDrawClear);
  server.on("/draw/stroke", HTTP_GET, routeDrawStroke);
  server.on("/backlight",   HTTP_GET, routeBacklight);
  server.on("/state",       HTTP_GET, routeState);
  server.on("/auto",         HTTP_GET, routeAuto);
  server.on("/auto/mode",    HTTP_GET, routeAutoMode);
  server.on("/auto/interval", HTTP_GET, routeAutoInterval);
  server.on("/alarm/get",    HTTP_GET, routeAlarmGet);
  server.on("/alarm/set",    HTTP_GET, routeAlarmSet);
  server.on("/alarm/add",    HTTP_GET, routeAlarmAdd);
  server.on("/alarm/del",    HTTP_GET, routeAlarmDel);
  server.on("/timer/set",    HTTP_GET, routeTimerSet);
  server.on("/timer/start",  HTTP_GET, routeTimerStart);
  server.on("/timer/pause",  HTTP_GET, routeTimerPause);
  server.on("/timer/reset",  HTTP_GET, routeTimerReset);
  server.on("/timer/state",  HTTP_GET, routeTimerState);
  server.on("/ip",           HTTP_GET, routeIP);
  server.onNotFound(routeNotFound);
  server.begin();
  wsServer.begin();
  wsServer.onEvent(wsEventHandler);

  // ── OTA setup ──────────────────────────────────────────────
  ArduinoOTA.setHostname("clawd-mochi");
  ArduinoOTA.setPassword("clawd1234");
  ArduinoOTA.onStart([]() {
    busy = true; termMode = false; alarmRinging = false;
    pinMode(BUZZER, INPUT);
    tft.fillScreen(C_BLACK);
    tft.setTextColor(C_WHITE); tft.setTextSize(2);
    tft.setCursor(30, DISP_H / 2 - 12); tft.print("OTA Update...");
  });
  ArduinoOTA.onEnd([]() {
    tft.fillScreen(C_BLACK);
    tft.setTextColor(C_GREEN); tft.setTextSize(2);
    tft.setCursor(40, DISP_H / 2 - 12); tft.print("Done!");
  });
  ArduinoOTA.onProgress([](unsigned int p, unsigned int t) {
    int pct = (p * 100) / t;
    tft.fillRect(20, DISP_H / 2 + 20, 200, 16, C_BLACK);
    tft.drawRect(20, DISP_H / 2 + 20, 200, 16, C_WHITE);
    tft.fillRect(22, DISP_H / 2 + 22, 196 * pct / 100, 12, C_ORANGE);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    tft.setTextColor(C_ORANGE); tft.setTextSize(1);
    tft.setCursor(20, DISP_H / 2 + 50);
    tft.print("OTA Err "); tft.print(e);
    safeDelay(3000);
  });
  ArduinoOTA.begin();

  // ── Startup animation (server is live — page loads during this!) ──
  animLogoReveal();

  // ── Auto‑detect location + start NTP ──────────────────────
  if (strlen(STA_SSID) > 0) {
    for (uint8_t i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
      safeDelay(250);
    }
    if (WiFi.status() == WL_CONNECTED) {
      resolveLocation();
    } else {
      configTzTime(wxTz.c_str(), NTP_SERVER);
    }
  } else {
    configTzTime(wxTz.c_str(), NTP_SERVER);
  }

  // ── Restore saved preferences ────────────────────────────
  loadPreferences();

  // ── WiFi info screen (stays until first web request) ───────
  tft.fillScreen(C_DARKBG);
  tft.fillRect(0, 0, DISP_W, 4, C_ORANGE);
  tft.setTextColor(C_WHITE);  tft.setTextSize(2);
  tft.setCursor(12, 16);  tft.print("WiFi: ClaWD-Mochi");
  tft.setTextColor(C_MUTED);  tft.setTextSize(1);
  tft.setCursor(12, 44);  tft.print("PW: clawd1234");
  tft.setTextColor(C_WHITE);  tft.setTextSize(2);
  tft.setCursor(12, 68);  tft.print("Browser:");
  tft.setTextColor(C_ORANGE); tft.setTextSize(2);
  tft.setCursor(12, 94);  tft.print("192.168.4.1");
  tft.setTextColor(C_MUTED);  tft.setTextSize(1);
  if (strlen(STA_SSID) > 0) {
    tft.setCursor(12, 124); tft.print("STA connecting...");
  } else {
    tft.setCursor(12, 124); tft.print("tap to start");
  }

  // Stay on WiFi info screen (user switches views from web)
  // WiFi info already drawn above — do nothing else
}

// ═════════════════════════════════════════════════════════════
//  LOOP
// ═════════════════════════════════════════════════════════════

void loop() {
  ArduinoOTA.handle();
  wsServer.loop();
  server.handleClient();

  // ── Check STA WiFi connection ─────────────────────────────────
  static bool staWasConnected = false;
  if (strlen(STA_SSID) > 0) {
    bool staConnected = (WiFi.status() == WL_CONNECTED);
    if (staConnected && !staWasConnected) {
      staIP = WiFi.localIP().toString();
      staWasConnected = true;
    } else if (!staConnected) {
      staWasConnected = false;
      staIP = "";
    }
  }

  // ── Clock / Weather periodic updates ─────────────────────────
  uint32_t ms = millis();

  // NTP sync with cooldown + periodic resync every 6 hours
  static uint32_t lastNtpTry = 0;
  static uint32_t lastNtpSync = 0;
  if (!timeSynced && WiFi.status() == WL_CONNECTED && ms - lastNtpTry >= 30000) {
    lastNtpTry = ms;
    syncTimeIfNeeded();
    if (timeSynced) lastNtpSync = ms;
  }
  if (timeSynced && ms - lastNtpSync >= 21600000) {  // re-sync every 6h
    lastNtpSync = ms;
    timeSynced = false;
  }

  // Redraw clocks every second
  if (timeSynced && !busy && !termMode && currentView != VIEW_DANCE) {
    if (ms - lastClockDraw >= 1000) {
      lastClockDraw = ms;
      if (currentView == VIEW_CLOCK_DIGITAL) drawClockDigital();
      else if (currentView == VIEW_CLOCK_ANALOG) drawClockAnalog();
    }
  }

  // Weather fetch (skip during animations — safeDelay drains queue after)
  if (WiFi.status() == WL_CONNECTED && !wxFetching && !termMode && !busy) {
    bool timeUp = (ms - lastWeatherFetch >= WEATHER_INTERVAL);
    bool firstFetch = (!wxValid && lastWeatherFetch == 0 && ms > 30000);
    bool retry = (!wxValid && lastWeatherFetch > 0 &&
                  ms - lastWeatherFetch >= 60000);  // retry every 60s on failure
    if (timeUp || firstFetch || retry) {
      lastWeatherFetch = ms;
      fetchWeather();
      if (currentView == VIEW_CLOCK_DIGITAL) drawClockDigital();
      else if (currentView == VIEW_WEATHER)  drawWeather();
    }
  }

  // ── Alarm check ──────────────────────────────────────────────
  if (timeSynced && !alarmRinging) {
    time_t n; struct tm t;
    time(&n); localtime_r(&n, &t);
    for (uint8_t i = 0; i < alarmCount; i++) {
      if (alarms[i].enabled &&
          t.tm_hour == alarms[i].hour &&
          t.tm_min  == alarms[i].min &&
          t.tm_sec  == 0) {
        alarmRinging   = true;
        activeAlarmIdx = i;
        alarmRingStart = millis();
        prevView       = currentView;
        busy           = true;
        currentView    = VIEW_ALARM;
        drawAlarm();
        break;
      }
    }
  }
  if (alarmRinging && activeAlarmIdx < alarmCount) {
    uint32_t durMs = (uint32_t)alarms[activeAlarmIdx].duration * 1000;
    if (durMs < 3000) durMs = 3000;  // min 3s
    if (millis() - alarmRingStart < durMs) {
      int phase = (millis() - alarmRingStart) % 300;
      if (phase < 200) {
        pinMode(BUZZER, OUTPUT);
        digitalWrite(BUZZER, LOW);
        if (phase < 80) tft.fillScreen(C_BLACK);
        else            drawAlarm();
      } else {
        pinMode(BUZZER, INPUT);
        drawAlarm();
      }
    } else {
      pinMode(BUZZER, INPUT);
      alarmRinging   = false;
      alarms[activeAlarmIdx].enabled = false;
      activeAlarmIdx = 255;
      busy           = false;
      currentView    = prevView; clockDirty = true;
      drawClockDigital();
    }
  }

  // ── Timer tick ────────────────────────────────────────────────
  static bool   timerBuzzing   = false;
  static uint32_t timerBuzzEnd = 0;
  if (timer.running) {
    if (ms - timer.lastTick >= 1000) {
      timer.lastTick = ms;
      if (timer.remaining > 0) {
        timer.remaining--;
        if (currentView == VIEW_TIMER) drawTimer();
        wsBroadcastState();  // push timer update to web clients
      }
      if (timer.remaining == 0) {
        timer.running = false;
        timerBuzzing  = true;
        timerBuzzEnd  = ms + (uint32_t)timer.buzzDuration * 1000;
        pinMode(BUZZER, OUTPUT);
        if (currentView != VIEW_ALARM) {
          prevView    = currentView;
          currentView = VIEW_TIMER;
        }
        drawTimer();
      }
    }
  }
  // Non-blocking timer buzz
  if (timerBuzzing) {
    uint32_t phase = ms % 400;
    if (phase < 250) {
      pinMode(BUZZER, OUTPUT);
      digitalWrite(BUZZER, LOW);
    } else {
      pinMode(BUZZER, INPUT);
    }
    if (ms >= timerBuzzEnd) {
      pinMode(BUZZER, INPUT);
      timerBuzzing = false;
      if (currentView == VIEW_TIMER) drawTimer();
    }
  }

  // ── Auto‑switch timer ──────────────────────────────────────────
  if (autoSwitch && !busy && !termMode && currentView != VIEW_DRAW) {
    uint32_t now = millis();
    if (now - lastAutoSwitch >= autoSwitchInterval) {
      lastAutoSwitch = now;
      uint8_t nextView;
      if (autoSwitchMode == 0) {
        // Expressions only
        nextView = pgm_read_byte(&AUTO_EXPR[autoSwitchIdx]);
        autoSwitchIdx = (autoSwitchIdx + 1) % AUTO_EXPR_COUNT;
      } else {
        // Info only (clock digital, clock analog, weather)
        nextView = pgm_read_byte(&AUTO_INFO[autoSwitchIdx]);
        autoSwitchIdx = (autoSwitchIdx + 1) % AUTO_INFO_COUNT;
      }
      currentView = nextView;
      transitionFlash();
      switch (nextView) {
        case VIEW_EYES_NORMAL:    animNormalEyes();    break;
        case VIEW_EYES_SQUISH:    animSquishEyes();    break;
        case VIEW_EYES_ANGRY:     animAngryEyes();     break;
        case VIEW_EYES_HEART:     animHeartEyes();     break;
        case VIEW_EYES_DIZZY:     animDizzyEyes();     break;
        case VIEW_EYES_SLEEPY:    animSleepyEyes();    break;
        case VIEW_EYES_SURPRISED: animSurprisedEyes(); break;
        case VIEW_WINK:           animWink();          break;
        case VIEW_EYES_CRY:       animCryEyes();       break;
        case VIEW_EYES_STAR:      animStarEyes();      break;
        case VIEW_EYES_SHY:       animShyEyes();       break;
        case VIEW_EYES_PUPPY:     animPuppyEyes();     break;
        case VIEW_CLOCK_DIGITAL:  animClockDigital();  break;
        case VIEW_CLOCK_ANALOG:   animClockAnalog();   break;
        case VIEW_WEATHER:        animWeather();       break;
      }
    }
  }

  // ── Idle blink (random natural blinks on eye views) ───────────
  if (!busy && !termMode && !autoSwitch && isEyeView(currentView)
      && currentView != VIEW_DANCE) {
    if (idleBlinkNext == 0) {
      lastIdleBlink = millis();
      idleBlinkNext = (esp_random() % 5000) + 3000;
    }
    if (millis() - lastIdleBlink >= idleBlinkNext) {
      idleBlink();
    }
  }
}
