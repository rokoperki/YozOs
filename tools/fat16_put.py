#!/usr/bin/env python3
import pathlib
import struct
import sys


def die(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


def name_to_83(name):
    if "." in name:
        base, ext = name.split(".", 1)
    else:
        base, ext = name, ""

    if not base or len(base) > 8 or len(ext) > 3:
        die("target name must be 8.3")

    valid = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"
    base = base.upper()
    ext = ext.upper()
    if any(c not in valid for c in base + ext):
        die("target name contains unsupported characters")

    return (base.ljust(8) + ext.ljust(3)).encode("ascii")


def read_u16(buf, off):
    return struct.unpack_from("<H", buf, off)[0]


def write_u16(buf, off, value):
    struct.pack_into("<H", buf, off, value)


def write_u32(buf, off, value):
    struct.pack_into("<I", buf, off, value)


def main():
    if len(sys.argv) != 4:
        die("usage: fat16_put.py <disk.img> <source.bin> <TARGET.BIN>")

    image_path = pathlib.Path(sys.argv[1])
    source_path = pathlib.Path(sys.argv[2])
    target_name = name_to_83(sys.argv[3])
    data = source_path.read_bytes()

    image = bytearray(image_path.read_bytes())
    bytes_per_sector = read_u16(image, 0x0B)
    sectors_per_cluster = image[0x0D]
    reserved_sectors = read_u16(image, 0x0E)
    num_fats = image[0x10]
    root_entries = read_u16(image, 0x11)
    sectors_per_fat = read_u16(image, 0x16)

    if bytes_per_sector != 512:
        die("only 512-byte sectors are supported")
    if sectors_per_cluster == 0 or num_fats == 0 or sectors_per_fat == 0:
        die("invalid FAT16 BPB")

    fat_start = reserved_sectors
    root_start = fat_start + num_fats * sectors_per_fat
    root_sectors = (root_entries * 32 + bytes_per_sector - 1) // bytes_per_sector
    data_start = root_start + root_sectors
    cluster_size = bytes_per_sector * sectors_per_cluster
    clusters_needed = max(1, (len(data) + cluster_size - 1) // cluster_size)

    fat0 = fat_start * bytes_per_sector
    total_fat_entries = sectors_per_fat * bytes_per_sector // 2

    root_off = root_start * bytes_per_sector
    root_size = root_sectors * bytes_per_sector
    entry_off = None
    free_entry_off = None

    for off in range(root_off, root_off + root_size, 32):
        first = image[off]
        if bytes(image[off : off + 11]) == target_name:
            entry_off = off
            break
        if free_entry_off is None and first in (0x00, 0xE5):
            free_entry_off = off

    if entry_off is not None:
        old_cluster = read_u16(image, entry_off + 26)
        while 2 <= old_cluster < 0xFFF8:
            next_cluster = read_u16(image, fat0 + old_cluster * 2)
            for fat_index in range(num_fats):
                off = (fat_start + fat_index * sectors_per_fat) * bytes_per_sector
                write_u16(image, off + old_cluster * 2, 0)
            old_cluster = next_cluster
    else:
        entry_off = free_entry_off

    if entry_off is None:
        die("no free root directory entry")

    free_clusters = []
    for cluster in range(2, total_fat_entries):
        if read_u16(image, fat0 + cluster * 2) == 0:
            free_clusters.append(cluster)
            if len(free_clusters) == clusters_needed:
                break

    if len(free_clusters) != clusters_needed:
        die("not enough free clusters")

    for i, cluster in enumerate(free_clusters):
        next_cluster = 0xFFFF if i + 1 == len(free_clusters) else free_clusters[i + 1]
        for fat_index in range(num_fats):
            off = (fat_start + fat_index * sectors_per_fat) * bytes_per_sector
            write_u16(image, off + cluster * 2, next_cluster)

    padded = data + bytes(cluster_size * clusters_needed - len(data))
    for i, cluster in enumerate(free_clusters):
        off = (data_start + (cluster - 2) * sectors_per_cluster) * bytes_per_sector
        chunk = padded[i * cluster_size : (i + 1) * cluster_size]
        image[off : off + cluster_size] = chunk

    image[entry_off : entry_off + 32] = bytes(32)
    image[entry_off : entry_off + 11] = target_name
    image[entry_off + 11] = 0x20
    write_u16(image, entry_off + 26, free_clusters[0])
    write_u32(image, entry_off + 28, len(data))

    image_path.write_bytes(image)
    print(f"wrote {sys.argv[3].upper()} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
