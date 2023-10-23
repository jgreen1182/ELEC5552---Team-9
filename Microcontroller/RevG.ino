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
const int RELAY2_PIN = 53;  // Control pin for interlock relay

// Chip select for 10k digipot
const int CSpin1 = 22;  

uint16_t lastDacValue = 0;

// For modulation frequency
int modFrequency = 10; 

// Interlock pin
const int INTERLOCK_PIN = A2;

void setup() {

    // This function initializes the system at startup. It sets up serial communication,
    // initializes the LCD screen, configures pins for various controls, and sets the 
    // resolution for analog operations.

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
    // This function manages the states of two relays: Relay 1 is based on the desiredVoltage 
    // and Relay 2 is controlled by the hardware interlock state
  if (desiredVoltage <= 30) {
    digitalWrite(RELAY1_PIN, HIGH);  // Turn on Relay 1 for 0-30V regulator
  } else {
    digitalWrite(RELAY1_PIN, LOW);   // Turn off Relay 1 for 0-30V regulator
  }
}

bool isInterlockActive() {
    // This function checks the status of the hardware interlock. If the voltage is 
    // below 3.0V, the interlock is considered active.
    float voltageReading = analogRead(INTERLOCK_PIN) * 3.3 / 4095.0; 
    return voltageReading < 3.0;  // if no voltage then interlock is active
}

void readVoltageEncoder() {
    // This function reads and processes input from a rotary encoder to adjust the desiredVoltage. 
    // It allows both fine and coarse adjustments and sets voltage limits based on the interlock state.
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
            if(isACMode){
              desiredVoltage = constrain(desiredVoltage, 1.4, 70.7);
            }
            else{
              desiredVoltage = constrain(desiredVoltage, 1.4, 100.0);
            }
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
    // This function reads input from a rotary encoder to adjust either the modulation frequency 
    // or the desired AC frequency, depending on the current mode. Fine and coarse adjustments 
    // can be made for the AC frequency, toggled by pressing the encoder button.
    static int lastStateFrequencyCLK = LOW;
    int currentStateFrequencyCLK = digitalRead(FREQUENCY_CLK_PIN);

    if (currentStateFrequencyCLK != lastStateFrequencyCLK && currentStateFrequencyCLK == HIGH) {
        int increment;

        if (isModulationOn) {
            // Adjust modulation frequency when modulation mode is activated
            increment = 10;  // Increment for modulation frequency
            modFrequency += digitalRead(FREQUENCY_DT_PIN) == LOW ? increment : -increment;
            modFrequency = constrain(modFrequency, 10, 100);  // Ensure modulation frequency stays within the 10Hz to 100Hz range
        } else if (isACMode) {
            // Adjust AC frequency when in AC mode and modulation mode is not activated
            increment = fineAdjustFrequency ? 1000 : 10000;  // Increment for AC frequency
            desiredFrequency += digitalRead(FREQUENCY_DT_PIN) == LOW ? increment : -increment;
            desiredFrequency = constrain(desiredFrequency, 50000, 300000);  // Ensure AC frequency stays within the 50kHz to 300kHz range
        }
    }

    lastStateFrequencyCLK = currentStateFrequencyCLK;

    // Check if the button is pressed to toggle fine adjust
    if (digitalRead(FREQUENCY_BTN_PIN) == LOW) {
        fineAdjustFrequency = !fineAdjustFrequency;
        delay(300);  // Simple debounce
    }
}


void handleACButton() {
    // This function reads the AC button state and toggles the isACMode variable, indicating 
    // if the system is operating in AC mode. A debounce delay is included.
    int acButtonState = digitalRead(AC_BUTTON_PIN);
    if (acButtonState == LOW && lastACButtonState == HIGH) {
        isACMode = !isACMode;
        delay(300);  // Debounce delay
    }
    lastACButtonState = acButtonState;
}

void handleModButton() {
    // This function reads the Modulation button state and toggles the isModulationOn variable, 
    // indicating if modulation mode is active. A debounce delay is used to prevent accidental triggers.
    int modButtonState = digitalRead(MOD_BUTTON_PIN);
    if (modButtonState == LOW && lastMODButtonState == HIGH) {
        isModulationOn = !isModulationOn;
        delay(300);  // Debounce delay
    }
    lastMODButtonState = modButtonState;
}

void sendSerialData() {
   // This function constructs a string containing the system's current settings, which includes:
    // - AC mode status (1 for AC, 0 for DC)
    // - Modulation state (1 if modulation is on, 0 otherwise)
    // - Rounded desired voltage (e.g., 12340 for 123.4V)
    // - Placeholder for current (set to "000000")
    // - Desired frequency in kHz (only included if in AC mode, set to "000" for DC)
    // - Additional placeholder data ("00")
    // The formatted string is designed for easy parsing and decoding when sent via serial communication.
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
    // This function sends a given value to a specified register on a digital potentiometer (digipot)
    // via SPI (Serial Peripheral Interface) protocol. The 16-bit value is prepared by shifting 
    // it 6 bits to the left before sending.
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
    // This function updates the Arduino Testing LCD screen to display the system's current settings:
    // - Desired voltage with two decimal precision
    // - Operating frequency when modulation is on or in AC mode (blank in DC mode)
    // - Mode (either "AC" or "DC")
    // - Modulation state (displayed as "Mod" when on)
    lcd.setCursor(0, 0);
    lcd.print("V: ");
    lcd.print(desiredVoltage, 2);  // Display with two decimal places
    lcd.print(" V   ");  // Extra spaces to clear any previous characters

    lcd.setCursor(0, 1);
    lcd.print("f: ");
    
    if (isModulationOn) {
        lcd.print(modFrequency);
        lcd.print(" Hz    ");
    } else if (isACMode) {
        lcd.print(desiredFrequency);
        lcd.print(" Hz ");
    } else {
        lcd.print("         "); // Display blank for frequency in DC mode
    }
    lcd.setCursor(12,0);
    lcd.print("|");
    lcd.setCursor(12,1);
    lcd.print("|");  
    lcd.setCursor(13, 1);
    lcd.print(isACMode ? "AC" : "DC");
    lcd.setCursor(13, 0);
    lcd.print(isModulationOn ? "Mod" : "   ");
}



void updateDCOutput() {
    // This function adjusts the DC output based on the desired voltage. 
    uint8_t reg = 0x02;  // Example register value, adjust as necessary.
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

unsigned long lastToggleTime = 0;

void handleModulationPWM() {
    // This function modulates a PWM signal based on the defined modulation frequency.
    int pwmPin = isACMode ? 7 : 6;  // Select the appropriate pin based on the mode

    // Calculate half the period of the modulation frequency (time for one half cycle) in milliseconds
    unsigned long halfPeriod = (1000 / modFrequency) / 2;

    // Check if it's time to toggle the output
    if (millis() - lastToggleTime >= halfPeriod) {
        digitalWrite(pwmPin, !digitalRead(pwmPin));  // Toggle the output
        lastToggleTime = millis();  // Update the last toggle time
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

    if (isModulationOn) {
        handleModulationPWM();
    }
 
    unsigned long currentMillis = millis();
    if (currentMillis - lastSendTime >= SEND_INTERVAL) {

        //TESTING PURPOSES ONLY 
        sendSerialData();

        if(desiredVoltage <= 30){
            float measuredVoltage = analogRead(A0) * 3.3 / 4095.0; // Use 1024.0 instead of 1026.0 for standard ADC conversion
            float R2 = (measuredVoltage * 10000.0) / 3.3;

            // Print voltage
            Serial.print("Digital Pot: Analog Voltage Reading: "); 
            Serial.print(measuredVoltage, 4); 
            Serial.print("V.  ");

            // Print equivalent resistance
            Serial.print("Equivalent Resistance: "); 
            Serial.print(R2, 2);  
            Serial.println(" ohms");
        } else{
        float PWMVoltage = analogRead(A8) * 3.3 / 4095.0;
        float DutyPercent = ((PWMVoltage - 0.55) / (2.75 - 0.55)) * 100;
        float DAC1Voltage = analogRead(A11) * 3.3 / 4095.0;

        //Print ADC read voltage and equivalent duty cycle on one row
        Serial.print("ADC Read Voltage: ");
        Serial.print(PWMVoltage, 4);
        Serial.print(" V  Equivalent Duty Cycle: ");
        Serial.print(DutyPercent, 3);  
        Serial.println(" %");

        // Print DAC1 voltage on the next row
        // Assuming you have a variable named dac1Voltage that holds the voltage value for DAC1
        Serial.print("DAC1 Voltage: ");
        Serial.print(DAC1Voltage, 4);  // Replace dac1Voltage with the appropriate variable or value
        Serial.println(" V");
        }

        lastSendTime = currentMillis;
    }

}
