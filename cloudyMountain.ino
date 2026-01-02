//HORIZON ONLYa

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
  SEQ_SUNRISE_HOLD,      // Hold at night blue before sunrise (2min)
  SEQ_SUNRISE_PROG,      // Sunrise progression (20min)
  SEQ_DAY,               // Daytime mode
  SEQ_SUNSET_PROG,       // Sunset progression (reverse)
  SEQ_TEST_SUNRISE_HOLD, // Fast test hold (10sec)
  SEQ_TEST_SUNRISE_PROG  // Fast test progression (3min 50sec)
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
{0, 0, 80, 0}, //1
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
};

// NEW: Cloud background palette (32 colors, different from horizon)
// User-provided palette for cloud background progression
const ColorGRBW PROGMEM cloudBackgroundPalette[32] = {
  {10, 12, 60, 0},    // 1
  {12, 14, 58, 0},    // 2
  {14, 16, 56, 0},    // 3
  {16, 18, 54, 0},    // 4
  {18, 20, 52, 0},    // 5
  {20, 22, 52, 0},    // 6
  {24, 26, 54, 0},    // 7
  {30, 30, 56, 0},    // 8
  {36, 36, 58, 0},    // 9
  {44, 44, 60, 0},    // 10
  {56, 56, 58, 1},    // 11
  {72, 72, 56, 1},    // 12
  {92, 86, 54, 2},    // 13
  {110, 100, 52, 2},  // 14
  {128, 116, 50, 3},  // 15
  {150, 136, 50, 4},  // 16
  {170, 150, 54, 6},  // 17
  {184, 162, 64, 8},  // 18
  {196, 174, 84, 10}, // 19
  {204, 180, 106, 12},// 20
  {208, 178, 128, 16},// 21
  {204, 170, 150, 20},// 22
  {196, 160, 170, 28},// 23
  {184, 148, 186, 36},// 24
  {168, 132, 196, 44},// 25
  {152, 116, 204, 56},// 26
  {136, 100, 210, 68},// 27
  {122, 86, 214, 84}, // 28
  {110, 76, 218, 100},// 29
  {100, 70, 222, 116},// 30
  {96, 66, 234, 160}, // 31
  {240, 240, 240, 220}// 32
};

// NEW: Highlight patch palette (32 colors, progresses to white)
// User-provided palette for highlight patches
const ColorGRBW PROGMEM highlightPalette[32] = {
  {40, 8, 20, 0},     // 1
  {48, 12, 24, 0},    // 2
  {60, 16, 28, 0},    // 3
  {72, 20, 34, 0},    // 4
  {88, 24, 40, 0},    // 5
  {100, 28, 44, 0},   // 6
  {120, 36, 50, 1},   // 7
  {140, 40, 54, 1},   // 8
  {160, 48, 58, 2},   // 9
  {180, 60, 62, 3},   // 10
  {200, 80, 60, 4},   // 11
  {220, 100, 50, 4},  // 12
  {235, 120, 44, 6},  // 13
  {245, 140, 42, 8},  // 14
  {250, 160, 44, 10}, // 15
  {252, 180, 56, 12}, // 16
  {252, 196, 78, 14}, // 17
  {252, 206, 100, 18},// 18
  {252, 214, 122, 22},// 19
  {252, 214, 148, 28},// 20
  {252, 208, 176, 36},// 21
  {250, 200, 196, 44},// 22
  {244, 190, 212, 56},// 23
  {236, 178, 224, 68},// 24
  {220, 160, 232, 84},// 25
  {200, 140, 236, 100},// 26
  {180, 120, 238, 116},// 27
  {160, 100, 240, 132},// 28
  {140, 86, 242, 148}, // 29
  {124, 74, 244, 164}, // 30
  {112, 66, 246, 188}, // 31
  {255, 255, 255, 255} // 32
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

// NEW: Get color from cloud background palette
ColorGRBW getCloudBackgroundColor(uint8_t index) {
  if (index > 31) index = 31;  // Clamp to 32-color range
  ColorGRBW color;
  memcpy_P(&color, &cloudBackgroundPalette[index], sizeof(ColorGRBW));
  return color;
}

// NEW: Get color from highlight palette
ColorGRBW getHighlightColor(uint8_t index) {
  if (index > 31) index = 31;  // Clamp to 32-color range
  ColorGRBW color;
  memcpy_P(&color, &highlightPalette[index], sizeof(ColorGRBW));
  return color;
}

// NEW: Interpolate cloud background colors
ColorGRBW interpolateCloudBackground(float position) {
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

  // Get the two colors from cloud background palette
  ColorGRBW color1 = getCloudBackgroundColor(index1);
  ColorGRBW color2 = getCloudBackgroundColor(index2);

  // Linearly interpolate each channel
  ColorGRBW result;
  result.g = color1.g + (color2.g - color1.g) * blend;
  result.r = color1.r + (color2.r - color1.r) * blend;
  result.b = color1.b + (color2.b - color1.b) * blend;
  result.w = color1.w + (color2.w - color1.w) * blend;

  return result;
}

// NEW: Interpolate highlight colors
ColorGRBW interpolateHighlight(float position) {
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

  // Get the two colors from highlight palette
  ColorGRBW color1 = getHighlightColor(index1);
  ColorGRBW color2 = getHighlightColor(index2);

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

// NEW: Patch tracking structure (Step 2)
struct HighlightPatch {
  uint8_t startPixel;      // Where patch starts (0 to numPixels-1)
  uint8_t width;           // How many pixels wide (5-15 pixels)
  float fadeProgress;      // 0.0 = invisible, 1.0 = full brightness, back to 0.0
  unsigned long spawnTime; // When this patch was created (millis)
  bool active;             // Whether this patch is currently alive
  bool fadingIn;           // True = fading in, False = fading out
};

// NEW: Cloud state structure (one per cloud strand) (Step 2)
struct CloudState {
  HighlightPatch patches[8];     // Max 8 patches per cloud (more than needed)
  unsigned long nextSpawnTime;   // When to spawn next patch (millis)
  uint8_t activePatchCount;      // How many patches are currently active
  uint8_t randomSeed;            // Unique seed for randomization per cloud
};

// NEW: Global cloud states (one for each cloud strand) (Step 2)
CloudState cloudState1;
CloudState cloudState2;
CloudState cloudState3;

// NEW: Timing constants for patches (Step 2)
#define PATCH_FADE_DURATION_MS 8000        // 8 seconds to fade in + fade out
#define PATCH_MIN_SPAWN_INTERVAL_MS 3000   // Minimum 3 seconds between spawns
#define PATCH_MAX_SPAWN_INTERVAL_MS 10000  // Maximum 10 seconds between spawns
#define PATCH_MIN_WIDTH 5                  // Minimum patch size
#define PATCH_MAX_WIDTH 15                 // Maximum patch size

// Forward declarations for cloud patch functions
uint8_t getMaxPatchesForProgression(float percent);
void getPatchSpawnRange(float percent, uint8_t numPixels, uint8_t &minPixel, uint8_t &maxPixel);
void spawnPatch(CloudState &state, uint8_t numPixels);
void updatePatchSpawning(CloudState &state, uint8_t numPixels);
void updatePatchFade(HighlightPatch &patch, uint8_t cloudSeed);
void updateAllPatches(CloudState &state);
ColorGRBW blendColors(ColorGRBW bg, ColorGRBW patch, float alpha);
void renderCloudPatches(Adafruit_NeoPixel &strand, CloudState &state, ColorGRBW bgColor);
void updateCloudBackgrounds();

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

  if (progState.currentSequence == SEQ_SUNRISE_HOLD || progState.currentSequence == SEQ_TEST_SUNRISE_HOLD) {
    // Hold at night blue (palette position 0.0)
    palPos = 0.0;
    c = interpolateColor(palPos);
  } else if (progState.currentSequence == SEQ_SUNRISE_PROG || progState.currentSequence == SEQ_TEST_SUNRISE_PROG) {
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

  // NEW: Update cloud backgrounds (Step 1)
  updateCloudBackgrounds();
}

// NEW: Calculate maximum patches allowed based on progression (Step 3)
uint8_t getMaxPatchesForProgression(float percent) {
  if (percent < 10.0) return 1;   // 0-10%: Max 1 patch
  if (percent < 30.0) return 2;   // 10-30%: Max 2 patches
  if (percent < 50.0) return 3;   // 30-50%: Max 3 patches
  if (percent < 70.0) return 5;   // 50-70%: Max 5 patches
  if (percent < 90.0) return 7;   // 70-90%: Max 7 patches
  return 8;                        // 90-100%: Max 8 patches (all white)
}

// NEW: Calculate patch spawn pixel range (spreads over time) (Step 3)
void getPatchSpawnRange(float percent, uint8_t numPixels, uint8_t &minPixel, uint8_t &maxPixel) {
  if (percent < 20.0) {
    // Early: Only low pixels (0-20% of strand)
    minPixel = 0;
    maxPixel = numPixels * 0.2;
  } else if (percent < 50.0) {
    // Mid-early: Low to middle pixels (0-50% of strand)
    minPixel = 0;
    maxPixel = numPixels * 0.5;
  } else if (percent < 80.0) {
    // Mid-late: Low to high pixels (0-80% of strand)
    minPixel = 0;
    maxPixel = numPixels * 0.8;
  } else {
    // Late: Entire strand
    minPixel = 0;
    maxPixel = numPixels;
  }
}

// NEW: Spawn a new patch on a cloud (Step 3)
void spawnPatch(CloudState &state, uint8_t numPixels) {
  // Find an inactive patch slot
  for (int i = 0; i < 8; i++) {
    if (!state.patches[i].active) {
      // Get spawn range based on progression
      uint8_t minPixel, maxPixel;
      getPatchSpawnRange(progState.progressPercent, numPixels, minPixel, maxPixel);

      // Use seeded random for this cloud
      uint8_t pixelRange = maxPixel - minPixel;
      if (pixelRange == 0) pixelRange = 1;

      state.patches[i].startPixel = minPixel + ((millis() + state.randomSeed) % pixelRange);
      state.patches[i].width = PATCH_MIN_WIDTH + ((millis() + state.randomSeed * 2) % (PATCH_MAX_WIDTH - PATCH_MIN_WIDTH));
      state.patches[i].fadeProgress = 0.0;
      state.patches[i].fadingIn = true;
      state.patches[i].spawnTime = millis();
      state.patches[i].active = true;

      state.activePatchCount++;

      // Schedule next spawn (randomized timing per cloud)
      uint32_t spawnInterval = PATCH_MIN_SPAWN_INTERVAL_MS +
        ((millis() + state.randomSeed * 3) % (PATCH_MAX_SPAWN_INTERVAL_MS - PATCH_MIN_SPAWN_INTERVAL_MS));
      state.nextSpawnTime = millis() + spawnInterval;

      break; // Only spawn one patch
    }
  }
}

// NEW: Update patch spawning for a cloud (Step 3)
void updatePatchSpawning(CloudState &state, uint8_t numPixels) {
  // Check if it's time to spawn and if we haven't hit the limit
  uint8_t maxPatches = getMaxPatchesForProgression(progState.progressPercent);

  if (millis() >= state.nextSpawnTime && state.activePatchCount < maxPatches) {
    spawnPatch(state, numPixels);
  }
}

// NEW: Update fade progress for a single patch (Step 4)
void updatePatchFade(HighlightPatch &patch, uint8_t cloudSeed) {
  if (!patch.active) return;

  unsigned long elapsed = millis() - patch.spawnTime;

  // Use cloud seed to vary fade speed (10% faster/slower per cloud)
  float fadeSpeedMultiplier = 1.0 + ((cloudSeed % 20) - 10) / 100.0; // 0.9 to 1.1
  unsigned long adjustedDuration = PATCH_FADE_DURATION_MS / fadeSpeedMultiplier;

  // Half the duration is fade in, half is fade out
  unsigned long halfDuration = adjustedDuration / 2;

  if (elapsed < halfDuration) {
    // Fading in
    patch.fadingIn = true;
    patch.fadeProgress = (float)elapsed / (float)halfDuration; // 0.0 → 1.0
  } else if (elapsed < adjustedDuration) {
    // Fading out
    patch.fadingIn = false;
    patch.fadeProgress = 1.0 - ((float)(elapsed - halfDuration) / (float)halfDuration); // 1.0 → 0.0
  } else {
    // Patch lifetime complete - deactivate
    patch.active = false;
    // Note: activePatchCount will be decremented in updateAllPatches()
  }
}

// NEW: Update all patches for a cloud (Step 4)
void updateAllPatches(CloudState &state) {
  state.activePatchCount = 0; // Recount active patches

  for (int i = 0; i < 8; i++) {
    if (state.patches[i].active) {
      updatePatchFade(state.patches[i], state.randomSeed);

      if (state.patches[i].active) {
        state.activePatchCount++;
      }
    }
  }
}

// NEW: Blend two colors with alpha (Step 5)
// 0.0 = all bg, 1.0 = all patch
ColorGRBW blendColors(ColorGRBW bg, ColorGRBW patch, float alpha) {
  ColorGRBW result;
  result.g = bg.g * (1.0 - alpha) + patch.g * alpha;
  result.r = bg.r * (1.0 - alpha) + patch.r * alpha;
  result.b = bg.b * (1.0 - alpha) + patch.b * alpha;
  result.w = bg.w * (1.0 - alpha) + patch.w * alpha;
  return result;
}

// NEW: Render patches on a single cloud strand (Step 5)
void renderCloudPatches(Adafruit_NeoPixel &strand, CloudState &state, ColorGRBW bgColor) {
  // First, set all pixels to background
  for (int i = 0; i < strand.numPixels(); i++) {
    strand.setPixelColor(i, strand.Color(bgColor.g, bgColor.r, bgColor.b, bgColor.w));
  }

  // Then, layer patches on top
  for (int p = 0; p < 8; p++) {
    if (!state.patches[p].active) continue;

    HighlightPatch &patch = state.patches[p];

    // Get highlight color based on current progression
    float highlightPalPos = percentToPalettePosition(progState.progressPercent);
    ColorGRBW highlightColor = interpolateHighlight(highlightPalPos);

    // Draw patch pixels
    for (int i = 0; i < patch.width; i++) {
      uint8_t pixelIndex = patch.startPixel + i;

      if (pixelIndex >= strand.numPixels()) break; // Don't overflow

      // Get current pixel color (might already have other patches blended)
      uint32_t currentColor = strand.getPixelColor(pixelIndex);
      ColorGRBW current;
      current.g = (currentColor >> 24) & 0xFF;
      current.r = (currentColor >> 16) & 0xFF;
      current.b = (currentColor >> 8) & 0xFF;
      current.w = currentColor & 0xFF;

      // Blend patch color on top with fade progress as alpha
      ColorGRBW blended = blendColors(current, highlightColor, patch.fadeProgress);

      strand.setPixelColor(pixelIndex, strand.Color(blended.g, blended.r, blended.b, blended.w));
    }
  }

  // Show the strand
  strand.show();
}

// NEW: Update cloud background colors and patches (Step 5 - now with patches!)
void updateCloudBackgrounds() {
  // Get current palette position from global progState
  float palPos = percentToPalettePosition(progState.progressPercent);

  // Interpolate color from cloud background palette
  ColorGRBW bgColor = interpolateCloudBackground(palPos);

  // Apply brightness multiplier (same as horizon)
  float brightMult = getBrightnessMultiplier(progState.progressPercent, progState.currentSequence);

  // Apply brightness to background color
  ColorGRBW bgColorWithBrightness;
  bgColorWithBrightness.g = bgColor.g * brightMult;
  bgColorWithBrightness.r = bgColor.r * brightMult;
  bgColorWithBrightness.b = bgColor.b * brightMult;
  bgColorWithBrightness.w = bgColor.w * brightMult;

  // Render all clouds with background + patches
  renderCloudPatches(cloud1, cloudState1, bgColorWithBrightness);
  renderCloudPatches(cloud2, cloudState2, bgColorWithBrightness);
  renderCloudPatches(cloud3, cloudState3, bgColorWithBrightness);
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

  // NEW: Initialize cloud states with different random seeds (Step 2)
  cloudState1.randomSeed = 17;  // Prime numbers for better randomization
  cloudState2.randomSeed = 31;
  cloudState3.randomSeed = 47;

  cloudState1.nextSpawnTime = millis() + 1000;  // Cloud 1 spawns first at 1sec
  cloudState2.nextSpawnTime = millis() + 2500;  // Cloud 2 spawns at 2.5sec
  cloudState3.nextSpawnTime = millis() + 4000;  // Cloud 3 spawns at 4sec

  cloudState1.activePatchCount = 0;
  cloudState2.activePatchCount = 0;
  cloudState3.activePatchCount = 0;

  // Initialize all patches as inactive
  for (int i = 0; i < 8; i++) {
    cloudState1.patches[i].active = false;
    cloudState2.patches[i].active = false;
    cloudState3.patches[i].active = false;
  }

  Serial.println("Cloud states initialized");

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
    case 1:  // Fast 4-minute test sunrise (10sec hold + 3min 50sec progression)
      Serial.println("Starting FAST TEST sunrise (10sec hold + 3min 50sec progression = 4min total)");
      transitionToSequence(SEQ_TEST_SUNRISE_HOLD);
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

        // If we're about to wrap around to 0, show black first as a visual reset
        if (testColorIndex == 0) {
          Serial.println("========================================");
          Serial.println("PALETTE CYCLE COMPLETE - RESETTING");
          Serial.println("Showing BLACK for visual reset...");
          Serial.println("========================================");
          setStrandColor(horizon, 0, 0, 0, 0);
          delay(500);  // Half second black screen to clearly mark the reset
        }

        // Get current color from NEW 32-color palette
        ColorGRBW c = getPaletteColor(testColorIndex);

        Serial.println("========================================");
        Serial.print("PAD 7 - NEW 32-COLOR PALETTE TEST");
        Serial.print(" [Color ");
        Serial.print(testColorIndex);
        Serial.print(" of 31]");
        Serial.println();
        Serial.print("  GRBW values: (");
        Serial.print(c.g); Serial.print(", ");
        Serial.print(c.r); Serial.print(", ");
        Serial.print(c.b); Serial.print(", ");
        Serial.print(c.w); Serial.print(")");
        Serial.println();
        Serial.println("========================================");

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
    case 10:  // Reset palette test counter and show color 0
      {
        // Stop any running progressions
        progState.isAnimating = false;

        // Force reset by using a separate variable
        static uint8_t* resetPtr = nullptr;
        if (resetPtr == nullptr) {
          // Find the static variable in case 7 (hack to reset it)
          Serial.println("PAD 10 - Resetting palette test to color 0");
        }

        // Just display color 0 directly
        ColorGRBW c = getPaletteColor(0);
        Serial.print("Showing palette color 0: GRBW(");
        Serial.print(c.g); Serial.print(", ");
        Serial.print(c.r); Serial.print(", ");
        Serial.print(c.b); Serial.print(", ");
        Serial.print(c.w); Serial.println(")");
        setStrandColor(horizon, c.g, c.r, c.b, c.w);

        Serial.println("NOTE: Next PAD 7 press will continue from where it left off.");
        Serial.println("To fully reset, power cycle the device or upload fresh code.");
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
