#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <DHT.h>
#include <NewPing.h>

// ==== Wi-Fi Credentials ====
#define WIFI_SSID "CommunityFibre10Gb_7BE64"
#define WIFI_PASS "b1x1jc@bhG"

// ==== Adafruit IO Settings ====
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "krystynawells1"
#define AIO_KEY         "aio_uLaZ00TbZBMc6NSfE3mKjzO0WvEZ"

// ==== Pins ====
#define DHTPIN D5
#define DHTTYPE DHT11
#define MOISTURE_PIN A0
#define TRIG_PIN D6
#define ECHO_PIN D7

// ==== Thresholds ====
#define CLOSE_DISTANCE_CM 20  // Too close to screen
#define MOISTURE_EMPTY 400    // No glass detected

// ==== Simulation mode ====
#define SIMULATION true

// ==== Objects ====
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
DHT dht(DHTPIN, DHTTYPE);
NewPing sonar(TRIG_PIN, ECHO_PIN);

// ==== MQTT Feeds ====
Adafruit_MQTT_Publish tempFeed  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humidFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish moistFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/moisture");
Adafruit_MQTT_Publish distFeed  = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/distance");
Adafruit_MQTT_Publish alertFeed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/alerts");

unsigned long lastDrinkTime = 0;

// ==== Setup Wi-Fi ====
void setup() {
  Serial.begin(115200);
  if (!SIMULATION) dht.begin();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
}

// ==== MQTT Connection ====
void MQTT_connect() {
  int8_t ret;

  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) {
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying in 5 seconds...");
    mqtt.disconnect();
    delay(5000);
  }

  Serial.println("MQTT Connected!");
}

// ==== Main Loop ====
void loop() {
  
  MQTT_connect();

  // ==== Sensor readings ====
  float temp, humid;
  int moisture, distance;

  if (SIMULATION) {
    temp = random(180, 300) / 10.0;       // 18°C – 30°C
    humid = random(30, 70);               // 30% – 70%
    moisture = random(300, 600);          // Simulate glass/no glass
    distance = random(10, 50);            // 10cm – 50cm
  } else {
    temp = dht.readTemperature();
    humid = dht.readHumidity();
    moisture = analogRead(MOISTURE_PIN);
    distance = sonar.ping_cm();
  }

  Serial.printf("Temp: %.2f C, Humidity: %.2f %%, Moisture: %d, Distance: %d cm\n",
                temp, humid, moisture, distance);

  // === Publish sensor data ===
  tempFeed.publish(temp);
  humidFeed.publish(humid);
  moistFeed.publish(moisture);
  distFeed.publish(distance);

  // === Hydration Reminder ===
  unsigned long now = millis();
  if (moisture > MOISTURE_EMPTY) {
    lastDrinkTime = now;  // Glass was placed or touched
  }
  if (now - lastDrinkTime > 1800000) {  // 30 minutes
    alertFeed.publish("Reminder: Take a sip of water!");
    lastDrinkTime = now;
  }

  // === Distance alert ===
  if (distance > 0 && distance < CLOSE_DISTANCE_CM) {
    alertFeed.publish("You're too close to the screen!");
  }

  // === Temp & Humidity alerts ===
  if (temp > 28) {
    alertFeed.publish("It's hot in here — open a window!");
  } else if (temp < 18) {
    alertFeed.publish("It's cold — consider heating.");
  }

  if (humid < 40) {
    alertFeed.publish("Air is dry — ventilate or hydrate.");
  }

  delay(15000);  // Wait 15 seconds before next reading
}
