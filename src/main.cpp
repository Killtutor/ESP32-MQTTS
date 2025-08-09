/**
 * @file remota.cpp
 * @brief Sistema de Monitoreo Remoto con ESP32 para Tesis de Ingenieria
 * @author [Tu Nombre]
 * @date [Fecha]
 * @version 1.0
 *
 * @description
 * Este codigo implementa un sistema de monitoreo remoto utilizando un ESP32 que:
 * - Se conecta a WiFi y MQTT de forma segura
 * - Lee sensores de temperatura, humedad y distancia
 * - Publica datos en tiempo real a traves de MQTT
 * - Implementa protocolos Modbus y HTTP para comunicacion
 *
 * @note Para compilar este codigo, es necesario generar el bundle de certificados
 *       usando el script generate_cert_bundle.py que crea el archivo _binary_data_cert_bundle_start
 *
 * @see generate_cert_bundle.py
 * @see secrets.h
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* ============================================================================
 * CONFIGURACION DE PINES Y CONSTANTES
 * ============================================================================ */

// Pines de sensores y actuadores
#define PIN_TRIGGER_ULTRASONICO 18 ///< Pin de disparo del sensor ultrasonico
#define PIN_ECHO_ULTRASONICO 16    ///< Pin de eco del sensor ultrasonico
#define PIN_DHT_SENSOR_1 26        ///< Pin del primer sensor DHT (temperatura/humedad)
#define PIN_DHT_SENSOR_2 25        ///< Pin del segundo sensor DHT (temperatura/humedad)
#define PIN_ONE_WIRE_TEMP 4        ///< Pin del bus OneWire para sensores DS18B20
#define PIN_LED_INDICADOR 2        ///< Pin del LED indicador de estado

// Configuracion de sensores
#define TIPO_DHT DHT21           ///< Tipo de sensor DHT (DHT21, DHT22, DHT11)
#define NUMERO_MUESTRAS 10       ///< Numero de muestras para promediar lecturas
#define DELAY_ENTRE_MUESTRAS 50  ///< Delay entre muestras en milisegundos
#define DELAY_ENTRE_SENSORES 200 ///< Delay entre lecturas de diferentes sensores

// Configuracion de comunicacion
#define KEEP_ALIVE_MQTT 1500   ///< Keep alive del cliente MQTT en segundos
#define TIEMPO_RECONEXION 5000 ///< Tiempo de espera para reconexion MQTT

/* ============================================================================
 * DECLARACION DE VARIABLES GLOBALES
 * ============================================================================ */

/**
 * @brief Bundle de certificados CA para conexion segura
 *
 * @important CONFIGURACION DEL BUNDLE DE CERTIFICADOS:
 * Este archivo se genera automaticamente usando el script generate_cert_bundle.py
 * ubicado en la carpeta scripts/ del proyecto. El script:
 *
 * 1. Lee certificados desde la carpeta ssl_certs/
 * 2. Los combina en un bundle
 * 3. Genera el archivo _binary_data_cert_bundle_start
 * 4. Este archivo se incluye automaticamente en el build
 *
 * Para regenerar el bundle:
 * python3 scripts/generate_cert_bundle.py
 *
 * @see scripts/generate_cert_bundle.py
 * @see ssl_certs/
 */
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_data_cert_bundle_start");

// Instancias de sensores
OneWire oneWire(PIN_ONE_WIRE_TEMP);
DallasTemperature sensoresTemperatura(&oneWire);
DHT sensorDHT1(PIN_DHT_SENSOR_1, TIPO_DHT);
DHT sensorDHT2(PIN_DHT_SENSOR_2, TIPO_DHT);

// Clientes de red
WiFiClientSecure clienteWiFiSeguro;
PubSubClient clienteMQTT(clienteWiFiSeguro);

/* ============================================================================
 * PROTOTIPOS DE FUNCIONES
 * ============================================================================ */

void configurarWiFi(void);
void configurarMQTT(void);
void reconectarMQTT(void);
void leerDistanciaYPublicar(void);
void leerTemperaturaYHumedad(void);
void leerTemperaturaOneWire(void);
void callbackMQTT(char *topic, byte *payload, unsigned int length);
void imprimirSeparador(int longitud);

/* ============================================================================
 * FUNCIONES AUXILIARES
 * ============================================================================ */

/**
 * @brief Imprime una linea separadora de caracteres
 * @param longitud Numero de caracteres a imprimir
 */
void imprimirSeparador(int longitud)
{
  for (int i = 0; i < longitud; i++)
  {
    Serial.print("=");
  }
  Serial.println();
}

/* ============================================================================
 * FUNCIONES PRINCIPALES
 * ============================================================================ */

/**
 * @brief Funcion de callback para mensajes MQTT entrantes
 *
 * Esta funcion se ejecuta cada vez que se recibe un mensaje MQTT en los
 * topicos a los que esta suscrito el ESP32. Maneja comandos para controlar
 * el LED indicador y otros actuadores.
 *
 * @param topic Topico MQTT donde se recibio el mensaje
 * @param payload Puntero al array de bytes del mensaje
 * @param length Longitud del payload en bytes
 *
 * @note La funcion es segura en memoria y no lee mas alla del buffer
 */
void callbackMQTT(char *topic, byte *payload, unsigned int length)
{
  // Crear buffer temporal para el mensaje
  char bufferMensaje[length + 1];
  memcpy(bufferMensaje, payload, length);
  bufferMensaje[length] = '\0';

  // Imprimir el payload recibido de forma segura
  Serial.print("Mensaje recibido en topico: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  Serial.println(bufferMensaje);

  // Procesar comandos segun el payload
  if (strcmp(bufferMensaje, "true") == 0)
  {
    digitalWrite(PIN_LED_INDICADOR, HIGH);
    Serial.println("-> LED encendido - Comando 'true' recibido");
  }
  else if (strcmp(bufferMensaje, "false") == 0)
  {
    digitalWrite(PIN_LED_INDICADOR, LOW);
    delay(5000); // Mantener apagado por 5 segundos
    Serial.println("-> LED apagado - Comando 'false' recibido");
  }
  else
  {
    Serial.println("-> Payload no reconocido - Comando ignorado");
  }
}

/**
 * @brief Configura y establece la conexion WiFi
 *
 * Esta funcion maneja la conexion WiFi del ESP32, incluyendo:
 * - Inicializacion de la conexion
 * - Espera hasta que la conexion este establecida
 * - Resolucion DNS del servidor MQTT
 * - Impresion de informacion de red
 */
void configurarWiFi(void)
{
  Serial.println();
  imprimirSeparador(50);
  Serial.println("CONFIGURANDO CONEXION WiFi");
  imprimirSeparador(50);

  WiFi.begin(wifissid, wifipass);

  Serial.print("Conectando a red WiFi: ");
  Serial.println(wifissid);
  Serial.print("Estado de conexion: ");

  // Esperar hasta que la conexion WiFi este establecida
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Inicializar generador de numeros aleatorios
  randomSeed(micros());

  // Mostrar informacion de la conexion establecida
  Serial.println("\n-> WiFi conectado exitosamente");
  Serial.println("Informacion de red:");
  Serial.print("  * Direccion IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("  * Direccion DNS: ");
  Serial.println(WiFi.dnsIP());

  // Resolver IP del servidor MQTT
  IPAddress ipServidorMQTT;
  if (WiFi.hostByName("mqtt.backend.kriollotech.com", ipServidorMQTT))
  {
    Serial.print("  * Servidor MQTT: ");
    Serial.println(ipServidorMQTT);
  }
  else
  {
    Serial.println("  * Error resolviendo servidor MQTT");
  }

  imprimirSeparador(50);
}

/**
 * @brief Configura la conexion MQTT segura
 *
 * Esta funcion configura el cliente MQTT con:
 * - Certificados CA para conexion segura
 * - Parametros de conexion del servidor
 * - Configuracion de keep-alive
 * - Funcion de callback para mensajes entrantes
 */
void configurarMQTT(void)
{
  Serial.println("CONFIGURANDO CONEXION MQTT SEGURA");
  imprimirSeparador(50);

  // Configurar certificados CA para conexion segura
  clienteWiFiSeguro.setCACertBundle(rootca_crt_bundle_start);

  // Configurar servidor MQTT
  clienteMQTT.setServer(mqtt_host, mqtt_port);
  clienteMQTT.setKeepAlive(KEEP_ALIVE_MQTT);
  clienteMQTT.setCallback(callbackMQTT);

  Serial.println("-> Cliente MQTT configurado");
  Serial.print("  * Servidor: ");
  Serial.println(mqtt_host);
  Serial.print("  * Puerto: ");
  Serial.println(mqtt_port);
  Serial.print("  * Keep-alive: ");
  Serial.print(KEEP_ALIVE_MQTT);
  Serial.println(" segundos");
  imprimirSeparador(50);
}

/**
 * @brief Reconoecta al servidor MQTT si se pierde la conexion
 *
 * Esta funcion maneja la reconexion automatica al servidor MQTT:
 * - Intenta reconectar hasta que sea exitoso
 * - Se suscribe a los topicos necesarios
 * - Publica mensajes de prueba para verificar la conexion
 * - Implementa reintentos con delays apropiados
 */
void reconectarMQTT(void)
{
  Serial.println("RECONECTANDO AL SERVIDOR MQTT");

  while (!clienteMQTT.connected())
  {
    Serial.print("Intentando conexion MQTT... ");

    // Intentar conectar con credenciales
    if (clienteMQTT.connect("ESP32Client", mqtt_user, mqtt_pass))
    {
      Serial.println("-> Conectado exitosamente");

      // Suscribirse a topicos de control
      if (clienteMQTT.subscribe("EIE_SEDE1_modbus/1/coil/0"))
      {
        Serial.println("-> Suscrito a topico de control");
      }

      // Publicar mensajes de prueba HTTP
      Serial.println("Publicando mensajes de prueba HTTP:");
      clienteMQTT.publish("EIE_SEDE1_http/alphanumeric", "Sistema ESP32 operativo");
      clienteMQTT.publish("EIE_SEDE2_http/numeric", "123.45");
      clienteMQTT.publish("EIE_SEDE2_http/int", "123");
      clienteMQTT.publish("EIE_SEDE2_http/boolean", "true");
      clienteMQTT.publish("EIE_SEDE2_http/ejemploJSON",
                          "{\"sistema\":\"ESP32\",\"estado\":\"operativo\",\"timestamp\":1234567890}");

      // Publicar mensajes de prueba Modbus
      Serial.println("Publicando mensajes de prueba Modbus:");
      clienteMQTT.publish("EIE_SEDE2_modbus/1/string/8", "Test desde ESP32");
      clienteMQTT.publish("EIE_SEDE2_modbus/1/holding/0", "123.45");
      clienteMQTT.publish("EIE_SEDE2_modbus/1/input/0", "123");

      Serial.println("-> Mensajes de prueba publicados");
    }
    else
    {
      Serial.print("-> Error de conexion, codigo: ");
      Serial.print(clienteMQTT.state());
      Serial.print(" - Reintentando en ");
      Serial.print(TIEMPO_RECONEXION / 1000);
      Serial.println(" segundos...");
      delay(TIEMPO_RECONEXION);
    }
  }
}

/**
 * @brief Lee la distancia del sensor ultrasonico y publica el resultado
 *
 * Esta funcion implementa un algoritmo de muestreo multiple para mejorar
 * la precision de las lecturas del sensor ultrasonico:
 * - Toma multiples muestras consecutivas
 * - Calcula el promedio para reducir ruido
 * - Publica el resultado en el topico MQTT correspondiente
 * - Implementa delays apropiados entre muestras
 */
void leerDistanciaYPublicar(void)
{
  float distanciaTotal = 0.0;
  float muestras[NUMERO_MUESTRAS];

  // Tomar multiples muestras para promediar
  for (int i = 0; i < NUMERO_MUESTRAS; i++)
  {
    // Generar pulso ultrasonico
    digitalWrite(PIN_TRIGGER_ULTRASONICO, LOW);
    delayMicroseconds(5);
    digitalWrite(PIN_TRIGGER_ULTRASONICO, HIGH);
    delayMicroseconds(25);
    digitalWrite(PIN_TRIGGER_ULTRASONICO, LOW);

    // Medir tiempo de eco
    float duracion = pulseIn(PIN_ECHO_ULTRASONICO, HIGH);
    float distancia = (duracion * 0.0343) / 2; // Velocidad del sonido (cm/us)

    muestras[i] = distancia;
    distanciaTotal += distancia;

    delay(DELAY_ENTRE_MUESTRAS);
  }

  // Calcular distancia promedio
  float distanciaPromedio = distanciaTotal / NUMERO_MUESTRAS;

  // Publicar resultado
  char bufferDistancia[20];
  sprintf(bufferDistancia, "%.2f", distanciaPromedio);

  if (clienteMQTT.publish("EIE_SEDE1_http/numeric", bufferDistancia))
  {
    Serial.print("-> Distancia publicada: ");
    Serial.print(bufferDistancia);
    Serial.println(" cm");
  }
  else
  {
    Serial.println("-> Error publicando distancia");
  }
}

/**
 * @brief Lee temperatura y humedad de los sensores DHT
 *
 * Esta funcion lee multiples sensores DHT simultaneamente:
 * - Sensor DHT1 en pin 26
 * - Sensor DHT2 en pin 25
 * - Implementa muestreo multiple para precision
 * - Publica datos en topicos separados por sede
 * - Maneja errores de lectura de forma robusta
 */
void leerTemperaturaYHumedad(void)
{
  float temperaturaTotal1 = 0.0, humedadTotal1 = 0.0;
  float temperaturaTotal2 = 0.0, humedadTotal2 = 0.0;

  // Tomar multiples muestras de cada sensor
  for (int i = 0; i < NUMERO_MUESTRAS; i++)
  {
    delay(DELAY_ENTRE_MUESTRAS);

    // Leer sensor DHT1
    float humedad1 = sensorDHT1.readHumidity();
    float temperatura1 = sensorDHT1.readTemperature();

    // Leer sensor DHT2
    float humedad2 = sensorDHT2.readHumidity();
    float temperatura2 = sensorDHT2.readTemperature();

    // Validar lecturas y acumular valores
    if (!isnan(humedad1))
      humedadTotal1 += humedad1;
    if (!isnan(temperatura1))
      temperaturaTotal1 += temperatura1;
    if (!isnan(humedad2))
      humedadTotal2 += humedad2;
    if (!isnan(temperatura2))
      temperaturaTotal2 += temperatura2;
  }

  // Calcular promedios
  float humedadPromedio1 = humedadTotal1 / NUMERO_MUESTRAS;
  float temperaturaPromedio1 = temperaturaTotal1 / NUMERO_MUESTRAS;
  float humedadPromedio2 = humedadTotal2 / NUMERO_MUESTRAS;
  float temperaturaPromedio2 = temperaturaTotal2 / NUMERO_MUESTRAS;

  // Publicar datos de sede 1
  char bufferDatos[20];
  sprintf(bufferDatos, "%.2f", temperaturaPromedio1);
  clienteMQTT.publish("EIE_SEDE1_http/temp", bufferDatos);

  sprintf(bufferDatos, "%.2f", humedadPromedio1);
  clienteMQTT.publish("EIE_SEDE1_http/humidity", bufferDatos);

  // Publicar datos de sede 2
  sprintf(bufferDatos, "%.2f", temperaturaPromedio2);
  clienteMQTT.publish("EIE_SEDE2_http/temp", bufferDatos);

  sprintf(bufferDatos, "%.2f", humedadPromedio2);
  clienteMQTT.publish("EIE_SEDE2_http/humidity", bufferDatos);

  // Mostrar resumen en consola
  Serial.println("-> Datos DHT publicados:");
  Serial.printf("  * Sede 1 - Temp: %.2f C, Hum: %.2f%%\n",
                temperaturaPromedio1, humedadPromedio1);
  Serial.printf("  * Sede 2 - Temp: %.2f C, Hum: %.2f%%\n",
                temperaturaPromedio2, humedadPromedio2);
}

/**
 * @brief Lee temperatura del sensor OneWire (DS18B20)
 *
 * Esta funcion lee el sensor de temperatura DS18B20 conectado al bus OneWire:
 * - Implementa muestreo multiple para precision
 * - Publica datos en formato Modbus
 * - Maneja la comunicacion OneWire de forma robusta
 */
void leerTemperaturaOneWire(void)
{
  float temperaturaTotal = 0.0;

  // Tomar multiples muestras
  for (int i = 0; i < NUMERO_MUESTRAS; i++)
  {
    sensoresTemperatura.requestTemperatures();
    float temperatura = sensoresTemperatura.getTempCByIndex(0);

    if (!isnan(temperatura))
    {
      temperaturaTotal += temperatura;
    }

    delay(DELAY_ENTRE_MUESTRAS);
  }

  // Calcular temperatura promedio
  float temperaturaPromedio = temperaturaTotal / NUMERO_MUESTRAS;

  // Publicar en formato Modbus
  char bufferTemperatura[20];
  sprintf(bufferTemperatura, "%.2f", temperaturaPromedio);

  if (clienteMQTT.publish("EIE_SEDE1_modbus/1/holding/0", bufferTemperatura))
  {
    Serial.printf("-> Temperatura OneWire publicada: %.2f C\n", temperaturaPromedio);
  }
  else
  {
    Serial.println("-> Error publicando temperatura OneWire");
  }
}

/* ============================================================================
 * FUNCIONES DE INICIALIZACION Y BUCLE PRINCIPAL
 * ============================================================================ */

/**
 * @brief Funcion de inicializacion del sistema
 *
 * Esta funcion se ejecuta una vez al inicio y configura:
 * - Comunicacion serial para debugging
 * - Pines de entrada/salida
 * - Conexiones WiFi y MQTT
 * - Inicializacion de sensores
 */
void setup(void)
{
  // Configurar comunicacion serial
  Serial.begin(9600);
  Serial.println();
  imprimirSeparador(60);
  Serial.println("SISTEMA DE MONITOREO REMOTO ESP32 - INICIANDO");
  imprimirSeparador(60);

  // Configurar pines
  pinMode(PIN_TRIGGER_ULTRASONICO, OUTPUT);
  pinMode(PIN_ECHO_ULTRASONICO, INPUT_PULLDOWN);
  pinMode(PIN_DHT_SENSOR_1, INPUT_PULLUP);
  pinMode(PIN_DHT_SENSOR_2, INPUT_PULLUP);
  pinMode(PIN_ONE_WIRE_TEMP, INPUT_PULLUP);
  pinMode(PIN_LED_INDICADOR, OUTPUT);

  Serial.println("-> Pines configurados");

  // Inicializar sensores
  sensorDHT1.begin();
  sensorDHT2.begin();
  sensoresTemperatura.begin();
  Serial.println("-> Sensores inicializados");

  // Configurar conexiones de red
  configurarWiFi();
  configurarMQTT();

  Serial.println("-> Sistema inicializado completamente");
  imprimirSeparador(60);
}

/**
 * @brief Bucle principal del sistema
 *
 * Esta funcion se ejecuta continuamente y maneja:
 * - Verificacion de conexion MQTT
 * - Lectura y publicacion de datos de sensores
 * - Reconexion automatica si es necesario
 * - Timing apropiado entre operaciones
 */
void loop(void)
{
  // Verificar conexion MQTT
  if (!clienteMQTT.connected())
  {
    reconectarMQTT();
  }

  // Procesar mensajes MQTT entrantes
  clienteMQTT.loop();

  // Ciclo de lectura de sensores
  leerDistanciaYPublicar();
  delay(DELAY_ENTRE_SENSORES);

  leerTemperaturaYHumedad();
  delay(DELAY_ENTRE_SENSORES);

  leerTemperaturaOneWire();
  delay(DELAY_ENTRE_SENSORES);

  // Delay adicional para estabilidad
  delay(DELAY_ENTRE_SENSORES);
}

/* ============================================================================
 * NOTAS IMPORTANTES PARA COMPILACION
 * ============================================================================
 *
 * 1. CERTIFICADOS SSL:
 *    - Ejecutar: python3 scripts/generate_cert_bundle.py
 *    - Esto genera el archivo _binary_data_cert_bundle_start
 *    - El archivo se incluye automaticamente en el build
 *
 * 2. ARCHIVO SECRETS.H:
 *    - Crear archivo secrets.h con credenciales WiFi y MQTT
 *    - NO incluir este archivo en el control de versiones
 *
 * 3. DEPENDENCIAS:
 *    - WiFi.h, WiFiClientSecure.h
 *    - PubSubClient.h
 *    - DHT.h, OneWire.h, DallasTemperature.h
 *
 * 4. CONFIGURACION PLATFORMIO:
 *    - Asegurar que las librerias esten instaladas
 *    - Verificar configuracion de certificados SSL
 *
 * ============================================================================ */
