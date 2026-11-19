#pragma once
#include <stdint.h>

enum BrI2cOperation : uint8_t {
  BR_I2C_OP_NONE = 0,
  BR_I2C_OP_INPUT_READ = 1,
  BR_I2C_OP_LED_WRITE = 2
};

// Host-testable I2C diagnostics. A raw input is committed only after a full
// two-byte read; failed or partial transactions never replace the last sample.
struct BrI2cDiagnostics {
  uint32_t inputReadErrors = 0;
  uint32_t ledWriteErrors = 0;
  uint8_t sdaPin = 21;
  uint8_t sclPin = 22;
  uint32_t frequencyHz = 10000;

  uint8_t lastReadRequested = 0;
  uint8_t lastReadReturned = 0;
  uint8_t lastReadAvailable = 0;
  uint16_t lastValidRaw = 0xffff;
  uint32_t lastValidAtMs = 0;
  bool hasValidRaw = false;

  uint8_t lastErrorAddress = 0;
  BrI2cOperation lastErrorOperation = BR_I2C_OP_NONE;
  int16_t lastErrorCode = 0;

  bool recordReadResult(uint8_t requested, uint8_t returned,
                        uint8_t available) {
    lastReadRequested = requested;
    lastReadReturned = returned;
    lastReadAvailable = available;
    if (requested == 2 && returned == 2 && available == 2) return true;
    ++inputReadErrors;
    lastErrorAddress = 0x22;
    lastErrorOperation = BR_I2C_OP_INPUT_READ;
    // Wire.requestFrom exposes a byte count, not the low-level ESP-IDF error.
    // Report that count as-is rather than inventing an I2C status code.
    lastErrorCode = returned;
    return false;
  }

  void recordReadValueError(int16_t readValue) {
    ++inputReadErrors;
    lastErrorAddress = 0x22;
    lastErrorOperation = BR_I2C_OP_INPUT_READ;
    lastErrorCode = readValue;
  }

  void commitValidRaw(uint16_t raw, uint32_t nowMs) {
    lastValidRaw = raw;
    lastValidAtMs = nowMs;
    hasValidRaw = true;
  }

  void recordLedWrite(uint8_t returnCode) {
    if (returnCode == 0) return;
    ++ledWriteErrors;
    lastErrorAddress = 0x20;
    lastErrorOperation = BR_I2C_OP_LED_WRITE;
    lastErrorCode = returnCode;
  }
};

inline const char *brI2cOperationName(BrI2cOperation operation) {
  switch (operation) {
    case BR_I2C_OP_INPUT_READ: return "input_read";
    case BR_I2C_OP_LED_WRITE: return "led_write";
    default: return "none";
  }
}
