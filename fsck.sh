#!/bin/sh
# Verify disk.img's FAT16 filesystem with the host fsck_msdos.
# fsck_msdos won't take a plain file path, so attach the image as a raw
# device (no mount), fsck the raw node, then detach.
set -e
IMG="${1:-disk.img}"
DEV=$(hdiutil attach -nomount -imagekey diskimage-class=CRawDiskImage "$IMG" | awk '{print $1; exit}')
RAW=$(echo "$DEV" | sed 's#/dev/disk#/dev/rdisk#')
fsck_msdos -n "$RAW"; RC=$?
hdiutil detach "$DEV" >/dev/null
exit $RC
