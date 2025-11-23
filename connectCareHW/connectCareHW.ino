#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h" // REQUIRED: Calculating algorithm

// ================= CONFIGURATION =================

// --- Pin Definitions ---
#define ECG_PIN 34
#define STATUS_LED 2

// --- BLE UUIDs ---
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_ECG_UUID          "beb5483e-36e1-4688-b7f5-ea07361b26a8" 
#define CHAR_HR_UUID           "885e63c2-8e1a-4f7d-9374-c6307392e604" 
#define CHAR_SPO2_UUID         "e37b7276-284d-4351-9429-91d75b33d603" 
#define CHAR_CONTROL_UUID      "19b10000-e8f2-537e-4f6c-d104768a1214" 

int activeSensorMode = 0; // 0 = MAX30102, 1 = ECG

// ================= GLOBAL OBJECTS =================

BLEServer* pServer = NULL;
BLECharacteristic* pECGCharacteristic = NULL;
BLECharacteristic* pHRCharacteristic = NULL;
BLECharacteristic* pSpO2Characteristic = NULL;
BLECharacteristic* pControlCharacteristic = NULL;
bool deviceConnected = false;

MAX30105 particleSensor;

// --- SpO2/HR Variables ---
#define MAX_BRIGHTNESS 255

// Buffers for the algorithm
uint32_t irBuffer[100]; // Infrared LED sensor data
uint32_t redBuffer[100];  // Red LED sensor data
int32_t bufferLength; // data length
int32_t spo2; // SPO2 value
int8_t validSPO2; // Indicator to show if SPO2 calculation is valid
int32_t heartRate; // Heart rate value
int8_t validHeartRate; // Indicator to show if heart rate calculation is valid

// ECG Variables
const int ECG_BUFFER_SIZE = 20; 
uint8_t ecgBuffer[ECG_BUFFER_SIZE];
int ecgIndex = 0;

// ================= CALLBACKS =================

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Client Connected");
      digitalWrite(STATUS_LED, HIGH); 
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Client Disconnected");
      digitalWrite(STATUS_LED, LOW);
      pServer->getAdvertising()->start(); 
    }
};

class ControlCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue(); 
      if (value.length() > 0) {
        char command = value[0];
        if (command == '0') {
           activeSensorMode = 0;
           Serial.println("Switched to Mode 0 (MAX30102)");
        }
        else if (command == '1') {
           activeSensorMode = 1;
           Serial.println("Switched to Mode 1 (ECG)");
        }
      }
    }
};

// ================= SETUP FUNCTIONS =================

void setupBLE() {
  BLEDevice::init("ESP32_Vitals_Monitor");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pECGCharacteristic = pService->createCharacteristic(CHAR_ECG_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pECGCharacteristic->addDescriptor(new BLE2902());
  pHRCharacteristic = pService->createCharacteristic(CHAR_HR_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pHRCharacteristic->addDescriptor(new BLE2902());
  pSpO2Characteristic = pService->createCharacteristic(CHAR_SPO2_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pSpO2Characteristic->addDescriptor(new BLE2902());
  pControlCharacteristic = pService->createCharacteristic(CHAR_CONTROL_UUID, BLECharacteristic::PROPERTY_WRITE);
  pControlCharacteristic->setCallbacks(new ControlCallback());

  pService->start();
  pServer->getAdvertising()->start();
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(ECG_PIN, INPUT); 
  
  // Initialize Sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not found. Checking wiring...");
  } else {
    Serial.println("MAX30102 Initialized");
    
    // SENSOR CONFIGURATION FOR SPO2 (Crucial!)
    byte ledBrightness = 60; // Options: 0=Off to 255=50mA
    byte sampleAverage = 4; // Options: 1, 2, 4, 8, 16, 32
    byte ledMode = 2; // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
    int sampleRate = 100; // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
    int pulseWidth = 411; // Options: 69, 118, 215, 411
    int adcRange = 4096; // Options: 2048, 4096, 8192, 16384

    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); 
  }

  setupBLE();
}

// ================= MAIN LOOP =================

void loop() {
  if (deviceConnected) {
    
    // ---------------- MODE 0: MAX30102 (Real Calculation) ----------------
    if (activeSensorMode == 0) {
      
      bufferLength = 100; // buffer length of 100 stores 4 seconds of samples running at 25sps

      // 1. FIRST RUN: Gather 100 samples (Takes ~1 second)
      // Note: This blocks BLE for 1 sec initially. Acceptable for startup.
      for (byte i = 0 ; i < bufferLength ; i++) {
        while (particleSensor.available() == false) 
          particleSensor.check(); // Wait for new data

        redBuffer[i] = particleSensor.getRed();
        irBuffer[i] = particleSensor.getIR();
        particleSensor.nextSample(); 
      }

      // 2. Calculate initial values
      maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

      // 3. CONTINUOUS LOOP
      while (activeSensorMode == 0 && deviceConnected) {
        
        // Dump the first 25 sets of samples in the memory and shift the last 75 sets to the top
        for (byte i = 25; i < 100; i++) {
          redBuffer[i - 25] = redBuffer[i];
          irBuffer[i - 25] = irBuffer[i];
        }

        // Take 25 new samples
        for (byte i = 75; i < 100; i++) {
          while (particleSensor.available() == false) 
             particleSensor.check(); 

          redBuffer[i] = particleSensor.getRed();
          irBuffer[i] = particleSensor.getIR();
          particleSensor.nextSample(); 
        }

        // Recalculate based on the new buffer
        maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

        // --- SEND DATA TO APP ---
        // Only send if data is valid (-999 often means invalid)
        if (validHeartRate == 1 && validSPO2 == 1 && heartRate < 255 && spo2 <= 100) {
            uint8_t hrByte = (uint8_t)heartRate;
            uint8_t spo2Byte = (uint8_t)spo2;

            pHRCharacteristic->setValue(&hrByte, 1);
            pHRCharacteristic->notify();

            pSpO2Characteristic->setValue(&spo2Byte, 1);
            pSpO2Characteristic->notify();
            
            Serial.print("HR: "); Serial.print(heartRate);
            Serial.print(" SpO2: "); Serial.println(spo2);
        } else {
            Serial.println("Finger not detected or calculating...");
        }
        
        // IMPORTANT: Must occasionally yield to allow BLE stack to process
        delay(10); 
      }
    }

    // ---------------- MODE 1: ECG (Waveform) ----------------
    else if (activeSensorMode == 1) {
      int analogVal = analogRead(ECG_PIN); 
      ecgBuffer[ecgIndex] = map(analogVal, 0, 4095, 0, 255);
      ecgIndex++;

      if (ecgIndex >= ECG_BUFFER_SIZE) {
        pECGCharacteristic->setValue(ecgBuffer, ECG_BUFFER_SIZE);
        pECGCharacteristic->notify();
        ecgIndex = 0; 
        delay(4); 
      }
    }
  }
  
  // General loop delay
  delay(1); 
}