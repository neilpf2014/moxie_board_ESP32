/* ***************************************************************************************************
NF 2019-09-24

Mashup of code from ESP8266 MQTT example --  rewritten from pub sub example to use the wrapper class
MQTThandler.  Since this is running on ESP32, will need to use
Stage version of ESP32 core and Dev version of WiFiManager (See PIO ini file)
This now reads 20 button using 9 GPIO's
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

#define NUM_BTNS 20 // number of buttons to be read
// all of the ESP32 button GPIO's 
// This needs to match the carrier PCB
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
// used to deal with button presses
uint32_t BtnTemp[NUM_BTNS];
uint32_t BtnRecord[NUM_BTNS];
uint32_t BtnsOutgoing[NUM_BTNS];
uint32_t CurrOutPin = 0;
uint32_t CurrInPin = 0;

uint32_t PressedBtnIndex = 0;
uint32_t LastPressIndex = 0;

uint64_t now;
uint64_t PasTime = 0;
uint64_t Period1 = 5;// in mill

// each time this is called will step though array of buttons
// scans inputs then incr the output pins
void CheckButton()
{
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
   // set next output high / current one low
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
uint64_t MessID;

uint64_t msgPeriod = 5000; //Message check interval in ms
uint32_t MsgSendNewPer;
String S_msg;
String BtnArraySend; // hold CSV of button array
int value = 0;
uint8_t GotMail;
uint8_t statusCode;
uint8_t BtnMessSuccess;
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

// send CSV line via MQTT
void SendNewBtnMessage(){
	MessID = millis();
	memcpy(BtnRecord, BtnsOutgoing, NUM_BTNS);
		BtnArraySend = "M" + MessID;
		for (size_t i = 0; i < NUM_BTNS; i++){
			BtnArraySend = BtnArraySend + "," + String(BtnsOutgoing[i]);
		}
		statusCode = MTQ.publish(BtnArraySend);
		// reset the button counter
		for (size_t i = 0; i < NUM_BTNS; i++)
			BtnRecord[i] = 0;
		BtnMessSuccess = false;
}

uint8_t MessageTest(String test)
{
   String tempST;
   uint64_t MessTest = 0;
   if(S_msg.startsWith("M")){
      tempST = test.substring(2);
      MessTest = tempST.toInt();
	}
   if (MessTest == MessID)
      return true;
   else
      return false;
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

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
	Serial.begin(115200);
	WiFiCP(true);
	//WiFiManager wifiManager;
	//wifiManager.setAPCallback(configModeCallback);
	//wifiManager.autoConnect("AutoConnectAP");
	//Serial.println("Print IP:");
	//Serial.println(WiFi.localIP());
	BtnMessSuccess = false;
   MsgSendNewPer = 0;
	for (size_t i = 0; i < 5; i++)
		pinMode(OutPins[i], OUTPUT);
	for (size_t i = 0; i < 4; i++)
		pinMode(InPins[i], INPUT_PULLDOWN);
	Serial.println("Started");
}

// main loop
void loop() {
	now = millis();
   // Button check 
   if(now > (PasTime + Period1)){
      PasTime = now;
      CurrInPin = 0;
      CurrOutPin = 0;
      // Scan all buttons once
      for (size_t i = 0; i < NUM_BTNS; i++)
         CheckButton();
      // Check temp array after scan, reset if 2 hits
	   for (size_t i = 0; i < NUM_BTNS; i++)
	   {
         if (BtnTemp[i] > 2){
            Serial.print("button ");  // test code
            Serial.print(i + 1);
            Serial.println(" Pressed !");
		      BtnRecord[i]++;
            BtnTemp[i] = 0;
         }
	   }
   }
   // check status here, still test code needs work !!!!
	GotMail = MTQ.update();
	if (GotMail == true){

		//**test code to be removed ****************
		Serial.print("message is: ");
		S_msg = MTQ.GetMsg();
		Serial.println(S_msg);
		LedCheck(S_msg.charAt(0));
		// ******************************************
		BtnMessSuccess = MessageTest(S_msg);
		GotMail = false;
	}
	// check every n sec to see if last message rc'ved
	// if so push out a new message every 2 min (n * 20)
   // remove for testing 
	if (now - lastMsg > msgPeriod) {
		lastMsg = now;
      MsgSendNewPer++;
      if((BtnArraySend != "") && (!BtnMessSuccess))
         statusCode = MTQ.publish(BtnArraySend);
      if(BtnMessSuccess && (MsgSendNewPer > 20)){
         SendNewBtnMessage();
         MsgSendNewPer = 0;
      }
		//**test code to be removed ****************
		++value;
		S_msg = "string message # " + String(value);
		Serial.print("Publish message (main): ");
		Serial.println(S_msg);
		statusCode = MTQ.publish(S_msg);
		// ******************************************
		
	}
}