const int potPin = A0; // Analog input pin connected to the potentiometer

void setup() {
  // Initialize serial communication at 9600 bits per second
  Serial.begin(9600);

  // Set the ADC resolution to 12 bits
  analogReadResolution(12);
}

void loop() {
  // Read the input on analog pin 0 (value will be between 0 and 4095)
  int potValue = analogRead(potPin);

  // Print out the value
  Serial.println(potValue);

  // Wait for a bit to avoid flooding the serial port
  delay(200);
}
