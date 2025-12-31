#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_NeoPixel.h>

// Pin definitions for NeoPixel strands
#define CLOUD_1_PIN D3
#define CLOUD_2_PIN D4
#define CLOUD_3_PIN D5
#define HORIZON_PIN D6

// Number of pixels in each strand
#define CLOUD_1_PIXELS 32
#define CLOUD_2_PIXELS 45
#define CLOUD_3_PIXELS 46
#define HORIZON_PIXELS 54

// Maximum total brightness across all lit LEDs (power management)
#define MAX_TOTAL_BRIGHTNESS 750

// Initialize NeoPixel objects (SK6812 GRBW type)
Adafruit_NeoPixel cloud1 = Adafruit_NeoPixel(CLOUD_1_PIXELS, CLOUD_1_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel cloud2 = Adafruit_NeoPixel(CLOUD_2_PIXELS, CLOUD_2_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel cloud3 = Adafruit_NeoPixel(CLOUD_3_PIXELS, CLOUD_3_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel horizon = Adafruit_NeoPixel(HORIZON_PIXELS, HORIZON_PIN, NEO_GRBW + NEO_KHZ800);

// Initialize MPR121 touch sensor
Adafruit_MPR121 touchSensor = Adafruit_MPR121();

// Variables to track touch state
uint16_t lastTouched = 0;
uint16_t currentTouched = 0;

// Global brightness scale factor (0.0 to 1.0)
float brightnessScale = 1.0;

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("CloudyMountain Initializing...");

  // Initialize MPR121 touch sensor
  if (!touchSensor.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring!");
    while (1);
  }
  Serial.println("MPR121 found!");

  // Initialize all NeoPixel strands
  cloud1.begin();
  cloud2.begin();
  cloud3.begin();
  horizon.begin();

  // Set all pixels to off initially
  cloud1.show();
  cloud2.show();
  cloud3.show();
  horizon.show();

  Serial.println("Setup complete!");
}

void loop() {
  // Get current touch state from MPR121
  currentTouched = touchSensor.touched();

  // Check each of the 12 touch pads
  for (uint8_t i = 0; i < 12; i++) {
    // Check if pad was just touched
    if ((currentTouched & (1 << i)) && !(lastTouched & (1 << i))) {
      Serial.print("Pad "); Serial.print(i); Serial.println(" touched");
      handleTouch(i);
    }

    // Check if pad was just released
    if (!(currentTouched & (1 << i)) && (lastTouched & (1 << i))) {
      Serial.print("Pad "); Serial.print(i); Serial.println(" released");
      handleRelease(i);
    }
  }

  // Update last touch state
  lastTouched = currentTouched;

  // Small delay to avoid overwhelming the serial output
  delay(10);
}

// Handle touch events for each pad
void handleTouch(uint8_t pad) {
  // Add your touch handling logic here
  // Example: Light up different strands based on pad number
  switch(pad) {
    case 0:
      // Example: Set CLOUD_1 to white
      setStrandColor(cloud1, 255, 255, 255, 255);
      break;
    case 1:
      // Example: Set CLOUD_2 to white
      setStrandColor(cloud2, 255, 255, 255, 255);
      break;
    case 2:
      // Example: Set CLOUD_3 to white
      setStrandColor(cloud3, 255, 255, 255, 255);
      break;
    case 3:
      // Example: Set HORIZON to white
      setStrandColor(horizon, 255, 255, 255, 255);
      break;
    default:
      // Handle other pads
      break;
  }
}

// Handle release events for each pad
void handleRelease(uint8_t pad) {
  // Add your release handling logic here
  // Example: Turn off the strand when pad is released
  switch(pad) {
    case 0:
      setStrandColor(cloud1, 0, 0, 0, 0);
      break;
    case 1:
      setStrandColor(cloud2, 0, 0, 0, 0);
      break;
    case 2:
      setStrandColor(cloud3, 0, 0, 0, 0);
      break;
    case 3:
      setStrandColor(horizon, 0, 0, 0, 0);
      break;
    default:
      break;
  }
}

// Calculate total brightness across all strands
uint32_t calculateTotalBrightness() {
  uint32_t total = 0;

  for (int i = 0; i < cloud1.numPixels(); i++) {
    uint32_t color = cloud1.getPixelColor(i);
    total += (color >> 24) & 0xFF; // Green
    total += (color >> 16) & 0xFF; // Red
    total += (color >> 8) & 0xFF;  // Blue
    total += color & 0xFF;          // White
  }

  for (int i = 0; i < cloud2.numPixels(); i++) {
    uint32_t color = cloud2.getPixelColor(i);
    total += (color >> 24) & 0xFF;
    total += (color >> 16) & 0xFF;
    total += (color >> 8) & 0xFF;
    total += color & 0xFF;
  }

  for (int i = 0; i < cloud3.numPixels(); i++) {
    uint32_t color = cloud3.getPixelColor(i);
    total += (color >> 24) & 0xFF;
    total += (color >> 16) & 0xFF;
    total += (color >> 8) & 0xFF;
    total += color & 0xFF;
  }

  for (int i = 0; i < horizon.numPixels(); i++) {
    uint32_t color = horizon.getPixelColor(i);
    total += (color >> 24) & 0xFF;
    total += (color >> 16) & 0xFF;
    total += (color >> 8) & 0xFF;
    total += color & 0xFF;
  }

  return total;
}

// Apply brightness scaling to all strands
void applyBrightnessLimit() {
  uint32_t totalBrightness = calculateTotalBrightness();

  if (totalBrightness > MAX_TOTAL_BRIGHTNESS) {
    brightnessScale = (float)MAX_TOTAL_BRIGHTNESS / (float)totalBrightness;

    Serial.print("Brightness limit applied: ");
    Serial.print(brightnessScale * 100);
    Serial.println("%");
  } else {
    brightnessScale = 1.0;
  }
}

// Helper function to set entire strand to one color
void setStrandColor(Adafruit_NeoPixel &strand, uint8_t green, uint8_t red, uint8_t blue, uint8_t white) {
  for (int i = 0; i < strand.numPixels(); i++) {
    strand.setPixelColor(i, strand.Color(green, red, blue, white));
  }
  strand.show();

  // After updating, check if we need to scale brightness
  applyBrightnessLimit();

  // If scaling is needed, update all strands with scaled values
  if (brightnessScale < 1.0) {
    // Scale cloud1
    for (int i = 0; i < cloud1.numPixels(); i++) {
      uint32_t color = cloud1.getPixelColor(i);
      uint8_t g = ((color >> 24) & 0xFF) * brightnessScale;
      uint8_t r = ((color >> 16) & 0xFF) * brightnessScale;
      uint8_t b = ((color >> 8) & 0xFF) * brightnessScale;
      uint8_t w = (color & 0xFF) * brightnessScale;
      cloud1.setPixelColor(i, cloud1.Color(g, r, b, w));
    }
    cloud1.show();

    // Scale cloud2
    for (int i = 0; i < cloud2.numPixels(); i++) {
      uint32_t color = cloud2.getPixelColor(i);
      uint8_t g = ((color >> 24) & 0xFF) * brightnessScale;
      uint8_t r = ((color >> 16) & 0xFF) * brightnessScale;
      uint8_t b = ((color >> 8) & 0xFF) * brightnessScale;
      uint8_t w = (color & 0xFF) * brightnessScale;
      cloud2.setPixelColor(i, cloud2.Color(g, r, b, w));
    }
    cloud2.show();

    // Scale cloud3
    for (int i = 0; i < cloud3.numPixels(); i++) {
      uint32_t color = cloud3.getPixelColor(i);
      uint8_t g = ((color >> 24) & 0xFF) * brightnessScale;
      uint8_t r = ((color >> 16) & 0xFF) * brightnessScale;
      uint8_t b = ((color >> 8) & 0xFF) * brightnessScale;
      uint8_t w = (color & 0xFF) * brightnessScale;
      cloud3.setPixelColor(i, cloud3.Color(g, r, b, w));
    }
    cloud3.show();

    // Scale horizon
    for (int i = 0; i < horizon.numPixels(); i++) {
      uint32_t color = horizon.getPixelColor(i);
      uint8_t g = ((color >> 24) & 0xFF) * brightnessScale;
      uint8_t r = ((color >> 16) & 0xFF) * brightnessScale;
      uint8_t b = ((color >> 8) & 0xFF) * brightnessScale;
      uint8_t w = (color & 0xFF) * brightnessScale;
      horizon.setPixelColor(i, horizon.Color(g, r, b, w));
    }
    horizon.show();
  }
}
