#include <Arduino.h>
#include <magnetic_read.h>
#include <AHT_read.h>
#include <MQ7_read.h>
#include <MQ135_read.h>

void readMagneticSensor();
// GPS-function
void readGPSData() {
    while (gpsSerial.available() > 0) {
        if (gps.encode(gpsSerial.read())) {
            if (gps.location.isUpdated()) {
                currentLatitude = gps.location.lat();
                currentLongitude = gps.location.lng();
                currentAltitude = gps.altitude.meters();
                currentSpeed = gps.speed.kmph();
                currentSatellites = gps.satellites.value();

                // Get UTC time and date components
                int utcYear = gps.date.year();
                int utcMonth = gps.date.month();
                int utcDay = gps.date.day();
                int utcHour = gps.time.hour();
                int utcMinute = gps.time.minute();
                int utcSecond = gps.time.second();

                // Add 7 hours to convert to UTC+7
                int localHour = utcHour + 7;
                int localDay = utcDay;
                int localMonth = utcMonth;
                int localYear = utcYear;

                // Handle the day and month rollover
                if (localHour >= 24) {
                    localHour -= 24;
                    localDay++;

                    // Handle month rollover (e.g., end of month)
                    if (localDay > 31 && (localMonth == 1 || localMonth == 3 || localMonth == 5 || localMonth == 7 || localMonth == 8 || localMonth == 10 || localMonth == 12)) {
                        localDay = 1;
                        localMonth++;
                    } else if (localDay > 30 && (localMonth == 4 || localMonth == 6 || localMonth == 9 || localMonth == 11)) {
                        localDay = 1;
                        localMonth++;
                    } else if (localDay > 28 && localMonth == 2) {
                        // This handles a simple case. Leap years would require more complex logic.
                        localDay = 1;
                        localMonth++;
                    }

                    // Handle year rollover
                    if (localMonth > 12) {
                        localMonth = 1;
                        localYear++;
                    }
                }

                char timeStr[20];
                sprintf(timeStr, "%04d/%02d/%02d,%02d:%02d:%02d",
                        localYear, localMonth, localDay,
                        localHour, utcMinute, utcSecond);
                currentTimeUTC = String(timeStr);

                // Print updated GPS data to the Serial Monitor.
                Serial.println("GPS Data Updated:");
                Serial.print("   LAT: "); Serial.print(currentLatitude, 6); Serial.println("\t");
                Serial.print("   LONG: "); Serial.println(currentLongitude, 6);
                Serial.print("   ALT (m): "); Serial.print(currentAltitude, 2); Serial.println("\t");
                Serial.print("   SPEED (km/h): "); Serial.println(currentSpeed, 2);
                Serial.print("   Satellites: "); Serial.print(currentSatellites); Serial.println("\t");
                Serial.print("   Time UTC+7: "); Serial.println(currentTimeUTC);
                Serial.println();
            }
        }
    }

    // --- Corrected GPS No Data Warning ---
    static unsigned long lastNoDataWarningTime = 0;
    const unsigned long NO_DATA_WARNING_INTERVAL_MS = 5000; // Recommended: 5 seconds
    unsigned long currentMillis = millis();

    if (gps.charsProcessed() < 10) { // Check if no data is being processed
        if (currentMillis - lastNoDataWarningTime >= NO_DATA_WARNING_INTERVAL_MS) {
            Serial.println("No GPS data detected! Ensure the module is properly connected.");
            lastNoDataWarningTime = currentMillis;
        }
    }
}
void powerPeripherals(bool on) {
  // If using a P-MOSFET high-side with gate pulled up to VCC through 100k:
  //   gate LOW = ON, gate HIGH = OFF
  pinMode(PWR_EN_PIN, OUTPUT);
  digitalWrite(PWR_EN_PIN, on ? LOW : HIGH);
}



// LoRa sending packets function
void sendLoRaPacket() {
    uint32_t t0 = millis();
    while (millis() - t0 < 500) {
    while (gpsSerial.available()) gps.encode(gpsSerial.read());
  }
    packetCounter++;      // Increment packet counter

  // Populate the struct with current data, performing necessary type conversions
  dataToSend.packetCounter = (uint8_t)packetCounter; // Cast int to uint8_t
  dataToSend.latitude = currentLatitude;
  dataToSend.longitude = currentLongitude;
  dataToSend.altitude = currentAltitude;
  dataToSend.speed = currentSpeed;
  dataToSend.satellites = (uint8_t)currentSatellites; // Cast int to uint8_t

  // Convert String (currentTimeUTC) to C-style string (char[]) for strncpy
  strncpy(dataToSend.timeUTC, currentTimeUTC.c_str(), sizeof(dataToSend.timeUTC) - 1);
  dataToSend.timeUTC[sizeof(dataToSend.timeUTC) - 1] = '\0'; // Ensure null termination


    Serial.print("\n----- Transmitting Data Packet # ");
    Serial.print(dataToSend.packetCounter);
    Serial.println ("-----");
    int packetSize = sizeof(dataToSend);
    Serial.print("Packet size: ");
    Serial.print(packetSize);
    Serial.println(" bytes");
    Serial.println();
    Serial.print("Lat: ");
    Serial.print(dataToSend.latitude, 6); Serial.print('\t'); Serial.print("  |  ");
    Serial.print("Long: ");
    Serial.print(dataToSend.longitude, 6);
    Serial.println();
    Serial.print("Alt: ");
    Serial.print(dataToSend.altitude, 2); 
    Serial.print("m"); Serial.print('\t'); Serial.print("  |  ");
    Serial.print("Speed: ");
    Serial.print(dataToSend.speed, 2);
    Serial.println("km/h");
    Serial.print("Satellites: ");
    Serial.print(dataToSend.satellites); Serial.print('\t'); Serial.print("  |  ");
    Serial.print("Time UTC: ");
    Serial.println(dataToSend.timeUTC);
    Serial.println();
    

  // Send the struct as raw bytes
  LoRa.beginPacket();
  LoRa.write((byte*)&dataToSend, sizeof(dataToSend));
  LoRa.endPacket();

}

void sendUBX(const uint8_t *msg, size_t len) {
  gpsSerial.write(msg, len);
  gpsSerial.flush();
}

void gpsPowerOn() {
  if (PIN_GPS_EN >= 0) {
    pinMode(PIN_GPS_EN, OUTPUT);
    digitalWrite(PIN_GPS_EN, LOW);      // turn GPS ON (if using high-side switch)
    delay(50);
  }
}

void gpsPowerOff() {
  if (PIN_GPS_EN >= 0) {
    digitalWrite(PIN_GPS_EN, HIGH);     // turn GPS OFF
  }
}

void gpsExitPSM() {
  sendUBX(UBX_PSM_OFF, sizeof(UBX_PSM_OFF));
}

void gpsEnterPSM() {
  sendUBX(UBX_PSM_ON, sizeof(UBX_PSM_ON));
}

bool waitForFirstFix(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (gpsSerial.available()) gps.encode(gpsSerial.read());
    if (gps.location.isValid() && gps.satellites.value() >= 3) return true;

    // let loop run other tasks
    delay(20);
  }
  return false;
}

void IRAM_ATTR vibISR() {
  vibEdgeFlag = true;
}

void readMagneticSensor() {
    magneticValue = digitalRead(Sensor);

    // Only print the status if it has changed
    if (magneticValue != lastMagneticValue) {
        if (magneticValue == 0) {
            magneticStatus = "Magnet Detected!";
        } else {
            magneticStatus = "No Magnet Detected";
        }

        Serial.print("Magnetic Sensor Status: ");
        Serial.println(magneticStatus);

        lastMagneticValue = magneticValue;
    }
}


void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n Starting Integrated LoRa-GPS-DHT Sensor Node...");

    pinMode(Sensor, INPUT);
    Serial.println(" Magnetic Sensor Initialized.");

    pinMode(PIN_VIB, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_VIB), vibISR, RISING);
    pinMode(GPS_HIGH, OUTPUT);
  

    // // dht setup
    // dht.begin();
    // Serial.println("DHT Sensor Initialized.");

    // gps setup
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
    Serial.println("GPS Serial Port Initialized.");

    // LoRa setup
    
    // LoRa.setPins(ss, rst, dio0);

    // while (!LoRa.begin(923E6)) { //frequency
    //     Serial.println("LoRa initialization failed. Retrying...");
    //     delay(10);
    // }
    // LoRa.setSyncWord(0x34);
    // Serial.println("LoRa Initialized OK!");
    // Serial.println("Ready for monitoring and transmitting data...");

    // gpsEnterPSM();
    // Serial.println("GPS set to PSM (IDLE).");
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial);
  Serial.println("LoRa Sender");

  // Setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);

  // Keep retrying until LoRa radio responds
  while (!LoRa.begin(923E6)) {
    Serial.println(".");
    delay(10);
  }

  // ---- Configure LoRa radio parameters ----
  LoRa.setSpreadingFactor(12);     // SF7..12 (higher = longer range, lower data rate)
  LoRa.setSignalBandwidth(125E3); // 7.8k..500k (narrower = better sensitivity, lower data rate)
  LoRa.setCodingRate4(8);         // 4/5..4/8 (higher = more robust, lower throughput)
  LoRa.setPreambleLength(12);      // default is 8, can increase for noisy links
  LoRa.setTxPower(20);            // dBm (2..20, PA_BOOST on RFM95W)
  LoRa.setSyncWord(0x34);         // must match RX
  LoRa.enableCrc();               // must match RX

  Serial.println("LoRa Initializing OK!");
}


void loop() {
    const uint32_t now = millis();

    // Feed GPS parser continuously (cheap)
    while (gpsSerial.available()) gps.encode(gpsSerial.read());

    // --- Vibration reading + send gating ---
    static const int VIB_THRESHOLD = 4000;            // analog threshold
    int  vibValue = analogRead(PIN_VIB);              // read SW-18010P analog
    bool detected = (vibValue < VIB_THRESHOLD);       // true = vibration detected

    // --- NEW: 5-minute LOW hold for GPS_HIGH after any vibration ---
    static bool     gpsLowHold      = false;          // are we currently holding LOW?
    static uint32_t gpsLowStartMs   = 0;              // when the hold started
    const  uint32_t GPS_LOW_HOLD_MS = 5UL * 1000UL;  // 5 seconds


    // Start/restart the 5-minute LOW hold whenever vibration is detected
    if (detected) {
        if (!gpsLowHold) {
            Serial.println("[GPS_HIGH] Vibration detected → start 5-min LOW hold");
        } else {
            Serial.println("[GPS_HIGH] Vibration detected → restart 5-min LOW hold");
        }
        gpsLowHold    = true;
        gpsLowStartMs = now;
    }

    // Apply the hold or release when time elapsed
    if (gpsLowHold) {
        digitalWrite(GPS_HIGH, LOW);
        if (now - gpsLowStartMs >= GPS_LOW_HOLD_MS) {
            gpsLowHold = false;
            digitalWrite(GPS_HIGH, HIGH);
            Serial.println("[GPS_HIGH] 5-minute window elapsed → set HIGH");
        }
    } else {
        // No active hold → keep HIGH
        digitalWrite(GPS_HIGH, HIGH);
    }

    // --- Original send gating logic (unchanged) ---
    if (detected) {
        lastVibrationMs = now;                        // keep inactivity timer meaningful
        if (now - lastSendMs >= LORA_SEND_INTERVAL) {
            Serial.println("[VIB] Vibration detected -> Sending LoRa packet");
            sendLoRaPacket();
            lastSendMs = now;                         // start cooldown
        } else {
            Serial.println("[VIB] Ignored (still within cooldown)");
        }
    }

    // --- Periodic status print ---
    {
      static uint32_t lastVibPrintMs = 0;             // print once per second
      if (now - lastVibPrintMs >= 1000) {
        lastVibPrintMs = now;

        bool canSend  = (now - lastSendMs >= LORA_SEND_INTERVAL);
        uint32_t cooldownMs  = canSend ? 0 : (LORA_SEND_INTERVAL - (now - lastSendMs));
        uint32_t cooldownSec = (cooldownMs + 999) / 1000;

        uint32_t holdRemainMs  = 0;
        if (gpsLowHold) {
          uint32_t elapsed = now - gpsLowStartMs;
          holdRemainMs = (elapsed >= GPS_LOW_HOLD_MS) ? 0 : (GPS_LOW_HOLD_MS - elapsed);
        }
        uint32_t holdRemainSec = (holdRemainMs + 999) / 1000;

        Serial.print("[VIB-STATUS] value=");
        Serial.print(vibValue);
        Serial.print(" thresh=");
        Serial.print(VIB_THRESHOLD);
        Serial.print(" | detected=");
        Serial.print(detected ? "YES" : "NO");
        Serial.print(" | GPS_HIGH=");
        Serial.print(gpsLowHold ? "LOW (holding)" : "HIGH");
        Serial.print(" | hold_left(s)=");
        Serial.print(gpsLowHold ? holdRemainSec : 0);
        Serial.print(" | can_send_now=");
        Serial.print(canSend ? "YES" : "NO");
        Serial.print(" | next_send_in=");
        Serial.print(canSend ? 0 : cooldownSec);
        Serial.println("s");
      }
    }

    // ACTIVE state behavior (kept minimal, only timer logic used)
    if (state == ACTIVE) {
        // If no vibration for 30 minutes -> back to PSM (IDLE)
        if (now - lastVibrationMs >= INACTIVITY_PSM_MS) {
            Serial.println("[IDLE] No vibration for 30 min. Entering GPS PSM.");
            gpsEnterPSM();
            gpsPowerOff();        // turn off if you use a high-side switch
            state = IDLE;
        }
    }

    // Light idle
    delay(10);

    // gps reading (your existing pretty print)
    readGPSData();
}