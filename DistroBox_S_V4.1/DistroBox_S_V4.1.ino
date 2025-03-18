#include <Wire.h>
#include <EEPROM.h>

const int NUM_SWITCHES = 11; // Number of switches connected (11 outputs)
#define TARGET_ADDRESS 0x09  // I2C slave address
#define MASTER_ADDR 0x08     // I2C master address

// EEPROM addresses for output states
const int outAddr[NUM_SWITCHES] = {2, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3}; // Correct pin mapping

// Variables for I2C communication
String receivedMessage = "";
int responseMessage = 0;
bool change = true;

void setup() {
    Wire.begin(TARGET_ADDRESS); // Join I2C bus as slave
    Wire.onReceive(receiveRequest);
    Wire.onRequest(requestResponse);
    Serial.begin(9600);

    // Set pin modes for outputs
    for (int i = 0; i < NUM_SWITCHES; i++) {
        pinMode(outAddr[i], OUTPUT); // Use outAddr to set pin modes
    }
}

void loop() {
    mosfetWrite();
}

void receiveRequest(int byteCount) {
  receivedMessage = "";
  responseMessage = 2; // Default response for unknown commands

  // Read incoming data
  while (Wire.available()) {
    receivedMessage += (char)Wire.read();
  }

  // Check EEPROM states or update them
  for (int i = 1; i <= NUM_SWITCHES; i++) { // Start from 1 to match master's indexing
    if (receivedMessage == "check " + String(i)) {
      responseMessage = EEPROM.read(outAddr[i - 1]); // Adjust for 0-based indexing
      return;
    } else if (receivedMessage == String(i) + " ON") {
      EEPROM.update(outAddr[i - 1], 1); // Set state to ON
      change = true;
      responseMessage = 1;
      return;
    } else if (receivedMessage == String(i) + " OFF") {
      EEPROM.update(outAddr[i - 1], 0); // Set state to OFF
      change = true;
      responseMessage = 1;
      return;
    }
  }
}

void requestResponse() {
    Wire.write((uint8_t)responseMessage);  // Send response to master
}

void mosfetWrite() {
  if (change) {
    for (int i = 0; i < NUM_SWITCHES; i++) {
      int pin = outAddr[i]; // Get the physical pin number from the outAddr array
      if (i == 0) { // CALDIGIT (index 0)
        digitalWrite(pin, EEPROM.read(outAddr[i]) == 1 ? LOW : HIGH); // Invert logic for CALDIGIT
      } else {
        // Invert logic for out2 to out11
        digitalWrite(pin, EEPROM.read(outAddr[i]) == 1 ? LOW : HIGH);
      }
    }
    change = false;
    printOutput();
  }
}

void printOutput() {
    String outputStates = "";
    for (int i = 0; i < NUM_SWITCHES; i++) {
        outputStates += String(EEPROM.read(outAddr[i]));
    }
    Serial.println(outputStates); // Print output states for debugging
}