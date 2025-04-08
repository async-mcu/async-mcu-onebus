// #include <async/Log.h>
// #include <async/Boot.h>
// #include <async/OneWire.h>

// using namespace async;

// OneWire oneWireAuto(5);  // Пин 5

// Boot autoSelect([](Executor* executor) {
//   Serial.begin(115200);
//   Serial.println("auto");

//   executor->add(&oneWireAuto);
  
//   // Начало в автоматическом режиме
//   oneWireAuto.begin(5000, [](bool isMaster) {
//     info("Работаем как Master? %s", isMaster);
//   });

//   // Общие обработчики
//   oneWireAuto.onReceive([](uint8_t* data, uint8_t length) {
//     Serial.print("Получены данные: ");
//     for (int i = 0; i < length; i++) {
//       Serial.print(data[i], HEX);
//       Serial.print(" ");
//     }
//     Serial.println();
//   });
// });