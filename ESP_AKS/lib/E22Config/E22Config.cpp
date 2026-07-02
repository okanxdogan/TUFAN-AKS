#include "E22Config.h"

size_t e22_buildReadAllCommand(uint8_t* outBuf, size_t outBufSize) {
  if (outBuf == nullptr || outBufSize < 3) return 0;
  outBuf[0] = E22_CMD_READ;
  outBuf[1] = E22_REG_BLOCK_START;
  outBuf[2] = E22_REG_BLOCK_LEN;
  return 3;
}

size_t e22_buildWriteCommand(const E22RegValues& regs, uint8_t* outBuf,
                              size_t outBufSize) {
  const size_t total = 3 + E22_REG_BLOCK_LEN;
  if (outBuf == nullptr || outBufSize < total) return 0;

  outBuf[0] = E22_CMD_WRITE;
  outBuf[1] = E22_REG_BLOCK_START;
  outBuf[2] = E22_REG_BLOCK_LEN;
  outBuf[3] = regs.addh;
  outBuf[4] = regs.addl;
  outBuf[5] = regs.netid;
  outBuf[6] = regs.reg0;
  outBuf[7] = regs.reg1;
  outBuf[8] = regs.reg2;
  outBuf[9] = regs.reg3;
  outBuf[10] = E22_CFG_CRYPT_H;
  outBuf[11] = E22_CFG_CRYPT_L;
  return total;
}

bool e22_isErrorResponse(const uint8_t* resp, size_t respLen) {
  if (resp == nullptr || respLen < 3) return false;
  return resp[0] == 0xFFU && resp[1] == 0xFFU && resp[2] == 0xFFU;
}

bool e22_parseRegResponse(const uint8_t* resp, size_t respLen,
                           E22RegValues& out) {
  const size_t expected = 3 + E22_REG_BLOCK_LEN;
  if (resp == nullptr || respLen < expected) return false;
  if (resp[0] != E22_CMD_READ) return false;
  if (resp[1] != E22_REG_BLOCK_START) return false;
  if (resp[2] != E22_REG_BLOCK_LEN) return false;

  out.addh  = resp[3];
  out.addl  = resp[4];
  out.netid = resp[5];
  out.reg0  = resp[6];
  out.reg1  = resp[7];
  out.reg2  = resp[8];
  out.reg3  = resp[9];
  // resp[10]/resp[11] = CRYPT_H/CRYPT_L — E22'de geri okunamaz, kasıtlı
  // olarak yok sayılır.
  return true;
}

bool e22_regsEqual(const E22RegValues& a, const E22RegValues& b) {
  return a.addh == b.addh && a.addl == b.addl && a.netid == b.netid &&
         a.reg0 == b.reg0 && a.reg1 == b.reg1 && a.reg2 == b.reg2 &&
         a.reg3 == b.reg3;
}
