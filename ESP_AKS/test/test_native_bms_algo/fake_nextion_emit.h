#pragma once
//
// buildBmsNextionCommands() çağrılarında üretilen komut metinlerini yakalayan
// basit test double'ı. BmsNextionEmit imzasıyla (void(const char*, size_t,
// void*)) birebir uyumlu, sabit boyutlu dizide saklar.
//
#include <cstddef>

void fake_nextion_reset();
void fake_nextion_capture(const char* cmd, size_t len, void* ctx);
size_t fake_nextion_command_count();
const char* fake_nextion_command_at(size_t index);

// Verilen önekle (ör. "j0.val=") başlayan İLK yakalanmış komutu döner;
// bulunamazsa nullptr.
const char* fake_nextion_find(const char* prefix);
