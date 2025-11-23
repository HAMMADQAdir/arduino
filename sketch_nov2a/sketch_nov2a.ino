#define ECG_PIN 4  // AD8232 Output connected to GPIO34

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("🫀 ECG Sensor Started...");
}

void loop() {
  int ecgValue = analogRead(ECG_PIN); // Read ECG analog signal (0–4095)

  // Print only the value (important for Serial Plotter)
  Serial.println(ecgValue);

  delay(4); // ~250 samples per second (1000/4 = 250 Hz)
}
