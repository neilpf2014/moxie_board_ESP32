/* ***************************************************************************************************
NF 2019-09-24

Mashup of code from ESP8266 MQTT example --  rewritten from pub sub example to use the wrapper class
MQTThandler and Shift register button handling routine.  Since this is running on ESP32, will need to use
Stage version of ESP32 core and Dev version of WiFiManager (See PIO ini file)
This will become the new Fubar Moxie board control code
Will collect button presses into array and then send them over to server via MQTT on intervals.
* ****************************************************************************************************
*/
#include <Arduino.h>
#include <MQTThandler.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

#define NUM_BTNS 20 // number of button to be read
#define OUTPIN_1 23
#define OUTPIN_2 22
#define OUTPIN_3 21
#define OUTPIN_4 19
#define OUTPIN_5 18
#define INPIN_1 34
#define INPIN_2 35
#define INPIN_3 32
#define INPIN_4 33


uint8_t OutPins[5] = {OUTPIN_1, OUTPIN_2, OUTPIN_3, OUTPIN_4, OUTPIN_5};
uint8_t InPins[4] = {INPIN_1, INPIN_2, INPIN_3, INPIN_4};
uint32_t BtnTemp[NUM_BTNS];
uint32_t BtnRecord[NUM_BTNS];
uint32_t BtnsOutgoing[NUM_BTNS];

uint32_t CurrOutPin = 0;
uint32_t CurrInPin = 0;

uint32_t PressedBtnIndex = 0;
unsigned int LastPressIndex = 0;
unsigned int TheButton = 0;

uint64_t now;
uint64_t PasTime = 0;
uint64_t Period1 = 5;// in mill

void CheckButton()
{
   	uint32_t RetVal = 0;
   	uint32_t InPinSz = sizeof(InPins);
   	uint32_t OutPinSz = sizeof(OutPins);
   
	if (CurrInPin >= InPinSz)
		CurrInPin = 0;
	if (CurrOutPin >= OutPinSz)
		CurrOutPin = 0;
 	// Step to next button
	delayMicroseconds(5);
	if (digitalRead(InPins[CurrInPin]) == HIGH)
		BtnTemp[(CurrOutPin)+(CurrInPin*OutPinSz)]++;
	if(CurrInPin == 0)
	{
		digitalWrite(OutPins[CurrOutPin],HIGH);
		if(CurrOutPin == 0)
			digitalWrite(OutPins[OutPinSz - 1],LOW);
		else
			digitalWrite(OutPins[CurrOutPin - 1],LOW);
		CurrOutPin++;
	}
	CurrInPin++;
}


//**** Wifi and MQTT stuff below *********************

// Update these with values suitable for the broker used.
const char* svrName = "pi-iot.local"; // if you have zeroconfig working
IPAddress MQTTIp(192,168,1,117); // IP oF the MQTT broker

WiFiClient espClient;
uint64_t lastMsg = 0;
String S_msg;
String BtnArraySend; // hold CSV of button array
int value = 0;
uint8_t GotMail;
uint8_t statusCode;
uint8_t ConnectedToAP = false;
//MQTThandler MTQ(espClient, svrName);
MQTThandler MTQ(espClient, MQTTIp);

// Wifi captive portal setup on ESP8266
void configModeCallback(WiFiManager *myWiFiManager) {
   Serial.println("Entered config mode");
	Serial.println(WiFi.softAPIP());
	digitalWrite(LED_BUILTIN, HIGH);
	//if you used auto generated SSID, print it
	Serial.println(myWiFiManager->getConfigPortalSSID());
}

void WiFiCP(uint8_t ResetAP)
{
	WiFiManager wifiManager;
	wifiManager.setAPCallback(configModeCallback);
	if (ResetAP){
		wifiManager.resetSettings();
		wifiManager.autoConnect("MoxieConfigAP");
		digitalWrite(LED_BUILTIN, LOW);
	}
	else
	{
		wifiManager.autoConnect("MoxieConfigAP");
		digitalWrite(LED_BUILTIN, LOW);
	}
	
	//wifiManager.autoConnect("AutoConnectAP");
	
	Serial.println("Print IP:");
	Serial.println(WiFi.localIP());
	GotMail = false;
	MTQ.setClientName("ESP32Client");
	MTQ.subscribeIncomming("ConfirmMsg");
	MTQ.subscribeOutgoing("BtnsOut");
}
void setup() {
	pinMode(LED_BUILTIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
	Serial.begin(115200);
	WiFiCP(true);
	//WiFiManager wifiManager;
	//wifiManager.setAPCallback(configModeCallback);
	//wifiManager.autoConnect("AutoConnectAP");
	//Serial.println("Print IP:");
	//Serial.println(WiFi.localIP());
	
  //Initialize array
  // BtnRegister = new byte[numOfRegisters];
//	for (size_t i = 0; i < numOfRegisters; i++)
//	   BtnRegister[i] = 0;

	//set pins to output so you can control the shift register
	//pinMode(LATCH_PIN, OUTPUT);
	//pinMode(CLOCK_PIN, OUTPUT);
	//pinMode(DATA_PIN, OUTPUT);
	for (size_t i = 0; i < 5; i++)
		pinMode(OutPins[i], OUTPUT);

	for (size_t i = 0; i < 4; i++)
		pinMode(InPins[i], INPUT_PULLDOWN);
	Serial.println("Started");
}
// For toggle of led on/off
void LedCheck(char Ckc){
	if (Ckc == '1') {
		digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
										  // but actually the LED is on; this is because
										  // it is acive low on the ESP-01)
	}
	else if (Ckc == '0') {
		digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
	}
}
// main loop
void loop() {
	now = millis();
   // Button check 
   if(now > (PasTime + Period1)){
      PasTime = now;
      CheckButton();
	  for (size_t i = 0; i < sizeof(BtnTemp); i++)
	  {
         if (BtnTemp[i] > 2){
            Serial.print("button ");  // test code
            Serial.print(i + 1);
            Serial.println(" Pressed !");
		      BtnRecord[TheButton - 1]++;  //keep
            BtnTemp[i] = 0;
         } 
      }
	}
	GotMail = MTQ.update();
	// check status here, still test code needs work !!!!
	if (GotMail == true){
		Serial.print("message is: ");
		S_msg = MTQ.GetMsg();
		Serial.println(S_msg);
		LedCheck(S_msg.charAt(0));
		GotMail = false;
	}
	
	// push out a message every 1 min for testing remove
	// Todo make sure to code a check status before send !!!!!
	if (now - lastMsg > 60000) {
		lastMsg = now;
		++value;
		S_msg = "string message # " + String(value);
		Serial.print("Publish message (main): ");
		Serial.println(S_msg);
		statusCode = MTQ.publish(S_msg);
		// send CSV line
		memcpy(BtnRecord, BtnsOutgoing, NUM_BTNS);
		BtnArraySend = "A";
		for (size_t i = 0; i < NUM_BTNS; i++){
			BtnArraySend = BtnArraySend + "," + String(BtnsOutgoing[i]);
		}
		statusCode = MTQ.publish(BtnArraySend);
		// reset the button counter
		for (size_t i = 0; i < NUM_BTNS; i++)
			BtnRecord[i] = 0;
	}
}