#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include <TimeLib.h>
#include "passwords.h"

#ifdef ESP8266
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif
WiFiClientSecure clientSecure;
UniversalTelegramBot bot(BOTtoken, clientSecure);
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;
bool ledState = LOW;

// ESP8266WebServer server(80);

const int outRelay = 5;
const int inRelay = 4;     
bool doorMonitor = true;  //true == door closed, false == door open   
const int DOOR_SENSOR_PIN = 14; // Arduino pin connected to door sensor's pin

// NTP Client

#define MY_NTP_SERVER "time1.google.com"   
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03";
WiFiUDP ntpUDP;
long requestTime = 0;
long timeZone = -25200;
long results_sunrise = 0;
long results_sunset = 0;
NTPClient timeClient(ntpUDP, "time1.google.com", timeZone);

 
unsigned long previousMillis = 0;
unsigned long interval = 5000;
bool dateInfoChecked = false;
bool morningOpened = false;
bool nightClosed = false;
int lastDayTimeChecked = -1; // day function only returns values 0-6 so this at startup indicates that the day has not been checked at start up. 
int lastDayMorningOpened = -1; // day function only returns values 0-6 so this at startup indicates that the day has not been checked at start up.
int lastDayNightClosed = -1; // day function only returns values 0-6 so this at startup indicates that the day has not been checked at start up.

String combineMessages(String inMessage) {
  String replyMessage = inMessage;
  replyMessage += "\n\n\nUse the following commands to control your outputs.\n\n";
  replyMessage += "/led_on to turn GPIO ON \n\n";
  replyMessage += "/led_off to turn GPIO OFF \n\n";
  replyMessage += "/state to request current GPIO state \n\n";
  replyMessage += "/time to get the current time \n\n";
  replyMessage += "/door to check if the door is open/closed \n\n";
  replyMessage += "/open to open the door \n\n";
  replyMessage += "/close to close the door \n\n";
  return replyMessage;
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;
    

    if (text == "/start") {
      // String welcome = "Welcome, " + from_name + ".\n";
      String sendBackMessage = combineMessages("Welcome, " + from_name + ".\n");
      bot.sendMessage(chat_id, sendBackMessage , "");
    }

    if (text == "/led_on") {
      bot.sendMessage(chat_id, combineMessages("LED state set to ON"), "");
      ledState = LOW;
      digitalWrite(LED_BUILTIN, ledState);
    }
    
    if (text == "/led_off") {
      bot.sendMessage(chat_id, combineMessages("LED state set to OFF"), "");
      ledState = HIGH;
      digitalWrite(LED_BUILTIN, ledState);
    }
    
    if (text == "/state") {
      if (digitalRead(LED_BUILTIN)){
        bot.sendMessage(chat_id, combineMessages("LED is OFF"), "");
      }
      else{
        bot.sendMessage(chat_id, combineMessages("LED is ON"), "");
      }
    }
    
    if (text == "/time") {
      bot.sendMessage(chat_id, combineMessages(getSunUpDown()), "");
    }

    if(text == "/door") {
      bot.sendMessage(chat_id, combineMessages("The door is: " + doorMonitor), "");
    }

    if(text == "/close") {
      bot.sendMessage(chat_id, combineMessages("Closing Door"), "");
      closeDoor(5000);
      bot.sendMessage(chat_id, combineMessages("The door is: " + doorMonitor), "");
    }

    if(text == "/open") {
      bot.sendMessage(chat_id, combineMessages("Opening Door"), "");
      openDoor(5000);
      bot.sendMessage(chat_id, combineMessages("The door is: " + doorMonitor), "");
    }

  }
}

void openDoor(int howLong) {
  digitalWrite(outRelay, HIGH);
  digitalWrite(inRelay, LOW);
  delay(howLong); // 35 seconds to fully open
  doorMonitor = false;  //false is open
  //turn off power to motor
  digitalWrite(outRelay, HIGH);// HIGH is off
  digitalWrite(inRelay, HIGH);// HIGH is off
}

void closeDoor(int howLong) {
  digitalWrite(outRelay, LOW);
  digitalWrite(inRelay, HIGH);
  delay(howLong);
  doorMonitor = true; // true is closed
  //turn off power to motor
  digitalWrite(outRelay, HIGH); // HIGH is off
  digitalWrite(inRelay, HIGH); // HIGH is off
}

 
void setup() {
  Serial.begin(115200);
  pinMode(outRelay, OUTPUT);
  pinMode(inRelay, OUTPUT);
  delay(100);
  openDoor(1000);
  closeDoor(1000);


  //need to read in the door monitor to check where the door is

  // #ifdef ESP8266
    // configTime(0, 0, "pool.ntp.org", "time.google.com");      // get UTC time via NTP
    clientSecure.setTrustAnchors(&cert); // Add root certificate for api.telegram.org
  // #endif
  
  pinMode(LED_BUILTIN, OUTPUT);             

  Serial.println("Connecting to ");
  Serial.println(ssid);

  //connect to your local wi-fi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, LOW); // on
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH); // off
  delay(100);
  digitalWrite(LED_BUILTIN, LOW); // on
  delay(100);
  digitalWrite(LED_BUILTIN, HIGH); // off

  // server.on("/", handle_OnConnect);
  // server.onNotFound(handle_NotFound);

  // server.begin();
  // Serial.println("HTTP server started");

  timeClient.begin();
  timeClient.forceUpdate();

  if(timeClient.getEpochTime() > results_sunrise && timeClient.getEpochTime() < results_sunset){ // and add to check the door is already open
    //set to todays date as the door is alreay been open. as if power restarted in the middle of the day
    lastDayMorningOpened = timeClient.getDay(); // day function only returns values 0-6 so this at startup indicates that the day has not been checked at start up.
  }
  if(timeClient.getEpochTime() > results_sunset) { // check if the door is closed
    lastDayNightClosed = timeClient.getDay(); // already past sunset and door is closed
  }
   
  
}

void loop() {
  
  // server.handleClient();
  
  if(dateInfoChecked == false){
    if(getSunUpDown() == "200") {
      String botMess = "got today's sun up: ";
      botMess += hour(results_sunrise);
      botMess += ":";
      botMess += minute(results_sunrise);
      botMess += ". sun down: ";
      botMess += hour(results_sunset);
      botMess += ":";
      botMess += minute(results_sunset);
      botMess += ". Current time: ";
      botMess += timeClient.getFormattedTime();
      bot.sendMessage(CHAT_ID, combineMessages(botMess), "");
      dateInfoChecked = true;
      lastDayTimeChecked = timeClient.getDay();
      morningOpened = false;  // set these to false as the door has not opened in the morning yet
      nightClosed = false;  // set this to false each morning as the door as not closed at night yet. 
    }
    else{
      Serial.println("Was not able to get up down times.");
      dateInfoChecked = false;
    }
  }

  unsigned long currentMillis = millis();  
  if(currentMillis - previousMillis > interval){
    previousMillis = currentMillis;

    if(timeClient.getEpochTime() > results_sunrise && timeClient.getEpochTime() < results_sunset){
      Serial.println("The sun is up.");
    }
    else{
      Serial.println("the sun is down");
    }
  }   
  if(timeClient.getHours() == 3 && dateInfoChecked == true && timeClient.getDay() != lastDayTimeChecked) {
    dateInfoChecked = false;
  }

  

  if(timeClient.getEpochTime() >= results_sunrise && timeClient.getEpochTime() < results_sunrise + 300  && morningOpened == false && lastDayMorningOpened != timeClient.getDay()){
    // openDoor(35000);
    Serial.println("The sun is up and the door is not open. opening door now" + timeClient.getFormattedTime());
    bot.sendMessage(CHAT_ID, combineMessages("The Door is opened at : " + timeClient.getFormattedTime()));
    openDoor(35000);
    //need to check door sensor
    doorMonitor = false; // this is just for testing
    lastDayMorningOpened = timeClient.getDay();
    morningOpened = true;
  }

  if(timeClient.getEpochTime() >= results_sunset && timeClient.getEpochTime() < results_sunset + 300 && nightClosed == false && lastDayNightClosed != timeClient.getDay()){
    // closeDoor(35000);
    Serial.println("The sun is down and the door is not closed. closing door now at " + timeClient.getFormattedTime());
    bot.sendMessage(CHAT_ID, combineMessages("The Door is closed at : " + timeClient.getFormattedTime()));
    closeDoor(35000);
    //need to check door sensor
    doorMonitor = true; // this is for testing
    lastDayNightClosed = timeClient.getDay();
    nightClosed = true;
  }

  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

String getSunUpDown() {

  // String responseCode = "";
  String upDownDomain = "api.openweathermap.org";
  // String upDownAddy = "http://api.openweathermap.org/data/2.5/weather?lat=41.18351344425365&lon=-111.98634995215585&appid=ee5ab2f2d75e42a49b0c4208d9ca768d";
  clientSecure.setInsecure();
  clientSecure.setTimeout(15000);
  int r = 0;
  while((!clientSecure.connect(upDownDomain, 443)) && (r < 30)) {
    delay(100);
    Serial.print(".");
    r++;
  }
  if(r == 30){
    Serial.println("was not able to get to upDownDomain");
  }
  else {
    Serial.println("Connected to upDownDowmain");
  }
  String upDownURL = "/data/2.5/weather?lat=";
  upDownURL += latVar;
  upDownURL += "&lon=";
  upDownURL += longVar;
  upDownURL += "&appid=";
  upDownURL += appIdVar;
  
  clientSecure.print(String("GET ") + upDownURL + " HTTP/1.1\r\n" +
               "Host: " + upDownDomain + "\r\n" +               
               "Connection: close\r\n\r\n");

  // if(clientSecure.connected())
  // {
  //   responseCode = clientSecure.readStringUntil('\n');
  //   responseCode = responseCode.substring(9,12);
  // }
  while(clientSecure.connected()) {
    String payload = clientSecure.readStringUntil('\n');
    if (payload == "\r") {
      break;
    }
  }

  Serial.println("reply was:");
  Serial.println("==========");
  String payload;
  while(clientSecure.available()){        
    payload = clientSecure.readStringUntil('\n');  //Read Line by Line
    Serial.println(payload); //Print response
  }
  Serial.println("==========");
  Serial.println("closing connection");
  // if(responseCode != "200") {
  //   return "failed to get correct Connection";
  // }

  StaticJsonDocument<1100> jsonBuffer;
  String json = payload;
 
  DeserializationError error = deserializeJson(jsonBuffer, json);
  if(error) {
    Serial.print("deserializition of json failed");
    Serial.print(error.c_str());  
    return "couldnt get times";      
  }
    
  if(timeZone != jsonBuffer["timezone"]){
    timeZone = jsonBuffer["timezone"];
    Serial.println("the time offset was changed");
    timeClient.setTimeOffset(timeZone);
  }
  timeZone = jsonBuffer["timezone"];
  requestTime = jsonBuffer["dt"];  
  requestTime += timeZone;
  JsonObject sysStuff = jsonBuffer["sys"];
  results_sunrise = sysStuff["sunrise"];
  results_sunrise += timeZone;
  results_sunset = sysStuff["sunset"];
  results_sunset += timeZone;  
  Serial.print("Requested Time: ");
  Serial.print(requestTime);  
  Serial.println("json results: ");
  Serial.print(results_sunrise);  
  Serial.print("......");
  Serial.print(results_sunset);   
  // String returnString = "currentTime: " + String(requestTime) + " sunrise: " + String(results_sunrise) + " sunset: " + String(results_sunset);
  // return returnString;
  int responseCode = jsonBuffer["cod"];
  return String(responseCode);
}

// void handle_OnConnect() {

//   getSunUpDown();
//  Temperature = dht.convertCtoF(dht.readTemperature()); // Gets the values of the temperature
//   Humidity = dht.readHumidity(); // Gets the values of the humidity 
//   server.send(200, "text/html", SendHTML(Temperature,Humidity)); 

// }

// void handle_NotFound(){
//   server.send(404, "text/plain", "Not found");
// }

// String SendHTML(float Temperaturestat,float Humiditystat){
//   digitalWrite(LED_BUILTIN, LOW); // on
//   delay(500);
//   digitalWrite(LED_BUILTIN, HIGH); // off
//   delay(500);
//   digitalWrite(LED_BUILTIN, LOW); // on
//   delay(500);
//   digitalWrite(LED_BUILTIN, HIGH); // off
//   String ptr = "<!DOCTYPE html> <html>\n";
//   ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
//   ptr +="<title>ESP8266 Weather Report</title>\n";
//   ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
//   ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
//   ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
//   ptr +="</style>\n";
//   ptr +="</head>\n";
//   ptr +="<body>\n";
//   ptr +="<div id=\"webpage\">\n";
//   ptr +="<h1>ESP8266 NodeMCU Weather Report</h1>\n";
  
//   ptr +="<p>Temperature: ";
//   ptr +=(int)Temperaturestat;
//   ptr +="&#176F</p>";
//   ptr +="<p>Humidity: ";
//   ptr +=(int)Humiditystat;
//   ptr +="%</p>";
//   ptr += "<p> Current time: ";
//   ptr +=  currentTime;
//   ptr += "</p>";
//   ptr += "<p>";
//   ptr += "Request time: ";
//   ptr += requestTime;  
//   ptr += " sunrise: ";
//   ptr += results_sunrise;
//   ptr += " sunset: ";
//   ptr += results_sunset;   
//   ptr += "</p>"; 
//   ptr +="</div>\n";
//   ptr +="</body>\n";
//   ptr +="</html>\n";
//   return ptr;
// }
