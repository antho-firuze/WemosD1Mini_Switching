#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ThingSpeak.h>
#include <WiFiUdp.h>
#include <TimeLib.h>

/*-------- shortcuts code ----------*/
boolean doSerial=true;      
#define sb    Serial.begin(9600)        // shortcuts for serial output
#define sp    Serial.print
#define spf   Serial.printf
#define spln  Serial.println
 
// Hotspot SSID and password
const char* ssid = "My Azuz"; //"My Azuz"; "AZ_ZAHRA_PELNI";
const char* password = "azzahra4579";
WiFiClient client;

/*-------- ThingSpeak code ----------*/
const char * WriteAPIKey = "MK9FKFGB7W5KYEL9";
const char * ReadAPIKey = "X02CKM0GVEV2SFPL";
const char * server = "api.thingspeak.com";
unsigned long ChannelNumber = 170741;
int FieldNumber = 1;
int minReadingInterval = 5000; // 1 min = 60000 ms, suggest 5 min = 300000 ms

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
  spln("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

void sendNTPpacket(IPAddress &address)
{
//   spln("sending NTP packet...");
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
  // float lamp_1 = ThingSpeak.readFloatField(ChannelNumber, FieldNumber, ReadAPIKey);
  sp("Lamp: "); spln((bol)?"ON":"OFF");
  // tp("Lamp_1: "+String(lamp_1));
  //digitalWrite(SwitchPin, (int(lamp_1)>0)?HIGH:LOW);
  digitalWrite(SwitchPin, (bol)?HIGH:LOW);
}

/*-------- Schedule code ----------*/
//write
int on_h, on_m, of_h, of_m = -1;
//http://api.thingspeak.com/update?api_key=MK9FKFGB7W5KYEL9&field2=17
//http://api.thingspeak.com/update?api_key=MK9FKFGB7W5KYEL9&field3=30
//http://api.thingspeak.com/update?api_key=MK9FKFGB7W5KYEL9&field4=5
//http://api.thingspeak.com/update?api_key=MK9FKFGB7W5KYEL9&field5=30
// ThingSpeak.writeField(ChannelNumber, FieldNumber, Value, WriteAPIKey);

//read
//https://thingspeak.com/channels/170741/field/2/last?api_key=X02CKM0GVEV2SFPL
// float lamp_1 = ThingSpeak.readFloatField(ChannelNumber, FieldNumber, ReadAPIKey);
// ThingSpeak.readFloatField
// ThingSpeak.readIntField
// ThingSpeak.readLongField
// ThingSpeak.readStringField
// ThingSpeak.readRaw

void setup() 
{
  sb; 
  
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
}
 
void loop() 
{
  if (timeStatus() == timeNotSet || timeStatus() == timeNeedsSync)
    setSyncProvider(getNtpTime);

  digitalClockDisplay();
  int on_h1 = ThingSpeak.readIntField(ChannelNumber, 2, ReadAPIKey);
  int on_m1 = ThingSpeak.readIntField(ChannelNumber, 3, ReadAPIKey);
  int of_h1 = ThingSpeak.readIntField(ChannelNumber, 4, ReadAPIKey);
  int of_m1 = ThingSpeak.readIntField(ChannelNumber, 5, ReadAPIKey);
  if (on_h < 0) {
    if (on_h1 > 0)
      on_h = on_h1;
  } else if (on_h != on_h1) {
    if (on_h1 > 0)
      on_h = on_h1;
  }
  if (on_m < 0) {
    on_m = on_m1;
  } else if (on_m1 >= 0) {
    if (on_m != on_m1) 
      on_m = on_m1;
  }
  
  if (of_h < 0) {
    if (of_h1 > 0)
      of_h = of_h1;
  } else if (of_h != of_h1) {
    if (of_h1 > 0)
      of_h = of_h1;
  }
  if (of_m < 0) {
    of_m = of_m1;
  } else if (of_m1 >= 0) {
    if (of_m != of_m1) 
      of_m = of_m1;
  }
    
  sp(on_h); sp(":"); spln(on_m); 
  sp(of_h); sp(":"); spln(of_m); 
  spln(); spln();
  
//  if (hour()>=on_h && minute()==on_m){
//    if (digitalRead(SwitchPin)==LOW){
//      switch_on(true);
//    }
//  }
//  if (hour()==off_h && minute()==off_m){
//    if (digitalRead(SwitchPin)==HIGH){
//      switch_on(false);
//    }
//  }
  
  delay(minReadingInterval);
}

void digitalClockDisplay()
{
  // digital clock display of the time
  sp(hour());  printDigits(minute());  printDigits(second());
  sp(" ");
  sp(day());  sp(".");  sp(month());  sp(".");  sp(year());
  spln();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  sp(":");
  if (digits < 10)
    sp('0');
  sp(digits);
}
