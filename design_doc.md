# Comboc Design Document
Alternative names: Combometer, ComboTracker, Combotroc, Combotron, Comboraptor

## Inspiration
- SSX Tricky style combo system
- Background activity tracking
- User-defined increment triggers
- Persistent across focus changes

## Goals

1. **Performance**
   - Zero heap allocations in core systems?
   - Cache-friendly data structures?
   - Predictable, consistent timing?
   - Minimal dependencies?
   - Written in Zig, with optional WASM target
  
2. **Usability** 
   - Simple, clear API
   - Flexible configuration, multiple contexts, embeddable, integrations
   - Good documentation

3. **Visual Feedback**
   - Smooth animations
   - Clear state communication
   - Configurable styling
   - Responsive updates

## Core Systems

### Combo Tracking
```zig
pub const ComboState = struct {
    label: [64]u8,
    score: i32,
    combo: i32,
    max_combo: i32,
    multiplier: f32,
    decay_pause: f32,
    total_hits: u32,
    perfect_hits: u32,
    miss_hits: u32,
    paused: bool,
};
```

Key features:
- Simple increment/decrement interface
- Background tracking with pause/resume
- Focus-aware decay system
- Multiple independent trackers
- Persistent state saving

### Hit Detection

```c
typedef enum {
COMBO_HIT_PERFECT = 3, // 100% accuracy
COMBO_HIT_GOOD = 2, // 75% accuracy
COMBO_HIT_OK = 1, // 50% accuracy
COMBO_MISS = 0 // Break combo
} ComboHitType;
```

Scoring formula:
- Base points × Combo multiplier × Hit type multiplier
- Configurable base points per hit type
- Combo multiplier caps at 2.0x by default

### Visual Feedback

UI Components:
1. Score Display
   - Current score
   - High score tracking
   - Score animations

2. Combo Counter
   - Current combo count
   - Combo multiplier
   - Decay timer visualization

3. Stats Display  
   - Perfect/Good/Miss counts
   - Accuracy percentage
   - Max combo reached

## Integration

### Clay UI Integration

```c
typedef struct {
    ComboState* state;
    Clay_Color perfect_color;
    Clay_Color good_color;
    Clay_Color miss_color;
    float animation_time;
} ComboUI;
```

Features:
- Floating overlay
- Smooth animations
- Color coding
- Configurable positioning

### Game Engine Integration

Required callbacks:
- Hit detection
- Score updates
- State persistence (optional)
- Custom rendering (optional)

## Future Considerations

1. **Extensions**
   - Sound effects system
   - Particle effects
   - Custom scoring formulas
   - Network sync support

2. **Optimizations**
   - SIMD support
   - Thread safety options
   - Memory pooling
   - State compression

3. **Tools**
   - Debug visualization
   - State inspection
   - Performance metrics
   - Testing utilities