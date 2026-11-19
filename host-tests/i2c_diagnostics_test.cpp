#include <cassert>
#include <cstdio>
#include <string>
#include "../firmware/bytereel_2026/i2c_diagnostics.h"

int main() {
  BrI2cDiagnostics d;
  assert(d.sdaPin == 21 && d.sclPin == 22 && d.frequencyHz == 10000);

  // A short/no-ACK read reports the observed transfer and cannot create a
  // replacement raw sample from zeroes or 0xFF fill values.
  assert(!d.recordReadResult(2, 0, 0));
  assert(d.inputReadErrors == 1 && !d.hasValidRaw);
  assert(d.lastErrorAddress == 0x22);
  assert(d.lastErrorOperation == BR_I2C_OP_INPUT_READ);
  assert(d.lastErrorCode == 0);
  assert(d.lastReadRequested == 2 && d.lastReadReturned == 0 &&
         d.lastReadAvailable == 0);

  assert(d.recordReadResult(2, 2, 2));
  d.commitValidRaw(0xA55A, 1234);
  assert(d.hasValidRaw && d.lastValidRaw == 0xA55A && d.lastValidAtMs == 1234);

  assert(!d.recordReadResult(2, 2, 1));
  assert(d.inputReadErrors == 2 && d.lastValidRaw == 0xA55A &&
         d.lastValidAtMs == 1234);
  assert(!d.recordReadResult(2, 1, 1));
  assert(d.inputReadErrors == 3 && d.lastReadReturned == 1);

  d.recordLedWrite(3);
  assert(d.ledWriteErrors == 1 && d.lastErrorAddress == 0x20);
  assert(d.lastErrorOperation == BR_I2C_OP_LED_WRITE && d.lastErrorCode == 3);
  d.recordLedWrite(0);
  assert(d.ledWriteErrors == 1);
  assert(brI2cOperationName(d.lastErrorOperation) == std::string("led_write"));
  std::puts("PASS I2C diagnostics: atomic samples, short reads, separate counters and last error");
}
