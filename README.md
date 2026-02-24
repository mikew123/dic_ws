# Platform control

The platform creates a wifi signal that needs to be connected to:<br>
SSID: "tech" PASSWORD: "Art723!!"<br>
Use a web browser to connect to "htpp://192.168.4.1"<br>
Configure the HOME offset degrees and periodic rotation period and degrees of rotation.<br>
<br>
The platform must be put in the "HOME" position by pressing the "HOME" button which points in the same direction each time after the homeing sequence completes. The homing sequence rotates 1 or 2 times before stopping.<br>
After the homeing is complete the "RUN" button starts the periodic rotations. The platform rotates the amount set by the periodic rotation amount and stops until the the next rotation starts. The rotations occur an the precise intervlas set by the periodic period.<br>

# Platform views

View of the bottom panel of the platform from the top. The housing in the center send the AC power to the top using a rotary coupler and has a 6" bearing on its top which fits into the ring on the bottom panel. There are 4 caster wheels and a large motor with a drive wheel which causes the top panel to rotate. Next to the motor is the motor controller. The limit switches are mounted so that the humps on the top panel activate them as the pass over the switch. <br>
![platform bottom top view](support/platform_bottom_topview.jpg) <br>
View of the top panel of the platform from the bottom. The center ring is used to keep the top panel centered using the bearing on the bottom panel. There are 4 humps which activate the limit switches on the bottom panel. <br>
![platform top bottom view](support/platform_top_bottomview.jpg) <br>
View of the bottom panel of the platform from the side.
![platform bottom sideview](support/platform_bottom_sideview.jpg) <br>
The height of the drive wheel cn be adjusted using the bolt nuts to raise the motor. The bolt threads provide  fine resolution in height adjustment.
![drivewheel adjust](support/drivewheel_adjust.jpg) <br>
View of the motor controller. The rotation rate is controlled by the SPEED knob. The switch to the left controlls CW and CCW platform rotation. The microcontroller on the right side (blue light) has the wifi interface and performs the periodic rotations by turning the motor ON/OFF.
![motor_controller](support/motor_controller.jpg) <br>
Top of the webpage which has the configuration.
![platform bottom sideview](support/webpage_top.jpg) <br>
Bottom of the webpage which has the control buttons.
![platform bottom sideview](support/webpage_bottom.jpg) <br>

# Micro-controller

![micro controller board](support/micro-controller_board.jpg) <br>
![micro controller](support/WEMOS_S2_MINI_pins.jpg) <br>