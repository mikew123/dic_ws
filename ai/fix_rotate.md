Add code to turn on LED=255 while motor is runing and LED=50 when not running
OR the HOME switch with the POSITION switch  so that all 4 90 deg positions are triggered
Start position count at -1 after 2nd HOME switch is detected while homing, this is used to offset the fact that the HOME switch added a POSITION count
Add motor ON and OFF buttons to web page that un-conditionally turn the motor ON or OFF. A MOTOR_ON state is added
HOME OFFSET remainder degree rotation is 1/10 of what it should be. i.e. OFFSET = 3 stops about 30 degrees after physical HOME.
The DELTATIME_MAX between 90 degree POSITION switches needs to be 60 seconds so the motor can operate at the slowest speed.