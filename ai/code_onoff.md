# Overview
Generate Arduino code in the arduino folder for the WEMOS S2-mini ESP32 S2 micro controller. The code is used to control a rotating platform using a motor using a web page. Fix any errors after generating the code.

# Hardware interface to platform
The platform motor control connects to a digital pin of the ESP32.

## Motor control
The motor is controlled with a digital output to transistors to voltage level convert to 5V.
- Use pin 39. When high the motor is ON, when low the motor is OFF

# Ethernet/wifi
Create a wifi interface in AP mode on the ESP32 creating a wifi network on which to serve web page(s). The ssid and password can be changed by editing the code file.
- The ssid is dicotomy
- The password is passion

## Web page
Serve a web page that sends commands the ESP32. The web page launches when a browser connects to the IP and when the external device performs a connection to the AP wifi. The web page should be formated for a browser on phone screen.

### Commands
These are "buttons" on the 
- ON: Turn the motor ON.
- OFF: Turn the motor OFF.
