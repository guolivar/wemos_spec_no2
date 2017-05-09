/* Wireless NO2 - SPEC
WEMOS-D1-pro based NO2 sensor

 */
 #include "Arduino.h"

//Wireless and web-server libraries
#include "ESP8266WiFi.h"
#include <WiFiClient.h>
/* TODO
From here
*/
//SparkFun's Phan library for Phant service
#include <Phant.h>

// SD card libraries
#include "SPI.h"
#include "SD.h"

//RTC libraries
#include <Wire.h>
#include "RTClib.h"

//DHT library
#include "DHT.h"
//RGB LED library
#include "Adafruit_NeoPixel.h"
//Constants and definitions
#define LEDPIN D2 // Where is the WS2812B_RGB LED
#define DHTTYPE DHT22
#define DHTPIN D4     // what pin the DHT22 is connected to
#define xSDA D1
#define xSCL D3
#define chipSelect D8 // Where is the SD card chipSelect pin
#define receiveDatIndex 24 // Sensor data payload size
#define dustPin D2 // Pin for the "sleep" trigger for dust sensor
// PMS3003 variables
byte receiveDat[receiveDatIndex]; //receive data from the air detector module
byte readbuffer[128]; //full serial port read buffer for clearing
unsigned int checkSum,checkresult;
unsigned int FrameLength,Data4,Data5,Data6;
unsigned int PM1,PM25,PM10,N300,N500,N1000,N2500,N5000,N10000;
int length,httpPort;
// Setup NeoPixel
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LEDPIN, NEO_GRB + NEO_KHZ800);
bool valid_data,iamok; // Instrument status
File myFile; //Output file
File configFile; // To read the configuration from sd card
RTC_DS3231 RTC; //RTC object
DateTime curr_time; //Date-Time objets
// Set time and date for when the clock has no time
char date_in[] = "Oct 21 2015"; // Yes, it's THAT date
char time_in[] = "07:28:00";
char eol='\n';
String sdCard,fname,ssid,passwd,serialn,t_res,srv_addr,private_key,public_key,port;
unsigned int interval;
char file_fname[50],c_ssid[50],c_passwd[50],c_serialn[50];
char c_srv_addr[50],c_private_key[50],c_public_key[50];
// To get the MAC address for the wireless link
uint8_t MAC_array[6];
char MAC_char[18];
DHT dht(DHTPIN, DHTTYPE); //DHT22 initialization
String timestring(DateTime time){
	// This function gets the data from the Chronodot and converts to strings.
	String xmonth, xday, xhour, xminute, xsecond;
	// For one digit months
	if (time.month()<10){
		xmonth="0"+String(time.month());
	}
	else {
		xmonth=String(time.month());
	}
	//One digit days
	if (time.day()<10){
		xday="0"+String(time.day());
	}
	else {
		xday=String(time.day());
	}
	//For one digit hours
	if (time.hour()<10){
		xhour="0"+String(time.hour());
	}
	else {
		xhour=String(time.hour());
	}
	//One digit minutes
	if (time.minute()<10){
		xminute="0"+String(time.minute());
	}
	else {
		xminute=String(time.minute());
	}
	//One digit seconds
	if (time.second()<10){
		xsecond="0"+String(time.second());
	}
	else {
		xsecond=String(time.second());
	}
	// xx2 gives date and time when sensor data collected.
	String xx=String(time.year())+"/"+xmonth+"/"+xday;
	String xx2= xx+"\t"+xhour+":"+xminute+":"+xsecond;
	// Conversion of the month and date to a string which will be displayed as the sdCard file name
	sdCard =String(time.year())+xmonth+xday;
	return xx2;
}
void readDust(){
	while (Serial.peek()!=66){
		receiveDat[0]=Serial.read();
		yield();
	}
	Serial.readBytes((char *)receiveDat,receiveDatIndex);
	checkSum = 0;
	for (int i = 0;i < receiveDatIndex;i++){
		checkSum = checkSum + receiveDat[i];
	}
	checkresult = receiveDat[receiveDatIndex-2]*256+receiveDat[receiveDatIndex-1]+receiveDat[receiveDatIndex-2]+receiveDat[receiveDatIndex-1];
	valid_data = (checkSum == checkresult);
}

void setup() {
	iamok = true;
  Serial.begin(9600);
	Serial.setTimeout(20000);
  Serial.println("\n\nWake up");

  pinMode(BUILTIN_LED, OUTPUT);

	//Start the clock
	unsigned long tic = millis();

  // Connect D0 to RST to wake up
  pinMode(D0, WAKEUP_PULLUP);
	//Wake up the PMS3003
	pinMode(dustPin, OUTPUT);
	digitalWrite(dustPin,LOW);

	//Initialize LED
	pixels.begin(); // This initializes the NeoPixel library.
	pixels.show();
	pixels.setPixelColor(0,pixels.Color(0,0,0));
	pixels.show();
	//Initialize DHT sensor
	dht.begin();
	// RTC initialization
	Serial.println("Starting RTC");
	Wire.begin(xSDA,xSCL);
	if (!RTC.begin()) {
    Serial.println("RTC not present!");
    //BLINK PINK
		while (1 < 20){
      pixels.setPixelColor(0,pixels.Color(255,105,180));
      pixels.show();
			delay(100);
      pixels.setPixelColor(0,pixels.Color(0,0,0));
      pixels.show();
			delay(100);
		}
  }
	if (RTC.lostPower()) {
		iamok = false;
		Serial.println(F("RTC lost power! Setting time to somthing more meaningful "));
		// following line sets the RTC to the date & time defined above
		RTC.adjust(DateTime(date_in, time_in));
		int j = 1;
    //BLINK RED
		while (j < 10){
      pixels.setPixelColor(0,pixels.Color(255,0,0));
      pixels.show();
			delay(100);
      pixels.setPixelColor(0,pixels.Color(0,0,0));
      pixels.show();
			delay(100);
			j = j+1;
		}
	}
  else {
    Serial.println("RTC running OK");
  }
	curr_time = RTC.now();
	Serial.println(timestring(curr_time));

	//Read data
	//Clear the serial receive buffer
	Serial.readBytes((char *)readbuffer,128);
	int nrecs = 0;
	pixels.setPixelColor(0,pixels.Color(0,255,0));
	pixels.show();
	while ((!valid_data)||(nrecs<5)){
		readDust();
		delay(5);
		nrecs = nrecs+1;
	}
	pixels.setPixelColor(0,pixels.Color(0,0,0));
	pixels.show();
	// Get time
	String timestamp = timestring(curr_time);
	FrameLength = (receiveDat[2]*256)+receiveDat[3];
	PM1 = (receiveDat[4]*256)+receiveDat[5];
	PM25 = (receiveDat[6]*256)+receiveDat[7];
	PM10 = (receiveDat[8]*256)+receiveDat[9];
	Data4 = (receiveDat[10]*256)+receiveDat[11];
	Data5 = (receiveDat[12]*256)+receiveDat[13];
	Data6 = (receiveDat[14]*256)+receiveDat[15];
	N300 = (receiveDat[16]*256)+receiveDat[17];
	N500 = (receiveDat[18]*256)+receiveDat[19];
	N1000 = (receiveDat[20]*256)+receiveDat[21];
	Serial.print(timestamp);
	Serial.print(F(" PM2.5 = "));
	Serial.println(PM25);
	fname =String(serialn + "/" + sdCard + ".txt");
	fname.toCharArray(file_fname,fname.length()+1);
	float temperature = dht.readTemperature();
	float rh = dht.readHumidity();

	// Put the PMS3003 to sleep
	digitalWrite(dustPin,HIGH);
	Serial.println(F("I sent the dust sensor to sleep"));


	//Initialize the SD card and read configuration
	Serial.println(F("Initializing SD Card ..."));
	if (!SD.begin(chipSelect)){
		Serial.println(F("initialization failed!"));
		iamok = false;
		// BLINK YELLOW
		while (1 < 20){
			pixels.setPixelColor(0,pixels.Color(232,246,31));
			pixels.show();
			delay(100);
			pixels.setPixelColor(0,pixels.Color(0,0,0));
			pixels.show();
			delay(100);
		}
		return;
	}
	Serial.println(F("Initialization done."));
	//Read configuration options
	//wifi SSID
	//wifi PWD
	//serialnumber
	//time resolution (seconds)
	//phant server address
	//phant server port
	//phant public_key
	//phant private_key
	configFile = SD.open("config.txt",FILE_READ);
	//Read SSID
	ssid = "";
	while (configFile.peek()!='\n'){
		ssid = ssid + String((char)configFile.read());
	}
	ssid.toCharArray(c_ssid,ssid.length()+1);
	eol=configFile.read();
	//Read password
	passwd = "";
	while (configFile.peek()!='\n'){
		passwd = passwd + String((char)configFile.read());
	}
	passwd.toCharArray(c_passwd,passwd.length()+1);
	eol=configFile.read();
	//Read Serial Number
	serialn = "";
	while (configFile.peek()!='\n'){
		serialn = serialn + String((char)configFile.read());
	}
	serialn.toCharArray(c_serialn,serialn.length()+1);
	eol=configFile.read();
	//Read time resolution
	t_res = "";
	while (configFile.peek()!='\n'){
		t_res = t_res + String((char)configFile.read());
	}
	interval = t_res.toInt();
	eol=configFile.read();
	//Read data server address
	srv_addr = "";
	while (configFile.peek()!='\n'){
		srv_addr = srv_addr + String((char)configFile.read());
	}
	srv_addr.toCharArray(c_srv_addr,srv_addr.length()+1);
	eol=configFile.read();
	//Read phat server port
	port = "";
	while (configFile.peek()!='\n'){
		port = port + String((char)configFile.read());
	}
	httpPort = port.toInt();
	eol=configFile.read();

	//Read public_key
	public_key = "";
	while (configFile.peek()!='\n'){
		public_key = public_key + String((char)configFile.read());
	}
	public_key.toCharArray(c_public_key,public_key.length()+1);
	eol=configFile.read();
	//Read time private_key
	private_key = "";
	while (configFile.peek()!='\n'){
		private_key = private_key + String((char)configFile.read());
	}
	private_key.toCharArray(c_private_key,private_key.length()+1);
	configFile.close();

	// Echo the config file
	Serial.println(c_ssid);
	Serial.println(c_passwd);
	Serial.println(serialn);
	Serial.println(interval);
	Serial.println(c_srv_addr);
	Serial.println(c_public_key);
	Serial.println(c_private_key);
	//Create the folder for this serial number
	bool mkd = SD.mkdir(c_serialn);
	//Start WiFi bit
	WiFi.begin(c_ssid, c_passwd);
	// Wait for connection ... or timeout
	int tout = 0;
  WiFi.macAddress(MAC_array);
    for (int i = 0; i < sizeof(MAC_array); ++i){
      sprintf(MAC_char,"%s%02x:",MAC_char,MAC_array[i]);
    }
  Serial.println(MAC_char);
	while ((WiFi.status() != WL_CONNECTED)&&(tout<10)) {
		delay(500);
		Serial.print(".");
		tout = tout +1;
	}
	Serial.println(".");
	if (WiFi.status() != WL_CONNECTED){
		Serial.println("WiFi not connected ... timed out.");
    // Light up YELLOW
		pixels.setPixelColor(0,pixels.Color(232,246,31));
		pixels.show();
		delay(800);
		pixels.setPixelColor(0,pixels.Color(0,0,0));
		pixels.show();
	}
	WiFiClient client;
	srv_addr.toCharArray(c_srv_addr,srv_addr.length()+1);
	if (client.connect(c_srv_addr, httpPort)) {
		//Push the data to the cloud
		//Start by completing the phant post message
		//Build Phant object
		Phant phant(srv_addr,public_key,private_key);
		phant.add("pm1",PM1);
		phant.add("pm2_5",PM25);
		phant.add("pm10",PM10);
		phant.add("temperature",temperature);
		phant.add("rh",rh);
		phant.add("recordtime",timestamp);
		client.print(phant.post());
		client.stop();
	}
	else {
		Serial.println("Can't connect to Phant server!");
	}
	myFile = SD.open(file_fname,FILE_WRITE);
	if (myFile) {
		myFile.print(timestamp);
		myFile.print(F("\t"));
		myFile.print(FrameLength);
		myFile.print(F("\t"));
		myFile.print(PM1);
		myFile.print(F("\t"));
		myFile.print(PM25);
		myFile.print(F("\t"));
		myFile.print(PM10);
		myFile.print(F("\t"));
		myFile.print(Data4);
		myFile.print(F("\t"));
		myFile.print(Data5);
		myFile.print(F("\t"));
		myFile.print(Data6);
		myFile.print(F("\t"));
		myFile.print(N300);
		myFile.print(F("\t"));
		myFile.print(N500);
		myFile.print(F("\t"));
		myFile.print(N1000);
		myFile.print(F("\t"));
		myFile.print(temperature,1);
		myFile.print(F("\t"));
		myFile.println(rh,1);
		myFile.close();
		Serial.println(F("Data recorded"));
	}
	else {
		Serial.print("Error opening file: ");
		Serial.println(file_fname);
		int j = 1;
		//Blink BLUE
		while (j < 20){
			pixels.setPixelColor(0,pixels.Color(0,0,255));
			pixels.show();
			delay(100);
			pixels.setPixelColor(0,pixels.Color(0,0,0));
			pixels.show();
			delay(100);
			j = j+1;
		}
	}
	//Now go to sleep for the rest of the time interval.
	Serial.print(F("I've been awake for: "));
	unsigned long toc = millis();
	Serial.print((toc-tic));
	Serial.println(".0 miliseconds");
	unsigned long nap_time = (interval*1000) - (toc - tic);
	Serial.print("I'll try to sleep for ");
	Serial.print(nap_time);
	Serial.println(".0 miliseconds");
	// convert to microseconds
	ESP.deepSleep(nap_time * 1000);
	pixels.setPixelColor(0,pixels.Color(0,0,0));
	pixels.show();
  Serial.printf("Sleep for %d seconds\n\n", interval);
  // convert to microseconds
  ESP.deepSleep(nap_time * 1000000);
}

void loop() {
}
