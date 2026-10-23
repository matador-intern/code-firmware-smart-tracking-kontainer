#include <Arduino.h>  // Required for Serial communication and delay functions
#include <Adafruit_AHTX0.h>
#include <Wire.h>

// Define the AHT sensor object. This is the actual instance.
Adafruit_AHTX0 aht;

float currentTemperatureC = 0.0;
float currentHumidity = 0.0;

/**
 * @brief Initializes the AHT10/AHT20 sensor.
 * This function should be called once in the main setup().
 */
void setupAHTSensor() {
  // Initialize AHT sensor
  if (!aht.begin()) {
    Serial.println("Failed to find AHT10/AHT20 chip");
    // If the sensor fails to initialize, halt the program
    while (1) {
      delay(10);
    }
  }
  Serial.println("AHT10/AHT20 Sensor Found and Initialized!");
  delay(10); // Small delay after initialization
}

/**
 * @brief Reads temperature and humidity from the AHT sensor
 * and stores them in global variables. It also prints the readings
 * to the Serial Monitor.
 */
void readAHTSensor() {
  // Get a new sensor event for both humidity and temperature
  sensors_event_t humidityEvent, tempEvent;
  aht.getEvent(&humidityEvent, &tempEvent);

  // Store the temperature and humidity values in the global variables
  currentTemperatureC = tempEvent.temperature;
  currentHumidity = humidityEvent.relative_humidity;

  // Print the temperature and humidity values to the Serial Monitor
  // Serial.print("AHT10 Temperature: ");
  // Serial.print(currentTemperatureC);
  // Serial.println(" °C");
  // Serial.print("AHT10 Humidity: ");
  // Serial.print(currentHumidity);
  // Serial.println(" %");
  // Serial.println();
}



void setup_AHT() {
  Serial.begin(115200);
  delay(10);

  // Initialize AHT sensor
  if (!aht.begin()) {
    Serial.println("Failed to find AHT10/AHT20 chip");
    while (1) {
      delay(10);
    }
  }

  Serial.println("AHT10/AHT20 Found!");
  delay(10);
}

void loop_AHT() {
  // Get a new sensor event
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);


  // Print the temperature and humidity values
  Serial.print("AHT10 Temperature: ");
  Serial.print(currentTemperatureC);
  Serial.println(" °C");
  Serial.print("AHT10 Humidity: ");
  Serial.print(currentHumidity);
  Serial.println(" %");
  Serial.println();

  // Wait before taking the next reading
  delay(10);
}
