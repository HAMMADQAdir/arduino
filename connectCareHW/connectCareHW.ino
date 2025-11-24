#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ---------------- BLE UUIDs ----------------
#define SERVICE_UUID     "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define HR_CHAR_UUID     "885e63c2-8e1a-4f7d-9374-c6307392e604"
#define SPO2_CHAR_UUID   "e37b7276-284d-4351-9429-91d75b33d603"
#define ECG_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ---------------- LED ----------------
#define LED_PIN 2

// ---------------- ECG ----------------
#define ECG_PIN 34
#define ECG_PACKET_SIZE 20
#define ECG_SAMPLING_HZ 250
#define ECG_SAMPLE_US (1000000UL / ECG_SAMPLING_HZ)

uint8_t ecgPacket[ECG_PACKET_SIZE];
uint8_t ecgIndex = 0;
unsigned long lastEcgMicros = 0;

// ---------------- MAX30102 ----------------
MAX30105 particleSensor;

const uint8_t bufferLength = 100;
uint32_t irBuffer[bufferLength];
uint32_t redBuffer[bufferLength];

int32_t spo2 = 0;
int8_t validSPO2 = 0;

int32_t heartRate = 0;
int8_t validHeartRate = 0;

uint8_t lastHR = 0;
uint8_t lastSPO2 = 0;

bool bufferReady = false;
int sampleCount = 0;

// ---------------- BLE ----------------
BLECharacteristic* hrChar;
BLECharacteristic* spo2Char;
BLECharacteristic* ecgChar;

bool deviceConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* p) { deviceConnected = true; }
  void onDisconnect(BLEServer* p) { deviceConnected = false; p->startAdvertising(); }
};

// ---------------- SENSOR SETUP ----------------
void setupSensor() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 NOT FOUND!");
    while (1);
  }

  particleSensor.setup(60, 4, 2, 100, 411, 4096);
  Serial.println("MAX30102 Ready");
}

// ---------------- INITIAL BUFFER FILL ----------------
void fillInitialBuffer() {
  Serial.println("Filling initial MAX30102 buffer...");

  for (int i = 0; i < bufferLength; i++) {
    while (!particleSensor.available()) particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i]  = particleSensor.getIR();
    particleSensor.nextSample();
  }

  maxim_heart_rate_and_oxygen_saturation(
    irBuffer, bufferLength,
    redBuffer,
    &spo2, &validSPO2,
    &heartRate, &validHeartRate
  );

  bufferReady = true;
  Serial.println("Initial buffer filled!");
}

// ---------------- PROCESS MAX30102 ----------------
void processSensor() {
  if (!particleSensor.available()) {
    particleSensor.check();
    return;
  }

  for (int i = 1; i < bufferLength; i++) {
    redBuffer[i - 1] = redBuffer[i];
    irBuffer[i - 1]  = irBuffer[i];
  }

  redBuffer[bufferLength - 1] = particleSensor.getRed();
  irBuffer[bufferLength - 1]  = particleSensor.getIR();
  particleSensor.nextSample();

  sampleCount++;

  if (sampleCount >= 25) {
    sampleCount = 0;

    maxim_heart_rate_and_oxygen_saturation(
      irBuffer, bufferLength,
      redBuffer,
      &spo2, &validSPO2,
      &heartRate, &validHeartRate
    );
  }
}

// ---------------- PROCESS ECG ----------------
void processECG() {
  unsigned long now = micros();
  if (now - lastEcgMicros < ECG_SAMPLE_US) return;

  lastEcgMicros = now;

  int raw = analogRead(ECG_PIN);
  uint8_t mapped = (uint8_t)map(raw, 0, 4095, 0, 255);

  ecgPacket[ecgIndex++] = mapped;

  if (ecgIndex >= ECG_PACKET_SIZE) {
    if (deviceConnected) {
      ecgChar->setValue(ecgPacket, ECG_PACKET_SIZE);
      ecgChar->notify();
    }

    // Blink LED for ECG change
    digitalWrite(LED_PIN, HIGH);
    delay(10);
    digitalWrite(LED_PIN, LOW);

    ecgIndex = 0;
  }
}

// ---------------- BLE SETUP ----------------
void setupBLE() {
  BLEDevice::init("ESP32_HR_SPO2_ECG");

  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(SERVICE_UUID);

  hrChar = service->createCharacteristic(HR_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  hrChar->addDescriptor(new BLE2902());

  spo2Char = service->createCharacteristic(SPO2_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  spo2Char->addDescriptor(new BLE2902());

  ecgChar = service->createCharacteristic(ECG_CHAR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  ecgChar->addDescriptor(new BLE2902());

  service->start();

  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::getAdvertising()->start();
}

// ---------------- SEND BLE DATA ----------------
unsigned long lastSend = 0;

void sendData() {
  if (!deviceConnected) return;
  if (millis() - lastSend < 1000) return;
  lastSend = millis();

  // ---------------- HR ----------------
  if (validHeartRate && heartRate > 40 && heartRate < 200) {
    uint8_t hrByte = (uint8_t)heartRate;

    if (hrByte != lastHR) {
      digitalWrite(LED_PIN, HIGH);
      delay(30);
      digitalWrite(LED_PIN, LOW);
      Serial.println("HR changed → LED blink");
    }

    hrChar->setValue(&hrByte, 1);
    hrChar->notify();

    lastHR = hrByte;
    Serial.print("HR: "); Serial.println(hrByte);
  }

  // ---------------- SPO2 ----------------
  if (validSPO2 && spo2 >= 70 && spo2 <= 100) {
    uint8_t spo2Byte = (uint8_t)spo2;

    if (spo2Byte != lastSPO2) {
      digitalWrite(LED_PIN, HIGH);
      delay(30);
      digitalWrite(LED_PIN, LOW);
      Serial.println("SpO2 changed → LED blink");
    }

    spo2Char->setValue(&spo2Byte, 1);
    spo2Char->notify();

    lastSPO2 = spo2Byte;
    Serial.print("SpO2: "); Serial.println(spo2Byte);
  }
}

// ---------------- MAIN ----------------
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(ECG_PIN, INPUT);

  setupSensor();
  setupBLE();
  fillInitialBuffer();

  lastEcgMicros = micros();
}

void loop() {
  processSensor();   // HR + SpO₂
  processECG();      // ECG sampling
  sendData();        // BLE HR + SpO₂
}
