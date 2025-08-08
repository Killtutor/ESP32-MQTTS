ESP32-MQTTS
Este proyecto proporciona un ejemplo de firmware robusto y bien documentado para un microcontrolador ESP32, que permite la comunicación segura a través de MQTTS (MQTT sobre SSL/TLS). Es un punto de partida ideal para desarrolladores que necesitan implementar soluciones IoT seguras con capacidades de lectura de sensores, publicación y suscripción.

🌟 Características Principales
Comunicación Segura (MQTTS): Utiliza SSL/TLS para cifrar el tráfico MQTT, garantizando la confidencialidad e integridad de los datos.

Certificados Incluidos: Incluye un bundle de certificados para establecer una conexión segura sin necesidad de configuraciones complejas.

Funcionalidad Completa: Demuestra las operaciones esenciales de MQTT: conexión, publicación de datos y suscripción a tópicos.

Lectura de Sensores: El código está diseñado para ser fácilmente adaptable, mostrando cómo leer datos de sensores (como temperatura y humedad) y publicarlos de forma segura.

Compatible con ESP32: Optimizado para la familia de microcontroladores ESP32, aprovechando su conectividad Wi-Fi y capacidades de seguridad.

Open-Source: El código es de libre uso, permitiendo su modificación y adaptación a las necesidades de cualquier proyecto.

📦 Requisitos del Hardware y Software
Hardware:

Placa de desarrollo ESP32 (cualquier modelo).

Sensores compatibles (ej. DS18B20, DHT22).

Software:

Arduino IDE o PlatformIO.

Librerías de Arduino para MQTT, Wi-Fi y los sensores utilizados.

🚀 Uso y Configuración
Clona el repositorio:

Bash

git clone https://github.com/Killtutor/ESP32-MQTTS.git
Abre el proyecto: Carga el archivo .ino en tu IDE de Arduino.

Configura las credenciales: Edita el archivo de configuración para ingresar tus credenciales de Wi-Fi, tu servidor MQTT y la información del broker.

Sube el código: Compila y sube el firmware a tu ESP32.

🤝 Contribuir
¡Las contribuciones son bienvenidas! Si encuentras un bug, tienes una sugerencia o quieres añadir una nueva funcionalidad, no dudes en abrir un issue o enviar un pull request.

📝 Licencia
Este proyecto está bajo la Licencia MIT. Consulta el archivo LICENSE para más detalles.
