#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>
#include "wav_header.h"

// Define HTTP methods if not defined
#ifndef HTTP_GET
#define HTTP_GET 0b00000001
#define HTTP_POST 0b00000010
#define HTTP_ANY 0b11111111
#endif

extern AsyncWebServer server;
extern bool isRecording;
extern String currentLabel;
extern fs::File audioFile;
extern size_t audioDataSize;
extern WAVHeader wavHeader;

// Add function declarations
void startRecording(String label);
void stopRecording();
void initWebServer();

#endif 