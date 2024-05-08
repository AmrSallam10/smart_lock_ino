#include <Arduino.h>
#include <EEPROM.h>
#include "DoorState.h"

/* Safe state */
#define EEPROM_ADDR_LOCKED   0
#define EEPROM_ADDR_CODE_LEN 1
#define EEPROM_ADDR_CODE     2
#define EEPROM_EMPTY         0xff

#define DOOR_STATE_OPEN (char)0
#define DOOR_STATE_LOCKED (char)1

DoorState::DoorState() {
  this->_locked = EEPROM.read(EEPROM_ADDR_LOCKED) == DOOR_STATE_LOCKED;
}

void DoorState::lock() {
  this->setLock(true);
}

bool DoorState::locked() {
  return this->_locked;
}

bool DoorState::hasCode() {
  auto codeLength = EEPROM.read(EEPROM_ADDR_CODE_LEN);
  return codeLength != EEPROM_EMPTY;
}

void DoorState::setCode(String newCode) {
  EEPROM.write(EEPROM_ADDR_CODE_LEN, newCode.length());
  for (byte i = 0; i < newCode.length(); i++) {
    EEPROM.write(EEPROM_ADDR_CODE + i, newCode[i]);
  }
}

bool DoorState::unlock(String code) {
  auto codeLength = EEPROM.read(EEPROM_ADDR_CODE_LEN);
  if (codeLength == EEPROM_EMPTY) {
    // There was no code, so unlock always succeeds
    this->setLock(false);
    return true;
  }
  if (code.length() != codeLength) {
    return false;
  }
  for (byte i = 0; i < code.length(); i++) {
    auto digit = EEPROM.read(EEPROM_ADDR_CODE + i);
    if (digit != code[i]) {
      return false;
    }
  }
  this->setLock(false);
  return true;
}

void DoorState::setLock(bool locked) {
  this->_locked = locked;
  EEPROM.write(EEPROM_ADDR_LOCKED, locked ? DOOR_STATE_LOCKED : DOOR_STATE_OPEN);
}
