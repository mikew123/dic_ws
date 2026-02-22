# Overview
Modify the existing arduino project "arduino/code_onoff" code which was created with the requirements in the "add_onoff.md" file to make a new arduino project "arduino/code_rotate" . Fix any errors after generating the code.

# Hardware interface to platform
The platform motor control and position limit switch sensors connect to digital pins of the ESP32.

## Position limit switches
There are 2 limit switches with transitors for voltage level conversion. High is when the switches are at the home location or one of the 4 positions at 90 degree offsets (0, 90, 180, 270). 
- The HOME switch is on pin 16. The home position switch is is triggered at position 0 which sets the platform alignment.
- The POSITION switch is on pin 18. The position switch gets triggered at each of the 4 positions 90 degrees apart.

# Ethernet/wifi
Modify the existing served web page with configuration inputs and 2 new buttons.

### Configurations
Validate all configurations are positive.
- HOME OFFSET: 
Degrees offset after the HOME limit switch is sensed. This establishes the platform periodic rotation start position after the HOME command is given. This is positive integer in degrees.
- PERIODIC PERIOD: The amount of time between start of each periodic rotation. This is positive floating seconds with 1 decimal place.
- PERIODIC ROTATION: This is the amount of rotation the platform performs after each periodic positioning period. This is positive integer degrees.

### Commands
Add these new command buttons to the existing ON and OFF buttons
- HOME: Rotate the platform to the HOME OFFSET from the last HOME limit switch trigger. The motor must be OFF to start the HOME sequence.
- RUN: Start the periodic rotations. The motor must be OFF.
- STOP: Stop the periodic rotations, turns the motor OFF.

## Details
- The code starts in the MOTOR OFF state
- Any motor ON/OFF commands stop the periodic rotations if active.
- The HOME sequence can be started any time the motor is OFF or STOP
- The RUN sequence can only perform after the HOME command is finished.

### Rotations in degrees.
The HOME and POSITION limit switches create a command signal when switching LOW to HIGH. This transition LO to HIGH and HIGH to LOW can have glitches which must be filtered out, use a 100 ms requirment to detect HI or LOW
- There are only 4 hardware detected positions 0(HOME), 90, 180 and 270 using the limit switches. 
- The home offset and position movement can be set to any positive rotation in degrees up to 4X360. 
- Rotations are tracked using the home and position limit switches and a rotataion estimate using the measured RPM when the rotation is supposed to stop between limit switch triggers. The RPM is measured using the time between each position limit switch trigger which are spaced 90 degrees. 
- When the HOME command is triggered the HOME sequence must rotate to detect the HOME switch location twice before continueing for the amount of HOME OFFSET using the estimated rate of rotation, this offset position is the start position of the periodic rotations.

# Examples
- After the HOME sequence completes and a RUN command is triggered, the PERIODIC ROTATION cycle sequences start precisely every PERIODIC PERIOD. The start of the next PERIODIC rotation cycle does NOT wait for the current periodic rotation to complete. There must be no accumulated time errors i.e. the 10th periodic cycles starts precisely at 10*(PERIODIC PERIOD) seconds after the RUN button is triggered.

## Example 1
- The HOME OFFSET is 0 degrees
- The PERIODIC ROTATION is 540 degrees (1 1/2 rotations)
- The PERIODIC PERIOD is 60 seconds
- When the HOME command is triggered the platform rotates more than 1 rotation, it stops after the 2nd HOME limit switch trigger occurs since the OFFSET is 0 degrees.
- 5 minutes after the RUN command is triggered the platform rotates 1 1/2 rotations before stopping after the 6th POSITION limit switch is detected since the ROTATION is exactly 6 X 90 degrees.
- 60 seconds after the RUN command is triggered a 2nd rotation starts. Rotations start after each 60 second period until a STOP or OFF command.

## Example 2
Assume the rotation speed is 10.0 RPM (6 seconds per rotation) which is measured while rotating between 90 degree limit switch triggers.
- The HOME OFFSET is 15 degrees
- The PERIODIC ROTATION is 540 degrees (1 1/2 rotations)
- The PERIODIC PERIOD is 60 seconds
- When the HOME command is triggered the platform rotates more than 1 rotation, it stops 15 degrees after the 2nd HOME limit switch trigger occurs since the HOME OFFSET is 15 degrees and the PERIODIC rotation is exactly 6 x 90 degrees. The 15 degrees rotation time after the 2nd HOME switch would be 0.25 seconds at 10 RPM. 
- After 5 minutes the platform rotates 1 1/2 rotations before rotating for 0.25 seconds which stops 15 degrees after the 6th POSITION limit switch. Since the RERIODIC ROTATION of 540 degrees is exactly 6 POSITION switch triggers (90 degrees between triggers) the 15 degree HOME OFFSET is the offset for each rotation.

## Example 3
Use the configuration of Example 2 but the PERIODIC ROTATION is 555 degrees (1 1/2 rotations plus 15 degrees)
- The HOME sequence is the same as Example 2
- The PERIODIC ROTATION has offsets of 30, 45, 60 ..  345 degrees after the 6th POSITION limit switch for the initial 24 rotations. The 25th rotation stops at the 7th limit switch trigger. The 26th rotation rotates 15 degrees after the 6th position limit switch trigger, then repeats with 30, 45, 60 .. 345 degrees etc.
