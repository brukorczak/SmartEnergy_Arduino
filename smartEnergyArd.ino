#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <Arduino.h>
#include <EmonLib.h>
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#endif

#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>

EnergyMonitor SCT013;
int pinSCT = A0;
double tensao = 127.0;
double consumo = 0.0;
double potencia = 0.0;
double Irms = 0;
unsigned long tempoAnterior = 0;
unsigned long tempoAnteriorSerial = 0;
double medicaoAtual = 0;
double kwh = 0;
String date = "";
String timeValue = "";
int idResidencia = 1;
int id = 0;

#define WIFI_SSID "BARROCA"
#define WIFI_PASSWORD "barroca9246.@"
#define API_KEY "AIzaSyDVcv7oOgDvh7be9uSfrgVQHr4vWnJe93M"
#define DATABASE_URL "https://smartenergyfirebase-420ab-default-rtdb.firebaseio.com"
#define USER_EMAIL "igorbtenorioidi@gmail.com"
#define USER_PASSWORD "123456789"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;

const size_t JSON_CAPACITY = 512;
StaticJsonDocument<JSON_CAPACITY> jsonDoc;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif

void setup()
{
  SCT013.current(pinSCT, 6.0606);
  Serial.begin(9600);
  tempoAnterior = millis();

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  multi.addAP(WIFI_SSID, WIFI_PASSWORD);
  multi.run();
#else
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif

  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    if (millis() - ms > 10000)
      break;
#endif
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

#if defined(ESP8266)
  fbdo.setBSSLBufferSize(2048, 2048);
#endif

  fbdo.setResponseSize(2048);
  Firebase.begin(&config, &auth);

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
  config.wifi.clearAP();
  config.wifi.addAP(WIFI_SSID, WIFI_PASSWORD);
#endif

  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;
}

void loop()
{
 
  Irms = SCT013.calcIrms(1480);
  potencia = Irms * tensao;
  consumo += potencia * (millis() - tempoAnterior) / 3600000.0;
  

  Serial.println("consumo ------------> " + String(consumo));

  if (Firebase.ready() && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();

    WiFiClient client;
    client.setTimeout(5000);

    if (client.connect("worldtimeapi.org", 80))
    {
      client.print(String("GET ") + "/api/timezone/America/Sao_Paulo" + " HTTP/1.1\r\n" +
                   "Host: " + "worldtimeapi.org" + "\r\n" +
                   "User-Agent: Arduino/1.0\r\n" +
                   "Connection: close\r\n\r\n");
    }

    String response = "";
    while (client.connected() && !client.available())
    {
      Serial.println("Aguardando resposta do servidor...");
      delay(100);
    }

    if (client.available())
    {
      response = client.readString();
    }
    else
    {
      Serial.println("Sem resposta do servidor.");
    }

    client.stop();

    int jsonStart = response.indexOf('{');
    int jsonEnd = response.lastIndexOf('}');
    String jsonValue = response.substring(jsonStart, jsonEnd + 1);

    if (!jsonValue.isEmpty())
    {
      DeserializationError error = deserializeJson(jsonDoc, jsonValue);
      if (!error)
      {
        const char *datetime = jsonDoc["datetime"];

        // Extrai apenas a data
        date = String(datetime).substring(0, 10);
        // Encontra a posição dos dois pontos ":"
        timeValue = String(datetime).substring(11, 16);

        Serial.print("Hora: ");
        Serial.println(timeValue);

        Serial.print("Data: ");
        Serial.println(date);

        Serial.println("id: " + String(id++));
        
        medicaoAtual += consumo;
        
        Serial.printf("Set idResidencia... %s\n", Firebase.RTDB.setInt(&fbdo, (String("Medidor/") + id + "/idResidencia").c_str(), idResidencia) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set consumo... %s\n", Firebase.RTDB.setDouble(&fbdo, (String("Medidor/") + id + "/consumo").c_str(), consumo/1000) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set medicaoAtual... %s\n", Firebase.RTDB.setDouble(&fbdo, (String("Medidor/") + id + "/medicaoAtual").c_str(), medicaoAtual/1000) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set date... %s\n", Firebase.RTDB.setString(&fbdo, (String("Medidor/") + id + "/data").c_str(), date) ? "ok" : fbdo.errorReason().c_str());
        Serial.printf("Set horario... %s\n", Firebase.RTDB.setString(&fbdo, (String("Medidor/") + id + "/horario").c_str(), timeValue) ? "ok" : fbdo.errorReason().c_str());

        consumo = 0;
      }
      else
      {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
      }
    }
  }

  delay(100);
}