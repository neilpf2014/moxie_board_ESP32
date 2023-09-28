/* ***************************************************************************************************
NF 2019-11-14

Mashup of code from ESP8266 MQTT example --  rewritten from pub sub example to use the wrapper class
MQTThandler.  Since this is running on ESP32, will need to use
Stage version of ESP32 core and Dev version of WiFiManager (See PIO ini file)
This now reads 20 button using 9 GPIO's
This will become the new Fubar Moxie board control code
Will collect button presses into array and then send them over to server via MQTT on intervals.

Simplifed version with no QOS check on if the message was recv'ed
* ****************************************************************************************************
*/
#include <Arduino.h>
#include <MQTThandler.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <ESPmDNS.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

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
#define INPIN_7 25 // # 21 - 25 on board
#define INPIN_8 26 // # 26 - 30 on board
#define INPIN_5 36 // # 31 - 35 on board
#define INPIN_6	39 // # 35 - 40 on board

#define BTN_DELAY 2500 // lockout delay in ms
#define DEBUG_ON 1

#define AP_DELAY 2000
#define HARD_CODE_BROKER "192.168.1.140"
#define CONFIG_FILE "/svr-config.json"

byte debugMode = DEBUG_ON;

#define DBG(...) debugMode == DEBUG_ON ? Serial.println(__VA_ARGS__) : NULL

uint8_t OutPins[5] = {OUTPIN_1, OUTPIN_2, OUTPIN_3, OUTPIN_4, OUTPIN_5};
uint8_t InPins[4] = {INPIN_1, INPIN_2, INPIN_3, INPIN_4};

// used for debounce of btn array
uint8_t BtnTemp[NUM_BTNS];

// holds time in ms of last button press
uint64_t BtnState[NUM_BTNS];

// running count of recorded button presses
uint32_t BtnRecord[NUM_BTNS];

uint32_t CurrOutPin = 0;
uint32_t CurrInPin = 0;

uint64_t now;
uint64_t PasTime = 0;

// Btn scan freq in mill
uint64_t Period1 = 5;// in mill

// use LED for debugging
uint8_t LedState;
uint64_t LedOnTime = 0;
uint64_t LedOnPer = 500;

String BtnArraySend; // hold CSV of button array
String S_msg;

// each time this is called will step though array of buttons
// scans inputs then incr the output pins
void CheckButton()
{
   uint32_t OutPinSz = sizeof(OutPins);
   
	// Step to next button
	digitalWrite(OutPins[CurrOutPin],HIGH);
	delayMicroseconds(5);
	if (digitalRead(InPins[CurrInPin]) == HIGH)
		if(BtnTemp[(CurrOutPin)+(CurrInPin*OutPinSz)] < 255)
			BtnTemp[(CurrOutPin)+(CurrInPin*OutPinSz)]++;
	if (digitalRead(InPins[CurrInPin]) == LOW)
		BtnTemp[(CurrOutPin)+(CurrInPin*OutPinSz)] = 0;
	digitalWrite(OutPins[CurrOutPin],LOW);
	CurrOutPin++;
	if(CurrOutPin >= OutPinSz){
		CurrInPin++;
		CurrOutPin = 0;
	}
}


//**** Wifi and MQTT stuff below *********************

// Update these with values suitable for the broker used.
// Comment out old wifi code -- remove once we know the new stuff works
/* **************************************************************************
const char* svrName = "pi-iot"; // if you have zeroconfig working
IPAddress MQTTIp(192,168,1,26); // IP oF the MQTT broker if not

WiFiClient espClient;
uint64_t lastMsg = 0;
unsigned long MessID;

uint64_t msgPeriod = 10000; //Message check interval in ms (10 sec for testing)

String S_msg;
String BtnArraySend; // hold CSV of button array
int value = 0;
uint8_t GotMail;
uint8_t statusCode;
uint8_t ConnectedToAP = false;
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
	//wifiManager.setAPCallback(configModeCallback);
	if (ResetAP){
		wifiManager.resetSettings();
		wifiManager.setHostname("MoxieBoard");
		wifiManager.autoConnect("MoxieConfigAP");
	}
	else
	{
		//wifiManager.setHostname("MoxieBoard");
		wifiManager.autoConnect("MoxieConfigAP");
	}

	// these are used for debug
	Serial.println("Print IP:");
	Serial.println(WiFi.localIP());
	// **************************
	GotMail = false;
	MTQ.setClientName("ESP32Client");
	MTQ.subscribeIncomming("ConfirmMsg");
	MTQ.subscribeOutgoing("BtnsOut");
}

// use to get ip from mDNS, return true if sucess
uint8_t mDNShelper(void){
	uint8_t logflag = true;
	unsigned int mdns_qu_cnt = 0;

	if (!MDNS.begin("esp32whatever")){
    	Serial.println("Error setting up MDNS responder!");
		logflag = false;
		}
	else 
    	Serial.println("Finished intitializing the MDNS client...");
	MQTTIp = MDNS.queryHost(svrName);
  	while ((MQTTIp.toString() == "0.0.0.0") && (mdns_qu_cnt < 10)) {
    	Serial.println("Trying again to resolve mDNS");
    	delay(250);
    	MQTTIp = MDNS.queryHost(svrName);
		mdns_qu_cnt++;
	  }
	  if(MQTTIp.toString() == "0.0.0.0")
	  	logflag = false;
	  return logflag;
}
*/
// ******************************************************************
// New Wifi code here
// IPAddress MQTTIp(192, 168, 1, 140); // IP oF the MQTT broker if not 192.168.1.183
const char* svrName = "pi-iot"; // if you have zeroconfig working
IPAddress MQTTIp(192,168,1,26); // IP oF the MQTT broker if not

WiFiClient espClient;
uint64_t lastMsg = 0;
unsigned long MessID;

uint64_t msgPeriod = 10000; // Message check interval in ms (10 sec for testing)

String S_Stat_msg;
String S_Match;
String sBrokerIP;
int value = 0;
uint8_t GotMail;
uint8_t statusCode;
bool SaveConf_flag = false;
bool Use_def_IP_flag = false;

uint8_t ConnectedToAP = false;
MQTThandler MTQ(espClient, MQTTIp);
const char *outTopic = "BtnsOut";
const char *inTopic = "ConfirmMsg";

// used to get JSON config
uint8_t GetConfData(void)
{
  uint8_t retVal = 1;
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    if (SPIFFS.exists(CONFIG_FILE))
    {
      File CfgFile = SPIFFS.open(CONFIG_FILE, "r");
      if (CfgFile.available())
      {
        StaticJsonDocument<512> jsn;
        DeserializationError jsErr = deserializeJson(jsn, CfgFile);
        serializeJsonPretty(jsn, Serial); // think this is just going to print to serial
        if (!jsErr)
        {
          sBrokerIP = jsn["BrokerIP"].as<String>();
          retVal = 0;
        }
        else
          DBG("JSON error");
      }
    }
    else
      DBG("File_not_found");
  }
  return retVal;
}

// used to save config as JSON
uint8_t SaveConfData(String sIP)
{
  uint8_t retVal = 1;
  // SPIFFS.format();
  if (SPIFFS.begin(true))
  {

    StaticJsonDocument<512> jsn;
    jsn["BrokerIP"] = sIP;
    File CfgFile = SPIFFS.open(CONFIG_FILE, "w");
    if (CfgFile)
    {
      if (serializeJson(jsn, CfgFile) != 0)
      {
        retVal = 0;
        DBG("wrote something");
      }
      else
        DBG("failed to write file");
    }
    CfgFile.close();
  }
  else
  {
    retVal = 1;
    DBG("failed to open file");
  }

  return retVal;
}

// Wifi captive portal setup on ESP8266
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  // if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  digitalWrite(LED_BUILTIN, HIGH); // LED_BUILTIN is the horn !!
  Use_def_IP_flag = true;
}
void saveConfigCallback()
{
  // Set save config flag
  SaveConf_flag = true;
}

// Split wifi config into 2 procs to avoid code duplication
// this does most of the heavy lifting
// Saving of Params is working now !!
void WiFiCP(WiFiManager &WFM)
{
  bool validIP;
  uint8_t loadedFile;
  uint8_t savedFile;
  uint8_t isConnected;
  bool replaceHCIP;
  String sIPaddr;
  IPAddress MQTTeIP;

  WFM.setSaveConfigCallback(saveConfigCallback);
  WFM.setAPCallback(configModeCallback);
  replaceHCIP = false;
  WiFiManagerParameter TB_brokerIP("TBbroker", "MQTT broker IP", "192.168.1.140", 30);
  WFM.setHostname("MoxieBoard");
  WFM.addParameter(&TB_brokerIP);
  isConnected = WFM.autoConnect("MoxieConfigAP");
  if (isConnected)
  {
    DBG("Connected");
    loadedFile = GetConfData();
    // load from file ignore TB
    if (!Use_def_IP_flag)
    {
      DBG("loaded IP from File");
      validIP = MQTTeIP.fromString(sBrokerIP);
      if (validIP)
        replaceHCIP = true;
    }
    else
    {
      sIPaddr = TB_brokerIP.getValue();
      DBG("Used IP from TB");
      if (!sIPaddr.isEmpty())
      {
        validIP = MQTTeIP.fromString(sIPaddr);
        if (validIP)
        {
          replaceHCIP = true;
          if (SaveConf_flag == true)
          {
            sBrokerIP = sIPaddr;
            SaveConf_flag = SaveConfData(sIPaddr);
          }
        }
      }
    }
    if (replaceHCIP == true)
    {
      MQTTIp = MQTTeIP;
      DBG("replaced default");
    }
  }
}

// called to set up wifi -- Part 1 of 2
void WiFiConf(uint8_t ResetAP)
{
  WiFiManager wifiManager;
  if (ResetAP)
  {
    wifiManager.resetSettings();
    WiFiCP(wifiManager);
  }
  else
    WiFiCP(wifiManager);
  // these are used for debug
  Serial.println("Print IP:");
  Serial.println(WiFi.localIP());
  // **************************
  GotMail = false;
  MTQ.setClientName("ESP32Client");
  MTQ.subscribeIncomming(inTopic);
  MTQ.subscribeOutgoing(outTopic);
}
// *******************************************************************

// send CSV line via MQTT
void SendNewBtnMessage(){
	MessID = millis();
	BtnArraySend = "M" + String(MessID);
	for (size_t i = 0; i < NUM_BTNS; i++){
		BtnArraySend = BtnArraySend + "," + String(BtnRecord[i]);
	}
	statusCode = MTQ.publish(BtnArraySend);
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

// toggle LED for testing
uint8_t TogLed(uint8_t state){
   if (state < 1) {
		digitalWrite(LED_BUILTIN, LOW);
      state = 1;
	}
	else if (state > 0) {
		digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
      state = 0;
	}
   return state;
}

// separate the Wifi / MQTT init from other setup stuff
void IOTsetup()
{
  bool testIP;
  int btnSta;
  uint64_t APmodeCKtimer;
  uint8_t Btnstate;
  uint8_t tempint;
  APmodeCKtimer = millis();
  Btnstate = 0;

  // Will wait 2 sec and check for reset to be held down / pressed
  digitalWrite(OUTPIN_1, HIGH);
  while ((APmodeCKtimer + AP_DELAY) > millis())
  {
	btnSta = digitalRead(INPIN_1);
	if (btnSta == HIGH)
      Btnstate = 1;
  }
  String TempIP = MQTTIp.toString();
  // these lines set up the access point, mqtt & other internet stuff
  WiFiConf(Btnstate);
  // comment this out so we can enter broker IP on setup
  /*
  testIP = mDNShelper();
  if (!testIP){
    MQTTIp.fromString(TempIP);
  }
  Serial.print("IP address of server: ");
  */
  Serial.print("IP address of broker: ");
  Serial.println(MQTTIp.toString());
  MTQ.setServerIP(MQTTIp);
  digitalWrite(LED_BUILTIN, LOW); // turn off the light if on from config
  // **********************************************************
}

void setup() {
	bool testIP;
	String TempIP = MQTTIp.toString();
	pinMode(LED_BUILTIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
	Serial.begin(115200); //debug over USBserial
	//WiFiCP(false);
	// initalize pins
	pinMode(LED_BUILTIN,OUTPUT);
	for (size_t i = 0; i < 5; i++)
		pinMode(OutPins[i], OUTPUT);
	for (size_t i = 0; i < 4; i++)
		pinMode(InPins[i], INPUT_PULLDOWN);
	void IOTsetup();
	/*
	testIP = mDNShelper();
	if (!testIP){
		MQTTIp.fromString(TempIP);
	}
	Serial.print("IP address of server: ");
	Serial.println(MQTTIp.toString());
	MTQ.setServerIP(MQTTIp);
	*/
	delay(1000)
	Serial.println("Started Moxie !!!!");
	LedState = 1;
	LedState = TogLed(LedState);
	LedOnTime = millis();
}

// main Moxie loop
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
         if (BtnTemp[i] > 2)
            if (BtnState[i] == 0)
            	BtnState[i] = millis(); // store curr mils of current button pressed 
	   }
	   // this is where we record the button press count / reset after the lockout period
   	for(size_t i = 0; i < NUM_BTNS; i++){
         if((BtnState[i] > 0) && (now > (BtnState[i] + BTN_DELAY))){
			// debug code
			Serial.print("button ");  
            Serial.print(i + 1);
            Serial.println(" Cycled !");
			// ***********
		      BtnRecord[i]++;
            BtnState[i] = 0;
         }
      }
   }
   // MQTT send / rec and check for incomming
	GotMail = MTQ.update();
	if (GotMail == true){

		//** debug code will blink LED on incomming****
		Serial.print("message is: ");
		S_msg = MTQ.GetMsg();
		Serial.println(S_msg);
		LedCheck(S_msg.charAt(0));
		// ******************************************
		GotMail = false;
	}
	// push out button press results every x Msec
   
	if (now - lastMsg > msgPeriod) {
		lastMsg = now;
		LedState = 1;
		LedState = TogLed(LedState);
		LedOnTime = millis();
      	SendNewBtnMessage();
		//** debug code can be removed ****************
		Serial.println(BtnArraySend);
		++value;
		S_msg = "string message # " + String(value);
		Serial.print("Publish message (main): ");
		Serial.println(S_msg);
		// ******************************************
    }
	if (LedOnTime + LedOnPer < now){
		LedState = 0;
		LedState = TogLed(LedState);
		LedOnTime = 0;
	}
}