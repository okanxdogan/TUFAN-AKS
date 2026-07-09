#pragma once
#include "SystemConfig.h"

// MotorTorque — motor sürücüsü torque komut yolunun SAF (donanımsız) kapısı.
//
// Motor sürücüsü henüz araca entegre DEĞİL (MOTOR_DRIVER_PRESENT=0). Bu bayrak
// 0 iken hiçbir torque CAN frame'i üretilmez; CanManager::sendTorqueCommand bu
// yardımcıyı gerçek gönderim kararının aynası olarak kullanır. Saf ve twai'siz
// olduğu için native testlerde link gerektirmeden doğrulanabilir (G2 iskeleti).
namespace MotorTorque {

// true → torque CAN frame'i ÜRETİLİR (yalnız MOTOR_DRIVER_PRESENT=1 iken).
inline bool frameEnabled() {
    return MOTOR_DRIVER_PRESENT != 0;
}

}  // namespace MotorTorque
