#include "bike/wfb_config.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>

namespace bike
{
namespace
{

using Kind = WfbConfigKind;

struct Known {
  std::string_view name;
  Kind kind;
  uint8_t wire;
  double scale, minimum, maximum;
};

// Documented WFB bike_config fields; package_disable is deliberately not editable.
constexpr Known known[] = {
    {"kp", Kind::Double, 7, 10, 0, 40},
    {"kp2", Kind::Double, 7, 100, 0, 3},
    {"mahony_kp", Kind::Double, 7, 100, 0, 3},
    {"kp_brake", Kind::Double, 7, 10, .2, 2},
    {"kp2_brake", Kind::Double, 7, 10, 0, 2},
    {"lift_resistance_kp2", Kind::Double, 7, 100, 0, 3},
    {"max_current", Kind::Int, 4, 1, 0, 150},
    {"max_current_brake", Kind::Int, 4, 1, -150, 0},
    {"ramp_alpha_pos", Kind::Double, 7, 10000, .0001, 1},
    {"ramp_alpha_neg", Kind::Double, 7, 10000, .0001, 1},
    {"ramp_alpha_return", Kind::Double, 7, 10000, .0001, 1},
    {"balance_pitch", Kind::Double, 7, 100, 5, 55},
    {"button_function", Kind::Enum, 0, 1, 0, 3},
    {"throttle_tilt_enabled", Kind::Bool, 0, 1, 0, 1},
    {"throttle_tilt_angle_limit", Kind::Double, 7, 100, -20, 20},
    {"brake_tilt_angle_limit", Kind::Double, 7, 100, -20, 20},
    {"persist_wheelie_throttle", Kind::Bool, 0, 1, 0, 1},
    {"persist_wheelie_brake", Kind::Bool, 0, 1, 0, 1},
    {"anti_loop_enabled", Kind::Bool, 0, 1, 0, 1},
    {"anti_loop_use_button_angle", Kind::Bool, 0, 1, 0, 1},
    {"anti_loop_angle", Kind::Double, 7, 100, 5, 55},
    {"horn_strength", Kind::Double, 7, 100, 0, 100},
    {"horn_frequency", Kind::Int, 3, 1, 200, 1000},
    {"speed_limit", Kind::Int, 1, 1, 0, 35},
};

const Known *known_field(std::string_view name) {
  const auto it = std::find_if(std::begin(known), std::end(known), [&](const auto &item) { return item.name == name; });
  return it == std::end(known) ? nullptr : it;
}

std::string_view trim(std::string_view value) {
  constexpr auto whitespace = " \r\n\t";
  const auto begin = value.find_first_not_of(whitespace);
  if (begin == value.npos)
    return {};
  return value.substr(begin, value.find_last_not_of(whitespace) - begin + 1);
}

bool parse_number(std::string_view text, double &value) {
  text = trim(text);
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value, std::chars_format::general);
  return !text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size() && std::isfinite(value);
}

bool parse_uint(std::string_view text, unsigned &value) {
  text = trim(text);
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return !text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size();
}

bool xml_character(uint32_t value) {
  return value == 9 || value == 10 || value == 13 || (value >= 32 && value <= 0xd7ff) ||
         (value >= 0xe000 && value <= 0xfffd) || (value >= 0x10000 && value <= 0x10ffff);
}

bool valid_utf8(std::string_view text) {
  for (size_t i = 0; i < text.size();) {
    auto first = uint8_t(text[i++]);
    unsigned count = first < 0x80                     ? 0
                     : first >= 0xc2 && first <= 0xdf ? 1
                     : first <= 0xef && first >= 0xe0 ? 2
                     : first >= 0xf0 && first <= 0xf4 ? 3
                                                      : 4;
    if (count == 4 || count > text.size() - i)
      return false;
    uint32_t code = count ? first & ((1u << (6 - count)) - 1) : first;
    for (unsigned j = 0; j < count; ++j) {
      const auto next = uint8_t(text[i++]);
      if ((next & 0xc0) != 0x80)
        return false;
      code = (code << 6) | (next & 0x3f);
    }
    if (!xml_character(code) || (count == 1 && code < 0x80) || (count == 2 && code < 0x800) ||
        (count == 3 && code < 0x10000))
      return false;
  }
  return true;
}

void append_utf8(std::string &out, uint32_t code) {
  if (code < 0x80)
    out += char(code);
  else if (code < 0x800) {
    out += char(0xc0 | (code >> 6));
    out += char(0x80 | (code & 0x3f));
  }
  else if (code < 0x10000) {
    out += char(0xe0 | (code >> 12));
    out += char(0x80 | ((code >> 6) & 0x3f));
    out += char(0x80 | (code & 0x3f));
  }
  else {
    out += char(0xf0 | (code >> 18));
    out += char(0x80 | ((code >> 12) & 0x3f));
    out += char(0x80 | ((code >> 6) & 0x3f));
    out += char(0x80 | (code & 0x3f));
  }
}

bool xml_text(std::string_view input, std::string *output) {
  if (input.find("]]>") != input.npos)
    return false;
  for (size_t i = 0; i < input.size();) {
    if (input[i] != '&') {
      if (output)
        *output += input[i];
      ++i;
      continue;
    }
    const auto end = input.find(';', i + 1);
    if (end == input.npos || end - i > 16)
      return false;
    auto entity = input.substr(i + 1, end - i - 1);
    uint32_t code = 0;
    if (entity == "amp")
      code = '&';
    else if (entity == "lt")
      code = '<';
    else if (entity == "gt")
      code = '>';
    else if (entity == "quot")
      code = '"';
    else if (entity == "apos")
      code = '\'';
    else if (entity.starts_with('#')) {
      entity.remove_prefix(1);
      const bool hex = entity.starts_with('x');
      if (hex)
        entity.remove_prefix(1);
      const auto number = std::from_chars(entity.data(), entity.data() + entity.size(), code, hex ? 16 : 10);
      if (entity.empty() || number.ec != std::errc{} || number.ptr != entity.data() + entity.size())
        return false;
    }
    else
      return false;
    if (!xml_character(code))
      return false;
    if (output)
      append_utf8(*output, code);
    i = end + 1;
  }
  return !output || output->size() <= 2048;
}

struct SchemaParser {
  struct Element {
    std::string_view name;
    std::string text;
    bool collect = false;
  };

  std::vector<WfbConfigField> &fields;
  std::vector<std::string> order;
  std::vector<Element> stack;
  WfbConfigField current;
  std::string config_name;
  bool root = false, closed = false, params = false, serialized = false, have_type = false;

  explicit SchemaParser(std::vector<WfbConfigField> &items) : fields(items) {
  }

  bool metadata(std::string_view name, const std::string &text) {
    unsigned integer = 0;
    if (name == "type") {
      if (have_type || !parse_uint(text, integer) || integer > 6)
        return false;
      current.kind = Kind(integer);
      have_type = true;
    }
    else if (name == "vTx") {
      if (!parse_uint(text, integer) || integer > 9)
        return false;
      current.wire_type = integer;
    }
    else if (name == "longName")
      current.label = text;
    else if (name == "suffix")
      current.units = text;
    else if (name == "enumNames") {
      if (current.enum_names.size() >= 32 || text.size() > 128)
        return false;
      current.enum_names.push_back(text);
    }
    else if (name == "minDouble" || name == "minInt")
      return parse_number(text, current.minimum);
    else if (name == "maxDouble" || name == "maxInt")
      return parse_number(text, current.maximum);
    else if (name == "stepDouble" || name == "stepInt")
      return parse_number(text, current.step);
    else if (name == "vTxDoubleScale")
      return parse_number(text, current.scale);
    else if (name == "valString" && current.name == "config_name")
      config_name = text;
    return current.label.size() <= 128 && current.units.size() <= 32;
  }

  bool open(std::string_view name) {
    if (stack.size() >= 16 || name.empty() || name.size() > 64)
      return false;
    if (stack.empty()) {
      if (root || closed || name != "ConfigParams")
        return false;
      root = true;
    }
    if (stack.size() == 1 && name == "Params") {
      if (params)
        return false;
      params = true;
    }
    if (stack.size() == 1 && name == "SerOrder") {
      if (serialized)
        return false;
      serialized = true;
    }
    if (stack.size() == 2 && stack[1].name == "Params") {
      if (fields.size() >= WfbConfig::max_fields ||
          std::any_of(fields.begin(), fields.end(), [&](const auto &field) { return field.name == name; }))
        return false;
      current = {};
      current.name = name;
      current.label = name;
      have_type = false;
    }
    const bool collect = (stack.size() == 3 && stack[1].name == "Params" && name != "description") ||
                         (stack.size() == 2 && stack[1].name == "SerOrder" && name == "ser");
    stack.push_back({name, {}, collect});
    return true;
  }

  bool close(std::string_view name) {
    if (stack.empty() || stack.back().name != name)
      return false;
    if (stack.size() == 4 && stack[1].name == "Params" && !metadata(name, stack.back().text))
      return false;
    if (stack.size() == 3 && stack[1].name == "Params") {
      if (!have_type)
        return false;
      fields.push_back(std::move(current));
    }
    if (stack.size() == 3 && stack[1].name == "SerOrder") {
      if (name != "ser" || order.size() >= WfbConfig::max_fields || stack.back().text.empty())
        return false;
      order.push_back(stack.back().text);
    }
    stack.pop_back();
    if (stack.empty())
      closed = true;
    return true;
  }

  bool parse(std::string_view xml) {
    if (xml.starts_with("\xef\xbb\xbf"))
      xml.remove_prefix(3);
    if (!valid_utf8(xml))
      return false;
    for (size_t at = 0; at < xml.size();) {
      if (xml[at] != '<') {
        const auto end = xml.find('<', at);
        const auto text = xml.substr(at, end == xml.npos ? xml.size() - at : end - at);
        if ((stack.empty() && !trim(text).empty()) ||
            !xml_text(text, !stack.empty() && stack.back().collect ? &stack.back().text : nullptr))
          return false;
        at += text.size();
        continue;
      }
      if (xml.substr(at).starts_with("<!--")) {
        const auto end = xml.find("-->", at + 4);
        if (end == xml.npos || xml.substr(at + 4, end - at - 4).find("--") != xml.npos)
          return false;
        at = end + 3;
        continue;
      }
      if (xml.substr(at).starts_with("<?xml ") && !root && at == 0) {
        const auto end = xml.find("?>", at + 6);
        if (end == xml.npos)
          return false;
        at = end + 2;
        continue;
      }
      const auto end = xml.find('>', at + 1);
      if (end == xml.npos)
        return false;
      auto name = trim(xml.substr(at + 1, end - at - 1));
      const bool closing = name.starts_with('/'), empty = name.ends_with('/');
      if (closing)
        name.remove_prefix(1);
      if (empty)
        name.remove_suffix(1);
      name = trim(name);
      // Qt-generated schema tags have no attributes. Reject declarations and
      // external entities rather than trying to implement an XML resolver.
      if (name.empty() ||
          name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_:-.") != name.npos ||
          (name[0] >= '0' && name[0] <= '9') || (closing && empty))
        return false;
      if (closing ? !close(name) : (!open(name) || (empty && !close(name))))
        return false;
      at = end + 1;
    }
    return stack.empty() && root && closed && params && serialized && !order.empty();
  }
};

uint32_t crc_update(uint32_t crc, std::string_view bytes) {
  for (const auto byte : bytes) {
    crc ^= uint8_t(byte);
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((crc & 1) ? 0x82f63b78u : 0);
  }
  return crc;
}

size_t fixed_size(const WfbConfigField &field) {
  if (field.kind == Kind::Undefined)
    return 0;
  if (field.kind == Kind::Bool || field.kind == Kind::Enum || field.kind == Kind::Bitfield)
    return 1;
  if (field.kind == Kind::String)
    return 0;
  if (field.kind == Kind::Int)
    return field.wire_type >= 1 && field.wire_type <= 6 ? size_t(1u << ((field.wire_type - 1) / 2)) : 0;
  return field.wire_type == 7 ? 2 : field.wire_type == 8 || field.wire_type == 9 ? 4 : 0;
}

uint32_t read_unsigned(std::span<const uint8_t> bytes) {
  uint32_t value = 0;
  for (auto byte : bytes)
    value = (value << 8) | byte;
  return value;
}

double read_number(const WfbConfigField &field, std::span<const uint8_t> bytes) {
  const auto raw = read_unsigned(bytes.subspan(field.offset, field.size));
  if (field.kind == Kind::Double && field.wire_type == 9)
    return std::bit_cast<float>(raw);
  const bool signed_value = field.kind == Kind::Double || (field.kind == Kind::Int && field.wire_type % 2 == 0);
  const double value = !signed_value     ? double(raw)
                       : field.size == 1 ? double(std::bit_cast<int8_t>(uint8_t(raw)))
                       : field.size == 2 ? double(std::bit_cast<int16_t>(uint16_t(raw)))
                                         : double(std::bit_cast<int32_t>(raw));
  return value / (field.kind == Kind::Double ? field.scale : 1);
}

} // namespace

void WfbConfig::clear() {
  fields_.clear();
  order_.clear();
  packet_size_ = 0;
  signature_ = 0;
  ready_ = schema_ready_ = false;
  error_ = "";
}

bool WfbConfig::fail(const char *reason, bool schema) {
  if (schema)
    clear();
  ready_ = false;
  error_ = reason;
  return false;
}

const WfbConfigField *WfbConfig::field(std::string_view name) const {
  const auto it = std::find_if(fields_.begin(), fields_.end(), [&](const auto &field) { return field.name == name; });
  return it == fields_.end() ? nullptr : &*it;
}

bool WfbConfig::parse_schema(std::string_view xml) {
  clear();
  if (xml.empty() || xml.size() > max_schema_bytes)
    return fail("Custom schema exceeds supported bounds", true);
  SchemaParser parser{fields_};
  if (!parser.parse(xml))
    return fail("Malformed or unsupported custom schema XML", true);
  for (const auto &name : parser.order) {
    auto it = std::find_if(fields_.begin(), fields_.end(), [&](const auto &field) { return field.name == name; });
    if (it == fields_.end() || it->serialized)
      return fail("Invalid custom schema serialization order", true);
    it->serialized = true;
    order_.push_back(size_t(it - fields_.begin()));
  }
  const auto identity = field("config_name");
  if (!identity || identity->kind != Kind::String ||
      (parser.config_name != "bike_config" && (!parser.config_name.empty() || !identity->serialized)))
    return fail("Custom schema is not WFB bike_config", true);
  constexpr std::string_view required[] = {"max_current",     "max_current_brake", "ramp_alpha_pos",
                                           "ramp_alpha_neg",  "speed_limit",       "balance_pitch",
                                           "button_function", "anti_loop_enabled", "anti_loop_use_button_angle",
                                           "anti_loop_angle"};
  for (const auto name : required) {
    const auto item = field(name);
    if (!item || !item->serialized)
      return fail("Custom schema is missing required WFB fields", true);
  }
  for (auto &item : fields_) {
    if (item.kind == Kind::Bool) {
      item.minimum = 0;
      item.maximum = 1;
      item.step = 1;
    }
    else if (item.kind == Kind::Enum) {
      if (item.enum_names.empty())
        return fail("Custom schema has an empty enumeration", true);
      item.minimum = 0;
      item.maximum = item.enum_names.size() - 1;
      item.step = 1;
    }
    if (!std::isfinite(item.scale) || item.scale <= 0 || item.minimum > item.maximum || item.step <= 0 ||
        (item.serialized && item.kind != Kind::String && item.kind != Kind::Undefined && !fixed_size(item)))
      return fail("Custom schema has an unsupported field encoding", true);
    if (const auto expected = known_field(item.name)) {
      if (item.kind != expected->kind || item.wire_type != expected->wire || item.scale != expected->scale ||
          item.minimum < expected->minimum || item.maximum > expected->maximum)
        return fail("WFB field type, scale or bounds do not match supported schema", true);
      if (item.name == "button_function" &&
          item.enum_names != std::vector<std::string>{"Wheelie", "Turbo", "Horn", "Disabled"})
        return fail("Unsupported WFB button actions", true);
      item.editable = item.serialized;
    }
  }
  uint32_t crc = 0xffffffffu;
  for (auto index : order_) {
    const auto &item = fields_[index];
    crc = crc_update(crc, item.name);
    const char type[] = {char('0' + uint8_t(item.kind)), char('0' + item.wire_type)};
    crc = crc_update(crc, {type, 2});
    for (const auto &name : item.enum_names)
      crc = crc_update(crc, name);
  }
  signature_ = ~crc;
  schema_ready_ = true;
  return true;
}

bool WfbConfig::decode(std::span<const uint8_t> reply) {
  if (!schema_ready_ || reply.size() < 6 || reply.size() > max_packet_bytes || reply[0] != 93 || reply[1] != 0 ||
      read_unsigned(reply.subspan(2, 4)) != signature_)
    return fail("Custom configuration signature or packet does not match schema");
  size_t at = 6;
  for (const auto index : order_) {
    auto &item = fields_[index];
    item.offset = at;
    item.size = fixed_size(item);
    if (item.kind == Kind::String) {
      const auto end = std::find(reply.begin() + at, reply.end(), 0);
      if (end == reply.end())
        return fail("Custom configuration has an unterminated string");
      item.size = size_t(end - reply.begin()) - at + 1;
      if (item.name == "config_name" &&
          std::string_view(reinterpret_cast<const char *>(reply.data() + at), item.size - 1) != "bike_config")
        return fail("Custom configuration is not bike_config");
    }
    if (item.size > reply.size() - at)
      return fail("Custom configuration is truncated");
    if (item.kind != Kind::Undefined && item.kind != Kind::String) {
      const auto value = read_number(item, reply);
      if (!std::isfinite(value) || (item.kind == Kind::Bool && value != 0 && value != 1) ||
          (item.kind == Kind::Enum && (value < 0 || value > item.maximum)) ||
          (item.editable && (value < item.minimum - 1e-8 || value > item.maximum + 1e-8)))
        return fail("Custom configuration contains an unsupported value");
    }
    at += item.size;
  }
  if (at != reply.size())
    return fail("Custom configuration has unexpected trailing data");
  std::copy(reply.begin(), reply.end(), original_.begin());
  packet_size_ = reply.size();
  ready_ = true;
  reset_draft();
  error_ = "";
  return true;
}

std::optional<double> WfbConfig::number(std::string_view name, bool draft) const {
  const auto item = field(name);
  if (!ready_ || !item || !item->serialized || item->kind == Kind::String || item->kind == Kind::Undefined)
    return {};
  return read_number(*item, std::span<const uint8_t>(draft ? draft_ : original_).first(packet_size_));
}

bool WfbConfig::set_number(std::string_view name, double value) {
  const auto item = field(name);
  if (!ready_ || !item || !item->editable || !std::isfinite(value) || value < item->minimum || value > item->maximum)
    return false;
  // stepDouble/stepInt describe editor increments, not a serialization grid.
  // Only quantize to the actual wire resolution, as VESC Tool does.
  const double scale = item->kind == Kind::Double ? item->scale : 1;
  const auto encoded = std::round(value * scale);
  const double quantized = encoded / scale;
  if (quantized < item->minimum || quantized > item->maximum)
    return false;
  const bool signed_value = item->kind == Kind::Double || (item->kind == Kind::Int && item->wire_type % 2 == 0);
  const auto bits = unsigned(item->size * 8);
  const auto minimum = signed_value ? -std::ldexp(1., bits - 1) : 0.;
  const auto maximum = std::ldexp(1., signed_value ? bits - 1 : bits) - 1;
  if (encoded < minimum || encoded > maximum)
    return false;
  auto raw = uint32_t(int64_t(encoded));
  for (size_t i = item->size; i > 0; --i) {
    draft_[item->offset + i - 1] = uint8_t(raw);
    raw >>= 8;
  }
  return true;
}

size_t WfbConfig::encode(std::span<uint8_t> output) const {
  if (!ready_ || output.size() < packet_size_)
    return 0;
  std::copy_n(draft_.begin(), packet_size_, output.begin());
  output[0] = 95;
  return packet_size_;
}

void WfbConfig::reset_draft() {
  std::copy_n(original_.begin(), packet_size_, draft_.begin());
}

bool WfbConfig::dirty() const {
  return ready_ && !std::equal(original_.begin(), original_.begin() + packet_size_, draft_.begin());
}

} // namespace bike
