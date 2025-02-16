#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "webserver.h"

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Audio Recording</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding: 15px; }
    .button { background-color: #4CAF50; color: white; padding: 15px 32px; border: none; border-radius: 4px; margin: 5px; }
    .button.red { background-color: #f44336; }
    #timer { font-size: 24px; margin: 10px; font-weight: bold; }
    #status { margin: 10px; }
    .footer { position: fixed; bottom: 10px; width: 100%; text-align: center; font-size: 12px; color: #666; }
  </style>
</head>
<body>
  <h1>ESP32 Audio Recording</h1>
  <input type="text" id="label" placeholder="Enter label name">
  <br><br>
  <div id="timer">00:00</div>
  <button class="button" onclick="startRecording()">Start Recording</button>
  <button class="button red" onclick="stopRecording()">Stop Recording</button>
  <p id="status"></p>
  
  <div class="footer">
    Developed by Srijan &copy; 2025<br>
    XIAO ESP32S3 Audio Recording System
  </div>

  <script>
    let timerInterval;
    let startTime;

    function updateTimer() {
      const now = new Date().getTime();
      const elapsed = now - startTime;
      const seconds = Math.floor(elapsed / 1000);
      const minutes = Math.floor(seconds / 60);
      const remainingSeconds = seconds % 60;
      
      document.getElementById('timer').textContent = 
        `${minutes.toString().padStart(2, '0')}:${remainingSeconds.toString().padStart(2, '0')}`;
    }

    function startRecording() {
      let label = document.getElementById('label').value;
      if (!label) {
        alert('Please enter a label name');
        return;
      }
      
      startTime = new Date().getTime();
      clearInterval(timerInterval);
      timerInterval = setInterval(updateTimer, 1000);
      updateTimer();
      
      fetch('/start?label=' + label)
        .then(response => response.text())
        .then(data => {
          document.getElementById('status').innerHTML = 'Recording...';
        });
    }
    
    function stopRecording() {
      clearInterval(timerInterval);
      
      fetch('/stop')
        .then(response => response.text())
        .then(data => {
          document.getElementById('status').innerHTML = 'Recording saved!';
          document.getElementById('timer').textContent = '00:00';
        });
    }

    document.getElementById('timer').textContent = '00:00';
  </script>
</body>
</html>
)rawliteral";

void initWebServer() {
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    // Start recording
    server.on("/start", HTTP_GET, [](AsyncWebServerRequest *request){
        if (request->hasParam("label")) {
            currentLabel = request->getParam("label")->value();
            startRecording(currentLabel);
            request->send(200, "text/plain", "Recording started");
        } else {
            request->send(400, "text/plain", "Label parameter missing");
        }
    });

    // Stop recording
    server.on("/stop", HTTP_GET, [](AsyncWebServerRequest *request){
        if (isRecording) {
            stopRecording();
            request->send(200, "text/plain", "Recording stopped");
        } else {
            request->send(400, "text/plain", "Not recording");
        }
    });

    server.begin();
} 