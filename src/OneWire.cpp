#include <async/OneWire.h>

using namespace async;

OneWire::OneWire(uint8_t pin, uint8_t timeoutMultiplier):
  pin(pin),
  timeoutMultiplier(timeoutMultiplier),
  state(AUTO_DETECT),
  isMasterMode(false),
  lastOpSuccess(false),
  inRequestCallback(false),
  pollInterval(5000),
  nextPollTime(0),
  modeCallback(nullptr),
  busScanCallback(nullptr),
  txLength(0),
  txPosition(0),
  txCallback(nullptr),
  rxLength(0),
  rxPosition(0),
  rxCallback(nullptr),
  requestCallback(nullptr),
  resetCallback(nullptr),
  slaveResponseLength(0),
  slaveResponsePos(0),
  lastActivityTime(0),
  devicesFound(0),
  scanStep(0) {
    setBusHigh();
  }

  void OneWire::begin(bool isMaster) {
    setMaster(isMaster);
    setBusLow();
    state = IDLE;
    lastActivityTime = millis();
  }


void OneWire::begin(uint16_t pollInterval, ModeDetectedCallback callback) {
  setBusLow();
  this -> pollInterval = pollInterval;
  if (pollInterval > 0) {
    generateRandomPollTime();
  }
  modeCallback = callback;
  lastActivityTime = millis();

  if (state == AUTO_DETECT) {
    if (pollInterval > 0) {
      state = AUTO_DETECT;
    } else {
      state = isMasterMode ? IDLE : SLAVE_LISTENING;
    }
  }
}

void OneWire::setMaster(bool masterMode) {
  isMasterMode = masterMode;

  if (state != AUTO_DETECT) {
    state = masterMode ? IDLE : SLAVE_LISTENING;
  }
}

void OneWire::setBusLow() {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void OneWire::setBusHigh() {
  pinMode(pin, INPUT_PULLUP);
}

bool OneWire::readBus() {
  return digitalRead(pin);
}

void OneWire::startReset() {
  setBusLow();
  operationStartTime = micros();
  state = RESET_START;
}

void OneWire::startWriteBit(uint8_t bit) {
  setBusLow();
  operationStartTime = micros();
  currentByte = bit;
  state = WRITE_BIT_START;
}

void OneWire::startReadBit() {
  setBusLow();
  operationStartTime = micros();
  state = READ_BIT_START;
}

void OneWire::processState() {
  uint32_t currentTime = micros();
  uint32_t elapsed = currentTime - operationStartTime;

  switch (state) {
  case RESET_START:
    if (elapsed >= 480 * timeoutMultiplier) {
      setBusHigh();
      operationStartTime = currentTime;
      state = RESET_WAIT;
    }
    break;

  case RESET_WAIT:
    if (elapsed >= 70 * timeoutMultiplier) {
      lastOpSuccess = !readBus();
      state = RESET_DETECT_PRESENCE;
      operationStartTime = currentTime;
    }
    break;

  case RESET_DETECT_PRESENCE:
    if (elapsed >= 410 * timeoutMultiplier) {
      if (lastOpSuccess && txLength > 0) {
        startNextTxByte();
      } else {
        completeOperation(lastOpSuccess);
      }
    }
    break;

  case WRITE_BIT_START:
    if (elapsed >= 1) {
      if (currentByte) {
        setBusHigh();
      }
      state = WRITE_BIT_LOW;
      operationStartTime = currentTime;
    }
    break;

  case WRITE_BIT_LOW:
    if (elapsed >= (currentByte ? 15 : 60) * timeoutMultiplier) {
      setBusHigh();
      state = WRITE_BIT_HIGH;
      operationStartTime = currentTime;
    }
    break;

  case WRITE_BIT_HIGH:
    if (elapsed >= 60 * timeoutMultiplier) {
      bitMask <<= 1;
      if (bitMask == 0) {
        startNextTxByte();
      } else {
        startWriteBit(currentByte & bitMask);
      }
    }
    break;

  case READ_BIT_START:
    if (elapsed >= 1 * timeoutMultiplier) {
      setBusHigh();
      state = READ_BIT_SAMPLE;
      operationStartTime = currentTime;
    }
    break;

  case READ_BIT_SAMPLE:
    if (elapsed >= 15 * timeoutMultiplier) {
      readResult = readBus();
      state = READ_BIT_COMPLETE;
      operationStartTime = currentTime;
    }
    break;

  case READ_BIT_COMPLETE:
    if (elapsed >= 45 * timeoutMultiplier) {
      currentByte >>= 1;
      if (readResult) {
        currentByte |= 0x80;
      }

      bitMask >>= 1;
      if (bitMask == 0) {
        processReceivedByte(currentByte);
        startNextTxByte();
      } else {
        startReadBit();
      }
    }
    break;

  default:
    break;
  }
}

void OneWire::completeOperation(bool success) {
  lastOpSuccess = success;
  state = isMasterMode ? IDLE : SLAVE_LISTENING;

  if (txCallback) {
    txCallback(success);
    txCallback = nullptr;
  }
}

void OneWire::startNextTxByte() {
  if (txPosition < txLength) {
    currentByte = txBuffer[txPosition++];
    bitMask = 0x01;
    startWriteBit(currentByte & bitMask);
  } else {
    completeOperation(true);
  }
}

void OneWire::processReceivedByte(uint8_t data) {
  if (rxPosition < sizeof(rxBuffer)) {
    rxBuffer[rxPosition++] = data;
  }

  if (rxCallback) {
    rxCallback(rxBuffer, rxPosition);
  }
}

void OneWire::handleSlaveMode() {
  static bool lastBusState = true;
  static uint32_t edgeTime = 0;

  bool currentBusState = readBus();
  uint32_t currentTime = micros();

  // Reset detection
  if (!currentBusState && lastBusState) {
    edgeTime = currentTime;
  } else if (currentBusState && !lastBusState) {
    uint32_t pulseWidth = currentTime - edgeTime;
    if (pulseWidth > 480 * timeoutMultiplier) {
      state = SLAVE_LISTENING;
      rxPosition = 0;

      if (resetCallback) {
        resetCallback();
      }

      // Send presence pulse
      setBusLow();
      delayMicroseconds(60 * timeoutMultiplier);
      setBusHigh();
      return;
    }
  }

  lastBusState = currentBusState;

  switch (state) {
  case SLAVE_LISTENING:
    if (!currentBusState) {
      state = SLAVE_READING;
      currentByte = 0;
      bitMask = 0x01;
      operationStartTime = currentTime;
    }
    break;

  case SLAVE_READING:
    if (currentTime - operationStartTime >= 15 * timeoutMultiplier &&
      currentTime - operationStartTime < 60 * timeoutMultiplier) {
      bool bitValue = currentBusState;
      if (bitValue) {
        currentByte |= bitMask;
      }
      bitMask <<= 1;

      if (bitMask == 0) {
        processReceivedByte(currentByte);

        if (requestCallback) {
          inRequestCallback = true;
          Responder responder( * this);
          //requestCallback(responder);
          inRequestCallback = false;
        }

        state = SLAVE_WRITING;
        bitMask = 0x01;
        currentByte = (slaveResponsePos < slaveResponseLength) ?
          slaveResponseBuffer[slaveResponsePos++] : 0xFF;
      }
    } else if (currentTime - operationStartTime >= 60 * timeoutMultiplier) {
      state = SLAVE_LISTENING;
    }
    break;

  case SLAVE_WRITING:
    if (!currentBusState) {
      bool bitToSend = currentByte & bitMask;

      if (bitToSend) {
        setBusHigh();
      } else {
        setBusLow();
        delayMicroseconds(60 * timeoutMultiplier);
        setBusHigh();
      }

      bitMask <<= 1;
      if (bitMask == 0) {
        state = SLAVE_LISTENING;
      }
    }
    break;

  default:
    state = SLAVE_LISTENING;
    break;
  }
}

void OneWire::handleAutoDetect() {
  static bool lastBusState = true;
  static uint32_t lowStartTime = 0;

  bool currentBusState = readBus();
  uint32_t currentTime = micros();

  if (!currentBusState && lastBusState) {
    lowStartTime = currentTime;
  } else if (currentBusState && !lastBusState) {
    uint32_t pulseWidth = currentTime - lowStartTime;

    if (pulseWidth > 480 * timeoutMultiplier) {
      switchToSlave();
      return;
    }
  }

  lastBusState = currentBusState;

  if (millis() - lastActivityTime > 5000) {
    switchToMaster();
  }
}

void OneWire::handleBusScan() {
  if (state == BUS_SCAN && scanStep == 0) {
    if (micros() - operationStartTime >= 480 * timeoutMultiplier) {
      setBusHigh();
      operationStartTime = micros();
      scanStep = 1;
    }
  } else if (state == BUS_SCAN && scanStep == 1) {
    if (micros() - operationStartTime >= 70 * timeoutMultiplier) {
      if (!readBus()) {
        devicesFound++;
      }
      scanStep = 2;
      operationStartTime = micros();
    }
  } else if (state == BUS_SCAN && scanStep == 2) {
    if (micros() - operationStartTime >= 410 * timeoutMultiplier) {
      state = IDLE;
      if (busScanCallback) {
        busScanCallback(devicesFound);
      }
      determineMode();
    }
  }
}

void OneWire::switchToMaster() {
  isMasterMode = true;
  state = IDLE;
  if (modeCallback) {
    modeCallback(true);
  }
}

void OneWire::switchToSlave() {
  isMasterMode = false;
  state = SLAVE_LISTENING;
  rxPosition = 0;

  if (resetCallback) {
    resetCallback();
  }

  if (modeCallback) {
    modeCallback(false);
  }
}

void OneWire::determineMode() {
  if (devicesFound == 0) {
    switchToMaster();
  } else {
    switchToSlave();
  }
}

void OneWire::generateRandomPollTime() {
  nextPollTime = millis() + random(pollInterval / 2, pollInterval * 3 / 2);
}

void OneWire::internalRespond(const uint8_t * data, uint8_t length) {
  if (length > sizeof(slaveResponseBuffer)) return;

  memcpy(slaveResponseBuffer, data, length);
  slaveResponseLength = length;
  slaveResponsePos = 0;
}

bool OneWire::send(const uint8_t * data, uint8_t length, SendCallback callback) {
  if (!isMasterMode || state != IDLE || length == 0 || length > 64)
    return false;

  memcpy(txBuffer, data, length);
  txLength = length;
  txPosition = 0;
  txCallback = callback;

  startReset();
  return true;
}

void OneWire::scanBus(BusScanCallback callback) {
  if (state != IDLE) return;

  busScanCallback = callback;
  devicesFound = 0;
  scanStep = 0;
  state = BUS_SCAN;
  startReset();
}

void OneWire::onRequest(RequestCallback callback) {
  requestCallback = callback;
}

void OneWire::onReset(ResetCallback callback) {
  resetCallback = callback;
}

void OneWire::onReceive(ReceiveCallback callback) {
  rxCallback = callback;
}

bool OneWire::isMaster() const {
  return isMasterMode;
}

bool OneWire::isBusy() const {
  return state != IDLE && state != AUTO_DETECT;
}

OneWireState OneWire::getState() const {
  return state;
}

bool OneWire::lastOperationSuccess() const {
  return lastOpSuccess;
}

bool OneWire::tick() {
  switch (state) {
  case AUTO_DETECT:
    handleAutoDetect();
    break;
  case BUS_SCAN:
    handleBusScan();
    break;
  case SLAVE_LISTENING:
    if (isMasterMode && millis() > nextPollTime) {
      scanBus([this](uint8_t deviceCount) {
        devicesFound = deviceCount;
        determineMode();
        generateRandomPollTime();
      });
    }
    handleSlaveMode();
    break;
  case SLAVE_READING:
  case SLAVE_WRITING:
    handleSlaveMode();
    break;
  default:
    if (isMasterMode) processState();
    break;
  }

  return true;
}

void async::Responder::respond(const uint8_t * data, uint8_t length) {
  if (ow.inRequestCallback && length <= 64) {
    memcpy(ow.slaveResponseBuffer, data, length);
    ow.slaveResponseLength = length;
    ow.slaveResponsePos = 0;
  }
}