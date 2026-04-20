#include <cassert>
#include <vector>

#include "icss/protocol/frame_codec.hpp"

int main() {
    using namespace icss::protocol;

    const auto json_frame = encode_json_frame("engage_order", "kind=engage_order;target_id=target-alpha");
    const auto parsed_json = decode_json_frame(json_frame);
    assert(parsed_json.kind == "engage_order");
    assert(parsed_json.payload == "kind=engage_order;target_id=target-alpha");

    const auto binary_frame = encode_binary_frame("telemetry", "payload-body");
    const auto parsed_binary = decode_binary_frame(binary_frame);
    assert(parsed_binary.kind == "telemetry");
    assert(parsed_binary.payload == "payload-body");
    return 0;
}
