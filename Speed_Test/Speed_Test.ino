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
const int sensorPin1 = 2; // Right sensor -- LR-TB5000C (black wire)
const int sensorPin2 = 3; // Left sensor  -- LR-TB5000C (black wire)

// ================================
//   Configuration
// ================================
const float SENSOR_DISTANCE        = 1.0; 
const unsigned long TIMEOUT_US     = 5000000UL;
const unsigned long MIN_DELTA_US   = 7000;
const unsigned long MIN_TRIGGER_US = 10000;

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
volatile int   resultDirection = 0; 

// Measurement counter
int measurementCount = 0;

// ================================
//   LCD Config -- 20x4 I2C
// ================================
LiquidCrystal_I2C lcd(0x27, 20, 4);
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
  Serial.begin(115200);
  Serial1.begin(115200); 
  delay(500);

  pinMode(sensorPin1, INPUT_PULLUP);
  pinMode(sensorPin2, INPUT_PULLUP);
  delay(250);

  printAll("=================================");
  printAll("  Speed Sensor System Starting  ");
  printAll("  Keyence LR-TB5000C x2         ");
  printAll("=================================");
  printAll("Initializing...");
  Serial.flush();
  Serial1.flush();

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

  Serial.print("Warming up sensors");
  Serial1.print("Warming up sensors");
  Serial.flush();
  Serial1.flush();

  for (int i = 0; i < 10; i++) {
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

  printAll("Checking sensor states...");
  Serial.flush();
  Serial1.flush();

  int pin1State = digitalRead(sensorPin1);
  int pin2State = digitalRead(sensorPin2);

  if (pin1State == LOW) {
    printAll("[WARNING] Right sensor (LR-TB5000C) reading LOW at idle!");
    printAll("          Check 24VDC power supply and voltage divider wiring.");
  }

  // FIXED LOGIC FOR MISMATCHED LEFT SENSOR (CL MODEL)
  if (pin2State == HIGH) { 
    printAll("[WARNING] Left sensor reading incorrectly at idle!");
    printAll("          Adjusting logic for mismatched sensor pair.");
  }

  if (pin1State == HIGH && pin2State == LOW) {
    printAll("Sensor states OK (Synced Mismatch Pair)");
  }

  Serial.flush();
  Serial1.flush();

  printAll("Sensors ready! Waiting for object...");
  printAll("=================================");
  Serial.flush();
  Serial1.flush();

  // RIGHT SENSOR (Standard FALLING logic)
  attachInterrupt(digitalPinToInterrupt(sensorPin1), rightTriggered, FALLING);
  // LEFT SENSOR (Flipped RISING logic for CL model)
  attachInterrupt(digitalPinToInterrupt(sensorPin2), leftTriggered, RISING);
}

// ================================
//   MAIN LOOP
// ================================
void loop() {

  if (measuring && (micros() - startTime > TIMEOUT_US)) {
    measuring   = false;
    firstSensor = 0;
    printAll("[Timeout] Measurement reset -- only one sensor triggered.");
    Serial.flush();
    Serial1.flush();
  }

  if (resultReady) {
    resultReady = false;
    measurementCount++;

    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    seconds = seconds % 60;

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

    Serial.println(line);
    Serial1.println(line);
    Serial.flush();
    Serial1.flush();
  }
}

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

void handleTrigger(int sensorID) {
  if (!measuring) {
    measuring   = true;
    firstSensor = sensorID;
    startTime   = micros();
    return;
  }

  unsigned long endTime   = micros();
  unsigned long deltaTime = endTime - startTime;

  if (sensorID == firstSensor) {
    startTime   = endTime;
    firstSensor = sensorID;
    return;
  }

  if (deltaTime < MIN_DELTA_US) {
    measuring   = false;
    firstSensor = 0;
    return;
  }

  resultSpeed = SENSOR_DISTANCE / (deltaTime / 1000000.0);

  if (firstSensor == 1 && sensorID == 2) resultDirection = 1;
  else                                   resultDirection = 2;

  resultReady = true;
  measuring   = false;
  firstSensor = 0;
}