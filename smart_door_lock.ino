#include <Keypad.h>
// #include <Servo.h>
#include "DoorState.h"

/* Locking mechanism definitions */
// #define SERVO_PIN 6
// #define SERVO_LOCK_POS 20
// #define SERVO_UNLOCK_POS 90
// Servo lockServo;

const int pinR = 13;
const int pinG = 12;
const int pinB = 11;
const int pinBuzz = 22;
const byte Serial1RxPin = 19;

const int startupMelody[] = {262, 330, 392, 523}; // C4, E4, G4, C5
const int openCloseMelody = 2000;  
const int alartMelody = 1000;  
const int startupNoteDuration = 200; 
const int openCloseNoteDuration = 200;              
const int alarmNoteDuration = 1000;

volatile String command = "";
volatile bool userMode = true;

/* Keypad setup */
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;
byte rowPins[KEYPAD_ROWS] = {2, 3, 4, 5};
byte colPins[KEYPAD_COLS] = {6, 7, 8, 9};
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '0', 'F', 'E', 'D' }
};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS);

/* SafeState stores the secret code in EEPROM */
DoorState doorState;

void showStartupMessage() {
  for (int i = 0; i < 4; i++) {
    tone(pinBuzz, startupMelody[i], startupNoteDuration);
    delay(startupNoteDuration);
    noTone(pinBuzz);
    delay(50); 
  }
}

void playOpenCloseMelody() {
    tone(pinBuzz, openCloseMelody, openCloseNoteDuration);
    delay(openCloseNoteDuration);
    noTone(pinBuzz);
    delay(50);  
}

void playAlartMelody() {
  for (int i = 0; i < 3; i++) {
    tone(pinBuzz, alartMelody, alarmNoteDuration);
    delay(alarmNoteDuration);
    noTone(pinBuzz);
    delay(200); // Pause between notes
  }
  delay(1000); // Pause before repeating the alarm (adjust as needed)
}

void lock() {
  // lockServo.write(SERVO_LOCK_POS);
  doorState.lock();
}

void unlock() {
  // lockServo.write(SERVO_UNLOCK_POS);
}

void showUnlockedStat() {
  digitalWrite(pinR, HIGH);
  digitalWrite(pinG, LOW);
  digitalWrite(pinB, HIGH);
  Serial.println("Unlocked");
}

void showLockedStat() {
  digitalWrite(pinR, HIGH);
  digitalWrite(pinG, HIGH);
  digitalWrite(pinB, LOW);
  Serial.println("Locked");
}

void showAlartMsg() {
  digitalWrite(pinR, LOW);
  digitalWrite(pinG, HIGH);
  digitalWrite(pinB, HIGH);
  Serial.println("Access Denied");
}

String inputSecretCode() {
  String result = "";
  while (userMode && result.length() < 4) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      result += key;
      Serial.println(result);
    }
  }
  return result;
}

void doorUnlockedLogic() {
  showUnlockedStat();
  auto key = keypad.getKey();
  while (key != 'C') {
    if (!userMode)
      return;
    key = keypad.getKey();
  }

  doorState.lock();
  lock();
  playOpenCloseMelody();
}

void doorLockedLogic() {
  showLockedStat();

  String userCode = inputSecretCode();
  if (userMode){
    bool unlockedSuccessfully = doorState.unlock(userCode);
    delay(200);
    if (unlockedSuccessfully) {
      unlock();
      playOpenCloseMelody();
    } else {
      showAlartMsg();
      playAlartMelody();
    }
  }
}

void parseCommand(){
  if (command.startsWith("state")) {
    if (doorState.locked()){
      Serial1.println("Locked");
    } else {
      Serial1.println("Unlocked");
    }
  } else if (command.startsWith("close")) {
    doorState.lock();
    lock();
    playOpenCloseMelody();
  } else if (command.startsWith("open")) {
    doorState.unlock("1919");
    unlock();
    playOpenCloseMelody();
  }
  command = "";
}

void readFromSerial1(){
  if (Serial1.available()) {  // Check if data is available from HM-10
    char receivedChar = Serial1.read();  // Read the incoming byte
    command += receivedChar;
    Serial.print(receivedChar);

    if (command.startsWith("state") || command.startsWith("close") || command.startsWith("open")) {
      userMode = false;
    } else if (receivedChar == '\n') {
        command = ""; 
    }
  }
}

void setup() {

  pinMode(pinR, OUTPUT);
  pinMode(pinG, OUTPUT);
  pinMode(pinB, OUTPUT);
  pinMode(pinBuzz, OUTPUT);
  pinMode(Serial1RxPin, INPUT_PULLUP); 

  digitalWrite(pinR, HIGH);
  digitalWrite(pinG, HIGH);
  digitalWrite(pinB, HIGH);

  Serial.begin(9600); // Initialize Serial Monitor
  Serial1.begin(9600);  // Initialize Serial communication with HM-10
  attachInterrupt(digitalPinToInterrupt(Serial1RxPin), readFromSerial1, FALLING); // Interrupt for Rx1 (from HM-10)

  String code = "1919";
  doorState.setCode(code);
  lock();
  showStartupMessage();
}

void loop() {
  if (userMode){
    if (doorState.locked()) {
      doorLockedLogic();
    } else {
      doorUnlockedLogic();
    }
  } else {
    parseCommand();
    userMode = true;
  }
}
