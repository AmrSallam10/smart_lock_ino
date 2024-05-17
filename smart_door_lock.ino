// ---------------------------------------- CONNECTIONS ----------------------------------------

// KEYPAD
/*
ROWS (UPPER PINS): 4, 3, 2, 1, GND, VCC
PINS:              9, 8, 7, 6, GND, 3.3V

COLS (DOWN PINS) : 4, 3, 2, 1, GND, VCC
PINS:              5, 4, 3, 2, GND, 3.3V
*/

// BUZZER
/*
+ to 41
- to GND
*/

// LED (R(common Anode)GB)
/*
R to 37
Anode to 5V
G to 35
B to 33
*/

// HM-10
/*
VCC to 5V
GND to GND
TX to 19 (Rx1 - interrupt pin)
RX to 18 (Tx1)
*/

#include <Keypad.h>
// #include <Servo.h>
#include "DoorState.h"

/* Locking mechanism definitions */
// #define SERVO_PIN 6
// #define SERVO_LOCK_POS 20
// #define SERVO_UNLOCK_POS 90
// Servo lockServo;

// MEGA Interrupts pins: 2, 3, 18, 19, 20, 21
// UNO Interrupts pins: 2, 3
// NANO Interrupts pins: 2, 3

// ---[ Arduino Pin Manipulation Macros ]---
#define SET_PIN_MODE(port, pin, mode) \
  if (mode == INPUT) { \
    DDR ## port &= ~(1 << pin); \
  } else if (mode == OUTPUT) { \
    DDR ## port |= (1 << pin); \
  }
#define SET_PIN_HIGH(port, pin) (PORT ## port |= (1 << pin))
#define SET_PIN_LOW(port, pin) ((PORT ## port) &= ~(1 << (pin)))
//-----------------------------------------

#define PINR 37
#define PING 35
#define PINB 33
#define PINBUZZ 41
#define SERIAL1_RX_PIN 19
#define MAX_CODE_LENGTH 4
#define MAX_ATTEMPTS 3
#define TIME_OUT 5000 // 5 seconds
#define MAX_UNLOCK_TIME 10000 // 10 seconds
#define LOCK_OUT_TIME 20000 // 20 seconds
#define PASSCODE "1919"
#define AUTHENTICATED_PASSCODE "1234"

const int startupMelody[] = {262, 330, 392, 523};
const int lockUnlockMelody = 2000;  
const int alartMelody = 1000;  
const int startupNoteDuration = 200; 
const int lockUnlockNoteDuration = 200;              
const int alarmNoteDuration = 1000;

volatile String command = "";
volatile bool newCommandReceived = false;
volatile char receivedChar;
enum class AuthenticatedState { UNAUTHENTICATED, LOGIN, AUTHENTICATED };
AuthenticatedState authenticatedState = AuthenticatedState::UNAUTHENTICATED;

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
  delay(1000); 
}

void playLockUnlockTone() {
    tone(PINBUZZ, lockUnlockMelody, lockUnlockNoteDuration);
    delay(lockUnlockNoteDuration);
    noTone(PINBUZZ);
    delay(1000); 
}

void playAlartMelody() {
  for (int i = 0; i < 3; i++) {
    tone(PINBUZZ, alartMelody, alarmNoteDuration);
    delay(alarmNoteDuration);
    noTone(PINBUZZ);
    delay(200); 
  }
  delay(1000); 
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
  while (!newCommandReceived && result.length() < 4) {
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
  unsigned long unlockTime = millis();
  displayUnlockedStateOnLED();
  auto key = keypad.getKey();
  while (key != 'C') {
    if (newCommandReceived)
      return;
    if (millis() - unlockTime > MAX_UNLOCK_TIME)
      break;
    key = keypad.getKey();
  }
  lock();
}

void doorLockedLogic() {
  displayLockedStateOnLED();

  String userCode = inputSecretCode();
  if (!newCommandReceived){
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
  switch (authenticatedState) {
    case AuthenticatedState::UNAUTHENTICATED:
      if (command == "login") {
        sendStringSerial1("Enter passcode");
        authenticatedState = AuthenticatedState::LOGIN;
      } else {
        sendStringSerial1("Access Denied");
      }
      break;

    case AuthenticatedState::LOGIN:
      if (command == AUTHENTICATED_PASSCODE) {
        sendStringSerial1("Authenticated");
        authenticatedState = AuthenticatedState::AUTHENTICATED;
      } else {
        sendStringSerial1("Access Denied");
        authenticatedState = AuthenticatedState::UNAUTHENTICATED;
      }
      break;

    case AuthenticatedState::AUTHENTICATED:
      if (command == "state") {
        sendStringSerial1(doorState.locked() ? "Locked" : "Unlocked");
      } else if (command == "lock") {
        lock();
      } else if (command == "unlock") {
        doorState.unlock(PASSCODE);
        unlock();
      } else if (command.startsWith("code") && command.length() == 8){
        String newCode = command.substring(4, 8);
        doorState.setCode(newCode);
        sendStringSerial1("Code set to: " + newCode);
        Serial.println("Setting new passcode");
      } else if (command == "logout") {
        authenticatedState = AuthenticatedState::UNAUTHENTICATED;
        sendStringSerial1("Logged out");
      } else {
        sendStringSerial1("Invalid command");
      }
      break;
  }
  command = "";
}

ISR(USART1_RX_vect) {
    receivedChar = UDR1;  // Read the incoming byte
    if (receivedChar == '\n') {
      newCommandReceived = true;
    } else {
      command += receivedChar;
    }
}

void sendCharSerial1(char c) {
    // Wait for empty transmit buffer
    while (!(UCSR1A & (1 << UDRE1)));
    // Put data into buffer, sends the data
    UDR1 = c;
}

void sendStringSerial1(const String& str) {
    for (int i = 0; i < str.length(); i++) {
        sendCharSerial1(str[i]);
    }
}

// setup uart for HM-10
void UART1_setup(){
  // Set baud rate
  uint16_t baud_setting = F_CPU / 16 /9600  - 1;
  UBRR1H = baud_setting >> 8;
  UBRR1L = baud_setting;

  // Enable receiver and transmitter
  UCSR1B |= (1 << RXEN1) | (1 << TXEN1);

  // Enable Rx interrupt
  UCSR1B |= (1 << RXCIE1);

  // Set 8-bit character sizes
  UCSR1C |= (1 << UCSZ11) | (1 << UCSZ10);
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
  UART1_setup();

  doorState.setCode(PASSCODE);
  lock();
  playStartupTone();
}

void loop() {
  if (newCommandReceived){
    parseCommand();
    newCommandReceived = false;
  } else {
    if (doorState.locked())
      doorLockedLogic();
    else 
      doorUnlockedLogic();
  }
}
