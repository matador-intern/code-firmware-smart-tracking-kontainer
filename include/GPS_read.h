#include <HardwareSerial.h> // Include HardwareSerial for Serial2
#include "defines.h"      // Include defines.h for global definitions and objects


// --- GPS Module Definitions ---
// These are included from defines.h, so re-defining them here is redundant
// #define RXD2 16
// #define TXD2 17
// #define GPS_BAUD 9600

// The TinyGPSPlus object 'gps' and HardwareSerial 'gpsSerial' are
// declared globally in defines.h. Their usage here relies on 'defines.h'
// being included.

void setup_gps() {
  // Serial.begin(115200); // Usually initialized once in main setup()
  Serial.println("GPS Neo-6M with ESP32 Initializing...");

  // Use the defined constants for pins and baud rate from defines.h
  // Note: 'gpsSerial' is the global HardwareSerial object defined in defines.h
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2); // Use defined constants
  Serial.println("GPS Serial Port Initialized.");
}

void loop_gps() {
  // IMPORTANT FIX: Declare and initialize currentMillis within this function
  // if it's to be a standalone loop_gps() function.
  // When integrated into main.cpp's loop(), this line would be removed
  // as currentMillis would be obtained once at the top of the main loop().
  unsigned long currentMillis = millis();

  // This sketch displays information every time a new sentence is correctly encoded.
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Static variable to track the last time GPS data was printed.
  // This allows for periodic printing without blocking the main loop.
  static unsigned long lastPrintTime = 0;
  // Define the interval (in milliseconds) at which GPS data should be printed to the Serial Monitor.
  // Here, data will be printed approximately every 1 second.
  const unsigned long PRINT_INTERVAL_MS = 1000;

  // Check two conditions before printing:
  // 1. Has enough time passed since the last print? (Ensures periodic updates).
  // 2. Has the GPS location data been updated? (Ensures new, valid data is available).
  if (currentMillis - lastPrintTime >= PRINT_INTERVAL_MS && gps.location.isUpdated()) {
    // Print Latitude with 6 decimal places for high precision.
    Serial.print("LAT: ");
    Serial.println(gps.location.lat(), 6);

    // Print Longitude with 6 decimal places for high precision.
    Serial.print("LONG: ");
    Serial.println(gps.location.lng(), 6);

    // Print speed in kilometers per hour (km/h).
    Serial.print("SPEED (km/h) = ");
    Serial.println(gps.speed.kmph());

    // Print altitude in meters.
    Serial.print("ALT (m)= "); // Corrected "min" to "m"
    Serial.println(gps.altitude.meters());

    // Print Horizontal Dilution of Precision (HDOP).
    // HDOP is a measure of the GPS fix quality; lower values indicate better accuracy.
    // The raw value from TinyGPS++ is typically multiplied by 100, so we divide by 100.0.
    Serial.print("HDOP = ");
    Serial.println(gps.hdop.value() / 100.0);

    // Print the number of satellites used to achieve the current GPS fix.
    Serial.print("Satellites = ");
    Serial.println(gps.satellites.value());

    // Print the current time in UTC (Coordinated Universal Time).
    // The time components are converted to String for concatenation.
    Serial.print("Time in UTC: ");
    Serial.println(String(gps.date.year()) + "/" + String(gps.date.month()) + "/" + String(gps.date.day()) + "," + String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second()));
    Serial.println(""); // Add an empty line for better readability between each set of updates

    // Update the last print time to the current time, preparing for the next interval.
    lastPrintTime = currentMillis;
  }

  // This block checks for GPS data activity.
  // It provides a warning if the GPS module isn't sending data or if there's a connection issue.
  static unsigned long lastNoDataWarningTime = 0;
  // Define the interval at which to check for and print the "no data" warning (e.g., every 5 seconds).
  const unsigned long NO_DATA_WARNING_INTERVAL_MS = 5000;

  // After an initial boot-up delay of 5 seconds, and if very few characters have been
  // processed by TinyGPSPlus (indicating no data), and if it's time to print a warning.
  if (currentMillis > NO_DATA_WARNING_INTERVAL_MS && gps.charsProcessed() < 10) {
    if (currentMillis - lastNoDataWarningTime > NO_DATA_WARNING_INTERVAL_MS) {
      Serial.println("Tidak ada data GPS yang terdeteksi! Pastikan modul terhubung dengan benar.");
      lastNoDataWarningTime = currentMillis; // Reset the timer for the next warning check
    }
  }
}
