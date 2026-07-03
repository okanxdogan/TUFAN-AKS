
# ESP_AKS Beginner Guide

## Overview
This guide helps beginners open, build, and run the ESP_AKS project using PlatformIO. The repository is already configured for an ESP32 Dev module with the ESP-IDF framework.

## Prerequisites
- PlatformIO IDE installed in VS Code
- ESP32 board connected by USB
- `esp32dev` board support installed in PlatformIO
- Basic familiarity with C/C++ and embedded projects

## Open the Existing Project
1. Open VS Code.
2. Use **File > Open Folder...** and select `/Users/melekzevit/Desktop/Ali-Okul/TFN-Codes/TUFAN-AKS/ESP_AKS`.
3. PlatformIO should detect the project and load the environment from `platformio.ini`.

## Project Layout
```
ESP_AKS/
├── include/          # Shared configuration headers
│   └── SystemConfig.h
├── lib/              # Project libraries and modules
│   ├── CanManager/
│   ├── DisplayHMI/
│   ├── RelayManager/
│   └── Telemetry/
├── src/              # Application sources
│   ├── main.cpp
│   ├── VcuLogic.cpp
│   └── VcuLogic.h
├── platformio.ini    # PlatformIO build configuration
├── sdkconfig.esp32dev
└── .pio/             # Build artifacts (do not edit)
```

## What This Project Uses
- `platformio.ini` defines:
  - `env:esp32dev`
  - `platform = espressif32@6.13.0`
  - `framework = espidf`
  - `monitor_speed = 115200`
  - `build_flags = -std=gnu++17`
- `src/main.cpp` starts three FreeRTOS tasks:
  - CAN communication task
  - HMI display task
  - VCU logic task
- `include/SystemConfig.h` holds pin assignments and UART/SPI settings.

## Key Files to Explore
- `src/main.cpp` — application entry point and task setup
- `src/VcuLogic.cpp` — vehicle control logic stub
- `include/SystemConfig.h` — board pin configuration
- `lib/CanManager/CanManager.h` — CAN bus manager interface
- `lib/DisplayHMI/DisplayHMI.h` — display/HMI interface
- `lib/RelayManager/RelayManager.h` — relay control abstractions
- `lib/Telemetry/Telemetry.h` — telemetry and data reporting

## Build and Upload
### Build
From the project root:
```bash
platformio run -e esp32dev
```

### Upload to the board
```bash
platformio run -e esp32dev --target upload
```

### Open the serial monitor
```bash
platformio device monitor -e esp32dev --baud 115200
```

## What to Change as a Beginner
- Update pin definitions in `include/SystemConfig.h` if your hardware wiring differs.
- Add CAN message handling and sensor reads inside `src/main.cpp` and `lib/CanManager`.
- Implement state transitions and contactor logic in `src/VcuLogic.cpp`.
- Add display updates to `lib/DisplayHMI` and call them from the HMI task.

## Troubleshooting
- If build fails, install the `espressif32` platform:
  ```bash
  platformio platform install espressif32
  ```
- If upload fails, verify the correct USB port and board connection.
- If monitor output is blank, check `monitor_speed = 115200` and board power.

### Known Build Errors & Python Version Conflicts
If you are using a bleeding-edge Linux distro (like Arch or CachyOS) or have Python 3.13+, you might encounter a `ModuleNotFoundError: No module named 'idf_component_manager'` or a PyO3 Rust compilation failure during the first build.

**Fix:** Force PlatformIO to use a stable Python version (like 3.11 or 3.12).
1. Install Python 3.11 on your system.
2. Delete the broken virtual environment: `rm -rf ~/.platformio/penv` and `rm -rf .pio/`
3. Recreate it manually: `python3.11 -m venv ~/.platformio/penv`
4. Install PIO: `~/.platformio/penv/bin/pip install -U platformio`
5. Re-run: `platformio run -e esp32dev`


## Beginner Workflow
1. Open the existing project folder in VS Code.
2. Inspect `platformio.ini` and `include/SystemConfig.h`.
3. Build the project with `platformio run -e esp32dev`.
4. Fix compiler issues in `src/` and `lib/` files.
5. Upload and observe behavior using `platformio device monitor`.
6. Commit your changes and push when ready.

## Helpful Links
- PlatformIO Docs: https://docs.platformio.org/
- ESP-IDF Docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/
- ESP32 Pin Mapping Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html
