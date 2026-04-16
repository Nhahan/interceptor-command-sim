#include "icss/protocol/frame_codec.hpp"

#include <stdexcept>
#include <string>

namespace icss::protocol {
namespace {

std::string escape_json(std::string_view input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string unescape_json(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    bool escaped = false;
    for (char ch : input) {
        if (!escaped) {
            if (ch == '\\') {
                escaped = true;
            } else {
                out += ch;
            }
            continue;
        }
        switch (ch) {
        case '\\': out += '\\'; break;
        case '"': out += '"'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        default: throw std::runtime_error("unsupported json escape");
        }
        escaped = false;
    }
    if (escaped) throw std::runtime_error("dangling json escape");
    return out;
}

std::string extract_json_field(std::string_view frame, std::string_view key) {
    const std::string needle = std::string{"\""} + std::string{key} + "\":\"";
    const auto start = frame.find(needle);
    if (start == std::string_view::npos) throw std::runtime_error("missing json key");
    std::size_t pos = start + needle.size();
    std::string raw;
    bool escaped = false;
    while (pos < frame.size()) {
        const char ch = frame[pos++];
        if (escaped) {
            raw += '\\';
            raw += ch;
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') break;
        raw += ch;
    }
    return unescape_json(raw);
}

void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
    out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
}

std::uint32_t read_u32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    if (offset + 4 > data.size()) throw std::runtime_error("binary frame too short");
    return (static_cast<std::uint32_t>(data[offset]) << 24U)
         | (static_cast<std::uint32_t>(data[offset + 1]) << 16U)
         | (static_cast<std::uint32_t>(data[offset + 2]) << 8U)
         | static_cast<std::uint32_t>(data[offset + 3]);
}

}  // namespace

std::string encode_json_frame(std::string_view kind, std::string_view payload) {
    return std::string{"{\"kind\":\""} + escape_json(kind) + "\",\"payload\":\"" + escape_json(payload) + "\"}";
}

FramedMessage decode_json_frame(std::string_view frame) {
    return {extract_json_field(frame, "kind"), extract_json_field(frame, "payload")};
}

std::vector<std::uint8_t> encode_binary_frame(std::string_view kind, std::string_view payload) {
    std::vector<std::uint8_t> out;
    out.reserve(8 + kind.size() + payload.size());
    append_u32(out, static_cast<std::uint32_t>(kind.size()));
    out.insert(out.end(), kind.begin(), kind.end());
    append_u32(out, static_cast<std::uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

FramedMessage decode_binary_frame(const std::vector<std::uint8_t>& frame) {
    const auto kind_size = read_u32(frame, 0);
    const auto payload_size_offset = 4U + kind_size;
    const auto payload_size = read_u32(frame, payload_size_offset);
    const auto kind_begin = frame.begin() + static_cast<std::ptrdiff_t>(4U);
    const auto kind_end = kind_begin + static_cast<std::ptrdiff_t>(kind_size);
    const auto payload_begin = frame.begin() + static_cast<std::ptrdiff_t>(payload_size_offset + 4U);
    const auto payload_end = payload_begin + static_cast<std::ptrdiff_t>(payload_size);
    if (payload_end > frame.end()) throw std::runtime_error("binary frame truncated");
    return {std::string(kind_begin, kind_end), std::string(payload_begin, payload_end)};
}

bool try_decode_binary_frame(std::string& buffer, FramedMessage& out) {
    if (buffer.size() < 8) {
        return false;
    }
    const std::vector<std::uint8_t> bytes(buffer.begin(), buffer.end());
    const auto kind_size = read_u32(bytes, 0);
    const auto payload_size_offset = 4U + kind_size;
    if (payload_size_offset + 4U > bytes.size()) {
        return false;
    }
    const auto payload_size = read_u32(bytes, payload_size_offset);
    const auto frame_size = payload_size_offset + 4U + payload_size;
    if (frame_size > bytes.size()) {
        return false;
    }
    std::vector<std::uint8_t> frame(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(frame_size));
    out = decode_binary_frame(frame);
    buffer.erase(0, frame_size);
    return true;
}

}  // namespace icss::protocol
