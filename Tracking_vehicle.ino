#define TINY_GSM_MODEM_SIM800

#include <HardwareSerial.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

HardwareSerial SerialAT(2); // RX, TX

// Network details
const char apn[] = "internet";
const char user[] = "";
const char pass[] = "";

// MQTT details
const char *broker = "test.mosquitto.org";
const char *topicOut = "testtopic/out";
const char *topicIn = "testtopic/in";

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);


SoftwareSerial SerialGPS(12, 13); 
TinyGPSPlus gps;

// Pin kontrol relay
const int relayPin = 2; 
bool relayState = false;

uint32_t lastReconnectAttempt = 0;
char receivedPayload[500];

void setup() {
  Serial.begin(115200);
  SerialAT.begin(9600, SERIAL_8N1, 16, 17);

  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); 

  Serial.println("System start.");
  modem.restart();
  Serial.println("Modem: " + modem.getModemInfo());

  // Coba koneksi GSM hingga berhasil
  while (!initGSM()) {
    Serial.println("GSM initialization failed. Retrying in 5 seconds...");
    delay(5000);
  }

  SerialGPS.begin(9600);
  Serial.println("GPS module initialized.");

  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
  Serial.println("Connecting to MQTT Broker: " + String(broker));
  while (!mqttConnect()) {
    Serial.println("MQTT connection failed. Retrying in 5 seconds...");
    delay(5000);
  }
  Serial.println();
}

void loop() {
  gpsData();  

  if (mqtt.connected()) {
    mqtt.loop();
  }
}

boolean initGSM() {
  Serial.println("Searching for telco provider.");
  if (!modem.waitForNetwork()) {
    Serial.println("No network found.");
    return false;
  }
  Serial.println("Connected to telco.");
  Serial.println("Signal Quality: " + String(modem.getSignalQuality()));

  Serial.println("Connecting to GPRS network.");
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println("GPRS connection failed.");
    return false;
  }
  Serial.println("Connected to GPRS: " + String(apn));
  return true;
}

boolean mqttConnect() {
  if (!mqtt.connect("GsmClientTest")) {
    Serial.print(".");
    return false;
  }
  Serial.println("Connected to broker.");
  mqtt.publish(topicOut, "Connected");
  mqtt.subscribe(topicIn);
  return mqtt.connected();
}

void mqttCallback(char *topic, byte *payload, unsigned int len) {
  Serial.print("Message received: ");
  Serial.write(payload, len);
  Serial.println();
  if (String(topic) == topicIn) { 
    if ((char)payload[0] == '1') {
      mqtt.publish(topicOut, "Relay ON");
      Serial.println("Relay ON");
      digitalWrite(relayPin, HIGH);
    } else if ((char)payload[0] == '0') {
      digitalWrite(relayPin, LOW);
      Serial.println("Relay OFF");
      mqtt.publish(topicOut, "Relay OFF");
    }
  } 
  
}
void gpsData(){
    while (SerialGPS.available() > 0) {
    if (gps.encode(SerialGPS.read())) {
      if (gps.location.isValid()) {
        float latitude = gps.location.lat();
        float longitude = gps.location.lng();
//        Serial.print("Latitude: ");
//        Serial.println(latitude, 6);
//        Serial.print("Longitude: ");
//        Serial.println(longitude, 6);

        char gpsDataChar[100];
        snprintf(gpsDataChar, sizeof(gpsDataChar), "Latitude: %.6f, Longitude: %.6f", latitude, longitude);
        Serial.println(gpsDataChar);
        mqtt.publish(topicOut, gpsDataChar);
        delay(1000);
      }
    }
    }
}
