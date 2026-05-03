#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <Keypad.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
ThreeWire myWire(7, 6, 8);
RtcDS1302<ThreeWire> Rtc(myWire);

const int RELAY1_PIN = 13;
const int RELAY2_PIN = A1;
const int LDR_PIN    = A0;

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {9, 10, 11, 12};
byte colPins[COLS] = {5,  4,  3,  2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ── Relay 1 (schedule) ───────────────────────────────────────────────────────
int  onHour  = -1, onMin  = -1;
int  offHour = -1, offMin = -1;
bool scheduleSet = false;
bool relay1State = false;

// ── Relay 2 (LDR) ────────────────────────────────────────────────────────────
// Hysteresis band: relay turns ON below (threshold - HYST_BAND),
//                  relay turns OFF above (threshold + HYST_BAND)
const int HYST_BAND   = 20;
int  ldrThreshold     = 400;
bool relay2State      = false;

// ── LDR smoothing (simple moving average over 5 samples) ─────────────────────
const int  LDR_SAMPLES = 5;
int        ldrBuffer[LDR_SAMPLES];
int        ldrIndex     = 0;
bool       ldrReady     = false;   // true once buffer is fully populated

int readSmoothedLDR() {
  ldrBuffer[ldrIndex] = analogRead(LDR_PIN);
  ldrIndex = (ldrIndex + 1) % LDR_SAMPLES;
  if (ldrIndex == 0) ldrReady = true;

  int count = ldrReady ? LDR_SAMPLES : ldrIndex;
  long sum  = 0;
  for (int i = 0; i < count; i++) sum += ldrBuffer[i];
  return (int)(sum / count);
}

// ── Menu ─────────────────────────────────────────────────────────────────────
enum State {
  NORMAL,
  SET_ON_HOUR, SET_ON_MIN, SET_OFF_HOUR, SET_OFF_MIN,
  CONFIRM,
  SET_THRESHOLD
};
State  menuState   = NORMAL;
String inputBuffer = "";

// ── Helpers ──────────────────────────────────────────────────────────────────

void printPrompt(const char* line0, const char* line1 = "") {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line0);
  lcd.setCursor(0, 1); lcd.print(line1);
}

void applyRelay1() {
  digitalWrite(RELAY1_PIN, relay1State ? HIGH : LOW);
}

void applyRelay2() {
  // Relay module is active-LOW: LOW = relay ON, HIGH = relay OFF
  digitalWrite(RELAY2_PIN, relay2State ? LOW : HIGH);
}

void handleBackspace(const char* promptLine1, const char* labelPrefix) {
  if (inputBuffer.length() > 0) {
    inputBuffer.remove(inputBuffer.length() - 1);
  }
  lcd.setCursor(0, 1);
  char buf[17];
  if (inputBuffer.length() > 0) {
    snprintf(buf, sizeof(buf), "%s=%s             ", labelPrefix, inputBuffer.c_str());
  } else {
    snprintf(buf, sizeof(buf), "%-16s", promptLine1);
  }
  lcd.print(buf);
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(9600);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  relay1State = false;
  relay2State = false;
  applyRelay1();
  applyRelay2();

  // Pre-fill LDR smoothing buffer with first real reading
  int firstRead = analogRead(LDR_PIN);
  for (int i = 0; i < LDR_SAMPLES; i++) ldrBuffer[i] = firstRead;
  ldrReady = true;

  lcd.init();
  lcd.backlight();
  printPrompt("  Light Monitor ", "  Please Wait.. ");
  delay(2000);

  Rtc.Begin();
  if (Rtc.GetIsWriteProtected()) Rtc.SetIsWriteProtected(false);
  if (!Rtc.GetIsRunning())       Rtc.SetIsRunning(true);
  if (!Rtc.IsDateTimeValid()) {
    printPrompt("  RTC ERROR!!   ", " Upload Code 1! ");
    while (true);
  }

  printPrompt("A=Schedule B=LDR", "D=Man.Tgl R1    ");
  delay(1800);
}

// ── Relay 1 scheduler ────────────────────────────────────────────────────────

void updateRelay1(int h, int m) {
  if (!scheduleSet) return;

  int nowMins = h * 60 + m;
  int onMins  = onHour  * 60 + onMin;
  int offMins = offHour * 60 + offMin;

  bool shouldBeOn;
  if (onMins < offMins) {
    shouldBeOn = (nowMins >= onMins && nowMins < offMins);
  } else {
    // Overnight schedule (e.g. 22:00 → 06:00)
    shouldBeOn = (nowMins >= onMins || nowMins < offMins);
  }

  if (shouldBeOn != relay1State) {
    relay1State = shouldBeOn;
    applyRelay1();
    Serial.println(relay1State ? "RELAY1 ON" : "RELAY1 OFF");
  }
}

// ── Relay 2 LDR control (with hysteresis) ────────────────────────────────────

void updateRelay2() {
  int raw = readSmoothedLDR();

  // Only switch ON when clearly dark (below lower bound)
  // Only switch OFF when clearly bright (above upper bound)
  // In between: hold current state → NO flickering at the boundary
  int lowerBound = ldrThreshold - HYST_BAND;   // e.g. 380
  int upperBound = ldrThreshold + HYST_BAND;   // e.g. 420

  bool shouldBeOn = relay2State;   // default: keep current state

  if (raw < lowerBound) {
    shouldBeOn = true;   // dark → ON
  } else if (raw > upperBound) {
    shouldBeOn = false;  // bright → OFF
  }
  // if lowerBound <= raw <= upperBound: do nothing (hold state)

  if (shouldBeOn != relay2State) {
    relay2State = shouldBeOn;
    applyRelay2();
    Serial.print("RELAY2 "); Serial.println(relay2State ? "OFF" : "ON");
  }
}

// ── Normal screen ─────────────────────────────────────────────────────────────

void showNormalScreen(RtcDateTime& now) {
  char row0[17];
  snprintf(row0, sizeof(row0), "%02d:%02d:%02d R1:%s",
           now.Hour(), now.Minute(), now.Second(),
           relay1State ? "ON " : "OFF");
  lcd.setCursor(0, 0);
  lcd.print(row0);

  char row1[17];
  snprintf(row1, sizeof(row1), "D:%02d/%02d/%02d R2:%s",
           now.Day(), now.Month(), now.Year() % 100,
           relay2State ? "OFF" : "ON ");
  lcd.setCursor(0, 1);
  lcd.print(row1);
}

// ── Keypad handler ────────────────────────────────────────────────────────────

void handleKey(char key) {
  switch (menuState) {

    // ── NORMAL ───────────────────────────────────────────────────────────────
    case NORMAL:
      if (key == 'A') {
        menuState   = SET_ON_HOUR;
        inputBuffer = "";
        printPrompt("Set ON Hour     ", "Enter HH then #:");
      } else if (key == 'B') {
        menuState   = SET_THRESHOLD;
        inputBuffer = "";
        char buf[17];
        snprintf(buf, sizeof(buf), "Cur:%4d (0-1023)", ldrThreshold);
        printPrompt("Set LDR Thresh  ", buf);
      } else if (key == 'D') {
        scheduleSet = false;
        relay1State = !relay1State;
        applyRelay1();
        printPrompt(relay1State ? "R1 ON (manual)  " : "R1 OFF (manual) ", "");
        delay(1000);
      }
      break;

    // ── SET LDR THRESHOLD ────────────────────────────────────────────────────
    case SET_THRESHOLD:
      if (key >= '0' && key <= '9') {
        if (inputBuffer.length() < 4) {
          inputBuffer += key;
          lcd.setCursor(0, 1);
          char buf[17];
          snprintf(buf, sizeof(buf), "Val=%s             ", inputBuffer.c_str());
          lcd.print(buf);
        }
      } else if (key == 'C') {
        handleBackspace("Enter val 0-1023", "Val");
      } else if (key == '#') {
        if (inputBuffer.length() == 0) break;
        int val = inputBuffer.toInt();
        if (val < 0 || val > 1023) {
          printPrompt("Invalid!(0-1023)", "Try again:      ");
          inputBuffer = "";
          break;
        }
        ldrThreshold = val;
        inputBuffer  = "";
        menuState    = NORMAL;
        char buf[17];
        snprintf(buf, sizeof(buf), "Threshold=%4d  ", ldrThreshold);
        printPrompt("LDR Thresh Saved", buf);
        Serial.print("LDR Threshold="); Serial.println(ldrThreshold);
        delay(1500);
      } else if (key == '*') {
        menuState   = NORMAL;
        inputBuffer = "";
        printPrompt("Cancelled.      ", "");
        delay(800);
      }
      break;

    // ── SET ON HOUR ──────────────────────────────────────────────────────────
    case SET_ON_HOUR:
      if (key >= '0' && key <= '9') {
        if (inputBuffer.length() < 2) {
          inputBuffer += key;
          lcd.setCursor(0, 1);
          char buf[17];
          snprintf(buf, sizeof(buf), "HH=%s             ", inputBuffer.c_str());
          lcd.print(buf);
        }
      } else if (key == 'C') {
        handleBackspace("Enter HH then #:", "HH");
      } else if (key == '#') {
        if (inputBuffer.length() == 0) break;
        onHour = inputBuffer.toInt();
        if (onHour < 0 || onHour > 23) {
          printPrompt("Invalid! (0-23) ", "Try again:      ");
          inputBuffer = "";
          break;
        }
        inputBuffer = "";
        menuState   = SET_ON_MIN;
        printPrompt("Set ON Minute   ", "Enter MM then #:");
      } else if (key == '*') {
        menuState   = NORMAL;
        inputBuffer = "";
        printPrompt("Cancelled.      ", "");
        delay(800);
      }
      break;

    // ── SET ON MINUTE ────────────────────────────────────────────────────────
    case SET_ON_MIN:
      if (key >= '0' && key <= '9') {
        if (inputBuffer.length() < 2) {
          inputBuffer += key;
          lcd.setCursor(0, 1);
          char buf[17];
          snprintf(buf, sizeof(buf), "MM=%s             ", inputBuffer.c_str());
          lcd.print(buf);
        }
      } else if (key == 'C') {
        handleBackspace("Enter MM then #:", "MM");
      } else if (key == '#') {
        if (inputBuffer.length() == 0) break;
        onMin = inputBuffer.toInt();
        if (onMin < 0 || onMin > 59) {
          printPrompt("Invalid! (0-59) ", "Try again:      ");
          inputBuffer = "";
          break;
        }
        inputBuffer = "";
        menuState   = SET_OFF_HOUR;
        printPrompt("Set OFF Hour    ", "Enter HH then #:");
      } else if (key == '*') {
        menuState   = NORMAL;
        inputBuffer = "";
        printPrompt("Cancelled.      ", "");
        delay(800);
      }
      break;

    // ── SET OFF HOUR ─────────────────────────────────────────────────────────
    case SET_OFF_HOUR:
      if (key >= '0' && key <= '9') {
        if (inputBuffer.length() < 2) {
          inputBuffer += key;
          lcd.setCursor(0, 1);
          char buf[17];
          snprintf(buf, sizeof(buf), "HH=%s             ", inputBuffer.c_str());
          lcd.print(buf);
        }
      } else if (key == 'C') {
        handleBackspace("Enter HH then #:", "HH");
      } else if (key == '#') {
        if (inputBuffer.length() == 0) break;
        offHour = inputBuffer.toInt();
        if (offHour < 0 || offHour > 23) {
          printPrompt("Invalid! (0-23) ", "Try again:      ");
          inputBuffer = "";
          break;
        }
        inputBuffer = "";
        menuState   = SET_OFF_MIN;
        printPrompt("Set OFF Minute  ", "Enter MM then #:");
      } else if (key == '*') {
        menuState   = NORMAL;
        inputBuffer = "";
        printPrompt("Cancelled.      ", "");
        delay(800);
      }
      break;

    // ── SET OFF MINUTE ───────────────────────────────────────────────────────
    case SET_OFF_MIN:
      if (key >= '0' && key <= '9') {
        if (inputBuffer.length() < 2) {
          inputBuffer += key;
          lcd.setCursor(0, 1);
          char buf[17];
          snprintf(buf, sizeof(buf), "MM=%s             ", inputBuffer.c_str());
          lcd.print(buf);
        }
      } else if (key == 'C') {
        handleBackspace("Enter MM then #:", "MM");
      } else if (key == '#') {
        if (inputBuffer.length() == 0) break;
        offMin = inputBuffer.toInt();
        if (offMin < 0 || offMin > 59) {
          printPrompt("Invalid! (0-59) ", "Try again:      ");
          inputBuffer = "";
          break;
        }
        char buf0[17], buf1[17];
        snprintf(buf0, sizeof(buf0), "ON%02d:%02d OFF%02d:%02d", onHour, onMin, offHour, offMin);
        snprintf(buf1, sizeof(buf1), "#=Save  *=Cancel");
        printPrompt(buf0, buf1);
        menuState   = CONFIRM;
        inputBuffer = "";
      } else if (key == '*') {
        menuState   = NORMAL;
        inputBuffer = "";
        printPrompt("Cancelled.      ", "");
        delay(800);
      }
      break;

    // ── CONFIRM SCHEDULE ─────────────────────────────────────────────────────
    case CONFIRM:
      if (key == '#') {
        scheduleSet = true;
        menuState   = NORMAL;
        printPrompt("Schedule Saved! ", "");
        Serial.print("ON=");   Serial.print(onHour);  Serial.print(":");
        Serial.print(onMin);   Serial.print(" OFF=");
        Serial.print(offHour); Serial.print(":"); Serial.println(offMin);
        delay(1500);
      } else if (key == '*') {
        menuState = NORMAL;
        printPrompt("Cancelled.      ", "");
        delay(800);
      }
      break;
  }
}

// ── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
  char key = keypad.getKey();
  if (key) handleKey(key);

  if (menuState == NORMAL) {
    RtcDateTime now = Rtc.GetDateTime();
    if (!Rtc.IsDateTimeValid()) {
      printPrompt("  RTC ERROR!!   ", "  Check Module  ");
      delay(1000);
      return;
    }
    updateRelay1(now.Hour(), now.Minute());
    updateRelay2();
    showNormalScreen(now);

    int raw = readSmoothedLDR();
    Serial.print("Time=");      Serial.print(now.Hour());
    Serial.print(":");          Serial.print(now.Minute());
    Serial.print(":");          Serial.print(now.Second());
    Serial.print("  Light=");   Serial.print(raw);
    Serial.print("  Thresh=");  Serial.print(ldrThreshold);
    Serial.print("  R1=");      Serial.print(relay1State ? "ON" : "OFF");
    Serial.print("  R2=");      Serial.println(relay2State ? "OFF" : "ON");
  }

  delay(300);
}
