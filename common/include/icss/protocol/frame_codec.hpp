#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace icss::protocol {

enum class FrameFormat : std::uint8_t {
    JsonLine,
    Binary,
};

struct FramedMessage {
    std::string kind;
    std::string payload;
};

std::string encode_json_frame(std::string_view kind, std::string_view payload);
FramedMessage decode_json_frame(std::string_view frame);

std::vector<std::uint8_t> encode_binary_frame(std::string_view kind, std::string_view payload);
FramedMessage decode_binary_frame(const std::vector<std::uint8_t>& frame);
bool try_decode_binary_frame(std::string& buffer, FramedMessage& out);

}  // namespace icss::protocol
