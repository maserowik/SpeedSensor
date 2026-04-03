// =================================
//   Speed Sensor System - FINAL GUI SYNC
//   Distance: 1.254m 
//   Feature: Auto-Detect Logic + Smart Timeout
//   Fixed: Middle GUI Direction Box Alignment
// =================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int sensorPin1 = 2; // Right sensor
const int sensorPin2 = 3; // Left sensor

// ================================
//   Configuration
// ================================
const float SENSOR_DISTANCE        = 1.254; 
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

volatile bool  resultReady     = false;
volatile float resultSpeed     = 0.0;
volatile int   resultDirection = 0; 

int measurementCount = 0;
LiquidCrystal_I2C lcd(0x27, 20, 4);
bool lcdAvailable = false;

// ================================
//   Helper -- maintains Wemos Handshake
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
  printAll("  Auto-Detecting Sensor Logic   ");
  printAll("=================================");
  
  Wire.begin();
  Wire.setWireTimeout(3000, true);
  lcdAvailable = detectLCD();
  Wire.clearWireTimeoutFlag();

  printAll("Calibrating idle states...");
  for (int i = 0; i < 10; i++) {
    delay(1000);
    Serial.print(".");
    Serial1.print(".");
    Serial.flush();
    Serial1.flush();
  }
  Serial.println();

  int idle1 = digitalRead(sensorPin1);
  int idle2 = digitalRead(sensorPin2);

  int mode1 = (idle1 == HIGH) ? FALLING : RISING;
  attachInterrupt(digitalPinToInterrupt(sensorPin1), rightTriggered, mode1);

  int mode2 = (idle2 == HIGH) ? FALLING : RISING;
  attachInterrupt(digitalPinToInterrupt(sensorPin2), leftTriggered, mode2);

  printAll("Sensor 1 Idle: " + String(idle1 == HIGH ? "HIGH (C)" : "LOW (CL)"));
  printAll("Sensor 2 Idle: " + String(idle2 == HIGH ? "HIGH (C)" : "LOW (CL)"));
  printAll("Sensors ready! Waiting for object...");
  printAll("=================================");
}

// ================================
//   MAIN LOOP
// ================================
void loop() {

  if (measuring && (micros() - startTime > TIMEOUT_US)) {
    if (firstSensor == 1) {
      printAll("[Timeout] Right triggered -- Missing Left sensor signal");
    } else if (firstSensor == 2) {
      printAll("[Timeout] Left triggered -- Missing Right sensor signal");
    }
    measuring   = false;
    firstSensor = 0;
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

    // Text formatting for History Table and Status Bar
    // We swapped the logic here to match the ID swap below
    if (resultDirection == 1)      line += "Direction: Left -> Right";
    else if (resultDirection == 2) line += "Direction: Right -> Left";
    
    Serial.println(line);
    Serial1.println(line);
    Serial.flush();
    Serial1.flush();
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
    measuring   = true;
    firstSensor = sensorID;
    startTime   = micros();
    return;
  }

  unsigned long endTime   = micros();
  unsigned long deltaTime = endTime - startTime;

  if (sensorID == firstSensor) {
    startTime = endTime;
    return;
  }

  if (deltaTime < MIN_DELTA_US) {
    measuring = false;
    firstSensor = 0;
    return;
  }

  resultSpeed = SENSOR_DISTANCE / (deltaTime / 1000000.0);
  
  // --- SYNCED DIRECTION LOGIC ---
  // If Sensor 2 (Left) was hit first, the object is moving Left -> Right.
  // We set resultDirection to 1 because your Wemos GUI box expects ID 1 for "RIGHT"
  if (firstSensor == 2) {
    resultDirection = 1; 
  } else {
    resultDirection = 2; 
  }

  resultReady = true;
  measuring   = false;
  firstSensor = 0;
}