/*
  Simplex Repeater on Arduino Mega using cheap 120s recording and playback module
  Please compile and run under PlatformIO IDE
  Licese: MIT
  Author: Peter Javorsky, MSc.
  E-mail: tekk.sk (at) gmail.com
  Video: https://youtu.be/nC-FqFgFxdM
  Github: https://github.com/tekk/simplex-repeater-arduino
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// Connections for the Nokia 5110 display (you can choose any other pins)
// 1 - Serial clock out (SCLK)
// 2 - Serial data out (DIN)
// 3 - Data/Command select (D/C)
// 4 - LCD chip select (CS/CE)
// 5 - LCD reset (RST)
Adafruit_PCD8544 display = Adafruit_PCD8544(13, 11, 12, 10, 9);

// constants
const unsigned char CONTRAST = 50;
// pins definitions
const unsigned char PIN_BACKLIGHT = 8;
const unsigned char PTT_PIN = 7;
const unsigned char RX_INPUT_PIN = A0;
const unsigned char TX_SIGNALLING_LED = 17;
const unsigned char RX_SIGNALLING_LED = 18;
const unsigned char MODULE_RECORD_PIN = 41;
const unsigned char MODULE_PLAY_PIN = 49;
// display size
const unsigned char DISPLAY_WIDTH = 84;
const unsigned char DISPLAY_HEIGHT = 48;
// in milliseconds
const unsigned long RX_GAP_LENGTH = 1000L;
const unsigned long END_TAIL_ELIMINATION_TIME = 500L;
const unsigned long MAX_TRANSMIT_TIME = 120L * 1000L;
const unsigned long MINIMUM_TRANSMIT_LENGTH = 1000L;
const unsigned long MAX_RECORD_TIME = 120L * 1000L;

// global variables
bool recording = false;
unsigned long recordedMillis = 0;
bool playing = false;
unsigned long playedMillis = 0;
unsigned long rxStartTime = 0;
unsigned long rxEndTime = 0;
bool isRXing = false;
bool maxTimeReached = false;

unsigned char centerH(unsigned char width) {
  return (DISPLAY_WIDTH - width) / 2;
}

unsigned char centerV(unsigned char height) {
  return (DISPLAY_HEIGHT - height) / 2;
}

bool isTx() {
  return digitalRead(PTT_PIN);
}

bool isRx() {
  return !digitalRead(RX_INPUT_PIN);
}

bool isRecording() {
  return !digitalRead(MODULE_RECORD_PIN);
}

bool isPlaying() {
  return playing;
}

void record(bool value) {
  digitalWrite(MODULE_RECORD_PIN, !value);
  recording = value;
}

void playOnce() {
  digitalWrite(MODULE_PLAY_PIN, LOW);
  delay(50);
  digitalWrite(MODULE_PLAY_PIN, HIGH);
  playing = true;
}

void ptt(bool on) {
  digitalWrite(PTT_PIN, on);
}

void setup()   {
  pinMode(RX_INPUT_PIN, INPUT);
  pinMode(PTT_PIN, OUTPUT);
  pinMode(TX_SIGNALLING_LED, OUTPUT);
  pinMode(RX_SIGNALLING_LED, OUTPUT);
  pinMode(MODULE_RECORD_PIN, OUTPUT);
  pinMode(MODULE_PLAY_PIN, OUTPUT);

  digitalWrite(MODULE_RECORD_PIN, HIGH);
  digitalWrite(MODULE_PLAY_PIN, HIGH);

  Serial.begin(115200);

  display.begin();
  display.setContrast(CONTRAST);
  display.clearDisplay();
  display.setRotation(1);
  display.setTextSize(1);
  display.setTextColor(BLACK);

  for (int i = 0; i < 10; i++) {
    display.setCursor(0, i * 8);
    display.println("INTEGRAC");
    display.display(); // show splashscreen
    delay(10);
  }

  for (int i = 255; i > 0; i--) {
    analogWrite(PIN_BACKLIGHT, i);
    delay(5);
  }

  digitalWrite(PIN_BACKLIGHT, LOW);
}

void updateUI() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.print("RX:");
  display.println(isRx());
  display.print("TX:");
  display.println(isTx());
  display.print("RE:");
  display.println(recording);
  display.print("PL:");
  display.println(playing);

  if (recording) {
    display.print(recordedMillis / 1000);
    display.println("s");
  }

  if (playing) {
    display.print(playedMillis / 1000);
    display.println("s");
  }

  display.display();
}

void transmitRecording() {

  recordedMillis -= RX_GAP_LENGTH + END_TAIL_ELIMINATION_TIME;

  record(false);
  recording = false;

  delay(500);

  // overflow protection
  if (recordedMillis > MAX_TRANSMIT_TIME + RX_GAP_LENGTH + END_TAIL_ELIMINATION_TIME) {
    playing = false;
    playedMillis = 0;
    recordedMillis = 0;
    maxTimeReached = false;
    return;
  }

  if (recordedMillis > 0 && recordedMillis > MINIMUM_TRANSMIT_LENGTH) {
    ptt(true);
    playing = true;
    playOnce();

    unsigned long start = millis();

    while ((millis() - start <= recordedMillis) && (millis() - start < MAX_TRANSMIT_TIME)) {
      playedMillis = millis() - start;
      updateUI();
      delay(200);
    }

    ptt(false);
  }

  playing = false;
  playedMillis = 0;
  recordedMillis = 0;
  maxTimeReached = false;

  delay(1000);
}

void loop() {

  if (isRx()) {
    if (!isRXing && !recording && !maxTimeReached) {
      rxStartTime = millis();
      isRXing = true;
      record(true);
      recording = true;
      maxTimeReached = false;
    }

    if (!maxTimeReached) recordedMillis = millis() - rxStartTime;

    if (recordedMillis > MAX_RECORD_TIME) {
      maxTimeReached = true;
      record(false);
      recording = false;
      rxEndTime = millis();
      isRXing = false;
      recordedMillis = MAX_RECORD_TIME;
    }
  } else {
    if (isRXing) {
        rxEndTime = millis();
        isRXing = false;
    }

    if (recording || maxTimeReached) {
      if (recordedMillis > 0) {
        if (millis() - rxEndTime > RX_GAP_LENGTH) {
          record(false);
          recording = false;
          maxTimeReached = false;
          transmitRecording();
        }
        if (!maxTimeReached) {
          if (recording) {
            recordedMillis = millis() - rxStartTime;
          } else {
            recordedMillis = 0;
          }
        }
      }
    }
  }

  updateUI();
}
