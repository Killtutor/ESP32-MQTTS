#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
// in secrets.h we set the constants to connect correctly
#include "secrets.h"
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define TRIGGER 18
#define ECHO 16
#define HTS1 26
#define HTS2 25
#define TS 4
#define LED 2
#define DHTTYPE DHT21
OneWire oneWire(TS);
DallasTemperature sensors(&oneWire);

DHT dht(HTS1, DHTTYPE);
DHT dht2(HTS2, DHTTYPE);

extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_bundle_start");
// Initialize the WIFI client
WiFiClientSecure espClient;
// Initialize the Pubsub client to MQTT
PubSubClient client(espClient);

/**
 * @brief Handles incoming MQTT messages. This improved version is memory-safe
 * and handles printing the payload correctly.
 * * @param topic The topic the message was received on.
 * @param payload A pointer to the byte array containing the message payload.
 * @param length The length of the payload in bytes.
 */
void callback(char *topic, byte *payload, unsigned int length)
{
  // Print the topic for context. Using Serial.print is efficient.
  Serial.print("Llegó mensaje al tópico: ");
  Serial.println(topic);

  // Print the payload label.
  Serial.print("Payload: ");

  // Print the payload data safely.
  // Serial.write() is the correct function to print a byte array of a known length.
  // It does not require a null terminator and will not read past the buffer.
  Serial.write(payload, length);

  // Print a new line for better readability in the Serial Monitor.
  Serial.println();
  char messageBuffer[length + 1];
  memcpy(messageBuffer, payload, length);
  messageBuffer[length] = '\0';

  if (strcmp(messageBuffer, "true") == 0)
  {
    digitalWrite(LED, HIGH);
    Serial.println("-> Payload es 'true'. Variable actualizada a 1.");
  }
  else if (strcmp(messageBuffer, "false") == 0)
  {
    digitalWrite(LED, LOW);
    delay(5000);
    Serial.println("-> Payload es 'false'. Variable actualizada a 0.");
  }
  else
  {
    Serial.println("-> Payload no reconocido (no es 'true' ni 'false').");
  }
}

// WIFI config
void setup_wifi()
{

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifissid);

  WiFi.begin(wifissid, wifipass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("DNS: ");
  Serial.println(WiFi.dnsIP());
  IPAddress ipHost;
  WiFi.hostByName("mqtt.backend.kriollotech.com", ipHost);
  Serial.println("IPHOST: ");
  Serial.println(ipHost);
}

// MQTT config
void setup_mqtt()
{
  espClient.setCACertBundle(rootca_crt_bundle_start);
  client.setServer(mqtt_host, mqtt_port);
  client.setKeepAlive(1500);
  client.setCallback(callback);
}

void reconnect()
{
  // Loop hasta que estemos reconectados
  while (!client.connected())
  {
    Serial.print("Intentando conectar al MQTT...");
    // Intenta conectar con usuario y contraseña
    if (client.connect("ESP32Client", mqtt_user, mqtt_pass))
    {
      Serial.println("conectado");
      // Una vez conectado, puedes suscribirte a un tema
      client.subscribe("EIE_SEDE1_modbus/1/coil/0");

      // Publicar un mensaje por HTTP
      // Distintos tipos de datos
      client.publish("EIE_SEDE1_http/alphanumeric", "Hola desde ESP32!");
      client.publish("EIE_SEDE2_http/numeric", "123.45");
      client.publish("EIE_SEDE2_http/int", "123");
      client.publish("EIE_SEDE2_http/boolean", "true");
      client.publish("EIE_SEDE2_http/ejemploJSON", "{\"texto\":\"Aja, texto en JSON\",\"numero\":123.45,\"entero\":12,\"boolean\":1}");

      // Publicar un mensaje por Modbus
      // SEDE/esclavo/tipo_dato/offset
      // Distintos tipos de datos
      client.publish("EIE_SEDE2_modbus/1/string/8", "Hola desde ESP32!");
      client.publish("EIE_SEDE2_modbus/1/holding/0", "123.45");
      client.publish("EIE_SEDE2_modbus/1/input/0", "123");
    }
    else
    {
      Serial.print("Error de conexión, rc=");
      Serial.print(client.state());
      Serial.println(" Intentando de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(9600);
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT_PULLDOWN);
  pinMode(HTS1, INPUT_PULLUP);
  pinMode(HTS2, INPUT_PULLUP);
  pinMode(TS, INPUT_PULLUP);
  pinMode(LED, OUTPUT);
  setup_wifi();
  setup_mqtt();
  dht.begin();
  dht2.begin();
  sensors.begin();
  Serial.print("Found ");
  Serial.print(sensors.getDeviceCount(), DEC);
  Serial.println(" devices.");
}

void readDistanceAndPublish()
{
  float totalDistance = 0;
  float samples[10]; // Array to store 10 samples

  // Take 10 samples
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(TRIGGER, LOW);
    delayMicroseconds(5);
    digitalWrite(TRIGGER, HIGH);
    delayMicroseconds(25);
    digitalWrite(TRIGGER, LOW);

    float duration = pulseIn(ECHO, HIGH);
    samples[i] = (duration * 0.0343) / 2;
    totalDistance += samples[i];
    delay(50); // Short delay between samples
  }

  // Calculate average
  float averageDistance = totalDistance / 10;

  // Print average distance
  Serial.print("Average Distance: ");
  Serial.print(averageDistance);
  Serial.println(" cm");

  // Publish average distance
  char str[20];
  sprintf(str, "%.2f", averageDistance);
  client.publish("EIE_SEDE1_http/numeric", str);
  delay(50);
}

void readTyH()
{
  float totalT = 0;  // Array to store 10 samples
  float totalH = 0;  // Array to store 10 samples
  float totalT2 = 0; // Array to store 10 samples
  float totalH2 = 0; // Array to store 10 samples

  // Take 10 samples
  for (int i = 0; i < 10; i++)
  {
    delay(50);
    // Reading temperature or humidity takes about 250 milliseconds!
    float humi1 = dht.readHumidity();

    if (isnan(humi1))
    {
      humi1 = 0;
    }
    Serial.print("humi1: ");
    Serial.print(humi1);
    totalH += humi1;
    // Reading temperature or humidity takes about 250 milliseconds!
    float humi2 = dht2.readHumidity();
    if (isnan(humi2))
    {
      humi2 = 0;
    }
    Serial.print("hum2: ");
    Serial.print(humi2);
    totalH2 += humi2;
    // Read temperature as Celsius (the default)
    float temp1 = dht.readTemperature();
    if (isnan(temp1))
    {
      temp1 = 0;
    }
    Serial.print("temp1: ");
    Serial.print(temp1);
    Serial.print("C ");

    totalT += temp1;
    // Read temperature as Celsius (the default)
    float temp2 = dht2.readTemperature();
    if (isnan(temp2))
    {
      temp2 = 0;
    }
    Serial.print("temp2: ");
    Serial.print(temp2);
    Serial.println("C");
    totalT2 += temp2;
  }
  // Publish average distance
  char str[20];
  sprintf(str, "%.2f", totalT / 10);
  client.publish("EIE_SEDE1_http/temp", str);
  Serial.print("Average temp1: ");
  Serial.print(str);
  Serial.println("C");
  sprintf(str, "%.2f", totalT2 / 10);
  client.publish("EIE_SEDE2_http/temp", str);
  Serial.print("Average temp2: ");
  Serial.print(str);
  Serial.println("C");
  sprintf(str, "%.2f", totalH / 10);
  client.publish("EIE_SEDE1_http/humidity", str);
  Serial.print("Average humi1: ");
  Serial.print(str);
  Serial.println("%");
  sprintf(str, "%.2f", totalH2 / 10);
  client.publish("EIE_SEDE2_http/humidity", str);
  Serial.print("Average humi2: ");
  Serial.print(str);
  Serial.println("%");
}

void readT()
{
  float temperatura = 0;
  for (int i = 0; i < 10; i++)
  {
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);
    Serial.printf("Temperatura: %.2f °C\n", tempC);
    temperatura += tempC;
  }
  char str[20];
  sprintf(str, "%.2f", temperatura / 10);
  client.publish("EIE_SEDE1_modbus/1/holding/0", str);
  Serial.print("Average temp1: ");
  Serial.print(str);
  Serial.println("C");
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  readDistanceAndPublish();
  delay(50);
  readTyH();
  delay(50);
  readT();
  delay(200); // Espera 2 segundos antes de publicar de nuevo

  // put your main code here, to run repeatedly:
}
