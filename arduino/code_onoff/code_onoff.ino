#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "dicotomy";
const char* password = "passion";

// Motor control pin
const int MOTOR_PIN = 39;

// Create web server on port 80
WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize motor pin
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);
  
  // Start WiFi in AP mode
  Serial.println("\nStarting WiFi AP Mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/on", handleMotorOn);
  server.on("/off", handleMotorOff);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
}

// Serve the main web page
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Motor Control</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 15px;
      padding: 40px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      max-width: 400px;
      width: 100%;
    }
    h1 {
      text-align: center;
      color: #333;
      margin-bottom: 30px;
      font-size: 28px;
    }
    .button-group {
      display: flex;
      gap: 15px;
      flex-direction: column;
    }
    button {
      padding: 15px 30px;
      font-size: 18px;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      font-weight: bold;
      transition: all 0.3s ease;
      color: white;
    }
    .on-btn {
      background-color: #4CAF50;
    }
    .on-btn:hover {
      background-color: #45a049;
      transform: scale(1.02);
    }
    .on-btn:active {
      transform: scale(0.98);
    }
    .off-btn {
      background-color: #f44336;
    }
    .off-btn:hover {
      background-color: #da190b;
      transform: scale(1.02);
    }
    .off-btn:active {
      transform: scale(0.98);
    }
    .status {
      margin-top: 30px;
      padding: 15px;
      background-color: #f0f0f0;
      border-radius: 8px;
      text-align: center;
      color: #666;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Rotating Platform</h1>
    <div class="button-group">
      <button class="on-btn" onclick="sendCommand('on')">ON</button>
      <button class="off-btn" onclick="sendCommand('off')">OFF</button>
    </div>
    <div class="status">
      <p>Motor Status: <span id="status">Unknown</span></p>
    </div>
  </div>
  
  <script>
    function sendCommand(command) {
      fetch('/' + command)
        .then(response => response.text())
        .then(data => {
          document.getElementById('status').textContent = data;
        })
        .catch(error => console.error('Error:', error));
    }
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

// Handle motor ON command
void handleMotorOn() {
  digitalWrite(MOTOR_PIN, HIGH);
  Serial.println("Motor ON");
  server.send(200, "text/plain", "ON");
}

// Handle motor OFF command
void handleMotorOff() {
  digitalWrite(MOTOR_PIN, LOW);
  Serial.println("Motor OFF");
  server.send(200, "text/plain", "OFF");
}

// Handle undefined routes
void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}