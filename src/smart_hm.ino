/*
  Smart Home Door Control System

  This program implements a smart home door system using an Arduino-compatible board.
  It uses an MFRC522 RFID reader for access control, a stepper motor to open/close the door,
  a buzzer and RGB LEDs for audio-visual feedback, and a photoresistor to automate lighting.
  The system checks for an authorized RFID card, opens the door if access is granted,
  and automatically closes it after a delay. It also turns on or off room lights based on ambient light.

  Components:
    - MFRC522 RFID Reader
    - Stepper Motor (controlled via Stepper library)
    - Buzzer
    - RGB LEDs (Red and Blue channels)
    - Photoresistor (for light detection)
    - 4 LEDs (for lighting control)
*/


#include <Stepper.h>      // Library for controlling stepper motors
#include <SPI.h>          // SPI communication library (used by RFID)
#include <MFRC522.h>      // Library for MFRC522 RFID reader

// Pin definitions for RFID
#define SDA 10            // RFID SDA pin
#define RST 9             // RFID Reset pin

// Pin definitions for RGB LED and Buzzer
#define RGB_R A4          // Red channel of RGB LED
#define RGB_B 3           // Blue channel of RGB LED
#define BUZZER_PIN 2      // Buzzer pin
#define PHOTO_PIN A5      // Photoresistor pin

// Pin definitions for regular LEDs
#define LED_PIN1 4
#define LED_PIN2 5
#define LED_PIN3 6
#define LED_PIN4 7

const int stepsPerRevolution = 200;  
// Initialize the stepper library on pins A0, A2, A1, A3
Stepper myStepper(stepsPerRevolution, A0, A2, A1, A3);

// RFID and Stepper Initialization
MFRC522 rfid(SDA, RST); 
byte authourizedUID[4] = {0x03, 0x4F, 0x1E, 0xF7}; // Authorized card UID

// Door state machine states
enum DoorState {IDLE, OPENING, WAIT_OPEN, CLOSING};
DoorState doorState = IDLE; // Current state of the door

int lightVal = 0;                      // Stores photoresistor value
bool waitBuzzer = false;               // Not used in logic, can be removed
bool buzzerActive = false;             // Tracks if buzzer is active (access granted)
bool buzzerNoAccessActive = false;     // Tracks if buzzer is active (access denied)
unsigned long buzzerStartTime = 0;     // Start time for buzzer (access granted)
unsigned long buzzerStartTimeNoAccess = 0; // Start time for buzzer (access denied)
unsigned long openTime = 0;            // Time when door was opened
bool cardProcessed = false;            // Prevents multiple triggers per card tap

void setup() {
    SPI.begin();         		// Initialize SPI bus for RFID
    rfid.PCD_Init();     		// Initialize RFID reader
    Serial.begin(9600);         // Start serial communication for debugging
    myStepper.setSpeed(40);     // Set stepper motor speed (RPM)
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RGB_B, OUTPUT);
    pinMode(RGB_R, OUTPUT);
    
    pinMode(LED_PIN1, OUTPUT);
    pinMode(LED_PIN2, OUTPUT);
    pinMode(LED_PIN3, OUTPUT);
    pinMode(LED_PIN4, OUTPUT);
}

void loop() {
    // --- RFID Card Handling ---
    // If no new card is present or can't read card, reset cardProcessed flag
    if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
        cardProcessed = false;
    } 
    // If a new card is detected and not already processed
    else if (!cardProcessed) {
        bool isAuthourized = true;
        // Compare scanned UID with authorized UID
        for (byte i = 0; i < 4; i++) {
            if(authourizedUID[i] != rfid.uid.uidByte[i]) {
                isAuthourized = false;
            }
        }
        // If authorized and door is idle, open the door
        if (isAuthourized && doorState == IDLE) {
            digitalWrite(RGB_B, HIGH);        // Turn on blue LED
            digitalWrite(BUZZER_PIN, HIGH);   // Turn on buzzer
            delay(200);                       // Wait 200ms
            digitalWrite(RGB_B, LOW);         // Turn off blue LED
            digitalWrite(BUZZER_PIN, LOW);    // Turn off buzzer
            myStepper.step(500);              // Open door (steps forward)
            doorState = OPENING;              // Change state to OPENING
            cardProcessed = true;             // Mark card as processed
        } 
        // If not authorized, trigger access denied feedback
        else if (!isAuthourized) {
            digitalWrite(BUZZER_PIN, HIGH);   // Turn on buzzer
            digitalWrite(RGB_R, HIGH);        // Turn on red LED
            buzzerStartTimeNoAccess = millis(); // Start timing for denied beep
            buzzerNoAccessActive = true;      // Set denied buzzer active
            cardProcessed = true;             // Mark card as processed
        }
        rfid.PCD_StopCrypto1();               // Stop encryption on RFID
        rfid.PICC_HaltA();                    // Halt RFID card
    }
    // Access denied: beep pattern for denied access
    if (buzzerNoAccessActive) {
        unsigned long elapsed = millis() - buzzerStartTimeNoAccess;
        if (elapsed < 100) {
            digitalWrite(BUZZER_PIN, HIGH);
        } else if (elapsed < 200) {
            digitalWrite(BUZZER_PIN, LOW);
        } else if (elapsed < 300) {
            digitalWrite(BUZZER_PIN, HIGH);
        } else {
            digitalWrite(RGB_R, LOW); 
            digitalWrite(BUZZER_PIN, LOW);
            buzzerNoAccessActive = false;
        }
    }

    // --- Door State Machine ---
    switch (doorState) {
        case OPENING:
            openTime = millis();              // Record time door opened
            doorState = WAIT_OPEN;            // Move to WAIT_OPEN state
            digitalWrite(BUZZER_PIN, HIGH);   // Short beep for open
            delay(300);
            digitalWrite(BUZZER_PIN, LOW);
        break;
        case WAIT_OPEN:
            // Wait 5 seconds before closing
            if (millis() - openTime >= 5000) {
                waitBuzzer = true;
                myStepper.step(-500);         // Close door (steps backward)
                doorState = CLOSING;          // Move to CLOSING state
            }
        break;
        case CLOSING:
            doorState = IDLE;                 // Return to idle after closing
        break;
        case IDLE:
        default:
        break;
    }

    // --- Light Automation Using Photoresistor ---
    lightVal = analogRead(PHOTO_PIN);         // Read light level
    if (lightVal <= 200) {                    // If dark
        digitalWrite(LED_PIN1, HIGH);
        digitalWrite(LED_PIN2, HIGH);
        digitalWrite(LED_PIN3, HIGH);
        digitalWrite(LED_PIN4, HIGH);
    } else {                                  // If bright
        digitalWrite(LED_PIN1, LOW);
        digitalWrite(LED_PIN2, LOW);
        digitalWrite(LED_PIN3, LOW);
        digitalWrite(LED_PIN4, LOW);
    }
}