//HORIZON ONLY

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

// Fast test durations (for pad 1 quick testing)
#define TEST_HOLD_DURATION_MS 10000          // 10 seconds hold
#define TEST_PROGRESSION_DURATION_MS 230000  // 3min 50sec progression (total 4min)

// Initialize NeoPixel objects (SK6812 RGBW type - but chips are wired as GRBW)
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

// Cloud zone structure - divides each cloud into thirds for patch placement
struct CloudZones {
  uint8_t lowerStart, lowerEnd;
  uint8_t middleStart, middleEnd;
  uint8_t upperStart, upperEnd;
};

// Global cloud zone definitions (calculated in setup())
CloudZones cloud1Zones, cloud2Zones, cloud3Zones;

// Forward declarations
typedef enum {
  SEQ_OFF,
  SEQ_SUNRISE_HOLD,      // Hold at night blue before sunrise (2min)
  SEQ_SUNRISE_PROG,      // Sunrise progression (20min)
  SEQ_DAY,               // Daytime mode
  SEQ_SUNSET_PROG,       // Sunset progression (reverse)
  SEQ_TEST_SUNRISE_HOLD, // Fast test hold (10sec)
  SEQ_TEST_SUNRISE_PROG  // Fast test progression (3min 50sec)
} SequenceState;

// Color structure for RGBW LEDs (SK6812RGBW chip order)
struct ColorGRBW {
  uint8_t r, g, b, w;  // RGBW order for SK6812RGBW
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

// NEW 40-color sunset/sunrise palette stored in PROGMEM to save SRAM
// User-provided palette for smoother transitions
const ColorGRBW PROGMEM sunsetPalette[40] = {
/*{0, 0, 80, 0}, //1
{4, 3, 72, 0}, //2
{8, 6, 68, 0}, //3
{12, 9, 62, 0}, //4
{18, 12, 58, 0}, //5
{28, 16, 56, 0}, //6
{40, 22, 56, 0}, //7
{56, 30, 58, 1}, //8
{72, 40, 60, 2}, //9
{88, 50, 62, 3}, //10
{100, 60, 54, 4}, //11 
{140, 92, 40, 6}, //13
{160, 108, 36, 8}, //14
{176, 124, 36, 10}, //15
{192, 140, 38, 12}, //16
{204, 154, 44, 14}, //17 
{214, 166, 58, 16}, //18
{222, 176, 78, 20}, //19
{228, 182, 102, 24}, //20 
{232, 186, 124, 32}, //21
{232, 182, 148, 44}, //22
{228, 174, 172, 60}, //23
{220, 162, 192, 76}, //24
{208, 146, 208, 96}, //25
{192, 128, 220, 112}, //26
{172, 110, 224, 128}, //27
{152, 92, 228, 144}, //28
{136, 78, 232, 160}, //29
{122, 68, 236, 176}, //30
{112, 62, 240, 192}, //31
{108, 60, 244, 208} //32
*/
{0, 0, 80, 0}, //0
{1, 0, 69, 3}, //1
{2, 0, 62, 6}, //2
{3, 0, 53, 9}, //3
{6, 0, 46, 12}, //4
{12, 0, 40, 16}, //5
{18, 0, 34, 22}, //6
{26, 0, 28, 29}, //7
{32, 0, 20, 38}, //8
{38, 0, 12, 47}, //9
{39, 60, 8, 48}, //10
{40, 0, 5, 53}, //11
{42, 1, 0, 57}, //12
{50, 7, 0, 56}, //13
{57, 13, 0, 55}, //14
{64, 18, 0, 54}, //15
{71, 24, 0, 53}, //16
{78, 29, 0, 52}, //17
{84, 35, 0, 51}, //18
{91, 40, 0, 50}, //19
{98, 45, 0, 49}, //20
{104, 50, 0, 48}, //21
{111, 56, 0, 47}, //22
{117, 61, 0, 46}, //23
{123, 66, 0, 45}, //24
{130, 71, 0, 44}, //25
{136, 76, 0, 43}, //26
{141, 80, 0, 43}, //27
{148, 85, 0, 42}, //28
{154, 90, 0, 41}, //29
{160, 95, 0, 40}, //30
{142, 90, 0, 78}, //31
{111, 172, 0, 122}, //32
{80, 55, 0, 165}, //33
{40, 28, 0, 210}, //34
{0, 0, 45, 210}, //35
{0, 15, 100, 150}, //36
{0, 28, 160, 90}, //37
{0, 30, 100, 130} //38
};

// Cloud color palette - 39 colors for cloud patch animations (RGBW order)
// User-refined palette optimized for LED display
const ColorGRBW PROGMEM cloudPalette[39] = {
  {1, 0, 7, 0},         // 0: Pale purple
  {1, 1, 5, 0},         // 1
  {2, 1, 6, 0},         // 2
  {1, 1, 8, 0},         // 3
  {3, 0, 15, 0},        // 4: Blues
  {1, 2, 16, 0},        // 5
  {12, 2, 4, 0},        // 6: Oranges
  {6, 2, 0, 0},         // 7
  {8, 2, 0, 0},         // 8
  {15, 4, 0, 0},        // 9
  {2, 0, 12, 0},        // 10
  {4, 0, 20, 0},        // 11
  {14, 2, 0, 0},        // 12: Dark reddish orange
  {16, 0, 0, 2},        // 13
  {16, 1, 2, 0},        // 14
  {13, 4, 0, 0},        // 15: Pale orange
  {22, 7, 0, 0},        // 16
  {22, 8, 0, 5},        // 17
  {30, 3, 0, 0},        // 18
  {30, 6, 0, 8},        // 19
  {35, 8, 1, 0},        // 20
  {40, 3, 0, 0},        // 21
  {40, 30, 1, 0},       // 22
  {40, 25, 0, 0},       // 23
  {50, 8, 0, 0},        // 24
  {40, 10, 1, 5},       // 25
  {50, 6, 0, 20},       // 26
  {50, 3, 0, 0},        // 27
  {50, 0, 0, 30},       // 28
  {75, 0, 10, 60},      // 29
  {75, 8, 10, 90},      // 30
  {80, 13, 8, 110},     // 31
  {200, 0, 25, 100},    // 32
  {200, 0, 25, 50},     // 33
  {200, 0, 75, 200},    // 34
  {150, 145, 175, 150}, // 35
  {170, 150, 165, 140}, // 36
  {140, 145, 175, 160}, // 37
  {100, 100, 100, 200}  // 38
};

// Helper function to read horizon color from PROGMEM
ColorGRBW getPaletteColor(uint8_t index) {
  if (index > 38) index = 38;  // Clamp to 39-color range (0-38)
  ColorGRBW color;
  memcpy_P(&color, &sunsetPalette[index], sizeof(ColorGRBW));
  return color;
}

// Helper function to read cloud color from PROGMEM
ColorGRBW getCloudPaletteColor(uint8_t index) {
  if (index > 38) index = 38;  // Clamp to 39-color range (0-38)
  ColorGRBW color;
  memcpy_P(&color, &cloudPalette[index], sizeof(ColorGRBW));
  return color;
}

// Interpolate between palette colors for smooth transitions
// Takes a float position from 0.0 to 38.0 (39 usable colors)
// Returns a blended color between the two adjacent palette entries
ColorGRBW interpolateColor(float position) {
  // Clamp position to valid range
  if (position < 0.0) position = 0.0;
  if (position > 38.0) position = 38.0;

  // Get the two palette indices to blend between
  uint8_t index1 = (uint8_t)position;  // Floor
  uint8_t index2 = index1 + 1;

  // Handle edge case at the end of the palette
  if (index2 > 38) index2 = 38;

  // Calculate blend factor (0.0 to 1.0 between the two colors)
  float blend = position - (float)index1;

  // Get the two colors
  ColorGRBW color1 = getPaletteColor(index1);
  ColorGRBW color2 = getPaletteColor(index2);

  // Linearly interpolate each channel (RGBW order)
  ColorGRBW result;
  result.r = color1.r + (color2.r - color1.r) * blend;
  result.g = color1.g + (color2.g - color1.g) * blend;
  result.b = color1.b + (color2.b - color1.b) * blend;
  result.w = color1.w + (color2.w - color1.w) * blend;

  return result;
}

// State tracking structure for horizon progression
struct ProgressionState {
  SequenceState currentSequence;
  float progressPercent;         // 0.0 to 100.0
  unsigned long phaseStartTime;  // When current phase started (millis)
  bool isAnimating;              // Whether progression is active
  bool dayModeDisplayed;         // Track if DAY mode has been displayed
};

// Global progression state
ProgressionState progState = {SEQ_OFF, 0.0, 0, false, false};

// Cloud patch system - tracks individual color patches fading in on clouds
#define MAX_PATCHES_PER_CLOUD 15  // Max simultaneous patches per cloud strand

struct CloudPatch {
  bool active;                    // Is this patch slot in use?
  uint8_t centerPixel;            // Center position of patch
  uint8_t patchSize;              // Total pixels in patch (4-9, or special first patch)
  uint8_t targetColorIndex;       // Index into cloudPalette

  // Fade-in timing
  unsigned long fadeStartTime;    // When fade began (millis)
  uint16_t fadeDuration;          // How long fade takes (4000-7000 ms)
  float fadeProgress;             // 0.0 = transparent, 1.0 = opaque

  // Blending state
  ColorGRBW targetColor;          // Final color (read from cloudPalette)
  ColorGRBW previousColors[9];    // Colors that were at these pixels before patch (max 9 pixels)
};

struct CloudState {
  CloudPatch patches[MAX_PATCHES_PER_CLOUD];
  ColorGRBW currentPixelColors[46];  // Track current state (max 46 for CLOUD_3)
  uint8_t numPixels;                  // Actual pixel count for this cloud
};

// Global cloud states
CloudState cloud1State, cloud2State, cloud3State;

// Helper: Convert percentage (0-100%) to palette position (0.0-38.0)
float percentToPalettePosition(float percent) {
  if (percent < 0.0) percent = 0.0;
  if (percent > 100.0) percent = 100.0;
  return (percent / 100.0) * 38.0;  // Map to 39 usable colors (0-38)
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
    case SEQ_TEST_SUNRISE_HOLD:
      // Hold at minimum brightness (night blue)
      return BRIGHTNESS_MIN_FACTOR;

    case SEQ_SUNRISE_PROG:
    case SEQ_TEST_SUNRISE_PROG:
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
  progState.dayModeDisplayed = false;  // Reset flag when transitioning

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
    case SEQ_TEST_SUNRISE_HOLD:
      phaseDuration = TEST_HOLD_DURATION_MS;
      break;
    case SEQ_TEST_SUNRISE_PROG:
      phaseDuration = TEST_PROGRESSION_DURATION_MS;
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

      case SEQ_TEST_SUNRISE_HOLD:
        Serial.println("TEST: Hold complete! Starting fast sunrise progression...");
        transitionToSequence(SEQ_TEST_SUNRISE_PROG);
        return;  // Exit and let next loop iteration handle the new state

      case SEQ_TEST_SUNRISE_PROG:
        Serial.println("TEST: Sunrise progression complete! Transitioning to DAY mode...");
        transitionToSequence(SEQ_DAY);
        return;  // Exit and let next loop iteration handle the new state

      case SEQ_SUNSET_PROG:
        Serial.println("Sunset progression complete! Ending at night blue...");
        progState.isAnimating = false;
        break;

      case SEQ_DAY:
        // DAY mode is static - set display and stop animating
        // Display the final daytime state (palette 38.0, full brightness, full spread)
        {
          ColorGRBW c = interpolateColor(38.0);
          int centerPixel = HORIZON_PIXELS / 2;

          // All pixels at full brightness in DAY mode (matching end of sunrise)
          for (int i = 0; i < HORIZON_PIXELS; i++) {
            horizon.setPixelColor(i, horizon.Color(c.g, c.r, c.b, c.w));
          }
          horizon.show();

          // Apply brightness limiting to other strands only
          applyBrightnessLimit();
          if (brightnessScale < 1.0) {
            applyBrightnessToStrand(cloud1, brightnessScale);
            applyBrightnessToStrand(cloud2, brightnessScale);
            applyBrightnessToStrand(cloud3, brightnessScale);
          }
          cloud1.show();
          cloud2.show();
          cloud3.show();
        }

        progState.isAnimating = false;
        Serial.println("DAY mode active (static) - displaying palette 39 at full brightness.");
        break;

      default:
        progState.isAnimating = false;
        Serial.println("Sequence complete!");
        break;
    }
  }

  // Special handling for DAY mode - display immediately and stop animating
  if (progState.currentSequence == SEQ_DAY && !progState.dayModeDisplayed) {
    // Test: read palette entry 39 directly
    ColorGRBW testColor = getPaletteColor(39);
    Serial.print("Direct palette[39] read - G:");
    Serial.print(testColor.g);
    Serial.print(" R:");
    Serial.print(testColor.r);
    Serial.print(" B:");
    Serial.print(testColor.b);
    Serial.print(" W:");
    Serial.println(testColor.w);

    // Test: Try reading from different indices
    Serial.println("Reading palette indices 36-39:");
    for(int idx = 36; idx < 40; idx++) {
      ColorGRBW test = getPaletteColor(idx);
      Serial.print("  [");
      Serial.print(idx);
      Serial.print("] G:");
      Serial.print(test.g);
      Serial.print(" R:");
      Serial.print(test.r);
      Serial.print(" B:");
      Serial.print(test.b);
      Serial.print(" W:");
      Serial.println(test.w);
    }

    // FIX: Use palette index 38 (the 39th entry) as the daytime color
    // Index 39 appears to be reading past array bounds
    ColorGRBW c = getPaletteColor(38);

    Serial.print("Using hardcoded daytime color - G:");
    Serial.print(c.g);
    Serial.print(" R:");
    Serial.print(c.r);
    Serial.print(" B:");
    Serial.print(c.b);
    Serial.print(" W:");
    Serial.println(c.w);

    // All pixels at full brightness in DAY mode
    for (int i = 0; i < HORIZON_PIXELS; i++) {
      horizon.setPixelColor(i, horizon.Color(c.g, c.r, c.b, c.w));
    }

    // Show horizon immediately (before brightness limiting which might affect other strands)
    horizon.show();

    Serial.println("Horizon strand updated and shown");

    progState.dayModeDisplayed = true;
    progState.isAnimating = false;
    Serial.println("DAY mode activated immediately - displaying palette 39 at full brightness.");
    return;
  }

  // Calculate color based on current state
  float palPos;
  ColorGRBW c;

  if (progState.currentSequence == SEQ_SUNRISE_HOLD || progState.currentSequence == SEQ_TEST_SUNRISE_HOLD) {
    // Hold at night blue (palette position 0.0)
    palPos = 0.0;
    c = interpolateColor(palPos);
  } else if (progState.currentSequence == SEQ_SUNRISE_PROG || progState.currentSequence == SEQ_TEST_SUNRISE_PROG) {
    // Progress through palette based on percentage (0% → 100% = palette 0.0 → 38.0)
    palPos = percentToPalettePosition(progState.progressPercent);
    c = interpolateColor(palPos);
  } else if (progState.currentSequence == SEQ_SUNSET_PROG) {
    // Reverse progress through palette (0% → 100% = palette 38.0 → 0.0)
    palPos = percentToPalettePosition(100.0 - progState.progressPercent);
    c = interpolateColor(palPos);
  } else if (progState.currentSequence == SEQ_DAY) {
    // Full daylight (palette position 38.0)
    palPos = 38.0;
    c = interpolateColor(palPos);
  } else {
    // Default to night blue
    palPos = 0.0;
    c = interpolateColor(palPos);
  }

  // Apply brightness multiplier (keep for future cloud use)
  float brightMult = getBrightnessMultiplier(progState.progressPercent, progState.currentSequence);

  // Calculate horizon-specific brightness based on palette position
  float horizonBrightness;
  if (progState.currentSequence == SEQ_SUNRISE_PROG || progState.currentSequence == SEQ_TEST_SUNRISE_PROG) {
    // Two-stage brightness curve for horizon during sunrise
    if (palPos <= 33.0) {
      // Stage 1: Slow ramp from 12.5% to 40% (colors 0-33)
      horizonBrightness = 0.125 + (palPos / 33.0) * (0.40 - 0.125);
    } else if (palPos <= 36.0) {
      // Stage 2: Fast ramp from 40% to 80% (colors 33-36)
      horizonBrightness = 0.40 + ((palPos - 33.0) / 3.0) * (0.80 - 0.40);
    } else {
      // Stage 3: Continue to 100% (colors 36-39)
      horizonBrightness = 0.80 + ((palPos - 36.0) / 3.0) * (1.0 - 0.80);
    }
  } else {
    // For non-sunrise sequences, use standard brightness
    horizonBrightness = brightMult;
  }

  // Print color information to serial every second
  static unsigned long lastPrintTime = 0;
  if (millis() - lastPrintTime > 1000) {
    Serial.print("Progress: ");
    Serial.print(progState.progressPercent, 1);
    Serial.print("% | Palette pos: ");
    Serial.print(palPos, 2);
    Serial.print(" | Color index: ");
    Serial.print((uint8_t)palPos);
    Serial.print(" | Horizon brightness: ");
    Serial.print(horizonBrightness * 100, 1);
    Serial.println("%");
    lastPrintTime = millis();
  }

  // Update the horizon strand with horizon-specific brightness
  // Special handling for colors 32-38: spread brightness from center outward
  if ((progState.currentSequence == SEQ_SUNRISE_PROG || progState.currentSequence == SEQ_TEST_SUNRISE_PROG)
      && palPos >= 32.0) {

    int centerPixel = HORIZON_PIXELS / 2;  // Middle LED (pixel 27 for 54 pixels)

    // Calculate which "ring" of pixels should be fully bright based on palette position
    // At color 32: center pixel (ring 0)
    // At color 33: center + 1 pixel on each side (ring 1)
    // At color 34: center + 2 pixels on each side (ring 2)
    // ... continuing until color 39 when all are bright
    float spreadProgress = palPos - 32.0;  // 0.0 at color 32, 7.0 at color 39
    int fullyBrightRing = (int)spreadProgress;  // Which ring is fully bright
    float ringBlend = spreadProgress - fullyBrightRing;  // How far into next ring (0.0-1.0)

    // Apply per-pixel brightness
    for (int i = 0; i < HORIZON_PIXELS; i++) {
      int distanceFromCenter = abs(i - centerPixel);
      float pixelBrightness;

      if (distanceFromCenter < fullyBrightRing) {
        // This pixel is in a fully bright ring
        pixelBrightness = 1.0;
      } else if (distanceFromCenter == fullyBrightRing) {
        // This pixel is in the currently transitioning ring
        pixelBrightness = horizonBrightness + (1.0 - horizonBrightness) * ringBlend;
      } else if (distanceFromCenter == fullyBrightRing + 1) {
        // This pixel is in the next ring, starting to brighten
        pixelBrightness = horizonBrightness + (1.0 - horizonBrightness) * ringBlend * 0.5;
      } else {
        // This pixel is still at base horizon brightness
        pixelBrightness = horizonBrightness;
      }

      // Set the pixel color with calculated brightness (RGBW order)
      horizon.setPixelColor(i, horizon.Color(
        c.r * pixelBrightness,
        c.g * pixelBrightness,
        c.b * pixelBrightness,
        c.w * pixelBrightness
      ));
    }

    // Show the horizon strand (other strands handled by power limiting below)
    horizon.show();

    // Apply brightness limiting to other strands only
    applyBrightnessLimit();
    if (brightnessScale < 1.0) {
      applyBrightnessToStrand(cloud1, brightnessScale);
      applyBrightnessToStrand(cloud2, brightnessScale);
      applyBrightnessToStrand(cloud3, brightnessScale);
    }
    cloud1.show();
    cloud2.show();
    cloud3.show();

  } else {
    // Normal uniform brightness for all other cases (RGBW order)
    setStrandColor(horizon, c.r * horizonBrightness, c.g * horizonBrightness, c.b * horizonBrightness, c.w * horizonBrightness);
  }
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

  // Calculate cloud zone boundaries (divide each cloud into thirds)
  // CLOUD_1: 32 pixels → lower: 0-10 (11px), middle: 11-21 (11px), upper: 22-31 (10px)
  cloud1Zones.lowerStart = 0;
  cloud1Zones.lowerEnd = 10;
  cloud1Zones.middleStart = 11;
  cloud1Zones.middleEnd = 21;
  cloud1Zones.upperStart = 22;
  cloud1Zones.upperEnd = 31;

  // CLOUD_2: 45 pixels → lower: 0-14 (15px), middle: 15-29 (15px), upper: 30-44 (15px)
  cloud2Zones.lowerStart = 0;
  cloud2Zones.lowerEnd = 14;
  cloud2Zones.middleStart = 15;
  cloud2Zones.middleEnd = 29;
  cloud2Zones.upperStart = 30;
  cloud2Zones.upperEnd = 44;

  // CLOUD_3: 46 pixels → lower: 0-15 (16px), middle: 16-30 (15px), upper: 31-45 (15px)
  cloud3Zones.lowerStart = 0;
  cloud3Zones.lowerEnd = 15;
  cloud3Zones.middleStart = 16;
  cloud3Zones.middleEnd = 30;
  cloud3Zones.upperStart = 31;
  cloud3Zones.upperEnd = 45;

  Serial.println("Cloud zones calculated:");
  Serial.print("  CLOUD_1: lower[0-10] middle[11-21] upper[22-31]");
  Serial.println();
  Serial.print("  CLOUD_2: lower[0-14] middle[15-29] upper[30-44]");
  Serial.println();
  Serial.print("  CLOUD_3: lower[0-15] middle[16-30] upper[31-45]");
  Serial.println();

  // Initialize cloud states
  cloud1State.numPixels = CLOUD_1_PIXELS;
  cloud2State.numPixels = CLOUD_2_PIXELS;
  cloud3State.numPixels = CLOUD_3_PIXELS;

  // Set all pixels to starting deep blue (cloud color 0)
  ColorGRBW startColor = getCloudPaletteColor(0);
  for (int i = 0; i < CLOUD_1_PIXELS; i++) {
    cloud1State.currentPixelColors[i] = startColor;
  }
  for (int i = 0; i < CLOUD_2_PIXELS; i++) {
    cloud2State.currentPixelColors[i] = startColor;
  }
  for (int i = 0; i < CLOUD_3_PIXELS; i++) {
    cloud3State.currentPixelColors[i] = startColor;
  }

  // Initialize all patches to inactive
  for (int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
    cloud1State.patches[i].active = false;
    cloud2State.patches[i].active = false;
    cloud3State.patches[i].active = false;
  }

  Serial.println("Cloud states initialized (all patches inactive, starting in deep blue)");

  // Initialize stars pin as output and turn on by default
  pinMode(STARS_PIN, OUTPUT);
  digitalWrite(STARS_PIN, HIGH);  // Turn stars ON by default
  Serial.println("Stars turned ON");

  Serial.println("Setup complete!");
}

void loop() {
  // Check for serial input (for pad 6 custom color testing)
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();  // Remove whitespace

    // Parse R,G,B,W format
    int r = -1, g = -1, b = -1, w = -1;
    int commaIndex1 = input.indexOf(',');
    int commaIndex2 = input.indexOf(',', commaIndex1 + 1);
    int commaIndex3 = input.indexOf(',', commaIndex2 + 1);

    if (commaIndex1 > 0 && commaIndex2 > 0 && commaIndex3 > 0) {
      r = input.substring(0, commaIndex1).toInt();
      g = input.substring(commaIndex1 + 1, commaIndex2).toInt();
      b = input.substring(commaIndex2 + 1, commaIndex3).toInt();
      w = input.substring(commaIndex3 + 1).toInt();

      // Validate range (0-255)
      if (r >= 0 && r <= 255 && g >= 0 && g <= 255 &&
          b >= 0 && b <= 255 && w >= 0 && w <= 255) {
        Serial.println("========================================");
        Serial.print("Setting CLOUD_2 to RGBW: (");
        Serial.print(r); Serial.print(", ");
        Serial.print(g); Serial.print(", ");
        Serial.print(b); Serial.print(", ");
        Serial.print(w); Serial.print(")");
        Serial.println();
        Serial.println("========================================");

        setStrandColor(cloud2, r, g, b, w);
      } else {
        Serial.println("ERROR: Values must be 0-255");
      }
    } else {
      Serial.println("ERROR: Invalid format. Use: R,G,B,W (example: 0,0,0,255)");
    }
  }

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
    case 1:  // Fast 4-minute test sunrise (10sec hold + 3min 50sec progression)
      Serial.println("Starting FAST TEST sunrise (10sec hold + 3min 50sec progression = 4min total)");
      transitionToSequence(SEQ_TEST_SUNRISE_HOLD);
      break;
    case 2:  // Start sunset sequence (20min progression from day to night)
      Serial.println("Starting sunset sequence (20min progression)");
      transitionToSequence(SEQ_SUNSET_PROG);
      break;
    case 3:  // Jump directly to daytime mode
      Serial.println("Jumping to DAY mode");
      transitionToSequence(SEQ_DAY);
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
    case 6:  // Custom color test - set via serial input
      Serial.println("========================================");
      Serial.println("PAD 6 - CUSTOM COLOR TEST (CLOUD_2)");
      Serial.println("Enter RGBW values in Serial Monitor:");
      Serial.println("Format: R,G,B,W (example: 0,0,0,255)");
      Serial.println("Waiting for input...");
      Serial.println("========================================");

      // Set flag to indicate we're waiting for color input
      progState.isAnimating = false;  // Stop any running animations
      break;
    case 7:  // CLOUD color palette test - cycles through all 39 cloud colors on CLOUD_1
      {
        // Stop any running progressions so they don't overwrite our test color
        progState.isAnimating = false;

        static uint8_t cloudTestColorIndex = 0;
        static bool showingBlackReset = false;

        // After showing color 38 (the last color), show black before resetting to 0
        if (cloudTestColorIndex == 0 && !showingBlackReset) {
          Serial.println("========================================");
          Serial.println("CLOUD PALETTE CYCLE COMPLETE - SHOWING BLACK");
          Serial.println("Press pad 7 again to restart from color 0");
          Serial.println("========================================");
          setStrandColor(cloud1, 0, 0, 0, 0);
          showingBlackReset = true;
          return;  // Wait for next press
        }

        // If we just showed black, reset the flag and start from 0
        if (showingBlackReset) {
          showingBlackReset = false;
          cloudTestColorIndex = 0;
        }

        // Get current color from cloud palette
        ColorGRBW c = getCloudPaletteColor(cloudTestColorIndex);

        Serial.println("========================================");
        Serial.print("PAD 7 - CLOUD COLOR PALETTE TEST (CLOUD_1)");
        Serial.println();
        Serial.print("  Cloud Color Index: ");
        Serial.print(cloudTestColorIndex);
        Serial.print(" of 38");
        Serial.println();
        Serial.print("  RGBW values: (");
        Serial.print(c.r); Serial.print(", ");
        Serial.print(c.g); Serial.print(", ");
        Serial.print(c.b); Serial.print(", ");
        Serial.print(c.w); Serial.print(")");
        Serial.println();
        Serial.println("========================================");

        // Display color on CLOUD_1 (RGBW order)
        setStrandColor(cloud1, c.r, c.g, c.b, c.w);

        // Increment to next color
        cloudTestColorIndex++;

        // After showing the last color (38), next press will trigger black
        if (cloudTestColorIndex >= 39) {
          cloudTestColorIndex = 0;
        }
      }
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
      // Reserved for FAST TEST sunrise
      break;
    case 2:
      // Reserved for SUNSET sequence
      break;
    case 3:
      // Reserved for DAY mode
      break;
    case 4:
      // Reserved for RESET/OFF
      break;
    case 5:  // Test: Turn off CLOUD_1
      setStrandColor(cloud1, 0, 0, 0, 0);
      break;
    case 6:  // Test: Turn off CLOUD_2
      setStrandColor(cloud2, 0, 0, 0, 0);
      break;
    case 7:  // Palette test - no release action needed
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

// Apply brightness scaling to a single strand (RGBW order)
void applyBrightnessToStrand(Adafruit_NeoPixel &strand, float scale) {
  for (int i = 0; i < strand.numPixels(); i++) {
    uint32_t color = strand.getPixelColor(i);
    uint8_t r = ((color >> 24) & 0xFF) * scale;  // Red is MSB for RGBW
    uint8_t g = ((color >> 16) & 0xFF) * scale;  // Green
    uint8_t b = ((color >> 8) & 0xFF) * scale;   // Blue
    uint8_t w = (color & 0xFF) * scale;           // White is LSB
    strand.setPixelColor(i, strand.Color(r, g, b, w));
  }
}

// Helper function to set entire strand to one color (RGBW order for parameters)
// NOTE: For NEO_RGBW, Color() expects (R,G,B,W) and we pass params in that order
void setStrandColor(Adafruit_NeoPixel &strand, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) {
  for (int i = 0; i < strand.numPixels(); i++) {
    strand.setPixelColor(i, strand.Color(red, green, blue, white));
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
