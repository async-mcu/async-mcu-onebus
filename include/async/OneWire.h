/**
 * @file OneWire.h
 * @brief Asynchronous 1-Wire protocol implementation
 */

#pragma once
#include <Arduino.h>

#include <Arduino.h>
#include <async/Tick.h>
#include <async/Chain.h>
#include <async/Function.h>

namespace async {
  class OneWire;
  class Responder;

  enum OneWireState {
    IDLE,
    RESET_START,
    RESET_WAIT,
    RESET_DETECT_PRESENCE,
    WRITE_BIT_START,
    WRITE_BIT_LOW,
    WRITE_BIT_HIGH,
    READ_BIT_START,
    READ_BIT_SAMPLE,
    READ_BIT_COMPLETE,
    ERROR,
    SLAVE_LISTENING,
    SLAVE_READING,
    SLAVE_WRITING,
    AUTO_DETECT,
    BUS_SCAN
  };

  class OneWire: public Tick {
    public:

    typedef Function < void(uint8_t * data, uint8_t length) > ReceiveCallback;
    typedef Function < void(bool success) > SendCallback;
    typedef Function < void(uint8_t* data, uint8_t length, Responder&) > RequestCallback;
    typedef Function < void() > ResetCallback;
    typedef Function < void(bool isMaster) > ModeDetectedCallback;
    typedef Function < void(uint8_t deviceCount) > BusScanCallback;

    OneWire(uint8_t pin, uint8_t timeoutMultiplier = 1);


    void begin(bool isMaster);
    void begin(uint16_t pollInterval, ModeDetectedCallback callback = nullptr);
    bool tick();
    bool send(const uint8_t * data, uint8_t length, SendCallback callback = nullptr);
    void scanBus(BusScanCallback callback);

    void onRequest(RequestCallback callback);
    void onReset(ResetCallback callback);
    void onReceive(ReceiveCallback callback);

    void setMaster(bool masterMode);
    bool isMaster() const;
    bool isBusy() const;
    OneWireState getState() const;
    bool lastOperationSuccess() const;

    private:
      // Private methods
    void setBusLow();
    void setBusHigh();
    bool readBus();
    void startReset();
    void startWriteBit(uint8_t bit);
    void startReadBit();
    void processState();
    void completeOperation(bool success);
    void startNextTxByte();
    void processReceivedByte(uint8_t data);
    void handleSlaveMode();
    void handleAutoDetect();
    void handleBusScan();
    void switchToMaster();
    void switchToSlave();
    void determineMode();
    void generateRandomPollTime();
    void internalRespond(const uint8_t * data, uint8_t length);

    friend class Responder;

    // Member variables
    uint8_t pin;
    OneWireState state;
    bool isMasterMode;
    uint32_t operationStartTime;
    bool lastOpSuccess;
    bool inRequestCallback;
    uint16_t pollInterval;
    uint32_t nextPollTime;
    ModeDetectedCallback modeCallback;
    BusScanCallback busScanCallback;

    uint8_t txBuffer[64];
    uint8_t txLength;
    uint8_t txPosition;
    SendCallback txCallback;

    uint8_t rxBuffer[64];
    uint8_t rxLength;
    uint8_t rxPosition;
    ReceiveCallback rxCallback;
    RequestCallback requestCallback;
    ResetCallback resetCallback;

    uint8_t slaveResponseBuffer[64];
    uint8_t slaveResponseLength;
    uint8_t slaveResponsePos;

    uint8_t timeoutMultiplier;
    uint8_t currentByte;
    uint8_t bitMask;
    uint8_t readResult;
    uint32_t lastActivityTime;

    uint8_t devicesFound;
    uint8_t scanStep;
  };

  class Responder {
    private: OneWire & ow;
    public: 
      Responder(OneWire & ow): ow(ow) {}
      void respond(const uint8_t * data, uint8_t length); // {
  };
}