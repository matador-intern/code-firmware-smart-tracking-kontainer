#include <SPI.h>
#include <LoRa.h>

// --- LoRa Module Definitions ---
#define ss 13
#define rst 12
#define dio0 4

int counter = 0;

void setup_lora() {
  // // Initialize Serial Monitor
  // Serial.begin(115200);
  // while (!Serial);
  // Serial.println("LoRa Sender");

  // // Setup LoRa transceiver module
  // LoRa.setPins(ss, rst, dio0);

  // // Keep retrying until LoRa radio responds
  // while (!LoRa.begin(923E6)) {
  //   Serial.println(".");
  //   delay(10);
  // }

  // // ---- Configure LoRa radio parameters ----
  // LoRa.setSpreadingFactor(12);     // SF7..12 (higher = longer range, lower data rate)
  // LoRa.setSignalBandwidth(125E3); // 7.8k..500k (narrower = better sensitivity, lower data rate)
  // LoRa.setCodingRate4(8);         // 4/5..4/8 (higher = more robust, lower throughput)
  // LoRa.setPreambleLength(12);      // default is 8, can increase for noisy links
  // LoRa.setTxPower(18);            // dBm (2..20, PA_BOOST on RFM95W)
  // // LoRa.setSyncWord(0x34);         // must match RX
  // LoRa.enableCrc();               // must match RX

  // Serial.println("LoRa Initializing OK!");
}


void loop_lora() {
  Serial.print("Sending packet: ");
  Serial.println(counter);

  //Send LoRa packet to receiver
  LoRa.beginPacket();
  LoRa.print("hello Electronicsinnovation follower");
  LoRa.print(counter);
  LoRa.endPacket();

  counter++;

  delay(10);
}
