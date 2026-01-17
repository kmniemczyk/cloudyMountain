// LED Identifier Test Program
// Use this to identify which LED number corresponds to each physical position
//
// Serial Commands:
//   1 - Select CLOUD_1 (32 LEDs)
//   2 - Select CLOUD_2 (45 LEDs)
//   3 - Select CLOUD_3 (46 LEDs)
//   + - Next LED
//   - - Previous LED
//   0 - Turn off all LEDs
//   a - Show all LEDs on current cloud
//   ? - Show current status

#include <Adafruit_NeoPixel.h>

// Pin definitions for NeoPixel strands
#define CLOUD_1_PIN D3
#define CLOUD_2_PIN D4
#define CLOUD_3_PIN D5

// Number of pixels in each strand
#define CLOUD_1_PIXELS 32
#define CLOUD_2_PIXELS 45
#define CLOUD_3_PIXELS 46

// Initialize NeoPixel objects (SK6812 RGBW type - but chips are wired as GRBW)
Adafruit_NeoPixel cloud1 = Adafruit_NeoPixel(CLOUD_1_PIXELS, CLOUD_1_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel cloud2 = Adafruit_NeoPixel(CLOUD_2_PIXELS, CLOUD_2_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel cloud3 = Adafruit_NeoPixel(CLOUD_3_PIXELS, CLOUD_3_PIN, NEO_GRBW + NEO_KHZ800);

// Current state
int selectedCloud = 1;  // 1, 2, or 3
int currentLED = 0;      // Current LED index being displayed

// Get pointer to current cloud
Adafruit_NeoPixel* getCurrentCloud() {
  switch(selectedCloud) {
    case 1: return &cloud1;
    case 2: return &cloud2;
    case 3: return &cloud3;
    default: return &cloud1;
  }
}

// Get max LEDs for current cloud
int getMaxLEDs() {
  switch(selectedCloud) {
    case 1: return CLOUD_1_PIXELS;
    case 2: return CLOUD_2_PIXELS;
    case 3: return CLOUD_3_PIXELS;
    default: return CLOUD_1_PIXELS;
  }
}

// Turn off all clouds
void allOff() {
  for(int i = 0; i < CLOUD_1_PIXELS; i++) {
    cloud1.setPixelColor(i, 0, 0, 0, 0);
  }
  cloud1.show();

  for(int i = 0; i < CLOUD_2_PIXELS; i++) {
    cloud2.setPixelColor(i, 0, 0, 0, 0);
  }
  cloud2.show();

  for(int i = 0; i < CLOUD_3_PIXELS; i++) {
    cloud3.setPixelColor(i, 0, 0, 0, 0);
  }
  cloud3.show();
}

// Display current LED
void displayCurrentLED() {
  Adafruit_NeoPixel* cloud = getCurrentCloud();
  int maxLEDs = getMaxLEDs();

  // Turn off all LEDs on all clouds
  allOff();

  // Light up the current LED (bright white)
  cloud->setPixelColor(currentLED, cloud->Color(255, 255, 255, 255));
  cloud->show();

  // Print status
  Serial.println("========================================");
  Serial.print("CLOUD_");
  Serial.print(selectedCloud);
  Serial.print(" - LED #");
  Serial.print(currentLED);
  Serial.print(" of ");
  Serial.print(maxLEDs - 1);
  Serial.print(" (");
  Serial.print(currentLED + 1);
  Serial.print("/");
  Serial.print(maxLEDs);
  Serial.println(")");
  Serial.println("========================================");
  Serial.println("Commands:");
  Serial.println("  1-3: Select cloud");
  Serial.println("  +: Next LED");
  Serial.println("  -: Previous LED");
  Serial.println("  0: All off");
  Serial.println("  a: Show all LEDs on current cloud");
  Serial.println("  ?: Show status");
  Serial.println("========================================");
}

// Show all LEDs on current cloud
void showAllLEDs() {
  Adafruit_NeoPixel* cloud = getCurrentCloud();
  int maxLEDs = getMaxLEDs();

  // Turn off other clouds
  allOff();

  // Light up all LEDs with a dim white
  for(int i = 0; i < maxLEDs; i++) {
    cloud->setPixelColor(i, cloud->Color(50, 50, 50, 50));
  }
  cloud->show();

  Serial.println("========================================");
  Serial.print("Showing all ");
  Serial.print(maxLEDs);
  Serial.print(" LEDs on CLOUD_");
  Serial.println(selectedCloud);
  Serial.println("========================================");
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("\n\n========================================");
  Serial.println("LED IDENTIFIER TEST PROGRAM");
  Serial.println("========================================");
  Serial.println("This program helps identify LED positions");
  Serial.println();

  // Initialize all NeoPixel strands
  cloud1.begin();
  cloud2.begin();
  cloud3.begin();

  // Turn all LEDs off
  allOff();

  Serial.println("Initialization complete!");
  Serial.println();

  // Display initial state
  displayCurrentLED();
}

void loop() {
  // Check for serial input
  if (Serial.available() > 0) {
    char command = Serial.read();

    // Clear any remaining newline characters
    while(Serial.available() > 0) {
      Serial.read();
    }

    switch(command) {
      case '1':  // Select CLOUD_1
        selectedCloud = 1;
        currentLED = 0;
        Serial.println("\nSelected CLOUD_1");
        displayCurrentLED();
        break;

      case '2':  // Select CLOUD_2
        selectedCloud = 2;
        currentLED = 0;
        Serial.println("\nSelected CLOUD_2");
        displayCurrentLED();
        break;

      case '3':  // Select CLOUD_3
        selectedCloud = 3;
        currentLED = 0;
        Serial.println("\nSelected CLOUD_3");
        displayCurrentLED();
        break;

      case '+':  // Next LED
      case '=':  // Also accept = key (shift + on many keyboards)
        currentLED++;
        if(currentLED >= getMaxLEDs()) {
          currentLED = 0;  // Wrap around
          Serial.println("\nWrapped to LED 0");
        }
        displayCurrentLED();
        break;

      case '-':  // Previous LED
      case '_':  // Also accept _ key
        currentLED--;
        if(currentLED < 0) {
          currentLED = getMaxLEDs() - 1;  // Wrap around
          Serial.print("\nWrapped to LED ");
          Serial.println(currentLED);
        }
        displayCurrentLED();
        break;

      case '0':  // Turn off all LEDs
        allOff();
        Serial.println("\n========================================");
        Serial.println("All LEDs turned OFF");
        Serial.println("Press 1-3 to select a cloud");
        Serial.println("========================================");
        break;

      case 'a':  // Show all LEDs on current cloud
      case 'A':
        showAllLEDs();
        break;

      case '?':  // Show current status
        displayCurrentLED();
        break;

      case '\n':  // Ignore newlines
      case '\r':
        break;

      default:
        Serial.print("\nUnknown command: ");
        Serial.println(command);
        Serial.println("Press ? for help");
        break;
    }
  }

  delay(10);  // Small delay to avoid overwhelming serial
}
