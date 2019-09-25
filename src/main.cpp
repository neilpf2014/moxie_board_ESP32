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

#define NUM_BTNS 8 // number of button to be read
#define LATCH_PIN GPIO_NUM_18
#define CLOCK_PIN GPIO_NUM_12
#define DATA_PIN GPIO_NUM_11
int readPin1 = GPIO_NUM_19;
int readPin2 = GPIO_NUM_21;
int numOfRegisters = 1;
byte* BtnRegister; //SR for buttons

uint32_t BtnRecord[NUM_BTNS];
unsigned int CurrSRIndex = 0;
unsigned int PressedBtnIndex = 0;
unsigned int LastPressIndex = 0;
unsigned int TheButton = 0;

unsigned long now;
uint64_t PasTime = 0;
uint64_t Period1 = 5;// in mill

// change pin value on SR
void SRWrite(int pin, bool state){
	//Determines register
	int reg = pin / 8;
	//Determines pin for actual register
	int actualPin = pin - (8 * reg);
	digitalWrite(LATCH_PIN, LOW);
	for (int i = 0; i < numOfRegisters; i++){
		//Get actual states for register
		byte* states = &BtnRegister[i];

		//Update state
		if (i == reg){
			bitWrite(*states, actualPin, state);
		}

		//Write
	   shiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, *states);
	}
   digitalWrite(LATCH_PIN, HIGH);
}

unsigned int CheckButton()
{
   uint32_t RetVal = 0;
   LastPressIndex = PressedBtnIndex;
   CurrSRIndex++;
	// Step to next button in SR
      if (CurrSRIndex >= NUM_BTNS)
         CurrSRIndex = 1;
      SRWrite(CurrSRIndex,true);
	// read GPIO, if high current button is pressed
      delayMicroseconds(15);
      if (digitalRead(readPin1) == HIGH)
         PressedBtnIndex = CurrSRIndex;
      if (digitalRead(readPin2) == HIGH)
         PressedBtnIndex = CurrSRIndex * 2;
      //delayMicroseconds(5);
	// Set cur button SR low again
      SRWrite(CurrSRIndex,false);
	// our test to see if button is pressed
      if((LastPressIndex > 0)&&(LastPressIndex == PressedBtnIndex))
         RetVal = PressedBtnIndex;
      else
         LastPressIndex = 0;
      return RetVal;
}

//**** Wifi and MQTT stuff below *********************

// Update these with values suitable for the broker used.
const char* svrName = "pi-iot.local"; // if you have zeroconfig working
IPAddress MQTTIp(192,168,1,117); // IP oF the MQTT broker

WiFiClient espClient;
unsigned long lastMsg = 0;
String S_msg;
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
   BtnRegister = new byte[numOfRegisters];
	for (size_t i = 0; i < numOfRegisters; i++)
	   BtnRegister[i] = 0;

	//set pins to output so you can control the shift register
	pinMode(LATCH_PIN, OUTPUT);
	pinMode(CLOCK_PIN, OUTPUT);
	pinMode(DATA_PIN, OUTPUT);
   pinMode(readPin1, INPUT_PULLDOWN);
   pinMode(readPin2, INPUT_PULLDOWN);
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
      TheButton = CheckButton();
      if (TheButton > 0){
         Serial.print("button ");  // test code
         Serial.print(TheButton);
         Serial.println(" Pressed !");
		 BtnRecord[TheButton - 1]++;  //keep
      } 
   }
	GotMail = MTQ.update();
	if (GotMail == true){
		Serial.print("message is: ");
		S_msg = MTQ.GetMsg();
		Serial.println(S_msg);
		LedCheck(S_msg.charAt(0));
		GotMail = false;
	}
	
	// push out a message every 2 sec
	if (now - lastMsg > 2000) {
		lastMsg = now;
		++value;
		S_msg = "string message # " + String(value);
		Serial.print("Publish message (main): ");
		Serial.println(S_msg);
		statusCode = MTQ.publish(S_msg);
	}
}