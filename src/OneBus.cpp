#include <async/OneBus.h>

using namespace async;

Responder::Responder(OneBus * onebus) {
    this->onebus = onebus;
};

bool Responder::respond(uint8_t* data, uint8_t length, SendCallback callback) {
    this->onebus->send(data, length, callback);
    return true;
}