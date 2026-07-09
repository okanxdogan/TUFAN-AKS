#include "fake_spi.h"

#include <cstdint>
#include <vector>

#include "driver/spi_master.h"
#include "esp_err.h"

namespace {
std::vector<FakeSpiWrite> s_writes;
uint8_t s_regs[256] = {0};  // MCP23S17 register modeli
int s_dummy_device = 1;     // non-null sentinel for spi_device_handle_t
}  // namespace

extern "C" {

esp_err_t spi_bus_initialize(spi_host_device_t /*host*/,
                             const spi_bus_config_t* /*cfg*/,
                             int /*dma*/) {
    return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t /*host*/,
                             const spi_device_interface_config_t* /*cfg*/,
                             spi_device_handle_t* handle) {
    if (handle != nullptr)
        *handle = &s_dummy_device;
    return ESP_OK;
}

esp_err_t spi_device_transmit(spi_device_handle_t /*h*/,
                              spi_transaction_t* trans) {
    if (trans == nullptr || trans->tx_buffer == nullptr)
        return ESP_OK;

    const uint8_t* p = static_cast<const uint8_t*>(trans->tx_buffer);
    const uint8_t opcode = p[0];
    const uint8_t reg = p[1];

    if (opcode & 0x01) {
        // READ (0x41): register modelinden değeri MISO 3. byte'a koy.
        if (trans->rx_buffer != nullptr) {
            uint8_t* rx = static_cast<uint8_t*>(trans->rx_buffer);
            rx[2] = s_regs[reg];
        }
    } else {
        // WRITE (0x40): modeli güncelle + write-log'a ekle.
        s_regs[reg] = p[2];
        s_writes.push_back(FakeSpiWrite{reg, p[2]});
    }
    return ESP_OK;
}

const char* esp_err_to_name(esp_err_t /*err*/) {
    return "FAKE_ERR";
}

}  // extern "C"

size_t fake_spi_write_count(void) {
    return s_writes.size();
}

FakeSpiWrite fake_spi_write_at(size_t i) {
    if (i >= s_writes.size())
        return {0, 0};
    return s_writes[i];
}

FakeSpiWrite fake_spi_last_write(void) {
    if (s_writes.empty())
        return {0, 0};
    return s_writes.back();
}

void fake_spi_reset(void) {
    s_writes.clear();
    // NOT: register modeli (s_regs) BİLEREK korunur — böylece begin()'in
    // yazdığı OLAT/IODIR değerleri sonraki verifyOutputs geri-okumasında
    // görülebilir. Register bozma testleri fake_spi_set_reg kullanır.
}

void fake_spi_set_reg(uint8_t reg, uint8_t value) {
    s_regs[reg] = value;
}

uint8_t fake_spi_get_reg(uint8_t reg) {
    return s_regs[reg];
}
