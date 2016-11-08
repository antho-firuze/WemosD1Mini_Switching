#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ThingSpeak.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <TimeLib.h>

boolean doSerial=true;      
#define sb    Serial.begin(9600)        // shortcuts for serial output
#define sp    Serial.print
#define spf   Serial.printf
#define spln  Serial.println
 
/*-------- Telnet code ----------*/
// start telnet server (do NOT put in setup())
const uint16_t aport = 23; // standard port
WiFiServer TelnetServer(aport);
WiFiClient TelnetClient;

void tp(String output) {
  if (!TelnetClient)  // otherwise it works only once
        TelnetClient = TelnetServer.available();
  if (TelnetClient.connected()) {
    TelnetClient.println(output);
  }  
}

// Hotspot SSID and password
const char* ssid = "My Azuz"; //"My Azuz"; "AZ_ZAHRA_PELNI";
const char* password = "azzahra4579";
WiFiClient client;

/*-------- ThingSpeak code ----------*/
String apiKey = "X02CKM0GVEV2SFPL";
const char* server = "api.thingspeak.com";
long ChannelNumber = 170741;
int FieldNumber = 1;
int minReadingInterval = 1000; // 1 min = 60000 ms, suggest 5 min = 300000 ms

/*-------- NTP code ----------*/
IPAddress ntpServerIP; // NTP server's ip address
const char* ntpServerName = "time.nist.gov";
const int timeZone = 7;     // Asia Time
//const int SECS_PER_HOUR = 3600; 
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

unsigned int localPort = 2390;      // local port to listen for UDP packets
WiFiUDP udp;

long getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  spln("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  sp(ntpServerName); sp(": "); spln(ntpServerIP);
  sendNTPpacket(ntpServerIP);

  delay(1500);

//  uint32_t beginWait = millis();
//  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      spln("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
//  }
  spln("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

void sendNTPpacket(IPAddress &address)
{
  // spln("sending NTP packet...");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

/*-------- Switching code ----------*/
//int LED = LED_BUILTIN;
int SwitchPin = D1;

void switch_on(boolean bol){
  // float lamp_1 = ThingSpeak.readFloatField(ChannelNumber, FieldNumber, "X02CKM0GVEV2SFPL");
  sp("Lamp: "); spln((bol)?"ON":"OFF");
  // tp("Lamp_1: "+String(lamp_1));
  //digitalWrite(SwitchPin, (int(lamp_1)>0)?HIGH:LOW);
  digitalWrite(SwitchPin, (bol)?HIGH:LOW);
}

/*-------- Schedule code ----------*/
int on_h = 16;
int on_m = 31;
int off_h = 16;
int off_m = 33;

void setup() 
{
  sb; 
  
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);
 
  spln();
  spln();
  spln("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    spln("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  spln("Ready");
  sp("IP address: "); spln(WiFi.localIP());
  
  udp.begin(localPort);
  ThingSpeak.begin(client);
  setSyncProvider(getNtpTime);
  setSyncInterval(300);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    spln("Start");
  });
  ArduinoOTA.onEnd([]() {
    spln("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    spf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    spf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) spln("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) spln("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) spln("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) spln("Receive Failed");
    else if (error == OTA_END_ERROR) spln("End Failed");
  });
  ArduinoOTA.begin();
}
 
void loop() 
{
  ArduinoOTA.handle();

  digitalClockDisplay();
  if (timeStatus() != timeNeedsSync) {
    setSyncProvider(getNtpTime);
  }
  if (hour()==on_h && minute()==on_m){
    if (digitalRead(SwitchPin)==LOW){
      switch_on(true);
    }
  }
  if (hour()==off_h && minute()==off_m){
    if (digitalRead(SwitchPin)==HIGH){
      switch_on(false);
    }
  }
  
  delay(minReadingInterval);
}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
