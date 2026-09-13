#include "thermal_mgr.h"
#include "console.h"
#include "errors.h"
#include "lm75bd.h"
#include "logging.h"

#include <FreeRTOS.h>
#include <os_task.h>
#include <os_queue.h>

#include <string.h>

#define THERMAL_MGR_STACK_SIZE 256U

static TaskHandle_t thermalMgrTaskHandle;
static StaticTask_t thermalMgrTaskBuffer;
static StackType_t thermalMgrTaskStack[THERMAL_MGR_STACK_SIZE];

#define THERMAL_MGR_QUEUE_LENGTH 10U
#define THERMAL_MGR_QUEUE_ITEM_SIZE sizeof(thermal_mgr_event_t)

static QueueHandle_t thermalMgrQueueHandle;
static StaticQueue_t thermalMgrQueueBuffer;
static uint8_t thermalMgrQueueStorageArea[THERMAL_MGR_QUEUE_LENGTH * THERMAL_MGR_QUEUE_ITEM_SIZE];

static void thermalMgr(void *pvParameters);

void initThermalSystemManager(lm75bd_config_t *config) {
  memset(&thermalMgrTaskBuffer, 0, sizeof(thermalMgrTaskBuffer));
  memset(thermalMgrTaskStack, 0, sizeof(thermalMgrTaskStack));
  
  thermalMgrTaskHandle = xTaskCreateStatic(
    thermalMgr, "thermalMgr", THERMAL_MGR_STACK_SIZE,
    config, 1, thermalMgrTaskStack, &thermalMgrTaskBuffer);

  memset(&thermalMgrQueueBuffer, 0, sizeof(thermalMgrQueueBuffer));
  memset(thermalMgrQueueStorageArea, 0, sizeof(thermalMgrQueueStorageArea));

  thermalMgrQueueHandle = xQueueCreateStatic(
    THERMAL_MGR_QUEUE_LENGTH, THERMAL_MGR_QUEUE_ITEM_SIZE,
    thermalMgrQueueStorageArea, &thermalMgrQueueBuffer);

}

error_code_t thermalMgrSendEvent(thermal_mgr_event_t *event) {
  if (xQueueSend(thermalMgrQueueHandle, event, 0) == errQUEUE_FULL)
    return ERR_CODE_QUEUE_FULL;

  return ERR_CODE_SUCCESS;
}

void osHandlerLM75BD(void) {
  error_code_t errCode;

  thermal_mgr_event_t event = {THERMAL_MGR_EVENT_OS_HANDLER_TRIG};
  if (xQueueSend(thermalMgrQueueHandle, &event, 0) == errQUEUE_FULL)
    LOG_ERROR_CODE(ERR_CODE_QUEUE_FULL);
}

static void thermalMgr(void *pvParameters) {
  error_code_t errCode;

  lm75bd_config_t *config = (lm75bd_config_t *)pvParameters;

  thermal_mgr_event_t rxEvent;
  float temperature;

  /* Implement this task */
  while (1) {
    xQueueReceive(thermalMgrQueueHandle, &rxEvent, portMAX_DELAY);
    switch (rxEvent.type) {
    case THERMAL_MGR_EVENT_MEASURE_TEMP_CMD:
      LOG_IF_ERROR_CODE(readTempLM75BD(LM75BD_OBC_I2C_ADDR, &temperature));
      if (errCode != ERR_CODE_SUCCESS)
        break;
      addTemperatureTelemetry(temperature);
      break;
    case THERMAL_MGR_EVENT_OS_HANDLER_TRIG:
      LOG_IF_ERROR_CODE(readTempLM75BD(LM75BD_OBC_I2C_ADDR, &temperature));
      if (errCode != ERR_CODE_SUCCESS)
        return;

      if (temperature > config->hysteresisThresholdCelsius)
        overTemperatureDetected();
      else
        safeOperatingConditions();
      break;
    default:
      LOG_ERROR("Unhandled type in thermal_mgr_event_t: %d", rxEvent.type);
      break;
    }
  }
}

void addTemperatureTelemetry(float tempC) {
  printConsole("Temperature telemetry: %f deg C\n", tempC);
}

void overTemperatureDetected(void) {
  printConsole("Over temperature detected!\n");
}

void safeOperatingConditions(void) { 
  printConsole("Returned to safe operating conditions!\n");
}
