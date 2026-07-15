#!/usr/bin/env python3
import pathlib
import struct
import sys


def die(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)


ATTR_DIRECTORY = 0x10
ATTR_ARCHIVE = 0x20
FAT_EOC = 0xFFFF


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


def read_u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def fat_entry(image, fat0, cluster):
    return read_u16(image, fat0 + cluster * 2)


def set_fat_entry(image, fat_start, sectors_per_fat, bytes_per_sector, num_fats, cluster, value):
    for fat_index in range(num_fats):
        off = (fat_start + fat_index * sectors_per_fat) * bytes_per_sector
        write_u16(image, off + cluster * 2, value)


def cluster_offset(data_start, sectors_per_cluster, bytes_per_sector, cluster):
    return (data_start + (cluster - 2) * sectors_per_cluster) * bytes_per_sector


def find_entry(image, start_off, size, name83):
    free_off = None

    for off in range(start_off, start_off + size, 32):
        first = image[off]
        if bytes(image[off : off + 11]) == name83:
            return off, free_off
        if free_off is None and first in (0x00, 0xE5):
            free_off = off

    return None, free_off


def find_free_clusters(image, fat0, total_fat_entries, count):
    clusters = []
    for cluster in range(2, total_fat_entries):
        if fat_entry(image, fat0, cluster) == 0:
            clusters.append(cluster)
            if len(clusters) == count:
                break
    return clusters


def free_chain(image, fat_start, sectors_per_fat, bytes_per_sector, num_fats, fat0, first_cluster):
    cluster = first_cluster
    while 2 <= cluster < 0xFFF8:
        next_cluster = fat_entry(image, fat0, cluster)
        set_fat_entry(image, fat_start, sectors_per_fat, bytes_per_sector, num_fats, cluster, 0)
        cluster = next_cluster


def write_dir_entry(image, entry_off, name83, attr, first_cluster, size):
    image[entry_off : entry_off + 32] = bytes(32)
    image[entry_off : entry_off + 11] = name83
    image[entry_off + 11] = attr
    write_u16(image, entry_off + 26, first_cluster)
    write_u32(image, entry_off + 28, size)


def create_root_child_dir(
    image,
    root_off,
    root_size,
    name83,
    fat_start,
    sectors_per_fat,
    bytes_per_sector,
    sectors_per_cluster,
    num_fats,
    fat0,
    data_start,
    total_fat_entries,
):
    entry_off, free_entry_off = find_entry(image, root_off, root_size, name83)

    if entry_off is not None:
        if image[entry_off + 11] & ATTR_DIRECTORY:
            return read_u16(image, entry_off + 26)
        die("target directory name exists as a file")

    if free_entry_off is None:
        die("no free root directory entry for target directory")

    clusters = find_free_clusters(image, fat0, total_fat_entries, 1)
    if len(clusters) != 1:
        die("not enough free clusters for target directory")

    cluster = clusters[0]
    set_fat_entry(image, fat_start, sectors_per_fat, bytes_per_sector, num_fats, cluster, FAT_EOC)

    directory_off = cluster_offset(data_start, sectors_per_cluster, bytes_per_sector, cluster)
    directory_size = bytes_per_sector * sectors_per_cluster
    image[directory_off : directory_off + directory_size] = bytes(directory_size)

    dot = b"." + b" " * 10
    dotdot = b".." + b" " * 9
    write_dir_entry(image, directory_off, dot, ATTR_DIRECTORY, cluster, 0)
    write_dir_entry(image, directory_off + 32, dotdot, ATTR_DIRECTORY, 0, 0)

    write_dir_entry(image, free_entry_off, name83, ATTR_DIRECTORY, cluster, 0)
    return cluster


def split_target_path(target):
    parts = target.split("/")
    if any(part == "" for part in parts):
        die("target path contains an empty component")

    if len(parts) == 1:
        return None, parts[0]

    if len(parts) == 2:
        return parts[0], parts[1]

    die("target path supports only one directory level")


def main():
    if len(sys.argv) != 4:
        die("usage: fat16_put.py <disk.img> <source.bin> <TARGET.BIN>")

    image_path = pathlib.Path(sys.argv[1])
    source_path = pathlib.Path(sys.argv[2])
    target_dir, target_leaf = split_target_path(sys.argv[3])
    target_name = name_to_83(target_leaf)
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

    if target_dir is None:
        directory_off = root_off
        directory_size = root_size
    else:
        dir_cluster = create_root_child_dir(
            image,
            root_off,
            root_size,
            name_to_83(target_dir),
            fat_start,
            sectors_per_fat,
            bytes_per_sector,
            sectors_per_cluster,
            num_fats,
            fat0,
            data_start,
            total_fat_entries,
        )
        directory_off = cluster_offset(data_start, sectors_per_cluster, bytes_per_sector, dir_cluster)
        directory_size = cluster_size

    entry_off, free_entry_off = find_entry(image, directory_off, directory_size, target_name)

    if entry_off is not None:
        if image[entry_off + 11] & ATTR_DIRECTORY:
            die("target path is a directory")
        old_cluster = read_u16(image, entry_off + 26)
        free_chain(image, fat_start, sectors_per_fat, bytes_per_sector, num_fats, fat0, old_cluster)
    else:
        entry_off = free_entry_off

    if entry_off is None:
        die("no free directory entry")

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
        set_fat_entry(image, fat_start, sectors_per_fat, bytes_per_sector, num_fats, cluster, next_cluster)

    padded = data + bytes(cluster_size * clusters_needed - len(data))
    for i, cluster in enumerate(free_clusters):
        off = (data_start + (cluster - 2) * sectors_per_cluster) * bytes_per_sector
        chunk = padded[i * cluster_size : (i + 1) * cluster_size]
        image[off : off + cluster_size] = chunk

    write_dir_entry(image, entry_off, target_name, ATTR_ARCHIVE, free_clusters[0], len(data))

    image_path.write_bytes(image)
    print(f"wrote {sys.argv[3].upper()} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
