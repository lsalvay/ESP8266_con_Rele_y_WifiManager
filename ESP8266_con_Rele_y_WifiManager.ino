#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

// Libreria agregada para que funcione MQTT
#include  <PubSubClient.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

// declaramos wifimanager como cliente de pubsubclient ok

WiFiClient espClient;
PubSubClient client(espClient);

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "1883";
char blynk_token[34] = "r1";
char blynk_token2[34] = "r2";
char blynk_token3[34] = "r3";
char blynk_token4[34] = "r4";

#define pinRele1 16
#define pinRele2 5
#define pinRele3 4
#define pinRele4 0

//flag for saving data
bool shouldSaveConfig = false;


// varibles agregadas por LP
long lastMsg = 0;
char msg[50];
int value = 0;
// fin varibles agregadas por LP

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  //Declaramos los pines de los Relé
  pinMode(pinRele1, OUTPUT);
  digitalWrite(pinRele1, HIGH);
  pinMode(pinRele2, OUTPUT);
  digitalWrite(pinRele2, HIGH);
  pinMode(pinRele3, OUTPUT);
  digitalWrite(pinRele3, HIGH);
  pinMode(pinRele4, OUTPUT);
  digitalWrite(pinRele4, HIGH);
  
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);
          strcpy(blynk_token2, json["blynk_token2"]);
          strcpy(blynk_token3, json["blynk_token3"]);
          strcpy(blynk_token4, json["blynk_token4"]);

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);
  WiFiManagerParameter custom_blynk_token2("blynk2", "blynk token2", blynk_token2, 32);
  WiFiManagerParameter custom_blynk_token3("blynk3", "blynk token3", blynk_token3, 32);
  WiFiManagerParameter custom_blynk_token4("blynk4", "blynk token4", blynk_token4, 32);
  //WiFiManagerParameter custom_text("<p> Esto es solo un párrafo de texto </p>"); 
  

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);
  wifiManager.addParameter(&custom_blynk_token2);
  wifiManager.addParameter(&custom_blynk_token3);
  wifiManager.addParameter(&custom_blynk_token4);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("ESP8266", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());
  strcpy(blynk_token2, custom_blynk_token2.getValue());
  strcpy(blynk_token3, custom_blynk_token3.getValue());
  strcpy(blynk_token4, custom_blynk_token4.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;
    json["blynk_token2"] = blynk_token2;
    json["blynk_token3"] = blynk_token3;
    json["blynk_token4"] = blynk_token4;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  const uint16_t mqtt_port_x = 1883; 
  client.setServer(mqtt_server, mqtt_port_x);
  client.setCallback(callback);

}

// codigo agregado por LP
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();


  if(String(topic) == "r1")
  {
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[9] == '1') {
       Serial.print("rele 1 = 1");
      digitalWrite(pinRele1, LOW);   // Turn the LED on (Note that LOW is the voltage level
      // but actually the LED is on; this is because
       client.publish("stateR1", "1");
      // it is acive low on the ESP-01)
    } else {
      Serial.print("rele 1 = 0");
      digitalWrite(pinRele1, HIGH);  // Turn the LED off by making the voltage HIGH
       client.publish("stateR1", "0");
    }
    
  }//fin if r1
  if(String(topic) == "r2")
  {
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[9] == '1') {
      Serial.print("rele 2 = 1");
      digitalWrite(pinRele2, LOW);   // Turn the LED on (Note that LOW is the voltage level
      // but actually the LED is on; this is because
      client.publish("stateR2", "1");
      // it is acive low on the ESP-01)
    } else {
      Serial.print("rele 2 = 0");
      digitalWrite(pinRele2, HIGH);  // Turn the LED off by making the voltage HIGH
       client.publish("stateR2", "0");
    }
    
  }//fin if r2
  
  if(String(topic) == "r3")
  {
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[9] == '1') {
      Serial.print("rele 3 = 1");
      digitalWrite(pinRele3, LOW);   // Turn the LED on (Note that LOW is the voltage level
      // but actually the LED is on; this is because
       client.publish("stateR3", "1");
      // it is acive low on the ESP-01)
    } else {
      Serial.print("rele 3 = 0");
      digitalWrite(pinRele3, HIGH);  // Turn the LED off by making the voltage HIGH
       client.publish("stateR3", "0");
    }
    
  }//fin if r3
  if(String(topic) == "r4")
  {
    // Switch on the LED if an 1 was received as first character
    if ((char)payload[9] == '1') {
      Serial.print("rele 4 = 1");
      digitalWrite(pinRele4, LOW);   // Turn the LED on (Note that LOW is the voltage level
      // but actually the LED is on; this is because
       client.publish("stateR4", "1");
      // it is acive low on the ESP-01)
    } else {
      Serial.print("rele 4 = 0");
      digitalWrite(pinRele4, HIGH);  // Turn the LED off by making the voltage HIGH
       client.publish("stateR4", "0");
    }
    
  }//fin if r4
  

}
// fin codigo agregado por LP

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe(blynk_token);
      client.subscribe(blynk_token2);
      client.subscribe(blynk_token3);
      client.subscribe(blynk_token4);
      client.subscribe("stateR1");
      client.subscribe("stateR2");
      client.subscribe("stateR3");
      client.subscribe("stateR4");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void loop() {

  if (!client.connected()) {
    
    reconnect();
    //Serial.print("Cliente mqtt desconectado");
  }
  client.loop();

//  long now = millis();
//  if (now - lastMsg > 2000) {
//    lastMsg = now;
//    ++value;
//    snprintf (msg, 75, "hello world #%ld", value);
//    Serial.print("Publish message: ");
//    Serial.println(msg);
//    client.publish("outTopic", msg);
//  }

}
