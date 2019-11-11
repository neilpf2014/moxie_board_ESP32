# Moxie board code for ESP32

For new PRS Moxieboard controler using MQTT to send teams button presses back via Wifi
modifed to use only the MCU pins now.

No QOS dev branch

Will send a string starting with UID followed by comma separated list of button presses.  Is not going to check for anything coming back for the MQTT broker.

 #*This is a WIP  -- todo fix mDNS*
