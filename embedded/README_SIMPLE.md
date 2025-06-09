# Simple Combo Counter - Embedded Edition

A flexible, configurable counter/combo system for nRF52840 with e-Paper display that can track any user-defined metrics and sync with external fitness tracking apps.

## Core Concept

This is the **embedded version** of ComboCounter - a simple but powerful counting device that:

- ✅ Tracks multiple user-defined counters (reps, sets, habits, scores, etc.)
- ✅ Supports different counter types (simple, combo, timed, accumulator)
- ✅ Uses e-Paper display for always-visible, battery-efficient operation
- ✅ Sends data via Bluetooth to external apps for logging and analysis
- ✅ Configurable quality levels for each action (Perfect/Good/Partial/Miss)

## Quick Start

### Hardware Setup
```
nRF52840 + 2.15" e-Paper Display + 4 Buttons + Vibration Motor
```

### Button Layout
```
┌─────────────────┐
│    UP/DOWN      │  ← Navigate counters / Adjust quality
│   SELECT/BACK   │  ← Increment / Decrement
└─────────────────┘
```

### Default Usage
1. **SELECT**: Increment current counter with selected quality
2. **BACK**: Decrement current counter  
3. **UP/DOWN**: Navigate between counters (or adjust quality if single counter)
4. **BACK (hold)**: Enter settings menu

## Counter Types

### 1. Simple Counter
```c
counter_configure_simple(counter, "Reps", 1);
```
- Basic increment/decrement
- Perfect for: rep counting, task completion, daily habits

### 2. Combo Counter  
```c
counter_configure_combo(counter, "Streak", 1, 3.0f, 0.1f);
```
- Multiplier builds with consecutive good actions
- Resets on miss (configurable)
- Perfect for: streaks, gaming scores, form-focused exercises

### 3. Timed Counter
```c
counter_configure_timed(counter, "Focus", 5, 0.2f);
```
- Multiplier decays over time
- Perfect for: meditation, focus sessions, sustained activities

### 4. Accumulator
```c
counter_configure_accumulator(counter, "Total", 1);
```
- Always incrementing total
- Perfect for: lifetime totals, volume tracking

## Quality Levels

Each action can be tracked with quality:
- **Perfect (3x)**: Full points, builds combo
- **Good (2x)**: Most points, builds combo  
- **Partial (1x)**: Some points, builds combo
- **Miss (0x)**: No points, may break combo

## External App Integration

### Bluetooth Messages

The device sends structured data that external apps can consume:

```c
typedef struct {
    uint8_t message_type;     // 1=counter_update, 2=stats, 3=config
    uint8_t counter_id;       // Which counter (0-7)
    uint32_t timestamp;       // When it happened
    int32_t count;           // Current count
    int32_t total;           // Lifetime total
    uint8_t quality;         // Action quality (0-3)
    char label[16];          // Counter name
    uint8_t checksum;        // Data validation
} BluetoothMessage;
```

### Integration Examples

**MyFitnessPal Integration:**
```javascript
// Receive rep data from ComboCounter
onComboCounterMessage(message) {
    if (message.label === "Reps" && message.quality >= 2) {
        logExerciseSet(currentExercise, message.count, currentWeight);
    }
}
```

**Habit Tracking App:**
```javascript
// Track daily habits
onComboCounterMessage(message) {
    if (message.label === "Streak") {
        updateHabitStreak(currentHabit, message.count);
    }
}
```

**Custom Analytics:**
```javascript
// Detailed form analysis
onComboCounterMessage(message) {
    analytics.track('exercise_rep', {
        quality: message.quality,
        count: message.count,
        timestamp: message.timestamp
    });
}
```

## Preset Configurations

### Workout Tracking
```c
preset_workout_reps(device);
// Creates: "Reps" (simple), "Sets" (simple), "Combo" (combo)
```

### Meditation/Breathing
```c
preset_meditation_breath(device);  
// Creates: "Breaths" (simple), "Focus" (timed)
```

### Habit Tracking
```c
preset_habit_tracker(device);
// Creates: "Streak" (combo), "Total" (accumulator)
```

### Gaming/Scoring
```c
preset_game_score(device);
// Creates: "Score" (combo), "Lives" (simple)
```

## Configuration

### Adding Custom Counters
```c
// Simple rep counter
counter_add(device, "Squats", COUNTER_TYPE_SIMPLE);

// Combo system for form focus
counter_add(device, "Perfect Form", COUNTER_TYPE_COMBO);
counter_configure_combo(&device->counters[1], "Perfect Form", 10, 5.0f, 0.1f);

// Timed breathing counter
counter_add(device, "Breath Hold", COUNTER_TYPE_TIMED);
counter_configure_timed(&device->counters[2], "Breath Hold", 5, 0.5f);
```

### Settings Menu
Hold BACK button to access:
- Add/Remove counters
- Save/Load presets  
- Reset all data
- Bluetooth settings

## Use Cases

### 1. Weight Lifting
- **Reps Counter**: Track reps with quality (full ROM vs partial)
- **Sets Counter**: Count completed sets
- **Form Streak**: Combo counter for consecutive perfect reps

### 2. Calisthenics
- **Push-ups**: Simple counter with quality tracking
- **Plank Time**: Timed counter for sustained holds
- **Daily Streak**: Combo counter for consistency

### 3. Meditation/Mindfulness
- **Breath Counter**: Simple breathing count
- **Focus Score**: Timed counter that decays with distractions
- **Session Streak**: Combo for daily practice

### 4. Habit Building
- **Daily Goal**: Simple binary tracker (done/not done)
- **Consistency Streak**: Combo that resets on missed days
- **Lifetime Total**: Accumulator for long-term progress

### 5. Gaming/Challenges
- **High Score**: Combo counter with multipliers
- **Achievement Points**: Accumulator for total progress
- **Lives Remaining**: Simple counter that decrements

## Memory Usage

```
Flash: ~180KB (including SoftDevice)
RAM:   ~48KB (Clay UI + App + Nordic SDK)
```

## Battery Life

- **Active Use**: 12-16 hours with frequent button presses
- **Standby**: 3-4 weeks with occasional wake-ups  
- **Deep Sleep**: 3-4 months with rare interactions

## Build Instructions

1. **Install Nordic SDK 17.1.0**
2. **Update Makefile paths**
3. **Build and flash**:
   ```bash
   make clean
   make
   make flash_all
   ```

## External App Development

### Bluetooth Service UUID
```
Service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
TX Char: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (device → app)
RX Char: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (app → device)
```

### Message Format
```javascript
// Parse incoming counter updates
function parseCounterMessage(data) {
    return {
        type: data[0],
        counterId: data[1], 
        timestamp: data.readUInt32LE(2),
        count: data.readInt32LE(6),
        total: data.readInt32LE(10),
        quality: data[14],
        label: data.slice(15, 31).toString().replace(/\0.*$/g,''),
        checksum: data[31]
    };
}
```

### React Native Example
```javascript
import { BleManager } from 'react-native-ble-plx';

class ComboCounterBridge {
    constructor() {
        this.manager = new BleManager();
        this.device = null;
    }
    
    async connect() {
        this.device = await this.manager.connectToDevice(COMBO_COUNTER_ID);
        await this.device.discoverAllServicesAndCharacteristics();
        
        // Listen for counter updates
        this.device.monitorCharacteristicForService(
            SERVICE_UUID, TX_CHARACTERISTIC_UUID,
            (error, characteristic) => {
                if (characteristic?.value) {
                    const message = parseCounterMessage(characteristic.value);
                    this.onCounterUpdate(message);
                }
            }
        );
    }
    
    onCounterUpdate(message) {
        // Send to your fitness tracking backend
        fetch('/api/workouts/log', {
            method: 'POST',
            body: JSON.stringify({
                exercise: message.label,
                reps: message.count,
                quality: message.quality,
                timestamp: message.timestamp
            })
        });
    }
}
```

## Philosophy

This embedded ComboCounter is designed to be:

1. **Flexible**: Configure for any counting/tracking scenario
2. **Focused**: Single-purpose device that does one thing well  
3. **Connected**: Integrates with existing fitness/productivity ecosystems
4. **Persistent**: Always-on e-Paper display, week-long battery life
5. **Simple**: Four buttons, clear feedback, no complexity

The goal is to provide a **physical interface** for tracking that complements digital apps, not replaces them. Think of it as a dedicated "clicker" that sends rich data to your existing tracking workflows.

## Contributing

This is designed to be easily customizable:
- Add new counter types by extending `CounterType` enum
- Create new presets in `preset_*` functions  
- Modify Clay UI layouts in render functions
- Add new Bluetooth message types for richer data

## License

Part of the ComboCounter project. See main project for license details.