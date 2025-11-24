#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>

// ================= CONFIGURATION =================

// 1. Wi-Fi Credentials
const char* ssid = "Sadique Reyaz"; // Must be 2.4GHz
const char* password = "88888888";
const int serverPort = 8080;

// 2. Microphone Pins
#define I2S_WS 15    // Word Select (LRC)
#define I2S_SD 32    // Serial Data (DIN)
#define I2S_SCK 14   // Serial Clock (BCLK)
#define I2S_PORT I2S_NUM_0

// ================= GLOBALS =================

WiFiServer audioServer(serverPort);
WiFiClient audioClient;

// ================= SETUP FUNCTIONS =================

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000, // 16kHz Sample Rate (Standard Voice Quality)
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // 16-bit resolution
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // Requires L/R pin grounded
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. Connect to Wi-Fi
  Serial.println("\nConnecting to Wi-Fi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n[SUCCESS] Wi-Fi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // <--- Use this IP in your Android App

  // 2. Start TCP Server
  audioServer.begin();
  Serial.println("Audio Server Started on Port 8080");

  // 3. Start Microphone Driver
  setupI2S();
  Serial.println("Microphone Initialized.");
}

// ================= MAIN LOOP =================

void loop() {
  // 1. Check for new client connection
  if (!audioClient || !audioClient.connected()) {
    audioClient = audioServer.available(); // Listen for incoming connection
    if(audioClient) {
      Serial.println("Client Connected! Streaming Audio...");
    }
  }

  // 2. Stream Audio if connected
  if (audioClient && audioClient.connected()) {
    // Buffer to hold audio chunk
    // 512 bytes = 256 samples (16-bit)
    char audioBuffer[512]; 
    size_t bytesRead = 0;

    // Read raw data from I2S Microphone
    i2s_read(I2S_PORT, &audioBuffer, sizeof(audioBuffer), &bytesRead, portMAX_DELAY);
    
    // Send raw data to Android via TCP
    if (bytesRead > 0) {
      audioClient.write((const uint8_t*)audioBuffer, bytesRead);
    }
  }
}