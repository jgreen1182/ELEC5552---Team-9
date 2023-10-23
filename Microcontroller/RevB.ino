#include <Arduino.h>
#include <LiquidCrystal.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

#define PWM_CLK 8
#define PWM_DT 9
#define SW 10

const int BUCK_LED_PIN = 6;
const int BOOST_LED_PIN = 7;

const int FINE_ADJUSTMENT = 1;   // 0.05V steps
const int LARGER_ADJUSTMENT = 20; // 1V steps

float desiredVoltage = 1.0;
bool fineAdjust = true;
int lastButtonState = HIGH;

void setup() {
    pinMode(PWM_CLK, INPUT);
    pinMode(PWM_DT, INPUT);
    pinMode(SW, INPUT_PULLUP);

    pinMode(BUCK_LED_PIN, OUTPUT);
    pinMode(BOOST_LED_PIN, OUTPUT);

    Serial.begin(9600);
    lcd.begin(16, 2);
}

void readEncoder() {
    static int lastStatePWMCLK = LOW;
    int currentStatePWMCLK = digitalRead(PWM_CLK);
    
    if (currentStatePWMCLK != lastStatePWMCLK && currentStatePWMCLK == 1) {
        if (digitalRead(PWM_DT) == 0) {
            desiredVoltage += (fineAdjust) ? 0.05 : 1.0;
        } else {
            desiredVoltage -= (fineAdjust) ? 0.05 : 1.0;
        }

        // Boundary checks
        if (desiredVoltage > 200) desiredVoltage = 200;
        if (desiredVoltage < 0) desiredVoltage = 0;

        displayOutput();
    }
    lastStatePWMCLK = currentStatePWMCLK;
}

void handleSwitchDebounce() {
    int buttonState = digitalRead(SW);
    if (buttonState == LOW && lastButtonState == HIGH) {
        fineAdjust = !fineAdjust;
        delay(50); // Debounce delay
    }
    lastButtonState = buttonState;
}

void displayOutput() {
    float dutyCycle = 0.0;
    bool isBuck = true;

    if (desiredVoltage <= 30.0) {
        dutyCycle = (desiredVoltage - 0.7811) / 0.3019;
        isBuck = true;
    } else {
        dutyCycle = log((desiredVoltage / 27.198)) / 0.2716;
        isBuck = false;
    }

    lcd.clear(); // Clear the LCD first

    // Display duty cycle on the LCD's first row
    lcd.setCursor(0, 0);
    lcd.print("D:");
    lcd.print(dutyCycle, 4);
    lcd.print("%");

    // Adjust cursor position to display mode at the end of the first row
    lcd.setCursor(11, 0); 
    lcd.print(isBuck ? "BUCK" : "BOOST");

    // Display desired voltage on the second row
    lcd.setCursor(0, 1);
    lcd.print("V:");
    lcd.print(desiredVoltage, 2);
    lcd.print("V");

    digitalWrite(BUCK_LED_PIN, isBuck ? HIGH : LOW);
    digitalWrite(BOOST_LED_PIN, isBuck ? LOW : HIGH);
}

void loop() {
    readEncoder();
    handleSwitchDebounce();
}
