const int encoderPin1 = 8; // CLK pin for the rotary encoder
const int encoderPin2 = 9; // DT pin for the rotary encoder

int pin1State;
int lastPin1State = LOW;

// Define the debounce delay in milliseconds
const int debounceDelay = 5;

void setup() {
  pinMode(encoderPin1, INPUT);
  pinMode(encoderPin2, INPUT);
  Serial.begin(9600);
}

void loop() {
  pin1State = digitalRead(encoderPin1);
  if (pin1State != lastPin1State) {
    delay(debounceDelay); // Add a debounce delay
    pin1State = digitalRead(encoderPin1); // Read the pin state again after the delay
    if (pin1State != lastPin1State) { // Check if the pin state has changed
      if (digitalRead(encoderPin2) != pin1State) {
        Serial.println("Clockwise");
      } else {
        Serial.println("Counter Clockwise");
      }
    }
  }
  lastPin1State = pin1State;
}
