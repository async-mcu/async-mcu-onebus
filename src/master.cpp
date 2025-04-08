#include <async/Boot.h>
#include <async/OneBus.h>

using namespace async;

OneBus oneBusMaster(22, 10);  // Пин 5

Boot master(0, [](Executor* executor) {
  Serial.begin(115200);
  
  executor->add(&oneBusMaster);

  oneBusMaster.begin(true); // as master
  
  oneBusMaster.onReceive([](OneBusReadData * read) {
    Serial.print("Master receive data: ");
    for (int i = 0; i < read->length; i++) {
      Serial.print(read->data[i], HEX);
    }
    Serial.println();
  });
  
  //send messages
  executor->onRepeat(5000, []() {
    uint64_t number = ESP.getEfuseMac();

    uint8_t result[8];
    for(int i = 0; i < 8; i++) {
        result[i] = uint8_t((number >> 8*(7 - i)) & 0xFF);
    }
  
    if(oneBusMaster.send(result, sizeof(result), []() {
      Serial.println("Send end");
    })) {
      Serial.println("Start send");
    }
    else {
      Serial.println("Busy!");
    }
  });
});