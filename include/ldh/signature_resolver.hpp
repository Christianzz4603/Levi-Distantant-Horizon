#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace ldh {

enum class SignatureError {
    None,
    EmptyPattern,
    InvalidToken,
    PatternTooLong,
    NoMatch,
    AmbiguousMatch
};

struct SignaturePattern {
    std::vector<std::uint8_t> bytes;
    // A zero mask byte is a wildcard; a non-zero byte must match exactly.
    std::vector<std::uint8_t> mask;
};

struct SignatureParseResult {
    SignaturePattern pattern;
    SignatureError error{SignatureError::None};
    std::size_t error_offset{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == SignatureError::None;
    }
};

struct SignatureScanResult {
    SignatureError error{SignatureError::None};
    std::size_t offset{};
    // Match counting is intentionally capped at two: 2 means "two or more".
    std::size_t matches{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == SignatureError::None && matches == 1U;
    }
};

// Parses whitespace-separated bytes, e.g. "FD 7B BF A9 ?? ?? 01 91".
// Both '?' and '??' represent one full-byte wildcard.
[[nodiscard]] SignatureParseResult parse_signature(
    std::string_view text, std::size_t max_pattern_bytes = 256U);

// Scans only the byte span supplied by the caller. Pass an executable segment,
// not an entire process address space. A result is successful only if unique.
[[nodiscard]] SignatureScanResult scan_signature(
    std::span<const std::uint8_t> executable_bytes,
    const SignaturePattern& pattern) noexcept;

} // namespace ldh
