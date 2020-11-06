/* Required libraries */
#include <ESP8266WiFi.h>
#include "src/iotc/common/string_buffer.h"
#include "src/iotc/iotc.h"

/* Wifi Central Config  - NEEDS UPDATE HERE */
#define WIFI_SSID "xxxxxxxxx"
#define WIFI_PASSWORD "xxxxxxxxx"

/* Azure IoT Central Config - NEEDS UPDATE HERE  */
const char* SCOPE_ID = "xxxxxxxxx";
const char* DEVICE_ID = "xxxxxxxxx";
const char* DEVICE_KEY = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

/* Sensor Input Config */
const byte interruptPin = 13;
int numberOfInterrupts = 0;
int countOfSeconds = 0;
float flow = 0; 
float average = 0; 
void ICACHE_RAM_ATTR handleInterrupt();  // See https://stackoverflow.com/questions/58113937/esp8266-arduino-why-is-it-necessary-to-add-the-icache-ram-attr-macro-to-isrs-an

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
    lastTick = 0;  
  }
}

void loop() {

  delay (1000);
  countOfSeconds++;

  flow = numberOfInterrupts / 5.5; //Conversion to L/min for 3/4in flow sensor
  average=average+flow; 
  Serial.print(flow); 
  Serial.print(" L/min - "); 
  Serial.print(countOfSeconds); 
  Serial.println("s"); 

  numberOfInterrupts=0;
  
  if (isConnected) {
    if (countOfSeconds==60) {  
      countOfSeconds=0;
      char msg[64] = {0};
      int pos = 0, errorCode = 0;

      average = average/60; // Average in Liters per minute
      average = average * 1000; // Converting from Liters to mili Liters to keep precision during Int convertion at line 89
      Serial.print("\nAverage per minute = "); 
      Serial.print(average); //Print average to serial in mL
      Serial.println(" mL/min - "); 
      pos = snprintf(msg, sizeof(msg) - 1, "{\"Flow\": %d}", int(average));
      errorCode = iotc_send_telemetry(context, msg, pos);
      msg[pos] = 0;
      average = 0;
      
      if (errorCode != 0) {
        LOG_ERROR("Sending message has failed with error code %d", errorCode);
      }
    }

    iotc_do_work(context);  
  } else {
    iotc_free_context(context);
    context = NULL;
    connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  }
}

void handleInterrupt() {
  numberOfInterrupts++;
}
