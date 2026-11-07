#include "bike/tuning.hpp"
#include "bike/wfb_config.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      std::cerr << __LINE__ << ": " #c "\n";                                                                           \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (0)

using Bytes = std::vector<uint8_t>;
using Config = bike::WfbConfig;

namespace
{

struct Spec {
  std::string name;
  unsigned kind, wire;
  double scale, minimum, maximum, step;
  std::string enums;
};

const std::vector<Spec> specs = {
    {"config_name", 3, 0, 1, 0, 0, 1, ""},
    {"max_current", 2, 4, 1, 0, 150, 1, ""},
    {"max_current_brake", 2, 4, 1, -150, 0, 1, ""},
    {"ramp_alpha_pos", 1, 7, 10000, .0001, 1, .0001, ""},
    {"ramp_alpha_neg", 1, 7, 10000, .0001, 1, .0001, ""},
    {"speed_limit", 2, 1, 1, 0, 35, 1, ""},
    {"balance_pitch", 1, 7, 100, 5, 55, .5, ""},
    {"button_function", 4, 0, 1, 0, 3, 1,
     "<enumNames>Wheelie</enumNames><enumNames>Turbo</enumNames><enumNames>Horn</enumNames><enumNames>Disabled</"
     "enumNames>"},
    {"anti_loop_enabled", 5, 0, 1, 0, 1, 1, ""},
    {"anti_loop_use_button_angle", 5, 0, 1, 0, 1, 1, ""},
    {"anti_loop_angle", 1, 7, 100, 5, 55, .5, ""},
    {"horn_strength", 1, 7, 100, 0, 100, .1, ""},
    {"horn_frequency", 2, 3, 1, 200, 1000, 1, ""},
    {"ramp_alpha_return", 1, 7, 10000, .0001, 1, .0001, ""},
    {"persist_wheelie_throttle", 5, 0, 1, 0, 1, 1, ""},
    {"future_blob", 3, 0, 1, 0, 0, 1, ""},
    {"future_limit", 2, 5, 1, 0, 4294967295., 1, ""},
};

std::string schema(std::vector<Spec> fields = specs, std::vector<std::string> order = {}) {
  std::string result = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><ConfigParams><Params>";
  result += "<hw_name><longName>WFB Cfg</longName><type>0</type></hw_name>";
  result += "<package_disable><type>5</type></package_disable>";
  for (const auto &field : fields) {
    result +=
        "<" + field.name + "><longName>" + field.name + "</longName><type>" + std::to_string(field.kind) + "</type>";
    result += "<vTx>" + std::to_string(field.wire) + "</vTx><vTxDoubleScale>" + std::to_string(field.scale) +
              "</vTxDoubleScale>";
    result += "<minDouble>" + std::to_string(field.minimum) + "</minDouble><maxDouble>" +
              std::to_string(field.maximum) + "</maxDouble>";
    result += "<stepDouble>" + std::to_string(field.step) + "</stepDouble>" + field.enums;
    if (field.name == "config_name")
      result += "<valString>bike_config</valString>";
    result += "</" + field.name + ">";
  }
  result += "</Params><SerOrder>";
  if (order.empty()) {
    for (const auto &field : fields)
      order.push_back(field.name);
  }
  for (const auto &name : order)
    result += "<ser>" + name + "</ser>";
  return result + "</SerOrder><Grouping><group><groupName>Bike</groupName></group></Grouping></ConfigParams>";
}

void append(Bytes &bytes, uint32_t value, size_t count) {
  for (size_t i = count; i > 0; --i)
    bytes.push_back(uint8_t(value >> ((i - 1) * 8)));
}

Bytes packet(uint32_t signature = 0x329c07da) {
  Bytes bytes{93, 0};
  append(bytes, signature, 4);
  constexpr char name[] = "bike_config";
  bytes.insert(bytes.end(), name, name + sizeof(name));
  append(bytes, 150, 2);
  append(bytes, uint16_t(-90), 2);
  append(bytes, 30, 2);
  append(bytes, 80, 2);
  append(bytes, 19, 1);
  append(bytes, 3000, 2);
  append(bytes, 2, 1);
  append(bytes, 1, 1);
  append(bytes, 1, 1);
  append(bytes, 3000, 2);
  append(bytes, 400, 2);
  append(bytes, 494, 2);
  append(bytes, 80, 2);
  append(bytes, 0, 1);
  constexpr uint8_t unknown[] = {'r', 'a', 'w', 0xff, '&', 0};
  bytes.insert(bytes.end(), std::begin(unknown), std::end(unknown));
  append(bytes, 0xfedcba98, 4);
  return bytes;
}

Bytes encode(const Config &config) {
  std::array<uint8_t, Config::max_packet_bytes> output{};
  const auto count = config.encode(output);
  CHECK(count);
  return {output.begin(), output.begin() + count};
}

void replace(std::string &text, std::string_view original, std::string_view replacement) {
  const auto at = text.find(original);
  CHECK(at != text.npos);
  text.replace(at, original.size(), replacement);
}

Config loaded() {
  Config config;
  CHECK(config.parse_schema(schema()));
  CHECK(config.schema_ready() && !config.ready());
  // Fixed CRC32C computed independently from the documented signature string.
  CHECK(config.signature() == 0x329c07da);
  CHECK(config.decode(packet()));
  return config;
}

void roundtrip_and_edits() {
  auto config = loaded();
  auto original = packet();
  auto write = original;
  write[0] = 95;
  CHECK(encode(config) == write);
  CHECK(!config.dirty());
  CHECK(config.number("max_current") == 150);
  CHECK(config.number("max_current_brake") == -90);
  CHECK(config.number("horn_strength") == 4);
  CHECK(config.number("future_limit") == double(0xfedcba98));
  CHECK(!config.number("future_blob"));
  CHECK(!config.number("hw_name"));
  CHECK(!config.field("future_limit")->editable);
  CHECK(!config.field("package_disable")->editable);
  const auto offset = config.field("balance_pitch")->offset;
  CHECK(config.set_number("balance_pitch", 31.244));
  CHECK(config.number("balance_pitch") == 31.24);
  CHECK(config.number("balance_pitch", false) == 30);
  CHECK(config.dirty());
  write[offset] = 0x0c;
  write[offset + 1] = 0x34;
  CHECK(encode(config) == write);
  CHECK(!config.set_number("future_limit", 1));
  CHECK(!config.set_number("package_disable", 0));
  CHECK(!config.set_number("balance_pitch", 55.01));
  CHECK(!config.set_number("balance_pitch", 4.99));
  CHECK(!config.set_number("balance_pitch", std::numeric_limits<double>::quiet_NaN()));
  CHECK(!config.set_number("max_current", std::numeric_limits<double>::infinity()));
  config.reset_draft();
  CHECK(!config.dirty());
  CHECK(config.number("balance_pitch") == 30);
  CHECK(config.set_number("max_current_brake", -100));
  CHECK(config.number("max_current_brake") == -100);
  CHECK(config.set_number("ramp_alpha_pos", .015));
  CHECK(config.number("ramp_alpha_pos") == .015);
  CHECK(config.set_number("speed_limit", 0));
  CHECK(config.number("speed_limit") == 0);
  CHECK(config.set_number("anti_loop_enabled", 0));
  CHECK(config.number("anti_loop_enabled") == 0);
  CHECK(config.set_number("button_function", 3));
  CHECK(config.number("button_function") == 3);
  const auto saved = encode(config);
  auto readback = saved;
  readback[0] = 93;
  CHECK(config.decode(readback));
  CHECK(!config.dirty());
  CHECK(config.number("max_current_brake", false) == -100);
  CHECK(encode(config) == saved);
  CHECK(config.encode(std::span<uint8_t>{}) == 0);
  config.clear();
  CHECK(!config.schema_ready() && !config.ready() && config.fields().empty());
}

void schema_validation() {
  Config config;
  auto text = schema();
  replace(text, "<longName>balance_pitch</longName>", "<longName>Angle &amp; tilt &#176;</longName>");
  CHECK(config.parse_schema(text));
  CHECK(config.signature() == 0x329c07da);
  CHECK(config.field("balance_pitch")->label == "Angle & tilt °");
  for (const auto &[from, to] : std::vector<std::pair<std::string, std::string>>{
           {"<vTx>4</vTx>", "<vTx>3</vTx>"},
           {"<valString>bike_config</valString>", "<valString>other_config</valString>"},
           {"<ser>speed_limit</ser>", "<ser>nonexistent</ser>"},
           {"<ser>speed_limit</ser>", "<ser>balance_pitch</ser>"},
           {"<enumNames>Wheelie</enumNames>", "<enumNames>Other</enumNames>"},
           {"<SerOrder>", "<SerOrder/><SerOrder>"},
           {"<Params>", "<Params/><Params>"},
           {"<type>3</type>", "<type>3</type><type>3</type>"},
           {"<longName>speed_limit</longName>", "<longName>&external;</longName>"},
           {"<longName>speed_limit</longName>", "<longName>&#0;</longName>"},
           {"<ConfigParams>", "<ConfigParams xmlns='unknown'>"},
           {"<ConfigParams>", "<!DOCTYPE x [<!ENTITY external SYSTEM 'file:///private'>]><ConfigParams>"},
           {"</ConfigParams>", "</ConfigParams><ConfigParams/>"},
           {"</ConfigParams>", "</Wrong>"},
           {"<ConfigParams>", "<?xml version=\"1.0\"?><ConfigParams>"},
           {"<longName>speed_limit</longName>", "<longName>" + std::string(2050, 'a') + "</longName>"},
       }) {
    text = schema();
    replace(text, from, to);
    CHECK(!config.parse_schema(text));
    CHECK(!config.schema_ready());
    CHECK(!config.ready());
    CHECK(!config.error().empty());
  }
  text = schema();
  replace(text, "<vTxDoubleScale>10000.000000</vTxDoubleScale>", "<vTxDoubleScale>1000</vTxDoubleScale>");
  CHECK(!config.parse_schema(text));
  auto unsupported = specs;
  unsupported.back().wire = 9;
  CHECK(!config.parse_schema(schema(unsupported)));
  auto many = specs;
  for (size_t i = 0; i < Config::max_fields; ++i)
    many.push_back({"extra_" + std::to_string(i), 2, 1, 1, 0, 255, 1, ""});
  CHECK(!config.parse_schema(schema(many)));
  CHECK(!config.parse_schema(std::string(Config::max_schema_bytes + 1, 'x')));
  text = schema();
  text.insert(0, "\xef\xbb\xbf");
  CHECK(config.parse_schema(text));
  text.insert(0, "<!-- schema -->");
  CHECK(!config.parse_schema(text)); // BOM is allowed only at the start.
  text = schema();
  replace(text, "</Params>", "<!-- legitimate comment --></Params>");
  CHECK(config.parse_schema(text));
  text = schema();
  replace(text, "</Params>", "<!-- invalid -- comment --></Params>");
  CHECK(!config.parse_schema(text));
  text = schema();
  text.insert(text.find("<Params>"), std::string(1, '\0'));
  CHECK(!config.parse_schema(text));
}

void binary_validation() {
  auto config = loaded();
  const auto original = packet();
  for (size_t length = 0; length < original.size(); ++length) {
    CHECK(!config.decode(std::span(original).first(length)));
    CHECK(config.schema_ready() && !config.ready());
    CHECK(config.decode(original));
  }
  for (size_t i : {size_t(0), size_t(1), size_t(2), size_t(3), size_t(4), size_t(5)}) {
    auto bad = original;
    bad[i] ^= 1;
    CHECK(!config.decode(bad));
    CHECK(config.decode(original));
  }
  auto bad = original;
  bad.push_back(0);
  CHECK(!config.decode(bad));
  CHECK(config.decode(original));
  bad = original;
  bad[config.field("button_function")->offset] = 4;
  CHECK(!config.decode(bad));
  CHECK(config.decode(original));
  bad = original;
  bad[config.field("anti_loop_enabled")->offset] = 2;
  CHECK(!config.decode(bad));
  CHECK(config.decode(original));
  bad = original;
  bad[6] = 'x';
  CHECK(!config.decode(bad));
  CHECK(config.decode(original));
  bad = original;
  auto blob = config.field("future_blob");
  std::fill(bad.begin() + blob->offset, bad.end(), 0xff);
  CHECK(!config.decode(bad));
  CHECK(!config.decode(Bytes(Config::max_packet_bytes + 1, 0)));
}

void schema_order_and_future_types() {
  auto original = loaded();
  const auto baseline = packet();
  std::vector<std::string> reverse;
  for (auto it = specs.rbegin(); it != specs.rend(); ++it)
    reverse.push_back(it->name);
  Config reordered;
  CHECK(reordered.parse_schema(schema(specs, reverse)));
  CHECK(reordered.signature() != original.signature());
  Bytes reversed{93, 0};
  append(reversed, reordered.signature(), 4);
  for (const auto &name : reverse) {
    const auto field = original.field(name);
    CHECK(field);
    reversed.insert(reversed.end(), baseline.begin() + field->offset, baseline.begin() + field->offset + field->size);
  }
  CHECK(reordered.decode(reversed));
  CHECK(reordered.number("max_current_brake") == -90);
  CHECK(reordered.number("ramp_alpha_pos") == .003);
  CHECK(reordered.number("horn_frequency") == 494);
  CHECK(reordered.set_number("speed_limit", 20));
  CHECK(reordered.number("speed_limit") == 20);

  auto extended = specs;
  extended.insert(extended.end(), {
                                      {"future_u8", 2, 1, 1, 0, 255, 1, ""},
                                      {"future_i8", 2, 2, 1, -128, 127, 1, ""},
                                      {"future_u16", 2, 3, 1, 0, 65535, 1, ""},
                                      {"future_i16", 2, 4, 1, -32768, 32767, 1, ""},
                                      {"future_i32", 2, 6, 1, -2147483648., 2147483647., 1, ""},
                                      {"future_double32", 1, 8, 100, -100, 100, .01, ""},
                                      {"future_auto", 1, 9, 1, -100, 100, .01, ""},
                                      {"future_bits", 6, 0, 1, 0, 255, 1, ""},
                                  });
  Config future;
  CHECK(future.parse_schema(schema(extended)));
  auto bytes = packet(future.signature());
  append(bytes, 255, 1);
  append(bytes, 128, 1);
  append(bytes, 65535, 2);
  append(bytes, uint16_t(-32768), 2);
  append(bytes, 0x80000000, 4);
  append(bytes, uint32_t(-125), 4);
  append(bytes, 0x40200000, 4); // VESC double32_auto's ordinary IEEE-754 2.5 encoding.
  append(bytes, 0xa5, 1);
  CHECK(future.decode(bytes));
  CHECK(future.number("future_u8") == 255);
  CHECK(future.number("future_i8") == -128);
  CHECK(future.number("future_u16") == 65535);
  CHECK(future.number("future_i16") == -32768);
  CHECK(future.number("future_i32") == -2147483648.);
  CHECK(future.number("future_double32") == -1.25);
  CHECK(future.number("future_auto") == 2.5);
  CHECK(future.number("future_bits") == 0xa5);
  auto expected = bytes;
  expected[0] = 95;
  CHECK(encode(future) == expected);
  CHECK(!future.set_number("future_auto", 1));
  const auto at = future.field("future_auto")->offset;
  bytes[at] = 0x7f;
  bytes[at + 1] = 0x80;
  bytes[at + 2] = 0;
  bytes[at + 3] = 0;
  CHECK(!future.decode(bytes));
}

void wire_quantization_and_editor_steps() {
  auto coarse = specs;
  for (auto &spec : coarse) {
    if (spec.name == "ramp_alpha_pos")
      spec.step = .01;
    if (spec.name == "balance_pitch")
      spec.step = 10;
  }
  Config config;
  CHECK(config.parse_schema(schema(coarse)));
  CHECK(config.signature() == 0x329c07da); // Editor steps are not part of the signature.
  CHECK(config.decode(packet()));
  CHECK(config.set_number("ramp_alpha_pos", .003));
  CHECK(config.number("ramp_alpha_pos") == .003);
  CHECK(!config.dirty());
  CHECK(config.set_number("balance_pitch", 30.5));
  CHECK(config.number("balance_pitch") == 30.5);
  CHECK(config.set_number("ramp_alpha_pos", .00314));
  CHECK(config.number("ramp_alpha_pos") == .0031);
  config.reset_draft();

  auto narrowed = specs;
  for (auto &spec : narrowed)
    if (spec.name == "balance_pitch") {
      spec.minimum = 5.004;
      spec.maximum = 30.004;
    }
  CHECK(config.parse_schema(schema(narrowed)));
  CHECK(config.decode(packet()));
  CHECK(!config.set_number("balance_pitch", 5.004)); // Would encode 5.00, below schema minimum.
  CHECK(!config.dirty());
  CHECK(config.set_number("balance_pitch", 5.006));
  CHECK(config.number("balance_pitch") == 5.01);
  config.reset_draft();
  CHECK(config.set_number("balance_pitch", 30.004)); // Encodes 30.00 within the maximum.
  CHECK(config.number("balance_pitch") == 30);
  for (auto &spec : narrowed)
    if (spec.name == "balance_pitch")
      spec.maximum = 29.995;
  CHECK(config.parse_schema(schema(narrowed)));
  auto reply = packet();
  auto baseline = loaded();
  const auto offset = baseline.field("balance_pitch")->offset;
  reply[offset] = 0x0b;
  reply[offset + 1] = 0x54; // 29.00 degrees.
  CHECK(config.decode(reply));
  CHECK(!config.set_number("balance_pitch", 29.995)); // Would encode 30.00, above schema maximum.
  CHECK(!config.dirty());
  CHECK(config.set_number("balance_pitch", 29.994));
  CHECK(config.number("balance_pitch") == 29.99);
}

void tuning_backends() {
  CHECK(!bike::make_tuning_backend(bike::IntegrationId::Auto));
  CHECK(!bike::make_tuning_backend(bike::IntegrationId::WheelE));
  CHECK(!bike::make_tuning_backend(bike::IntegrationId::Vesc));
  auto backend = bike::make_tuning_backend(bike::IntegrationId::Wfb);
  CHECK(backend && backend->integration_id() == bike::IntegrationId::Wfb && backend->config_slot() == 0);
  CHECK(!backend->schema_ready() && !backend->ready());
  CHECK(backend->preset_labels().size() == 3 && backend->preset_descriptions().size() == 3);
  CHECK(backend->section_labels().size() == 5);
  CHECK(!backend->select_preset(0) && !backend->limit_mph());
  CHECK(backend->minimum_limit_mph() == 5 && backend->maximum_limit_mph() == 35 && !backend->supports_unlimited());
  CHECK(backend->parse_schema(schema()));
  CHECK(backend->schema_ready() && !backend->ready());
  CHECK(backend->decode(packet()));
  CHECK(backend->preset_index() == 1 && backend->preset_index(false) == 1);
  CHECK(backend->limit_mph() == 19 && !backend->dirty());
  CHECK(backend->minimum_limit_mph() == 5 && backend->maximum_limit_mph() == 35 && backend->supports_unlimited());
  CHECK(backend->controls(0).size() == 4 && backend->controls(4).size() == 1);
  CHECK(backend->controls(-1).empty() && backend->controls(5).empty());
  auto find = [&](int section, std::string_view label) {
    for (const auto &control : backend->controls(section))
      if (control.label == label)
        return control;
    return bike::TuningControl{};
  };
  auto button = find(1, "Button action"), horn = find(3, "Horn strength");
  CHECK(button.id >= 0 && button.kind == 2 && horn.id >= 0);
  CHECK(find(1, "Wheelie angle").id >= 0 && find(2, "Anti-loop angle").read_only);
  CHECK(find(1, "Maintain on throttle").id == -1); // Only the shared anti-loop angle remains relevant with Horn.
  CHECK(backend->adjust(horn.id, 1));
  CHECK(backend->adjust(button.id, -1) && backend->adjust(button.id, -1));
  CHECK(find(1, "Wheelie angle").id >= 0 && find(3, "Horn strength").id == -1);
  CHECK(!backend->adjust(horn.id, 1)); // Hidden controls cannot be changed accidentally.
  const auto changed = backend->changed_controls();
  CHECK(changed.size() == 2); // Hidden pending edits remain in the confirmation summary.
  auto same_angle = find(2, "Same angle as button");
  CHECK(same_angle.kind == 1 && backend->adjust(same_angle.id, -1));
  CHECK(!find(2, "Anti-loop angle").read_only);
  CHECK(!backend->adjust(-1, 1) && !backend->adjust(button.id, 2));
  backend->reset_draft();
  CHECK(!backend->dirty() && find(3, "Horn strength").id == horn.id);
  CHECK(backend->set_limit_mph(0) && backend->limit_mph() == 0);
  CHECK(backend->changed_controls().front().value == "No limit");
  backend->reset_draft();
  CHECK(!backend->set_limit_mph(4) && !backend->set_limit_mph(36));
  CHECK(backend->set_limit_mph(35) && backend->limit_mph() == 35 && backend->limit_mph(false) == 19);
  CHECK(backend->changed_controls().size() == 1);
  backend->reset_draft();
  CHECK(!backend->select_preset(-1) && !backend->select_preset(3) && !backend->dirty());
  CHECK(backend->select_preset(0) && backend->preset_index() == 0 && backend->preset_index(false) == 1);
  std::array<uint8_t, bike::TuningBackend::max_packet_bytes> output{};
  const auto size = backend->encode(output);
  auto config = loaded();
  CHECK(config.set_number("max_current", 90) && config.set_number("ramp_alpha_pos", .002) &&
        config.set_number("ramp_alpha_neg", .005));
  CHECK(Bytes(output.begin(), output.begin() + size) == encode(config)); // Includes unknown fields byte for byte.
  CHECK(backend->select_preset(2) && backend->preset_index() == 2);
  CHECK(backend->adjust(find(0, "Throttle strength").id, -1));
  CHECK(backend->preset_index() == int(backend->preset_labels().size()));
  backend->reset_draft();
  auto restricted = specs;
  for (auto &spec : restricted)
    if (spec.name == "ramp_alpha_pos")
      spec.minimum = .003;
  CHECK(backend->parse_schema(schema(restricted)) && backend->decode(packet()));
  CHECK(!backend->select_preset(0) && !backend->dirty() && !backend->error().empty());
  for (auto &spec : restricted)
    if (spec.name == "speed_limit")
      spec.minimum = 5;
  CHECK(backend->parse_schema(schema(restricted)) && backend->decode(packet()));
  CHECK(!backend->set_limit_mph(0) && !backend->dirty() && !backend->supports_unlimited());
  for (auto &spec : restricted)
    if (spec.name == "speed_limit") {
      spec.minimum = 10;
      spec.maximum = 25;
    }
  CHECK(backend->parse_schema(schema(restricted)) && backend->decode(packet()));
  CHECK(backend->minimum_limit_mph() == 10 && backend->maximum_limit_mph() == 25);
  CHECK(!backend->set_limit_mph(9) && !backend->set_limit_mph(26));
  CHECK(backend->set_limit_mph(10) && backend->set_limit_mph(25));
  backend->clear();
  CHECK(!backend->schema_ready() && !backend->ready() && backend->controls(0).empty());
}

void guide_control_metadata() {
  auto fields = specs;
  fields.push_back({"lift_resistance_kp2", 1, 7, 100, 0, 3, .1, ""});
  fields.push_back({"throttle_tilt_enabled", 5, 0, 1, 0, 1, 1, ""});
  fields.push_back({"throttle_tilt_angle_limit", 1, 7, 100, -20, 20, .5, ""});
  fields.push_back({"brake_tilt_angle_limit", 1, 7, 100, -20, 20, .5, ""});
  auto backend = bike::make_tuning_backend(bike::IntegrationId::Wfb);
  CHECK(backend->parse_schema(schema(fields)));
  Config codec;
  CHECK(codec.parse_schema(schema(fields)));
  auto reply = packet(codec.signature());
  append(reply, 150, 2);
  append(reply, 1, 1);
  append(reply, 500, 2);
  append(reply, uint16_t(-500), 2);
  CHECK(backend->decode(reply));
  auto find = [&](int section, std::string_view label) {
    for (const auto &control : backend->controls(section))
      if (control.label == label)
        return control;
    return bike::TuningControl{};
  };
  auto strength = find(0, "Throttle strength"), regen = find(0, "Brake strength");
  CHECK(strength.kind == 3 && strength.minimum == 0 && strength.maximum == 9 && strength.position == 9 &&
        strength.step == 1 && strength.default_position == 9);
  CHECK(regen.position == 3 && regen.value == "-90 A");
  CHECK(backend->set_control(strength.id, 0) && find(0, "Throttle strength").value == "60 A");
  CHECK(backend->set_control(regen.id, 9) && find(0, "Brake strength").value == "-150 A");
  CHECK(!backend->set_control(strength.id, .5) && !backend->set_control(regen.id, 10));
  CHECK(!backend->set_control(strength.id, std::numeric_limits<double>::quiet_NaN()));
  CHECK(!backend->set_control(strength.id, std::numeric_limits<double>::infinity()));
  const auto response = find(0, "Throttle responsiveness");
  const double ramp_values[] = {.0005, .001, .002, .003, .005, .008, .015, .03, .06, 1};
  std::array<uint8_t, bike::TuningBackend::max_packet_bytes> bytes{};
  for (int i = 1; i <= 10; ++i) {
    CHECK(backend->set_control(response.id, i));
    auto count = backend->encode(bytes);
    bytes[0] = 93;
    CHECK(codec.decode({bytes.data(), count}) && codec.number("ramp_alpha_pos") == ramp_values[i - 1]);
  }
  CHECK(find(0, "Throttle responsiveness").value == "Instant (1.0000)");
  CHECK(!backend->set_control(response.id, 0) && !backend->set_control(response.id, 5.5));
  const auto lift = find(2, "Wheel lift resistance");
  CHECK(lift.position == 5 && lift.minimum == 0 && lift.maximum == 10 && lift.default_position == 5);
  CHECK(backend->set_control(lift.id, 10));
  CHECK(find(2, "Wheel lift resistance").value == "10.0 / 10");
  const auto tilt = find(2, "Throttle tilt"), brake_tilt = find(2, "Brake tilt");
  CHECK(tilt.id >= 0 && !tilt.read_only && tilt.minimum == -20 && tilt.maximum == 20 && tilt.step == .5);
  CHECK(tilt.value.find("+5.0") == 0 && brake_tilt.value.find("-5.0") == 0); // Available with Horn selected.
  CHECK(backend->set_control(tilt.id, -19.5));
  const auto tilt_enabled = find(2, "Throttle / brake tilt");
  CHECK(backend->set_control(tilt_enabled.id, 0));
  CHECK(find(2, "Throttle tilt").read_only && !backend->set_control(tilt.id, 5));
  CHECK(find(2, "Throttle tilt").read_only_reason == "Enable throttle / brake tilt to edit");
  const auto anti = find(2, "Anti-loop angle");
  CHECK(anti.read_only && anti.position == 30 && !backend->set_control(anti.id, 35));
  CHECK(anti.read_only_reason == "Linked to wheelie angle");
  CHECK(backend->set_control(find(1, "Wheelie angle").id, 35));
  CHECK(find(2, "Anti-loop angle").position == 35 &&
        find(2, "Anti-loop angle").value == find(1, "Wheelie angle").value);
  CHECK(backend->set_control(find(2, "Same angle as button").id, 0));
  CHECK(!find(2, "Anti-loop angle").read_only && find(2, "Anti-loop angle").position == 30);
  CHECK(backend->set_control(find(2, "Anti-loop throttle").id, 0));
  CHECK(find(2, "Anti-loop angle").read_only_reason == "Enable anti-loop to edit");
  CHECK(!backend->set_control(anti.id, 40));
  const auto horn = find(3, "Horn strength"), note = find(3, "Horn frequency");
  CHECK(horn.minimum == 1 && horn.maximum == 10 && !backend->set_control(horn.id, 11));
  CHECK(note.position == 15 && note.maximum == 27 && note.tick_every == 2 && note.value == "B4 (494 Hz)");
  CHECK(backend->set_control(note.id, 0) && find(3, "Horn frequency").value == "G#3 (208 Hz)");
  CHECK(backend->set_control(note.id, 27) && find(3, "Horn frequency").value == "B5 (988 Hz)");
  CHECK(find(1, "Button action").options.size() == 4);
  CHECK(backend->set_control(find(4, "Return response").id, double(float(.008))));
  backend->reset_draft();
  auto count = backend->encode(bytes);
  bytes[0] = 93;
  CHECK(codec.decode({bytes.data(), count}));
  CHECK(codec.set_number("max_current", 55) && codec.set_number("ramp_alpha_pos", .004) &&
        codec.set_number("horn_strength", 42) && codec.set_number("horn_frequency", 500));
  count = codec.encode(bytes);
  bytes[0] = 93;
  CHECK(backend->decode({bytes.data(), count}) && !backend->dirty());
  CHECK(find(0, "Throttle strength").off_grid && find(0, "Throttle strength").value == "55 A");
  CHECK(find(0, "Throttle responsiveness").off_grid && find(0, "Throttle responsiveness").value == "0.0040");
  CHECK(find(3, "Horn strength").off_grid && find(3, "Horn strength").value == "42");
  CHECK(find(3, "Horn frequency").off_grid && find(3, "Horn frequency").value == "500 Hz");
  CHECK(!find(3, "Horn strength").can_increase && find(3, "Horn strength").can_decrease);
  const Bytes original(bytes.begin(), bytes.begin() + count);
  count = backend->encode(bytes);
  bytes[0] = 93;
  CHECK(Bytes(bytes.begin(), bytes.begin() + count) == original); // Rendering never normalizes reported values.
  CHECK(backend->adjust(find(0, "Throttle responsiveness").id, 1));
  CHECK(find(0, "Throttle responsiveness").position == 5); // Next documented stop is .005.
  CHECK(backend->adjust(find(3, "Horn strength").id, -1));
  CHECK(find(3, "Horn strength").value == "10"); // Explicit edit enters supported UI range.
  auto narrow_fields = fields;
  for (auto &field : narrow_fields)
    if (field.name == "max_current") {
      field.minimum = 95;
      field.maximum = 145;
    }
  CHECK(backend->parse_schema(schema(narrow_fields)));
  CHECK(codec.parse_schema(schema(fields)) && codec.decode(reply) && codec.set_number("max_current", 125));
  count = codec.encode(bytes);
  bytes[0] = 93;
  CHECK(backend->decode({bytes.data(), count}));
  strength = find(0, "Throttle strength");
  CHECK(strength.minimum == 4 && strength.maximum == 8 && strength.off_grid && strength.value == "125 A");
  CHECK(!backend->set_control(strength.id, 3) && !backend->set_control(strength.id, 9));
  CHECK(backend->set_control(strength.id, 4) && find(0, "Throttle strength").value == "100 A");
  for (auto &field : narrow_fields)
    if (field.name == "max_current") {
      field.minimum = 0;
      field.maximum = 50;
    }
  CHECK(backend->parse_schema(schema(narrow_fields)));
  CHECK(codec.set_number("max_current", 40));
  count = codec.encode(bytes);
  bytes[0] = 93;
  CHECK(backend->decode({bytes.data(), count}));
  strength = find(0, "Throttle strength");
  CHECK(strength.read_only && strength.off_grid && strength.value == "40 A");
  CHECK(strength.read_only_reason == "Not supported by this bike");
  CHECK(!backend->set_control(strength.id, 0) && !backend->adjust(strength.id, 1));
}

void bounded_random_inputs() {
  std::mt19937 random(0x574642);
  Config config;
  for (size_t iteration = 0; iteration < 4000; ++iteration) {
    std::string text(random() % 500, ' ');
    for (auto &byte : text)
      byte = char(random());
    CHECK(!config.parse_schema(text));
    CHECK(config.parse_schema(schema()));
    Bytes bytes(random() % 200, 0);
    for (auto &byte : bytes)
      byte = uint8_t(random());
    CHECK(!config.decode(bytes));
  }
}

} // namespace

int main() {
  roundtrip_and_edits();
  schema_validation();
  binary_validation();
  schema_order_and_future_types();
  wire_quantization_and_editor_steps();
  tuning_backends();
  guide_control_metadata();
  bounded_random_inputs();
  std::cout << "WFB schema CRC, field validation, byte-preserving edits and malformed inputs passed\n";
}
