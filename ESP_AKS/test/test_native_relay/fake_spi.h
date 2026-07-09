#pragma once
// Native testler için SPI yakalayıcı + MCP23S17 register modeli.
// spi_device_transmit her çağrıda 3 byte alır: [opcode, register, value].
//   opcode 0x40 (R/W=0) → WRITE : regs[register] = value, ayrıca (reg,value)
//                                  çifti write-log'a eklenir.
//   opcode 0x41 (R/W=1) → READ  : rx_buffer[2] = regs[register].
// Böylece RelayManager'ın geri-okuma (verifyOutputs) yolu native'de test
// edilebilir; register'ı fake_spi_set_reg ile bozarak reset/brown-out senaryosu
// simüle edilir.
#include <cstddef>
#include <cstdint>

struct FakeSpiWrite {
    uint8_t reg;
    uint8_t value;
};

size_t       fake_spi_write_count(void);
FakeSpiWrite fake_spi_write_at(size_t i);
FakeSpiWrite fake_spi_last_write(void);
void         fake_spi_reset(void);  // yalnız write-log'u temizler (register modeli KALIR)

// --- MCP23S17 register modeli (geri-okuma testleri için) ---
void    fake_spi_set_reg(uint8_t reg, uint8_t value);  // reset/bozulma simülasyonu
uint8_t fake_spi_get_reg(uint8_t reg);
