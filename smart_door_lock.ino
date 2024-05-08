#include <Keypad.h>
// #include <Servo.h>
#include "DoorState.h"

/* Locking mechanism definitions */
// #define SERVO_PIN 6
// #define SERVO_LOCK_POS 20
// #define SERVO_UNLOCK_POS 90
// Servo lockServo;

#define PINR 13
#define PING 12
#define PINB 11
#define PINBUZZ 22
#define SERIAL1_RX_PIN 19
#define MAX_CODE_LENGTH 4
#define MAX_ATTEMPTS 3
#define TIME_OUT 5000 // 5 seconds
#define LOCK_OUT_TIME 20000 // 20 seconds
#define PASSCODE "1919"

const int startupMelody[] = {262, 330, 392, 523};
const int openCloseMelody = 2000;  
const int alartMelody = 1000;  
const int startupNoteDuration = 200; 
const int openCloseNoteDuration = 200;              
const int alarmNoteDuration = 1000;

volatile String command = "";
volatile bool userMode = true;

int attempts = 0;

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

DoorState doorState;

void playStartupTone() {
  Serial.println("Starting up...");
  for (int i = 0; i < MAX_CODE_LENGTH; i++) {
    tone(PINBUZZ, startupMelody[i], startupNoteDuration);
    delay(startupNoteDuration);
    noTone(PINBUZZ);
    delay(50); 
  }
}

void playLockUnlockTone() {
    tone(PINBUZZ, openCloseMelody, openCloseNoteDuration);
    delay(openCloseNoteDuration);
    noTone(PINBUZZ);
    delay(50);  
}

void playAlartMelody() {
  for (int i = 0; i < 3; i++) {
    tone(PINBUZZ, alartMelody, alarmNoteDuration);
    delay(alarmNoteDuration);
    noTone(PINBUZZ);
    delay(200); // Pause between notes
  }
  delay(1000); // Pause before repeating the alarm (adjust as needed)
}

void lock() {
  // lockServo.write(SERVO_LOCK_POS);
  doorState.lock();
  playLockUnlockTone();
}

void unlock() {
  // lockServo.write(SERVO_UNLOCK_POS);
  playLockUnlockTone();
}

void alart(){
  displayAlartStateOnLED();
  playAlartMelody();
}

void displayUnlockedStateOnLED() {
  digitalWrite(PINR, HIGH);
  digitalWrite(PING, LOW);
  digitalWrite(PINB, HIGH);
  Serial.println("Unlocked");
}

void displayLockedStateOnLED() {
  digitalWrite(PINR, HIGH);
  digitalWrite(PING, HIGH);
  digitalWrite(PINB, LOW);
  Serial.println("Locked");
}

void displayAlartStateOnLED() {
  digitalWrite(PINR, LOW);
  digitalWrite(PING, HIGH);
  digitalWrite(PINB, HIGH);
  Serial.println("Access Denied");
}

String inputSecretCode() {
  String result = "";
  unsigned long lastKeyPressTime = 0;
  while (userMode && result.length() < 4) {
    char key = keypad.getKey();
    if (key != NO_KEY) {
      if (result.length() == 0) {
        lastKeyPressTime = millis();
      }
      result += key;
      Serial.println(result);
      lastKeyPressTime = millis();
    }

    if (lastKeyPressTime != 0 && millis() - lastKeyPressTime > TIME_OUT) {
      Serial.println("Time out");
      return "";
    }
  }
  return result;
}

void doorUnlockedLogic() {
  displayUnlockedStateOnLED();
  auto key = keypad.getKey();
  while (key != 'C') {
    if (!userMode)
      return;
    key = keypad.getKey();
  }
  lock();
}

void doorLockedLogic() {
  displayLockedStateOnLED();

  String userCode = inputSecretCode();
  if (userMode){
    bool unlockedSuccessfully = doorState.unlock(userCode);
    delay(200);
    if (unlockedSuccessfully) {
      unlock();
    } else {
      alart();
      attempts++;
      Serial.println("Attempts: " + String(attempts));
      if (attempts >= MAX_ATTEMPTS) {
        Serial.println("Locking out for 20 seconds");
        delay(LOCK_OUT_TIME);
        attempts = 0;
        Serial.println("Locking out time is over");
      }
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
  } else if (command.startsWith("lock")) {
    lock();
  } else if (command.startsWith("unlock")) {
    doorState.unlock(PASSCODE);
    unlock();
  } else if (command.startsWith("code")) {
    String newCode = command.substring(4, 8);
    doorState.setCode(newCode);
    Serial1.println("Code set to: " + newCode);
    Serial.println("Setting new passcode");
  }
  command = "";
}

void readFromSerial1(){
  if (Serial1.available()) {  // Check if data is available from HM-10
    char receivedChar = Serial1.read();  // Read the incoming byte
    command += receivedChar;
    Serial.print(receivedChar);

    if (command.startsWith("state") || command.startsWith("lock") || command.startsWith("unlock") || 
                                            (command.startsWith("code") && command.length() == 8)) {
      Serial.print('\n');
      userMode = false;
    } else if (receivedChar == '\n') {
        command = ""; 
    }
  }
}

void timerIsr() {
  userMode = true;
}

void setup() {

  pinMode(PINR, OUTPUT);
  pinMode(PING, OUTPUT);
  pinMode(PINB, OUTPUT);
  pinMode(PINBUZZ, OUTPUT);
  pinMode(SERIAL1_RX_PIN, INPUT_PULLUP); 

  digitalWrite(PINR, HIGH);
  digitalWrite(PING, HIGH);
  digitalWrite(PINB, HIGH);

  Serial.begin(9600); // Initialize Serial Monitor
  Serial1.begin(9600);  // Initialize Serial communication with HM-10
  attachInterrupt(digitalPinToInterrupt(SERIAL1_RX_PIN), readFromSerial1, FALLING); // Interrupt for Rx1 (from HM-10)

  doorState.setCode(PASSCODE);
  lock();
  playStartupTone();
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
