#include <ESP8266WiFi.h>
#include "src/iotc/common/string_buffer.h"
#include "src/iotc/iotc.h"

#define WIFI_SSID "xxxxxxxxx"
#define WIFI_PASSWORD "xxxxxxxxx"

/* Azure IoT Central Config */
const char* SCOPE_ID = "xxxxxxxxx";
const char* DEVICE_ID = "xxxxxxxxx";
const char* DEVICE_KEY = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

/* Sensor Input Config */
const byte interruptPin = 13;
int numberOfInterrupts = 0;
int countOfSeconds = 0;
float flow = 0; 
float average = 0; 
void ICACHE_RAM_ATTR handleInterrupt();

void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo);
#include "src/connection.h"

void on_event(IOTContext ctx, IOTCallbackInfo* callbackInfo) {
  // ConnectionStatus
  if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0) {
    LOG_VERBOSE("Is connected ? %s (%d)",
                callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO",
                callbackInfo->statusCode);
    isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    return;
  }

  // payload buffer doesn't have a null ending.
  // add null ending in another buffer before print
  AzureIOT::StringBuffer buffer;
  if (callbackInfo->payloadLength > 0) {
    buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
  }

  LOG_VERBOSE("- [%s] event was received. Payload => %s\n",
              callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

  if (strcmp(callbackInfo->eventName, "Command") == 0) {
    LOG_VERBOSE("- Command name was => %s\r\n", callbackInfo->tag);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(interruptPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);

  connect_wifi(WIFI_SSID, WIFI_PASSWORD);
  connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);

  if (context != NULL) {
    lastTick = 0;  // set timer in the past to enable first telemetry a.s.a.p
  }
}

void loop() {

  delay (1000);
  countOfSeconds++;

  flow = numberOfInterrupts / 5.5; //Converte para L/min sensor de 3/4 polegada
  average=average+flow; //Soma a vazão para o calculo da media
  Serial.print(flow); //Imprime na serial o valor da vazão
  Serial.print(" L/min - "); //Imprime L/min
  Serial.print(countOfSeconds); //Imprime a contagem i (segundos)
  Serial.println("s"); //Imprime s indicando que está em segundos

  numberOfInterrupts=0;
  
  if (isConnected) {
    if (countOfSeconds==60) {  // send telemetry every 60 seconds
      countOfSeconds=0;
      char msg[64] = {0};
      int pos = 0, errorCode = 0;

      average = average/60; // Tira a media dividindo por 60
      Serial.print("\nMedia por minuto = "); //Imprime a frase Media por minuto =
      Serial.print(average); //Imprime o valor da media
      Serial.println(" L/min - "); //Imprime L/min
      pos = snprintf(msg, sizeof(msg) - 1, "{\"Flow\": %d}", int(average));
      errorCode = iotc_send_telemetry(context, msg, pos);
      msg[pos] = 0;
      average = 0;
      
      if (errorCode != 0) {
        LOG_ERROR("Sending message has failed with error code %d", errorCode);
      }
    }

    iotc_do_work(context);  // do background work for iotc
  } else {
    iotc_free_context(context);
    context = NULL;
    connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  }
}

void handleInterrupt() {
  numberOfInterrupts++;
}
