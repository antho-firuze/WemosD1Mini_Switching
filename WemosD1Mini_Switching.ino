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

/*-------- Schedule code ----------*/
int on_h, on_m, of_h, of_m = -1;
int on_h1, on_m1, of_h1, of_m1, lamp, is_auto = 0;
int on_t, of_t, cur_t = 0;
// ThingSpeak write on server ===============================================
// http://api.thingspeak.com/update?api_key=MK9FKFGB7W5KYEL9&field2=1
// ThingSpeak.writeField(ChannelNumber, FieldNumber, Value, WriteAPIKey);
// ThingSpeak read on server ================================================
// https://thingspeak.com/channels/170741/field/2/last?api_key=X02CKM0GVEV2SFPL
// float lamp = ThingSpeak.readFloatField(ChannelNumber, FieldNumber, ReadAPIKey);
// ThingSpeak.readFloatField
// ThingSpeak.readIntField
// ThingSpeak.readLongField
// ThingSpeak.readStringField
// ThingSpeak.readRaw

bool readScheduleOnServer(){
  lamp = ThingSpeak.readIntField(ChannelNumber, 1, ReadAPIKey);
  is_auto = ThingSpeak.readIntField(ChannelNumber, 2, ReadAPIKey);
  on_h1 = ThingSpeak.readIntField(ChannelNumber, 3, ReadAPIKey);
  on_m1 = ThingSpeak.readIntField(ChannelNumber, 4, ReadAPIKey);
  of_h1 = ThingSpeak.readIntField(ChannelNumber, 5, ReadAPIKey);
  of_m1 = ThingSpeak.readIntField(ChannelNumber, 6, ReadAPIKey);
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
    
  cur_t = hour() * 100 + minute();
  on_t = on_h * 100 + on_m;
  of_t = of_h * 100 + of_m;

  sp("lamp:"); spln(lamp);
  sp("is_auto:"); spln(is_auto);
  sp("on_t:"); spln(on_t);
  sp("of_t:"); spln(of_t);
  sp("cur_t:"); spln(cur_t);

  if (is_auto == 1) {
    if (cur_t >= of_t && cur_t < on_t) {
      spln("lampu mati !");
      switch_lamp(false);
      if (lamp == 1) {
        ThingSpeak.writeField(ChannelNumber, 1, 0, WriteAPIKey);
        spln("Set Lamp On Server = 0");
      }
    } else {
      spln("lampu menyala !");
      switch_lamp(true);
      if (lamp == 0) {
        ThingSpeak.writeField(ChannelNumber, 1, 1, WriteAPIKey);
        spln("Set Lamp On Server = 1");
      }
    }
  } else {
    if (lamp == 1)
      switch_lamp(true);
    else
      switch_lamp(false);
  }
  spln();
  return true;
}

/*-------- Switching code ----------*/
//int switchPin = LED_BUILTIN;
int switchPin = D1;

void switch_lamp(boolean stat){
  sp("Set Lamp On Device: "); spln((stat)?"ON":"OFF");

  if (stat){
    if (digitalRead(switchPin)==LOW)
      digitalWrite(switchPin, HIGH);
  } else {
    if (digitalRead(switchPin)==HIGH)
      digitalWrite(switchPin, LOW);
  }
}

void setup() 
{
  sb; 
  
  spln(); spln();
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

  pinMode(switchPin, OUTPUT);
}
 
void loop() 
{
  if (timeStatus() == timeNotSet || timeStatus() == timeNeedsSync)
    setSyncProvider(getNtpTime);

  digitalClockDisplay();

  if (!readScheduleOnServer())
    spln("readOnServer: Failed !");
  
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

