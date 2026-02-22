#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "dicotomy";
const char* password = "passion";

// Motor control pin
const int MOTOR_PIN = 39;

// Limit switch pins
const int HOME_SWITCH_PIN = 16;
const int POSITION_SWITCH_PIN = 18;

// Debounce time in milliseconds
const unsigned long DEBOUNCE_TIME = 100;

// State machine states
enum MotorState {
  MOTOR_OFF,
  HOMING,
  HOMING_OFFSET,
  RUNNING_PERIODIC,
  STOPPING
};

// Configuration variables
int homeOffset = 0;           // degrees
float periodicPeriod = 60.0;  // seconds
int periodicRotation = 360;   // degrees

// Runtime variables
MotorState motorState = MOTOR_OFF;
bool motorOn = false;
unsigned long lastHomeDetectTime = 0;
unsigned long lastPositionDetectTime = 0;
int homeDetectCount = 0;
int positionDetectCount = 0;
float measuredRPM = 10.0;     // measured RPM between 90 degree positions
unsigned long homeCommandTime = 0;
unsigned long runCommandTime = 0;
unsigned long rotationStartTime = 0;
int targetRotationDegrees = 0;
int accumulatedDegrees = 0;
int remainingDegrees = 0;
unsigned long remainingDegreesStartTime = 0;
unsigned long lastHomeSwitchTransitionTime = 0;
unsigned long lastPositionSwitchTransitionTime = 0;
bool lastHomeState = false;
bool lastPositionState = false;
bool homeOffsetApplied = false;
bool homeSwitchDebouncing = false;
bool positionSwitchDebouncing = false;

// Create web server on port 80
WebServer server(80);

void setup() {
  // LED is ON while waiting for serial connection
  analogWrite(LED_BUILTIN, 255);
  Serial.begin(115200);
  uint32_t t0 = millis();
  while(!Serial) {
    // Wait for 10 seconds for USB serial connection
    if((millis()-t0) > 10000) break;
    delay(100);
  }
  analogWrite(LED_BUILTIN, 10);

  // Initialize motor pin
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);
  motorOn = false;

  // Initialize limit switch pins
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
  server.on("/on", handleMotorOn);
  server.on("/off", handleMotorOff);
  server.on("/home", handleHome);
  server.on("/run", handleRun);
  server.on("/stop", handleStop);
  server.on("/config", handleConfigUpdate);
  server.on("/status", handleStatus);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Web server started");

  // Initialize limit switch states
  lastHomeState = digitalRead(HOME_SWITCH_PIN);
  lastPositionState = digitalRead(POSITION_SWITCH_PIN);
  lastHomeSwitchTransitionTime = millis();
  lastPositionSwitchTransitionTime = millis();
}

void loop() {
  server.handleClient();
  handleLimitSwitches();
  handleRotationControl();
}

// Handle limit switch transitions with debouncing
void handleLimitSwitches() {
  unsigned long currentTime = millis();
  bool currentHomeState = digitalRead(HOME_SWITCH_PIN);
  bool currentPositionState = digitalRead(POSITION_SWITCH_PIN);

  // Check HOME switch for LOW to HIGH transition
  if (currentHomeState && !lastHomeState) {
    // Start of LOW->HIGH transition detected
    homeSwitchDebouncing = true;
    lastHomeSwitchTransitionTime = currentTime;
  } else if (homeSwitchDebouncing && currentHomeState) {
    // Check if debounce period has elapsed
    if ((currentTime - lastHomeSwitchTransitionTime) >= DEBOUNCE_TIME) {
      onHomeSwitchTriggered();
      homeSwitchDebouncing = false;
      Serial.println("HOME trigger");
    }
  } else if (!currentHomeState && lastHomeState) {
    // Transition from HIGH to LOW detected
    homeSwitchDebouncing = false;
  }
  lastHomeState = currentHomeState;

  // Check POSITION switch for LOW to HIGH transition
  if (currentPositionState && !lastPositionState) {
    // Start of LOW->HIGH transition detected
    positionSwitchDebouncing = true;
    lastPositionSwitchTransitionTime = currentTime;
  } else if (positionSwitchDebouncing && currentPositionState) {
    // Check if debounce period has elapsed
    if ((currentTime - lastPositionSwitchTransitionTime) >= DEBOUNCE_TIME) {
      onPositionSwitchTriggered();
      positionSwitchDebouncing = false;
      Serial.println("POSITION trigger");
    }
  } else if (!currentPositionState && lastPositionState) {
    // Transition from HIGH to LOW detected
    positionSwitchDebouncing = false;
  }
  lastPositionState = currentPositionState;
}

// Called when HOME switch is triggered (LOW to HIGH)
void onHomeSwitchTriggered() {
  unsigned long currentTime = millis();

  if (motorState == HOMING) {
    homeDetectCount++;
    Serial.print("HOME switch detected. Count: ");
    Serial.println(homeDetectCount);

    if (homeDetectCount >= 2) {
      // Home position found, now rotate by offset
      Serial.println("Home position detected twice. Start offset rotation.");
      motorState = HOMING_OFFSET;
      positionDetectCount = 0;
      accumulatedDegrees = 0;
      targetRotationDegrees = homeOffset;
      rotationStartTime = currentTime;
      startMotor();
    }
  }
}

// Called when POSITION switch is triggered (LOW to HIGH, occurs every 90 degrees)
void onPositionSwitchTriggered() {
  unsigned long currentTime = millis();

  if (motorState == HOMING || motorState == HOMING_OFFSET || motorState == RUNNING_PERIODIC) {
    // Measure RPM: time between position triggers = 90 degrees
    if (lastPositionDetectTime > 0) {
      unsigned long timeDifference = currentTime - lastPositionDetectTime;
      if (timeDifference > 0) {
        float rotationsPerMinute = (90.0 / 360.0) / (timeDifference / 60000.0);
        measuredRPM = rotationsPerMinute;
        Serial.print("RPM measured: ");
        Serial.println(measuredRPM);
      }
    }
    lastPositionDetectTime = currentTime;
  }

  if (motorState == HOMING_OFFSET || motorState == RUNNING_PERIODIC) {
    positionDetectCount++;
    accumulatedDegrees += 90; // Each trigger is 90 degrees

    // // Measure RPM: time between position triggers = 90 degrees
    // if (positionDetectCount > 1) {
    //   unsigned long timeDifference = currentTime - lastPositionDetectTime;
    //   if (timeDifference > 0) {
    //     float rotationsPerMinute = (90.0 / 360.0) / (timeDifference / 60000.0);
    //     measuredRPM = rotationsPerMinute;
    //     Serial.print("RPM measured: ");
    //     Serial.println(measuredRPM);
    //   }
    // }
    // lastPositionDetectTime = currentTime;

    // Check if target rotation reached
    if (motorState == HOMING_OFFSET) {
      checkHomingOffsetComplete(currentTime, positionDetectCount);
    } else if (motorState == RUNNING_PERIODIC) {
      checkPeriodicRotationComplete(currentTime, positionDetectCount);
    }
  }
}

// Check if homing offset rotation is complete
void checkHomingOffsetComplete(unsigned long currentTime, int positionSwitches) {
  int fullSwitches = targetRotationDegrees / 90;
  int extraDegrees = targetRotationDegrees % 90;

Serial.printf("Check homing: full %d extra %d pos %d",fullSwitches,extraDegrees,positionSwitches);
  // Check if we've reached the full switch count
  if (positionSwitches > fullSwitches) {
    // Too many switches, should have stopped already
    stopMotor();
    motorState = MOTOR_OFF;
    homeOffsetApplied = false;
    positionDetectCount = 0;
    Serial.println("ERROR: Overshot target rotation");
  } else if (positionSwitches == fullSwitches) {
    // Exactly at the switch count
    if (extraDegrees > 0 && !homeOffsetApplied) {
      // Start timing for remaining degrees
      remainingDegrees = extraDegrees;
      remainingDegreesStartTime = currentTime;
      homeOffsetApplied = true;
      Serial.print("Need additional ");
      Serial.print(extraDegrees);
      Serial.println(" degrees for offset");
    } else if (extraDegrees == 0) {
      // Exact rotation completed
      stopMotor();
      motorState = MOTOR_OFF;
      homeOffsetApplied = false;
      positionDetectCount = 0;
      Serial.println("Homing offset complete");
    }
  }
}

// Check if periodic rotation is complete
void checkPeriodicRotationComplete(unsigned long currentTime, int positionSwitches) {
  int fullSwitches = periodicRotation / 90;
  int extraDegrees = periodicRotation % 90;

  // Check if we've reached the full switch count
  if (positionSwitches > fullSwitches) {
    // Too many switches, should have stopped already
    stopMotor();
    motorState = RUNNING_PERIODIC;
    homeOffsetApplied = false;
    positionDetectCount = 0;
    Serial.println("ERROR: Overshot periodic target rotation");
  } else if (positionSwitches == fullSwitches) {
    // Exactly at the switch count
    if (extraDegrees > 0 && !homeOffsetApplied) {
      // Start timing for remaining degrees
      remainingDegrees = extraDegrees;
      remainingDegreesStartTime = currentTime;
      homeOffsetApplied = true;
      Serial.print("Need additional ");
      Serial.print(extraDegrees);
      Serial.println(" degrees for rotation");
    } else if (extraDegrees == 0) {
      // Exact rotation completed
      stopMotor();
      motorState = RUNNING_PERIODIC;
      homeOffsetApplied = false;
      positionDetectCount = 0;
      Serial.println("One periodic rotation complete");
    }
  }
}

// Handle rotation control and timing
void handleRotationControl() {
  unsigned long currentTime = millis();

  // Handle time-based rotation for remaining degrees (both HOMING_OFFSET and RUNNING_PERIODIC)
  if ((motorState == HOMING_OFFSET || motorState == RUNNING_PERIODIC) && homeOffsetApplied && remainingDegrees > 0) {
    // Calculate time needed for remaining degrees
    float secondsPerRotation = 60.0 / measuredRPM;
    float degreesPerSecond = 360.0 / secondsPerRotation;
    float timeNeededMs = (remainingDegrees / degreesPerSecond) * 1000.0;

    if ((currentTime - remainingDegreesStartTime) >= timeNeededMs) {
      // Time to stop
      stopMotor();
      homeOffsetApplied = false;
      remainingDegrees = 0;

      if (motorState == HOMING_OFFSET) {
        motorState = MOTOR_OFF;
        positionDetectCount = 0;
        Serial.println("Homing offset complete");
      } else if (motorState == RUNNING_PERIODIC) {
        // Stay in RUNNING_PERIODIC state, motor will restart at next scheduled time
        Serial.println("One periodic rotation complete");
      }
    }
  }

  // Handle RUNNING_PERIODIC periodic timing - schedule rotations precisely
  if (motorState == RUNNING_PERIODIC && !motorOn && !homeOffsetApplied) {
    unsigned long timeSinceRun = currentTime - runCommandTime;
    unsigned long nextRotationStartMs = (unsigned long)(periodicPeriod * 1000.0);

    // Calculate how many rotation periods have elapsed
    unsigned long periodNumber = timeSinceRun / nextRotationStartMs;

    // Check if we should start the next rotation
    unsigned long expectedStartTime = runCommandTime + ((periodNumber + 1) * nextRotationStartMs);

    if (currentTime >= expectedStartTime && motorState == RUNNING_PERIODIC && !motorOn) {
      Serial.print("Starting periodic rotation #");
      Serial.println(periodNumber + 1);
      targetRotationDegrees = periodicRotation;
      positionDetectCount = 0;
      accumulatedDegrees = 0;
      homeOffsetApplied = false;
      rotationStartTime = currentTime;
      startMotor();
    }
  }
}

// Start motor
void startMotor() {
  if (!motorOn) {
    digitalWrite(MOTOR_PIN, HIGH);
    analogWrite(LED_BUILTIN, 255);
    motorOn = true;
    Serial.println("Motor ON");
  }
}

// Stop motor
void stopMotor() {
  if (motorOn) {
    digitalWrite(MOTOR_PIN, LOW);
    analogWrite(LED_BUILTIN, 10);
    motorOn = false;
    Serial.println("Motor OFF");
  }
}

// Web server handlers

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
      margin-bottom: 30px;
      font-size: 28px;
    }
    h2 {
      color: #555;
      font-size: 18px;
      margin-top: 25px;
      margin-bottom: 15px;
      border-bottom: 2px solid #667eea;
      padding-bottom: 10px;
    }
    .config-group {
      margin-bottom: 20px;
    }
    label {
      display: block;
      color: #666;
      margin-bottom: 8px;
      font-weight: bold;
      font-size: 14px;
    }
    input {
      width: 100%;
      padding: 12px;
      border: 2px solid #ddd;
      border-radius: 8px;
      font-size: 16px;
      box-sizing: border-box;
      transition: border-color 0.3s;
    }
    input:focus {
      outline: none;
      border-color: #667eea;
    }
    .button-group {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 15px;
      margin-top: 25px;
      margin-bottom: 20px;
    }
    .button-group-full {
      display: flex;
      gap: 15px;
      flex-direction: column;
      margin-top: 20px;
    }
    button {
      padding: 15px 20px;
      font-size: 16px;
      border: none;
      border-radius: 8px;
      cursor: pointer;
      font-weight: bold;
      transition: all 0.3s ease;
      color: white;
    }
    .on-btn {
      background-color: #4CAF50;
      grid-column: 1;
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
      grid-column: 2;
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
      background-color: #FF9800;
    }
    .run-btn:hover {
      background-color: #e68900;
      transform: scale(1.02);
    }
    .run-btn:active {
      transform: scale(0.98);
    }
    .stop-btn {
      background-color: #9C27B0;
    }
    .stop-btn:hover {
      background-color: #7b1fa2;
      transform: scale(1.02);
    }
    .stop-btn:active {
      transform: scale(0.98);
    }
    .status {
      margin-top: 30px;
      padding: 20px;
      background-color: #f0f0f0;
      border-radius: 8px;
      color: #666;
      font-size: 14px;
      line-height: 1.6;
    }
    .status-item {
      margin-bottom: 10px;
    }
    .status-item strong {
      color: #333;
    }
    .error {
      color: #d32f2f;
      margin-top: 10px;
      padding: 10px;
      background-color: #ffebee;
      border-radius: 4px;
      display: none;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>Rotating Platform</h1>

    <h2>Configuration</h2>
    <div class="config-group">
      <label for="homeOffset">Home Offset (degrees):</label>
      <input type="number" id="homeOffset" min="0" value="0" />
    </div>
    <div class="config-group">
      <label for="periodicPeriod">Periodic Period (seconds):</label>
      <input type="number" id="periodicPeriod" min="0.1" step="0.1" value="60.0" />
    </div>
    <div class="config-group">
      <label for="periodicRotation">Periodic Rotation (degrees):</label>
      <input type="number" id="periodicRotation" min="0" value="360" />
    </div>

    <h2>Motor Control</h2>
    <div class="button-group">
      <button class="on-btn" onclick="sendCommand('on')">ON</button>
      <button class="off-btn" onclick="sendCommand('off')">OFF</button>
    </div>

    <h2>Rotation Commands</h2>
    <div class="button-group-full">
      <button class="home-btn" onclick="sendCommand('home')">HOME</button>
      <button class="run-btn" onclick="sendCommand('run')">RUN</button>
      <button class="stop-btn" onclick="sendCommand('stop')">STOP</button>
    </div>

    <div class="status">
      <div class="status-item"><strong>Motor Status:</strong> <span id="status">Unknown</span></div>
      <div class="status-item"><strong>Platform State:</strong> <span id="state">Unknown</span></div>
      <div id="error" class="error"></div>
    </div>
  </div>

  <script>
    function sendCommand(command) {
      const homeOffset = document.getElementById('homeOffset').value;
      const periodicPeriod = document.getElementById('periodicPeriod').value;
      const periodicRotation = document.getElementById('periodicRotation').value;

      if (command === 'home' || command === 'run') {
        // Validate config before home/run commands
        if (!validateConfig()) {
          return;
        }
        const params = `?homeOffset=${homeOffset}&periodicPeriod=${periodicPeriod}&periodicRotation=${periodicRotation}`;
        fetch('/' + command + params)
          .then(response => response.text())
          .then(data => updateStatus(command, data))
          .catch(error => showError('Error: ' + error));
      } else {
        fetch('/' + command)
          .then(response => response.text())
          .then(data => updateStatus(command, data))
          .catch(error => showError('Error: ' + error));
      }
    }

    function validateConfig() {
      const homeOffset = parseInt(document.getElementById('homeOffset').value);
      const periodicPeriod = parseFloat(document.getElementById('periodicPeriod').value);
      const periodicRotation = parseInt(document.getElementById('periodicRotation').value);

      if (homeOffset < 0 || periodicPeriod <= 0 || periodicRotation <= 0) {
        showError('All configuration values must be positive');
        return false;
      }
      return true;
    }

    function updateStatus(command, data) {
      document.getElementById('error').style.display = 'none';
      const lines = data.split('|');
      if (lines.length >= 2) {
        document.getElementById('status').textContent = lines[0];
        document.getElementById('state').textContent = lines[1];
      }
      fetch('/status')
        .then(response => response.text())
        .then(data => {
          const status = document.getElementById('status');
          status.textContent = data.includes('ON') ? 'ON' : 'OFF';
        });
    }

    function showError(message) {
      const errorDiv = document.getElementById('error');
      errorDiv.textContent = message;
      errorDiv.style.display = 'block';
    }
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// Handle motor ON command
void handleMotorOn() {
  stopPeriodicRotations();
  startMotor();
  motorState = MOTOR_OFF;
  String response = "ON|Motor turned ON";
  server.send(200, "text/plain", response);
}

// Handle motor OFF command
void handleMotorOff() {
  stopPeriodicRotations();
  stopMotor();
  motorState = MOTOR_OFF;
  String response = "OFF|Motor turned OFF";
  server.send(200, "text/plain", response);
}

// Handle HOME command
void handleHome() {
  // Get configuration from query parameters
  if (server.hasArg("homeOffset")) {
    homeOffset = server.arg("homeOffset").toInt();
  }
  if (server.hasArg("periodicPeriod")) {
    periodicPeriod = server.arg("periodicPeriod").toFloat();
  }
  if (server.hasArg("periodicRotation")) {
    periodicRotation = server.arg("periodicRotation").toInt();
  }

  // Validate config
  if (homeOffset < 0 || periodicPeriod <= 0 || periodicRotation <= 0) {
    server.send(400, "text/plain", "ERROR|Invalid configuration");
    return;
  }

  if (motorOn) {
    server.send(400, "text/plain", "ERROR|Motor must be OFF to start HOME sequence");
    return;
  }

  motorState = HOMING;
  homeDetectCount = 0;
  positionDetectCount = 0;
  accumulatedDegrees = 0;
  targetRotationDegrees = homeOffset;
  homeOffsetApplied = false;
  homeCommandTime = millis();
  startMotor();

  Serial.println("HOME sequence started");
  String response = "HOMING|Home sequence started";
  server.send(200, "text/plain", response);
}

// Handle RUN command
void handleRun() {
  // Get configuration from query parameters
  if (server.hasArg("homeOffset")) {
    homeOffset = server.arg("homeOffset").toInt();
  }
  if (server.hasArg("periodicPeriod")) {
    periodicPeriod = server.arg("periodicPeriod").toFloat();
  }
  if (server.hasArg("periodicRotation")) {
    periodicRotation = server.arg("periodicRotation").toInt();
  }

  if (motorOn) {
    server.send(400, "text/plain", "ERROR|Motor must be OFF to start RUN sequence");
    return;
  }

  if (motorState != MOTOR_OFF) {
    server.send(400, "text/plain", "ERROR|Must complete HOME sequence before RUN");
    return;
  }

  motorState = RUNNING_PERIODIC;
  runCommandTime = millis();
  positionDetectCount = 0;
  accumulatedDegrees = 0;
  targetRotationDegrees = periodicRotation;
  homeOffsetApplied = false;

  Serial.println("RUN periodic rotations started");
  String response = "RUNNING|Periodic rotations started";
  server.send(200, "text/plain", response);
}

// Handle STOP command
void handleStop() {
  stopPeriodicRotations();
  stopMotor();
  motorState = MOTOR_OFF;
  String response = "STOPPED|Periodic rotations stopped";
  server.send(200, "text/plain", response);
}

// Stop periodic rotations
void stopPeriodicRotations() {
  if (motorState == RUNNING_PERIODIC || motorState == HOMING || motorState == HOMING_OFFSET) {
    motorState = MOTOR_OFF;
    stopMotor();
    Serial.println("Periodic rotations stopped");
  }
}

// Handle configuration update
void handleConfigUpdate() {
  if (server.hasArg("homeOffset")) {
    homeOffset = server.arg("homeOffset").toInt();
  }
  if (server.hasArg("periodicPeriod")) {
    periodicPeriod = server.arg("periodicPeriod").toFloat();
  }
  if (server.hasArg("periodicRotation")) {
    periodicRotation = server.arg("periodicRotation").toInt();
  }

  server.send(200, "text/plain", "Config updated");
}

// Handle status request
void handleStatus() {
  String status = motorOn ? "ON" : "OFF";
  String state = "IDLE";
  switch (motorState) {
    case MOTOR_OFF:
      state = "OFF";
      break;
    case HOMING:
      state = "HOMING";
      break;
    case HOMING_OFFSET:
      state = "HOMING_OFFSET";
      break;
    case RUNNING_PERIODIC:
      state = "RUNNING_PERIODIC";
      break;
    case STOPPING:
      state = "STOPPING";
      break;
  }
  server.send(200, "text/plain", status + "|" + state);
}

// Handle undefined routes
void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}
