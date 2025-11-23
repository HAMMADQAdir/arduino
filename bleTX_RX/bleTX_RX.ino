/*
 * ESP32 BLE Server for Two-Way Communication
 *
 * *** UPDATED CODE ***
 * This version adds a "BLE2902" descriptor to
 * force the nRF Connect app to show the "Notify" button.
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h> // <-- Add this new include file
const int ECG_PIN = 35;
// You can generate new UUIDs here: https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_TX "beb5483e-36e1-4688-b7f5-ea07361b26a8" // ESP32 -> Phone (Notify)
#define CHARACTERISTIC_UUID_RX "a0a7e75e-579e-48a0-8339-1b5e8d1b714b" // Phone -> ESP32 (Write)

BLECharacteristic *pTxCharacteristic; // Pointer to the TX Characteristic
bool deviceConnected = false;
uint32_t txValue = 0; // A counter value to send

// --- Callback for when a client (phone) connects or disconnects ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Phone Connected");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Phone Disconnected");
      pServer->getAdvertising()->start(); // Restart advertising
    }
};

// --- Callback for when data is RECEIVED from the phone ---
class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue(); // Use Arduino "String"
      if (rxValue.length() > 0) {
        Serial.print("Received: ");
        Serial.println(rxValue);
      }
    }
};


// --- Setup Function ---
void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE server...");

  BLEDevice::init("ESP32_TX_RX");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // 4. Create the TX (Transmit) Characteristic (ESP32 to Phone)
  pTxCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_TX,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  
  // **********************************
  // *** THIS IS THE NEW, IMPORTANT LINE ***
  pTxCharacteristic->addDescriptor(new BLE2902());
  // **********************************


  // 5. Create the RX (Receive) Characteristic (Phone to ESP32)
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID_RX,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  pRxCharacteristic->setCallbacks(new MyRxCallbacks());

  // 6. Start the Service
  pService->start();

  // 7. Start Advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("Device advertising. Waiting for client..."); 
  analogSetPinAttenuation(ECG_PIN, ADC_11db); 
  Serial.println("Starting ECG Reader...");
 
}


// --- Main Loop ---
void loop() {
  if (deviceConnected) {
    // --- THIS IS THE SENDING PART ---
    int raw = analogRead(ECG_PIN); // value 0–4095 // Print for Serial Plotter (just the number) String txString = String(raw);
    String txString = String(raw);
    pTxCharacteristic->setValue(txString.c_str());
    pTxCharacteristic->notify();
    
    Serial.print("Sending value: ");
    Serial.println(txString);

    txValue++; // Increment the counter
  }
  delay(1000); 
}