# Research Container - LoRa Telemetry System

## Overview
Dual ESP32 system: **Transmitter (Bare Metal ESP-IDF)** + **Receiver (Arduino Framework)**.
Monitoring container: GPS, Door (magnetic), Temp/Hum (AHT20), Gas (MQ7/MQ135), Vibration.
Data sent via LoRa 923MHz → Receiver forwards to InfluxDB (Cloud + Local).

---

## Folder Structure

```
research-container-main/           # TRANSMITTER (Bare Metal ESP-IDF)
├── platformio.ini                 # framework = espidf
├── src/main.c                     # Bare metal TX code (no Arduino libs)
└── include/config_nvs.h           # Shared struct definition

research-container-main-receiver/  # RECEIVER (Arduino Framework)
├── platformio.ini                 # framework = arduino
├── src/main.cpp                   # Arduino RX + InfluxDB HTTP
└── include/config_nvs.h           # Same struct definition
```

---

## Critical Pin Mapping (MUST MATCH HARDWARE)

| Signal | Transmitter (Bare Metal) | Receiver (Arduino) | Notes |
|--------|--------------------------|-------------------|-------|
| LoRa SS   | GPIO 5  | GPIO 5  | ✅ Same |
| LoRa RST  | GPIO 12 | **GPIO 12** (was 13, fixed) | ⚠️ **MUST BE 12** |
| LoRa DIO0 | GPIO 25 | GPIO 25 | ✅ Same |
| LoRa SCK  | GPIO 18 | Default (18) | ✅ Same |
| LoRa MISO | GPIO 19 | Default (19) | ✅ Same |
| LoRa MOSI | GPIO 23 | Default (23) | ✅ Same |
| GPS RX    | GPIO 16 | - | TX only |
| GPS TX    | GPIO 17 | - | TX only |
| GPS EN    | GPIO 21 | - | TX only (Active LOW) |
| Magnetic  | GPIO 33 | - | TX only (Pull-up) |
| Vibration | GPIO 34 (ADC1_CH6) | - | TX only |
| MQ7       | GPIO 35 (ADC1_CH7) | - | TX only |
| MQ135     | GPIO 32 (ADC1_CH4) | - | TX only |
| AHT20 SDA | GPIO 21 | - | TX only |
| AHT20 SCL | GPIO 22 | - | TX only |

---

## LoRa Parameters (Identical Both Sides)

```c
Frequency:     923 MHz
Spreading Factor: 12
Bandwidth:     125 kHz
Coding Rate:   4/8
Sync Word:     0x34
CRC:           Enabled
Preamble:      12
TX Power:      +20 dBm (PA_BOOST)
```

---

## Data Packet Structure (80 bytes, packed)

```c
#pragma pack(1)
struct TelemetryPacket {
    uint8_t  packetCounter;      // 1
    uint32_t deviceID;           // 4 (from MAC)
    double   latitude;           // 8
    double   longitude;          // 8
    double   altitude;           // 8
    double   speed;              // 8
    uint8_t  satellites;         // 1
    char     timeUTC[20];        // 20 "YYYY/MM/DD,HH:MM:SS"
    bool     doorValue;          // 1 (true=Closed)
    char     tujuan[14];         // 14
    char     perusahaan[20];     // 20
    char     isi[16];            // 16
}; // Total = 80 bytes
```

---

## Build & Upload Procedures

### Transmitter (Bare Metal ESP-IDF)
**Folder:** `research-container-main`
```ini
; platformio.ini
framework = espidf
```
1. Open folder in VSCode
2. **Close all Serial Monitors** (🔌 Plug icon)
3. Click **✓ Build** (Ctrl+Alt+B) - first build takes 2-5 min
4. Click **→ Upload** (auto-detects COM port)
5. Monitor: **🔌 Plug** → 115200 baud

### Receiver (Arduino)
**Folder:** `research-container-main-receiver/research-container-main`
```ini
; platformio.ini
framework = arduino
lib_deps = sandeepmistry/LoRa, WiFi, HTTPClient, ArduinoJson, etc.
```
1. Open folder in VSCode
2. **Close all Serial Monitors**
3. Click **✓ Build**
4. Click **→ Upload**
5. Monitor: **🔌 Plug** → 115200 baud

---

## Switching Between TX/RX on Same Machine

**DO NOT** keep both folders open simultaneously in VSCode (different frameworks confuse IntelliSense).

**Workflow:**
1. Work on TX → Build/Upload → Test → Close VSCode
2. Work on RX → Build/Upload → Test → Close VSCode
3. **Always unplug one board before plugging the other** (COM port conflict)

---

## Current Debugging Status (2026-10-21)

### Transmitter Issues
- [x] Build passes (ESP-IDF v6.0.1)
- [x] LoRa init OK (RFM95 Version 0x12 detected)
- [ ] **LoRa TX Timeout** - DIO0 (GPIO25) wiring suspect or Antenna missing
- [ ] **GPS Lat/Lon = 0.000000** - Needs outdoor fix (30-120s cold start)
- [ ] Vibration trigger threshold: `adc < 2000` (test by shaking board)

### Receiver Issues
- [x] Build passes (Arduino)
- [x] Fixed `LORA_RST` from 13 → 12 (matches hardware)
- [ ] Waiting for TX to send valid packets

### Hardware Checks Required
- [ ] **Antenna on BOTH LoRa modules** (critical - PA damage risk without)
- [ ] **DIO0 jumper: GPIO25 ↔ RFM95 DIO0** on BOTH boards
- [ ] RST jumper: GPIO12 ↔ RFM95 RST on BOTH boards
- [ ] GPS antenna outdoor / near window
- [ ] Common GND for all sensors + LoRa + ESP32
- [ ] Power: USB 2A or LiPo (ESP32+LoRa+GPS > 500mA peak)

---

## Quick Test Procedure

1. **Power RX first** → Monitor: `LoRa Initialized OK!` + `WiFi OK` + IP
2. **Power TX** → Monitor: `System Ready. DeviceID: xxxxx`
3. **Shake TX board** (vibration sensor) → TX Log: `TX #1 | LoRa TX OK`
4. **RX Monitor** should show: `✅ Packet detected | Bytes=80 | RSSI=-xx dBm`
5. **Parse output** shows Lat/Lon, Door, Tujuan, Perusahaan, Isi

---

## Key Files to Modify

| Task | File |
|------|------|
| Change LoRa params | TX: `src/main.c` `lora_init_hw()` / RX: `src/main.cpp` `setup()` |
| Change WiFi/Influx | RX: `src/main.cpp` top constants |
| Change Pin Mapping | TX: `src/main.c` `#define` section / RX: `src/main.cpp` `#define` section |
| Struct changes | BOTH: `include/config_nvs.h` (must stay identical) |
| NVS Config (Serial JSON) | TX: `src/main.c` `handle_serial_config()` (minimal) |

---

## Serial Config Format (TX)
Send via Serial Monitor (115200) to set `tujuan`, `perusahaan`, `isi`:
```json
{"tujuan":"JAKARTA","perusahaan":"PT TEST","isi":"ELEKTRONIK"}
```
Saved to NVS flash, survives reboot.

---

## Common Errors & Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| `TX Timeout` | DIO0 not connected / No antenna | Check GPIO25 wiring, attach antenna |
| `Lat: 0.000` | No GPS fix | Move outdoors, check GPS TX/RX wiring |
| `Unexpected packet size` | Struct size mismatch | Verify `#pragma pack(1)` both sides, `sizeof()==80` |
| `LoRa init failed` | SPI wiring / Wrong RST pin | Check SS/RST/DIO0/SCK/MISO/MOSI, RST=12 |
| `COM port busy` | Serial Monitor open | Click 🔌 Plug to disconnect before upload |
| `IDF_VER redefined` | Build flags conflict | Remove custom `build_flags` from platformio.ini |

---

## Next Steps Priority

1. **Fix Hardware**: Attach LoRa antennas, verify DIO0 (GPIO25) & RST (GPIO12) jumpers on both boards
2. **Test TX Loopback**: Add dummy send every 5s in `main.c` loop to isolate radio link
3. **Get GPS Fix**: Take TX outdoors, wait for `Sats: >3`
4. **Verify RX Reception**: Confirm `Bytes=80` packets arriving
5. **InfluxDB**: Verify cloud/local tokens & URLs in RX `main.cpp`

---

## Useful Commands

```bash
# Clean build cache
Remove-Item -Recurse -Force .pio

# List COM ports (PowerShell)
Get-CimInstance Win32_SerialPort | ft Name, Description

# Manual upload (if auto-detect fails)
pio run -t upload --upload-port COM12

# Git init (needed for ESP-IDF version detection)
git init
```

---

## Notes for AI Resume
- Transmitter = **Bare Metal C (ESP-IDF)** in `research-container-main`
- Receiver = **Arduino C++** in `research-container-main-receiver`
- **RST pin = 12** on both (hardware fixed)
- Struct `TelemetryPacket` must remain identical (80 bytes packed)
- GPS needs outdoor, LoRa needs antennas
- Vibration trigger = ADC1_CH6 (GPIO34) < 2000