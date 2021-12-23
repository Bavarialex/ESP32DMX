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
#include <ESPAsyncE131.h>

#define UNIVERSE 1                      // First DMX Universe to listen for
#define UNIVERSE_COUNT 1


const char* ssid = "SSID";
const char* password = "WifiPassword";
const char* mqtt_server = "10.11.12.20";  //IP of mqtt-server

IPAddress staticIP(10, 11, 12, 61); //ESP static ip
IPAddress gateway(10, 11, 12, 1);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
IPAddress dns(10, 11, 12, 1);  //DNS

//  MQTT
String clientId = "ESP32DMX_";
char const* switchTopic1 = "/ESP32DMX/Reset";
WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
long int value = 0;
long rssi;
const char* WifiIP;
unsigned long lastMsg = (millis());
unsigned long currentMillis;
unsigned long timerReconnect;

// Mapping DevKit Ports to Arduino GPIO Pins
#define LEDrt 5  // WLAN
#define LEDgr 18        // blink
#define LEDbl 19     // MQTT
bool Lgruen = false;
bool Lrot = false;
bool Lblau = false;
int lauf;

const long interval = 1100;           // interval at which to blink (milliseconds)
unsigned long previousMillis = 0;        // will store last time LED was updated
int ledState = LOW;             // ledState used to set the LED

TelnetSpy SerialAndTelnet;

AsyncWebServer server(80);

#define SERIAL SerialAndTelnet

// ESPAsyncE131 instance with UNIVERSE_COUNT buffer slots
ESPAsyncE131 e131(UNIVERSE_COUNT);

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
    
    if (client.connect(clientId.c_str())) 
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
  SerialAndTelnet.setWelcomeMsg("Hier ist der ESP32DMX\n");
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
    request->send(200, "text/plain", "Hi! I am ESP32DMX.");
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
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) 
  {
    previousMillis = currentMillis;
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
      Lgruen = false;
      client.publish("/ESP32DMX/DMXactive", "false");
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
    //else 
    //{
     // digitalWrite(LEDrt, LOW);
    //}
    
    if (lauf > 2)
    {
      lauf = 0;
    }

  }

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
  setupTelnet();
  setup_wifi();
  setupOTA();
  setupElegantOTA();
  setupMQTT();
  reconnect();
      
  SERIAL.println("Chars will be echoed. Play around...\n");
  SERIAL.println("R for reboot...\n");

  // Choose one to begin listening for E1.31 data
    if (e131.begin(E131_UNICAST))                               // Listen via Unicast
    //if (e131.begin(E131_MULTICAST, UNIVERSE, UNIVERSE_COUNT))   // Listen via Multicast
        Serial.println(F("Listening for data..."));
    else 
        Serial.println(F("*** e131.begin failed ***"));
}

void loop() {
  SerialAndTelnet.handle();
  ArduinoOTA.handle();
  AsyncElegantOTA.loop();   // ElegantOTA

     // MQTT wieder verbinden  
  if (!client.connected())  
  {
    if ((millis() - timerReconnect) > 5000)
    {
      timerReconnect = millis();
      clientId = "ESP32DMX_";
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

  unsigned long now = millis();  // Statusmeldungen
  if (now - lastMsg > 60000) 
  {
    lastMsg = now;
    ++value;
    snprintf (msg, MSG_BUFFER_SIZE, "true #%ld", value);
    SERIAL.print("Publish message: ");
    SERIAL.println(msg);
    client.publish("/ESP32DMX/available", msg);
    WifiIP = WiFi.localIP().toString().c_str();
    // SERIAL.print(WifiIP);
    snprintf (msg, MSG_BUFFER_SIZE, "%s", WifiIP);
    client.publish("/ESP32DMX/IP", msg);
    rssi = WiFi.RSSI();
    snprintf (msg, MSG_BUFFER_SIZE, "%ld", rssi);
    client.publish("/ESP32DMX/rssi", msg);
    
  }

  if (!e131.isEmpty()) 
  {
        Lgruen = true;
        client.publish("/ESP32DMX/DMXactive", "true");
        e131_packet_t packet;
        e131.pull(&packet);     // Pull packet from ring buffer
        
        //Serial.printf("Universe %u / %u Channels | Packet#: %u / Errors: %u / CH1: %u\n",
        //        htons(packet.universe),                 // The Universe for this packet
        //        htons(packet.property_value_count) - 1, // Start code is ignored, we're interested in dimmer data
         //       e131.stats.num_packets,                 // Packet counter
        //        e131.stats.packet_errors,               // Packet error counter
        SERIAL.print("CH1: ");             // Dimmer data for Channel 1
        SERIAL.print(packet.property_values[1]); 
        snprintf (msg, MSG_BUFFER_SIZE, "%ld", packet.property_values[1]);
        client.publish("/ESP32DMX/CH1", msg);
        SERIAL.print("   CH2: ");             // Dimmer data for Channel 2
        SERIAL.print(packet.property_values[2]); 
        snprintf (msg, MSG_BUFFER_SIZE, "%ld", packet.property_values[2]);
        client.publish("/ESP32DMX/CH2", msg);
        SERIAL.print("   CH3: ");             // Dimmer data for Channel 3
        SERIAL.print(packet.property_values[3]); 
        snprintf (msg, MSG_BUFFER_SIZE, "%ld", packet.property_values[3]);
        client.publish("/ESP32DMX/CH3", msg);
        SERIAL.print("   CH4: ");             // Dimmer data for Channel 4
        SERIAL.print(packet.property_values[4]); 
        snprintf (msg, MSG_BUFFER_SIZE, "%ld", packet.property_values[4]);
        client.publish("/ESP32DMX/CH4", msg);
        SERIAL.print("   CH5: ");             // Dimmer data for Channel 5
        SERIAL.print(packet.property_values[5]); 
        snprintf (msg, MSG_BUFFER_SIZE, "%ld", packet.property_values[5]);
        client.publish("/ESP32DMX/CH5", msg);
        SERIAL.print("   CH6: ");             // Dimmer data for Channel 6
        SERIAL.println(packet.property_values[6]); 
        snprintf (msg, MSG_BUFFER_SIZE, "%ld", packet.property_values[6]);
        client.publish("/ESP32DMX/CH6", msg);
  }
   
  
  serialEcho();

  LEDaction();
  
}
