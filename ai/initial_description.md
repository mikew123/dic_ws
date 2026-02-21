# Overview
Generate Arduino code in the arduino folder for the WEMOS S2-mini ESP32 micro controller. The code is used to control a rotating platform that periodically rotates a specified degrees. The platform can be "homed" to a specific angle. Fix any errors after generating the code.

# Hardware interface to platform
The platform motor control and position limit switch sensors connect to digital pins of the ESP32.

## Motor control
The motor is controlled with a digital output to transistors to voltage level convert to 5V.
- Use pin 39, high is motor ON, low is motor OFF

## Position limit switches
There are 2 limit switches with transitors for voltage level conversion. High is when the switches are at the home location or one of the 4 positions at 90 degree offsets (0, 90, 180, 270).
- The HOME switch is on pin 16. The home position switch is is triggered at position 0 which sets the platform alignment.
- The POSITION switch is on pin 18. The position switch gets triggered at each of the 4 positions 90 grees apart.

# Ethernet/wifi
Create a wifi interface in station mode on the ESP32 to serve web page(s). The ssid and password can be chaanged by editing the code file.
- The ssid is Dicotomy
- The password is OfPassion
## Web page
Serve a web page that configures and sends commands the ESP32
### Configurations
Validate all configurations are positive.
- HOME OFFSET: 
Degrees offset after the HOME limit switch is sensed. This establishes the platform HOME position after the HOME command is given. This is positive integer in degrees.
- POSITION PERIOD: The amount of time between start of each rotation. This is positive floating seconds with 1 decimal place.
- POSITION MOVEMENT: This is the amount of rotation the platform performs after each periodic positioning period. This is positive integer degrees.

### Commands
- HOME: Rotate the platform to the position offset from the HOME limit switch.
- RUN: Start the periodic rotations. The motor must be OFF.
- STOP: Stop the periodic rotations.
- ON: Turn the motor ON.
- OFF: Turn the motor OFF.

## Details
- The code starts in the MOTOR OFF state
- Any motor ON/OFF commands stop the periodic rotations if active.
- The HOME sequence can be started any time the motor is OFF
- The RUN sequence can only perform after the HOME command is finished.
### Postioning in degrees.
- There are only 4 hardware detected positions 0(HOME), 90, 180 and 270 using the limit switches. 
- The home offset and position movement can be set to any positive degrees up to 4X360 degrees. 
- Positioning not at 0, 90, 180, 270 or multiples must be estimated based on the time between each limit switch detection which are spaced 90 degrees. The final rotation after the last limit switch is detected will be a simple timed movement using the estimated rate of rotation.
- The HOME sequence must rotate to detect the HOME switch location twice before continueing for the amount of HOME OFFSET using the estimated rate of rotation, this offset position is the start of the periodic rotation position.
