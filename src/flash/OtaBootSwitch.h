#pragma once

#include <esp_partition.h>

#include <cstddef>
#include <cstdint>

// Switch the bootloader's selected app partition by writing otadata directly,
// bypassing esp_ota_set_boot_partition()'s esp_image_verify call — which
// rejects the patched images the stock X3/X4 bootloader happily boots.
//
// Ported from CrossPoint Reader (github.com/crosspoint-reader/crosspoint-reader,
// src/network/OtaBootSwitch.{h,cpp}, MIT License, Copyright (c) 2025 Dave Allie).
// Layout reference: esp_flash_partitions.h. CRC covers ota_seq (4 bytes) only.

namespace ota_boot {

struct __attribute__((packed)) SelectEntry {
  uint32_t ota_seq;
  uint8_t seq_label[20];
  uint32_t ota_state;
  uint32_t crc;
};
static_assert(sizeof(SelectEntry) == 32, "SelectEntry must be 32 bytes");

constexpr uint32_t kOtaImgNew = 0;      // ESP_OTA_IMG_NEW
constexpr uint32_t kOtaImgInvalid = 3;  // ESP_OTA_IMG_INVALID
constexpr uint32_t kOtaImgAborted = 4;  // ESP_OTA_IMG_ABORTED
constexpr size_t kOtaSeqCrcLen = 4;

// CRC32-LE over the 4-byte ota_seq, init UINT32_MAX. Matches IDF.
uint32_t computeSeqCrc(uint32_t seq);

// Point the bootloader at `dest` by writing a fresh otadata entry into the
// inactive otadata slot. The bytes in `dest` must already be a valid app
// image — the caller is responsible for validating before calling this.
bool switchTo(const esp_partition_t* dest);

}  // namespace ota_boot
