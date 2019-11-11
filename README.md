# Moxie board code for ESP32

For new PRS Moxieboard controler using MQTT to send teams button presses back via Wifi.</br>
Use scan of button from array of MCU pins.

No QOS for recpt of messages

Will send a string starting with M(millisecs since ESP32 Boot); followed by comma separated list of button presses.  This is not going to check for anything coming back for the MQTT broker.
</br>
 #*mDNS works now*
