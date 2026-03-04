// =================================
//   Speed Sensor System
//   Keyence LR-TB5000C  (Right / Pin 2)
//   Keyence LR-TB5000C  (Left  / Pin 3)
//   20x4 I2C LCD Display (optional)
//   Wemos D1 Mini WiFi on Serial1
//   24VDC supply required for sensors
//   Signal wires via voltage divider to Arduino
// =================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================================
//   Pin Assignments
// ================================
const int sensorPin1 = 2;  // Right sensor -- LR-TB5000C (black wire)
const int sensorPin2 = 3;  // Left sensor  -- LR-TB5000C (black wire)

// ================================
//   Configuration
// ================================
const float SENSOR_DISTANCE        = 1.0;       // Distance between sensors in meters -- measure accurately
const unsigned long TIMEOUT_US     = 5000000UL; // 5 second timeout if only one sensor fires
const unsigned long MIN_DELTA_US   = 7000;      // 7ms minimum -- matches LR-TB5000C response time
const unsigned long MIN_TRIGGER_US = 3000;      // 3ms per-sensor debounce

// ================================
//   Sensor State
// ================================
volatile unsigned long startTime   = 0;
volatile bool          measuring   = false;
volatile int           firstSensor = 0;

volatile unsigned long lastTriggerTime1 = 0;
volatile unsigned long lastTriggerTime2 = 0;

// Result transfer from ISR to loop()
volatile bool  resultReady     = false;
volatile float resultSpeed     = 0.0;
volatile int   resultDirection = 0;  // 1 = Right->Left, 2 = Left->Right, 0 = Unknown

// Measurement counter
int measurementCount = 0;

// ================================
//   LCD Config -- 20x4 I2C
// ================================
LiquidCrystal_I2C lcd(0x27, 20, 4);  // Change 0x27 if your LCD has a different I2C address
bool lcdAvailable = false;

// ================================
//   Helper -- send to Serial,
//   Serial1 (Wemos) and flush both
// ================================
void printAll(String msg, bool newline = true) {
  if (newline) {
    Serial.println(msg);
    Serial1.println(msg);
  } else {
    Serial.print(msg);
    Serial1.print(msg);
  }
}

// ================================
//   Safe LCD Detection
//   Uses Wire timeout so it never
//   hangs if LCD is not connected
// ================================
bool detectLCD() {
  Wire.beginTransmission(0x27);
  Wire.write(0);
  byte error = Wire.endTransmission();
  return (error == 0);
}

// ================================
//   SETUP
// ================================
void setup() {
  Serial.begin(115200);   // USB Serial Monitor
  Serial1.begin(115200);  // Wemos D1 Mini on TX1 (pin 18)
  delay(500);

  pinMode(sensorPin1, INPUT_PULLUP);
  pinMode(sensorPin2, INPUT_PULLUP);
  delay(250);

  // --- Serial startup banner first ---
  printAll("=================================");
  printAll("  Speed Sensor System Starting  ");
  printAll("  Keyence LR-TB5000C x2         ");
  printAll("=================================");
  printAll("Initializing...");
  Serial.flush();
  Serial1.flush();

  // --- Safely initialize I2C with timeout ---
  Serial.print("Checking for LCD...");
  Serial1.print("Checking for LCD...");
  Serial.flush();
  Serial1.flush();

  Wire.begin();
  Wire.setWireTimeout(3000, true);
  delay(50);

  lcdAvailable = detectLCD();
  Wire.clearWireTimeoutFlag();

  if (lcdAvailable) {
    printAll("LCD detected OK");
    lcd.begin(20, 4);
    lcd.backlight();
    lcd.setCursor(0, 0); lcd.print("  Speed Sensor Sys  ");
    lcd.setCursor(0, 1); lcd.print("  Keyence LR-T x2   ");
    lcd.setCursor(0, 2); lcd.print("  Warming up...     ");
    lcd.setCursor(0, 3); lcd.print("                    ");
  } else {
    printAll("No LCD found - Serial only");
  }

  Serial.flush();
  Serial1.flush();

  // --- Warm-up countdown ---
  Serial.print("Warming up sensors");
  Serial1.print("Warming up sensors");
  Serial.flush();
  Serial1.flush();

  for (int i = 0; i < 3; i++) {
    delay(1000);
    Serial.print(".");
    Serial1.print(".");
    Serial.flush();
    Serial1.flush();

    if (lcdAvailable) {
      lcd.setCursor(0, 2);
      if (i == 0) lcd.print("  Warming up .      ");
      if (i == 1) lcd.print("  Warming up ..     ");
      if (i == 2) lcd.print("  Warming up ...    ");
    }
  }

  Serial.println();
  Serial1.println();
  Serial.flush();
  Serial1.flush();

  // --- Sanity check sensor pins before arming interrupts ---
  printAll("Checking sensor states...");
  Serial.flush();
  Serial1.flush();

  int pin1State = digitalRead(sensorPin1);
  int pin2State = digitalRead(sensorPin2);

  if (pin1State == LOW) {
    printAll("[WARNING] Right sensor (LR-TB5000C) reading LOW at idle!");
    printAll("          Check 24VDC power supply and voltage divider wiring.");
    if (lcdAvailable) {
      lcd.setCursor(0, 2); lcd.print("WARN: Right sensor! ");
      lcd.setCursor(0, 3); lcd.print("Check wiring/power  ");
    }
  }

  if (pin2State == LOW) {
    printAll("[WARNING] Left sensor (LR-TB5000C) reading LOW at idle!");
    printAll("          Check 24VDC power supply and voltage divider wiring.");
    if (lcdAvailable) {
      lcd.setCursor(0, 2); lcd.print("WARN: Left sensor!  ");
      lcd.setCursor(0, 3); lcd.print("Check wiring/power  ");
    }
  }

  if (pin1State == HIGH && pin2State == HIGH) {
    printAll("Sensor states OK (both HIGH at idle)");
    if (lcdAvailable) {
      lcd.setCursor(0, 2); lcd.print("  Sensors OK        ");
      lcd.setCursor(0, 3); lcd.print("                    ");
    }
  }

  Serial.flush();
  Serial1.flush();

  if (lcdAvailable) delay(1500);

  // --- Ready ---
  printAll("Sensors ready! Waiting for object...");
  printAll("=================================");
  Serial.flush();
  Serial1.flush();

  if (lcdAvailable) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("  ** SYSTEM READY **");
    lcd.setCursor(0, 1); lcd.print("  Waiting for       ");
    lcd.setCursor(0, 2); lcd.print("  object...         ");
    lcd.setCursor(0, 3); lcd.print("                    ");
  }

  // Attach interrupts AFTER warm-up so startup noise is ignored
  attachInterrupt(digitalPinToInterrupt(sensorPin1), rightTriggered, FALLING);
  attachInterrupt(digitalPinToInterrupt(sensorPin2), leftTriggered, FALLING);
}

// ================================
//   MAIN LOOP
// ================================
void loop() {

  // --- Timeout check ---
  if (measuring && (micros() - startTime > TIMEOUT_US)) {
    measuring   = false;
    firstSensor = 0;
    printAll("[Timeout] Measurement reset -- only one sensor triggered.");
    Serial.flush();
    Serial1.flush();

    if (lcdAvailable) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("[Timeout] Reset     ");
      lcd.setCursor(0, 1); lcd.print("One sensor only     ");
      lcd.setCursor(0, 2); lcd.print("Waiting for object  ");
      lcd.setCursor(0, 3); lcd.print("                    ");
    }
  }

  // --- Print result safely outside of ISR ---
  if (resultReady) {
    resultReady = false;

    measurementCount++;

    // Build timestamp
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    seconds = seconds % 60;

    // Build full output line
    String line = "";
    line += "[";
    if (minutes < 10) line += "0";
    line += String(minutes) + ":";
    if (seconds < 10) line += "0";
    line += String(seconds) + "]  ";
    line += "[#" + String(measurementCount) + "]  ";
    line += "Speed: " + String(resultSpeed, 3) + " m/s  |  ";
    line += String(resultSpeed * 3.6, 2) + " km/h  |  ";
    line += String(resultSpeed * 2.237, 2) + " mph  |  ";

    if (resultDirection == 1)      line += "Direction: Right -> Left";
    else if (resultDirection == 2) line += "Direction: Left -> Right";
    else                           line += "Direction: Unknown";

    // Send to Serial Monitor and Wemos
    Serial.println(line);
    Serial1.println(line);
    Serial.flush();
    Serial1.flush();

    // --- LCD output ---
    if (lcdAvailable) {
      lcd.clear();

      // Row 0 -- Speed in m/s and km/h
      lcd.setCursor(0, 0);
      lcd.print("Spd:");
      lcd.print(resultSpeed, 2);
      lcd.print("m/s ");
      lcd.print(resultSpeed * 3.6, 1);
      lcd.print("k");

      // Row 1 -- Speed in mph and direction
      lcd.setCursor(0, 1);
      lcd.print("mph:");
      lcd.print(resultSpeed * 2.237, 2);
      lcd.print("  ");
      if (resultDirection == 1)      lcd.print("R->L");
      else if (resultDirection == 2) lcd.print("L->R");
      else                           lcd.print("????");

      // Row 2 -- Measurement count
      lcd.setCursor(0, 2);
      lcd.print("Measurement #:");
      lcd.print(measurementCount);

      // Row 3 -- Timestamp
      lcd.setCursor(0, 3);
      lcd.print("Time: ");
      if (minutes < 10) lcd.print("0");
      lcd.print(minutes);
      lcd.print(":");
      if (seconds < 10) lcd.print("0");
      lcd.print(seconds);
    }
  }
}

// ================================
//   INTERRUPT HANDLERS
// ================================
void rightTriggered() {
  unsigned long now = micros();
  if (now - lastTriggerTime1 >= MIN_TRIGGER_US) {
    lastTriggerTime1 = now;
    handleTrigger(1);
  }
}

void leftTriggered() {
  unsigned long now = micros();
  if (now - lastTriggerTime2 >= MIN_TRIGGER_US) {
    lastTriggerTime2 = now;
    handleTrigger(2);
  }
}

// ================================
//   SPEED / DIRECTION CALCULATION
// ================================
void handleTrigger(int sensorID) {

  if (!measuring) {
    // First sensor hit -- start timing
    measuring   = true;
    firstSensor = sensorID;
    startTime   = micros();
    return;
  }

  // Second sensor hit -- stop timing
  unsigned long endTime   = micros();
  unsigned long deltaTime = endTime - startTime;

  // Ignore if too fast -- must meet LR-TB5000C minimum response time (7ms)
  if (deltaTime < MIN_DELTA_US) return;

  // Calculate speed
  resultSpeed = SENSOR_DISTANCE / (deltaTime / 1000000.0);

  // Determine direction
  if      (firstSensor == 1 && sensorID == 2) resultDirection = 1;  // Right -> Left
  else if (firstSensor == 2 && sensorID == 1) resultDirection = 2;  // Left -> Right
  else                                         resultDirection = 0;  // Unknown

  // Flag result ready for loop() to print
  resultReady = true;

  // Reset for next measurement
  measuring   = false;
  firstSensor = 0;
}
