/*
Power Distribution Box Master-Microcontroller software.
Property of Mission Digital.
All rights reserved.
Available for Power Distribution Box version 1.0UC; 1.0; BETA1.0 and BETA1.0UC and PDBv1.0.0.0
Written by: inż. Adrian Kieżel
Adjusted for DistroBox v4 by Eng. Mostafa Abdelghaffar
*/

//-- Libraries --
#include <LiquidCrystal_I2C.h> // LCD library
#include <Wire.h>              // I2C Library
#include <Bounce2.h>           // Debouncing library
#include <OneButton.h>         // Button handling library
#include <EEPROM.h>            // EEPROM library

//-- Menu Size Definitions --
#define TOP_MENU_ITEMS 3  // Number of top menu items
#define BOT_MENU_ITEMS 25 // Number of bottom menu items

//-- LCD I2C Address --
#define LCD_ADDR 0x27 // Double-check address!!!

//-- Menu Names --
const String mainMenu[TOP_MENU_ITEMS] = {"-", "Pwr line ctrl:", "Output ctrl:"}; // Top menu options
const String botMenu[BOT_MENU_ITEMS] = {"-", "-", "-", "-", "-", "-", "-", "-", "Mains1: ", "Bat 1: ", "Mains2: ", "Bat 2: ", "Mains3: ", "Bat3: ",
                                        "CALDIGIT", "out2", "out3", "out4", "out5", "out6", "out7", "out8", "out9", "out10", "out11"}; // Bottom submenu options

//-- I2C Communication Setup --
#define TARGET_ADDRESS 0x09 // I2C address of the slave device

//-- EEPROM Addresses --
#define RELAY1_ADDR 1 // Relay1 state
#define RELAY2_ADDR 2 // Relay2 state
#define RELAY3_ADDR 3 // Relay3 state
#define RELAY4_ADDR 4 // Relay4 state
#define RELAY5_ADDR 5 // Relay5 state
#define RELAY6_ADDR 6 // Relay6 state

//-- Pin Definitions --
#define BTN_TL 12            // Top Left button
#define BTN_TR 11            // Top Right button
#define BTN_BL 10            // Bottom Left button
#define BTN_BR 5             // Bottom Right button
#define BTN_OK 4             // OK button
#define DEBOUNCE_INTERVAL 10 // Debounce interval (ms)

//-- I2C Communication Variables --
//int incomingMsg;        // Stores incoming I2C message // NOT USED 
String checkMessage;    // Message to request state (e.g., "check 2")
String controlMessage;  // Message to control state (e.g., "2 OFF")
String incomingConfirm; // Confirmation from slave

//-- LCD Setup --
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2); // Initialize LCD

//-- Button Setup --
OneButton btnOK(BTN_OK, false, false);         // Initialize OK button
Bounce bncBtnTL, bncBtnTR, bncBtnBL, bncBtnBR; // Debounced buttons

//-- Menu State Variables --
int menu[2] = {1, 8}; // [Top menu index, Bottom menu index]
bool change = true;   // Flag to indicate menu change
bool outputStates[11] = {false}; // Array to store the state of each output (11 outputs)

//-- Setup Function --
void setup() {
  // Initialize relay pins as OUTPUT
  pinMode(6, OUTPUT);  // Relay1 (Mains1)
  pinMode(7, OUTPUT);  // Relay2 (Bat 1)
  pinMode(8, OUTPUT);  // Relay3 (Mains2)
  pinMode(9, OUTPUT);  // Relay4 (Bat 2)
  pinMode(2, OUTPUT);  // Relay5 (Mains3)
  pinMode(13, OUTPUT); // Relay6 (Bat3)

  // Initialize relay states from EEPROM
  digitalWrite(6, EEPROM.read(RELAY1_ADDR));  // Mains1: Relay1 → D6
  digitalWrite(7, EEPROM.read(RELAY2_ADDR));  // Bat 1: Relay2 → D7
  digitalWrite(8, EEPROM.read(RELAY3_ADDR));  // Mains2: Relay3 → D8
  digitalWrite(9, EEPROM.read(RELAY4_ADDR));  // Bat 2: Relay4 → D9
  digitalWrite(2, EEPROM.read(RELAY5_ADDR));  // Mains3: Relay5 → D2
  digitalWrite(13, EEPROM.read(RELAY6_ADDR)); // Bat3: Relay6 → D13

  Wire.begin();    // Initialize I2C communication
  lcd.init();      // Initialize LCD
  lcd.backlight(); // Turn on LCD backlight
  lcd.clear();     // Clear LCD screen

  // Initialize buttons with debouncing
  bncBtnTL.attach(BTN_TL, INPUT);
  bncBtnTR.attach(BTN_TR, INPUT);
  bncBtnBL.attach(BTN_BL, INPUT);
  bncBtnBR.attach(BTN_BR, INPUT);
  bncBtnTL.interval(DEBOUNCE_INTERVAL);
  bncBtnTR.interval(DEBOUNCE_INTERVAL);
  bncBtnBL.interval(DEBOUNCE_INTERVAL);
  bncBtnBR.interval(DEBOUNCE_INTERVAL);

  // Attach OK button click event
  btnOK.attachClick(handleOKClick);

  // Initialize output states
  for (int i = 0; i < 11; i++) {
    I2C_request(i); // Request the state of each output
  }

  // Initial menu display
  menuDisplay(menu[0], menu[1]);
}

//-- Main Loop --
void loop() {
  unsigned long currentTime = millis();
  static unsigned long lastLCD = 0;

  // Update button states
  bncBtnTL.update();
  bncBtnTR.update();
  bncBtnBL.update();
  bncBtnBR.update();
  btnOK.tick();

  // Handle button presses
  if (bncBtnTL.fell()) { // Top Left: Previous top menu
    menu[0] = (menu[0] - 1 + TOP_MENU_ITEMS) % TOP_MENU_ITEMS;
    if (menu[0] == 0) menu[0] = TOP_MENU_ITEMS - 1; // Skip the first "-" item
    resetBottomMenu();
    change = true;
  }
  if (bncBtnTR.fell()) { // Top Right: Next top menu
    menu[0] = (menu[0] + 1) % TOP_MENU_ITEMS;
    if (menu[0] == 0) menu[0] = 1; // Skip the first "-" item
    resetBottomMenu();
    change = true;
  }
  if (bncBtnBL.fell()) { // Bottom Left: Previous bottom menu
    menu[1] = getPreviousBottomMenuIndex();
    change = true;
  }
  if (bncBtnBR.fell()) { // Bottom Right: Next bottom menu
    menu[1] = getNextBottomMenuIndex();
    change = true;
  }

  // Refresh LCD every 200ms
  if (currentTime - lastLCD > 200) {
    menuDisplay(menu[0], menu[1]);
    lastLCD = currentTime;
  }
}

//-- Reset Bottom Menu Based on Top Menu --
void resetBottomMenu() {
  switch (menu[0]) {
    case 1:        // "Pwr line ctrl:"
      menu[1] = 8; // Start at "Mains1: "
      break;
    case 2:         // "Output ctrl:"
      menu[1] = 14; // Start at "CALDIGIT"
      break;
    default:
      menu[1] = 0; // Default to "-"
      break;
  }
}

//-- Get Next Bottom Menu Index --
int getNextBottomMenuIndex() {
  int nextIndex = menu[1] + 1;
  switch (menu[0]) {
    case 1: // "Pwr line ctrl:"
      if (nextIndex > 13) nextIndex = 8; // Wrap around to "Mains1: "
      break;
    case 2: // "Output ctrl:"
      if (nextIndex > 24) nextIndex = 14; // Wrap around to "CALDIGIT"
      break;
    default:
      nextIndex = 0; // Default to "-"
      break;
  }
  return nextIndex;
}

//-- Get Previous Bottom Menu Index --
int getPreviousBottomMenuIndex() {
  int prevIndex = menu[1] - 1;
  switch (menu[0]) {
    case 1: // "Pwr line ctrl:"
      if (prevIndex < 8) prevIndex = 13; // Wrap around to "Bat3: "
      break;
    case 2: // "Output ctrl:"
      if (prevIndex < 14) prevIndex = 24; // Wrap around to "out11"
      break;
    default:
      prevIndex = 0; // Default to "-"
      break;
  }
  return prevIndex;
}

//-- OK Button Handler --
void handleOKClick() {
  if (menu[0] == 1) { // "Pwr line ctrl:"
    int relayAddr = RELAY1_ADDR + (menu[1] - 8); // Calculate EEPROM address
    bool state = !EEPROM.read(relayAddr); // Toggle the state
    EEPROM.update(relayAddr, state); // Update EEPROM
    relayWrite(); // Update the physical relay pins
    change = true; // Force LCD update
  } else if (menu[0] == 2) { // "Output ctrl:"
    int outputIndex = menu[1] - 14; // Adjust indexing: CALDIGIT = 0, out2 = 1, ..., out11 = 10
    if (outputIndex >= 0 && outputIndex < 11) { // Ensure the index is valid
      // Determine the new state (ON or OFF)
      bool newState = !outputStates[outputIndex]; // Toggle the state locally
      outputStates[outputIndex] = newState; // Update the local state

      // Send the command to the slave
      controlMessage = String(outputIndex + 1) + (newState ? " ON" : " OFF");
      I2C_command();

      // Update the LCD display
      change = true; // Force LCD update
    }
  }
}

//-- Menu Display Function --
void menuDisplay(int top, int bot) {
  if (change) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(mainMenu[top]);
    lcd.setCursor(0, 1);
    lcd.print(botMenu[bot]);

    // Display ON/OFF state for selected item
    if (top == 1) { // "Pwr line ctrl:"
      lcd.setCursor(13, 1);
      int relayAddr = RELAY1_ADDR + (bot - 8); // Calculate EEPROM address
      lcd.print(EEPROM.read(relayAddr) ? "OFF" : "ON ");
    } else if (top == 2) { // "Output ctrl:"
      lcd.setCursor(13, 1);
      int outputIndex = bot - 14; // Calculate output index (14 -> 0, 15 -> 1, etc.)
      if (outputIndex >= 0 && outputIndex < 11) { // Ensure the index is valid
        lcd.print(outputStates[outputIndex] ? "OFF" : "ON ");
      }
    }
    change = false;
  }
}

//-- I2C Command Function --
void I2C_request(int outputIndex) {
  checkMessage = "check " + String(outputIndex + 1); // +1 because outputs start from 1
  Wire.beginTransmission(TARGET_ADDRESS);
  Wire.write(checkMessage.c_str());
  if (Wire.endTransmission() != 0) {
    return; // Transmission failed
  }

  Wire.requestFrom(TARGET_ADDRESS, 1);
  unsigned long startTime = millis();
  while (!Wire.available() && (millis() - startTime < 1000)) {
    // Wait for data with a timeout of 1 second
  }

  if (Wire.available()) {
    int state = Wire.read();
    outputStates[outputIndex] = (state == 1); // Update the state in the array
  }
}

void I2C_command() {
  Wire.beginTransmission(TARGET_ADDRESS);
  Wire.write(controlMessage.c_str());
  if (Wire.endTransmission() != 0) {
    return;
  }

  Wire.requestFrom(TARGET_ADDRESS, 1);
  if (Wire.available()) {
    incomingConfirm = "";
    while (Wire.available()) {
      incomingConfirm += (char)Wire.read();
    }
  }
  if (incomingConfirm != "0") {
    change = true;
  }
}

//-- Relay Control Function --
void relayWrite() {
  digitalWrite(6, EEPROM.read(RELAY1_ADDR));  // Mains1: Relay1 → D6
  digitalWrite(7, EEPROM.read(RELAY2_ADDR));  // Bat 1: Relay2 → D7
  digitalWrite(8, EEPROM.read(RELAY3_ADDR));  // Mains2: Relay3 → D8
  digitalWrite(9, EEPROM.read(RELAY4_ADDR));  // Bat 2: Relay4 → D9
  digitalWrite(2, EEPROM.read(RELAY5_ADDR));  // Mains3: Relay5 → D2
  digitalWrite(13, EEPROM.read(RELAY6_ADDR)); // Bat3: Relay6 → D13
}