#include <SPI.h>
#include <WiFi101.h>
#include <FlashStorage.h>
#include <RTCZero.h>
#include <WiFiUdp.h>
#include <ArduinoLowPower.h>

WiFiUDP udp;
WiFiUDP Udp;
RTCZero rtc;

#define MAGIC_NUMBER 0x7423  // arbitrary number to double check the validity of SSID
#define MaxNet 30  // max amount of network to be saved 

// RGB LED pins 
int redPin = 6; 
int greenPin = 8; 
int bluePin = 7; 

const char* home_ssid = "Jonos WiFi";    //  your network SSID (name)
const char* password = "password";  // your network password

int BuzzerPin = 9;
int SensorPin = A2;

int PosToBeSaved = 0; // Variable used to navigate the array of saved networks
int daily_amount_of_food = 6; // The amount of food per day needed to survive
int sleeping_time = 1800000; // 30 min *60 sec *1000 millisec 

bool atHome = false;
bool hungry=true;
bool justWokeUp=false;

//time related stuff
unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
const int GMT = -5 * 60 * 60; //change this to adapt it to your time zone   hours*minutes*seconds

unsigned long epoch;

// Struct of variable to be saved in flash memory
typedef struct {  
  int magic;
  boolean valid[MaxNet];
  char SSIDs[MaxNet][100];
  int alive_days;
  int last_time_feeded;
} Networks;

FlashStorage(my_flash_store, Networks);
Networks values;

void setup() {

  Serial.begin(115200);
  delay(2000);
  
  pinMode(redPin, OUTPUT); 
  pinMode(greenPin, OUTPUT); 
  pinMode(bluePin, OUTPUT); 
  
  rtc.begin(); // enable real time clock functionalities

  // set up a WakeUp function, it will be triggered each time the Nerd exit the sleeping mode
  LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, WakeUp, CHANGE); 

  values = my_flash_store.read(); // Read values from flash memory
  if (values.magic == MAGIC_NUMBER) { // If token is correct print saved networks
    Serial.println("saved data:");
    Serial.println("");
    for (int a = 0; a < MaxNet; a++) {
      if (values.valid[a]) {
        Serial.println(values.SSIDs[a]);
      } else {
        PosToBeSaved = a;
      }
    }
  }

}

void loop() {
  
  int SensorValue=analogRead(SensorPin);

  // Awaking notification
  if(SensorValue>30 && justWokeUp){
    justWokeUp=false;
    setColor(0, 255, 0); // green 
    tone(BuzzerPin, 31, 200); // tone(Pin, Note, Duration);
    delay(200);
    setColor(0, 0, 0); // off 
    noTone(BuzzerPin);
    delay(1000);
 }
 
   // Check if the Nerd has been fed within 2 days
  if(rtc.getEpoch() - values.last_time_feeded >= 86400*2){ // 86400 is the number of seconds in one day
    // DIE :(
    SOS();
    values.alive_days = 0 ;
    values.last_time_feeded = rtc.getEpoch();
    empty_network_array();
  }
  
  // End of the day, empty the network array and go to sleep
  if(rtc.getHours() == 23 && rtc.getMinutes() >= 30){ 
    // Empty the array of network
    values.alive_days +=1;
    empty_network_array();
    hungry=true;
    LowPower.sleep(3600*8); // sleep 8 hours 
  }
  

  if(!atHome) check_home();
    
  if (!atHome && hungry) {
    Serial.println("checking for network");
    
    // Temporarly save the number of networks
    int networks_already_saved = PosToBeSaved; 
    
    getNetwork();
    
    // compare the two values and complain if no new network is detected
    if (networks_already_saved == PosToBeSaved) SOS(); 
    
    if(PosToBeSaved >= daily_amount_of_food) hungry=false; // check if had enough food
    if(SensorValue < 30)  LowPower.sleep(sleeping_time); // snooze if dark
  }
  else if(atHome && !hungry) {
    Serial.println("back to sleep");
    LowPower.sleep(sleeping_time);
  }
  else if(atHome && hungry){
    if(SensorValue < 30)  LowPower.sleep(sleeping_time); // back to sleep if it's dark
    SOS();
  }
  
  // Set color status feedback
  if(PosToBeSaved >= 8){ // if starving show red
  setColor(255, 0, 0); // Red 
  }
  else if(PosToBeSaved > 4 && PosToBeSaved < 8){
  setColor(255, 255, 0); // yellow 
  }
  else{ 
  setColor(0, 255, 0); // green 
  }
  
}


  
void WakeUp() {
  Serial.println("awake");
  atHome = false;
  justWokeUp =true;
}

void check_home() {
  int numSsid = WiFi.scanNetworks();
  if (numSsid != -1) {
    for (int thisNet = 0; thisNet < numSsid; thisNet++) {
      delay(100);
      if (strncmp(WiFi.SSID(thisNet), home_ssid, 100) == 0) {
        Serial.println("Yay, I'm home \n");
        atHome = true;
        connect_WiFi();
      }
    }
  }
}

void connect_WiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    while (WiFi.begin(home_ssid, password) != WL_CONNECTED) {
      delay(500);
    }
    Serial.println("WiFi connected \n");
    GetCurrentTime();
    printTime();
  }
}

// Feed the Nerd with networks's SSID
void getNetwork() {
  // scan for nearby networks:
  Serial.println("*Scan Networks*");
  int numSsid = WiFi.scanNetworks();
  delay(1000);
  if (numSsid == -1)
  {
    Serial.println("There are no WiFi networks here..");
  } else {
    Serial.print("number of available networks: ");
    Serial.println(numSsid);
    // print the network number and name for each network found:
    for (int thisNet = 0; thisNet < numSsid; thisNet++) {
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID(thisNet));
      delay(500);

      char* net = WiFi.SSID(thisNet);
      bool canBeSaved = true;

      // check if the network has already been saved
      for (int a = 0; a < PosToBeSaved ; a++) { 
        if (values.valid[a]) {
          if (strncmp(net, values.SSIDs[a], 100) == 0 || strnlen(net, 100) == 0) { 
            Serial.println("Not saved");
            canBeSaved = false;
          }
        }
      }
      
      // Store ssid name
      if (canBeSaved && PosToBeSaved < MaxNet) { 
        if (strlen(net) + 1 < 100 && strlen(net) > 0) { // check if the SSID name fits 100 bytes
          memset(values.SSIDs[PosToBeSaved], 0, sizeof(values.SSIDs[PosToBeSaved])); // set all characters to zero
          memcpy(values.SSIDs[PosToBeSaved], net, strlen(net) + 1); // copy "net" to values.SSDs[thisNet]
          values.valid[PosToBeSaved] = true;
          values.last_time_feeded = rtc.getEpoch();
          values.magic = MAGIC_NUMBER;
          my_flash_store.write(values);
          Serial.println(String(values.SSIDs[PosToBeSaved]) + " saved in position " + String(PosToBeSaved));
          PosToBeSaved ++;
        }
        else {
          Serial.println(" network skipped");
        }
      }
    }
  }
}

// Reset the array in which networks are saved
void empty_network_array(){
  for(int a = 0; a < PosToBeSaved; a++ ){
    values.valid[a] = false;
  }
    values.magic = 0;
    my_flash_store.write(values);
}

void SOS(){
  for(int a = 0; a< 3; a++){  
    setColor(255, 0, 0); // Red 
    tone(BuzzerPin, 31, 100); // tone(Pin, Note, Duration);
    delay(100);
    setColor(0, 0, 0); // off 
    noTone(BuzzerPin);
    delay(50);
  }
  
  delay(1000);
  for(int a = 0; a< 3; a++){  
    setColor(255, 0, 0); // Red 
    tone(BuzzerPin, 31, 2000); // tone(Pin, Note, Duration);
    delay(1000);
  }
  
  for(int a = 0; a< 3; a++){  
    setColor(255, 0, 0); // Red 
    tone(BuzzerPin, 31, 100); // tone(Pin, Note, Duration);
    delay(100);
    setColor(0, 0, 0); // off 
    noTone(BuzzerPin);
    delay(50);
  }
  delay(10000);
}

// Send RGB values to the LED pins 
void setColor(int red, int green, int blue){ 
 analogWrite(redPin, red); 
 analogWrite(greenPin, green); 
 analogWrite(bluePin, blue);   
}

void GetCurrentTime() {

  int numberOfTries = 0, maxTries = 6;
  do {
    epoch = readLinuxEpochUsingNTP();
    numberOfTries++;
  }
  while ((epoch == 0) || (numberOfTries > maxTries));

  if (numberOfTries > maxTries) {
    Serial.print("NTP unreachable!!");
    while (1);
  }
  else {
    Serial.print("Epoch received: ");
    Serial.println(epoch);
    rtc.setEpoch(epoch + GMT);

    Serial.println();
  }

}

unsigned long readLinuxEpochUsingNTP()
{
  Udp.begin(localPort);
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);

  if ( Udp.parsePacket() ) {
    Serial.println("NTP time received");
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    // now convert NTP time into everyday time:
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:

    Udp.stop();
    return (secsSince1900 - seventyYears);
  }

  else {
    Udp.stop();
    return 0;
  }
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress & address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)

  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void printTime() {
  // Print date...
  Serial.print(rtc.getDay());
  Serial.print(" / ");
  Serial.print(rtc.getMonth());
  Serial.print(" / ");
  Serial.print(rtc.getYear());
  Serial.print("\t");


  // ...and time
  print2digits(rtc.getHours());
  Serial.print(": ");
  print2digits(rtc.getMinutes());
  Serial.print(": ");
  print2digits(rtc.getSeconds());
  Serial.println("");

}

void print2digits(int number) {
  if (number < 10) {
    Serial.print("0");
  }
  Serial.print(number);
} 
