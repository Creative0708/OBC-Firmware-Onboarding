#include "lm75bd.h"
#include "i2c_io.h"
#include "errors.h"
#include "logging.h"

#include <stdint.h>
#include <string.h>

#define POINTER_WRITE_BUFF_SIZE 1U
#define TEMP_READ_BUFF_SIZE 2U
#define CONF_WRITE_BUFF_SIZE 2U

/* LM75BD Registers (p.8) */
typedef enum {
  LM75BD_REG_TEMP = 0x0U,  /* Temperature Register (R/O) */
  LM75BD_REG_CONF = 0x1U,  /* Configuration Register (R/W) */
  LM75BD_REG_THYST = 0x2U, /* Overtemperature Hysteresis Register (R/W) */
  LM75BD_REG_TOR = 0x3U,   /* Overtemperature Shutdown Register (R/W) */
} lm75bd_reg_t;

error_code_t lm75bdInit(lm75bd_config_t *config) {
  error_code_t errCode;

  if (config == NULL) return ERR_CODE_INVALID_ARG;

  RETURN_IF_ERROR_CODE(writeConfigLM75BD(config->devAddr, config->osFaultQueueSize, config->osPolarity,
                                         config->osOperationMode, config->devOperationMode));

  // Assume that the overtemperature and hysteresis thresholds are already set
  // Hysteresis: 75 degrees Celsius
  // Overtemperature: 80 degrees Celsius

  return ERR_CODE_SUCCESS;
}

/* LM75BD Write to Pointer Register (p.15) */
static error_code_t writePointerLM75BD(uint8_t devAddr, lm75bd_reg_t reg) {
  uint8_t buff[POINTER_WRITE_BUFF_SIZE] = {0};

  buff[0] = reg;

  return i2cSendTo(devAddr, buff, POINTER_WRITE_BUFF_SIZE);
}

/* LM75BD Read Temp, Tos or Thyst register with preset pointer (p.15) */
error_code_t readTempLM75BD(uint8_t devAddr, float *temp) {
  error_code_t errCode;

  if (temp == NULL)
    return ERR_CODE_INVALID_ARG;

  RETURN_IF_ERROR_CODE(writePointerLM75BD(devAddr, LM75BD_REG_TEMP));

  uint8_t buff[TEMP_READ_BUFF_SIZE];
  errCode = i2cReceiveFrom(devAddr, buff, TEMP_READ_BUFF_SIZE);
  if (errCode != ERR_CODE_SUCCESS)
    return errCode;

  // Per data sheet
  int16_t rawTemp = (int16_t)(int8_t)buff[0] << 3 // MSB, sign extended
                    | (int16_t)buff[1] >> 5;      // LSB, not sign extended
  *temp = rawTemp * 0.125;

  return ERR_CODE_SUCCESS;
}

error_code_t writeConfigLM75BD(uint8_t devAddr, uint8_t osFaultQueueSize, uint8_t osPolarity,
                                   uint8_t osOperationMode, uint8_t devOperationMode) {
  error_code_t errCode;

  // Stores the register address and data to be written
  // 0: Register address
  // 1: Data
  uint8_t buff[CONF_WRITE_BUFF_SIZE] = {0};

  buff[0] = LM75BD_REG_CONF;

  uint8_t osFaultQueueRegData = 0;
  switch (osFaultQueueSize) {
    case 1:
      osFaultQueueRegData = 0;
      break;
    case 2:
      osFaultQueueRegData = 1;
      break;
    case 4:
      osFaultQueueRegData = 2;
      break;
    case 6:
      osFaultQueueRegData = 3;
      break;
    default:
      return ERR_CODE_INVALID_ARG;
  }

  buff[1] |= (osFaultQueueRegData << 3);
  buff[1] |= (osPolarity << 2);
  buff[1] |= (osOperationMode << 1);
  buff[1] |= devOperationMode;

  errCode = i2cSendTo(devAddr, buff, CONF_WRITE_BUFF_SIZE);
  if (errCode != ERR_CODE_SUCCESS) return errCode;

  return ERR_CODE_SUCCESS;
}
