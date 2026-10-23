#include <Arduino.h>
#define Sensor 35

 
void setup_magnetic() {
  Serial.begin(9600);
  pinMode(Sensor, INPUT);
  
}
 
void loop_magnetic() {
  bool value = digitalRead(Sensor);
  Serial.println(value);
 
  if (value == 0) {
    Serial.println("no magnetic field detected");
  } else {
    Serial.println("magnetic field detected");
  } delay(10); 
}