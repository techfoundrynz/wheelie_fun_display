#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bike
{

// Numeric values match VESC Tool's ConfigParams CFG_T enum.
enum class WfbConfigKind : uint8_t {
  Undefined = 0,
  Double = 1,
  Int = 2,
  String = 3,
  Enum = 4,
  Bool = 5,
  Bitfield = 6
};

struct WfbConfigField {
  std::string name, label, units;
  WfbConfigKind kind = WfbConfigKind::Undefined;
  uint8_t wire_type = 0;
  double minimum = 0, maximum = 0, step = 1, scale = 1;
  std::vector<std::string> enum_names;
  bool editable = false, serialized = false;
  size_t offset = 0, size = 0;
};

// A bounded schema-driven codec. No magic signature or field order is guessed.
// The original packet is retained, and edits replace only one field's bytes.
class WfbConfig {
public:

  static constexpr size_t max_schema_bytes = 128 * 1024;
  static constexpr size_t max_fields = 64, max_packet_bytes = 1024;

  bool parse_schema(std::string_view xml);

  bool decode(std::span<const uint8_t> reply);

  size_t encode(std::span<uint8_t> output) const;

  bool set_number(std::string_view name, double value);

  std::optional<double> number(std::string_view name, bool draft = true) const;

  const WfbConfigField *field(std::string_view name) const;

  const std::vector<WfbConfigField> &fields() const {
    return fields_;
  }

  bool schema_ready() const {
    return schema_ready_;
  }

  bool ready() const {
    return ready_;
  }

  bool dirty() const;

  uint32_t signature() const {
    return signature_;
  }

  std::string_view error() const {
    return error_;
  }

  void reset_draft();

  void clear();

private:

  bool fail(const char *reason, bool schema = false);

  std::vector<WfbConfigField> fields_;
  std::vector<size_t> order_;
  std::array<uint8_t, max_packet_bytes> original_{}, draft_{};
  size_t packet_size_ = 0;
  uint32_t signature_ = 0;
  bool schema_ready_ = false, ready_ = false;
  const char *error_ = "";
};

} // namespace bike
