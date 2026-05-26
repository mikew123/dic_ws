#include <WiFi.h>
#include <WebServer.h>

// #include <WiFiServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>

#include <Preferences.h>
#include <cstring>
#include <FreeRTOS.h>
#include "ledStuff.h"

// WiFi credentials
const char* ssid = "dicotomy";
const char* password = "passion";

// Constants
const uint16_t WEB_SERVER_PORT = 80;
const uint16_t OSC_PORT = 53000;

bool udpStarted = false;

// create ethernet stuff
WiFiUDP udp;

WebServer server(WEB_SERVER_PORT);
// WiFiServer server(WEB_SERVER_PORT);

// Create LED controller for blinking non-blocking etc
ledStuff led;

// Hardware pins
const int MOTOR_PIN = 39;
const int HOME_SWITCH_PIN = 16;
const int POSITION_SWITCH_PIN = 18;

// Motor control states
enum MotorState {
  MOTOR_OFF,
  MOTOR_ON,
  HOMING,
  HOME_COMPLETE,
  PERIODIC_RUNNING,
  PERIODIC_WAITING
};

// Configuration parameters
float periodicPeriod = 60.0;           // 60-1000 seconds with 1 decimal
int periodicRotation = 180;             // [90, 180, 270, 360] degrees

// Global state
MotorState motorState = MOTOR_OFF;
int homeCountDuringHome = 0;           // Count of HOME switch triggers during homing
int positionCountDuringRotation = 0;   // Count of POSITION switch triggers during rotation
bool homeSwitchDetected = false;
bool positionSwitchDetected = false;
bool homeSwitchLastState = LOW;
bool positionSwitchLastState = LOW;
int lastRotationNumber = -1;           // Track which rotation we're on

// Debounce constants
const int DEBOUNCE_TIME = 10;         // 10ms debounce requirement
// Wait time for Serial port connection before continuing without
const int SERIALWAIT = 10000; // 10 sec


void setup() {
  // LED blinks while waiting for serial connection
  led.ledBlink(1000, 255, 250, 0, 250);

  Serial.begin(115200);
  uint32_t t0 = millis();
  while(!Serial) {
    // Wait for USB serial connection before continuing without
    if((millis()-t0) > SERIALWAIT) break;
    delay(100);
  }
  led.stopBlink(50);

  // Initialize pins
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);
  pinMode(HOME_SWITCH_PIN, INPUT_PULLUP);
  pinMode(POSITION_SWITCH_PIN, INPUT_PULLUP);
  
  // Start WiFi in AP mode
  Serial.println("\nStarting WiFi AP Mode...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/config", handleConfig);
  server.on("/on", handleMotorOn);
  server.on("/off", handleMotorOff);
  server.on("/home", handleHome);
  server.on("/run", handleRun);
  server.on("/stop", handleStop);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  updateSwitchStates();
  updateMotorControl();
  handleOSC();
  
  // MRW: LED ON when motor is on
  if(digitalRead(MOTOR_PIN)) 
    led.setIntensity(255);
  else 
    led.setIntensity(50);

}


////////////////////////////////////////////////////////////////////////
// OSC STUFF

//Decoded OSC messages
bool calibrateHome = false;
bool motorOn = false;
bool motorOff = false;
bool rotateTo0 = false;
bool rotateTo90 = false;
bool rotateTo180 = false;
bool rotateTo270 = false;
bool rotateNow = false;

class OSCDecoder {
public:
    struct OSCMessage {
        String address;
        String typeTag;
        struct Argument {
            char type;  // 'i', 'f', 's', 'b'
            union {
                int32_t intValue;
                float floatValue;
            };
            String stringValue;
            uint8_t blobValue[256];
            int blobSize;
        };
        static const int MAX_ARGS = 32;
        Argument args[MAX_ARGS];
        int argCount;
    };

    static String padString(const uint8_t* data, int& index, int maxLen) {
        String result;
        while (index < maxLen && data[index] != 0) {
            result += (char)data[index];
            index++;
        }
        index++;  // Skip null terminator
        // Pad to 4-byte boundary
        index += (4 - (index % 4)) % 4;
        return result;
    }

    static int32_t readInt32(const uint8_t* data, int& index) {
        int32_t value;
        memcpy(&value, &data[index], 4);
        // Convert from big-endian to little-endian if needed
        value = __builtin_bswap32(value);
        index += 4;
        return value;
    }

    static float readFloat32(const uint8_t* data, int& index) {
        uint32_t intBits;
        memcpy(&intBits, &data[index], 4);
        intBits = __builtin_bswap32(intBits);
        float value;
        memcpy(&value, &intBits, 4);
        index += 4;
        return value;
    }

    static OSCMessage* decodeMessage(const uint8_t* data, int length) {
        OSCMessage* msg = new OSCMessage();
        int index = 0;
        msg->argCount = 0;

        // Read address pattern
        msg->address = padString(data, index, length);

        // Read type tag string
        msg->typeTag = padString(data, index, length);

        // Parse arguments based on type tag
        if (msg->typeTag.startsWith(",")) {
            String types = msg->typeTag.substring(1);  // Remove leading comma

            for (int i = 0; i < types.length() && msg->argCount < OSCMessage::MAX_ARGS; i++) {
                char typeChar = types[i];
                OSCMessage::Argument& arg = msg->args[msg->argCount];
                arg.type = typeChar;
                arg.blobSize = 0;

                if (typeChar == 'i') {  // int32
                    arg.intValue = readInt32(data, index);
                    msg->argCount++;
                }
                else if (typeChar == 'f') {  // float32
                    arg.floatValue = readFloat32(data, index);
                    msg->argCount++;
                }
                else if (typeChar == 's') {  // string
                    arg.stringValue = padString(data, index, length);
                    msg->argCount++;
                }
                else if (typeChar == 'b') {  // blob
                    int32_t blobSize = readInt32(data, index);
                    if (blobSize <= 256) {
                        memcpy(arg.blobValue, data + index, blobSize);
                        arg.blobSize = blobSize;
                    }
                    index += blobSize;
                    // Pad to 4-byte boundary
                    index += (4 - (index % 4)) % 4;
                    msg->argCount++;
                }
            }
        }

        return msg;
    }
};


void printOSCMessage(OSCDecoder::OSCMessage* msg);
void decodeOSCMessage(OSCDecoder::OSCMessage* msg);
void doOSC(void);

void printOSCMessage(OSCDecoder::OSCMessage* msg) {
    Serial.print("Address: ");
    Serial.println(msg->address);

    Serial.print("Type tag: ");
    Serial.println(msg->typeTag);

    if (msg->argCount > 0) {
        Serial.println("Arguments:");
        for (int i = 0; i < msg->argCount; i++) {
            const auto& arg = msg->args[i];
            Serial.print("  [");
            Serial.print(i);
            Serial.print("] ");

            switch (arg.type) {
                case 'i':
                    Serial.print("int: ");
                    Serial.println(arg.intValue);
                    break;
                case 'f':
                    Serial.print("float: ");
                    Serial.println(arg.floatValue);
                    break;
                case 's':
                    Serial.print("string: ");
                    Serial.println(arg.stringValue);
                    break;
                case 'b':
                    Serial.print("blob (");
                    Serial.print(arg.blobSize);
                    Serial.println(" bytes)");
                    break;
            }
        }
    }
    Serial.println();
}


// Control motor using info in the OSC message
void decodeOSCMessage(OSCDecoder::OSCMessage* msg) {
    // Serial.print("Address: ");
    // Serial.println(msg->address);
    String osc = msg->address;
    // Serial.printf("OSC address: %s \n", osc.c_str());

    if (osc.indexOf("/") != 0) {
        Serial.println("ERROR: Invalid OSC address format");
        return;
    }

    int cnt = 0;
    int i0 = 0;
    // extract up to 3 fields from the OSC address
    String oscField[3];
    for (int i=0; i<3; i++) {
        int i1 = osc.indexOf("/", i0+1);
        if (i1 == -1) {
            oscField[i] = osc.substring(i0+1);
            cnt++;
            break;
        }
        else {
            oscField[i] = osc.substring(i0+1, i1);
            cnt++;
        }
        i0 = i1;
    }

    if (oscField[0] != "motor") {
        Serial.println("ERROR: First OSC field is not \"motor\"");
        return;
    }

    calibrateHome = false;
    motorOn = false;
    motorOff = false;
    if(cnt == 2) {
        calibrateHome = oscField[1] == "home";
        motorOn = oscField[1] == "on";
        motorOff = oscField[1] == "off";
    }

    rotateTo0 = false;
    rotateTo90 = false;
    rotateTo180 = false;
    rotateTo270 = false;
    rotateNow = false;
    if(cnt == 3) {
        if(oscField[1] == "rotate") {
          rotateTo0 = oscField[2] == "0";
          rotateTo90 = oscField[2] == "90";
          rotateTo180 = oscField[2] == "180";
          rotateTo270 = oscField[2] == "270";
          rotateNow = oscField[2] == "now";
        }
    }
}

// activate the OSC messages already decoded
void doOSC(void) {
    if (motorOn) Serial.println("Turn motor ON");
    if (motorOff) Serial.println("Turn motor OFF");
    if (calibrateHome) Serial.println("Calibrate to the HOME position");
    if (rotateTo0) Serial.println("Rotate to the 0 position");
    if (rotateTo90) Serial.println("Rotate to the 90 position");
    if (rotateTo180) Serial.println("Rotate to the 180 position");
    if (rotateTo270) Serial.println("Rotate to the 270 position");

    if (rotateNow) {
      // start new rotation using periodic rotation degrees on web page

    }
}

// Handle OSC messages
void handleOSC(void) {
    // // Check WiFi connection status and reconnect if needed
    // if (WiFi.status() != WL_CONNECTED) {
    //     Serial.println("WiFi disconnected! Attempting to reconnect...");
    //     connectToWiFi(storedSSID, storedPassword);
        
    //     // If reconnection fails, switch back to AP mode
    //     if (WiFi.status() != WL_CONNECTED) {
    //         Serial.println("Reconnection failed. Switching back to AP mode...");
    //         isAPMode = true;
    //         udpStarted = false;
    //         startAPMode();
    //         return; // Exit loop to allow AP mode to handle requests
    //     }
    // }
    
    // Start UDP listener if not already started
    if (!udpStarted) {
        if (udp.begin(OSC_PORT)) {
            Serial.print("Listening for OSC messages on port ");
            Serial.println(OSC_PORT);
            udpStarted = true;
        }
    }
    
    int packetSize = udp.parsePacket();
    if (packetSize) {
        led.ledBlink(1, 255, 100, 50, 0);
        // Serial.print("Received from ");
        // Serial.print(udp.remoteIP());
        // Serial.print(":");
        // Serial.println(udp.remotePort());

        // Read packet into buffer
        uint8_t buffer[1024];
        int len = udp.read(buffer, 1024);

        // Decode OSC message
        OSCDecoder::OSCMessage* msg = OSCDecoder::decodeMessage(buffer, len);
        if (msg != nullptr) {
            printOSCMessage(msg);
            decodeOSCMessage(msg);
            doOSC();
            // // The web page does not seem to be found to load in a browser
            // // messageHistory.add(formatOSCMessageForWeb(msg));
            delete msg;
        }
        else {
            Serial.println("Error decoding OSC message");
        }
    }
}

////////////////////////////////////////////////////////////////////////
// Platform switch stuff
unsigned long homeDetectTime = 0;
unsigned long lastHomeSwitchTime = 0;
unsigned long lastPositionSwitchTime = 0;

// Update limit switch states with debouncing
void updateSwitchStates() {
  // Time used for switch debounce
  unsigned long now = millis();
  
  // Read HOME switch
  bool homeRaw = digitalRead(HOME_SWITCH_PIN);
  if (homeRaw != homeSwitchLastState) {
    homeDetectTime = now;
    homeSwitchLastState = homeRaw;
  }
  
  // Debounce HOME switch
  if ((now - homeDetectTime) >= DEBOUNCE_TIME) {
    if (homeRaw && !homeSwitchDetected) {
      // LOW to HIGH transition detected
      homeSwitchDetected = true;
      lastHomeSwitchTime = now;
      Serial.println("HOME switch triggered");
      onHomeSwitchTriggered(now);
    }
    homeSwitchDetected = homeRaw;
  }
  
  // Read POSITION switch
  // MRW: OR in HOME switch to complete 4 quadrant positions
  bool positionRaw = digitalRead(POSITION_SWITCH_PIN) | homeRaw;
  if (positionRaw != positionSwitchLastState) {
    homeDetectTime = now;
    positionSwitchLastState = positionRaw;
  }
  
  // Debounce POSITION switch
  if ((now - homeDetectTime) >= DEBOUNCE_TIME) {
    if (positionRaw && !positionSwitchDetected) {
      // LOW to HIGH transition detected
      positionSwitchDetected = true;
      lastPositionSwitchTime = now;
      Serial.println("POSITION switch triggered");
      onPositionSwitchTriggered(now);
    }
    positionSwitchDetected = positionRaw;
  }
}

// Handle HOME switch trigger
void onHomeSwitchTriggered(unsigned long now) {
  if (motorState == HOMING) {
    homeCountDuringHome++;
    Serial.print("Home switch count during homing: ");
    Serial.println(homeCountDuringHome);
    
    if (homeCountDuringHome == 1) {
      positionCountDuringRotation = -1; 
    }
  }
}

// Handle POSITION switch trigger for 90,180,270,360 rotations
void onPositionSwitchTriggered(unsigned long now) {
  
  if (motorState == PERIODIC_RUNNING) {
    // Count position switches during periodic rotation
    positionCountDuringRotation++;
    Serial.print("Position switch count during rotation: ");
    Serial.println(positionCountDuringRotation);
    
    // Check if we've reached the target rotation
    int targetPositionSwitches = periodicRotation / 90;
    if (positionCountDuringRotation >= targetPositionSwitches) {
      // Stop motor for next scheduled rotation
      digitalWrite(MOTOR_PIN, LOW);
      motorState = PERIODIC_WAITING;
      Serial.print("Periodic rotation complete, next in ");
      Serial.print(periodicPeriod);
      Serial.println(" seconds");
    }
  }
}

////////////////////////////////////////////////////////////////////////
// Motor control stuff
unsigned long runCommandTime = 0;

// Update motor control logic
void updateMotorControl() {
  unsigned long now = millis();

  if (motorState == HOMING) {
    if(homeCountDuringHome >= 1) {
        digitalWrite(MOTOR_PIN, LOW);
        motorState = PERIODIC_WAITING;
        Serial.println("Homing complete, ready for periodic rotation");
    }
   
  } else if (motorState == PERIODIC_RUNNING) {
    int fullRotationSwitches = periodicRotation / 90;
    
    if (positionCountDuringRotation == fullRotationSwitches) {
        digitalWrite(MOTOR_PIN, LOW);
        motorState = PERIODIC_WAITING;
        Serial.println("Rotation complete, waiting for next period");
    }
  } else if (motorState == PERIODIC_WAITING) {
    // Calculate which rotation we should be on now
    unsigned long timeSinceRun = now - runCommandTime;
    unsigned long periodMs = (unsigned long)(periodicPeriod * 1000.0);
    
    // Which rotation number should we be starting?
    int currentRotationNumber = timeSinceRun / periodMs;
    
    // If we're on a new rotation and motor is off, start it
    if (currentRotationNumber > lastRotationNumber) {
      unsigned long expectedStartTime = runCommandTime + ((currentRotationNumber) * periodMs);
      
      // Start if we've reached or p assed the scheduled time
      if (now >= expectedStartTime) {
        Serial.print("Starting periodic rotation #");
        Serial.println(currentRotationNumber + 1);
        digitalWrite(MOTOR_PIN, HIGH);
        motorState = PERIODIC_RUNNING;
        positionCountDuringRotation = 0;
        lastRotationNumber = currentRotationNumber;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////
// WEB page stuff
// Serve the main web page
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Rotating Platform Control</title>
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
      max-width: 500px;
      width: 100%;
    }
    h1 {
      text-align: center;
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
    }
    h2 {
      color: #555;
      font-size: 16px;
      margin-top: 30px;
      margin-bottom: 15px;
      border-bottom: 2px solid #667eea;
      padding-bottom: 10px;
    }
    .config-group {
      margin-bottom: 20px;
    }
    .config-group label {
      display: block;
      margin-bottom: 5px;
      color: #333;
      font-weight: bold;
    }
    .config-group input,
    .config-group select {
      width: 100%;
      padding: 10px;
      border: 1px solid #ddd;
      border-radius: 5px;
      font-size: 14px;
    }
    .config-group input:focus,
    .config-group select:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 5px rgba(102, 126, 234, 0.3);
    }
    .button-group {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-top: 30px;
    }
    button {
      flex: 1;
      min-width: 100px;
      padding: 12px 20px;
      font-size: 14px;
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
    .home-btn {
      background-color: #2196F3;
    }
    .home-btn:hover {
      background-color: #0b7dda;
      transform: scale(1.02);
    }
    .home-btn:active {
      transform: scale(0.98);
    }
    .run-btn {
      background-color: #4CAF50;
    }
    .run-btn:hover {
      background-color: #45a049;
      transform: scale(1.02);
    }
    .run-btn:active {
      transform: scale(0.98);
    }
    .stop-btn {
      background-color: #f44336;
    }
    .stop-btn:hover {
      background-color: #da190b;
      transform: scale(1.02);
    }
    .stop-btn:active {
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
    .error {
      color: #f44336;
      border: 1px solid #f44336;
      padding: 10px;
      border-radius: 5px;
      margin-bottom: 15px;
      display: none;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Rotating Platform</h1>
    
    <div class="error" id="error"></div>
    
    <h2>Configuration</h2>
    
    <div class="config-group">
      <label for="periodicPeriod">PERIODIC PERIOD (seconds, 60-1000):</label>
      <input type="number" id="periodicPeriod" min="60" max="1000" step="0.1" value="60.0">
    </div>
    
    <div class="config-group">
      <label for="periodicRotation">PERIODIC ROTATION (degrees):</label>
      <select id="periodicRotation">
        <option value="90">90</option>
        <option value="180">180</option>
        <option value="270">270</option>
        <option value="360">360</option>
      </select>
    </div>
    
    <h2>Commands</h2>
    <div class="button-group">
      <button class="home-btn" onclick="sendCommand('home')">HOME</button>
      <button class="run-btn" onclick="sendCommand('run')">RUN</button>
      <button class="stop-btn" onclick="sendCommand('stop')">STOP</button>
      <h3>Motor testing</h3>
      <button class="on-btn" onclick="sendCommand('on')">ON</button>
      <button class="off-btn" onclick="sendCommand('off')">OFF</button>
    </div>
    
    <div class="status">
      <p>Status: <span id="status">Unknown</span></p>
    </div>
  </div>
  
  <script>
    function sendCommand(command) {
      const periodicPeriod = document.getElementById('periodicPeriod').value;
      const periodicRotation = document.getElementById('periodicRotation').value;
      
      const params = new URLSearchParams();
      params.append('periodicPeriod', periodicPeriod);
      params.append('periodicRotation', periodicRotation);
      
      fetch('/' + command + '?' + params.toString())
        .then(response => response.text())
        .then(data => {
          if (data.startsWith('ERROR')) {
            document.getElementById('error').textContent = data;
            document.getElementById('error').style.display = 'block';
            document.getElementById('status').textContent = 'Error';
          } else {
            document.getElementById('error').style.display = 'none';
            document.getElementById('status').textContent = data;
          }
        })
        .catch(error => {
          console.error('Error:', error);
          document.getElementById('status').textContent = 'Communication Error';
        });
    }
    
    // Refresh status periodically
    setInterval(() => {
      fetch('/status')
        .then(response => response.text())
        .then(data => {
          document.getElementById('status').textContent = data;
        })
        .catch(error => console.error('Error:', error));
    }, 1000);
  </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

// Handle configuration updates
void handleConfig() {
  
  if (server.hasArg("periodicPeriod")) {
    float period = server.arg("periodicPeriod").toFloat();
    if (period < 60.0 || period > 1000.0) {
      server.send(400, "text/plain", "ERROR: PERIODIC PERIOD must be 60-1000 seconds");
      return;
    }
    periodicPeriod = period;
  }
  
  if (server.hasArg("periodicRotation")) {
    int rotation = server.arg("periodicRotation").toInt();
    int validRotations[] = {90, 180, 270, 360};
    bool valid = false;
    for (int i = 0; i < 8; i++) {
      if (rotation == validRotations[i]) {
        valid = true;
        break;
      }
    }
    if (!valid) {
      server.send(400, "text/plain", "ERROR: Invalid PERIODIC ROTATION value");
      return;
    }
    periodicRotation = rotation;
  }
  
  server.send(200, "text/plain", "Configuration updated");
}

// Handle HOME command
void handleHome() {
  // Update config from parameters
  
  if (server.hasArg("periodicPeriod")) {
    float period = server.arg("periodicPeriod").toFloat();
    if (period >= 60.0 && period <= 1000.0) {
      periodicPeriod = period;
    }
  }
  
  if (server.hasArg("periodicRotation")) {
    int rotation = server.arg("periodicRotation").toInt();
    int validRotations[] = {90, 180, 270, 360};
    for (int i = 0; i < 8; i++) {
      if (rotation == validRotations[i]) {
        periodicRotation = rotation;
        break;
      }
    }
  }
  
  if (motorState != MOTOR_OFF) {
    server.send(400, "text/plain", "ERROR: Motor must be OFF to start HOME sequence");
    return;
  }
  
  motorState = HOMING;
  homeCountDuringHome = 0;
  positionCountDuringRotation = 0;

  digitalWrite(MOTOR_PIN, HIGH);
  Serial.println("HOME command issued");
  
  server.send(200, "text/plain", "Homing started");
}

// Handle RUN command
void handleRun() {
  // Update config from parameters
  
  if (server.hasArg("periodicPeriod")) {
    float period = server.arg("periodicPeriod").toFloat();
    if (period >= 60.0 && period <= 1000.0) {
      periodicPeriod = period;
    }
  }
  
  if (server.hasArg("periodicRotation")) {
    int rotation = server.arg("periodicRotation").toInt();
    int validRotations[] = {90, 180, 270, 360};
    for (int i = 0; i < 8; i++) {
      if (rotation == validRotations[i]) {
        periodicRotation = rotation;
        break;
      }
    }
  }
  
  if (motorState != HOME_COMPLETE) {
    server.send(400, "text/plain", "ERROR: HOME sequence must be completed first");
    return;
  }
  
  motorState = PERIODIC_RUNNING;
  positionCountDuringRotation = 0;
  lastRotationNumber = 0;
  digitalWrite(MOTOR_PIN, HIGH);
  Serial.println("RUN command issued");
  
  server.send(200, "text/plain", "Periodic rotation started");
}

// Handle STOP command
void handleStop() {
  if (motorState == MOTOR_OFF) {
    server.send(200, "text/plain", "Motor already OFF");
    return;
  }
  
  digitalWrite(MOTOR_PIN, LOW);
  motorState = MOTOR_OFF;
  homeCountDuringHome = 0;
  positionCountDuringRotation = 0;
  Serial.println("STOP command issued");
  
  server.send(200, "text/plain", "Motor stopped");
}

// Handle motor ON command
void handleMotorOn() {
  digitalWrite(MOTOR_PIN, HIGH);
  motorState = MOTOR_ON;
  Serial.println("Motor ON");
  server.send(200, "text/plain", "ON");
}

// Handle motor OFF command
void handleMotorOff() {
  digitalWrite(MOTOR_PIN, LOW);
  motorState = MOTOR_OFF;
  Serial.println("Motor OFF");
  server.send(200, "text/plain", "OFF");
}

// Handle status request
void handleStatus() {
  String status = "Unknown";
  
  switch (motorState) {
    case MOTOR_OFF:
      status = "OFF";
      break;
    case HOMING:
      status = "Homing...";
      break;
    case HOME_COMPLETE:
      status = "Home confirmed";
      break;
    case PERIODIC_RUNNING:
      status = "Running periodic rotations";
      break;
    case PERIODIC_WAITING:
      status = "Waiting for next rotation";
      break;
  }
  
  server.send(200, "text/plain", status);
}

// Handle undefined routes
void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}
