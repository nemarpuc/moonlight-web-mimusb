// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 the moonlight-web-mimusb contributors
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace mwi {

// SHA-256 (FIPS 180-4). Small and dependency-free; the installer only
// compares files, so speed is not the point.
class Sha256 {
public:
    Sha256();
    void update(const void* data, std::size_t size);
    // The digest as 64 lowercase hex digits; the object is spent afterwards.
    [[nodiscard]] std::string hex();

private:
    void block(const std::uint8_t* chunk);

    std::array<std::uint32_t, 8> h_;
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffered_ = 0;
    std::uint64_t total_ = 0;
};

// The hex digest of a file, or an empty string with `error` set.
[[nodiscard]] std::string sha256_file(const std::filesystem::path& path, std::string& error);

} // namespace mwi
