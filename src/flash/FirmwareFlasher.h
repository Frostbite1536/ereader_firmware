#pragma once

#include <cstddef>
#include <cstdint>

// Flash a firmware image from an SD-card path into the inactive OTA app
// partition, then switch otadata so the stock X3/X4 bootloader boots it.
// Raw esp_partition_erase_range + esp_partition_write + ota_boot::switchTo —
// no Arduino Update class, no esp_image_verify (those reject the patched
// images the stock bootloader accepts on X4 silicon; docs/HARDWARE.md).
//
// Ported from CrossPoint Reader (github.com/crosspoint-reader/crosspoint-reader,
// src/network/FirmwareFlasher.{h,cpp}, MIT License, Copyright (c) 2025 Dave
// Allie), with storage routed through the FreeInk SDK's SDCardManager.

namespace firmware_flash {

enum class Result {
  OK,
  OPEN_FAIL,
  TOO_SMALL,
  TOO_LARGE,
  BAD_MAGIC,
  BAD_SEGMENTS,  // segment table malformed or runs past EOF
  BAD_CHECKSUM,  // ESP image XOR checksum mismatch
  BAD_SHA,       // SHA256 trailer mismatch (hash_appended images)
  BAD_SIZE,      // body+pad+sha length doesn't match file size
  NO_PARTITION,
  OOM,
  READ_FAIL,
  ERASE_FAIL,
  WRITE_FAIL,
  OTADATA_FAIL,
};

// Progress callback: called after every chunk write. Bytes written / total.
using ProgressCb = void (*)(size_t written, size_t total, void* ctx);

// Validate then stream `sdPath` into the next OTA partition and switch
// otadata. Caller is responsible for ESP.restart() afterwards. Pass
// alreadyValidated=true only when validateImageFile() just ran on this path.
Result flashFromSdPath(const char* sdPath, ProgressCb onProgress, void* ctx, bool alreadyValidated = false);

// Full-image integrity check mirroring the bootloader's verification: header
// magic, segment-table walk, XOR checksum, SHA256 trailer. `partitionSize` 0
// skips the size-fits check. The file is reopened by the flasher afterwards.
Result validateImageFile(const char* sdPath, size_t partitionSize);

const char* resultName(Result r);

}  // namespace firmware_flash
