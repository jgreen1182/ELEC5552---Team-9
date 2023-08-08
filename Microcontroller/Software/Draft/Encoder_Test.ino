#include <LiquidCrystal.h>

// Initialize the LCD
LiquidCrystal lcd(12, 11, 5, 4, 3, 2); // Adjust these pin numbers to match your setup

// Rotary Encoder Inputs
#define CLK 8
#define DT 9
#define SW 10
#define POT A0

int counter = 0;
int currentStateCLK;
int lastStateCLK;
String currentDir ="";
bool fineAdjust = false;
int lastButtonState = HIGH;

void setup() {
        
  // Set encoder pins as inputs
  pinMode(CLK,INPUT);
  pinMode(DT,INPUT);
  pinMode(SW,INPUT_PULLUP);

  // Setup Serial Monitor
  Serial.begin(9600);

  // Initialize the LCD with 16 columns and 2 rows
  lcd.begin(16, 2);

  // Read the initial state of CLK
  lastStateCLK = digitalRead(CLK);

  // Print the static text on the LCD
  lcd.setCursor(0, 0);
  lcd.print("Pot Duty:");
  lcd.setCursor(0, 1);
  lcd.print("Enc Duty:");
}

void loop() {
        
  // Read the current state of CLK
  currentStateCLK = digitalRead(CLK);

  // Check if the switch is pressed
  if (digitalRead(SW) == LOW && lastButtonState == HIGH) {
    fineAdjust = !fineAdjust;
    delay(200); // Debounce delay
  }
  lastButtonState = digitalRead(SW);

  // If last and current state of CLK are different, then pulse occurred
  // React to only 1 state change to avoid double count
  if (currentStateCLK != lastStateCLK  && currentStateCLK == 1){

    // If the DT state is different than the CLK state then
    // the encoder is rotating CCW so decrement
    if (digitalRead(DT) != currentStateCLK) {
      counter += fineAdjust ? 1 : 40;
      currentDir ="CW";
    } else {
      // Encoder is rotating CW so increment
      counter -= fineAdjust ? 1 : 40;
      currentDir ="CCW";
    }

    // Constrain the counter value between 0 and 4000
    counter = constrain(counter, 0, 4000);
  }

  // Read the value from the potentiometer
  int potValue = analogRead(POT);

  // Calculate the duty cycles as percentages
  float potDutyCyclePercentage = (float)potValue / 1023.0 * 100.0;
  float encoderDutyCyclePercentage = (float)counter / 4000.0 * 100.0;

  // Display the duty cycles on the LCD
  lcd.setCursor(9, 0);
  lcd.print("     "); // Clear the previous value
  lcd.setCursor(9, 0);
  lcd.print(potDutyCyclePercentage, 3);
  lcd.print("%");

  lcd.setCursor(9, 1);
  lcd.print("     "); // Clear the previous value
  lcd.setCursor(9, 1);
  lcd.print(encoderDutyCyclePercentage, 3);
  lcd.print("%");

  // Remember last CLK state
  lastStateCLK = currentStateCLK;

  // Put in a slight delay to help debounce the reading
  delay(1);
}
