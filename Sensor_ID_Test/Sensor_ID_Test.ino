// =================================
//   Sensor Identification Test
//   Keyence LR-TB5000C x2
//   Use this to identify which
//   sensor is Left and which is Right
//   Open Serial Monitor at 115200
//   Block each sensor one at a time
//   and watch which pin fires
// =================================

const int sensorPin1 = 2;  // Currently labeled RIGHT
const int sensorPin2 = 3;  // Currently labeled LEFT

bool lastPin1 = HIGH;
bool lastPin2 = HIGH;

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(sensorPin1, INPUT_PULLUP);
  pinMode(sensorPin2, INPUT_PULLUP);
  delay(250);

  Serial.println("=================================");
  Serial.println("  Sensor Identification Test    ");
  Serial.println("  Keyence LR-TB5000C x2         ");
  Serial.println("=================================");
  Serial.println("Block each sensor one at a time");
  Serial.println("and watch which pin fires below.");
  Serial.println("=================================");
  Serial.flush();
}

void loop() {
  bool pin1 = digitalRead(sensorPin1);
  bool pin2 = digitalRead(sensorPin2);

  if (pin1 == LOW && lastPin1 == HIGH) {
    Serial.println(">>> Pin 2 triggered (currently labeled RIGHT)");
    Serial.flush();
  }

  if (pin2 == LOW && lastPin2 == HIGH) {
    Serial.println(">>> Pin 3 triggered (currently labeled LEFT)");
    Serial.flush();
  }

  lastPin1 = pin1;
  lastPin2 = pin2;
}
