#!/usr/bin/env python3
import struct
import sys


def die(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


def write_u16(buf, off, value):
    struct.pack_into("<H", buf, off, value)


def write_u32(buf, off, value):
    struct.pack_into("<I", buf, off, value)


def main():
    if len(sys.argv) not in (2, 3):
        die("usage: fat16_mkfs.py <disk.img> [sectors]")

    path = sys.argv[1]
    total_sectors = int(sys.argv[2]) if len(sys.argv) == 3 else 2048

    if total_sectors <= 0 or total_sectors > 0xFFFF:
        die("sector count must fit FAT16 BPB total_sectors")

    bytes_per_sector = 512
    sectors_per_cluster = 2
    reserved_sectors = 1
    num_fats = 2
    root_entries = 512
    sectors_per_fat = 8
    root_sectors = (root_entries * 32 + bytes_per_sector - 1) // bytes_per_sector

    if reserved_sectors + num_fats * sectors_per_fat + root_sectors >= total_sectors:
        die("disk too small")

    image = bytearray(total_sectors * bytes_per_sector)

    bpb = image
    bpb[0:3] = b"\xEB\x3C\x90"
    bpb[3:11] = b"YOZOS   "
    write_u16(bpb, 0x0B, bytes_per_sector)
    bpb[0x0D] = sectors_per_cluster
    write_u16(bpb, 0x0E, reserved_sectors)
    bpb[0x10] = num_fats
    write_u16(bpb, 0x11, root_entries)
    write_u16(bpb, 0x13, total_sectors)
    bpb[0x15] = 0xF8
    write_u16(bpb, 0x16, sectors_per_fat)
    write_u16(bpb, 0x18, 1)
    write_u16(bpb, 0x1A, 1)
    write_u32(bpb, 0x1C, 0)
    write_u32(bpb, 0x20, 0)
    bpb[0x24] = 0x80
    bpb[0x26] = 0x29
    write_u32(bpb, 0x27, 0x595A4F53)
    bpb[0x2B:0x36] = b"YOZOS DISK "
    bpb[0x36:0x3E] = b"FAT16   "
    bpb[510] = 0x55
    bpb[511] = 0xAA

    fat_start = reserved_sectors * bytes_per_sector
    for fat_index in range(num_fats):
        off = fat_start + fat_index * sectors_per_fat * bytes_per_sector
        write_u16(image, off, 0xFFF8)
        write_u16(image, off + 2, 0xFFFF)

    with open(path, "wb") as f:
        f.write(image)

    print(f"formatted {path} as FAT16 ({total_sectors} sectors)")


if __name__ == "__main__":
    main()
