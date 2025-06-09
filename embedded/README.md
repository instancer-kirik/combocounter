# Embedded Fitness Tracker with Clay UI

A complete weight lifting rep counter and workout tracker for the nRF52840 microcontroller with Clay UI and e-Paper display integration.

## Overview

This embedded fitness tracker combines the power of Clay UI (immediate-mode GUI library) with a 4-color e-Paper display to create a battery-efficient, always-visible workout companion. The system tracks reps, sets, rest periods, and provides comprehensive workout statistics.

## Hardware Requirements

### Core Components
- **nRF52840 Development Kit** (PCA10056) or compatible board
- **2.15" e-Paper HAT+ (G)** - 296x160 pixels, 4-color (Black/White/Red/Yellow)
- **4 Push Buttons** - Navigation (Up/Down/Select/Back)
- **Vibration Motor** - Haptic feedback for rest timer
- **3.7V LiPo Battery** - For portable operation

### Pin Connections

| Component | nRF52840 Pin | Description |
|-----------|--------------|-------------|
| E-Paper CS | P0.08 | SPI Chip Select |
| E-Paper DC | P0.09 | Data/Command |
| E-Paper RST | P0.10 | Reset |
| E-Paper BUSY | P0.11 | Busy Status |
| E-Paper SCK | P0.03 | SPI Clock |
| E-Paper MOSI | P0.04 | SPI Data |
| Button Up | P0.13 | Navigation Up |
| Button Down | P0.14 | Navigation Down |
| Button Select | P0.15 | Confirm/Select |
| Button Back | P0.16 | Back/Cancel |
| Vibration Motor | P0.17 | PWM/Digital Out |
| Battery ADC | P0.02 | Battery Level |

## Software Architecture

### Clay UI Integration

The system uses Clay UI as the primary rendering engine with a custom e-Paper renderer:

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   Fitness Core  │───▶│    Clay UI       │───▶│  E-Paper        │
│   (Logic)       │    │  (Rendering)     │    │  Hardware       │
└─────────────────┘    └──────────────────┘    └─────────────────┘
```

**Key Components:**
- `fitness_core.c/h` - Core fitness tracking logic
- `clay_epaper_renderer.c/h` - Clay UI to e-Paper adapter
- `epaper_hardware_nrf52840.c` - Hardware-specific SPI driver
- `nrf52840_main_complete.c` - Main application with Clay integration

### Memory Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    nRF52840 RAM (256KB)                    │
├─────────────────┬─────────────────┬─────────────────────────┤
│   Clay Arena    │   App Memory    │    Nordic SDK Stack     │
│     (48KB)      │     (16KB)      │      (Rest ~192KB)      │
└─────────────────┴─────────────────┴─────────────────────────┘
```

## Software Setup

### Prerequisites

1. **Nordic nRF5 SDK 17.1.0**
   ```bash
   wget https://www.nordicsemi.com/-/media/Software-and-other-downloads/SDKs/nRF5/Binaries/nRF5SDK1710ddde560.zip
   unzip nRF5SDK1710ddde560.zip
   ```

2. **ARM GCC Toolchain**
   ```bash
   # On Ubuntu/Debian
   sudo apt install gcc-arm-none-eabi

   # On macOS with Homebrew
   brew install --cask gcc-arm-embedded
   ```

3. **nRF Command Line Tools**
   - Download from Nordic's website
   - Includes `nrfjprog` and `mergehex`

4. **Clay UI Library**
   - Already included in `../src/clay.h`
   - Single-header immediate-mode UI library

### Building the Project

1. **Update SDK Path**
   Edit `Makefile` and set the correct SDK path:
   ```makefile
   SDK_ROOT := /path/to/nRF5_SDK_17.1.0_ddde560
   ```

2. **Build the Firmware**
   ```bash
   cd ComboCounter/embedded
   make clean
   make
   ```

3. **Check Memory Usage**
   ```bash
   make size
   ```

4. **Flash SoftDevice and Application**
   ```bash
   make flash_all
   ```

## Features

### Core Functionality

- **Rep Counting**: Manual rep entry with quality tracking (Perfect/Good/Partial/Failed)
- **Set Management**: Track sets, weight, and rest periods
- **Workout Sessions**: Organize exercises into complete workouts
- **Rest Timer**: Configurable rest periods with vibration alerts
- **Statistics**: Lifetime tracking of workouts, sets, reps, and volume
- **Power Management**: Auto-sleep with e-Paper display persistence
- **Clay UI**: Smooth, immediate-mode interface optimized for e-Paper

### Clay UI Advantages

1. **Memory Efficient**: Arena allocation perfect for embedded systems
2. **Immediate Mode**: No state management overhead
3. **Flexible Layout**: Responsive design that adapts to content
4. **E-Paper Optimized**: Render commands batched for efficient display updates
5. **Font System**: Multiple embedded fonts (8x8, 8x12, 8x16, 12x24)

### User Interface

#### Navigation
- **Up/Down**: Navigate menus, adjust values
- **Select**: Confirm selection, add rep
- **Back**: Return to previous screen, cancel action

#### Screens

1. **Workout Select**: Choose from predefined or custom workouts
2. **Exercise List**: View and select exercises in current workout
3. **Active Set**: Rep counting interface with live feedback
4. **Rest Timer**: Countdown timer between sets
5. **Statistics**: View workout history and totals
6. **Settings**: Configure app behavior

### Exercise Types

- **Compound**: Multi-joint movements (squats, deadlifts, bench press)
- **Isolation**: Single-joint movements (curls, extensions)
- **Cardio**: Time-based or interval tracking
- **Isometric**: Hold-based exercises (planks, wall sits)

## Usage Guide

### Starting a Workout

1. Power on device
2. Select "Quick Workout" from main menu
3. Navigate through pre-loaded exercises
4. Begin first set

### Tracking Reps

During an active set:
- **Up Button**: Add perfect rep (full ROM, good form)
- **Down Button**: Add partial rep (incomplete ROM)
- **Select**: Complete current set
- **Back**: End exercise

### Set Completion

1. Press **Select** when set is complete
2. Weight is auto-filled from target
3. Rest timer starts automatically (if enabled)
4. **Select** during rest to skip, **Back** to return to set

### Data Persistence

- Workout data saved to Nordic's FDS (Flash Data Storage)
- Statistics persist across power cycles
- Settings retained between sessions

## Customization

### Adding New Exercises

Edit the workout initialization in `nrf52840_main_complete.c`:

```c
// In handle_navigation() for SCREEN_WORKOUT_SELECT
exercise_add(&g_fitness_tracker.current_workout, "New Exercise", 
             EXERCISE_COMPOUND, 8, 1000);  // 8 reps, 100.0kg
```

### Modifying UI Layout

Clay UI makes customization straightforward. Example from `render_active_set_screen()`:

```c
CLAY(CLAY_ID("SetInfo"), 
     CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()}, 
                 .layoutDirection = CLAY_TOP_TO_BOTTOM, 
                 .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}, 
                 .childGap = 8)) {
    
    // Add your custom UI elements here
    CLAY_TEXT(CLAY_ID("CustomText"), CLAY_STRING("Custom Element"),
             CLAY_TEXT_CONFIG(.textColor = {0, 0, 0, 1}, .fontSize = 16));
}
```

### E-Paper Color Mapping

Customize color conversion in `clay_epaper_renderer.c`:

```c
EpaperColor clay_epaper_convert_color(Clay_Color clay_color) {
    // Custom color mapping logic
    if (clay_color.r > 0.8f && clay_color.g < 0.3f) {
        return EPAPER_COLOR_RED;  // Perfect reps in red
    }
    // ... rest of mapping
}
```

### Fonts

Add custom fonts by defining new `EpaperFont` structures:

```c
const EpaperFont CUSTOM_FONT = {
    .bitmap = custom_font_data,
    .width = 16,
    .height = 24,
    .first_char = 32,
    .char_count = 95
};
```

## Power Optimization

### Battery Life Strategies

1. **E-Paper Advantage**: Zero power consumption when static
2. **Clay Efficiency**: Immediate-mode rendering minimizes memory allocations
3. **Smart Updates**: Only refresh display when content changes
4. **Deep Sleep**: CPU enters low-power mode between interactions

### Expected Battery Life

- **Active Use**: 12-16 hours continuous workout tracking
- **Standby**: 3-5 weeks with periodic wake-ups
- **Sleep Mode**: 3-4 months with occasional button presses

## Development and Debugging

### Memory Usage

Check current memory usage:
```bash
make size
```

Typical usage:
- **Flash**: ~180KB (including SoftDevice)
- **RAM**: ~64KB (Clay + App + SDK stack)

### Debugging

1. **Enable RTT Logging**:
   ```c
   #define NRF_LOG_ENABLED 1
   #define NRF_LOG_DEFAULT_LEVEL 4
   ```

2. **Start Debug Session**:
   ```bash
   make debug
   # In another terminal:
   arm-none-eabi-gdb _build/nrf52840_xxaa.out
   (gdb) target remote localhost:2331
   ```

3. **View Logs**:
   ```bash
   JLinkRTTViewer
   ```

### Performance Monitoring

Clay UI provides built-in performance metrics. Enable with:
```c
Clay_SetDebugModeEnabled(true);
```

## Troubleshooting

### Build Issues

1. **SDK Path Error**: Verify `SDK_ROOT` in Makefile
2. **Missing Tools**: Install ARM GCC and nRF command line tools
3. **Clay Compilation**: Ensure `CLAY_IMPLEMENTATION` is defined once

### Runtime Issues

1. **Display Not Working**: 
   - Check SPI connections
   - Verify e-Paper power supply
   - Check busy pin wiring

2. **Memory Errors**:
   - Reduce `CLAY_MEMORY_SIZE` if needed
   - Check arena allocation in Clay
   - Monitor stack usage

3. **Button Issues**:
   - Verify pull-up resistors
   - Check debouncing settings
   - Test GPIO configuration

### Display Issues

1. **Ghosting**: Normal for e-Paper, use full refresh occasionally
2. **Slow Updates**: Expected behavior, ~2-3 seconds per refresh
3. **Color Issues**: Check 4-color display wiring and commands

## Future Enhancements

### Planned Features

1. **Bluetooth Connectivity**: Sync with smartphone app
2. **IMU Integration**: Automatic rep detection
3. **Advanced Analytics**: Form analysis and progression tracking
4. **Custom Workouts**: User-defined exercise templates

### Clay UI Roadmap

1. **Animation Support**: Smooth transitions for better UX
2. **Advanced Layouts**: Grid and table components
3. **Touch Support**: For future touch-enabled displays
4. **Theming**: Dark/light mode support

## Hardware Alternatives

### Microcontroller Options

- **nRF52832**: Lower cost, requires memory optimization
- **ESP32**: WiFi capability, different toolchain
- **STM32L4**: Ultra-low power, ARM ecosystem

### Display Alternatives

- **2.9" e-Paper**: Larger display, same driver
- **Sharp Memory LCD**: Faster updates, different power profile
- **OLED**: Higher contrast, requires power management changes

## Contributing

### Code Style

- Follow Nordic SDK conventions
- Use Clay UI best practices
- Comment complex algorithms
- Maintain consistent naming

### Testing Checklist

- [ ] All navigation paths work
- [ ] Data persists across power cycles
- [ ] Memory usage within limits
- [ ] Display updates correctly
- [ ] Battery life meets expectations

### Pull Request Guidelines

1. Test on actual hardware
2. Update documentation
3. Verify no build warnings
4. Include memory usage report

## Resources

- [nRF52840 Product Specification](https://infocenter.nordicsemi.com/pdf/nRF52840_PS_v1.1.pdf)
- [Clay UI Documentation](https://github.com/nicbarker/clay)
- [e-Paper Display Datasheet](https://www.waveshare.com/wiki/2.15inch_e-Paper_HAT_(G))
- [Nordic nRF5 SDK Documentation](https://infocenter.nordicsemi.com/index.jsp)

## License

This embedded fitness tracker is part of the ComboCounter project. See the main project README for license information.