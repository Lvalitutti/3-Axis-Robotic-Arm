#include <SoftwareSerial.h>

SoftwareSerial armSerial(8,7); // RX, TX

const int suctionPin = 4;
const int releasePin = 3;
const int signalPin = 2;
void setup() {

  armSerial.begin(9600);

  pinMode(signalPin, INPUT);

  pinMode(suctionPin, OUTPUT);
  pinMode(releasePin, OUTPUT);

  initialize();
}

void initialize() {

  digitalWrite(suctionPin, LOW);
  digitalWrite(releasePin, LOW);
}

void inhale() {

  // hold vacuum continuously
  digitalWrite(suctionPin, HIGH);
  digitalWrite(releasePin, LOW);
}

void exhale() {

  digitalWrite(suctionPin, LOW);

  digitalWrite(releasePin, HIGH);
  delay(1000);          // short air release
  digitalWrite(releasePin, LOW);
}

void loop() {

  if (digitalRead(signalPin) == HIGH) {
    inhale();
  } 
  else {
    exhale();
  }

}