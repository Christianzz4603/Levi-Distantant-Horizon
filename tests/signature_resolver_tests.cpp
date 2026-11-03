#include "ldh/signature_resolver.hpp"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    using ldh::SignatureError;

    const auto parsed = ldh::parse_signature("FD 7b BF A9 ?? ? 01 91");
    assert(parsed);
    assert(parsed.pattern.bytes.size() == 8U);
    assert(parsed.pattern.mask.size() == 8U);
    assert(parsed.pattern.bytes[0] == 0xfdU);
    assert(parsed.pattern.bytes[1] == 0x7bU);
    assert(parsed.pattern.mask[4] == 0U);
    assert(parsed.pattern.mask[5] == 0U);
    assert(parsed.pattern.mask[6] == 0xffU);

    const std::array<std::uint8_t, 12> image{
        0x00U, 0xfdU, 0x7bU, 0xbfU, 0xa9U, 0x11U,
        0x22U, 0x01U, 0x91U, 0x00U, 0x00U, 0x00U
    };
    const auto unique = ldh::scan_signature(image, parsed.pattern);
    assert(unique);
    assert(unique.offset == 1U);
    assert(unique.matches == 1U);

    const std::array<std::uint8_t, 4> no_match{0x00U, 0x01U, 0x02U, 0x03U};
    const auto missing = ldh::scan_signature(no_match, parsed.pattern);
    assert(missing.error == SignatureError::NoMatch);
    assert(missing.matches == 0U);

    const auto short_pattern = ldh::parse_signature("AA BB");
    assert(short_pattern);
    const std::array<std::uint8_t, 6> repeated{
        0xaaU, 0xbbU, 0x00U, 0xaaU, 0xbbU, 0x00U
    };
    const auto ambiguous = ldh::scan_signature(repeated, short_pattern.pattern);
    assert(ambiguous.error == SignatureError::AmbiguousMatch);
    assert(ambiguous.matches == 2U);

    const auto malformed = ldh::parse_signature("AA GG");
    assert(malformed.error == SignatureError::InvalidToken);
    assert(malformed.error_offset == 3U);

    const auto empty = ldh::parse_signature("   \t ");
    assert(empty.error == SignatureError::EmptyPattern);

    const auto too_long = ldh::parse_signature("AA BB CC", 2U);
    assert(too_long.error == SignatureError::PatternTooLong);

    const ldh::SignaturePattern invalid_shape{{0xaaU, 0xbbU}, {0xffU}};
    const auto invalid = ldh::scan_signature(repeated, invalid_shape);
    assert(invalid.error == SignatureError::EmptyPattern);

    return 0;
}
