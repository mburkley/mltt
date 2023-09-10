#include <stdbool.h>
#include <stdint.h>

bool cassetteMotor(int index, uint8_t value);
bool cassetteAudioGate(int index, uint8_t value);
bool cassetteTapeOutput(int index, uint8_t value);
uint8_t cassetteTapeInput(int index, uint8_t value);

