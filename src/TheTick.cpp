#include <Arduino.h>
#include "tick_default_config.h"

#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>

#include "tick_wifi.h"
#include "tick_utils.h"
#include "tick_lcd.h"
#include "tick_syslog.h"
#include "tick_ota.h"
#include "tick_osdp.h"
#include "tick_ble.h"
#include "tick_http.h"
#include "tick_mdns_responder.h"

// byte incoming_byte = 0;
unsigned long config_reset_millis = 30000;
byte reset_pin_state = 1;

unsigned long last_aux_change = 0;
byte last_aux = 1;
byte expect_aux = 2;

#include "tick_wiegand_reader.h"

void detachInterrupts(void) {
  noInterrupts();
  switch(current_tick_mode){
    #ifdef USE_WIEGAND
    case tick_mode_wiegand:
      detachInterrupt(digitalPinToInterrupt(wiegand_pin_d1));
      detachInterrupt(digitalPinToInterrupt(wiegand_pin_d0));
      break;
    #endif
  }
  interrupts();
}

void attachInterrupts(void) {
  noInterrupts();
  switch(current_tick_mode){
    #ifdef USE_WIEGAND
    case tick_mode_wiegand:
      attachInterrupt(digitalPinToInterrupt(wiegand_pin_d0), reader1_wiegand_trigger, FALLING);
      attachInterrupt(digitalPinToInterrupt(wiegand_pin_d1), reader1_wiegand_trigger, FALLING);
      break;
    #endif
  }
  interrupts();
}

void auxChange(void) {
  volatile byte new_value = digitalRead(PIN_AUX);
  if (new_value == expect_aux) {
    last_aux = new_value;
    expect_aux = 2;
    return;
  }
  if (new_value != last_aux) {
    if (millis() - last_aux_change > 10) {
      last_aux_change = millis();
      last_aux = new_value;
      append_log("Aux changed to "+String(new_value));
    }
  }
}

void output_debug_string(String s){
  DBG_OUTPUT_PORT.println(s);
  display_string(s);
}


void jamming_enable(void){
  switch(current_tick_mode){
    #ifdef USE_WIEGAND
    case tick_mode_wiegand:
      wiegand_drainD0();
      break;
    #endif
  }
}

void jamming_disable(void){
  switch(current_tick_mode){
    #ifdef USE_WIEGAND
    case tick_mode_wiegand:
      wiegand_restoreD0();
      break;
    #endif
  }
}

#include "tick_config_handling.h"

void append_log(String text) {
  File file = SPIFFS.open("/log.txt", "a");
  if (file) {
    file.println(String(millis()) + " " + text);
    DBG_OUTPUT_PORT.println("Appending to log: " + String(millis()) + " " + text);
    file.close();
  } else {
    DBG_OUTPUT_PORT.println("Failed opening log file.");
  }
}

void resetConfig(void) {
  if (millis() > 30000){
    return;
  }
  if (digitalRead(CONF_RESET) == LOW && reset_pin_state == 1) {
    reset_pin_state = 0;
    config_reset_millis = millis();
  } else {
    reset_pin_state = 1;
    if (millis() > (config_reset_millis + 2000)) {
      output_debug_string(F("CONFIG RESET"));
      append_log(F("Config reset by pin."));
      SPIFFS.remove(CONFIG_FILE);
      delay(5000);
      ESP.restart();
    }
  }
}

void setup() {
  // Inputs
  pinMode(PIN_D0, INPUT);
  digitalWrite(PIN_D0, HIGH);
  pinMode(PIN_D1, INPUT);
  digitalWrite(PIN_D1, HIGH);
  pinMode(PIN_AUX, INPUT);
  digitalWrite(PIN_AUX, LOW);

  pinMode(PIN_PCB_LED, OUTPUT);
  digitalWrite(PIN_PCB_LED, HIGH);

  pinMode(CONF_RESET, INPUT);

  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");


  osdp_init();
  display_init();

  output_debug_string("Chip ID: 0x" + String(getChipID(), HEX));

  dhcp_hostname = String(HOSTNAME);
  dhcp_hostname += String(getChipID(), HEX);
  output_debug_string("Hostname: " + dhcp_hostname);

  delay(1000);

  if (!SPIFFS.begin()) {
    output_debug_string(F("Failed to mount SPIFFS"));
    delay(1000);
    return;
  } else {
    DBG_OUTPUT_PORT.println(F("SPIFFS mount suceeded"));
  }

  // If a log.txt exists, use ap_ssid=TheTick-<chipid> instead of the default TheTick-config
  // A config file will take precedence over this
  if (SPIFFS.exists("/log.txt")) {
    dhcp_hostname.toCharArray(ap_ssid, sizeof(ap_ssid));
  }
  append_log(F("Starting up!"));

  if (!loadConfig()) {
    output_debug_string(F("No configuration. Defaults."));
  }

  switch(current_tick_mode){
    case tick_mode_wiegand:
      pinMode(wiegand_pin_d0, INPUT);
      digitalWrite(wiegand_pin_d0, HIGH);
      pinMode(wiegand_pin_d1, INPUT);
      digitalWrite(wiegand_pin_d1, HIGH);
      break;
  }

  wifi_init();
  syslog_init();
  ota_init();
  ble_init();
  http_init();
  mdns_responder_init();

  // Input interrupts
  attachInterrupts();
  attachInterrupt(digitalPinToInterrupt(CONF_RESET), resetConfig, CHANGE);
  //attachInterrupt(digitalPinToInterrupt(PIN_AUX), auxChange, CHANGE);
}

int counter = 0;
void heartbeat(){
  if(counter++ % 2){
      digitalWrite(PIN_PCB_LED, HIGH);
  } else {
      digitalWrite(PIN_PCB_LED, LOW);
  }
}


void card_read_handler(const char * card_id){
    output_debug_string(card_id);

    ble_card_read(card_id);

    if(strcmp(card_id, DoS_id) == 0) {
      jamming_enable();
      append_log("DoS mode enabled by control card");
    } else {
      append_log(String(card_id));
    }
}

void transmit_id(String sendValue, unsigned long bitcount){
  switch(current_tick_mode){
    #ifdef USE_WIEGAND
    case tick_mode_wiegand:
      wiegand_transmit_id(sendValue, bitcount);
      break;
    #endif
  }
}

void loop()
{
  heartbeat();

  switch(current_tick_mode){
    #ifdef USE_WIEGAND
    case tick_mode_wiegand:
      wiegand_loop(card_read_handler);
      break;
    #endif
  }

  http_loop();
  ble_loop();
  ota_loop();
  
  delay(100);
}