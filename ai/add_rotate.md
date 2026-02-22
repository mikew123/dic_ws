# Overview
Modify the existing arduino project "arduino/code_onoff" code which was created with the requirements in the "add_onoff.md" file to make a new arduino project "arduino/code_rotate". The platform can be rotated to a HOME position then periodically rotated relative to the HOME position. Fix any errors after generating the code.

# Hardware interface to platform
The platform motor control and position limit switch sensors connect to digital pins of the ESP32.

## Position limit switches
There are 2 limit switches with transitors for voltage level conversion. High is when the switches are at the home location (position 0) or one of the 3 positions at 90, 180 and 270 degree offsets from HOME. 
- The HOME switch is on pin 16. The home position switch is is triggered at position 0 which sets the platform alignment.
- The POSITION switch is on pin 18. The position switch gets triggered at each of the 3 positions 90 degrees apart.

# Ethernet/wifi
Modify the existing served web page with configuration inputs and 2 new buttons.

### Configurations
Validate all configurations are positive.
- HOME OFFSET: 
Degrees offset after the HOME limit switch is sensed. This establishes the platform periodic rotation start position after the HOME command is given. This is positive integer in degrees. The range is 0 to 360 degrees.
- PERIODIC PERIOD: The amount of time between start of each periodic rotation. This is positive floating seconds with 1 decimal place. The range is limited to 60 to 1000 seconds.
- PERIODIC ROTATION: This is the amount of rotation the platform performs after each periodic positioning period. It is a integer pick list with 90 degrees incriments of [90, 180, 270, 360, 450, 540, 630, 720].

### Commands
Add these new command buttons to the existing ON and OFF buttons
- HOME: Rotate the platform to the HOME OFFSET from the last HOME limit switch trigger. The motor must be OFF to start the HOME sequence.
- RUN: Start the periodic rotations. The motor must be OFF/STOP and a HOME sequence executed.
- STOP: Stop the periodic rotations, turns the motor OFF.

## Details
- The code starts in the MOTOR OFF state
- Any motor ON/OFF commands stop the periodic rotations if active.
- The HOME sequence can be started any time the motor is OFF/STOP
- The RUN sequence can only perform after the HOME sequence is finished and the motor is OFF/STOP.

### Rotations in degrees.
The HOME and POSITION limit switches create a command signal when switching LOW to HIGH. This transition LO to HIGH and HIGH to LOW can have glitches which must be filtered out, use a 100 ms requirment to detect HI or LOW
- There are only 4 hardware detected positions 0(HOME), 90, 180 and 270 using the limit switches. 
- The home offset can be set to any positive rotation in degrees up to 360. 
- Rotations are tracked using the home and position limit switches and a rotataion estimate using the measured RPM when the rotation is supposed to stop between limit switch triggers. The RPM is measured using the time between each position limit switch trigger which are spaced 90 degrees. 
- When the HOME command is triggered the HOME sequence must rotate to detect the HOME switch location twice before continuing for the amount of HOME OFFSET using limit switch triggers for each 90 degrees of the offset and the remainder of the offset degrees is rotated using the estimated rate of rotation; this offset position is the start position of the periodic rotations.
- When the RUN command is triggered the platform is rotated at the configured periodic period rate for the amount of degrees configured in the periodic rotation. The periodic rotations start precisely every periodic period with no drift, the rotation will complete before the next rotation starts. The periodic rotation is in increments of 90 degrees which uses the limit switches to measure. After the last limit switch for the periodic period is triggered then the HOME offset degrees are added to the rotation.

# Examples
- After the HOME sequence completes and a RUN command is triggered, the PERIODIC ROTATION cycle sequences start immediately and continue precisely every PERIODIC PERIOD. There must be no accumulated time errors between each periodic rotation cycle, i.e. the 10th periodic cycles starts precisely at 10*(PERIODIC PERIOD) seconds after the RUN button is triggered.

## Example 1
- The HOME OFFSET is 0 degrees
- The PERIODIC ROTATION is 540 degrees (1 1/2 rotations)
- The PERIODIC PERIOD is 60 seconds
- When the HOME command is triggered the platform rotates more than 1 rotation, it stops immediately after the 2nd HOME limit switch trigger occurs since the OFFSET is 0 degrees.
- Immediately when the RUN command is triggered the platform starts to rotates 1 1/2 rotations before stopping immediately after the 6th POSITION limit switch is detected since the ROTATION is exactly 6 X 90 degrees and the HOME offset is 0 degrees.
- 60 seconds after the RUN command is triggered a 2nd rotation starts. Rotations start after each 60 second period until a STOP or OFF command.

## Example 2
Assume the rotation speed is 10.0 RPM (6 seconds per rotation) which is measured while rotating between 90 degree limit switch triggers.
- The HOME OFFSET is 15 degrees
- The PERIODIC ROTATION is 540 degrees (1 1/2 rotations)
- The PERIODIC PERIOD is 60 seconds
- When the HOME command is triggered the platform rotates more than 1 rotation, it stops 15 degrees after the 2nd HOME limit switch trigger occurs since the HOME OFFSET is 15 degrees. The 15 degrees rotation time after the 2nd HOME switch would be 0.25 seconds at 10 RPM. 
- Immediately after the RUN command the platform rotates 1 1/2 rotations relative to the HOME position. The platform rotates for 6 limit switch triggers then continues to rotate 15 degrees more based on the HOME OFFSET which is 0.25 seconds based on the estimated RPM. Since the RERIODIC ROTATION of 540 degrees is exactly 6 POSITION switch triggers (90 degrees between triggers) the 15 degree HOME OFFSET is the offset for each rotation.
- 60 seconds after the RUN command is triggered a 2nd rotation starts. Rotations start after each 60 second period until a STOP or OFF command.
