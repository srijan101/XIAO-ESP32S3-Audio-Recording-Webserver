#include <Arduino.h>
#include <WiFi.h>
#include <I2S.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "config.h"
#include "webserver.h"
#include "esp_task_wdt.h"  // Add watchdog timer header

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Constants for audio recording
#define SAMPLE_RATE   16000U
#define PDM_SAMPLE_RATE 32000U  // Changed this
#define SAMPLE_BITS   16
#define WAV_HEADER_SIZE 44
#define VOLUME_GAIN   2
#define BUFFER_SIZE   2048
#define SD_CS         21

// Global variables
bool isRecording = false;
String currentLabel = "";
uint32_t audioDataSize = 0;
WAVHeader wavHeader;
File audioFile;
uint8_t *rec_buffer = NULL;
unsigned long lastFlushTime = 0;

void generate_wav_header(uint8_t *wav_header, uint32_t wav_size, uint32_t sample_rate) {
    uint32_t file_size = wav_size + WAV_HEADER_SIZE - 8;
    uint32_t byte_rate = sample_rate * 2;  // 2 bytes per sample (16-bit)
    const uint8_t set_wav_header[] = {
        'R', 'I', 'F', 'F',
        (uint8_t)(file_size & 0xFF), 
        (uint8_t)((file_size >> 8) & 0xFF), 
        (uint8_t)((file_size >> 16) & 0xFF), 
        (uint8_t)((file_size >> 24) & 0xFF),
        'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ',
        0x10, 0x00, 0x00, 0x00,
        0x01, 0x00,  // PCM format
        0x01, 0x00,  // 1 channel
        (uint8_t)(SAMPLE_RATE & 0xFF),  // Using SAMPLE_RATE directly
        (uint8_t)((SAMPLE_RATE >> 8) & 0xFF),
        (uint8_t)((SAMPLE_RATE >> 16) & 0xFF),
        (uint8_t)((SAMPLE_RATE >> 24) & 0xFF),
        (uint8_t)(byte_rate & 0xFF),
        (uint8_t)((byte_rate >> 8) & 0xFF),
        (uint8_t)((byte_rate >> 16) & 0xFF),
        (uint8_t)((byte_rate >> 24) & 0xFF),
        0x02, 0x00,  // Block align (2 bytes)
        SAMPLE_BITS, 0x00,  // Bits per sample
        'd', 'a', 't', 'a',
        (uint8_t)(wav_size & 0xFF),
        (uint8_t)((wav_size >> 8) & 0xFF),
        (uint8_t)((wav_size >> 16) & 0xFF),
        (uint8_t)((wav_size >> 24) & 0xFF)
    };
    memcpy(wav_header, set_wav_header, sizeof(set_wav_header));
}

void startRecording(String label) {
    if (isRecording) return;

    String filename = "/" + label + ".wav";  // Removed timestamp
    Serial.printf("Start recording %s...\n", filename.c_str());
    
    // Check if file already exists and delete it
    if (SD.exists(filename.c_str())) {
        SD.remove(filename.c_str());
    }
    
    // Open file
    audioFile = SD.open(filename.c_str(), FILE_WRITE);
    if (!audioFile) {
        Serial.println("Failed to create file");
        return;
    }

    // Write initial WAV header
    uint8_t wav_header[WAV_HEADER_SIZE];
    generate_wav_header(wav_header, 0, SAMPLE_RATE);
    if (audioFile.write(wav_header, WAV_HEADER_SIZE) != WAV_HEADER_SIZE) {
        Serial.println("Failed to write WAV header");
        audioFile.close();
        return;
    }

    // Start recording
    audioDataSize = 0;
    isRecording = true;
    Serial.println("Recording started...");
}

void stopRecording() {
    if (!isRecording) return;

    isRecording = false;

    // Update WAV header with final size
    uint8_t wav_header[WAV_HEADER_SIZE];
    generate_wav_header(wav_header, audioDataSize, SAMPLE_RATE);
    audioFile.seek(0);
    audioFile.write(wav_header, WAV_HEADER_SIZE);

    audioFile.close();
    Serial.printf("Recording stopped. File size: %.2f KB\n", audioDataSize/1024.0);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) ;  // Wait for Serial to be ready
    delay(1000);
    
    // Disable watchdog timer
    esp_task_wdt_init(30, false);  // 30 second timeout, don't panic on timeout
    
    Serial.println("\nInitializing system...");

    // Initialize I2S with PDM sample rate
    I2S.setAllPins(-1, 42, 41, -1, -1);
    if (!I2S.begin(PDM_MONO_MODE, PDM_SAMPLE_RATE, SAMPLE_BITS)) {
        Serial.println("Failed to initialize I2S!");
        while (1) ;
    }

    // Initialize SD card exactly as in Seeed example
    if(!SD.begin(SD_CS)){
        Serial.println("Failed to mount SD Card!");
        while (1) ;
    }
    Serial.println("SD Card initialized successfully!");

    // Pre-allocate recording buffer
    rec_buffer = (uint8_t *)malloc(BUFFER_SIZE);
    if (rec_buffer == NULL) {
        Serial.println("Failed to allocate recording buffer!");
        while (1) ;
    }

    // Initialize WiFi
    WiFi.begin(ssid, password);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
        delay(500);
        Serial.print(".");
        retry++;
    }
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi connection failed!");
        while (1) ;
    }
    
    Serial.println("\nConnected to WiFi");
    Serial.println(WiFi.localIP());

    // Initialize web server
    initWebServer();
    Serial.println("Setup complete!");
}

void loop() {
    if (isRecording && rec_buffer != NULL) {
        size_t bytesRead = I2S.read(rec_buffer, BUFFER_SIZE);
        
        if (bytesRead > 0) {
            // Process samples directly without downsampling
            uint8_t processedBuffer[BUFFER_SIZE];
            for (size_t i = 0; i < bytesRead; i += 2) {
                int16_t* sample = (int16_t*)(rec_buffer + i);
                int16_t processedSample = (*sample) * VOLUME_GAIN;
                processedBuffer[i] = processedSample & 0xFF;
                processedBuffer[i + 1] = (processedSample >> 8) & 0xFF;
            }

            // Write all data
            size_t bytesWritten = audioFile.write(processedBuffer, bytesRead);
            if (bytesWritten > 0) {
                audioDataSize += bytesWritten;
            }

            if (millis() - lastFlushTime > 1000) {
                audioFile.flush();
                lastFlushTime = millis();
            }
        }
    }
    delay(1);
}