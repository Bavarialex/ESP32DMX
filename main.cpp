#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <TelnetSpy.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Ticker.h>
#include "time.h"
#include "uRTCLib.h"

const char* ssid = "ASComWomo";
const char* password = "25101964";
const char* mqtt_server = "10.11.13.10";

IPAddress staticIP(10, 11, 13, 20); //ESP static ip
IPAddress gateway(10, 11, 13, 1);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
IPAddress dns(10, 11, 13, 1);  //DNS

// ntp
const char* ntpServer = "10.11.12.1";  // Fritzbox
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
struct tm timeinfo;

// rtc
uRTCLib rtc(0x68);
char Wochentag[][3] = {"0", "Mo", "Di", "Mi", "Do", "Fr", "Sa", "So" };

//  MQTT
String clientId = "WomoUhr_";
char const* switchTopic1 = "/WomoUhr/Reset";
WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
long int value = 0;
long rssi;
unsigned long lastMsg = (millis());
unsigned long currentMillis;
unsigned long timerReconnect;

LiquidCrystal_I2C lcd(0x27, 20, 4);

// Mapping DevKit Ports to Arduino GPIO Pins
#define LEDrt 5         // WLAN
#define LEDgr 18        // blink
#define LEDbl 19        // MQTT

const int U1Pin = 34;   // U1 messen Spannungsteiler 12k - 3,3k
const int U2Pin = 35;   // U2 messen Spannungsteiler 12k - 3,3k
const int U3Pin = 32;   // U3 - EBL solar 2k an Masse

float U1;
float U2;
float U3;
long int U1Pro;
long int U2Pro;
float U3Amp;
float U3Watt;
char data1[6];
char data2[6];
char data3[6];
char data4[6];
bool Lgruen = false;
bool Lrot = false;
bool Lblau = false;
int lauf;
bool uhrgestellt = false;

TelnetSpy SerialAndTelnet;

AsyncWebServer server(80);

#define SERIAL SerialAndTelnet

// Timers
Ticker StatusLED;   
Ticker StatusMQTT;
Ticker GetLocalTime;
Ticker Messung;


void callback(char* topic, byte* payload, unsigned int length) 
{
  String topicStr = topic;
  SERIAL.print("Message arrived [");
  SERIAL.print(topicStr);
  SERIAL.print("] ");
  for (unsigned int i = 0; i < length; i++) 
    {
     SERIAL.print((char)payload[i]);
    }
    
   SERIAL.println();

   if (topicStr == switchTopic1)
   {
     if (payload[0] == '1')
      {
        //nix
      }       
      
      else
         {
         // auchnix
         }
   }
}

void reconnect() 
{
    SERIAL.print("Attempting MQTT connection...");   
    clientId += String(random(0xffff), HEX); // Create a random client ID
    
    if (client.connect(clientId.c_str()) && WL_CONNECTED) 
    {   // Attempt to connect
        SERIAL.println("MQTT Connected");
        client.subscribe(switchTopic1);
        delay(50);
        Lblau = true;
    }
    else 
    {        
        SERIAL.print("failed, rc=");
        SERIAL.print(client.state());
        Lblau = false;
        SERIAL.println(" try again in 5 seconds");
        
     }
  
}

void setup_wifi() 
{
  delay(10);
     
  // We start by connecting to a WiFi network
  SERIAL.println();
  SERIAL.print("Connecting to ");
  SERIAL.println(ssid);
  WiFi.config(staticIP, gateway, subnet, dns);
  WiFi.setHostname("ESP32WomoUhr");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) 
  {
      delay(500);
      SERIAL.print(".");
      Lrot = false;
  }

  randomSeed(micros());

  SERIAL.println("");
  SERIAL.println("WiFi connected");
  SERIAL.println("IP address: ");
  SERIAL.println(WiFi.localIP());
  SERIAL.println("RSSI:");
  rssi = WiFi.RSSI();
  SERIAL.println(rssi);
  Lrot = true;
}

void telnetConnected() {
  SERIAL.println("Telnet connection established.");
}

void telnetDisconnected() {
  SERIAL.println("Telnet connection closed.");
}

void serialEcho()
  {
  // Serial Echo
  if (SERIAL.available() > 0) 
  {
    char c = SERIAL.read();
    switch (c) 
    {
      case '\r':
        SERIAL.println();
        break;
      case 'C':
        // nix
        break;
      case 'D':
        // nix
        break;
      case 'R':
        ESP.restart();
        break;
      default:
        SERIAL.print(c);
        break;
    }
  }
}

void setupTelnet()
{
  SerialAndTelnet.setWelcomeMsg("Hier ist der WomoUhr\n");
  SerialAndTelnet.setCallbackOnConnect(telnetConnected);
  SerialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);
  SERIAL.begin(74880);
  delay(100); // Wait for serial port
  SERIAL.setDebugOutput(false);
}

void setupOTA()
{
  // During updates "over the air" the telnet session will be closed.
  // So the operations of ArduinoOTA cannot be seen via telnet.
  // So we only use the standard "Serial" for logging.
  ArduinoOTA.onStart([]() {
    // Lgruen = false;
    // Lrot = false;
    // Lblau = false;
    digitalWrite(LEDrt, LOW);
    digitalWrite(LEDgr, LOW);
    digitalWrite(LEDbl, LOW);
    StatusLED.detach(); 
    StatusMQTT.detach();
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) SERIAL.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) SERIAL.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) SERIAL.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) SERIAL.println("Receive Failed");
    else if (error == OTA_END_ERROR) SERIAL.println("End Failed");
  });
  ArduinoOTA.begin();
}

void setupElegantOTA()
{
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! I am WomoUhr.");
  });

  AsyncElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  SERIAL.println("HTTP server started");
}

void setupMQTT()
{
  Wire.begin();
  client.setServer(mqtt_server, 1883);
  client.setKeepAlive(120);
  client.setCallback(callback);
}

void LEDaction()
{
  lauf = lauf + 1;

  if (Lrot == false && lauf == 1)
  {
    lauf = 2;
  }
  if (Lrot == true && lauf == 1)
  {
    digitalWrite(LEDrt, HIGH);
    digitalWrite(LEDgr, LOW);
    digitalWrite(LEDbl, LOW);
  }
  else 
  {
    digitalWrite(LEDrt, LOW);
  }
  if (Lgruen == false && lauf == 2)
  {
    lauf = 3;
  }
  if (Lgruen == true && lauf == 2)
  {
    digitalWrite(LEDgr, HIGH);
    digitalWrite(LEDbl, LOW);
    digitalWrite(LEDrt, LOW);
  }
  else 
  {
    digitalWrite(LEDgr, LOW);
  }

  if (Lblau == false && lauf ==3)
  {
    lauf = 4;
  }
  if (Lblau == true && lauf == 3)
  {
    digitalWrite(LEDbl, HIGH);
    digitalWrite(LEDrt, LOW);
    digitalWrite(LEDgr, LOW);
  }
  else
  {
    digitalWrite(LEDbl, LOW);
  }

  if (Lrot == false && lauf == 4)
  {
    lauf = 0;
  }
  if (Lrot == true && lauf == 4)
  {
    digitalWrite(LEDrt, HIGH);
    digitalWrite(LEDgr, LOW);
    digitalWrite(LEDbl, LOW);
    lauf = 1;
  }
    
  if (lauf > 2)
  {
    lauf = 0;
    Lgruen = false;
  }
}

void NTPgetLocalTime()
{
  if (uhrgestellt == false)
  { 
  if(!getLocalTime(&timeinfo))
    {
      SERIAL.println("Failed to obtain time");
      return;
    }
  
  SERIAL.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  SERIAL.print("Day of week: ");
  SERIAL.println(&timeinfo, "%A");
  SERIAL.print("Month: ");
  SERIAL.println(&timeinfo, "%B");
  SERIAL.print("Day of Month: ");
  SERIAL.println(&timeinfo, "%d");
  SERIAL.print("Year: ");
  SERIAL.println(&timeinfo, "%Y");
  SERIAL.print("Hour: ");
  SERIAL.println(&timeinfo, "%H");
  SERIAL.print("Hour (12 hour format): ");
  SERIAL.println(&timeinfo, "%I");
  SERIAL.print("Minute: ");
  SERIAL.println(&timeinfo, "%M");
  SERIAL.print("Second: ");
  SERIAL.println(&timeinfo, "%S");
  

  char timeHour[3];
  strftime(timeHour,3, "%H", &timeinfo);
  char timeMinutes[3];
  strftime(timeMinutes,3, "%M", &timeinfo);
  char timeSec[3];
  strftime(timeSec,3, "%S", &timeinfo);
  char timeDay[10];
  strftime(timeDay,10, "%d", &timeinfo);
  char timeDayofWeek[3];
  strftime(timeDayofWeek,3, "%u", &timeinfo);
  char timeMonth[12];
  strftime(timeMonth,12, "%m", &timeinfo);
  char timeYear[3];
  strftime(timeYear,3, "%y", &timeinfo);

  // set rtc
  byte sec = byte(atoi(timeSec));
  byte min = byte(atoi(timeMinutes));
  byte hour = byte(atoi(timeHour));
  byte dow = byte(atoi(timeDayofWeek));
  byte tday = byte(atoi(timeDay));
  byte mon = byte(atoi(timeMonth));
  byte yr = byte(atoi(timeYear));

  rtc.set(sec, min, hour, dow, tday, mon, yr);
	//  RTCLib::set(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
  delay(100);
  SERIAL.println("Uhr gestellt...");
  uhrgestellt = true;
  delay(100);
  }
}

void SendStatus()
{
  NTPgetLocalTime();
  delay(500);
  ++value;
  snprintf (msg, MSG_BUFFER_SIZE, "true #%ld", value);
  SERIAL.print("Publish message: ");
  SERIAL.println(msg);
  if (WiFi.status() == WL_CONNECTED) 
  {
    client.publish("/WomoUhr/available", msg);
    rssi = WiFi.RSSI();
    snprintf (msg, MSG_BUFFER_SIZE, "%ld", rssi);
    client.publish("/WomoUhr/rssi", msg);
    // snprintf (msg, MSG_BUFFER_SIZE, "%ld", data1);
    client.publish("/WomoUhr/U1", data1);
    client.publish("/WomoUhr/U2", data2);
    snprintf (msg, MSG_BUFFER_SIZE, "%ld", U1Pro);
    client.publish("/WomoUhr/P1", msg);
    snprintf (msg, MSG_BUFFER_SIZE, "%ld", U2Pro);
    client.publish("/WomoUhr/P2", msg);
    //snprintf (msg, MSG_BUFFER_SIZE, "%ld", U3Amp);
    client.publish("/WomoUhr/LadenA", data3);
    //snprintf (msg, MSG_BUFFER_SIZE, "%ld", U3Watt);
    client.publish("/WomoUhr/LadenW", data4);
  }
  else
  {
    value = value - 1;
  }
  
}

void Mess()
{
  Lgruen = true;
  U1 = analogRead(U1Pin) * 0.00395;
  sprintf(data1, "%.1f", U1);
  U2 = analogRead(U2Pin) * 0.00395;
  sprintf(data2, "%.1f", U2);
  U3 = analogRead(U3Pin) * 0.395;
  U3Amp = U3 / 2; // 1V an 2k sind 10A - stimmt nich: 1V sind 5A
  sprintf(data3, "%.1f", U3Amp/10);
  U3Watt = U1 * U3Amp; // Wattanzeige in Bezug zu Batteriespannung
  sprintf(data4, "%.1f", U3Watt/10);
  SERIAL.print(String(U3));
  SERIAL.print(" Volt, ");
  SERIAL.print(data3);
  SERIAL.print(" Amps, ");
  SERIAL.print(data4);
  SERIAL.println(" Watts ");
  SERIAL.print("Aufbau: ");
  SERIAL.print(U1);
  SERIAL.println(" V");
 

  U1Pro = 0;  
  if (U1 >= 10.5)  {    U1Pro = 10;  }
  if (U1 >= 11.31)  {    U1Pro = 20;  }
  if (U1 >= 11.58)  {    U1Pro = 30;  }
  if (U1 >= 11.79)  {    U1Pro = 40;  }
  if (U1 >= 11.9)  {    U1Pro = 50;  }
  if (U1 >= 12.08)  {    U1Pro = 60;  }
  if (U1 >= 12.2)  {    U1Pro = 70;  }
  if (U1 >= 12.31)  {    U1Pro = 80;  }
  if (U1 >= 12.42)  {    U1Pro = 90;  }
  if (U1 >= 12.72)  {    U1Pro = 100;  }
  U2Pro = 0;  
  if (U2 >= 10.5)  {    U2Pro = 10;  }
  if (U2 >= 11.31)  {    U2Pro = 20;  }
  if (U2 >= 11.58)  {    U2Pro = 30;  }
  if (U2 >= 11.79)  {    U2Pro = 40;  }
  if (U2 >= 11.9)  {    U2Pro = 50;  }
  if (U2 >= 12.08)  {    U2Pro = 60;  }
  if (U2 >= 12.2)  {    U2Pro = 70;  }
  if (U2 >= 12.31)  {    U2Pro = 80;  }
  if (U2 >= 12.42)  {    U2Pro = 90;  }
  if (U2 >= 12.72)  {    U2Pro = 100;  }
}

void setup() 
{
  pinMode(LEDbl, OUTPUT);
  pinMode(LEDrt, OUTPUT);
  pinMode(LEDgr, OUTPUT);
  digitalWrite(LEDbl, LOW);
  digitalWrite(LEDrt, LOW);
  digitalWrite(LEDgr, LOW);
  Wire.begin();
  lcd.begin(20, 4, LCD_5x8DOTS);
  lcd.backlight();
  // lcd.setBacklight(LOW);
  setupTelnet();
  setup_wifi();
  setupOTA();
  setupElegantOTA();
  setupMQTT();
  reconnect();
  URTCLIB_WIRE.begin(); // rtc
  rtc.enableBattery();

  // Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  NTPgetLocalTime();

  // timers
  StatusLED.attach( 1, LEDaction); 
  StatusMQTT.attach( 60, SendStatus);
  GetLocalTime.attach( 2000, NTPgetLocalTime);
  Messung.attach(40,Mess);

  SendStatus();
      
  SERIAL.println("Chars will be echoed. Play around...\n");
  SERIAL.println("R for reboot...\n");
}

void loop() 
{
  SerialAndTelnet.handle();
  ArduinoOTA.handle();
  AsyncElegantOTA.loop();   // ElegantOTA

  String zeitOnline;

  // MQTT wieder verbinden  
  if (!client.connected())  
  {
    if ((millis() - timerReconnect) > 5000)
    {
      timerReconnect = millis();
      clientId = "WomoUhr_";
      setupMQTT();
      reconnect();
    }
  }
  client.loop();

  // Wifi wieder verbinden  
  if (WiFi.status() != WL_CONNECTED) 
  {
    SERIAL.println("Reconnect WiFi...");
    Lrot = false;
    WiFi.config(staticIP, gateway, subnet, dns);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    delay(500);
    // setup_wifi();
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Lrot = true;
  }
    
  // LCD
  lcd.setCursor(0, 0); 
  lcd.print(Wochentag[rtc.dayOfWeek()]);
  lcd.print(" ");
  lcd.print(rtc.day());
  lcd.print(".");
  lcd.print(rtc.month());
  lcd.print(".");
  lcd.print(rtc.year());
  //SERIAL.print("Jahr  ");
  //SERIAL.println(rtc.year());
  lcd.print(" ");
  lcd.setCursor(12, 0);
  lcd.print(rtc.hour());
  //SERIAL.print(rtc.hour());
  lcd.print(":");
  String min = "";
  if (rtc.minute() < 10)
  {
    min = "0";
  }
  min = min + String(rtc.minute());
  //SERIAL.print(min);
  lcd.print(min);
  lcd.print(":");
  String sek = "";
  if (rtc.second() < 10)
  {
    sek = "0";
  }
  sek = sek + String(rtc.second());
  // SERIAL.println(sek);
  lcd.print(sek);
  lcd.print(" ");

  lcd.setCursor(0, 1); 
  lcd.print("B: ");
  lcd.print(data1);
  lcd.print("V ");

  lcd.setCursor(9, 1); 
  lcd.print(U1Pro);
  lcd.print("% ");

  lcd.setCursor(15, 1); 
  lcd.print(data3);
  lcd.print("A ");

  lcd.setCursor(14, 2); 
  lcd.print(data4);
  lcd.print("W ");

  lcd.setCursor(0, 2); 
  lcd.print("K: ");
  lcd.print(data2);
  lcd.print("V ");
  lcd.setCursor(9, 2); 
  lcd.print(U2Pro);
  lcd.print("% ");

  lcd.setCursor(0, 3); 
  if (Lrot == true)
  {
    lcd.print("W");
  }
  else
  {
    lcd.print(" ");
  }
  lcd.setCursor(9, 3); 
  lcd.print(WiFi.localIP());
  lcd.setCursor(1, 3); 
  if (Lblau == true)
  {
    lcd.print("M");
  }
  else
  {
    lcd.print(" ");
  }
  lcd.setCursor(0, 2); 
  // lcd.print("Zeile 3");
  
  lcd.setCursor(3, 3); 
  if (value > 9999)
  {
    zeitOnline = "9999+";
  }
  else
  {
    zeitOnline = String(value);
  }
  
  lcd.print(zeitOnline);
  
  serialEcho();

  // rtc
  rtc.refresh();
  
}