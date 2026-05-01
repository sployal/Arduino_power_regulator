#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

ThreeWire myWire(7, 6, 8); // DAT=7, CLK=6, RST=8
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("  Setting Time  ");
  lcd.setCursor(0, 1);
  lcd.print("  Please Wait.. ");
  delay(2000);

  Rtc.Begin();

  if (Rtc.GetIsWriteProtected()) {
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning()) {
    Rtc.SetIsRunning(true);
  }

  // FORCE SET TIME FROM YOUR PC CLOCK
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Rtc.SetDateTime(compiled);

  delay(500);

  // Confirm on LCD
  lcd.clear();
  if (Rtc.IsDateTimeValid()) {
    lcd.setCursor(0, 0);
    lcd.print("  Time Set OK!  ");
    lcd.setCursor(0, 1);
    lcd.print(" Upload Code 2! ");
    Serial.println("SUCCESS: Time set correctly!");
    Serial.print("Date: ");
    Serial.print(compiled.Day());   Serial.print("/");
    Serial.print(compiled.Month()); Serial.print("/");
    Serial.println(compiled.Year());
    Serial.print("Time: ");
    Serial.print(compiled.Hour());   Serial.print(":");
    Serial.print(compiled.Minute()); Serial.print(":");
    Serial.println(compiled.Second());
  } else {
    lcd.setCursor(0, 0);
    lcd.print("  FAILED!!      ");
    lcd.setCursor(0, 1);
    lcd.print("  Check Wiring  ");
    Serial.println("FAILED: Check wiring and battery!");
  }
}

void loop() {
  // Nothing here — just upload Code 2 after this works
}