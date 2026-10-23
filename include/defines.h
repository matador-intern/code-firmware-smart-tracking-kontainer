#include <SPI.h>       // For LoRa module communication
#include <LoRa.h>      // LoRa library
#include <TinyGPSPlus.h> // GPS parsing library
#include "DHT.h"       // DHT sensor library
#include "MQ7.h"      // MQ7 gas sensor library
#include <MQUnifiedsensor.h>   // MQ135 gas sensor library
#include <esp_sleep.h>
// --- LoRa Module Definitions ---
#define ss 5    // SS (Slave Select) pin for LoRa module
#define rst 12   // RST (Reset) pin for LoRa module
#define dio0 25   // DIO0 pin for LoRa module (Interrupt pin)

static const int PIN_VIB     = 34;  
static const int PIN_GPS_EN  = -1;
#define PWR_EN_PIN      27            // drives load switch / P-MOSFET gate


// --- GPS Module Definitions ---
#define RXD2 16    // RX pin for GPS module (connected to TX of GPS)
#define TXD2 17    // TX pin for GPS module (connected to RX of GPS)
#define GPS_BAUD 9600 // Baud rate for GPS communication
#define GPS_HIGH 21
// Note: 'gpsSerial' is defined as a HardwareSerial object below,
// not as a simple #define.

// --- DHT22 Sensor Definitions ---
#define DHTPIN 13     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22 // Type of DHT sensor (DHT11, DHT22, DHT21)

//Pin Sensor Gas
//--MQ7
#define MQ7_PIN 34 // sesuaikan lg sma gpio
MQ7 mq7(MQ7_PIN);
//--MQ135    
// (ada di include/MQ135_read.h)      
//MQUnifiedsensor MQ135("ESP-32", 5, 12, 16, "MQ-135"); note: yang  ke-4 dari kiri

// --- Global Objects ---
TinyGPSPlus gps; // GPS object to parse NMEA sentences
HardwareSerial gpsSerial(2); // Use Serial2 for GPS communication on ESP32 (pins 16 and 17)
enum State { IDLE, ACTIVE };
State state = IDLE;

volatile bool vibEdgeFlag = false;
volatile uint32_t vibIsrStamp = 0;

uint32_t lastVibrationMs = 0;   // updated on valid vibration
uint32_t lastSendMs      = 0;   // last LoRa send
DHT dht(DHTPIN, DHTTYPE); // DHT sensor object, initialized with pin and type

// --- Global Variables for Sensor Data and Timing ---
int packetCounter = 0; // Counter for LoRa packets sent

// Magnetic sensor variables
#define magnetic 33
bool magneticValue;
int doorState = 0; // 
const char* magneticStatus;
int lastMagneticValue = -1; // initialize invalid value

// Timers for non-blocking operations using millis()
unsigned long lastDhtReadMillis = 0;
const long DHT_READ_INTERVAL = 2000; // Read DHT every 2 seconds (2000 ms)

unsigned long lastLoRaSendMillis = 0;
const long LORA_SEND_INTERVAL = 5000UL; // Send LoRa packet every 10 seconds (10000 ms)


// Variables to store current sensor readings
double currentLatitude = 0.0;
double currentLongitude = 0.0;
float currentAltitude = 0.0; // In meters---
float currentSpeed = 0.0;    // In km/h
int currentSatellites = 0;
String currentTimeUTC = "N/A"; // Store GPS time as a string

const uint32_t INACTIVITY_PSM_MS     = 30UL * 60UL * 1000UL;  // 2 minutes without vibration -> back to PSM
const uint32_t GPS_FIX_TIMEOUT_MS    = 90UL * 1000UL;         // wait up to 90 s for first fix
const uint32_t VIB_DEBOUNCE_MS       = 120UL;                 // debounce for SW-420 edges
const uint8_t UBX_PSM_ON[]  = {0xB5,0x62,0x06,0x11,0x02,0x00,0x08,0x01,0x22,0x92};
const uint8_t UBX_PSM_OFF[] = {0xB5,0x62,0x06,0x11,0x02,0x00,0x08,0x00,0x21,0x91};

//gas yang dibaca
float coPPM = 0;
float aqiCO2 = 0;
float alcoholPPM = 0;
float toluenPPM = 0;
float nh4PPM = 0;
float acetonPPM = 0;

// Define a structure to hold your data
// #pragma pack(push,1)
// struct LoRaData {
//   uint32_t deviceID;
//   uint8_t  packetCounter;
//   double   latitude;
//   double   longitude;
//   float    altitude;
//   float    speed;
//   uint8_t  satellites;
//   char     timeUTC[20];  // enough for "YYYY/MM/DD,HH:MM:SS" + '\0'
//   bool     doorValue;
  

// };
// #pragma pack(pop)

// // Create an instance of the struct that will be populated with data to send
// LoRaData dataToSend;