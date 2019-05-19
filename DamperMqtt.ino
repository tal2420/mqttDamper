extern "C" {
#include "user_interface.h"
}
#include <Stepper.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include "config.h"

const int stepsPerRevolution = 200;  // change this to fit the number of steps per revolution
// of your motor

// initialize the stepper library on pins 8 through 11:
Stepper myStepper(stepsPerRevolution, 15, 12, 13, 14);

// Update these with values suitable for your network.

//const char* ssid = "ssid";
//const char* password = "pass";
//const char* mqtt_server = "192.168.1.123";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
long lastLedCheck = 0;
char msg[50];
int value = 0;
int limiter_pin = 4;
int led_red_pin = 16;
int led_green_pin = 5;
int led_blue_pin = 2;
int toggle_btn_pin = 0;
int toggle_btn_state = 0;

int damperState = 0; //0-stop, 1-closed, 2-open, 3-closing, 4-opening

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  wifi_station_set_hostname("3dDamper");
  Serial.print("Host Name: ");
  Serial.println(WiFi.hostname());
  //WiFi.setSleepMode(WIFI_NONE_SLEEP); 
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    //openDamper();
    if (damperState<2) //not in motion  or already open
    {
      damperState = 4;
    }
    else
    {
      client.publish(outTopic, "error: damper status=" + damperState);
    }
    
    
  }
  else if ((char)payload[0] == '2') {
    //closeDamper();
    if (damperState<3) //not in motion
    {
      damperState = 3;
    }
    else
    {
      client.publish(outTopic, "error: damper status=" + damperState);
    }
  } else {
    //digitalWrite(0, HIGH);  // Turn the LED off by making the voltage HIGH
    client.publish(outTopic, "error: unrecognized action");
  }

}




void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "Damper";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(outTopic, "hello world");
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void releaseEngine() {
  digitalWrite(15, LOW);
  digitalWrite(12, LOW);
  digitalWrite(13, LOW);
  digitalWrite(14, LOW);
}

void setup() {
  // set the speed at 60 rpm:
  myStepper.setSpeed(60);
  // initialize the serial port:
  Serial.begin(115200);

  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(led_red_pin, OUTPUT); // LED
  pinMode(led_green_pin, OUTPUT);
  pinMode(led_blue_pin, OUTPUT);
  pinMode(limiter_pin,INPUT_PULLUP);
  pinMode(toggle_btn_pin,INPUT_PULLUP);
  //digitalWrite(limiter_pin, HIGH);
  
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // OTA begin
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  ArduinoOTA.handle();

  if (!client.connected()) {
  reconnect();
  }
  client.loop();

  //Main logic


  if (damperState == 0) //close Damper on init
  {
    damperState = 3;
  }

  if (damperState == 3 ) //close Damper
  {
    int status;
    status = digitalRead(limiter_pin);
    Serial.println(status);
    if (status == HIGH)
    {
      digitalWrite(led_blue_pin, LOW);
      digitalWrite(led_green_pin, HIGH);
      myStepper.step(-5);
    }
    else
    {
        damperState = 1; //closed
        releaseEngine();
        digitalWrite(led_green_pin, LOW);
        digitalWrite(led_red_pin, HIGH);
    }
    
  }
  
  if (damperState == 4) //opening Damper
  {
    digitalWrite(led_red_pin, LOW);   // Turn the LED on (Note that LOW is the voltage level
    digitalWrite(led_green_pin, HIGH);
    Serial.println("open_damper");
    snprintf (msg, 50, "open_damper", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    
    client.publish(outTopic, msg);
    myStepper.step(-openDamperSteps);
    damperState = 2;
    releaseEngine();
    digitalWrite(led_green_pin, LOW);
    digitalWrite(led_blue_pin, HIGH);
  } 
  
  

  //Watchdog
  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    ++value;
    int status;
    status = digitalRead(limiter_pin);
    Serial.println(status);

    snprintf (msg, 50, "whatchdog is alive #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish(outTopic, msg);
  }

  if (now - lastLedCheck > 2000) {
    lastLedCheck = now;
  
    int status;
    status = digitalRead(limiter_pin);
    //Serial.println(status);
    if (status == 0)
    {
      digitalWrite(led_blue_pin, LOW);
      digitalWrite(led_red_pin, HIGH);
    }
    else if (status == 1)
    {
      digitalWrite(led_red_pin, LOW);
      digitalWrite(led_blue_pin, HIGH);
    }
    
    
    
    
  }

  // check toggle_btn
  int status;
  status = digitalRead(toggle_btn_pin);
  if (status == LOW)
  {
    if (toggle_btn_state == 0) // change
    {
      Serial.println("Button pressed");
      toggle_btn_state = 1;
      //0-stop, 1-closed, 2-open, 3-closing, 4-opening
        if (damperState == 0 || damperState == 2) 
        {
          damperState = 3;
        }
        else if (damperState == 1) 
        {
          damperState = 4;
        }
    }  
  }
  else
  {
    if (toggle_btn_state == 1) // change
    {
      Serial.println("Button depressed");
      toggle_btn_state = 0;

    }
  }
  
  
  

}



