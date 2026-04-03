void setup() {
  Serial.begin(115200);
  Serial.println("D1 ready.");
}

void loop() {
  // Print anything received from Mega
  while (Serial.available()) {
    Serial.print("Received from Mega: ");
    Serial.println(Serial.readStringUntil('\n'));
  }

  // Send a reply every 3 seconds
  static unsigned long last = 0;
  if (millis() - last >= 3000) {
    Serial.println("HELLO FROM D1");
    last = millis();
  }
}