# Rotating Platform Control - code_rotate

## Overview
Arduino code for the ESP32 (WEMOS S2-mini) microcontroller that controls a rotating platform. The platform can be rotated to a HOME position and then periodically rotated with precise timing and no accumulated drift.

## Hardware Interface

### Motor Control
- **Pin 39**: Motor control digital output
  - HIGH: Motor ON
  - LOW: Motor OFF

### Position Limit Switches
Two limit switches with transistor voltage level converters (5V to ESP32 compatible):

- **Pin 16 (HOME_SWITCH_PIN)**: HOME position limit switch
  - Triggered when platform reaches position 0 (HOME position)
  - Used to establish home position reference
  - Part of position detection system (every 90 degrees)

- **Pin 18 (POSITION_SWITCH_PIN)**: Position limit switch
  - Triggered at each 90-degree offset from HOME
  - Positions: 90°, 180°, 270° (HOME is 0°)
  - Used to measure rotation speed (RPM) between triggers
  - Also used to detect HOME position (both switches trigger at home)

### Switch Characteristics
- Both switches use LOW to HIGH transitions to signal detection
- 100ms debounce time required to filter glitches
- All 4 discrete positions (0°, 90°, 180°, 270°) are detectable

## WiFi & Web Interface

### Network
- **Mode**: Access Point (AP)
- **SSID**: "dicotomy"
- **Password**: "passion"
- **Port**: 80 (HTTP)

### Web Page Features
The web interface provides configuration and control:

#### Configuration Parameters
1. **HOME OFFSET** (0-360 degrees)
   - Degrees to rotate after HOME switch is detected
   - Establishes the starting position for periodic rotations
   - Valid range: 0-360

2. **PERIODIC PERIOD** (60-1000 seconds, 1 decimal place)
   - Time between start of each periodic rotation
   - Maintains precise timing with no accumulated drift
   - Example: 60.0 means rotations start every 60 seconds

3. **PERIODIC ROTATION** (degrees in 90° increments)
   - Valid options: 90, 180, 270, 360, 450, 540, 630, 720
   - Amount to rotate in each periodic cycle
   - Example: 540 = 1.5 rotations

#### Commands
- **HOME**: Rotate platform to HOME position + HOME OFFSET
  - Motor must be OFF to start HOME sequence
  - Detects HOME switch twice, then rotates to offset position
  
- **RUN**: Start periodic rotations
  - Only available after HOME sequence completes
  - Rotations start immediately and repeat every PERIODIC PERIOD
  - No accumulated timing drift

- **STOP**: Stop motor and periodic rotations
  - Halts any active rotation
  - Motor transitions to OFF state

## Motion Control Details

### Rotation Tracking
- Four hardware-detected positions: 0° (HOME), 90°, 180°, 270°
- Additional positions calculated based on HOME OFFSET (up to 360°)
- Rotation speed (RPM) measured from time between POSITION switch triggers (90° apart)

### HOME Sequence (Example)
1. User clicks HOME button
2. Motor rotates until HOME switch triggers (first detection)
3. Motor continues until HOME switch triggers again (second detection)
4. Motor rotates additional HOME OFFSET degrees
   - If offset divides evenly by 90°, uses position switches
   - Remaining degrees (< 90°) calculated using estimated RPM
5. Motor stops at final home position
6. Platform ready for RUN command

### Periodic Rotation Sequence (Example)
1. Home position confirmed
2. User clicks RUN button
3. First rotation starts immediately
4. Platform rotates PERIODIC_ROTATION degrees (using position switches and time)
5. Motor stops
6. Precisely PERIODIC_PERIOD seconds after RUN was issued, rotation #2 starts
7. Rotations continue: #3 starts at 2×PERIODIC_PERIOD, etc.
8. No accumulated time drift between rotations

### Timing Precision
- Rotations are scheduled from the RUN button time, not from motor stop time
- This eliminates accumulated drift even if individual rotations take slightly different times
- Each periodic rotation starts at: RUN_TIME + (N × PERIODIC_PERIOD)

## Code Architecture

### State Machine
```
MOTOR_OFF → HOMING → HOME_COMPLETE → PERIODIC_RUNNING ↔ PERIODIC_WAITING
   ↑                      ↑                              ↑
   +--- STOP command -----+-------------- STOP command --+
```

### Key Functions
- `updateSwitchStates()`: Debounces limit switch inputs
- `onHomeSwitchTriggered()`: Handles HOME switch logic
- `onPositionSwitchTriggered()`: Tracks rotation and measures RPM
- `updateMotorControl()`: Manages motor timing and state transitions
- `handleHome()`, `handleRun()`, `handleStop()`: Web command handlers

### Important Variables
- `motorState`: Current operational state
- `homeCountDuringHome`: Tracks HOME switch triggers during homing
- `positionCountDuringRotation`: Counts 90° position switches
- `estimatedRPM`: Calculated speed from switch timing
- `runCommandTime`: Reference time for periodic rotation scheduling
- `lastRotationNumber`: Prevents multiple starts of same rotation

## Validation Rules
All configuration values are validated:
- HOME OFFSET: Must be 0-360 (integer)
- PERIODIC PERIOD: Must be 60-1000 (float with 1 decimal)
- PERIODIC ROTATION: Must be one of [90, 180, 270, 360, 450, 540, 630, 720]

Invalid configurations are rejected with error messages.

## Serial Debug Output
Monitor serial port (115200 baud) for debugging information:
- WiFi connection status
- Web server startup
- Switch trigger events
- RPM measurements
- Command execution logs
- State transitions

## Safety Notes
- Motor OFF when power applied (safe default)
- HOME sequence required before RUN
- STOP always available to halt rotation
- Configuration validates all inputs
- 100ms debounce prevents false triggers
