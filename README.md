> ⚠️ **BU DİZİN (repo kökü) ASIL ARAÇ FİRMWARE'İNİ İÇERMEZ.** Bu, bataryayı ilk keşif/test amaçlı kullanılan bir CAN sniffer aracıdır. Asıl AKS firmware'i için `ESP_AKS/` dizinine bakınız (`ESP_AKS/README.md`).

# TUFAN-AKS

TUFAN-AKS is the Vehicle Control Unit (VCU / AKS) firmware for the TUFAN electric vehicle platform. The project targets ESP32 with ESP-IDF through PlatformIO, uses FreeRTOS for task-based concurrency, and keeps all application code in C++17 without Arduino core dependencies.

## Build Environment

- PlatformIO environment: `env:esp32dev`
- Framework: ESP-IDF
- Language standard: C++17
- Main transport interfaces: CAN, SPI, UART

## Repository Layout

The current firmware repository is organized around seven runtime nodes / code areas:

1. `src/main.cpp`
Application entry point, task creation, watchdog refresh points, and inter-task queue wiring.

2. `src/VcuLogic.*`
Event-driven VCU state machine for `INIT`, `IDLE`, `READY`, `DRIVE`, `EMERGENCY_STOP`, and `FAULT`.

3. `lib/CanManager`
TWAI/CAN transport, motor driver communication, BMS frame parsing, and CAN-originated fault reporting.

4. `lib/RelayManager`
MCP23S17-based active-low relay control for the positive contactor bank.

5. `lib/DisplayHMI`
Nextion HMI UART transport and command handling.

6. `lib/Telemetry`
LoRa / telemetry packet formatting and UART uplink support.

7. `include/SystemConfig.h`
Shared pin map, message IDs, timing constants, LoRa mode defaults, and relay channel definitions.

## Current CAN Coverage

The active CAN message set is documented in [CAN_Message_Table.md](CAN_Message_Table.md).

Implemented frames:

- `0x100`: torque command
- `0x200`: motor status
- `0xE000`: Lithium Balance BMS config
- `0xE001`: Lithium Balance BMS live

## LoRa / E22 Baseline

The radio module is E22-400T30D-V2 (SX1268), a pin-compatible successor to
the retired E32-433T30D. Pin assignments are unchanged; the register-based
configuration protocol and config-mode pin levels are not (see
`include/E22Regs.h` for the address/value contract and
`lib/E22Config` for the pure command-build / response-parse helpers).

Startup mode for normal (transparent) operation:

- `M0 = 0`
- `M1 = 0`

Config mode (boot-time register sync against the contract):

- `M0 = 0`
- `M1 = 1`

Integration notes:

- Configure `LORA_M0_PIN` and `LORA_M1_PIN` before UART traffic starts.
- Use `LORA_AUX_PIN` as a readiness gate before telemetry transmission.
- Keep telemetry TX in transparent UART mode; radio register configuration
  happens once at boot in `vTask_LoRa_UKS` before entering the main loop.

## Relay Mapping Status

All ten relay channels are currently treated as positive contactor outputs. The software channel numbers are fixed, but the final harness-level physical assignments still need to be validated against the wiring package. Until that validation is complete, `allOn()` and `allOff()` should be treated as bank-wide operations on the positive contactor group, not as independently named vehicle loads.

**Bench'te yapılacaklar (HIL — kapsam dışı, native testlerle kanıtlanamaz):** G3 actuator geri-okuma doğrulaması (`RelayManager::verifyOutputs`, OLAT/IODIR readback + re-init/re-assert + actuator-fault) gerçek MCP23S17 ile bench'te doğrulanmalı: (1) çalışırken MCP23S17 gücünü kısa süre keserek brown-out reset tetikle, IODIR'in 0xFF'e döndüğünü ve VcuLogic'in FAULT'a geçtiğini osiloskopla/röle sesiyle teyit et; (2) MISO hattını fiziksel olarak ayırarak readback hatasında güvenli tarafa (kontaktör açık) düşüldüğünü doğrula.

## Contributors

Based on current git history, the repository contributors are:

- Sedat Ali Zevit - Seqat
- incubation-0
- Mesalt-f4
- Order
- Nisa Köken - NisaKoken

## Development Rules

Contributor workflow and naming rules are documented in [CONTRIBUTING.md](CONTRIBUTING.md).
