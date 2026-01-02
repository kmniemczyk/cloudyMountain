#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_NeoPixel.h>

// Pin definitions for NeoPixel strands
#define CLOUD_1_PIN D3
#define CLOUD_2_PIN D4
#define CLOUD_3_PIN D5
#define HORIZON_PIN D2
#define STARS_PIN D6

// Number of pixels in each strand
#define CLOUD_1_PIXELS 32
#define CLOUD_2_PIXELS 45
#define CLOUD_3_PIXELS 46
#define HORIZON_PIXELS 54
#define STARS_PIXELS 20

// Maximum total brightness across all lit LEDs (power management)
// Calculation: 5V 12A supply, 85% safe usage = 10.2A
// SK6812RGBW: 80mA per pixel max, 177 pixels = 14.16A theoretical max
// Safe operation: (10.2A / 14.16A) × 180540 max units = 129989
#define MAX_TOTAL_BRIGHTNESS 129000  // 72% of theoretical max (10.2A / 14.16A)

// Brightness ramping factors for sunrise/sunset
#define BRIGHTNESS_MIN_FACTOR 0.125  // 1/8 brightness for night scenes
#define BRIGHTNESS_MAX_FACTOR 1.0    // Full brightness for day scenes

// Timing constants for progression (PRODUCTION durations)
#define SUNRISE_HOLD_DURATION_MS 120000      // 2 minutes night blue hold
#define PROGRESSION_DURATION_MS 1200000      // 20 minutes sunrise/sunset progression

// Initialize NeoPixel objects (SK6812 GRBW type)
Adafruit_NeoPixel cloud1 = Adafruit_NeoPixel(CLOUD_1_PIXELS, CLOUD_1_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel cloud2 = Adafruit_NeoPixel(CLOUD_2_PIXELS, CLOUD_2_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel cloud3 = Adafruit_NeoPixel(CLOUD_3_PIXELS, CLOUD_3_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel horizon = Adafruit_NeoPixel(HORIZON_PIXELS, HORIZON_PIN, NEO_GRBW + NEO_KHZ800);

// Stars are simple on/off white LEDs (not addressable)
bool starsOn = true;  // Default to ON

// Initialize MPR121 touch sensor
Adafruit_MPR121 touchSensor = Adafruit_MPR121();

// Variables to track touch state
uint16_t lastTouched = 0;
uint16_t currentTouched = 0;

// Global brightness scale factor (0.0 to 1.0)
float brightnessScale = 1.0;

// Forward declarations
typedef enum {
  SEQ_OFF,
  SEQ_SUNRISE_HOLD,  // Hold at night blue before sunrise
  SEQ_SUNRISE_PROG,  // Sunrise progression
  SEQ_DAY,           // Daytime mode
  SEQ_SUNSET_PROG    // Sunset progression (reverse)
} SequenceState;

// Color structure for GRBW LEDs
struct ColorGRBW {
  uint8_t g, r, b, w;
};

// COMMENTED OUT - Previous 24-color palette
// const ColorGRBW PROGMEM sunsetPalette_OLD[24] = {
//   {0, 0, 80, 0},        // 0: nightBlue
//   {5, 8, 72, 0},        // 1: midnight transition
//   {10, 15, 65, 0},      // 2: darkIndigo
//   {17, 25, 68, 0},      // 3: early twilight
//   {25, 35, 65, 0},      // 4: deepPurple
//   {33, 48, 60, 0},      // 5: dawn approach
//   {40, 60, 55, 0},      // 6: twilightPurple
//   {50, 70, 48, 0},      // 7: light purple
//   {60, 80, 40, 0},      // 8: lavender
//   {70, 90, 35, 0},      // 9: purple-rose bridge
//   {80, 100, 30, 0},     // 10: deepRose
//   {90, 110, 25, 0},     // 11: rose pink bridge
//   {100, 120, 20, 0},    // 12: rosePink
//   {110, 125, 15, 0},    // 13: warm pink transition
//   {120, 130, 10, 0},    // 14: warmPink
//   {130, 135, 7, 0},     // 15: peach approach
//   {140, 140, 5, 0},     // 16: peach
//   {160, 130, 0, 10},    // 17: deepOrange
//   {180, 110, 0, 20},    // 18: orange
//   {200, 90, 0, 30},     // 19: goldenOrange
//   {220, 70, 0, 50},     // 20: goldenYellow
//   {240, 50, 0, 80},     // 21: warmYellow
//   {200, 40, 10, 180},   // 22: paleYellow
//   {80, 50, 30, 255}     // 23: softWhite
// };

// NEW 32-color sunset/sunrise palette stored in PROGMEM to save SRAM
// User-provided palette for smoother transitions
const ColorGRBW PROGMEM sunsetPalette[32] = {
  {12, 10, 25, 0},      // 0
  {18, 14, 34, 0},      // 1
  {28, 20, 46, 0},      // 2
  {40, 28, 58, 0},      // 3
  {58, 36, 68, 0},      // 4
  {80, 44, 72, 4},      // 5
  {100, 56, 70, 8},     // 6
  {120, 72, 66, 12},    // 7
  {140, 92, 60, 18},    // 8
  {160, 112, 52, 28},   // 9
  {180, 132, 44, 36},   // 10
  {200, 152, 38, 48},   // 11
  {216, 168, 34, 64},   // 12
  {228, 184, 34, 80},   // 13
  {236, 196, 36, 100},  // 14
  {244, 208, 40, 120},  // 15
  {250, 216, 52, 140},  // 16
  {252, 220, 70, 156},  // 17
  {252, 224, 92, 172},  // 18
  {250, 220, 120, 188}, // 19
  {248, 214, 146, 200}, // 20
  {244, 206, 176, 212}, // 21
  {236, 194, 200, 220}, // 22
  {224, 178, 216, 228}, // 23
  {208, 160, 224, 232}, // 24
  {188, 140, 228, 236}, // 25
  {168, 120, 232, 240}, // 26
  {148, 100, 236, 244}, // 27
  {132, 88, 238, 248},  // 28
  {120, 78, 240, 252},  // 29
  {112, 72, 242, 255},  // 30
  {108, 70, 244, 255}   // 31
};

// Helper function to read color from PROGMEM
ColorGRBW getPaletteColor(uint8_t index) {
  if (index > 31) index = 31;  // Clamp to 32-color range
  ColorGRBW color;
  memcpy_P(&color, &sunsetPalette[index], sizeof(ColorGRBW));
  return color;
}

// Interpolate between palette colors for smooth transitions
// Takes a float position from 0.0 to 31.0 (32-color palette)
// Returns a blended color between the two adjacent palette entries
ColorGRBW interpolateColor(float position) {
  // Clamp position to valid range
  if (position < 0.0) position = 0.0;
  if (position > 31.0) position = 31.0;

  // Get the two palette indices to blend between
  uint8_t index1 = (uint8_t)position;  // Floor
  uint8_t index2 = index1 + 1;

  // Handle edge case at the end of the palette
  if (index2 > 31) index2 = 31;

  // Calculate blend factor (0.0 to 1.0 between the two colors)
  float blend = position - (float)index1;

  // Get the two colors
  ColorGRBW color1 = getPaletteColor(index1);
  ColorGRBW color2 = getPaletteColor(index2);

  // Linearly interpolate each channel
  ColorGRBW result;
  result.g = color1.g + (color2.g - color1.g) * blend;
  result.r = color1.r + (color2.r - color1.r) * blend;
  result.b = color1.b + (color2.b - color1.b) * blend;
  result.w = color1.w + (color2.w - color1.w) * blend;

  return result;
}

// State tracking structure
struct ProgressionState {
  SequenceState currentSequence;
  float progressPercent;         // 0.0 to 100.0
  unsigned long phaseStartTime;  // When current phase started (millis)
  bool isAnimating;              // Whether progression is active
};

// Global progression state
ProgressionState progState = {SEQ_OFF, 0.0, 0, false};

// Helper: Convert percentage (0-100%) to palette position (0.0-31.0)
float percentToPalettePosition(float percent) {
  if (percent < 0.0) percent = 0.0;
  if (percent > 100.0) percent = 100.0;
  return (percent / 100.0) * 31.0;  // Map to 32-color palette
}

// Calculate brightness multiplier based on progression percentage and state
// Returns a value between BRIGHTNESS_MIN_FACTOR (0.125) and BRIGHTNESS_MAX_FACTOR (1.0)
float getBrightnessMultiplier(float percent, SequenceState state) {
  if (percent < 0.0) percent = 0.0;
  if (percent > 100.0) percent = 100.0;

  float normalizedPercent = percent / 100.0;  // Convert to 0.0-1.0 range

  // State-specific brightness behavior
  switch(state) {
    case SEQ_SUNRISE_HOLD:
      // Hold at minimum brightness (night blue)
      return BRIGHTNESS_MIN_FACTOR;

    case SEQ_SUNRISE_PROG:
      // Ramp from min to max brightness during sunrise
      return BRIGHTNESS_MIN_FACTOR + normalizedPercent * (BRIGHTNESS_MAX_FACTOR - BRIGHTNESS_MIN_FACTOR);

    case SEQ_SUNSET_PROG:
      // Ramp from max to min brightness during sunset (reverse of sunrise)
      return BRIGHTNESS_MAX_FACTOR - normalizedPercent * (BRIGHTNESS_MAX_FACTOR - BRIGHTNESS_MIN_FACTOR);

    case SEQ_DAY:
      // Full brightness during daytime
      return BRIGHTNESS_MAX_FACTOR;

    case SEQ_OFF:
    default:
      // Default to full brightness
      return BRIGHTNESS_MAX_FACTOR;
  }
}

// Transition to a new sequence state
void transitionToSequence(SequenceState newState) {
  progState.currentSequence = newState;
  progState.phaseStartTime = millis();
  progState.progressPercent = 0.0;
  progState.isAnimating = true;

  Serial.print("Transitioning to sequence: ");
  Serial.println(newState);
}

// Update the progression based on elapsed time
void updateProgression() {
  if (!progState.isAnimating) {
    return;  // Nothing to do if not animating
  }

  unsigned long currentTime = millis();
  unsigned long elapsed = currentTime - progState.phaseStartTime;
  unsigned long phaseDuration;

  // Determine phase duration based on current state
  switch(progState.currentSequence) {
    case SEQ_SUNRISE_HOLD:
      phaseDuration = SUNRISE_HOLD_DURATION_MS;
      break;
    case SEQ_SUNRISE_PROG:
    case SEQ_SUNSET_PROG:
      phaseDuration = PROGRESSION_DURATION_MS;
      break;
    default:
      phaseDuration = PROGRESSION_DURATION_MS;
      break;
  }

  // Calculate progress percentage based on elapsed time
  progState.progressPercent = ((float)elapsed / (float)phaseDuration) * 100.0;

  // Check if phase is complete and handle transitions
  if (progState.progressPercent >= 100.0) {
    progState.progressPercent = 100.0;

    // Auto-transition to next phase based on current state
    switch(progState.currentSequence) {
      case SEQ_SUNRISE_HOLD:
        Serial.println("Hold complete! Starting sunrise progression...");
        transitionToSequence(SEQ_SUNRISE_PROG);
        return;  // Exit and let next loop iteration handle the new state

      case SEQ_SUNRISE_PROG:
        Serial.println("Sunrise progression complete! Transitioning to DAY mode...");
        transitionToSequence(SEQ_DAY);
        return;  // Exit and let next loop iteration handle the new state

      case SEQ_SUNSET_PROG:
        Serial.println("Sunset progression complete! Ending at night blue...");
        progState.isAnimating = false;
        break;

      case SEQ_DAY:
        // DAY mode is static - just stop animating
        progState.isAnimating = false;
        Serial.println("DAY mode active (static).");
        break;

      default:
        progState.isAnimating = false;
        Serial.println("Sequence complete!");
        break;
    }
  }

  // Calculate color based on current state
  float palPos;
  ColorGRBW c;

  if (progState.currentSequence == SEQ_SUNRISE_HOLD) {
    // Hold at night blue (palette position 0.0)
    palPos = 0.0;
    c = interpolateColor(palPos);
  } else if (progState.currentSequence == SEQ_SUNRISE_PROG) {
    // Progress through palette based on percentage (0% → 100% = palette 0.0 → 31.0)
    palPos = percentToPalettePosition(progState.progressPercent);
    c = interpolateColor(palPos);
  } else if (progState.currentSequence == SEQ_SUNSET_PROG) {
    // Reverse progress through palette (0% → 100% = palette 31.0 → 0.0)
    palPos = percentToPalettePosition(100.0 - progState.progressPercent);
    c = interpolateColor(palPos);
  } else if (progState.currentSequence == SEQ_DAY) {
    // Full daylight (palette position 31.0)
    palPos = 31.0;
    c = interpolateColor(palPos);
  } else {
    // Default to night blue
    palPos = 0.0;
    c = interpolateColor(palPos);
  }

  // Apply brightness multiplier
  float brightMult = getBrightnessMultiplier(progState.progressPercent, progState.currentSequence);

  // Update the horizon strand with the current color and brightness
  setStrandColor(horizon, c.g * brightMult, c.r * brightMult, c.b * brightMult, c.w * brightMult);
}

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("CloudyMountain Initializing...");

  // Initialize I2C bus
  Wire.begin();
  delay(100);

  // Initialize MPR121 touch sensor
  if (!touchSensor.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring!");
    while (1);
  }
  Serial.println("MPR121 found!");

  // Stop the sensor before configuring
  touchSensor.writeRegister(MPR121_ECR, 0x00);
  delay(10);

  // Soft reset
  touchSensor.writeRegister(MPR121_SOFTRESET, 0x63);
  delay(10);

  // Configure MPR121 registers for proper operation
  // Set baseline filtering control register
  touchSensor.writeRegister(MPR121_MHDR, 0x01);
  touchSensor.writeRegister(MPR121_NHDR, 0x01);
  touchSensor.writeRegister(MPR121_NCLR, 0x0E);
  touchSensor.writeRegister(MPR121_FDLR, 0x00);

  touchSensor.writeRegister(MPR121_MHDF, 0x01);
  touchSensor.writeRegister(MPR121_NHDF, 0x05);
  touchSensor.writeRegister(MPR121_NCLF, 0x01);
  touchSensor.writeRegister(MPR121_FDLF, 0x00);

  touchSensor.writeRegister(MPR121_NHDT, 0x00);
  touchSensor.writeRegister(MPR121_NCLT, 0x00);
  touchSensor.writeRegister(MPR121_FDLT, 0x00);

  // Set debounce
  touchSensor.writeRegister(MPR121_DEBOUNCE, 0x00);

  // Set config registers - increase charge current for better sensitivity
  touchSensor.writeRegister(MPR121_CONFIG1, 0x10); // FFI = 6, CDC = 16uA
  touchSensor.writeRegister(MPR121_CONFIG2, 0x24); // CDT = 0.5uS, ESI = 4ms

  // Auto-configure registers
  touchSensor.writeRegister(MPR121_AUTOCONFIG0, 0x0B);
  touchSensor.writeRegister(MPR121_UPLIMIT, 200);
  touchSensor.writeRegister(MPR121_LOWLIMIT, 130);
  touchSensor.writeRegister(MPR121_TARGETLIMIT, 180);

  // Set touch and release thresholds for all 12 pads
  for (uint8_t i = 0; i < 12; i++) {
    touchSensor.setThresholds(12, 6);  // touch threshold: 12, release threshold: 6
  }

  // Enable all electrodes - this also runs auto-config
  touchSensor.writeRegister(MPR121_ECR, 0x8F);  // Start with first 12 electrodes

  delay(200);  // Give it time to stabilize and auto-configure

  Serial.println("MPR121 configured!");

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

  // Initialize stars pin as output and turn on by default
  pinMode(STARS_PIN, OUTPUT);
  digitalWrite(STARS_PIN, HIGH);  // Turn stars ON by default
  Serial.println("Stars turned ON");

  Serial.println("Setup complete!");
}

void loop() {
  // Get current touch state from MPR121
  currentTouched = touchSensor.touched();

  // Debug: Print raw touch values every second (commented out after testing)
  // static unsigned long lastDebugTime = 0;
  // if (millis() - lastDebugTime > 1000) {
  //   Serial.print("Touch state: 0x"); Serial.println(currentTouched, HEX);
  //   // Print filtered data and baseline for first 4 pads
  //   for (uint8_t i = 0; i < 4; i++) {
  //     Serial.print("Pad "); Serial.print(i);
  //     Serial.print(" - Filtered: "); Serial.print(touchSensor.filteredData(i));
  //     Serial.print(", Baseline: "); Serial.print(touchSensor.baselineData(i));
  //     Serial.print(", Delta: "); Serial.println(touchSensor.baselineData(i) - touchSensor.filteredData(i));
  //   }
  //   Serial.println();
  //   lastDebugTime = millis();
  // }

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

  // Update progression if animating
  if (progState.isAnimating) {
    updateProgression();
  }

  // Small delay to avoid overwhelming the serial output
  delay(10);
}

// Handle touch events for each pad
void handleTouch(uint8_t pad) {
  Serial.print("handleTouch called for pad: ");
  Serial.println(pad);

  // Add your touch handling logic here
  // Example: Light up different strands based on pad number
  switch(pad) {
    case 0:  // Start full sunrise sequence (2min hold + 20min progression)
      Serial.println("Starting full sunrise sequence (2min hold + 20min progression)");
      transitionToSequence(SEQ_SUNRISE_HOLD);
      break;
    case 1:  // Reserved for DAYTIME mode (will implement later)
      Serial.println("DAYTIME mode - not yet implemented");
      break;
    case 2:  // Start sunset sequence (20min progression from day to night)
      Serial.println("Starting sunset sequence (20min progression)");
      transitionToSequence(SEQ_SUNSET_PROG);
      break;
    case 3:  // Display current progression with brightness multiplier
      {
        float palPos = percentToPalettePosition(progState.progressPercent);
        ColorGRBW c = interpolateColor(palPos);

        // Apply brightness multiplier based on progression
        float brightMult = getBrightnessMultiplier(progState.progressPercent, progState.currentSequence);

        setStrandColor(horizon, c.g * brightMult, c.r * brightMult, c.b * brightMult, c.w * brightMult);

        Serial.print("Displaying progression - Percent: ");
        Serial.print(progState.progressPercent);
        Serial.print("%, Palette pos: ");
        Serial.print(palPos);
        Serial.print(", Brightness mult: ");
        Serial.println(brightMult);
      }
      break;
    case 4:  // RESET/OFF - Turn everything off and reset state
      Serial.println("RESET/OFF - Turning off all LEDs and stopping sequences");

      // Stop any running sequences
      progState.isAnimating = false;
      progState.currentSequence = SEQ_OFF;
      progState.progressPercent = 0.0;

      // Turn off all LED strands
      setStrandColor(cloud1, 0, 0, 0, 0);
      setStrandColor(cloud2, 0, 0, 0, 0);
      setStrandColor(cloud3, 0, 0, 0, 0);
      setStrandColor(horizon, 0, 0, 0, 0);

      Serial.println("All LEDs off, state reset to SEQ_OFF");
      break;
    case 5:  // Was case 0 - Test function
      Serial.println("Setting CLOUD_1 to white");
      setStrandColor(cloud1, 255, 255, 255, 255);
      break;
    case 6:  // Was case 1 - Test function
      Serial.println("Setting CLOUD_2 to white");
      setStrandColor(cloud2, 255, 255, 255, 255);
      break;
    case 7:  // Palette test - cycles through all 32 colors
      {
        // Stop any running progressions so they don't overwrite our test color
        progState.isAnimating = false;

        static uint8_t testColorIndex = 0;
        Serial.print("PAD 7 TOUCHED - Palette color index: ");
        Serial.print(testColorIndex);
        ColorGRBW c = getPaletteColor(testColorIndex);
        Serial.print(" -> GRBW(");
        Serial.print(c.g); Serial.print(",");
        Serial.print(c.r); Serial.print(",");
        Serial.print(c.b); Serial.print(",");
        Serial.print(c.w); Serial.println(")");
        setStrandColor(horizon, c.g, c.r, c.b, c.w);
        testColorIndex = (testColorIndex + 1) % 32;  // Cycle through 32-color palette
      }
      break;
    case 8:  // Was case 3 - Test function
      Serial.println("Setting HORIZON to white");
      setStrandColor(horizon, 255, 255, 255, 255);
      break;
    case 9:  // Was case 2 - Test function
      Serial.println("Setting CLOUD_3 to white");
      setStrandColor(cloud3, 255, 255, 255, 255);
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
      // Reserved for SUNRISE sequence
      break;
    case 1:
      // Reserved for DAYTIME mode
      break;
    case 2:
      // Reserved for SUNSET sequence
      break;
    case 3:
      // Reserved for STORM mode
      break;
    case 4:
      // Reserved for RESET/OFF
      break;
    case 5:  // Was case 0 - Test function
      setStrandColor(cloud1, 0, 0, 0, 0);
      break;
    case 6:  // Was case 1 - Test function
      setStrandColor(cloud2, 0, 0, 0, 0);
      break;
    case 7:  // Palette test - no release action needed
      break;
    case 8:  // Was case 3 - Test function
      setStrandColor(horizon, 0, 0, 0, 0);
      break;
    case 9:  // Was case 2 - Test function
      setStrandColor(cloud3, 0, 0, 0, 0);
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

// Apply brightness scaling to a single strand
void applyBrightnessToStrand(Adafruit_NeoPixel &strand, float scale) {
  for (int i = 0; i < strand.numPixels(); i++) {
    uint32_t color = strand.getPixelColor(i);
    uint8_t g = ((color >> 24) & 0xFF) * scale;
    uint8_t r = ((color >> 16) & 0xFF) * scale;
    uint8_t b = ((color >> 8) & 0xFF) * scale;
    uint8_t w = (color & 0xFF) * scale;
    strand.setPixelColor(i, strand.Color(g, r, b, w));
  }
}

// Helper function to set entire strand to one color
void setStrandColor(Adafruit_NeoPixel &strand, uint8_t green, uint8_t red, uint8_t blue, uint8_t white) {
  for (int i = 0; i < strand.numPixels(); i++) {
    strand.setPixelColor(i, strand.Color(green, red, blue, white));
  }

  // After updating, check if we need to scale brightness
  applyBrightnessLimit();

  // If scaling is needed, apply scale to all strands
  if (brightnessScale < 1.0) {
    applyBrightnessToStrand(cloud1, brightnessScale);
    applyBrightnessToStrand(cloud2, brightnessScale);
    applyBrightnessToStrand(cloud3, brightnessScale);
    applyBrightnessToStrand(horizon, brightnessScale);
  }

  // Show all strands to keep them in sync
  cloud1.show();
  cloud2.show();
  cloud3.show();
  horizon.show();
}
