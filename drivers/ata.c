#include "ata.h"
#include "../cpu/types.h"
#include "low_level.h"
#include "screen.h"

void ata_400ns() {
  for (int i = 0; i < 4; i++) {
    port_byte_in(ATA_ALT_STATUS);
  }
}

int ata_poll() {
  while (port_byte_in(ATA_ALT_STATUS) & ATA_SR_BSY)
    ;
  if ((port_byte_in(ATA_ALT_STATUS) & ATA_SR_ERR) ||
      (port_byte_in(ATA_ALT_STATUS) & ATA_SR_DF))
    return 1;

  int drq_status = 0;
  while (drq_status == 0) {
    drq_status = port_byte_in(ATA_ALT_STATUS) & ATA_SR_DRQ;
  }
  return 0;
}

void ata_identify() {
  port_byte_out(ATA_DRIVE, ATA_MASTER);
  port_byte_out(ATA_SECCOUNT, 0x0);
  port_byte_out(ATA_LBA_LO, 0x0);
  port_byte_out(ATA_LBA_MID, 0x0);
  port_byte_out(ATA_LBA_HI, 0x0);

  port_byte_out(ATA_CMD, ATA_CMD_IDENTIFY);

  if (port_byte_in(ATA_STATUS) == 0x00)
    return;

  if (port_byte_in(ATA_LBA_MID) != 0x0 || port_byte_in(ATA_LBA_HI) != 0x0)
    return;

  ata_poll();
  ata_400ns();

  u16 buff[256];
  for (int i = 0; i < 256; i++) {
    buff[i] = port_word_in(ATA_DATA);
  }

  char model[41];
  for (int i = 0; i < 20; i++) {
    model[i * 2] = buff[27 + i] >> 8;       // high byte = first char
    model[i * 2 + 1] = buff[27 + i] & 0xFF; // low byte  = second char
  }
  model[40] = '\0';
  println(model);
}
