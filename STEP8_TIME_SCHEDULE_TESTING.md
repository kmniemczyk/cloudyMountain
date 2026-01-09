# Step 8: Time Sync & Scheduling - Testing Guide

## Overview
This guide covers testing the time synchronization and scheduling features added in Step 8. These features allow the CloudyMountain lamp to automatically start sunrise sequences at scheduled times.

## Features Implemented

### 1. Time Synchronization (BLE Characteristic: TIME_SYNC_UUID)
- Receives current time from BLE app
- Maintains internal clock based on sync point
- Tracks day of week for scheduling

### 2. Schedule Configuration (BLE Characteristic: SCHEDULE_CONFIG_UUID)
- Configure scheduled start times
- Day-of-week selection (Sunday-Saturday)
- Optional "Night after sunset" mode
- Enable/disable scheduling

### 3. Storm Configuration (BLE Characteristic: STORM_CONFIG_UUID)
- Enable/disable storms during day mode
- Enable/disable storms during night mode

---

## Touch Pad Testing Interface

### Quick Reference
| Pad | Function | Description |
|-----|----------|-------------|
| 9   | Time Sync Test | Simulates BLE time sync to 6:00 AM Thursday |
| 10  | Schedule Test | Sets schedule for 30 seconds from now |
| 11  | Schedule Status | Displays current time and schedule info |

---

## Testing Procedures

### Test 1: Time Synchronization

**Objective:** Verify that time sync works correctly

**Steps:**
1. Open Serial Monitor (115200 baud)
2. Touch pad **9** (Time Sync Test)
3. Observe serial output

**Expected Output:**
```
========================================
PAD 9 - TIME SYNC TEST
Simulating BLE time sync command
========================================
BLE: Time synchronization received
  Unix timestamp: 1704441600
  Day of week: 4 (Thursday)
  Time synchronized successfully!
========================================
Time sync test complete. Use pad 10 to test scheduling.
========================================
```

**Verification:**
- Time sync should show "synchronized successfully"
- Day of week should be displayed correctly

---

### Test 2: Schedule Status Check

**Objective:** Verify current time tracking

**Steps:**
1. After syncing time (pad 9), touch pad **11** (Schedule Status)
2. Observe current time display

**Expected Output:**
```
========================================
PAD 11 - SCHEDULE STATUS
Current time: 6:0:0
Day: Thursday

Schedule enabled: NO
========================================
```

**Verification:**
- Current time should increment each time you check
- Day should match the synced day

---

### Test 3: Scheduled Start (30-second test)

**Objective:** Verify automatic sunrise trigger at scheduled time

**Steps:**
1. Touch pad **9** to sync time
2. Touch pad **11** to verify time is synced
3. Touch pad **10** to set schedule for 30 seconds from now
4. Wait and watch serial monitor

**Expected Output (immediately after pad 10):**
```
========================================
PAD 10 - SCHEDULE TEST
Setting test schedule for 30 seconds from now
========================================
BLE: Schedule configuration received
  Enabled: YES
  Time: 06:00
  Days: Thu
  Night after sunset: NO
  Schedule activated - waiting for next scheduled time
========================================

TEST SCHEDULE STATUS:
  Current time: 6:0:5
  Scheduled time: 06:00

Wait 30 seconds for scheduled sunrise to start...
========================================
```

**Expected Output (after ~30 seconds):**
```
Schedule monitor - Current: 6:0 | Target: 06:00 | Waiting: YES
Schedule monitor - Current: 6:0 | Target: 06:00 | Waiting: YES
========================================
SCHEDULED START TRIGGERED
========================================
Transitioning to sequence: 1
```

**Verification:**
- Every 10 seconds, you should see "Schedule monitor" status
- After 30 seconds, "SCHEDULED START TRIGGERED" should appear
- Sunrise sequence should begin automatically

---

## BLE Testing (With Mobile App)

### Time Sync BLE Command Format

**Characteristic:** `TIME_SYNC_UUID` (beb5483e-36e1-4688-b7f5-ea07361b26ae)

**Data Format:** 6 bytes (little-endian)
```
Byte 0-3: Unix timestamp (uint32_t)
Byte 4:   Day of week (0=Sunday, 6=Saturday)
Byte 5:   Reserved (0x00)
```

**Example (Python):**
```python
import time
import struct

# Get current time
timestamp = int(time.time())
day_of_week = time.localtime().tm_wday  # 0=Monday in Python, adjust if needed

# Pack into bytes (little-endian)
data = struct.pack('<IB', timestamp, day_of_week) + b'\x00'

# Write to BLE characteristic
await client.write_gatt_char(TIME_SYNC_UUID, data)
```

---

### Schedule Config BLE Command Format

**Characteristic:** `SCHEDULE_CONFIG_UUID` (beb5483e-36e1-4688-b7f5-ea07361b26aa)

**Data Format:** 5 bytes
```
Byte 0: Enable (0x00=disabled, 0x01=enabled)
Byte 1: Hour (0-23)
Byte 2: Minute (0-59)
Byte 3: Day mask (bit 0=Sunday, bit 6=Saturday)
        Example: 0x7F = every day (0111 1111)
                 0x3E = weekdays only (0011 1110)
                 0x41 = weekends only (0100 0001)
Byte 4: Night after sunset (0x00=disabled, 0x01=enabled)
```

**Example (Python):**
```python
# Schedule for 6:30 AM, Monday-Friday, with night mode after sunset
data = bytes([
    0x01,  # Enabled
    6,     # Hour: 6 AM
    30,    # Minute: 30
    0x3E,  # Days: Mon-Fri (bits 1-5 set)
    0x01   # Night after sunset enabled
])

await client.write_gatt_char(SCHEDULE_CONFIG_UUID, data)
```

---

### Storm Config BLE Command Format

**Characteristic:** `STORM_CONFIG_UUID` (beb5483e-36e1-4688-b7f5-ea07361b26ab)

**Data Format:** 2 bytes
```
Byte 0: Storm during day (0x00=disabled, 0x01=enabled)
Byte 1: Storm during night (0x00=disabled, 0x01=enabled)
```

**Example (Python):**
```python
# Enable storms during day only
data = bytes([0x01, 0x00])

await client.write_gatt_char(STORM_CONFIG_UUID, data)
```

---

## Schedule Monitoring

When a schedule is active, the serial monitor will output status every 10 seconds:

```
Schedule monitor - Current: 6:15 | Target: 06:30 | Waiting: YES
```

This helps you verify that:
1. Time is tracking correctly
2. Schedule target is set correctly
3. System is waiting for the scheduled time

---

## Night Mode After Sunset

When enabled, this feature automatically transitions to NIGHT mode after a sunset sequence completes.

**To Test:**
1. Set schedule with "Night after sunset" enabled (byte 4 = 0x01)
2. Trigger sunset sequence (pad 2)
3. Wait for sunset to complete (20 minutes in production, 4 minutes in test mode)
4. System should automatically enter NIGHT mode:
   - Low brightness (12.5%)
   - Deep blue colors
   - Stars ON

---

## Troubleshooting

### Issue: "Time not synchronized" error
**Solution:** Touch pad 9 to sync time before testing schedules

### Issue: Schedule doesn't trigger
**Checks:**
1. Verify time is synchronized (pad 11)
2. Check that current day matches scheduled days
3. Verify schedule is enabled
4. Check that "Waiting" status is YES

### Issue: Wrong time displayed
**Solution:** Time sync uses Unix timestamp - ensure your app sends local time, not UTC

### Issue: Schedule triggers immediately
**Possible cause:** Scheduled time is in the past or within 60-second window of current time

---

## Advanced Testing Scenarios

### Scenario 1: Multi-Day Schedule
Test scheduling for specific days of the week:
```python
# Monday, Wednesday, Friday only
day_mask = (1 << 1) | (1 << 3) | (1 << 5)  # Bits 1, 3, 5 = 0x2A

data = bytes([0x01, 7, 0, day_mask, 0x00])
```

### Scenario 2: Daily Schedule with Night Mode
Test complete day cycle:
```python
# Every day at 6:00 AM with auto-night after sunset
data = bytes([0x01, 6, 0, 0xFF, 0x01])
```

### Scenario 3: Weekend-Only Schedule
```python
# Saturday and Sunday at 9:00 AM
day_mask = (1 << 0) | (1 << 6)  # Sunday (bit 0) and Saturday (bit 6) = 0x41

data = bytes([0x01, 9, 0, day_mask, 0x00])
```

---

## Integration with Mobile App

### Connection Flow
1. App connects to "CloudyMountain" BLE device
2. App sends TIME_SYNC command with current local time
3. App sends SCHEDULE_CONFIG to set wake-up time
4. App sends STORM_CONFIG to enable/disable storms
5. Device stores settings and runs autonomously

### Recommended App Features
- Time picker for schedule configuration
- Day-of-week selector (checkboxes for each day)
- Toggle for "Night mode after sunset"
- Status display showing:
  - Current device time
  - Next scheduled trigger
  - Time until next trigger
  - Schedule enabled/disabled state

---

## BLE Testing Tools

### nRF Connect (Mobile App)
1. Install nRF Connect for iOS/Android
2. Connect to "CloudyMountain"
3. Navigate to TIME_SYNC characteristic
4. Write hex values manually (e.g., for timestamp 1704441600 = `0x659B8C80`)

### Python bleak Library
```python
import asyncio
from bleak import BleakClient

SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
TIME_SYNC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26ae"

async def test_time_sync():
    async with BleakClient("CloudyMountain") as client:
        timestamp = int(time.time())
        day = 4  # Thursday
        data = struct.pack('<IB', timestamp, day) + b'\x00'
        await client.write_gatt_char(TIME_SYNC_UUID, data)
        print("Time synced!")

asyncio.run(test_time_sync())
```

---

## Expected Behavior Summary

| Action | Expected Result |
|--------|----------------|
| Time Sync | Device stores current time and day |
| Schedule Config | Device waits for scheduled time |
| Scheduled Time Reached | Sunrise sequence starts automatically |
| Sunset with Night Mode ON | Auto-transition to NIGHT mode after sunset |
| Storm Config | Enables/disables auto storm triggers |

---

## Next Steps

After verifying all tests pass:
1. Integrate time sync into your mobile app
2. Add schedule configuration UI
3. Test with real-world schedules (e.g., wake up at 6:30 AM daily)
4. Monitor long-term time accuracy (may need periodic re-sync)

---

## Notes

- Internal clock drifts slightly - recommend re-syncing daily
- Schedule check runs every loop iteration (~10ms)
- Scheduled trigger has 60-second window for flexibility
- Multiple schedules per day not currently supported (single schedule time only)
- Midnight wraparound is handled automatically