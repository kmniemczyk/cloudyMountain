// make star changes
//test the storm more and it made pad 8 the turn it on pad and that needs to change
//then fix the end of the dawn sequence. maybe brightness issue?
//need to invert cloud top /bottom

#include <Wire.h>
#include <Adafruit_MPR121.h>
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

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

// Storm timing constants (production)
#define STORM_DIM_DURATION_MS 60000           // 1 minute dim to 25%
#define STORM_CLEAR_DURATION_MS 60000         // 1 minute return to 100%
#define STORM_MIN_DURATION_MS 300000          // 5 minutes minimum
#define STORM_MAX_DURATION_MS 600000          // 10 minutes maximum
#define STORM_MIN_CHECK_INTERVAL_MS 3600000   // 1 hour between checks
#define STORM_TRIGGER_PROBABILITY 30          // 30% chance when eligible

// Lightning timing
#define LIGHTNING_MIN_INTERVAL_MS 800         // Min time between strikes
#define LIGHTNING_MAX_INTERVAL_MS 8000        // Max time between strikes
#define LIGHTNING_FLASH_DURATION_MS 150       // Flash duration
#define LIGHTNING_MULTI_FLASH_CHANCE 40       // 40% chance of 2-3 flashes

// Test mode (fast timing for pad 5)
#define TEST_STORM_DIM_DURATION_MS 5000       // 5 seconds
#define TEST_STORM_ACTIVE_DURATION_MS 30000   // 30 seconds
#define TEST_STORM_CLEAR_DURATION_MS 5000     // 5 seconds

// Initialize NeoPixel objects (SK6812 RGBW type - but chips are wired as GRBW)
Adafruit_NeoPixel cloud1 = Adafruit_NeoPixel(CLOUD_1_PIXELS, CLOUD_1_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel cloud2 = Adafruit_NeoPixel(CLOUD_2_PIXELS, CLOUD_2_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel cloud3 = Adafruit_NeoPixel(CLOUD_3_PIXELS, CLOUD_3_PIN, NEO_GRBW + NEO_KHZ800);
Adafruit_NeoPixel horizon = Adafruit_NeoPixel(HORIZON_PIXELS, HORIZON_PIN, NEO_GRBW + NEO_KHZ800);

// Stars PWM control (fade in/out over 5 seconds)
#define STAR_FADE_DURATION_MS 5000  // 5 second fade

// Star fade state
struct StarState {
  float currentBrightness;      // 0.0 to 1.0
  float targetBrightness;       // 0.0 to 1.0
  unsigned long fadeStartTime;  // When fade began
  bool isFading;                // Currently fading?
};

// Global star state
StarState starState = {0.0, 0.0, 0, false};

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
  SEQ_SUNRISE_PROG,      // Sunrise progression (configurable)
  SEQ_DAY_HOLD,          // Timed daylight hold (configurable)
  SEQ_DAY,               // Daytime mode (static, infinite)
  SEQ_NIGHT,             // Night mode (static, low brightness)
  SEQ_SUNSET_PROG,       // Sunset progression (configurable)
  SEQ_TEST_SUNRISE_HOLD, // Fast test hold (10sec)
  SEQ_TEST_SUNRISE_PROG, // Fast test progression (3min 50sec)
  SEQ_STORM_DIM,         // Dimming clouds to 25% (60 seconds)
  SEQ_STORM_ACTIVE,      // Active lightning strikes (5-10 minutes)
  SEQ_STORM_CLEAR        // Returning to normal brightness (60 seconds)
} SequenceState;

// Color structure for RGBW LEDs (SK6812RGBW chip order)
struct ColorGRBW {
  uint8_t r, g, b, w;  // RGBW order for SK6812RGBW
};


// NEW 39-color sunset/sunrise palette stored in PROGMEM to save SRAM
// User-provided palette for smoother transitions (indices 0-38)
const ColorGRBW PROGMEM sunsetPalette[39] = {

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
  {50, 3, 0, 30},        // 27
  {50, 0, 0, 45},       // 28
  {75, 0, 10, 60},      // 29
  {75, 8, 10, 105},      // 30
  {80, 13, 8, 130},     // 31
  {200, 0, 25, 140},    // 32
  {200, 0, 25, 155},     // 33
  {200, 59, 125, 165},    // 34
  {170, 170, 175, 175}, // 35
  {180, 180, 180, 180}, // 36
  {180, 180, 180, 180}, // 37
  {180, 180, 180, 180}  // 38
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

// Storm color palette - 8 colors for storm clouds and lightning intensities
// Stored in PROGMEM to save SRAM
const ColorGRBW PROGMEM stormPalette[8] = {
  {25, 25, 35, 10},      // 0: Dark stormy blue-gray
  {30, 30, 40, 15},      // 1: Lighter storm cloud
  {35, 35, 45, 20},      // 2: Medium storm cloud
  {255, 255, 255, 255},  // 3: Bright white (max intensity)
  {240, 240, 255, 240},  // 4: Bright lightning (close) - increased
  {200, 200, 230, 200},  // 5: Medium lightning - increased
  {160, 160, 200, 160},  // 6: Dim lightning (distant) - increased
  {120, 120, 160, 120}   // 7: Very dim lightning - increased
};

// Helper function to read storm color from PROGMEM
ColorGRBW getStormPaletteColor(uint8_t index) {
  if (index > 7) index = 7;  // Clamp to 8-color range (0-7)
  ColorGRBW color;
  memcpy_P(&color, &stormPalette[index], sizeof(ColorGRBW));
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

// Storm state structure for tracking storm progression and lightning
struct StormState {
  bool stormEnabled;                    // Can storms auto-trigger?
  unsigned long lastStormCheckTime;     // Last random check
  unsigned long stormPhaseStartTime;    // Phase timing
  unsigned long stormDuration;          // Active phase duration
  unsigned long nextLightningTime;      // When next strike happens
  unsigned long lightningFlashStartTime;// Current flash timing
  uint8_t lightningFlashCount;          // Multi-flash tracking
  uint8_t currentFlashNumber;           // Which flash in sequence
  uint8_t strikeType;                   // 0=single, 1=multi-cloud
  uint8_t strikeCloud;                  // Target cloud (single)
  uint8_t strikeCloudStart;             // Arc start (multi)
  uint8_t strikeCloudEnd;               // Arc end (multi)
  uint8_t strikeIntensity;              // Color index 3-7
  uint8_t strikePixels[3];              // Affected pixels
  uint8_t strikeNumPixels;              // How many pixels lit
  float preStormBrightness;             // Save for restore
};

// Global storm state
StormState stormState = {false, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 3, {0,0,0}, 0, 1.0};

// ========== BLE CONFIGURATION AND STATE ==========

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MODE_CONTROL_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CYCLE_CONFIG_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define SCHEDULE_CONFIG_UUID "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define STORM_CONFIG_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define TIME_SYNC_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26ae"
#define CURRENT_STATE_UUID  "beb5483e-36e1-4688-b7f5-ea07361b26ac"
#define CONFIG_ECHO_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26ad"

// BLE server and characteristics pointers
BLEServer* pServer = NULL;
BLECharacteristic* pModeControlChar = NULL;
BLECharacteristic* pCycleConfigChar = NULL;
BLECharacteristic* pScheduleConfigChar = NULL;
BLECharacteristic* pStormConfigChar = NULL;
BLECharacteristic* pTimeSyncChar = NULL;
BLECharacteristic* pCurrentStateChar = NULL;
BLECharacteristic* pConfigEchoChar = NULL;

// BLE connection state
bool deviceConnected = false;
bool oldDeviceConnected = false;

// BLE Control State - manages all app-controlled features
struct BLEControlState {
  // Pause/Resume state
  bool isPaused;
  unsigned long pausedTimeRemaining;  // ms remaining when paused
  unsigned long pauseStartTime;       // When pause was initiated

  // Configurable cycle duration
  uint16_t sunCycleDayDurationMinutes;  // Total day cycle duration (default 120 = 2 hours)
  uint16_t sunriseDurationMinutes;      // Sunrise progression time (default 15)
  uint16_t sunsetDurationMinutes;       // Sunset progression time (default 15)
  uint32_t calculatedDaylightDuration;  // Calculated daylight hold in ms
  uint32_t calculatedSunriseDuration;   // Sunrise duration in ms
  uint32_t calculatedSunsetDuration;    // Sunset duration in ms

  // Scheduling
  bool scheduleEnabled;
  uint8_t scheduleHour;               // 0-23
  uint8_t scheduleMinute;             // 0-59
  uint8_t scheduleDayMask;            // Bit mask for days (bit 0=Sunday, bit 6=Saturday)
  bool waitingForScheduledStart;      // True when waiting for next scheduled time

  // Night mode control
  bool enableNightAfterSunset;
  bool currentlyInAutoNight;          // True when in auto-night mode

  // Storm control
  bool stormEnabledDay;
  bool stormEnabledNight;

  // Current mode from app
  uint8_t currentAppMode;             // 0=off, 1=cycle, 2=night, 3=day
};

// Global BLE control state with defaults
BLEControlState bleControl = {
  false,    // isPaused
  0,        // pausedTimeRemaining
  0,        // pauseStartTime
  120,      // sunCycleDayDurationMinutes (2 hours default)
  15,       // sunriseDurationMinutes (15 min default)
  15,       // sunsetDurationMinutes (15 min default)
  5400000,  // calculatedDaylightDuration (90 min = 2hr - 15min - 15min, in ms)
  900000,   // calculatedSunriseDuration (15 min in ms)
  900000,   // calculatedSunsetDuration (15 min in ms)
  false,    // scheduleEnabled
  6,        // scheduleHour (default 6 AM)
  0,        // scheduleMinute
  0xFF,     // scheduleDayMask (every day)
  false,    // waitingForScheduledStart
  false,    // enableNightAfterSunset
  false,    // currentlyInAutoNight
  false,    // stormEnabledDay
  false,    // stormEnabledNight
  0         // currentAppMode
};

// Time synchronization state (app sends time on connect)
struct TimeSync {
  bool synchronized;
  unsigned long syncMillis;      // millis() when synchronized
  uint32_t syncEpoch;            // Unix timestamp at sync (seconds since 1970)
  uint8_t syncDayOfWeek;         // 0=Sunday, 1=Monday, ..., 6=Saturday
};

// Global time sync state
TimeSync timeSync = {false, 0, 0, 0};

// ========== END BLE CONFIGURATION ==========

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

// Enum for zone selection (used in Phase 3)
enum CloudZone { ZONE_LOWER, ZONE_MIDDLE, ZONE_UPPER };

// Forward declarations for Phase 2 and Phase 3 functions
// (Arduino IDE preprocessor needs these for complex struct types)
ColorGRBW blendColorsWithDip(ColorGRBW oldColor, ColorGRBW newColor, float fadeProgress);
void updateCloudPatches(CloudState &cloudState, Adafruit_NeoPixel &strand);
void createTestPatch(CloudState* cloudState, uint8_t centerPixel, uint8_t patchSize, uint8_t colorIndex, uint16_t fadeDuration);
CloudZone selectZoneForHorizonColor(uint8_t horizonColorIndex);
void getZoneBounds(CloudZones &zones, CloudZone zone, uint8_t &start, uint8_t &end);
void selectPatchColors(uint8_t horizonColorIndex, uint8_t *selectedColors);
void triggerCloudPatchesForHorizonColor(uint8_t horizonColorIndex);

// Helper: Convert percentage (0-100%) to palette position (0.0-38.0)
float percentToPalettePosition(float percent) {
  if (percent < 0.0) percent = 0.0;
  if (percent > 100.0) percent = 100.0;
  return (percent / 100.0) * 38.0;  // Map to 39 usable colors (0-38)
}

// ========== PHASE 2: CLOUD PATCH BLENDING SYSTEM ==========

// Blend two GRBW colors with transparent-to-opaque fade and subtle brightness dip
// fadeProgress: 0.0 = fully oldColor, 1.0 = fully newColor
ColorGRBW blendColorsWithDip(ColorGRBW oldColor, ColorGRBW newColor, float fadeProgress) {
  // Calculate brightness dip factor (gentle 80% minimum at midpoint)
  float dipFactor = 1.0;
  if (fadeProgress > 0.0 && fadeProgress < 1.0) {
    // Dip curve: starts at 1.0, dips to 0.8 at 0.5, returns to 1.0
    float dipAmount = 0.2;  // 20% dip
    dipFactor = 1.0 - (dipAmount * sin(fadeProgress * PI));
  }

  // Blend each channel with dip applied
  ColorGRBW result;
  result.g = ((oldColor.g * (1.0 - fadeProgress)) + (newColor.g * fadeProgress)) * dipFactor;
  result.r = ((oldColor.r * (1.0 - fadeProgress)) + (newColor.r * fadeProgress)) * dipFactor;
  result.b = ((oldColor.b * (1.0 - fadeProgress)) + (newColor.b * fadeProgress)) * dipFactor;
  result.w = ((oldColor.w * (1.0 - fadeProgress)) + (newColor.w * fadeProgress)) * dipFactor;

  return result;
}

// Update all active patches for one cloud and render to LED strand
void updateCloudPatches(CloudState &cloudState, Adafruit_NeoPixel &strand) {
  unsigned long currentTime = millis();

  // Update fade progress for all active patches
  for (int p = 0; p < MAX_PATCHES_PER_CLOUD; p++) {
    if (!cloudState.patches[p].active) continue;

    CloudPatch &patch = cloudState.patches[p];

    // Calculate fade progress based on elapsed time
    unsigned long elapsed = currentTime - patch.fadeStartTime;
    patch.fadeProgress = min(1.0f, (float)elapsed / (float)patch.fadeDuration);

    // Calculate which pixels this patch affects
    int halfSize = patch.patchSize / 2;
    int startPixel = patch.centerPixel - halfSize;
    int endPixel = startPixel + patch.patchSize;

    // Clamp to valid range
    startPixel = max(0, startPixel);
    endPixel = min((int)cloudState.numPixels, endPixel);

    // Update each affected pixel
    for (int i = startPixel; i < endPixel; i++) {
      int patchLocalIndex = i - startPixel;

      // Blend from previous color to target color
      ColorGRBW blended = blendColorsWithDip(
        patch.previousColors[patchLocalIndex],
        patch.targetColor,
        patch.fadeProgress
      );

      // Update cloud state with blended color
      cloudState.currentPixelColors[i] = blended;
    }

    // If patch has completed fading in, mark it as inactive to free the slot
    if (patch.fadeProgress >= 1.0f) {
      patch.active = false;
    }
  }

  // Apply final colors to LED strand
  for (int i = 0; i < cloudState.numPixels; i++) {
    ColorGRBW c = cloudState.currentPixelColors[i];
    strand.setPixelColor(i, strand.Color(c.r, c.g, c.b, c.w));  // RGBW order (same as horizon)
  }

  strand.show();
}

// Test helper: Create a single patch manually for testing
void createTestPatch(CloudState* cloudState, uint8_t centerPixel, uint8_t patchSize,
                     uint8_t colorIndex, uint16_t fadeDuration) {
  // Find empty patch slot
  int patchSlot = -1;
  for (int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
    if (!cloudState->patches[i].active) {
      patchSlot = i;
      break;
    }
  }

  if (patchSlot == -1) {
    Serial.println("ERROR: No available patch slots!");
    return;
  }

  CloudPatch &patch = cloudState->patches[patchSlot];

  // Initialize patch data
  patch.active = true;
  patch.centerPixel = centerPixel;
  patch.patchSize = patchSize;
  patch.targetColorIndex = colorIndex;
  patch.fadeStartTime = millis();
  patch.fadeDuration = fadeDuration;
  patch.fadeProgress = 0.0;

  // Read target color from cloud palette
  patch.targetColor = getCloudPaletteColor(colorIndex);

  // Capture current colors at patch locations (for blending)
  int halfSize = patchSize / 2;
  int startPixel = centerPixel - halfSize;

  for (int i = 0; i < patchSize; i++) {
    int pixelIndex = startPixel + i;
    if (pixelIndex >= 0 && pixelIndex < cloudState->numPixels) {
      patch.previousColors[i] = cloudState->currentPixelColors[pixelIndex];
    } else {
      // Out of bounds, use deep blue default (color 0)
      patch.previousColors[i] = getCloudPaletteColor(0);
    }
  }

  Serial.println("========================================");
  Serial.println("TEST PATCH CREATED:");
  Serial.print("  Center pixel: "); Serial.println(centerPixel);
  Serial.print("  Patch size: "); Serial.println(patchSize);
  Serial.print("  Target color index: "); Serial.println(colorIndex);
  Serial.print("  Target color RGBW: (");
  Serial.print(patch.targetColor.r); Serial.print(", ");
  Serial.print(patch.targetColor.g); Serial.print(", ");
  Serial.print(patch.targetColor.b); Serial.print(", ");
  Serial.print(patch.targetColor.w); Serial.println(")");
  Serial.print("  Fade duration: "); Serial.print(fadeDuration); Serial.println("ms");
  Serial.println("========================================");
}

// ========== END PHASE 2 ==========

// ========== PHASE 3: PATCH TRIGGERING LOGIC ==========

// Track last horizon color to detect changes
uint8_t lastHorizonColorIndex = 0;

// Select which zone to place a patch based on horizon color progression
CloudZone selectZoneForHorizonColor(uint8_t horizonColorIndex) {
  // Early sunrise (1-12): patches only in lower zone
  if (horizonColorIndex >= 1 && horizonColorIndex <= 12) {
    return ZONE_LOWER;
  }
  // Mid sunrise (13-25): patches in lower and middle zones
  else if (horizonColorIndex >= 13 && horizonColorIndex <= 25) {
    return (random(100) < 50) ? ZONE_LOWER : ZONE_MIDDLE;
  }
  // Late sunrise (26-37): patches mixed across all zones
  else if (horizonColorIndex >= 26 && horizonColorIndex <= 37) {
    int r = random(100);
    if (r < 33) return ZONE_LOWER;
    else if (r < 66) return ZONE_MIDDLE;
    else return ZONE_UPPER;
  }
  // Color 38+ is handled separately in triggerCloudPatchesForHorizonColor()
  // with full-cloud coverage
  else {
    // Fallback: evenly distributed
    int r = random(100);
    if (r < 33) return ZONE_LOWER;
    else if (r < 66) return ZONE_MIDDLE;
    else return ZONE_UPPER;
  }
}

// Get zone boundaries for a given cloud and zone type
void getZoneBounds(CloudZones &zones, CloudZone zone, uint8_t &start, uint8_t &end) {
  switch(zone) {
    case ZONE_LOWER:
      start = zones.lowerStart;
      end = zones.lowerEnd;
      break;
    case ZONE_MIDDLE:
      start = zones.middleStart;
      end = zones.middleEnd;
      break;
    case ZONE_UPPER:
      start = zones.upperStart;
      end = zones.upperEnd;
      break;
  }
}

// Select 3 cloud colors for patch creation (2 of one color, 1 of another)
// Based on current horizon color, picks from eligible cloud palette colors
void selectPatchColors(uint8_t horizonColorIndex, uint8_t *selectedColors) {
  // Simplified mapping: Use cloud colors that are close to the horizon progression
  // For a more sophisticated approach, you could create a lookup table

  // For now, use a simple strategy:
  // - Use the same index as horizon (clamped to 0-38)
  // - Add variety by sometimes picking nearby colors

  uint8_t baseColor = min(horizonColorIndex, (uint8_t)38);

  // First color: use base color
  selectedColors[0] = baseColor;

  // Second color: same as first (creates 2 patches of same color)
  selectedColors[1] = baseColor;

  // Third color: nearby color for variety
  if (baseColor > 0 && random(100) < 50) {
    selectedColors[2] = baseColor - 1;  // Use previous color
  } else if (baseColor < 38) {
    selectedColors[2] = baseColor + 1;  // Use next color
  } else {
    selectedColors[2] = baseColor;  // At boundary, use same
  }
}

// Trigger cloud patches based on current horizon color
void triggerCloudPatchesForHorizonColor(uint8_t horizonColorIndex) {
  // Special case: For the final color (38), create full-cloud patches
  if (horizonColorIndex >= 38) {
    // CLOUD_1 - full coverage
    {
      uint8_t centerPixel = CLOUD_1_PIXELS / 2;  // Center of cloud
      uint8_t patchSize = CLOUD_1_PIXELS;         // Entire cloud
      uint8_t colorIndex = 38;                     // Final daytime white (200,200,200,200)
      uint16_t fadeDuration = 8000;                // 8 second fade for final transition

      createTestPatch(&cloud1State, centerPixel, patchSize, colorIndex, fadeDuration);
    }

    // CLOUD_2 - full coverage
    {
      uint8_t centerPixel = CLOUD_2_PIXELS / 2;
      uint8_t patchSize = CLOUD_2_PIXELS;
      uint8_t colorIndex = 38;
      uint16_t fadeDuration = 8000;                // 8 second fade

      createTestPatch(&cloud2State, centerPixel, patchSize, colorIndex, fadeDuration);
    }

    // CLOUD_3 - full coverage
    {
      uint8_t centerPixel = CLOUD_3_PIXELS / 2;
      uint8_t patchSize = CLOUD_3_PIXELS;
      uint8_t colorIndex = 38;
      uint16_t fadeDuration = 8000;                // 8 second fade

      createTestPatch(&cloud3State, centerPixel, patchSize, colorIndex, fadeDuration);
    }

    Serial.println("Created FULL-CLOUD patches for final daytime color (index 38) - 8 second fade");
    return;
  }

  // Normal behavior for all other colors
  // Select which zone to use based on horizon progression
  CloudZone zone = selectZoneForHorizonColor(horizonColorIndex);

  // Get 3 colors to use for patches (2 of one, 1 of another)
  uint8_t selectedColors[3];
  selectPatchColors(horizonColorIndex, selectedColors);

  // Create 1 patch on each of the 3 clouds (3 total patches)
  // Each cloud gets one randomly selected color from the 3 colors

  // CLOUD_1
  {
    uint8_t zoneStart, zoneEnd;
    getZoneBounds(cloud1Zones, zone, zoneStart, zoneEnd);

    // Random position within zone
    uint8_t centerPixel = random(zoneStart, zoneEnd + 1);

    // Random patch size (4-9 pixels)
    uint8_t patchSize = random(4, 10);

    // Random color from our 3 selected colors
    uint8_t colorIndex = selectedColors[random(3)];

    // Random fade duration (4000-7000ms)
    uint16_t fadeDuration = random(4000, 7001);

    createTestPatch(&cloud1State, centerPixel, patchSize, colorIndex, fadeDuration);
  }

  // CLOUD_2
  {
    uint8_t zoneStart, zoneEnd;
    getZoneBounds(cloud2Zones, zone, zoneStart, zoneEnd);

    uint8_t centerPixel = random(zoneStart, zoneEnd + 1);
    uint8_t patchSize = random(4, 10);
    uint8_t colorIndex = selectedColors[random(3)];
    uint16_t fadeDuration = random(4000, 7001);

    createTestPatch(&cloud2State, centerPixel, patchSize, colorIndex, fadeDuration);
  }

  // CLOUD_3
  {
    uint8_t zoneStart, zoneEnd;
    getZoneBounds(cloud3Zones, zone, zoneStart, zoneEnd);

    uint8_t centerPixel = random(zoneStart, zoneEnd + 1);
    uint8_t patchSize = random(4, 10);
    uint8_t colorIndex = selectedColors[random(3)];
    uint16_t fadeDuration = random(4000, 7001);

    createTestPatch(&cloud3State, centerPixel, patchSize, colorIndex, fadeDuration);
  }

  Serial.print("Created 3 cloud patches for horizon color ");
  Serial.print(horizonColorIndex);
  Serial.print(" in zone ");
  Serial.println(zone == ZONE_LOWER ? "LOWER" : (zone == ZONE_MIDDLE ? "MIDDLE" : "UPPER"));
}

// ========== END PHASE 3 ==========

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

  // Reset horizon color tracking for patch triggering
  lastHorizonColorIndex = 0;

  // Reset storm check timer when entering DAY mode
  if (newState == SEQ_DAY) {
    stormState.lastStormCheckTime = millis();
  }

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
      phaseDuration = bleControl.calculatedSunriseDuration;
      break;
    case SEQ_DAY_HOLD:
      phaseDuration = bleControl.calculatedDaylightDuration;
      break;
    case SEQ_SUNSET_PROG:
      phaseDuration = bleControl.calculatedSunsetDuration;
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

      case SEQ_DAY_HOLD:
        Serial.println("Daylight hold complete! Starting sunset progression...");
        transitionToSequence(SEQ_SUNSET_PROG);
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
        Serial.println("Sunset progression complete! Transitioning to NIGHT mode...");
        transitionToSequence(SEQ_NIGHT);
        return;  // Exit and let next loop iteration handle the new state

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

          // Set all cloud pixels to final daytime white (cloudPalette[38])
          ColorGRBW cloudDayColor = getCloudPaletteColor(38);

          // Update cloud state arrays
          for (int i = 0; i < CLOUD_1_PIXELS; i++) {
            cloud1State.currentPixelColors[i] = cloudDayColor;
          }
          for (int i = 0; i < CLOUD_2_PIXELS; i++) {
            cloud2State.currentPixelColors[i] = cloudDayColor;
          }
          for (int i = 0; i < CLOUD_3_PIXELS; i++) {
            cloud3State.currentPixelColors[i] = cloudDayColor;
          }

          // Set all cloud LED strands to final color
          setStrandColor(cloud1, cloudDayColor.r, cloudDayColor.g, cloudDayColor.b, cloudDayColor.w);
          setStrandColor(cloud2, cloudDayColor.r, cloudDayColor.g, cloudDayColor.b, cloudDayColor.w);
          setStrandColor(cloud3, cloudDayColor.r, cloudDayColor.g, cloudDayColor.b, cloudDayColor.w);

          // Deactivate all cloud patches since we're now in static DAY mode
          for (int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
            cloud1State.patches[i].active = false;
            cloud2State.patches[i].active = false;
            cloud3State.patches[i].active = false;
          }

          // Stars handled by updateStarVisibility() in main loop

          Serial.print("DAY mode: Clouds set to cloudPalette[38] RGBW: (");
          Serial.print(cloudDayColor.r); Serial.print(", ");
          Serial.print(cloudDayColor.g); Serial.print(", ");
          Serial.print(cloudDayColor.b); Serial.print(", ");
          Serial.print(cloudDayColor.w); Serial.println(")");
        }

        progState.isAnimating = false;
        Serial.println("DAY mode active (static) - displaying palette 39 at full brightness, stars OFF.");
        break;

      default:
        progState.isAnimating = false;
        Serial.println("Sequence complete!");
        break;
    }
  }

  // Special handling for DAY mode - display immediately and stop animating
  if (progState.currentSequence == SEQ_DAY && !progState.dayModeDisplayed) {
    // Test: read palette entry 38 (final sunset color)
    ColorGRBW testColor = getPaletteColor(38);
    Serial.print("Direct palette[38] read - R:");
    Serial.print(testColor.r);
    Serial.print(" G:");
    Serial.print(testColor.g);
    Serial.print(" B:");
    Serial.print(testColor.b);
    Serial.print(" W:");
    Serial.println(testColor.w);

    // Test: Try reading from different indices
    Serial.println("Reading sunset palette indices 36-38:");
    for(int idx = 36; idx <= 38; idx++) {
      ColorGRBW test = getPaletteColor(idx);
      Serial.print("  [");
      Serial.print(idx);
      Serial.print("] R:");
      Serial.print(test.r);
      Serial.print(" G:");
      Serial.print(test.g);
      Serial.print(" B:");
      Serial.print(test.b);
      Serial.print(" W:");
      Serial.println(test.w);
    }

    // Use palette index 38 (the final sunset color) as the daytime horizon color
    ColorGRBW c = getPaletteColor(38);

    Serial.print("Using daytime horizon color - R:");
    Serial.print(c.r);
    Serial.print(" G:");
    Serial.print(c.g);
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

    // Set all cloud pixels to final daytime white (cloudPalette[38])
    ColorGRBW cloudDayColor = getCloudPaletteColor(38);

    // Update cloud state arrays
    for (int i = 0; i < CLOUD_1_PIXELS; i++) {
      cloud1State.currentPixelColors[i] = cloudDayColor;
    }
    for (int i = 0; i < CLOUD_2_PIXELS; i++) {
      cloud2State.currentPixelColors[i] = cloudDayColor;
    }
    for (int i = 0; i < CLOUD_3_PIXELS; i++) {
      cloud3State.currentPixelColors[i] = cloudDayColor;
    }

    // Set all cloud LED strands to final color (RGBW order)
    for (int i = 0; i < CLOUD_1_PIXELS; i++) {
      cloud1.setPixelColor(i, cloud1.Color(cloudDayColor.r, cloudDayColor.g, cloudDayColor.b, cloudDayColor.w));
    }
    for (int i = 0; i < CLOUD_2_PIXELS; i++) {
      cloud2.setPixelColor(i, cloud2.Color(cloudDayColor.r, cloudDayColor.g, cloudDayColor.b, cloudDayColor.w));
    }
    for (int i = 0; i < CLOUD_3_PIXELS; i++) {
      cloud3.setPixelColor(i, cloud3.Color(cloudDayColor.r, cloudDayColor.g, cloudDayColor.b, cloudDayColor.w));
    }

    cloud1.show();
    cloud2.show();
    cloud3.show();

    // Deactivate all cloud patches since we're now in static DAY mode
    for (int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
      cloud1State.patches[i].active = false;
      cloud2State.patches[i].active = false;
      cloud3State.patches[i].active = false;
    }

    // Stars handled by updateStarVisibility() in main loop

    Serial.print("Clouds set to cloudPalette[38] RGBW: (");
    Serial.print(cloudDayColor.r); Serial.print(", ");
    Serial.print(cloudDayColor.g); Serial.print(", ");
    Serial.print(cloudDayColor.b); Serial.print(", ");
    Serial.print(cloudDayColor.w); Serial.println(")");

    progState.dayModeDisplayed = true;
    progState.isAnimating = false;
    Serial.println("DAY mode activated immediately - displaying palette 39 at full brightness, stars OFF.");
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
  } else if (progState.currentSequence == SEQ_DAY_HOLD) {
    // Hold at full daylight (palette position 38.0)
    palPos = 38.0;
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

  // Track horizon color changes and trigger cloud patches
  uint8_t currentHorizonColorIndex = (uint8_t)palPos;

  // Only trigger patches during progression sequences (not hold or day modes)
  if ((progState.currentSequence == SEQ_SUNRISE_PROG ||
       progState.currentSequence == SEQ_TEST_SUNRISE_PROG ||
       progState.currentSequence == SEQ_SUNSET_PROG) &&
      currentHorizonColorIndex != lastHorizonColorIndex) {

    // Trigger 3 new cloud patches (1 on each cloud)
    triggerCloudPatchesForHorizonColor(currentHorizonColorIndex);

    // Update tracking variable
    lastHorizonColorIndex = currentHorizonColorIndex;
  }

  // Gradual cloud base color transition during sunrise/sunset progression
  if (progState.currentSequence == SEQ_SUNRISE_PROG || progState.currentSequence == SEQ_TEST_SUNRISE_PROG ||
      progState.currentSequence == SEQ_SUNSET_PROG) {

    // Get target cloud base color based on current palette position
    ColorGRBW cloudBaseColor = getCloudPaletteColor((uint8_t)palPos);
    float blendFactor = 0.05;  // 5% blend per frame for smooth transition

    // Update CLOUD_1 base colors (skip pixels with active patches)
    for (int i = 0; i < CLOUD_1_PIXELS; i++) {
      bool hasActivePatch = false;

      // Check if this pixel is part of an active patch
      for (int p = 0; p < MAX_PATCHES_PER_CLOUD; p++) {
        if (cloud1State.patches[p].active) {
          CloudPatch &patch = cloud1State.patches[p];
          int halfSize = patch.patchSize / 2;
          int startPixel = patch.centerPixel - halfSize;
          int endPixel = startPixel + patch.patchSize;
          if (i >= startPixel && i < endPixel) {
            hasActivePatch = true;
            break;
          }
        }
      }

      // Only blend base color for pixels without active patches
      if (!hasActivePatch) {
        ColorGRBW &currentColor = cloud1State.currentPixelColors[i];
        currentColor.r = currentColor.r + (cloudBaseColor.r - currentColor.r) * blendFactor;
        currentColor.g = currentColor.g + (cloudBaseColor.g - currentColor.g) * blendFactor;
        currentColor.b = currentColor.b + (cloudBaseColor.b - currentColor.b) * blendFactor;
        currentColor.w = currentColor.w + (cloudBaseColor.w - currentColor.w) * blendFactor;
      }
    }

    // Update CLOUD_2 base colors
    for (int i = 0; i < CLOUD_2_PIXELS; i++) {
      bool hasActivePatch = false;

      for (int p = 0; p < MAX_PATCHES_PER_CLOUD; p++) {
        if (cloud2State.patches[p].active) {
          CloudPatch &patch = cloud2State.patches[p];
          int halfSize = patch.patchSize / 2;
          int startPixel = patch.centerPixel - halfSize;
          int endPixel = startPixel + patch.patchSize;
          if (i >= startPixel && i < endPixel) {
            hasActivePatch = true;
            break;
          }
        }
      }

      if (!hasActivePatch) {
        ColorGRBW &currentColor = cloud2State.currentPixelColors[i];
        currentColor.r = currentColor.r + (cloudBaseColor.r - currentColor.r) * blendFactor;
        currentColor.g = currentColor.g + (cloudBaseColor.g - currentColor.g) * blendFactor;
        currentColor.b = currentColor.b + (cloudBaseColor.b - currentColor.b) * blendFactor;
        currentColor.w = currentColor.w + (cloudBaseColor.w - currentColor.w) * blendFactor;
      }
    }

    // Update CLOUD_3 base colors
    for (int i = 0; i < CLOUD_3_PIXELS; i++) {
      bool hasActivePatch = false;

      for (int p = 0; p < MAX_PATCHES_PER_CLOUD; p++) {
        if (cloud3State.patches[p].active) {
          CloudPatch &patch = cloud3State.patches[p];
          int halfSize = patch.patchSize / 2;
          int startPixel = patch.centerPixel - halfSize;
          int endPixel = startPixel + patch.patchSize;
          if (i >= startPixel && i < endPixel) {
            hasActivePatch = true;
            break;
          }
        }
      }

      if (!hasActivePatch) {
        ColorGRBW &currentColor = cloud3State.currentPixelColors[i];
        currentColor.r = currentColor.r + (cloudBaseColor.r - currentColor.r) * blendFactor;
        currentColor.g = currentColor.g + (cloudBaseColor.g - currentColor.g) * blendFactor;
        currentColor.b = currentColor.b + (cloudBaseColor.b - currentColor.b) * blendFactor;
        currentColor.w = currentColor.w + (cloudBaseColor.w - currentColor.w) * blendFactor;
      }
    }
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

// ========== STORM SYSTEM FUNCTIONS ==========

// Calculate storm brightness based on current phase and progress
// Returns 0.25 during DIM/ACTIVE phases, ramps during transitions
float getStormBrightness() {
  if (progState.currentSequence != SEQ_STORM_DIM &&
      progState.currentSequence != SEQ_STORM_ACTIVE &&
      progState.currentSequence != SEQ_STORM_CLEAR) {
    return 1.0;  // Not in storm, full brightness
  }

  unsigned long elapsed = millis() - stormState.stormPhaseStartTime;
  unsigned long phaseDuration;

  // Determine which phase we're in
  if (progState.currentSequence == SEQ_STORM_DIM) {
    // Dimming phase: 1.0 → 0.15
    phaseDuration = (stormState.stormDuration == TEST_STORM_ACTIVE_DURATION_MS) ?
                    TEST_STORM_DIM_DURATION_MS : STORM_DIM_DURATION_MS;
    float progress = min(1.0f, (float)elapsed / (float)phaseDuration);
    return 1.0 - (progress * 0.85);  // 1.0 → 0.15
  }
  else if (progState.currentSequence == SEQ_STORM_ACTIVE) {
    // Active phase: hold at 0.15
    return 0.15;
  }
  else if (progState.currentSequence == SEQ_STORM_CLEAR) {
    // Clearing phase: 0.15 → 1.0
    phaseDuration = (stormState.stormDuration == TEST_STORM_ACTIVE_DURATION_MS) ?
                    TEST_STORM_CLEAR_DURATION_MS : STORM_CLEAR_DURATION_MS;
    float progress = min(1.0f, (float)elapsed / (float)phaseDuration);
    return 0.15 + (progress * 0.85);  // 0.15 → 1.0
  }

  return 1.0;  // Default full brightness
}

// Apply storm dimming to all clouds AND horizon
void applyStormDimming() {
  float brightness = getStormBrightness();

  // Dim all three clouds
  for (int i = 0; i < cloud1State.numPixels; i++) {
    ColorGRBW c = cloud1State.currentPixelColors[i];
    cloud1.setPixelColor(i, cloud1.Color(
      c.r * brightness,
      c.g * brightness,
      c.b * brightness,
      c.w * brightness
    ));
  }

  for (int i = 0; i < cloud2State.numPixels; i++) {
    ColorGRBW c = cloud2State.currentPixelColors[i];
    cloud2.setPixelColor(i, cloud2.Color(
      c.r * brightness,
      c.g * brightness,
      c.b * brightness,
      c.w * brightness
    ));
  }

  for (int i = 0; i < cloud3State.numPixels; i++) {
    ColorGRBW c = cloud3State.currentPixelColors[i];
    cloud3.setPixelColor(i, cloud3.Color(
      c.r * brightness,
      c.g * brightness,
      c.b * brightness,
      c.w * brightness
    ));
  }

  // Dim horizon too
  for (int i = 0; i < HORIZON_PIXELS; i++) {
    uint32_t color = horizon.getPixelColor(i);
    uint8_t r = ((color >> 24) & 0xFF) * brightness;
    uint8_t g = ((color >> 16) & 0xFF) * brightness;
    uint8_t b = ((color >> 8) & 0xFF) * brightness;
    uint8_t w = (color & 0xFF) * brightness;
    horizon.setPixelColor(i, horizon.Color(r, g, b, w));
  }

  // Show all strands
  cloud1.show();
  cloud2.show();
  cloud3.show();
  horizon.show();
}

// Start a storm sequence
void startStorm(bool testMode) {
  // Guard against double-starting
  if (progState.currentSequence == SEQ_STORM_DIM ||
      progState.currentSequence == SEQ_STORM_ACTIVE ||
      progState.currentSequence == SEQ_STORM_CLEAR) {
    Serial.println("ERROR: Storm already active - ignoring request");
    return;
  }

  Serial.println("========================================");
  Serial.println(testMode ? "STARTING TEST STORM" : "STARTING STORM");

  // Save current brightness
  stormState.preStormBrightness = 1.0;

  // Set storm duration
  if (testMode) {
    stormState.stormDuration = TEST_STORM_ACTIVE_DURATION_MS;
    Serial.println("Test mode: 5s dim + 30s active + 5s clear = 40s total");
  } else {
    stormState.stormDuration = random(STORM_MIN_DURATION_MS, STORM_MAX_DURATION_MS + 1);
    Serial.print("Production mode: ");
    Serial.print(stormState.stormDuration / 60000);
    Serial.println(" minute active phase");
  }

  // Initialize storm timing
  stormState.stormPhaseStartTime = millis();
  stormState.nextLightningTime = 0;
  stormState.lightningFlashStartTime = 0;

  // Transition to dimming phase
  progState.currentSequence = SEQ_STORM_DIM;
  progState.isAnimating = false;  // We'll handle storm updates separately

  // Schedule first lightning strike (will be triggered during ACTIVE phase)
  scheduleNextLightning();

  Serial.println("Storm started - entering DIM phase");
  Serial.println("========================================");
}

// End storm and return to DAY mode
void endStorm() {
  Serial.println("========================================");
  Serial.println("ENDING STORM - Returning to DAY mode");
  Serial.println("========================================");

  // Restore brightness
  stormState.preStormBrightness = 1.0;

  // Clear storm state
  stormState.nextLightningTime = 0;
  stormState.lightningFlashStartTime = 0;

  // Return to DAY mode
  transitionToSequence(SEQ_DAY);
}

// Update storm state machine - handles all three storm phases
void updateStorm() {
  unsigned long elapsed = millis() - stormState.stormPhaseStartTime;
  unsigned long phaseDuration;

  // Handle each storm phase
  if (progState.currentSequence == SEQ_STORM_DIM) {
    // Dimming phase: 100% → 25%
    phaseDuration = (stormState.stormDuration == TEST_STORM_ACTIVE_DURATION_MS) ?
                    TEST_STORM_DIM_DURATION_MS : STORM_DIM_DURATION_MS;

    // Apply dimming
    applyStormDimming();

    // Check if dim phase complete
    if (elapsed >= phaseDuration) {
      Serial.println("========================================");
      Serial.println("DIM phase complete - entering ACTIVE phase");
      Serial.print("Active phase duration: ");
      Serial.print(stormState.stormDuration / 1000);
      Serial.println(" seconds");
      Serial.println("========================================");

      // Transition to active phase
      progState.currentSequence = SEQ_STORM_ACTIVE;
      stormState.stormPhaseStartTime = millis();
    }
  }
  else if (progState.currentSequence == SEQ_STORM_ACTIVE) {
    // Active phase: maintain 25% brightness with lightning
    unsigned long currentTime = millis();

    // Check if we have an active flash that needs to be cleared
    if (stormState.lightningFlashStartTime > 0) {
      unsigned long flashElapsed = currentTime - stormState.lightningFlashStartTime;

      if (flashElapsed >= LIGHTNING_FLASH_DURATION_MS) {
        // Flash duration complete - clear it
        clearLightning();

        // Increment flash counter
        stormState.currentFlashNumber++;

        // Check if this was a multi-flash and more flashes remain
        if (stormState.currentFlashNumber < stormState.lightningFlashCount) {
          // Schedule next flash in the sequence (short delay: 50-200ms)
          stormState.nextLightningTime = currentTime + random(50, 201);
          Serial.print("Multi-flash continues (");
          Serial.print(stormState.currentFlashNumber + 1);
          Serial.print("/");
          Serial.print(stormState.lightningFlashCount);
          Serial.println(")");
        } else {
          // All flashes complete - schedule next strike
          scheduleNextLightning();
        }
      }
    }
    // Check if it's time for next lightning strike
    else if (currentTime >= stormState.nextLightningTime) {
      // Trigger the lightning flash
      triggerLightning();
    }
    else {
      // No flash active - just maintain storm dimming
      applyStormDimming();
    }

    // Check if active phase complete
    if (elapsed >= stormState.stormDuration) {
      Serial.println("========================================");
      Serial.println("ACTIVE phase complete - entering CLEAR phase");
      Serial.println("========================================");

      // Transition to clearing phase
      progState.currentSequence = SEQ_STORM_CLEAR;
      stormState.stormPhaseStartTime = millis();
    }
  }
  else if (progState.currentSequence == SEQ_STORM_CLEAR) {
    // Clearing phase: 25% → 100%
    phaseDuration = (stormState.stormDuration == TEST_STORM_ACTIVE_DURATION_MS) ?
                    TEST_STORM_CLEAR_DURATION_MS : STORM_CLEAR_DURATION_MS;

    // Apply dimming (brightness will ramp up)
    applyStormDimming();

    // Check if clear phase complete
    if (elapsed >= phaseDuration) {
      Serial.println("CLEAR phase complete");
      endStorm();
    }
  }
}

// Trigger a lightning flash based on scheduled parameters
void triggerLightning() {
  ColorGRBW lightningColor = getStormPaletteColor(stormState.strikeIntensity);

  if (stormState.strikeType == 0) {
    // Single cloud strike - flash selected pixels
    Adafruit_NeoPixel* targetCloud;
    CloudState* targetState;

    switch(stormState.strikeCloud) {
      case 0:
        targetCloud = &cloud1;
        targetState = &cloud1State;
        break;
      case 1:
        targetCloud = &cloud2;
        targetState = &cloud2State;
        break;
      case 2:
        targetCloud = &cloud3;
        targetState = &cloud3State;
        break;
      default:
        targetCloud = &cloud1;
        targetState = &cloud1State;
        break;
    }

    // Set selected pixels to lightning color
    for (int i = 0; i < stormState.strikeNumPixels; i++) {
      uint8_t pixel = stormState.strikePixels[i];
      targetCloud->setPixelColor(pixel, targetCloud->Color(
        lightningColor.r, lightningColor.g, lightningColor.b, lightningColor.w
      ));
    }

    targetCloud->show();
  } else {
    // Multi-cloud arc - flash one pixel on start cloud and one on end cloud
    Adafruit_NeoPixel* cloudA;
    Adafruit_NeoPixel* cloudB;

    switch(stormState.strikeCloudStart) {
      case 0: cloudA = &cloud1; break;
      case 1: cloudA = &cloud2; break;
      case 2: cloudA = &cloud3; break;
      default: cloudA = &cloud1; break;
    }

    switch(stormState.strikeCloudEnd) {
      case 0: cloudB = &cloud1; break;
      case 1: cloudB = &cloud2; break;
      case 2: cloudB = &cloud3; break;
      default: cloudB = &cloud1; break;
    }

    // Flash random pixel on each cloud
    uint8_t pixelA = random(cloudA->numPixels());
    uint8_t pixelB = random(cloudB->numPixels());

    cloudA->setPixelColor(pixelA, cloudA->Color(
      lightningColor.r, lightningColor.g, lightningColor.b, lightningColor.w
    ));
    cloudB->setPixelColor(pixelB, cloudB->Color(
      lightningColor.r, lightningColor.g, lightningColor.b, lightningColor.w
    ));

    cloudA->show();
    cloudB->show();
  }

  // Record flash start time
  stormState.lightningFlashStartTime = millis();

  Serial.print("FLASH! Type: ");
  Serial.print(stormState.strikeType == 0 ? "single" : "arc");
  Serial.print(", Intensity: ");
  Serial.println(stormState.strikeIntensity);
}

// Clear lightning flash and return to storm-dimmed state
void clearLightning() {
  // Simply re-apply storm dimming which will restore the 25% brightness
  applyStormDimming();

  // Reset flash tracking
  stormState.lightningFlashStartTime = 0;

  Serial.println("Flash cleared");
}

// Check if storm should trigger randomly (called from loop when in DAY mode)
void checkStormTrigger() {
  if (!stormState.stormEnabled) {
    return;  // Auto-triggering disabled
  }

  unsigned long currentTime = millis();
  unsigned long timeSinceLastCheck = currentTime - stormState.lastStormCheckTime;

  // Only check once per interval (default 1 hour)
  if (timeSinceLastCheck >= STORM_MIN_CHECK_INTERVAL_MS) {
    // Roll for storm trigger (30% probability)
    if (random(100) < STORM_TRIGGER_PROBABILITY) {
      Serial.println("========================================");
      Serial.println("RANDOM STORM TRIGGERED!");
      Serial.println("========================================");
      startStorm(false);  // false = production mode
    } else {
      Serial.println("Storm check: No storm triggered");
    }

    // Update last check time
    stormState.lastStormCheckTime = currentTime;
  }
}

// Schedule the next lightning strike with random parameters
void scheduleNextLightning() {
  // Choose strike type: 60% single cloud, 40% multi-cloud arc
  int strikeTypeRoll = random(100);
  if (strikeTypeRoll < 60) {
    // Single cloud strike
    stormState.strikeType = 0;

    // Pick random cloud (0=cloud1, 1=cloud2, 2=cloud3)
    stormState.strikeCloud = random(3);

    // Pick 1-3 random pixels on that cloud
    stormState.strikeNumPixels = random(1, 4);

    uint8_t maxPixels;
    switch(stormState.strikeCloud) {
      case 0: maxPixels = CLOUD_1_PIXELS; break;
      case 1: maxPixels = CLOUD_2_PIXELS; break;
      case 2: maxPixels = CLOUD_3_PIXELS; break;
      default: maxPixels = CLOUD_1_PIXELS; break;
    }

    // Pick random pixels (ensure they're different)
    for (int i = 0; i < stormState.strikeNumPixels; i++) {
      stormState.strikePixels[i] = random(maxPixels);
    }

    Serial.print("Scheduled SINGLE-CLOUD strike: cloud ");
    Serial.print(stormState.strikeCloud);
    Serial.print(", ");
    Serial.print(stormState.strikeNumPixels);
    Serial.println(" pixels");
  } else {
    // Multi-cloud arc strike
    stormState.strikeType = 1;

    // Pick 2 different clouds for arc
    stormState.strikeCloudStart = random(3);
    stormState.strikeCloudEnd = random(3);

    // Ensure they're different
    while (stormState.strikeCloudEnd == stormState.strikeCloudStart) {
      stormState.strikeCloudEnd = random(3);
    }

    // Pick 1 pixel on each cloud (total 2-3 pixels depending on if we span 2 or 3 clouds)
    stormState.strikeNumPixels = 2;  // Will be 2-3 clouds

    Serial.print("Scheduled MULTI-CLOUD arc: clouds ");
    Serial.print(stormState.strikeCloudStart);
    Serial.print(" -> ");
    Serial.println(stormState.strikeCloudEnd);
  }

  // Choose intensity: 40% bright (3-4), 40% medium (5-6), 20% dim (7)
  int intensityRoll = random(100);
  if (intensityRoll < 40) {
    // Bright lightning (close)
    stormState.strikeIntensity = random(3, 5);  // 3 or 4
  } else if (intensityRoll < 80) {
    // Medium lightning
    stormState.strikeIntensity = random(5, 7);  // 5 or 6
  } else {
    // Dim lightning (distant)
    stormState.strikeIntensity = 7;
  }

  Serial.print("Intensity: index ");
  Serial.println(stormState.strikeIntensity);

  // Decide if this is a multi-flash strike (40% chance of 2-3 flashes)
  if (random(100) < LIGHTNING_MULTI_FLASH_CHANCE) {
    stormState.lightningFlashCount = random(2, 4);  // 2 or 3 flashes
    Serial.print("Multi-flash: ");
    Serial.print(stormState.lightningFlashCount);
    Serial.println(" flashes");
  } else {
    stormState.lightningFlashCount = 1;
  }

  stormState.currentFlashNumber = 0;

  // Schedule timing: random interval between strikes (800-8000ms)
  unsigned long interval = random(LIGHTNING_MIN_INTERVAL_MS, LIGHTNING_MAX_INTERVAL_MS + 1);
  stormState.nextLightningTime = millis() + interval;

  Serial.print("Next strike in ");
  Serial.print(interval);
  Serial.println("ms");
}

// ========== STAR FADE SYSTEM ==========

// Set target brightness for stars (triggers fade if different from current)
void setStarBrightness(float targetBrightness) {
  // Clamp to valid range
  if (targetBrightness < 0.0) targetBrightness = 0.0;
  if (targetBrightness > 1.0) targetBrightness = 1.0;

  // Only start a new fade if target is different
  if (abs(targetBrightness - starState.targetBrightness) > 0.01) {
    starState.targetBrightness = targetBrightness;
    starState.fadeStartTime = millis();
    starState.isFading = true;
  }
}

// Update star fade and apply PWM
void updateStarFade() {
  if (!starState.isFading) {
    return;  // Not fading, nothing to do
  }

  unsigned long elapsed = millis() - starState.fadeStartTime;
  float fadeProgress = min(1.0f, (float)elapsed / (float)STAR_FADE_DURATION_MS);

  // Calculate current brightness based on fade progress
  float startBrightness = starState.currentBrightness;
  starState.currentBrightness = startBrightness + (starState.targetBrightness - startBrightness) * fadeProgress;

  // Apply PWM to stars pin (0-255)
  analogWrite(STARS_PIN, (uint8_t)(starState.currentBrightness * 255));

  // Check if fade complete
  if (fadeProgress >= 1.0f) {
    starState.isFading = false;
    starState.currentBrightness = starState.targetBrightness;
  }
}

// Determine if stars should be visible based on current sequence and progress
void updateStarVisibility() {
  float targetBrightness = 0.0;  // Default: off

  // Check storm state first - stars always off during storms
  if (progState.currentSequence == SEQ_STORM_DIM ||
      progState.currentSequence == SEQ_STORM_ACTIVE ||
      progState.currentSequence == SEQ_STORM_CLEAR) {
    targetBrightness = 0.0;
  }
  // Night mode - stars always on
  else if (progState.currentSequence == SEQ_NIGHT) {
    targetBrightness = 1.0;
  }
  // Day mode - stars always off
  else if (progState.currentSequence == SEQ_DAY) {
    targetBrightness = 0.0;
  }
  // Sunrise hold - stars on
  else if (progState.currentSequence == SEQ_SUNRISE_HOLD ||
           progState.currentSequence == SEQ_TEST_SUNRISE_HOLD) {
    targetBrightness = 1.0;
  }
  // Sunrise progression - stars on for first 50%, then fade out
  else if (progState.currentSequence == SEQ_SUNRISE_PROG ||
           progState.currentSequence == SEQ_TEST_SUNRISE_PROG) {
    if (progState.progressPercent <= 50.0) {
      targetBrightness = 1.0;  // First half: stars on
    } else {
      targetBrightness = 0.0;  // Second half: stars off
    }
  }
  // Sunset progression - stars off for first 50%, then fade in
  else if (progState.currentSequence == SEQ_SUNSET_PROG) {
    if (progState.progressPercent <= 50.0) {
      targetBrightness = 0.0;  // First half: stars off
    } else {
      targetBrightness = 1.0;  // Second half: stars on
    }
  }
  // OFF mode - stars off
  else if (progState.currentSequence == SEQ_OFF) {
    targetBrightness = 0.0;
  }

  // Set target brightness (will trigger fade if needed)
  setStarBrightness(targetBrightness);
}

// ========== END STAR FADE SYSTEM ==========

// ========== BLE COMMAND HANDLERS ==========

// Handle mode control commands from BLE
void handleModeControl(uint8_t* data, size_t length) {
  if (length < 1) {
    Serial.println("BLE: Error - empty mode control command");
    return;
  }

  uint8_t mode = data[0];
  Serial.print("BLE: Mode control received: 0x");
  Serial.println(mode, HEX);

  switch(mode) {
    case 0x00: // OFF mode
      {
        Serial.println("BLE: Setting OFF mode");
        bleControl.currentAppMode = 0x00;

        // Stop any active sequences first
        progState.currentSequence = SEQ_OFF;
        progState.isAnimating = false;

        // Clear all cloud state arrays (set to black)
        ColorGRBW blackColor = {0, 0, 0, 0};
        for(int i = 0; i < CLOUD_1_PIXELS; i++) {
          cloud1State.currentPixelColors[i] = blackColor;
        }
        for(int i = 0; i < CLOUD_2_PIXELS; i++) {
          cloud2State.currentPixelColors[i] = blackColor;
        }
        for(int i = 0; i < CLOUD_3_PIXELS; i++) {
          cloud3State.currentPixelColors[i] = blackColor;
        }

        // Deactivate all cloud patches
        for(int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
          cloud1State.patches[i].active = false;
          cloud2State.patches[i].active = false;
          cloud3State.patches[i].active = false;
        }

        // Turn off all LED strands
        for(int i = 0; i < HORIZON_PIXELS; i++) {
          horizon.setPixelColor(i, 0, 0, 0, 0);
        }
        horizon.show();

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

        // Stars handled by updateStarVisibility() in main loop

        Serial.println("BLE: All LEDs OFF");
      }
      break;

    case 0x01: // CYCLE mode (with pause/resume)
      {
        if (length < 2) {
          Serial.println("BLE: Error - CYCLE command requires pause state byte");
          return;
        }

        uint8_t pauseState = data[1];  // 0x00=resume, 0x01=pause
        Serial.print("BLE: CYCLE mode - pause state: 0x");
        Serial.println(pauseState, HEX);

        if (pauseState == 0x01) {
          // PAUSE requested
          if (!bleControl.isPaused && progState.isAnimating) {
            // Currently running - pause it
            bleControl.isPaused = true;
            bleControl.pauseStartTime = millis();

            // Calculate remaining time in current phase
            unsigned long elapsed = millis() - progState.phaseStartTime;
            unsigned long phaseDuration;

            // Determine phase duration based on current state
            switch(progState.currentSequence) {
              case SEQ_SUNRISE_HOLD:
                phaseDuration = SUNRISE_HOLD_DURATION_MS;
                break;
              case SEQ_SUNRISE_PROG:
                phaseDuration = bleControl.calculatedSunriseDuration;
                break;
              case SEQ_DAY_HOLD:
                phaseDuration = bleControl.calculatedDaylightDuration;
                break;
              case SEQ_SUNSET_PROG:
                phaseDuration = bleControl.calculatedSunsetDuration;
                break;
              default:
                phaseDuration = bleControl.calculatedSunriseDuration;
                break;
            }

            // Store remaining time
            if (elapsed < phaseDuration) {
              bleControl.pausedTimeRemaining = phaseDuration - elapsed;
            } else {
              bleControl.pausedTimeRemaining = 0;
            }

            // Stop animation
            progState.isAnimating = false;

            Serial.println("BLE: Cycle PAUSED");
            Serial.print("  Remaining time: ");
            Serial.print(bleControl.pausedTimeRemaining / 1000);
            Serial.println(" seconds");
          } else {
            Serial.println("BLE: Already paused or not animating");
          }
        }
        else if (pauseState == 0x00) {
          // RESUME requested
          if (bleControl.isPaused) {
            // Currently paused - resume it
            bleControl.isPaused = false;

            // Restore phase start time accounting for paused duration
            unsigned long phaseDuration;
            switch(progState.currentSequence) {
              case SEQ_SUNRISE_HOLD:
                phaseDuration = SUNRISE_HOLD_DURATION_MS;
                break;
              case SEQ_SUNRISE_PROG:
                phaseDuration = bleControl.calculatedSunriseDuration;
                break;
              case SEQ_DAY_HOLD:
                phaseDuration = bleControl.calculatedDaylightDuration;
                break;
              case SEQ_SUNSET_PROG:
                phaseDuration = bleControl.calculatedSunsetDuration;
                break;
              default:
                phaseDuration = bleControl.calculatedSunriseDuration;
                break;
            }
            progState.phaseStartTime = millis() - (progState.progressPercent / 100.0 * phaseDuration);

            // Resume animation
            progState.isAnimating = true;

            Serial.println("BLE: Cycle RESUMED");
          } else {
            // Not currently paused - start a new cycle
            Serial.println("BLE: Starting new cycle (not paused)");
            bleControl.currentAppMode = 0x01;
            bleControl.isPaused = false;

            // Reset clouds to night colors before starting cycle
            ColorGRBW nightCloudColor = getCloudPaletteColor(0);  // Night cloud color
            float nightBrightness = BRIGHTNESS_MIN_FACTOR;         // 0.125 (12.5%)

            // Update cloud state arrays to night colors
            for(int i = 0; i < CLOUD_1_PIXELS; i++) {
              cloud1State.currentPixelColors[i] = nightCloudColor;
            }
            for(int i = 0; i < CLOUD_2_PIXELS; i++) {
              cloud2State.currentPixelColors[i] = nightCloudColor;
            }
            for(int i = 0; i < CLOUD_3_PIXELS; i++) {
              cloud3State.currentPixelColors[i] = nightCloudColor;
            }

            // Deactivate any active cloud patches
            for(int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
              cloud1State.patches[i].active = false;
              cloud2State.patches[i].active = false;
              cloud3State.patches[i].active = false;
            }

            // Actually display the night colors on LEDs
            for(int i = 0; i < CLOUD_1_PIXELS; i++) {
              cloud1.setPixelColor(i,
                nightCloudColor.g * nightBrightness,
                nightCloudColor.r * nightBrightness,
                nightCloudColor.b * nightBrightness,
                nightCloudColor.w * nightBrightness);
            }
            cloud1.show();

            for(int i = 0; i < CLOUD_2_PIXELS; i++) {
              cloud2.setPixelColor(i,
                nightCloudColor.g * nightBrightness,
                nightCloudColor.r * nightBrightness,
                nightCloudColor.b * nightBrightness,
                nightCloudColor.w * nightBrightness);
            }
            cloud2.show();

            for(int i = 0; i < CLOUD_3_PIXELS; i++) {
              cloud3.setPixelColor(i,
                nightCloudColor.g * nightBrightness,
                nightCloudColor.r * nightBrightness,
                nightCloudColor.b * nightBrightness,
                nightCloudColor.w * nightBrightness);
            }
            cloud3.show();

            transitionToSequence(SEQ_SUNRISE_HOLD);
          }
        }
        else {
          Serial.print("BLE: Unknown pause state: 0x");
          Serial.println(pauseState, HEX);
        }
      }
      break;

    case 0x02: // NIGHT mode
      {
        Serial.println("BLE: Setting NIGHT mode");
        bleControl.currentAppMode = 0x02;

        // Stop any active sequences
        progState.currentSequence = SEQ_NIGHT;
        progState.isAnimating = false;

        // Set horizon to night blue (palette color 0) at low brightness (12.5%)
        ColorGRBW nightHorizonColor = getPaletteColor(0);  // Deep blue
        float nightBrightness = BRIGHTNESS_MIN_FACTOR;     // 0.125 (12.5%)

        for(int i = 0; i < HORIZON_PIXELS; i++) {
          horizon.setPixelColor(i, horizon.Color(
            nightHorizonColor.r * nightBrightness,
            nightHorizonColor.g * nightBrightness,
            nightHorizonColor.b * nightBrightness,
            nightHorizonColor.w * nightBrightness
          ));
        }
        horizon.show();

        // Set clouds to night color (cloudPalette color 0) at low brightness
        ColorGRBW nightCloudColor = getCloudPaletteColor(0);  // Night cloud color

        // Update cloud state arrays
        for(int i = 0; i < CLOUD_1_PIXELS; i++) {
          cloud1State.currentPixelColors[i] = nightCloudColor;
          cloud1.setPixelColor(i, cloud1.Color(
            nightCloudColor.r * nightBrightness,
            nightCloudColor.g * nightBrightness,
            nightCloudColor.b * nightBrightness,
            nightCloudColor.w * nightBrightness
          ));
        }
        cloud1.show();

        for(int i = 0; i < CLOUD_2_PIXELS; i++) {
          cloud2State.currentPixelColors[i] = nightCloudColor;
          cloud2.setPixelColor(i, cloud2.Color(
            nightCloudColor.r * nightBrightness,
            nightCloudColor.g * nightBrightness,
            nightCloudColor.b * nightBrightness,
            nightCloudColor.w * nightBrightness
          ));
        }
        cloud2.show();

        for(int i = 0; i < CLOUD_3_PIXELS; i++) {
          cloud3State.currentPixelColors[i] = nightCloudColor;
          cloud3.setPixelColor(i, cloud3.Color(
            nightCloudColor.r * nightBrightness,
            nightCloudColor.g * nightBrightness,
            nightCloudColor.b * nightBrightness,
            nightCloudColor.w * nightBrightness
          ));
        }
        cloud3.show();

        // Deactivate all cloud patches (static night mode)
        for(int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
          cloud1State.patches[i].active = false;
          cloud2State.patches[i].active = false;
          cloud3State.patches[i].active = false;
        }

        // Stars handled by updateStarVisibility() in main loop

        Serial.println("BLE: Night mode active (low brightness, stars ON)");
      }
      break;

    case 0x03: // DAY mode
      Serial.println("BLE: Jumping to DAY mode");
      bleControl.currentAppMode = 0x03;
      transitionToSequence(SEQ_DAY);
      Serial.println("BLE: Day mode active");
      break;

    case 0x04: // SUNSET mode
      {
        Serial.println("BLE: Starting SUNSET sequence");
        bleControl.currentAppMode = 0x04;

        // Initialize to DAY colors (palette index 38) before starting progression
        ColorGRBW dayHorizonColor = getPaletteColor(38);  // Bright cyan
        float dayBrightness = BRIGHTNESS_MAX_FACTOR;      // 1.0 (100%)

        for(int i = 0; i < HORIZON_PIXELS; i++) {
          horizon.setPixelColor(i, horizon.Color(
            dayHorizonColor.r * dayBrightness,
            dayHorizonColor.g * dayBrightness,
            dayHorizonColor.b * dayBrightness,
            dayHorizonColor.w * dayBrightness
          ));
        }
        horizon.show();

        // Set clouds to day color (cloudPalette index 38) at full brightness
        ColorGRBW dayCloudColor = getCloudPaletteColor(38);  // Day cloud color

        // Update cloud state arrays
        for(int i = 0; i < CLOUD_1_PIXELS; i++) {
          cloud1State.currentPixelColors[i] = dayCloudColor;
          cloud1.setPixelColor(i, cloud1.Color(
            dayCloudColor.r * dayBrightness,
            dayCloudColor.g * dayBrightness,
            dayCloudColor.b * dayBrightness,
            dayCloudColor.w * dayBrightness
          ));
        }
        cloud1.show();

        for(int i = 0; i < CLOUD_2_PIXELS; i++) {
          cloud2State.currentPixelColors[i] = dayCloudColor;
          cloud2.setPixelColor(i, cloud2.Color(
            dayCloudColor.r * dayBrightness,
            dayCloudColor.g * dayBrightness,
            dayCloudColor.b * dayBrightness,
            dayCloudColor.w * dayBrightness
          ));
        }
        cloud2.show();

        for(int i = 0; i < CLOUD_3_PIXELS; i++) {
          cloud3State.currentPixelColors[i] = dayCloudColor;
          cloud3.setPixelColor(i, cloud3.Color(
            dayCloudColor.r * dayBrightness,
            dayCloudColor.g * dayBrightness,
            dayCloudColor.b * dayBrightness,
            dayCloudColor.w * dayBrightness
          ));
        }
        cloud3.show();

        // Deactivate all cloud patches before starting progression
        for(int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
          cloud1State.patches[i].active = false;
          cloud2State.patches[i].active = false;
          cloud3State.patches[i].active = false;
        }

        // Now start the sunset progression (38 → 0 over 20 minutes)
        transitionToSequence(SEQ_SUNSET_PROG);
        Serial.println("BLE: Sunset sequence started (20min progression from day to night)");
      }
      break;

    default:
      Serial.print("BLE: Unknown mode command: 0x");
      Serial.println(mode, HEX);
      break;
  }
}

// Handle cycle configuration commands from BLE
void handleCycleConfig(uint8_t* data, size_t length) {
  if (length < 6) {
    Serial.println("BLE: Error - Cycle config requires 6 bytes");
    return;
  }

  // Parse little-endian uint16_t values
  uint16_t sunCycleDayDuration = data[0] | (data[1] << 8);
  uint16_t sunriseDuration = data[2] | (data[3] << 8);
  uint16_t sunsetDuration = data[4] | (data[5] << 8);

  Serial.println("========================================");
  Serial.println("BLE: Cycle configuration received");
  Serial.print("  Sun Cycle Day Duration: ");
  Serial.print(sunCycleDayDuration);
  Serial.println(" minutes");
  Serial.print("  Sunrise Duration: ");
  Serial.print(sunriseDuration);
  Serial.println(" minutes");
  Serial.print("  Sunset Duration: ");
  Serial.print(sunsetDuration);
  Serial.println(" minutes");

  // Validate values
  // Sun Cycle Duration: 1 to 14 hours (60 to 840 minutes)
  if (sunCycleDayDuration < 60 || sunCycleDayDuration > 840) {
    Serial.println("BLE: Error - Sun Cycle Day Duration must be 60-840 minutes (1-14 hours)");
    return;
  }

  // Sunrise Duration: 5 to 20 minutes
  if (sunriseDuration < 5 || sunriseDuration > 20) {
    Serial.println("BLE: Error - Sunrise Duration must be 5-20 minutes");
    return;
  }

  // Sunset Duration: 5 to 20 minutes
  if (sunsetDuration < 5 || sunsetDuration > 20) {
    Serial.println("BLE: Error - Sunset Duration must be 5-20 minutes");
    return;
  }

  // Ensure sunrise + sunset durations fit within total cycle duration
  if (sunriseDuration + sunsetDuration >= sunCycleDayDuration) {
    Serial.println("BLE: Error - Sunrise + Sunset duration must be less than total day duration");
    return;
  }

  // Store configuration
  bleControl.sunCycleDayDurationMinutes = sunCycleDayDuration;
  bleControl.sunriseDurationMinutes = sunriseDuration;
  bleControl.sunsetDurationMinutes = sunsetDuration;

  // Calculate daylight duration (total - sunrise - sunset)
  uint16_t daylightMinutes = sunCycleDayDuration - sunriseDuration - sunsetDuration;
  bleControl.calculatedDaylightDuration = (uint32_t)daylightMinutes * 60000UL;  // Convert to ms
  bleControl.calculatedSunriseDuration = (uint32_t)sunriseDuration * 60000UL;
  bleControl.calculatedSunsetDuration = (uint32_t)sunsetDuration * 60000UL;

  Serial.print("  Calculated Daylight Duration: ");
  Serial.print(daylightMinutes);
  Serial.println(" minutes");
  Serial.print("  Calculated Sunrise Duration (ms): ");
  Serial.println(bleControl.calculatedSunriseDuration);
  Serial.print("  Calculated Daylight Duration (ms): ");
  Serial.println(bleControl.calculatedDaylightDuration);
  Serial.print("  Calculated Sunset Duration (ms): ");
  Serial.println(bleControl.calculatedSunsetDuration);
  Serial.println("========================================");
}

// Handle time synchronization commands from BLE
void handleTimeSync(uint8_t* data, size_t length) {
  if (length < 6) {
    Serial.println("BLE: Error - Time sync requires 6 bytes");
    return;
  }

  // Parse time sync data (little-endian):
  // Bytes 0-3: Unix timestamp (uint32_t)
  // Byte 4: Day of week (0=Sunday, 6=Saturday)
  // Byte 5: Reserved/padding

  uint32_t unixTimestamp = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
  uint8_t dayOfWeek = data[4];

  Serial.println("========================================");
  Serial.println("BLE: Time synchronization received");
  Serial.print("  Unix timestamp: ");
  Serial.println(unixTimestamp);
  Serial.print("  Day of week: ");
  Serial.print(dayOfWeek);
  Serial.print(" (");

  // Print day name
  const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  if (dayOfWeek <= 6) {
    Serial.print(dayNames[dayOfWeek]);
  } else {
    Serial.print("INVALID");
  }
  Serial.println(")");

  // Validate day of week
  if (dayOfWeek > 6) {
    Serial.println("BLE: Error - Invalid day of week (must be 0-6)");
    return;
  }

  // Store time sync data
  timeSync.synchronized = true;
  timeSync.syncMillis = millis();
  timeSync.syncEpoch = unixTimestamp;
  timeSync.syncDayOfWeek = dayOfWeek;

  Serial.println("  Time synchronized successfully!");
  Serial.println("========================================");
}

// Handle schedule configuration commands from BLE
void handleScheduleConfig(uint8_t* data, size_t length) {
  if (length < 5) {
    Serial.println("BLE: Error - Schedule config requires 5 bytes");
    return;
  }

  // Parse schedule data:
  // Byte 0: Enable flag (0x00=disabled, 0x01=enabled)
  // Byte 1: Hour (0-23)
  // Byte 2: Minute (0-59)
  // Byte 3: Day mask (bit 0=Sunday, bit 6=Saturday)
  // Byte 4: Night mode after sunset flag (0x00=disabled, 0x01=enabled)

  uint8_t enabled = data[0];
  uint8_t hour = data[1];
  uint8_t minute = data[2];
  uint8_t dayMask = data[3];
  uint8_t nightAfterSunset = data[4];

  Serial.println("========================================");
  Serial.println("BLE: Schedule configuration received");
  Serial.print("  Enabled: ");
  Serial.println(enabled ? "YES" : "NO");
  Serial.print("  Time: ");
  if (hour < 10) Serial.print("0");
  Serial.print(hour);
  Serial.print(":");
  if (minute < 10) Serial.print("0");
  Serial.println(minute);

  Serial.print("  Days: ");
  const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  bool firstDay = true;
  for (int i = 0; i < 7; i++) {
    if (dayMask & (1 << i)) {
      if (!firstDay) Serial.print(", ");
      Serial.print(dayNames[i]);
      firstDay = false;
    }
  }
  if (firstDay) Serial.print("NONE");
  Serial.println();

  Serial.print("  Night after sunset: ");
  Serial.println(nightAfterSunset ? "YES" : "NO");

  // Validate values
  if (hour > 23) {
    Serial.println("BLE: Error - Hour must be 0-23");
    return;
  }
  if (minute > 59) {
    Serial.println("BLE: Error - Minute must be 0-59");
    return;
  }
  if (dayMask == 0) {
    Serial.println("BLE: Error - At least one day must be selected");
    return;
  }

  // Store schedule configuration
  bleControl.scheduleEnabled = (enabled != 0);
  bleControl.scheduleHour = hour;
  bleControl.scheduleMinute = minute;
  bleControl.scheduleDayMask = dayMask;
  bleControl.enableNightAfterSunset = (nightAfterSunset != 0);

  // Reset waiting flag to recheck schedule
  if (bleControl.scheduleEnabled) {
    bleControl.waitingForScheduledStart = true;
    Serial.println("  Schedule activated - waiting for next scheduled time");
  } else {
    bleControl.waitingForScheduledStart = false;
    Serial.println("  Schedule disabled");
  }

  Serial.println("========================================");
}

// Handle storm configuration commands from BLE
void handleStormConfig(uint8_t* data, size_t length) {
  if (length < 2) {
    Serial.println("BLE: Error - Storm config requires 2 bytes");
    return;
  }

  // Parse storm data:
  // Byte 0: Storm enabled during day (0x00=disabled, 0x01=enabled)
  // Byte 1: Storm enabled during night (0x00=disabled, 0x01=enabled)

  uint8_t stormDay = data[0];
  uint8_t stormNight = data[1];

  Serial.println("========================================");
  Serial.println("BLE: Storm configuration received");
  Serial.print("  Storm during day: ");
  Serial.println(stormDay ? "ENABLED" : "DISABLED");
  Serial.print("  Storm during night: ");
  Serial.println(stormNight ? "ENABLED" : "DISABLED");

  // Store storm configuration
  bleControl.stormEnabledDay = (stormDay != 0);
  bleControl.stormEnabledNight = (stormNight != 0);

  // Update global storm state based on current mode
  if (progState.currentSequence == SEQ_DAY) {
    stormState.stormEnabled = bleControl.stormEnabledDay;
  } else if (progState.currentSequence == SEQ_NIGHT) {
    stormState.stormEnabled = bleControl.stormEnabledNight;
  }

  Serial.print("  Current storm auto-trigger: ");
  Serial.println(stormState.stormEnabled ? "ENABLED" : "DISABLED");
  Serial.println("========================================");
}

// Get current time from synchronized clock
// Returns seconds since midnight (0-86399)
// Returns -1 if time is not synchronized
int32_t getCurrentTimeOfDay() {
  if (!timeSync.synchronized) {
    return -1;
  }

  // Calculate elapsed seconds since sync
  unsigned long elapsedMillis = millis() - timeSync.syncMillis;
  uint32_t elapsedSeconds = elapsedMillis / 1000;

  // Calculate current epoch time
  uint32_t currentEpoch = timeSync.syncEpoch + elapsedSeconds;

  // Calculate seconds since midnight (UTC)
  // Note: This is a simplified calculation that doesn't account for timezone
  // The app should send time in local timezone
  uint32_t secondsSinceMidnight = currentEpoch % 86400;

  return (int32_t)secondsSinceMidnight;
}

// Get current day of week (0=Sunday, 6=Saturday)
// Returns -1 if time is not synchronized
int8_t getCurrentDayOfWeek() {
  if (!timeSync.synchronized) {
    return -1;
  }

  // Calculate elapsed days since sync
  unsigned long elapsedMillis = millis() - timeSync.syncMillis;
  uint32_t elapsedDays = elapsedMillis / 86400000UL;

  // Calculate current day of week
  uint8_t currentDay = (timeSync.syncDayOfWeek + elapsedDays) % 7;

  return (int8_t)currentDay;
}

// Check if current time matches scheduled time
bool isScheduledTime() {
  if (!bleControl.scheduleEnabled) {
    return false;
  }

  if (!timeSync.synchronized) {
    return false;
  }

  // Get current time
  int32_t currentSeconds = getCurrentTimeOfDay();
  int8_t currentDay = getCurrentDayOfWeek();

  if (currentSeconds < 0 || currentDay < 0) {
    return false;
  }

  // Check if today is a scheduled day
  if (!(bleControl.scheduleDayMask & (1 << currentDay))) {
    return false;
  }

  // Convert schedule time to seconds since midnight
  int32_t scheduleSeconds = (bleControl.scheduleHour * 3600) + (bleControl.scheduleMinute * 60);

  // Check if we're within the scheduled minute (give 60 second window)
  int32_t timeDiff = abs(currentSeconds - scheduleSeconds);

  return (timeDiff < 60);
}

// Update scheduling logic in main loop
void updateScheduling() {
  if (!bleControl.scheduleEnabled || !timeSync.synchronized) {
    return;
  }

  // If waiting for scheduled start time
  if (bleControl.waitingForScheduledStart) {
    if (isScheduledTime()) {
      // Time to start!
      Serial.println("========================================");
      Serial.println("SCHEDULED START TRIGGERED");
      Serial.println("========================================");

      // Start sunrise sequence
      bleControl.waitingForScheduledStart = false;
      bleControl.currentAppMode = 0x01;  // CYCLE mode
      transitionToSequence(SEQ_SUNRISE_HOLD);
    }
  }

  // Check if we should transition to night mode after sunset
  if (bleControl.enableNightAfterSunset &&
      progState.currentSequence == SEQ_SUNSET_PROG &&
      progState.progressPercent >= 100.0) {

    Serial.println("========================================");
    Serial.println("SUNSET COMPLETE - TRANSITIONING TO NIGHT MODE");
    Serial.println("========================================");

    bleControl.currentAppMode = 0x02;  // NIGHT mode
    bleControl.currentlyInAutoNight = true;

    // Transition to night mode (same as pad 2 NIGHT mode handler)
    progState.currentSequence = SEQ_NIGHT;
    progState.isAnimating = false;

    // Set horizon to night blue at low brightness
    ColorGRBW nightHorizonColor = getPaletteColor(0);
    float nightBrightness = BRIGHTNESS_MIN_FACTOR;

    for(int i = 0; i < HORIZON_PIXELS; i++) {
      horizon.setPixelColor(i, horizon.Color(
        nightHorizonColor.r * nightBrightness,
        nightHorizonColor.g * nightBrightness,
        nightHorizonColor.b * nightBrightness,
        nightHorizonColor.w * nightBrightness
      ));
    }
    horizon.show();

    // Set clouds to night color
    ColorGRBW nightCloudColor = getCloudPaletteColor(0);
    for(int i = 0; i < CLOUD_1_PIXELS; i++) {
      cloud1State.currentPixelColors[i] = nightCloudColor;
      cloud1.setPixelColor(i, cloud1.Color(
        nightCloudColor.r * nightBrightness,
        nightCloudColor.g * nightBrightness,
        nightCloudColor.b * nightBrightness,
        nightCloudColor.w * nightBrightness
      ));
    }
    cloud1.show();

    for(int i = 0; i < CLOUD_2_PIXELS; i++) {
      cloud2State.currentPixelColors[i] = nightCloudColor;
      cloud2.setPixelColor(i, cloud2.Color(
        nightCloudColor.r * nightBrightness,
        nightCloudColor.g * nightBrightness,
        nightCloudColor.b * nightBrightness,
        nightCloudColor.w * nightBrightness
      ));
    }
    cloud2.show();

    for(int i = 0; i < CLOUD_3_PIXELS; i++) {
      cloud3State.currentPixelColors[i] = nightCloudColor;
      cloud3.setPixelColor(i, cloud3.Color(
        nightCloudColor.r * nightBrightness,
        nightCloudColor.g * nightBrightness,
        nightCloudColor.b * nightBrightness,
        nightCloudColor.w * nightBrightness
      ));
    }
    cloud3.show();

    // Deactivate cloud patches
    for(int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
      cloud1State.patches[i].active = false;
      cloud2State.patches[i].active = false;
      cloud3State.patches[i].active = false;
    }

    // Stars handled by updateStarVisibility() in main loop
  }
}

// ========== BLE CALLBACK CLASSES ==========

// Server callbacks - handle connection/disconnection
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("BLE: Device connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("BLE: Device disconnected");
  }
};

// Mode Control characteristic callbacks
class ModeControlCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      // Pass full buffer to handler
      handleModeControl((uint8_t*)value.data(), value.length());
    }
  }
};

// Cycle Config characteristic callbacks
class CycleConfigCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      // Pass full buffer to handler
      handleCycleConfig((uint8_t*)value.data(), value.length());
    }
  }
};

// Schedule Config characteristic callbacks
class ScheduleConfigCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      // Pass full buffer to handler
      handleScheduleConfig((uint8_t*)value.data(), value.length());
    }
  }
};

// Storm Config characteristic callbacks
class StormConfigCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      // Pass full buffer to handler
      handleStormConfig((uint8_t*)value.data(), value.length());
    }
  }
};

// Time Sync characteristic callbacks
class TimeSyncCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      // Pass full buffer to handler
      handleTimeSync((uint8_t*)value.data(), value.length());
    }
  }
};

// ========== BLE INITIALIZATION ==========

void initializeBLE() {
  Serial.println("========================================");
  Serial.println("Initializing BLE...");

  // Create BLE device with name "CloudyMountain"
  BLEDevice::init("CloudyMountain");

  // Create BLE server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create Mode Control characteristic (Write only)
  pModeControlChar = pService->createCharacteristic(
    MODE_CONTROL_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pModeControlChar->setCallbacks(new ModeControlCallbacks());

  // Create Cycle Config characteristic (Write only)
  pCycleConfigChar = pService->createCharacteristic(
    CYCLE_CONFIG_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCycleConfigChar->setCallbacks(new CycleConfigCallbacks());

  // Create Schedule Config characteristic (Write only)
  pScheduleConfigChar = pService->createCharacteristic(
    SCHEDULE_CONFIG_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pScheduleConfigChar->setCallbacks(new ScheduleConfigCallbacks());

  // Create Storm Config characteristic (Write only)
  pStormConfigChar = pService->createCharacteristic(
    STORM_CONFIG_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pStormConfigChar->setCallbacks(new StormConfigCallbacks());

  // Create Time Sync characteristic (Write only)
  pTimeSyncChar = pService->createCharacteristic(
    TIME_SYNC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pTimeSyncChar->setCallbacks(new TimeSyncCallbacks());

  // Create Current State characteristic (Read + Notify)
  pCurrentStateChar = pService->createCharacteristic(
    CURRENT_STATE_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCurrentStateChar->addDescriptor(new BLE2902());

  // Create Config Echo characteristic (Read + Notify)
  pConfigEchoChar = pService->createCharacteristic(
    CONFIG_ECHO_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pConfigEchoChar->addDescriptor(new BLE2902());

  // Start the service
  pService->start();
  Serial.println("BLE: Service started");

  // Configure advertising for universal compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);

  // Use standard BLE advertising intervals (slower but more compatible)
  // Android devices work best with slower, more standard intervals
  pAdvertising->setMinInterval(0x50);  // 50ms
  pAdvertising->setMaxInterval(0xA0);  // 100ms

  // Set connection parameters - standard values for broad compatibility
  pAdvertising->setMinPreferred(0x06);  // 7.5ms min connection interval
  pAdvertising->setMaxPreferred(0x12);  // 22.5ms max connection interval

  // Start advertising
  BLEDevice::startAdvertising();

  Serial.println("BLE: Advertising started (universal compatibility mode)");
  Serial.println("BLE: Device name: CloudyMountain");
  Serial.println("BLE: Advertising interval: 50-100ms");
  Serial.println("========================================");
}

// ========== SETUP FUNCTION ==========

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("CloudyMountain Initializing...");

  // Initialize random seed for varied lightning patterns
  randomSeed(analogRead(A0));  // Use floating analog pin for entropy

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

  // Initialize BLE
  initializeBLE();

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

  // Initialize stars pin as PWM output (starts at 0)
  pinMode(STARS_PIN, OUTPUT);
  analogWrite(STARS_PIN, 0);  // Start with stars off
  Serial.println("Stars PWM initialized (0%)");

  Serial.println("Setup complete!");
}

void loop() {
  // Handle BLE connection state changes
  // Restart advertising when disconnected
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Give the bluetooth stack time to get ready
    BLEDevice::startAdvertising(); // Use BLEDevice instead of pServer
    Serial.println("BLE: Disconnected - restarting advertising");
    oldDeviceConnected = deviceConnected;
  }
  // Connection event
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    Serial.println("BLE: Connected");
  }

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

  // Periodic schedule status output (every 10 seconds when schedule enabled)
  static unsigned long lastScheduleStatusTime = 0;
  if (bleControl.scheduleEnabled && timeSync.synchronized) {
    if (millis() - lastScheduleStatusTime > 10000) {
      int32_t currentSeconds = getCurrentTimeOfDay();
      int8_t currentDay = getCurrentDayOfWeek();

      Serial.print("Schedule monitor - Current: ");
      Serial.print(currentSeconds / 3600);
      Serial.print(":");
      int mins = (currentSeconds % 3600) / 60;
      if (mins < 10) Serial.print("0");
      Serial.print(mins);
      Serial.print(" | Target: ");
      if (bleControl.scheduleHour < 10) Serial.print("0");
      Serial.print(bleControl.scheduleHour);
      Serial.print(":");
      if (bleControl.scheduleMinute < 10) Serial.print("0");
      Serial.print(bleControl.scheduleMinute);
      Serial.print(" | Waiting: ");
      Serial.println(bleControl.waitingForScheduledStart ? "YES" : "NO");

      lastScheduleStatusTime = millis();
    }
  }

  // Update progression if animating
  if (progState.isAnimating) {
    updateProgression();
  }

  // Update scheduling logic (checks for scheduled start times)
  updateScheduling();

  // Check for random storm trigger when in DAY or NIGHT mode
  if (progState.currentSequence == SEQ_DAY || progState.currentSequence == SEQ_NIGHT) {
    checkStormTrigger();
  }

  // Update storm or cloud patches (mutually exclusive)
  if (progState.currentSequence == SEQ_STORM_DIM ||
      progState.currentSequence == SEQ_STORM_ACTIVE ||
      progState.currentSequence == SEQ_STORM_CLEAR) {
    // Storm is active - update storm state machine
    updateStorm();
  } else if (progState.currentSequence != SEQ_OFF) {
    // Normal mode - update cloud patch animations (skip if OFF)
    updateCloudPatches(cloud1State, cloud1);
    updateCloudPatches(cloud2State, cloud2);
    updateCloudPatches(cloud3State, cloud3);
  }

  // Update star visibility and fade
  updateStarVisibility();
  updateStarFade();

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
      {
        Serial.println("RESET/OFF - Turning off all LEDs and stopping sequences");

        // Stop any running sequences
        progState.isAnimating = false;
        progState.currentSequence = SEQ_OFF;
        progState.progressPercent = 0.0;

        // Clear storm state
        stormState.lightningFlashStartTime = 0;
        stormState.nextLightningTime = 0;

        // HARD RESET: Clear all cloud patches and reset to starting state
        Serial.println("Clearing all cloud patches...");

        // Deactivate all patches on all clouds
        for (int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
          cloud1State.patches[i].active = false;
          cloud2State.patches[i].active = false;
          cloud3State.patches[i].active = false;
        }

        // Reset all cloud pixels to starting deep blue (cloud color 0)
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

        // Turn off all LED strands
        setStrandColor(cloud1, 0, 0, 0, 0);
        setStrandColor(cloud2, 0, 0, 0, 0);
        setStrandColor(cloud3, 0, 0, 0, 0);
        setStrandColor(horizon, 0, 0, 0, 0);

        Serial.println("All LEDs off, all patches cleared, cloud states reset to deep blue");
      }
      break;
    case 5:  // Storm test mode (fast timing)
      Serial.println("Starting TEST STORM (fast timing)");
      startStorm(true);  // true = test mode
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
    case 8:  // Toggle storm auto-triggering
      stormState.stormEnabled = !stormState.stormEnabled;
      Serial.println("========================================");
      Serial.print("Storm auto-trigger: ");
      Serial.println(stormState.stormEnabled ? "ENABLED" : "DISABLED");
      Serial.println("========================================");
      if (stormState.stormEnabled) {
        stormState.lastStormCheckTime = millis();
      }
      break;
    case 9:  // TIME SYNC TEST - Simulate time sync from BLE
      {
        // Test scenario: Sync to current day at 6:00 AM
        // This allows testing scheduled start at 6:00 AM + offset seconds
        Serial.println("========================================");
        Serial.println("PAD 9 - TIME SYNC TEST");
        Serial.println("Simulating BLE time sync command");

        // Get current millis as base
        unsigned long currentMillis = millis();

        // Create a test timestamp for 6:00 AM today (21600 seconds = 6 hours)
        // We'll use the current day and set time to just before 6:00 AM
        // so we can trigger a scheduled event by pressing pad 10
        uint32_t testTimestamp = 1704441600;  // Example: Jan 5, 2024, 6:00:00 AM UTC
        uint8_t testDayOfWeek = 4;  // Thursday

        // Manually build the BLE command (6 bytes)
        uint8_t timeSyncData[6];
        timeSyncData[0] = testTimestamp & 0xFF;
        timeSyncData[1] = (testTimestamp >> 8) & 0xFF;
        timeSyncData[2] = (testTimestamp >> 16) & 0xFF;
        timeSyncData[3] = (testTimestamp >> 24) & 0xFF;
        timeSyncData[4] = testDayOfWeek;
        timeSyncData[5] = 0x00;  // Reserved

        // Call the BLE handler
        handleTimeSync(timeSyncData, 6);

        Serial.println("Time sync test complete. Use pad 10 to test scheduling.");
        Serial.println("========================================");
      }
      break;
    case 10:  // SCHEDULE TEST - Set up test schedule and display status
      {
        Serial.println("========================================");
        Serial.println("PAD 10 - SCHEDULE TEST");
        Serial.println("Setting test schedule for 30 seconds from now");

        // Calculate target time (30 seconds from now)
        int32_t currentSeconds = getCurrentTimeOfDay();
        if (currentSeconds < 0) {
          Serial.println("ERROR: Time not synchronized! Press pad 9 first.");
          Serial.println("========================================");
          break;
        }

        int32_t targetSeconds = currentSeconds + 30;  // 30 seconds from now
        if (targetSeconds >= 86400) targetSeconds -= 86400;  // Wrap at midnight

        uint8_t targetHour = targetSeconds / 3600;
        uint8_t targetMinute = (targetSeconds % 3600) / 60;

        // Get current day
        int8_t currentDay = getCurrentDayOfWeek();
        uint8_t dayMask = (1 << currentDay);  // Today only

        // Build schedule config BLE command (5 bytes)
        uint8_t scheduleData[5];
        scheduleData[0] = 0x01;  // Enabled
        scheduleData[1] = targetHour;
        scheduleData[2] = targetMinute;
        scheduleData[3] = dayMask;
        scheduleData[4] = 0x00;  // No night after sunset

        // Call the BLE handler
        handleScheduleConfig(scheduleData, 5);

        Serial.println();
        Serial.println("TEST SCHEDULE STATUS:");
        Serial.print("  Current time: ");
        Serial.print(getCurrentTimeOfDay() / 3600);
        Serial.print(":");
        Serial.print((getCurrentTimeOfDay() % 3600) / 60);
        Serial.print(":");
        Serial.println(getCurrentTimeOfDay() % 60);

        Serial.print("  Scheduled time: ");
        if (targetHour < 10) Serial.print("0");
        Serial.print(targetHour);
        Serial.print(":");
        if (targetMinute < 10) Serial.print("0");
        Serial.println(targetMinute);

        Serial.println();
        Serial.println("Wait 30 seconds for scheduled sunrise to start...");
        Serial.println("========================================");
      }
      break;
    case 11:  // SCHEDULE STATUS - Display current time and schedule status
      {
        Serial.println("========================================");
        Serial.println("PAD 11 - SCHEDULE STATUS");

        if (!timeSync.synchronized) {
          Serial.println("Time: NOT SYNCHRONIZED");
          Serial.println("Press pad 9 to sync time");
        } else {
          int32_t currentSeconds = getCurrentTimeOfDay();
          int8_t currentDay = getCurrentDayOfWeek();

          Serial.print("Current time: ");
          Serial.print(currentSeconds / 3600);
          Serial.print(":");
          int mins = (currentSeconds % 3600) / 60;
          if (mins < 10) Serial.print("0");
          Serial.print(mins);
          Serial.print(":");
          int secs = currentSeconds % 60;
          if (secs < 10) Serial.print("0");
          Serial.println(secs);

          const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
          Serial.print("Day: ");
          Serial.println(dayNames[currentDay]);
        }

        Serial.println();
        Serial.print("Schedule enabled: ");
        Serial.println(bleControl.scheduleEnabled ? "YES" : "NO");

        if (bleControl.scheduleEnabled) {
          Serial.print("Scheduled time: ");
          if (bleControl.scheduleHour < 10) Serial.print("0");
          Serial.print(bleControl.scheduleHour);
          Serial.print(":");
          if (bleControl.scheduleMinute < 10) Serial.print("0");
          Serial.println(bleControl.scheduleMinute);

          Serial.print("Waiting for start: ");
          Serial.println(bleControl.waitingForScheduledStart ? "YES" : "NO");

          Serial.print("Night after sunset: ");
          Serial.println(bleControl.enableNightAfterSunset ? "YES" : "NO");
        }

        Serial.println("========================================");
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
    case 5:  // Clear test patches and turn off CLOUD_1
      // Deactivate all patches on CLOUD_1
      for (int i = 0; i < MAX_PATCHES_PER_CLOUD; i++) {
        cloud1State.patches[i].active = false;
      }
      setStrandColor(cloud1, 0, 0, 0, 0);
      Serial.println("CLOUD_1 patches cleared and LEDs turned off");
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
