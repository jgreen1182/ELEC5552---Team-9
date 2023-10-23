#include <Arduino.h>
#include <LiquidCrystal.h>
#include <SPI.h>

// LCD initialization with new pin assignments
LiquidCrystal lcd(A5, A4, A3, 6, 7, 5);  // Reassigned LCD pins

// Pins for rotary encoder
#define PWM_CLK 8
#define PWM_DT 9
#define SW 10

// Pins for buttons
#define AC_BUTTON_PIN A0
#define MOD_BUTTON_PIN A1

// Pins for relays and digital potentiometer
const int LM317_RELAY_PIN = 3;   // Moved to pin 3
const int BOOST_RELAY_PIN = 4;   // Moved to pin 4
const uint8_t CSpin = 2;         // Moved to pin 2 for digital potentiometer CS


// Constants for LM317
const float V_REF = 1.25;
const float I_ADJ = 50e-6;
const float R1 = 421.05;

// Variables
float desiredVoltage = 1.4;
bool fineAdjust = true;
bool isACMode = false;
bool isModulationOn = false;
int lastButtonState = HIGH;
int lastACButtonState = HIGH;
int lastMODButtonState = HIGH;
uint16_t potValue = 0;

void setup() {
    pinMode(PWM_CLK, INPUT);
    pinMode(PWM_DT, INPUT);
    pinMode(SW, INPUT_PULLUP);
    pinMode(AC_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MOD_BUTTON_PIN, INPUT_PULLUP);
    pinMode(LM317_RELAY_PIN, OUTPUT);
    pinMode(BOOST_RELAY_PIN, OUTPUT);
    pinMode(CSpin, OUTPUT);
    pinMode(10, OUTPUT);  // Ensure the board remains in SPI master mode

    Serial.begin(9600);
    lcd.begin(16, 2);
    SPI.begin();
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

    //Serial.println(data);
}


void update_digipot(uint8_t reg, uint16_t value) {
    union {
        uint16_t val;
        uint8_t bytes[2];
    } in;

    in.val = value << 6;

    digitalWrite(CSpin, LOW);
    SPI.transfer(reg);
    SPI.transfer(in.bytes[1]);
    SPI.transfer(in.bytes[0]);
    digitalWrite(CSpin, HIGH);
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
        if (desiredVoltage < 1.4) desiredVoltage = 1.4;
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
    float R2;
    bool isLM317;

    if (desiredVoltage <= 30.0) {
        R2 = (desiredVoltage - 1.374) / 0.0028;  
        R2 = constrain(R2, 0, 10000);  // constrain between 0 and 10k
        potValue = map(R2, 0, 10000, 0, 1023);
        potValue = constrain(potValue, 0, 1023);
        update_digipot(0x02, potValue);
        isLM317 = true;

        // Print potValue to Serial
        Serial.print("potValue: ");
        Serial.println(potValue);
        delay(100);
        // New code to read and print the wiper voltage
        Serial.print("R2: "); Serial.print(((analogRead(A2)*5/1023.0)/3.3)*10000,2); Serial.println("Ohms");

    } else {
        dutyCycle = (100.0 * log(desiredVoltage / 27.198)) / 2.7162;
        isLM317 = false;

        // Convert duty cycle directly to voltage
        float voltageOutput = map(dutyCycle, 0, 100, 0, 3.3 * 100) / 100.0;

        // Print voltage value to Serial
        Serial.print("Voltage for duty cycle: ");
        Serial.println(voltageOutput, 2);
    }

    dutyCycle = constrain(dutyCycle, 0, 100);

    lcd.clear();
    lcd.setCursor(0, 0);
    if (isLM317) {
        lcd.print("R:");
        lcd.print(R2, 2);
        lcd.print("");
    } else {
        lcd.print("Duty:");
        lcd.print(dutyCycle, 2);
        lcd.print("%");
    }
    
    lcd.setCursor(11, 0);
    lcd.print(isLM317 ? "LM317" : "BOOST");

    lcd.setCursor(0, 1);
    lcd.print("V:");
    lcd.print(desiredVoltage, 2);
    lcd.print("V");
    lcd.setCursor(10, 1);
    lcd.print(isACMode ? "AC" : "DC");
    lcd.setCursor(13, 1);
    lcd.print(isModulationOn ? "Mod" : "");

    digitalWrite(LM317_RELAY_PIN, isLM317 ? HIGH : LOW);
    digitalWrite(BOOST_RELAY_PIN, isLM317 ? LOW : HIGH);
}

void loop() {
    readEncoder();
    handleSwitchDebounce();
    handleACButton();
    handleModButton();
}
