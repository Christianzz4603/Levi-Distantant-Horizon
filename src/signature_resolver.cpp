#include "ldh/signature_resolver.hpp"

#include <cctype>

namespace ldh {
namespace {

int hex_value(char value) noexcept {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return 10 + value - 'a';
    if (value >= 'A' && value <= 'F') return 10 + value - 'A';
    return -1;
}

bool is_space(char value) noexcept {
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

} // namespace

SignatureParseResult parse_signature(std::string_view text,
                                     std::size_t max_pattern_bytes) {
    SignatureParseResult result{};
    if (max_pattern_bytes == 0U) {
        result.error = SignatureError::PatternTooLong;
        return result;
    }

    std::size_t cursor = 0U;
    while (cursor < text.size()) {
        while (cursor < text.size() && is_space(text[cursor])) ++cursor;
        if (cursor == text.size()) break;

        const std::size_t token_start = cursor;
        while (cursor < text.size() && !is_space(text[cursor])) ++cursor;
        const std::string_view token = text.substr(token_start, cursor - token_start);

        if (result.pattern.bytes.size() >= max_pattern_bytes) {
            result.error = SignatureError::PatternTooLong;
            result.error_offset = token_start;
            result.pattern = {};
            return result;
        }

        if (token == "?" || token == "??") {
            result.pattern.bytes.push_back(0U);
            result.pattern.mask.push_back(0U);
            continue;
        }

        if (token.size() != 2U) {
            result.error = SignatureError::InvalidToken;
            result.error_offset = token_start;
            result.pattern = {};
            return result;
        }

        const int high = hex_value(token[0]);
        const int low = hex_value(token[1]);
        if (high < 0 || low < 0) {
            result.error = SignatureError::InvalidToken;
            result.error_offset = token_start;
            result.pattern = {};
            return result;
        }

        result.pattern.bytes.push_back(
            static_cast<std::uint8_t>((high << 4) | low));
        result.pattern.mask.push_back(0xffU);
    }

    if (result.pattern.bytes.empty()) {
        result.error = SignatureError::EmptyPattern;
    }
    return result;
}

SignatureScanResult scan_signature(
    std::span<const std::uint8_t> executable_bytes,
    const SignaturePattern& pattern) noexcept {
    if (pattern.bytes.empty() || pattern.bytes.size() != pattern.mask.size()) {
        return {SignatureError::EmptyPattern, 0U, 0U};
    }
    if (pattern.bytes.size() > executable_bytes.size()) {
        return {SignatureError::NoMatch, 0U, 0U};
    }

    const std::size_t last_start =
        executable_bytes.size() - pattern.bytes.size();
    std::size_t found_offset = 0U;
    std::size_t matches = 0U;

    for (std::size_t offset = 0U; offset <= last_start; ++offset) {
        bool matched = true;
        for (std::size_t index = 0U; index < pattern.bytes.size(); ++index) {
            if (pattern.mask[index] != 0U &&
                executable_bytes[offset + index] != pattern.bytes[index]) {
                matched = false;
                break;
            }
        }

        if (!matched) continue;
        ++matches;
        if (matches == 1U) {
            found_offset = offset;
        } else {
            return {SignatureError::AmbiguousMatch, 0U, 2U};
        }
    }

    if (matches == 0U) {
        return {SignatureError::NoMatch, 0U, 0U};
    }
    return {SignatureError::None, found_offset, 1U};
}

} // namespace ldh
