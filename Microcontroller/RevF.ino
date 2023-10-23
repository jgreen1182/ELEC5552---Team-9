#include <LiquidCrystal.h>
#include <SPI.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Pin assignments for voltage encoder
const int VOLTAGE_CLK_PIN = 48;
const int VOLTAGE_DT_PIN = 50;
const int VOLTAGE_BTN_PIN = 52;

// Pin assignments for frequency encoder
const int FREQUENCY_CLK_PIN = 42;
const int FREQUENCY_DT_PIN = 44;
const int FREQUENCY_BTN_PIN = 46;

float desiredVoltage = 1.4;
int desiredFrequency = 50000;  // In kHz

bool fineAdjustVoltage = false;
bool fineAdjustFrequency = false;

// Button Pin assignments
const int AC_BUTTON_PIN = 38;
const int MOD_BUTTON_PIN = 40;
bool isACMode = false;  // Start in DC mode
bool isModulationOn = false;
int lastACButtonState = HIGH;
int lastMODButtonState = HIGH;

const int RELAY1_PIN = 51;  // Control pin for 0-30V regulator relay
const int RELAY2_PIN = 53;  // Control pin for 30-200V boost relay

// Chip select for 10k digipot
const int CSpin1 = 22;  

uint16_t lastDacValue = 0;

// Interlock pin
const int INTERLOCK_PIN = A2;

void setup() {
    Serial.begin(9600);
    SPI.begin();
    lcd.begin(16, 2);  // Initialize the 16x2 LCD
    lcd.clear();
    
    // For voltage encoder
    pinMode(VOLTAGE_CLK_PIN, INPUT);
    pinMode(VOLTAGE_DT_PIN, INPUT);
    pinMode(VOLTAGE_BTN_PIN, INPUT_PULLUP);
    
    // For frequency encoder
    pinMode(FREQUENCY_CLK_PIN, INPUT);
    pinMode(FREQUENCY_DT_PIN, INPUT);
    pinMode(FREQUENCY_BTN_PIN, INPUT_PULLUP);

    // For AC/DC and Mod/ NO Mod on/off
    pinMode(AC_BUTTON_PIN, INPUT_PULLUP);
    pinMode(MOD_BUTTON_PIN, INPUT_PULLUP);

    // For Relays
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    // Initially set relays
    updateRelays();

    pinMode(CSpin1, OUTPUT);

    // For interlock
    pinMode(INTERLOCK_PIN, INPUT);

    analogWriteResolution(12);
    analogReadResolution(12);
}

void updateRelays() {
  if (desiredVoltage <= 30) {
    digitalWrite(RELAY1_PIN, HIGH);  // Turn on Relay 1 for 0-30V regulator
    digitalWrite(RELAY2_PIN, LOW);   // Turn off Relay 2 for 30-200V boost
  } else {
    digitalWrite(RELAY1_PIN, LOW);   // Turn off Relay 1 for 0-30V regulator
    digitalWrite(RELAY2_PIN, HIGH);  // Turn on Relay 2 for 30-200V boost
  }
}

bool isInterlockActive() {
    float voltageReading = analogRead(INTERLOCK_PIN) * 3.3 / 4095.0; 
    return voltageReading < 3.0;  // if no voltage then interlock is active
}

void readVoltageEncoder() {
    static int lastStateVoltageCLK = LOW;
    int currentStateVoltageCLK = digitalRead(VOLTAGE_CLK_PIN);

    if (currentStateVoltageCLK != lastStateVoltageCLK && currentStateVoltageCLK == HIGH) {
        float increment = (fineAdjustVoltage) ? 0.05 : 1.0;
        if (digitalRead(VOLTAGE_DT_PIN) == LOW) {
            desiredVoltage += increment;
        } else {
            desiredVoltage -= increment;
        }
        desiredVoltage = round(desiredVoltage * 20) / 20.0;
        
        // Check if the interlock is active and if so limit to 100V
        if (isInterlockActive()) {
            desiredVoltage = constrain(desiredVoltage, 1.4, 100.0);
        } else {
            desiredVoltage = constrain(desiredVoltage, 1.4, 200.0);
        }
    }
    lastStateVoltageCLK = currentStateVoltageCLK;

    // Check if the button is pressed to toggle fine adjust
    if (digitalRead(VOLTAGE_BTN_PIN) == LOW) {
        fineAdjustVoltage = !fineAdjustVoltage;
        delay(300);  // Simple debounce
    }
}

void readFrequencyEncoder() {
    static int lastStateFrequencyCLK = LOW;
    int currentStateFrequencyCLK = digitalRead(FREQUENCY_CLK_PIN);

    if (isACMode) {
        if (currentStateFrequencyCLK != lastStateFrequencyCLK && currentStateFrequencyCLK == HIGH) {
            int increment = (fineAdjustFrequency) ? 1000 : 10000;  // Change increment values to Hz
            if (digitalRead(FREQUENCY_DT_PIN) == LOW) {
                desiredFrequency += increment;
            } else {
                desiredFrequency -= increment;
            }
            desiredFrequency = constrain(desiredFrequency, 50000, 300000);  // Ensure it stays within the 50kHz to 300kHz range
        }
        lastStateFrequencyCLK = currentStateFrequencyCLK;

        // Check if the button is pressed to toggle fine adjust
        if (digitalRead(FREQUENCY_BTN_PIN) == LOW) {
            fineAdjustFrequency = !fineAdjustFrequency;
            delay(300);  // Simple debounce
        }
    }
}

void handleACButton() {
    int acButtonState = digitalRead(AC_BUTTON_PIN);
    if (acButtonState == LOW && lastACButtonState == HIGH) {
        isACMode = !isACMode;
        delay(300);  // Debounce delay
    }
    lastACButtonState = acButtonState;
}

void handleModButton() {
    int modButtonState = digitalRead(MOD_BUTTON_PIN);
    if (modButtonState == LOW && lastMODButtonState == HIGH) {
        isModulationOn = !isModulationOn;
        delay(300);  // Debounce delay
    }
    lastMODButtonState = modButtonState;
}

void sendSerialData() {
    String data = isACMode ? "1" : "0"; 
    data += isModulationOn ? "1" : "0";
    
    // Round the desiredVoltage to the nearest 0.05
    desiredVoltage = round(desiredVoltage * 20) / 20.0;

    String voltageStr = String((int)(desiredVoltage * 100));
    while (voltageStr.length() < 5) {
        voltageStr = "0" + voltageStr;
    }
    data += voltageStr;

    data += "000000";  // Current placeholder

    if (isACMode) {
        // Convert the desiredFrequency from Hz to kHz
        int frequencyKHz = desiredFrequency / 1000;
        String frequencyStr = String(frequencyKHz);
        while (frequencyStr.length() < 3) {
            frequencyStr = "0" + frequencyStr;
        }
        data += frequencyStr;
    } else {
        data += "000";  // Frequency part is "000" in DC mode
    }
    
    data += "00";  // Placeholder for additional data

    //Serial.println(data);
}

void update_digipot(uint8_t reg, uint16_t value) {
    union {
        uint16_t val;
        uint8_t bytes[2];
    } in;

    in.val = value << 6;

    digitalWrite(CSpin1, LOW);
    SPI.transfer(reg);
    SPI.transfer(in.bytes[1]);
    SPI.transfer(in.bytes[0]);
    digitalWrite(CSpin1, HIGH);
}

void displayValues() {
    lcd.setCursor(0, 0);
    lcd.print("V: ");
    lcd.print(desiredVoltage, 2);  // Display with two decimal places
    lcd.print(" V   ");  // Extra spaces to clear any previous characters

    lcd.setCursor(0, 1);
    lcd.print("f: ");
    
    if (isACMode) {
        lcd.print(desiredFrequency);
        lcd.print(" Hz ");
    } else {
        lcd.print("     "); // Display blank for frequency in DC mode
    }

    lcd.setCursor(13, 1);
    lcd.print(isACMode ? "AC" : "DC");
    lcd.setCursor(13, 0);
    lcd.print(isModulationOn ? "Mod" : "   ");
}

void updateDCOutput() {
    uint8_t reg = 0x02;  // Example register value
    uint16_t potValue;

    if (desiredVoltage <= 30.0) {
        float R2 = (desiredVoltage - 1.374) / 0.0028;
        R2 = constrain(R2, 0, 10000);  // constrain between 0 and 10k
        potValue = map(R2, 0, 10000, 0, 1023);
        potValue = constrain(potValue, 0, 1023);

        update_digipot(reg, potValue);
    } else {
        float dutyCycle  = (100.0 * log(desiredVoltage / 27.198)) / 2.7162;
        dutyCycle = constrain(dutyCycle, 0, 100);

        // Convert dutyCycle to equivalent voltage between 0 and 3.3V
        float voltageOutput = 0.55 + ((dutyCycle / 100.0) * (2.75 - 0.55)); 
        voltageOutput = constrain(voltageOutput, 0.55, 2.75); // constraining between 0 and 3.3V

        // Map the voltageOutput to 12-bit range (0-4095) for the DAC
        uint16_t dacValue = map(voltageOutput * 1000, 550, 2750, 0, 4095);  // Multiplied by 1000 to work in millivolts for better accuracy
        // Output the value to the DAC
        analogWrite(DAC0, dacValue);
        lastDacValue = dacValue; // Store the DAC value

        //Output 0.85V to DAC1 for the subtractor circuit
        float desiredVoltageDAC1 = 0.85;
        uint16_t dacValueDAC1 = map(desiredVoltageDAC1 * 1000, 550, 2750, 0, 4095); // Map the desired voltage to 12-bit range
        analogWrite(DAC1, dacValueDAC1);
    }
}

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 1000;  // Send to LCD every second

void loop() {
    readVoltageEncoder();
    readFrequencyEncoder();
    handleACButton();
    handleModButton();
    displayValues();
    updateRelays();
    updateDCOutput();
 
    unsigned long currentMillis = millis();
    if (currentMillis - lastSendTime >= SEND_INTERVAL) {
        sendSerialData();

        if(desiredVoltage <= 30){
            float measuredVoltage = analogRead(A0) * 3.3 / 4095.0; // Use 1024.0 instead of 1026.0 for standard ADC conversion
            float R2 = (measuredVoltage * 10000.0) / 3.3;

            // // Print voltage
            // Serial.print("Digital Pot: Analog Voltage Reading: "); 
            // Serial.print(measuredVoltage, 4); 
            // Serial.print("V.  ");

            // // Print equivalent resistance
            // Serial.print("Equivalent Resistance: "); 
            // Serial.print(R2, 2);  
            // Serial.println(" ohms");
        } else{
        float PWMVoltage = analogRead(A8) * 3.3 / 4095.0;
        float DutyPercent = ((PWMVoltage - 0.55) / (2.75 - 0.55)) * 100;
        float DAC1Voltage = analogRead(A11) * 3.3 / 4095.0;

        // Print ADC read voltage and equivalent duty cycle on one row
        // Serial.print("ADC Read Voltage: ");
        // Serial.print(PWMVoltage, 4);
        // Serial.print(" V  Equivalent Duty Cycle: ");
        // Serial.print(DutyPercent, 3);  
        // Serial.println(" %");

        // // Print DAC1 voltage on the next row
        // // Assuming you have a variable named dac1Voltage that holds the voltage value for DAC1
        // Serial.print("DAC1 Voltage: ");
        // Serial.print(DAC1Voltage, 4);  // Replace dac1Voltage with the appropriate variable or value
        // Serial.println(" V");
        }

        lastSendTime = currentMillis;
    }

}
