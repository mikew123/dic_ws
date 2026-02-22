## Arduino Rotating Platform Control - Code Implementation Summary

### Project: arduino/code_rotate

#### Overview
This Arduino sketch implements a rotating platform controller for an ESP32 with precise periodic rotation control using limit switches and speed estimation.

#### Hardware Configuration
- **Motor Control**: Pin 39 (HIGH = ON, LOW = OFF)
- **HOME Switch**: Pin 16 (triggered at position 0°)
- **POSITION Switch**: Pin 18 (triggered every 90°)
- **WiFi**: AP mode with SSID "dicotomy" and password "passion"

#### Key Features

##### 1. State Machine
- **MOTOR_OFF**: Idle state, ready for commands
- **HOMING**: Detecting HOME switch to find home position
- **HOMING_OFFSET**: Rotating to apply HOME OFFSET after home detection
- **RUNNING_PERIODIC**: Executing periodic rotations at fixed intervals

##### 2. Configuration Parameters (with validation)
- **HOME OFFSET**: Degrees to rotate after detecting HOME switch (0+)
- **PERIODIC PERIOD**: Seconds between rotation start times (0.1+, 1 decimal)
- **PERIODIC ROTATION**: Degrees to rotate each period (0+)

##### 3. Limit Switch Handling
- **Debouncing**: 100ms required for stable detection
- **Counting**: 
  - HOME switch detected twice during HOME sequence
  - POSITION switch counted to track 90° intervals
- **RPM Measurement**: Calculated from time between position switches

##### 4. Rotation Control Algorithm
- **Full 90° Intervals**: Tracked via position switch counts
- **Remaining Degrees**: Calculated using measured RPM
  - Formula: time = (remaining_degrees / RPM) / 6
  - Applied via elapsed time tracking
- **Precise Periodic Timing**: No accumulated errors
  - Each rotation starts at exactly: runCommandTime + (n × periodicPeriod)

##### 5. Command Flow
- **ON/OFF**: Basic motor control, stops periodic rotations
- **HOME**: 
  1. Detect HOME switch (2 times required)
  2. Rotate by HOME OFFSET using measured RPM
  3. Motor stops at final position
- **RUN**: 
  - Only available after HOME completes
  - Waits PERIODIC_PERIOD, then starts rotations
  - Subsequent rotations start at precise intervals
- **STOP**: Stops rotations immediately, motor OFF

#### Web Interface Features
- Configuration input fields with validation
- 5 command buttons: ON, OFF, HOME, RUN, STOP
- Real-time status display
- Responsive mobile-friendly design

#### Critical Implementation Details

1. **Switch Transition Detection**:
   - Detects LOW → HIGH transition
   - Waits for stable HIGH state (100ms debounce)
   - Prevents multiple triggers until signal returns LOW

2. **RPM Calculation**:
   - Measured between consecutive position switches (90° apart)
   - Used for time-based partial rotations
   - Default: 10.0 RPM if not yet measured

3. **Homing Offset Logic**:
   - After 2nd HOME switch detection, motor continues
   - Position switches counted toward full 90° intervals
   - Remaining degrees applied via timer
   - Motor stops at exact offset position

4. **Periodic Rotation Timing**:
   - Rotations scheduled at t = runTime + (n × period)
   - No dependency on previous rotation completion
   - Motor automatically starts at scheduled times
   - Prevents floating-point time accumulation errors

#### Configuration Validation
- All values must be positive
- periodicPeriod: minimum 0.1 seconds
- Validated on both client (JavaScript) and server (Arduino)

#### Build Notes
- Requires: WiFi.h, WebServer.h (included with ESP32 board)
- Tested on: WEMOS S2-mini ESP32 S2
- Serial Monitor output at 115200 baud
- LED indicates motor status: bright = ON, dim = OFF
