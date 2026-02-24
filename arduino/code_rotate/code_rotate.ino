#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "dicotomy";
const char* password = "passion";

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
int homeOffset = 0;                    // 0-360 degrees
float periodicPeriod = 60.0;           // 60-1000 seconds with 1 decimal
int periodicRotation = 90;             // [90, 180, 270, 360, 450, 540, 630, 720] degrees

// Global state
MotorState motorState = MOTOR_OFF;
unsigned long lastHomeSwitchTime = 0;
unsigned long lastPositionSwitchTime = 0;
unsigned long homeDetectTime = 0;      // Time when home switch was first detected
unsigned long lastHomeOffsetCompleteTime = 0;
float estimatedRPM = 10.0;             // Default RPM estimate
int homeCountDuringHome = 0;           // Count of HOME switch triggers during homing
int positionCountDuringRotation = 0;   // Count of POSITION switch triggers during rotation
bool homeSwitchDetected = false;
bool positionSwitchDetected = false;
bool homeSwitchLastState = LOW;
bool positionSwitchLastState = LOW;
unsigned long runCommandTime = 0;      // Time when RUN command was issued
int lastRotationNumber = -1;           // Track which rotation we're on

// Debounce constants
const int DEBOUNCE_TIME = 100;         // 100ms debounce requirement
// Maximum time between 90 degree positions while rotating at lowest speed
const unsigned long  TIMEDELTA_MAX = 50000; // 50 sec
// Wait time for Serial port connection before continuing without
const int SERIALWAIT = 10000; // 10 sec

// Create web server on port 80
WebServer server(80);

void setup() {
  // LED is ON while waiting for serial connection
  analogWrite(LED_BUILTIN, 255);
  Serial.begin(115200);
  uint32_t t0 = millis();
  while(!Serial) {
    // Wait for USB serial connection before continuing without
    if((millis()-t0) > SERIALWAIT) break;
    delay(100);
  }
  analogWrite(LED_BUILTIN, 10);

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

  // MRW: LED ON when motor is on
  if(digitalRead(MOTOR_PIN)) 
    analogWrite(LED_BUILTIN, 255);
  else 
    analogWrite(LED_BUILTIN, 50);

}

// Update limit switch states with debouncing
void updateSwitchStates() {
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
    
    if (homeCountDuringHome == 2) {
      // Start home offset rotation
      lastHomeOffsetCompleteTime = now;
      // MRW: HOME also causes ROTATION trigger so start at -1 to get 0
      positionCountDuringRotation = -1; 
      Serial.print("Starting home offset rotation: ");
      Serial.println(homeOffset);
    }
  }
}

// Handle POSITION switch trigger
void onPositionSwitchTriggered(unsigned long now) {
  // Estimate RPM from time between position switches (90 degrees apart)
  static unsigned long lastPosTime = 0;
  if (lastPosTime > 0) {
    unsigned long timeDelta = now - lastPosTime;
    // RPM = (90 degrees / timeDelta in ms) * 60000 ms/min / 360 degrees per rotation
    // MRW: Ignore large time between position switches
    if ((timeDelta > 0) && (timeDelta < TIMEDELTA_MAX)){
      // estimatedRPM = (90.0 / (float)timeDelta) * 60.0;
      // MRW: Accurate RPM calc since timeDelta is in msec using millis()
      estimatedRPM = (90.0/360.0) * ((60.0*1000)/float(timeDelta));
      Serial.print("timeDelta:");
      Serial.println(timeDelta);
      Serial.print("Estimated RPM: ");
      Serial.println(estimatedRPM);
    }
  }
  lastPosTime = now;
  
  if (motorState == HOMING && homeCountDuringHome >= 2) {
    // During home offset rotation, count position switches
    positionCountDuringRotation++;
    Serial.print("Position switch during home offset: ");
    Serial.println(positionCountDuringRotation);
    
    // Each position switch represents 90 degrees
    int degreesFromLastHome = 90 * positionCountDuringRotation;
    
    if (degreesFromLastHome >= homeOffset) {
      // Need to calculate if we should stop or continue
      int fullSwitches = homeOffset / 90;
      int remainingDegrees = homeOffset % 90;
      
      if (positionCountDuringRotation > fullSwitches) {
        // We've gone too far, stop motor
        digitalWrite(MOTOR_PIN, LOW);
        motorState = HOME_COMPLETE;
        Serial.println("Homing complete (after offset)");
      } else if (positionCountDuringRotation == fullSwitches && remainingDegrees == 0) {
        // Exact stop at switch
        digitalWrite(MOTOR_PIN, LOW);
        motorState = HOME_COMPLETE;
        positionCountDuringRotation = 0;
        homeCountDuringHome = 0;
        Serial.println("Homing complete (exact offset)");
      }
    }
  } else if (motorState == PERIODIC_RUNNING) {
    // Count position switches during periodic rotation
    positionCountDuringRotation++;
    Serial.print("Position switch count during rotation: ");
    Serial.println(positionCountDuringRotation);
    
    // Check if we've reached the target rotation
    int targetPositionSwitches = (periodicRotation+homeOffset) / 90;
    int remainingDegrees = (periodicRotation+homeOffset) % 90;
    if (positionCountDuringRotation >= targetPositionSwitches && remainingDegrees == 0) {
      // Stop motor for next scheduled rotation
      digitalWrite(MOTOR_PIN, LOW);
      motorState = PERIODIC_WAITING;
      Serial.print("Periodic rotation complete, next in ");
      Serial.print(periodicPeriod);
      Serial.println(" seconds");
    }
  }
}

// Update motor control logic
void updateMotorControl() {
  unsigned long now = millis();
  
  if (motorState == HOMING && homeCountDuringHome >= 2) {
    // Handle home offset rotation
    int fullRotationSwitches = homeOffset / 90;
    int remainingDegrees = homeOffset % 90;
    
    if (remainingDegrees > 0 && positionCountDuringRotation == fullRotationSwitches) {
      // Rotating the remaining degrees using time
      unsigned long timeSinceLastSwitch = now - lastPositionSwitchTime;
      // float rotationTimeForRemainder = (remainingDegrees / 90.0) * (60000.0 / estimatedRPM);
      //MRW: New calc for correct RPM
      float rotationTimeForRemainder = (remainingDegrees / 360.0) * (60000.0 / estimatedRPM);
   
      Serial.print("Remaining degrees: ");
      Serial.print(remainingDegrees);
      Serial.print(", time needed: ");
      Serial.println((unsigned long)rotationTimeForRemainder);
      
      if (timeSinceLastSwitch >= (unsigned long)rotationTimeForRemainder) {
        digitalWrite(MOTOR_PIN, LOW);
        motorState = HOME_COMPLETE;
        positionCountDuringRotation = 0;
        homeCountDuringHome = 0;
        Serial.println("Homing complete (time-based offset)");
      }
    }
  } else if (motorState == PERIODIC_RUNNING) {
    // Check if we need to handle remaining degrees for this rotation
    int fullRotationSwitches = (periodicRotation+homeOffset) / 90;
    int remainingDegrees = (periodicRotation+homeOffset) % 90;
    
    if (remainingDegrees > 0 && positionCountDuringRotation == fullRotationSwitches) {
      // Rotating the remaining degrees using time
      unsigned long timeSinceLastSwitch = now - lastPositionSwitchTime;
      // float rotationTimeForRemainder = (remainingDegrees / 90.0) * (60000.0 / estimatedRPM);
      //MRW: New calc for correct RPM
      float rotationTimeForRemainder = (remainingDegrees / 360.0) * (60000.0 / estimatedRPM);
      
      Serial.print("Remaining degrees: ");
      Serial.println(remainingDegrees);
      Serial.print(", time needed: ");
      Serial.println((unsigned long)rotationTimeForRemainder);

      if (timeSinceLastSwitch >= (unsigned long)rotationTimeForRemainder) {
        digitalWrite(MOTOR_PIN, LOW);
        motorState = PERIODIC_WAITING;
        Serial.println("Rotation complete, waiting for next period");
      }
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
      
      // Start if we've reached or passed the scheduled time
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
      <label for="homeOffset">HOME OFFSET (degrees 0-360):</label>
      <input type="number" id="homeOffset" min="0" max="360" value="0">
    </div>
    
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
        <option value="450">450</option>
        <option value="540">540</option>
        <option value="630">630</option>
        <option value="720">720</option>
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
      const homeOffset = document.getElementById('homeOffset').value;
      const periodicPeriod = document.getElementById('periodicPeriod').value;
      const periodicRotation = document.getElementById('periodicRotation').value;
      
      const params = new URLSearchParams();
      params.append('homeOffset', homeOffset);
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
  if (server.hasArg("homeOffset")) {
    int offset = server.arg("homeOffset").toInt();
    if (offset < 0 || offset > 360) {
      server.send(400, "text/plain", "ERROR: HOME OFFSET must be 0-360 degrees");
      return;
    }
    homeOffset = offset;
  }
  
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
    int validRotations[] = {90, 180, 270, 360, 450, 540, 630, 720};
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
  if (server.hasArg("homeOffset")) {
    int offset = server.arg("homeOffset").toInt();
    if (offset >= 0 && offset <= 360) {
      homeOffset = offset;
    }
  }
  
  if (server.hasArg("periodicPeriod")) {
    float period = server.arg("periodicPeriod").toFloat();
    if (period >= 60.0 && period <= 1000.0) {
      periodicPeriod = period;
    }
  }
  
  if (server.hasArg("periodicRotation")) {
    int rotation = server.arg("periodicRotation").toInt();
    int validRotations[] = {90, 180, 270, 360, 450, 540, 630, 720};
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
  if (server.hasArg("homeOffset")) {
    int offset = server.arg("homeOffset").toInt();
    if (offset >= 0 && offset <= 360) {
      homeOffset = offset;
    }
  }
  
  if (server.hasArg("periodicPeriod")) {
    float period = server.arg("periodicPeriod").toFloat();
    if (period >= 60.0 && period <= 1000.0) {
      periodicPeriod = period;
    }
  }
  
  if (server.hasArg("periodicRotation")) {
    int rotation = server.arg("periodicRotation").toInt();
    int validRotations[] = {90, 180, 270, 360, 450, 540, 630, 720};
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
  runCommandTime = millis();
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
