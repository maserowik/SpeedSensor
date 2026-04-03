void setup() {
  Serial.begin(115200);   // USB to PC
  Serial1.begin(115200);  // TX1 pin 18 -> D1 Mini RX
  Serial.println("Mega ready.");
}

void loop() {
  // Send a test message to D1 every 2 seconds
  static unsigned long last = 0;
  if (millis() - last >= 2000) {
    Serial1.println("HELLO FROM MEGA");
    Serial.println("Sent: HELLO FROM MEGA");
    last = millis();
  }

  // Print anything received from D1 Mini
  while (Serial.available()) {
    // nothing inbound from PC needed
  }
  while (Serial1.available()) {
    Serial.print("Received from D1: ");
    Serial.println(Serial1.readStringUntil('\n'));
  }
}