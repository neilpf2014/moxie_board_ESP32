# Moxie board code for ESP32

For new PRS Moxieboard controler using MQTT to send teams button presses back via Wifi
modifed to use only the MCU pins now.
Will send a string starting with UID followed by comma separated list of button presses since the 
last successful send operation.
Success is confimed by sending back the UID to the MCU.

 #*This is a WIP*
