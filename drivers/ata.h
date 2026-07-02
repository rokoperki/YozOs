#ifndef ATA_H
#define ATA_H

#include "../cpu/types.h"

#define ATA_DATA 0x1f0
#define ATA_ERROR 0x1f1
#define ATA_SECCOUNT 0x1f2
#define ATA_LBA_LO 0x1f3
#define ATA_LBA_MID 0x1f4
#define ATA_LBA_HI 0x1f5
#define ATA_DRIVE 0x1f6
#define ATA_CMD 0x1f7
#define ATA_STATUS 0x1f7
#define ATA_ALT_STATUS 0x3f6

#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_FLUSH 0xe7
#define ATA_CMD_IDENTIFY 0xec

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01
#define ATA_SR_DF 0x20

#define ATA_MASTER 0xa0

void ata_identify(void);
int ata_poll(void);
void ata_read(u32 lba, int sector_count, u16 *buff);
void ata_write(u32 lba, int sector_count, u16 *buff);

#endif
