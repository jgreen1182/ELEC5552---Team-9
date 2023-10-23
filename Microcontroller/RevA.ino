#include <Arduino.h>
#include <LiquidCrystal.h>

// Assuming RS is connected to pin 12, E to pin 11, D4 to pin 5, D5 to pin 4, D6 to pin 3, and D7 to pin 2
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Rotary Encoder for PWM Control Inputs
#define PWM_CLK 8
#define PWM_DT 9
#define SW 10 // Rotary encoder push switch
// #define DAC_PIN DAC0 // Commented out for now

const int BUCK_LED_PIN = 6;  // Digital pin for buck mode LED
const int BOOST_LED_PIN = 7; // Digital pin for boost mode LED

const int BUCK_MAX = 600;
const int BOOST_MAX = 3400;
const int FINE_ADJUSTMENT = 1;   // 0.05V steps
const int LARGER_ADJUSTMENT = 20; // 1V steps

int pwmCounter = 0; 
int currentStatePWMCLK;
int lastStatePWMCLK = LOW;
bool isBuck = true; 
bool fineAdjust = true;
int lastButtonState = HIGH;  // For debouncing the switch

bool encoderStateChanged = false; // Initialize the flag

void setup() {
    pinMode(PWM_CLK, INPUT);
    pinMode(PWM_DT, INPUT);
    pinMode(SW, INPUT_PULLUP);

    // Set LED pins as OUTPUT
    pinMode(BUCK_LED_PIN, OUTPUT);
    pinMode(BOOST_LED_PIN, OUTPUT);

    currentStatePWMCLK = digitalRead(PWM_CLK);

    Serial.begin(9600);  // Initialize Serial Monitor
    lcd.begin(16, 2); // Initialize 16x2 LCD
}

int readEncoder() {
    currentStatePWMCLK = digitalRead(PWM_CLK);

    if (currentStatePWMCLK != lastStatePWMCLK && currentStatePWMCLK == 1) {
        if (digitalRead(PWM_DT) == 0) {
            pwmCounter += (fineAdjust) ? FINE_ADJUSTMENT : LARGER_ADJUSTMENT;
        } else {
            pwmCounter -= (fineAdjust) ? FINE_ADJUSTMENT : LARGER_ADJUSTMENT;
        }

        // Mode switching and boundary checking
        if (isBuck) {
            if (pwmCounter > BUCK_MAX) {
                pwmCounter = 0; // Reset counter for boost mode
                isBuck = false;
            } else if (pwmCounter < 0) {
                pwmCounter = 0; 
            }
        } else {
            if (pwmCounter > BOOST_MAX) {
                pwmCounter = BOOST_MAX;
            } else if (pwmCounter < 0) {
                pwmCounter = BUCK_MAX; // Set counter to BUCK_MAX for buck mode
                isBuck = true;
            }
        }

        encoderStateChanged = true; // Set the flag
    }
    lastStatePWMCLK = currentStatePWMCLK;
}

void handlePWLEncoder() {
    readEncoder();
    //printOutput();
}

void handleSwitchDebounce() {
    int buttonState = digitalRead(SW);
    if (buttonState == LOW && lastButtonState == HIGH) {
        fineAdjust = !fineAdjust;
        Serial.print("Switch State: ");
        Serial.println(fineAdjust ? "FINE" : "LARGER");
        delay(50); // Debounce delay
    }
    lastButtonState = buttonState;
}

void loop() {
    readEncoder(); // Handle encoder input with debouncing
    handleSwitchDebounce();
    
    // Check if the encoder state has changed
    if (encoderStateChanged) {
        encoderStateChanged = false; // Clear the flag
        Serial.print("Mode: ");
        Serial.print(isBuck ? "BUCK\t" : "BOOST\t");

        // Calculate duty cycle
        float dutyCycle = (float)pwmCounter / (isBuck ? BUCK_MAX : BOOST_MAX) * 100.0;

        // Calculate output voltage
        float outputVoltage;
        if (isBuck) {
            outputVoltage = (float)pwmCounter / BUCK_MAX * 30.0;  // Scale according to the 0-30V range for buck
        } else {
            outputVoltage = 30 + (float)(pwmCounter) * FINE_ADJUSTMENT / (BOOST_MAX - BUCK_MAX) * 170.0; // Start at 30V + scale according to the 30-200V range for boost
        }
        
        // Now, update the LCD:
        // Now, update the LCD:
        lcd.clear();  // Clear the LCD screen

        // Print the mode and duty cycle on the first row
        lcd.setCursor(0, 0);  // Set cursor to the top-left
        lcd.print(isBuck ? "BUCK " : "BOOST");
        lcd.print(dutyCycle, 2);
        lcd.print("%");

        // Print the output voltage on the second row
        lcd.setCursor(0, 1);  // Set cursor to the start of the second row
        lcd.print("V: ");
        lcd.print(outputVoltage, 2);
        lcd.print("V");

        // Control LEDs based on the current mode
        digitalWrite(BUCK_LED_PIN, isBuck ? HIGH : LOW);
        digitalWrite(BOOST_LED_PIN, isBuck ? LOW : HIGH);
    }
}
