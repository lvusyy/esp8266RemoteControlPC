//#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass 
//#define _TASK_TIMEOUT
#include <SimpleDHT.h>
#include <ESP8266WiFi.h>
#include <TaskScheduler.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

//**** dht init
const int pinDHT11 = 2;
SimpleDHT11 dht11(pinDHT11);
const int POWERPIN = 4;
const int OPENBTPIN = 5;
uint8_t poweroffTimeout=0;
/************************* WiFi Access Point *********************************/

#define WLAN_SSID       "HOME"
#define WLAN_PASS       "HOME++++"

/************************* Adafruit.io Setup *********************************/

#define AIO_SERVER      "ip"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    "user"
#define AIO_KEY         "pass"

/************ Global State (you don't need to change this!) ******************/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

/****************************** Feeds ***************************************/

// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish temperaturePS = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/poweronoff");

Adafruit_MQTT_Subscribe openbt = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/btonoff");

int get_temperature();
void pushTemperature();
void clickBtStop();
void set_powerOff();
//void t2disable();
// Task init
Scheduler runner;
Task t1(1000,TASK_FOREVER,&pushTemperature);
//Task t2(100,1,&clickBtStop,&runner,false,NULL,&set_powerOff);

/*************************** Sketch Code ************************************/
void clickBtStart(){
    set_powerOn();
    digitalWrite(OPENBTPIN,HIGH);
    delay(1300);
    digitalWrite(OPENBTPIN,LOW);
}



void clickBtStop(){
    digitalWrite(OPENBTPIN,HIGH);
//    delay(8000);
    delay(1000);
    digitalWrite(OPENBTPIN,LOW);
    poweroffTimeout=90;
//    delay(120000L);
//    set_powerOff();
//    delay(200);
//    digitalWrite(OPENBTPIN,HIGH);
//    delay(8000);
//    digitalWrite(OPENBTPIN,LOW);
}

void set_powerOn(){
    digitalWrite(POWERPIN,LOW);
    Serial.println("called set_powerOn ");
    poweroffTimeout=0;
}

void set_powerOff(){
    digitalWrite(POWERPIN,HIGH);
    Serial.println("called set_poweroff ");
    poweroffTimeout=0;
}

int get_temperature() {

  byte temperature = 0;
  byte humidity = 0;
  int err = SimpleDHTErrSuccess;
  if ((err = dht11.read(&temperature, &humidity, NULL)) != SimpleDHTErrSuccess) {
    Serial.print("Read DHT11 failed, err="); Serial.println(err);delay(1000);
    return 0;
  }
  
  Serial.print("Sample OK: ");
  Serial.print((int)temperature); Serial.print(" *C, "); 
  Serial.print((int)humidity); Serial.println(" H");
  return (int)temperature;
}
void pushTemperature()
{
  uint8_t t=0;
  for (int i=0;i<10;i++){
    t=get_temperature();
    if(t<=0){
      t=0;
      }else {
        break;
        }
  }
  if (! temperaturePS.publish(t)) {
    Serial.println(F("Temperature publish Failed"));
  } else {
    Serial.println(F("Temperature publish OK!"));
  }
  if (poweroffTimeout>1){
    poweroffTimeout--;
    Serial.println((int)poweroffTimeout);
    }else if (poweroffTimeout==1){
      poweroffTimeout=0;
      set_powerOff();
    }

}

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

void setup() {
  pinMode(POWERPIN,OUTPUT);
  pinMode(OPENBTPIN,OUTPUT);
  Serial.begin(115200);
  delay(10);
  //digitalWrite(OPENBTPIN,HIGH);//init state
  Serial.println(F("Adafruit MQTT demo"));

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());
  //task 
  runner.init();
  delay(50);
  runner.addTask(t1); 
//  runner.addTask(t2);
//  delay(100);

  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&onoffbutton);
  mqtt.subscribe(&openbt);
  t1.enable();
}


void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  int onoffstatus=0;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got onoffbutton: "));
      onoffstatus=atoi((char *)onoffbutton.lastread);
      Serial.println(onoffstatus);
      if (onoffstatus >= 1){
          set_powerOn();
      }else {
          set_powerOff();
      }
      
    }else if (subscription == &openbt) {
      Serial.print(F("Got openbt: "));
      onoffstatus=0;
      onoffstatus=atoi((char *)openbt.lastread);
      Serial.println(onoffstatus);
      if (onoffstatus >= 1){
          clickBtStart();
      }else
      {
          clickBtStop();
//          t2.setTimeout(120 * TASK_SECOND);
//          t2.enable();
      }
    }
    delay(100);
  }

  // Now we can publish stuff!
 
  runner.execute();
  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  /*
  if(! mqtt.ping()) {
    mqtt.disconnect();
  }
  */
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}
