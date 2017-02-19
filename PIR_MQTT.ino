/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <stephane@sbarthelemy.com> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return Stéphane BARTHELEMY
 * ----------------------------------------------------------------------------
 */

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <PubSubClient.h>  //https://github.com/knolleary/pubsubclient
#include <Ticker.h>

//needed for library WifiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#define DEFAULT_MQTT_SERVER "192.168.2.239"
#define DEFAULT_MQTT_USER ""  //s'il a été configuré sur Mosquitto
#define DEFAULT_MQTT_PASSWD "" //
#define DEFAULT_DOMOTICZ_ID "25"

#define STATUS_LED  5 // D1
#define BUTTON      0 // D3
#define PIR        16 // D0
#define BUTTON_CHECK_PERIODE 0.1
#define BUTTON_CHECK_TIME 4

#define DOMOTICZ_TOPIC "domoticz/in"

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff[100];
char mqtt_server[40];
char mqtt_user[40];
char mqtt_passwd[40];
char domoticz_id[40];

Ticker ticker, ticker_bp;
boolean bp_active = false ; 
boolean bp_long_press = false ;
int pir_status = 0 ; 
int old_pir_status  = 0 ; 
int bp_counter = 0 ;  

WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient client(espClient);

void bp_long_callback (void) {
  wifiManager.resetSettings();
  Serial.println("Reset wifi");
  ESP.reset();
  delay(1000);
}

void bp_short_callback (void) {
  
}

void tick_bp()
{
  int state = digitalRead(BUTTON);
  if (( bp_active == true ) && (bp_long_press == false ))
  {
    if ( state == 0)
    {
      Serial.print("BP counter : ");
      Serial.println(bp_counter);
      bp_counter ++ ;
      if ( bp_counter > (BUTTON_CHECK_TIME / BUTTON_CHECK_PERIODE )) {
         Serial.println("BP Long press : ");
         bp_long_press = true ;
         bp_long_callback (); 
      }
    } else {
      Serial.println("BP short press : ");
      bp_short_callback ();
    }
  }
  if (( state == 0) && (bp_active == false )) {
      bp_active = true ; 
      bp_counter = 0 ;           
  }
  if (state == 1) {
      bp_active = false ;  
      bp_long_press = false ;
  }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {

  int i = 0;
  if ( 1 ) {
    Serial.println("Message recu =>  topic: " + String(topic));
    Serial.print(" | longueur: " + String(length,DEC));
  }
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  
  String msgString = String(message_buff);
  if ( 1 ) {
    Serial.println("Payload: " + msgString);
  }
}

void tick()
{
  //toggle state
  int state = digitalRead(STATUS_LED);  // get the current state of GPIO1 pin
  digitalWrite(STATUS_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}


void setup() {
  Serial.begin(115200);

  //set led pin as output
  pinMode(STATUS_LED, OUTPUT);
  pinMode(BUTTON, INPUT);
  pinMode(PIR, INPUT);

  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);
  ticker_bp.attach(BUTTON_CHECK_PERIODE, tick_bp);

  //WiFiManager
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", DEFAULT_MQTT_SERVER, 40);
  wifiManager.addParameter(&custom_mqtt_server);
  WiFiManagerParameter custom_mqtt_user("user", "mqtt user", DEFAULT_MQTT_USER, 40);
  wifiManager.addParameter(&custom_mqtt_user);
  WiFiManagerParameter custom_mqtt_passwd("passwd", "mqtt password", DEFAULT_MQTT_PASSWD, 40);
  wifiManager.addParameter(&custom_mqtt_passwd);
  WiFiManagerParameter custom_domoticz_id("idx", "domoticz id", DEFAULT_DOMOTICZ_ID, 40);
  wifiManager.addParameter(&custom_domoticz_id);
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }
  
 //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();
  //keep LED on
  digitalWrite(STATUS_LED, LOW);
  strcpy(mqtt_server,custom_mqtt_server.getValue());
  strcpy(mqtt_user,custom_mqtt_user.getValue());
  strcpy(mqtt_passwd,custom_mqtt_passwd.getValue());
  strcpy(domoticz_id,custom_domoticz_id.getValue());

  client.setServer(mqtt_server, 1883);    //Configuration de la connexion au serveur MQTT
  client.setCallback(mqtt_callback);  //La fonction de callback qui est executée à chaque réception de message   
}

//Reconnexion
void reconnect() {
  //Boucle jusqu'à obtenur une reconnexion
  while (!client.connected()) {
    Serial.print("Connexion au serveur MQTT...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_passwd)) {
      Serial.println("OK");
    } else {
      Serial.print("KO, erreur : ");
      Serial.print(client.state());
      Serial.println(" On attend 5 secondes avant de recommencer");
      delay(5000);
    }
  }
}


void loop() {
  // put your main code here, to run repeatedly:
    if (!client.connected()) {
    reconnect();
  }
  client.loop();
  pir_status = digitalRead (PIR) ;
  if ( pir_status != old_pir_status) {
    char msg [100] ;
    digitalWrite(STATUS_LED, pir_status);

    strcpy (msg,"{\"command\": \"udevice\", \"idx\": ");
    strcat (msg,domoticz_id);
    strcat (msg,", \"svalue\": ");
    strcat (msg,(pir_status == 1) ? "\"On\"" :"\"Off\"" ); 
    strcat (msg,", \"nvalue\": ");
    strcat (msg,(pir_status == 1) ? "1" :"0" ); 
 
    strcat (msg,"}"); ; 
    client.publish(DOMOTICZ_TOPIC, msg, true);   //Publie au changement sur le topic 
      Serial.print("msg : ");
  Serial.println (msg) ;
  }

  old_pir_status = pir_status ; 

}
