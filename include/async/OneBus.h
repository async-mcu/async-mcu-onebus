#pragma once

#include <async/Pin.h>
#include <async/Log.h>
#include <async/Tick.h>
#include <async/Chain.h>
#include <async/Executor.h>

namespace async {
    class OneBus;

    struct OneBusReadData {
        ~OneBusReadData() {
            free(data);
        }

        OneBusReadData(uint64_t serial, uint8_t * data, uint32_t length): serial(serial), data(data), length(length) {}

        uint64_t serial;
        uint8_t * data;
        uint32_t length;
    };

    typedef Function < void() > SendCallback;
    typedef Function < void(OneBusReadData * data) > ReceiveCallback;
    typedef Function < void() > ResetCallback;
    typedef Function < void(bool isMaster) > ModeDetectedCallback;
    typedef Function < void(uint8_t deviceCount) > BusScanCallback;


    class Responder;

    class Responder {
        OneBus * onebus;
        public:
        Responder(OneBus * onebus);
        bool respond(uint8_t* data, uint8_t length, SendCallback callback = nullptr);
    };
    
    typedef Function < void(OneBusReadData * data, Responder *) > RequestCallback;

    class OneBus : public Tick {
        private:
            ModeDetectedCallback modeCallback;
            BusScanCallback busScanCallback;
            ReceiveCallback rxCallback;
            RequestCallback requestCallback;
            ResetCallback resetCallback;
            SendCallback txCallback;
            Executor executor;  
            Semaphore semaphoreWrite;
            Semaphore semaphoreRead;
            Responder responder;
            bool isMaster;
            volatile bool busyWrite;
            volatile bool busyRead;
            int timeSlot;
            Pin * pin;
            int writes;
            uint8_t byteRead;
            uint8_t receive[8];
            uint8_t* readByteArray;  // Массив для хранения байтов
            uint32_t readByteCount;  // Количество собранных байтов
            uint8_t readBitBuffer;   // Буфер для накопления битов
            uint8_t readBitsInBuffer;// Количество битов в буфере (0-7)

            void setBusLow() {
                pin->digitalWrite(LOW);
            }

            void setBusHigh() {
                pin->digitalWrite(HIGH);
            }

            void setBusRead() {
                pin->setMode(INPUT_PULLUP);
            }
        
        public: 
            OneBus(Pin * pin, int timeSlot = 60): 
                pin(pin), 
                isMaster(false),
                timeSlot(timeSlot),
                busyWrite(false),
                busyRead(false),
                modeCallback(nullptr),
                busScanCallback(nullptr),
                txCallback(nullptr),
                rxCallback(nullptr),
                requestCallback(nullptr),
                resetCallback(nullptr),
                writes(0), 
                semaphoreWrite(Semaphore(1, 1)), 
                semaphoreRead(Semaphore(1, 1)),
                responder(Responder(this)),
                readByteArray(nullptr), 
                readByteCount(0), 
                readBitBuffer(0), 
                readBitsInBuffer(0) { }

            bool start() override {
                    setBusRead();

                    // reader
                    executor.add(chain<uint32_t>(0)
                        ->interrupt(pin, FALLING) // wait fo
                        ->then([this](uint32_t time) {  // set current time
                            busyRead = true;

                            if(busyWrite) {
                                return time;
                            }
                            
                            return (uint32_t)millis(); 
                        })
                        ->interrupt(pin, RISING) // wait for high
                        ->then([this](uint32_t time) {

                            if(busyWrite) {
                                return time;
                            }

                            uint32_t interval = millis() - time;

                            int halfSlot = timeSlot/2;

                            if(interval > timeSlot * 8 + halfSlot) {
                                //Serial.println("bad request");
                            }
                            else if(interval >= timeSlot * 8 - halfSlot && interval < timeSlot * 8 + halfSlot) {
                                //Serial.println("read reset");
                                resetCallback();
                            }
                            // read end command
                            else if(interval >= timeSlot * 4 - halfSlot && interval < timeSlot * 4 + halfSlot) {
                                uint8_t* result = readByteArray;
                                Serial.println("read end");

                                OneBusReadData data({0, readByteArray, readByteCount});

                                if(isMaster) {
                                    rxCallback(&data);
                                }
                                else {
                                    requestCallback(&data, &responder);
                                }

                                // Сбрасываем состояние класса
                                readByteArray = nullptr;
                                readByteCount = 0;
                                readBitBuffer = 0;
                                readBitsInBuffer = 0;
                            }
                            // read start command
                            else if(interval >= timeSlot * 3 - halfSlot && interval < timeSlot * 3 + halfSlot) {
                                //Serial.println("read start");
                                // Сбрасываем состояние 
                                readByteArray = nullptr;
                                readByteCount = 0;
                                readBitBuffer = 0;
                                readBitsInBuffer = 0;
                            }
                            // read 0 bit
                            else if(interval >= timeSlot * 2 - halfSlot && interval < timeSlot * 2 + halfSlot) {
                                //Serial.println("read 0 bit");
                                readBitBuffer =  (readBitBuffer >> 1) | ((false & 1) << 7);
                                readBitsInBuffer++;
                            }
                            // read 1 bit
                            else if(interval >= timeSlot - halfSlot && interval < timeSlot + halfSlot) {
                                //Serial.println("read 1 bit");
                                readBitBuffer =  (readBitBuffer >> 1) | ((true & 1) << 7);
                                readBitsInBuffer++;
                            }

                            if (readBitsInBuffer == 8) {
                                // Расширяем массив
                                uint8_t* newArray = (uint8_t*)realloc(readByteArray, readByteCount + 1);
                                
                                readByteArray = newArray;
                                readByteArray[readByteCount] = readBitBuffer;
                                readByteCount++;
                                
                                // Сбрасываем буфер
                                readBitBuffer = 0;
                                readBitsInBuffer = 0;
                            }

                            busyRead = false;
                            return time;
                        })
                        ->loop());

                    return true;
                }

            void begin(bool isMaster) {
                this->isMaster = isMaster;
            }

            // auto select master/slave
            // void begin(uint16_t pollInterval, ModeDetectedCallback callback = nullptr) {
                
            // }

            bool send(uint8_t * data, uint8_t length, SendCallback callback = nullptr) {
                if(busyWrite) {
                    return false;
                }

                writes = length * 8;
                busyWrite = true;
                semaphoreRead.acquire();

                // write start command
                executor.add(chain()
                    ->semaphoreWaitAcquire(&semaphoreWrite)
                    ->delay(timeSlot)
                    ->then([this]() {
                        setBusLow();
                    })
                    ->delay(timeSlot * 3)
                    ->then([this]() {
                        setBusHigh();
                    })
                    ->delay(timeSlot)
                    ->then([this]() {
                        semaphoreWrite.release();
                    }));

                for(uint8_t m=0; m < length; m++) {
                    uint8_t value = data[m];

                    for (uint8_t g = 0; g < 8; g++) {
                        bool bit = value & 0x01;
                        value >>= 1;

                        // is performed sequentially
                        executor.add(chain()
                            ->semaphoreWaitAcquire(&semaphoreWrite)
                            ->then([this, bit]() {
                                setBusLow();
                            })
                            ->delay(bit ? timeSlot : timeSlot * 2)
                            ->then([this]() {
                                setBusHigh();
                            })
                            ->delay(timeSlot)
                            ->then([this]() {
                                semaphoreWrite.release();
                            }));
                    }
                }

                executor.add(chain()
                    ->semaphoreWaitAcquire(&semaphoreWrite)
                    ->then([this]() {
                        setBusLow();
                    })
                    ->delay(timeSlot * 4)
                    ->then([this]() {
                        setBusHigh();
                    })
                    ->delay(timeSlot)
                    ->then([this, callback]() {
                        semaphoreRead.release();
                        semaphoreWrite.release();
                        setBusRead();
                        busyWrite = false;
                        callback();
                    }));

                return true;
            }

            bool reset() {
                if(busyWrite) {
                    return false;
                }

                busyWrite = true;

                executor.add(chain()
                    ->then([this]() {
                        setBusLow();
                    })
                    ->delay(timeSlot * 8)
                    ->then([this]() {
                        setBusRead();
                        busyWrite = false;
                    }));

                return true;
            }

            //void scanBus(BusScanCallback callback) { busScanCallback = callback; }

            void onRequest(RequestCallback callback) { 
                requestCallback = callback; 
            }

            void onReset(ResetCallback callback) {
                resetCallback = callback;
            }
            void onReceive(ReceiveCallback callback) { rxCallback = callback; }

            bool tick() {
                return executor.tick();
            }
    };
}