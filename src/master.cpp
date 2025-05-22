#include <async/OneBus.h>

using namespace async;

Pin pinMaster(22);
OneBus oneBusMaster(&pinMaster, 10);  // Пин 5

Pin pinSlave(23);
OneBus oneBusSlave(&pinSlave, 10);  // Пин 5

Executor executor;

void setup() {
  Serial.begin(115200);
  Serial.println("setup");

  executor.start();

  executor.add(&pinMaster);
  executor.add(&pinSlave);
  executor.add(&oneBusMaster);
  executor.add(&oneBusSlave);

  oneBusMaster.begin(true); // as master
  oneBusSlave.begin(false); // as slave
  
  oneBusMaster.onReceive([](OneBusReadData * read) {
    Serial.print("Master receive data: ");
    for (int i = 0; i < read->length; i++) {
      Serial.print(read->data[i], HEX);
    }
    Serial.println();
  });



  executor.onDelay(2000, []() {
    uint64_t number = ESP.getEfuseMac();

    uint8_t result[8];
    for(int i = 0; i < 8; i++) {
        result[i] = uint8_t((number >> 8*(7 - i)) & 0xFF);
    }
  
    if(oneBusMaster.send(result, sizeof(result), []() {
      Serial.println("Send end");
      oneBusMaster.reset();
    })) {
      Serial.println("Start send");
    }
    else {
      Serial.println("Busy!");
    }
  });


  oneBusSlave.onReset([]() {
    Serial.println("Slave request reset");
  });

  oneBusSlave.onRequest([](OneBusReadData * read, Responder * responder) {
    Serial.print("Slave request data size: ");
    Serial.println(read->length);
    for (int i = 0; i < read->length; i++) {
      Serial.print(read->data[i], HEX);
    }
    Serial.println();

    uint64_t number = ESP.getEfuseMac();

    uint8_t result[8];
    for(int i = 0; i < 8; i++) {
        result[i] = uint8_t((number >> 8*(7 - i)) & 0xFF);
    }

    responder->respond(result, sizeof(result));
  });
}

void loop() {
  executor.tick();
}