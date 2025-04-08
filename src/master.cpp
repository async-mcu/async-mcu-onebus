#include <async/Boot.h>
#include <async/OneBus.h>

using namespace async;

OneBus oneBusMaster(22, 10);  // Пин 5

Boot master(0, [](Executor* executor) {
  Serial.begin(115200);
  Serial.print("master ");
  Serial.println(xPortGetCoreID());
  

  executor->add(&oneBusMaster);

  oneBusMaster.begin(true); // as master
  
  // Общие обработчики
  oneBusMaster.onReceive([](OneBusReadData * read) {
    Serial.print("Master Получены данные: ");
    for (int i = 0; i < read->length; i++) {
      Serial.print(read->data[i], HEX);
    }
    Serial.println();
  });

  // uint8_t data[] = {0x01, 0x02}; // SKIP ROM + CONVERT T
  // oneWireMaster.send(data, sizeof(data), [](bool success) {
  //   Serial.println(success ? "Send successful" : "Send failed");
  // });
  
  //send messages
  executor->onRepeat(5000, []() {
    uint64_t number = ESP.getEfuseMac();

    uint8_t result[8];
    for(int i = 0; i < 8; i++) {
        result[i] = uint8_t((number >> 8*(7 - i)) & 0xFF);
    }
  
    if(oneBusMaster.send(result, sizeof(result), []() {
      //Serial.println("Send end");

      Serial.print("Total heap: ");
      Serial.println(ESP.getHeapSize());
      Serial.print("Free heap: ");
      Serial.println(ESP.getFreeHeap());
      Serial.print("Total PSRAM: ");
      Serial.println(ESP.getPsramSize());
      Serial.print("Free PSRAM: ");
      Serial.println(ESP.getFreePsram());
    })) {
      Serial.println("Start send");
    }
    else {
      Serial.println("Busy!");
    }


      //Serial.print("spiram size: ");
      //Serial.println(esp_spiram_get_size());
      // Serial.print("himem free: ");
      // Serial.println(esp_himem_get_free_size());
      // Serial.print("himem phys: ");
      // Serial.println(esp_himem_get_phys_size());
      // Serial.print("himem reserved: ");
      // Serial.println(esp_himem_reserved_area_size());

    // if(oneBusMaster.reset()) {
    //   Serial.println("Start reset");
    // }
    // else {
    //   Serial.println("Busy!");
    // }
    //uint8_t data[] = {0x01, 0x02}; // SKIP ROM + CONVERT T

    //bool success = oneBusMaster.send(data, sizeof(data));
    //Serial.println(success ? "Send successful" : "Send failed");
  });
});