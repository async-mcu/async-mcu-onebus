#include <Arduino.h>
#include <async/Boot.h>
#include <async/OneBus.h>

using namespace async;

OneBus oneBusSlave(23, 10);  // Пин 5

Boot slave(1, [](Executor* executor) {
  Serial.begin(115200);
  Serial.print("slave ");
  Serial.println(xPortGetCoreID());

  executor->add(&oneBusSlave);
  oneBusSlave.begin(false); // as master

  oneBusSlave.onReset([]() {
    Serial.println("Slave request reset");
  });

  oneBusSlave.onRequest([](OneBusReadData * read, Responder * responder) {
    Serial.print("Slave Получены данные: ");
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

  // // // Начало в slave режимк
  // oneWireSlave.begin(false);

  // // // Общие обработчики
  // oneWireSlave.onRequest([](uint8_t* data, uint8_t length, Responder& responder) {
  //   Serial.print("Master requested data after receiving: ");
  //   for (uint8_t i = 0; i < length; i++) {
  //       Serial.print(data[i], HEX);
  //       Serial.print(" ");
  //   }
  //   Serial.println();
    
  //   if (data[0] == 0x44) {
  //     uint8_t result[] = {0x01, 0x02};
  //     responder.respond(result, sizeof(result));
  //   }
  // });
});