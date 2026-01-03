//Serial RGBW Color Test for Horizon Strand
//Enter colors in serial monitor as: R,G,B,W (e.g., "255,0,0,0" for red)

#include <Adafruit_NeoPixel.h>

// Pin definitions
#define HORIZON_PIN D2
#define HORIZON_PIXELS 54

// Initialize NeoPixel object (SK6812 GRBW type)
Adafruit_NeoPixel horizon = Adafruit_NeoPixel(HORIZON_PIXELS, HORIZON_PIN, NEO_GRBW + NEO_KHZ800);

// Serial input buffer
String inputString = "";
bool stringComplete = false;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("========================================");
  Serial.println("Serial RGBW Color Test - Horizon Strand");
  Serial.println("========================================");
  Serial.println("Enter colors as: R,G,B,W");
  Serial.println("Example: 255,0,0,0 (red)");
  Serial.println("         0,255,0,0 (green)");
  Serial.println("         0,0,255,0 (blue)");
  Serial.println("         0,0,0,255 (white)");
  Serial.println("         255,128,64,32 (mixed)");
  Serial.println("========================================");

  // Initialize horizon strand
  horizon.begin();

  // Set all pixels to off initially
  setStrandColor(0, 0, 0, 0);
  horizon.show();

  Serial.println("Ready! Waiting for input...");

  inputString.reserve(50); // Reserve space for input string
}

void loop() {
  // Check for serial input
  while (Serial.available()) {
    char inChar = (char)Serial.read();

    // Add character to input string
    if (inChar == '\n' || inChar == '\r') {
      if (inputString.length() > 0) {
        stringComplete = true;
      }
    } else {
      inputString += inChar;
    }
  }

  // Check if we have a complete input string
  if (stringComplete) {
    parseAndSetColor(inputString);

    // Clear the string and flag
    inputString = "";
    stringComplete = false;

    Serial.println("Ready! Waiting for input...");
  }
}

// Parse the input string and set the color
void parseAndSetColor(String input) {
  input.trim(); // Remove whitespace

  Serial.print("Input received: ");
  Serial.println(input);

  // Parse comma-separated values
  int r = -1, g = -1, b = -1, w = -1;
  int commaIndex1 = input.indexOf(',');
  int commaIndex2 = input.indexOf(',', commaIndex1 + 1);
  int commaIndex3 = input.indexOf(',', commaIndex2 + 1);

  if (commaIndex1 > 0 && commaIndex2 > 0 && commaIndex3 > 0) {
    // Parse R,G,B,W values
    r = input.substring(0, commaIndex1).toInt();
    g = input.substring(commaIndex1 + 1, commaIndex2).toInt();
    b = input.substring(commaIndex2 + 1, commaIndex3).toInt();
    w = input.substring(commaIndex3 + 1).toInt();

    // Validate values (0-255)
    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 &&
        b >= 0 && b <= 255 && w >= 0 && w <= 255) {

      Serial.println("========================================");
      Serial.print("Setting color - R: "); Serial.print(r);
      Serial.print(", G: "); Serial.print(g);
      Serial.print(", B: "); Serial.print(b);
      Serial.print(", W: "); Serial.println(w);
      Serial.println("========================================");

      setStrandColor(r, g, b, w);
    } else {
      Serial.println("ERROR: Values must be between 0 and 255");
    }
  } else {
    Serial.println("ERROR: Invalid format. Use R,G,B,W (e.g., 255,0,0,0)");
  }
}

// Helper function to set entire strand to one color
// Note: Using R,G,B,W order for user input, but NeoPixel uses G,R,B,W internally
void setStrandColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
  for (int i = 0; i < horizon.numPixels(); i++) {
    horizon.setPixelColor(i, horizon.Color(green, red, blue, white));
  }
  horizon.show();
}
