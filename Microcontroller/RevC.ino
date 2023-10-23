#include <Arduino.h>
#include <LiquidCrystal.h>

// LCD initialization
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Pins
#define PWM_CLK 8
#define PWM_DT 9
#define SW 10
#define AC_BUTTON_PIN A0
#define MOD_BUTTON_PIN A1
const int BUCK_RELAY_PIN = 6;
const int BOOST_RELAY_PIN = 7;

// Variables
float desiredVoltage = 1.0;
bool fineAdjust = true;
bool isACMode = false;
bool isModulationOn = false;
int lastButtonState = HIGH;
int lastACButtonState = HIGH;
int lastMODButtonState = HIGH;

void setup() {
    pinMode(PWM_CLK, INPUT);
    pinMode(PWM_DT, INPUT);
    pinMode(SW, INPUT_PULLUP);
    pinMode(AC_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MOD_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUCK_RELAY_PIN, OUTPUT);
    pinMode(BOOST_RELAY_PIN, OUTPUT);

    Serial.begin(9600);
    lcd.begin(16, 2);
}

void sendSerialData() {
    String data = isACMode ? "1" : "0"; 
    data += isModulationOn ? "1" : "0";
    
    String voltageStr = String((int)(desiredVoltage * 100));
    while(voltageStr.length() < 5) {
        voltageStr = "0" + voltageStr;
    }
    data += voltageStr;
    data += "000000";  // Current placeholder
    data += "000";     // Frequency placeholder
    data += "00";      // Modulation frequency placeholder

    Serial.println(data);
}

void readEncoder() {
    static int lastStatePWMCLK = LOW;
    int currentStatePWMCLK = digitalRead(PWM_CLK);
    
    if (currentStatePWMCLK != lastStatePWMCLK && currentStatePWMCLK == 1) {
        float increment = (fineAdjust) ? 0.05 : 1.0;
        if (digitalRead(PWM_DT) == 0) {
            desiredVoltage += increment;
        } else {
            desiredVoltage -= increment;
        }

        desiredVoltage = round(desiredVoltage * 20) / 20.0;  // Ensure it's rounded to nearest 0.05

        if (desiredVoltage > 200) desiredVoltage = 200;
        if (desiredVoltage < 0) desiredVoltage = 0;
        displayOutput();
        sendSerialData();
    }
    lastStatePWMCLK = currentStatePWMCLK;
}

void handleSwitchDebounce() {
    int buttonState = digitalRead(SW);
    if (buttonState == LOW && lastButtonState == HIGH) {
        fineAdjust = !fineAdjust;
        delay(50); // debounce delay
    }
    lastButtonState = buttonState;
}

void handleACButton() {
    int acButtonState = digitalRead(AC_BUTTON_PIN);
    if (acButtonState == LOW && lastACButtonState == HIGH) {
        isACMode = !isACMode;
        sendSerialData();
        displayOutput();
        delay(1000); 
    }
    lastACButtonState = acButtonState;
}

void handleModButton() {
    int modButtonState = digitalRead(MOD_BUTTON_PIN);
    if (modButtonState == LOW && lastMODButtonState == HIGH) {
        isModulationOn = !isModulationOn;
        sendSerialData();
        displayOutput();
        delay(1000); 
    }
    lastMODButtonState = modButtonState;
}

void displayOutput() {
    float dutyCycle;
    bool isBuck;
    if (desiredVoltage <= 30.0) {
        dutyCycle = (desiredVoltage - 0.7811) / 0.3019;
        isBuck = true;
    } else {
        dutyCycle = (100.0 * log(desiredVoltage / 27.198)) / 2.7162;
        isBuck = false;
    }

    dutyCycle = constrain(dutyCycle, 0, 100);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("D:");
    lcd.print(dutyCycle, 4);
    lcd.print("%");
    lcd.setCursor(11, 0);
    lcd.print(isBuck ? "BUCK" : "BOOST");

    lcd.setCursor(0, 1);
    lcd.print("V:");
    lcd.print(desiredVoltage, 2);
    lcd.print("V");
    lcd.setCursor(10, 1);
    lcd.print(isACMode ? "AC" : "DC");
    lcd.setCursor(13, 1);
    lcd.print(isModulationOn ? "Mod" : "");

    digitalWrite(BUCK_RELAY_PIN, isBuck ? HIGH : LOW);
    digitalWrite(BOOST_RELAY_PIN, isBuck ? LOW : HIGH);
}

void loop() {
    readEncoder();
    handleSwitchDebounce();
    handleACButton();
    handleModButton();
}
