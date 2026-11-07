#include "bike/tuning.hpp"
#include "bike/wfb_config.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace bike
{
namespace
{

enum class Mapping {
  Direct,
  Strength,
  Regen,
  Ramp,
  Lift,
  Note
};

struct FieldSpec {
  const char *name, *label, *suffix;
  int section, kind;
  double minimum, maximum, step, default_position;
  int decimals, tick_every;
  Mapping mapping = Mapping::Direct;
};

constexpr FieldSpec specs[] = {
    {"max_current", "Throttle strength", " A", 0, 3, 0, 9, 1, 9, 0, 1, Mapping::Strength},
    {"max_current_brake", "Brake strength", " A", 0, 3, 0, 9, 1, 3, 0, 1, Mapping::Regen},
    {"ramp_alpha_pos", "Throttle responsiveness", "", 0, 3, 1, 10, 1, 4, 4, 1, Mapping::Ramp},
    {"ramp_alpha_neg", "Brake responsiveness", "", 0, 3, 1, 10, 1, 6, 4, 1, Mapping::Ramp},
    {"ramp_alpha_return", "Return response", "", 4, 0, .0001, 1, .0001, .008, 4, 1},
    {"balance_pitch", "Wheelie angle", "\u00b0", 1, 3, 5, 55, .5, 30, 1, 10},
    {"button_function", "Button action", "", 1, 2, 0, 3, 1, 2, 0, 1},
    {"anti_loop_enabled", "Anti-loop throttle", "", 2, 1, 0, 1, 1, 1, 0, 1},
    {"anti_loop_use_button_angle", "Same angle as button", "", 2, 1, 0, 1, 1, 1, 0, 1},
    {"anti_loop_angle", "Anti-loop angle", "\u00b0", 2, 3, 5, 55, .5, 30, 1, 10},
    {"lift_resistance_kp2", "Wheel lift resistance", "", 2, 3, 0, 10, 1, 5, 0, 1, Mapping::Lift},
    {"throttle_tilt_enabled", "Throttle / brake tilt", "", 2, 1, 0, 1, 1, 1, 0, 1},
    {"throttle_tilt_angle_limit", "Throttle tilt", "\u00b0", 2, 3, -20, 20, .5, 5, 1, 10},
    {"brake_tilt_angle_limit", "Brake tilt", "\u00b0", 2, 3, -20, 20, .5, -5, 1, 10},
    {"persist_wheelie_throttle", "Maintain on throttle", "", 1, 1, 0, 1, 1, 0, 0, 1},
    {"persist_wheelie_brake", "Maintain on brake", "", 1, 1, 0, 1, 1, 0, 0, 1},
    {"horn_strength", "Horn strength", "", 3, 3, 1, 10, 1, 4, 0, 1},
    {"horn_frequency", "Horn frequency", " Hz", 3, 3, 0, 27, 1, 15, 0, 2, Mapping::Note},
    {"kp", "Angle P", "", 4, 0, 0, 40, .5, 30, 1, 1},
    {"kp2", "Rate P", "", 4, 0, 0, 3, .05, 1, 2, 1},
    {"mahony_kp", "Pitch filter gain", "", 4, 0, 0, 3, .1, 2, 1, 1},
    {"kp_brake", "Brake angle P", "", 4, 0, .2, 2, .1, 1.5, 1, 1},
    {"kp2_brake", "Brake rate P", "", 4, 0, 0, 2, .1, 1.5, 1, 1},
};
constexpr double ramps[] = {.0005, .001, .002, .003, .005, .008, .015, .03, .06, 1};
constexpr double notes[] = {208, 220, 233, 247, 262, 277, 294, 311, 330, 349, 370, 392, 415, 440,
                            466, 494, 523, 554, 587, 622, 659, 698, 740, 784, 831, 880, 932, 988};
constexpr const char *note_names[] = {"G#3", "A3", "A#3", "B3", "C4",  "C#4", "D4",  "D#4", "E4", "F4",
                                      "F#4", "G4", "G#4", "A4", "A#4", "B4",  "C5",  "C#5", "D5", "D#5",
                                      "E5",  "F5", "F#5", "G5", "G#5", "A5",  "A#5", "B5"};

// Slider positions are presentation coordinates; raw configuration stays untouched
// until an explicit edit selects a representable position.
double wire_value(const FieldSpec &spec, double position) {
  switch (spec.mapping) {
  case Mapping::Strength:
    return 60 + position * 10;
  case Mapping::Regen:
    return -60 - position * 10;
  case Mapping::Lift:
    return position * .3;
  case Mapping::Ramp:
    return ramps[size_t(std::clamp(std::round(position) - 1, 0., 9.))];
  case Mapping::Note:
    return notes[size_t(std::clamp(std::round(position), 0., 27.))];
  default:
    return position;
  }
}

double raw_position(const FieldSpec &spec, double value) {
  if (spec.mapping == Mapping::Strength)
    return (value - 60) / 10;
  if (spec.mapping == Mapping::Regen)
    return (-value - 60) / 10;
  if (spec.mapping == Mapping::Lift)
    return value / .3;
  if (spec.mapping == Mapping::Ramp || spec.mapping == Mapping::Note) {
    const std::span<const double> values =
        spec.mapping == Mapping::Ramp ? std::span<const double>(ramps) : std::span<const double>(notes);
    const double base = spec.minimum;
    if (value < values.front())
      return base - 1;
    for (size_t i = 0; i < values.size(); ++i) {
      if (std::abs(values[i] - value) < 1e-8)
        return base + i;
      if (value < values[i])
        return base + i - 1 + (value - values[i - 1]) / (values[i] - values[i - 1]);
    }
    return spec.maximum + 1;
  }
  return value;
}

constexpr std::string_view response_labels[] = {"Eco", "Cruise", "Sport"};
constexpr std::string_view response_descriptions[] = {
    "90 A motor limit with gentle throttle and regen response.",
    "150 A motor limit with smooth throttle response.",
    "150 A motor limit with sharper throttle response.",
};
constexpr std::string_view sections[] = {"Response", "Button", "Throttle", "Horn", "Advanced"};

class WfbTuningBackend final : public TuningBackend {
  WfbConfig config_;
  std::string error_;

  double displayed_value(const FieldSpec &spec, bool draft) const {
    if (std::string_view(spec.name) == "anti_loop_angle" &&
        config_.number("anti_loop_use_button_angle", draft).value_or(0) != 0)
      return config_.number("balance_pitch", draft).value_or(0);
    return config_.number(spec.name, draft).value_or(0);
  }

  std::string formatted(const FieldSpec &spec, bool draft, bool derived = true) const {
    const auto number = config_.number(spec.name, draft);
    const auto *field = config_.field(spec.name);
    if (!number || !field)
      return "--";
    const double value = derived ? displayed_value(spec, draft) : *number;
    if (field->kind == WfbConfigKind::Bool)
      return value ? "Enabled" : "Disabled";
    if (field->kind == WfbConfigKind::Enum) {
      const auto index = size_t(value);
      return index < field->enum_names.size() ? field->enum_names[index] : "Unsupported";
    }
    char text[80];
    if (spec.mapping == Mapping::Note) {
      const auto position = raw_position(spec, value);
      if (position >= 0 && position <= 27 && std::abs(position - std::round(position)) < 1e-8) {
        std::snprintf(text, sizeof(text), "%s (%.0f Hz)", note_names[size_t(position)], value);
        return text;
      }
    }
    if (spec.mapping == Mapping::Ramp) {
      const auto position = raw_position(spec, value);
      if (position >= 1 && position <= 10 && std::abs(position - std::round(position)) < 1e-8) {
        if (position == 10)
          std::snprintf(text, sizeof(text), "Instant (%.4f)", value);
        else
          std::snprintf(text, sizeof(text), "%.0f / 10 (%.4f)", position, value);
        return text;
      }
    }
    if (spec.mapping == Mapping::Lift) {
      std::snprintf(text, sizeof(text), "%.1f / 10", value / .3);
    }
    else if (std::string_view(spec.name) == "throttle_tilt_angle_limit" ||
             std::string_view(spec.name) == "brake_tilt_angle_limit") {
      std::snprintf(text, sizeof(text), "%+.1f%s", value, spec.suffix);
    }
    else
      std::snprintf(text, sizeof(text), "%.*f%s", spec.decimals, value, spec.suffix);
    return text;
  }

  bool show(const FieldSpec &spec) const {
    auto value = [&](const char *name, double fallback) { return config_.number(name).value_or(fallback); };
    const double button = value("button_function", -1);
    const std::string_view name = spec.name;
    if (name == "horn_strength" || name == "horn_frequency")
      return button == 2;
    if (name == "balance_pitch")
      return button == 0 || (value("anti_loop_enabled", 0) != 0 && value("anti_loop_use_button_angle", 0) != 0);
    if (name == "persist_wheelie_throttle" || name == "persist_wheelie_brake")
      return button == 0;
    return true;
  }

  bool read_only(const FieldSpec &spec) const {
    const std::string_view name = spec.name;
    auto on = [&](const char *field) { return config_.number(field).value_or(0) != 0; };
    if (name == "anti_loop_angle")
      return !on("anti_loop_enabled") || on("anti_loop_use_button_angle");
    if (name == "anti_loop_use_button_angle")
      return !on("anti_loop_enabled");
    if (name == "throttle_tilt_angle_limit" || name == "brake_tilt_angle_limit")
      return !on("throttle_tilt_enabled");
    return false;
  }

  std::pair<double, double> bounds(const FieldSpec &spec, const WfbConfigField &field) const {
    double minimum = spec.minimum, maximum = spec.maximum;
    if (spec.mapping == Mapping::Ramp || spec.mapping == Mapping::Note) {
      while (minimum <= maximum && wire_value(spec, minimum) < field.minimum - 1e-8)
        minimum += spec.step;
      while (maximum >= minimum && wire_value(spec, maximum) > field.maximum + 1e-8)
        maximum -= spec.step;
    }
    else {
      double first = raw_position(spec, field.minimum), last = raw_position(spec, field.maximum);
      if (first > last)
        std::swap(first, last);
      minimum = std::max(minimum, first);
      maximum = std::min(maximum, last);
      minimum = spec.minimum + std::ceil((minimum - spec.minimum) / spec.step - 1e-8) * spec.step;
      maximum = spec.minimum + std::floor((maximum - spec.minimum) / spec.step + 1e-8) * spec.step;
    }
    return {minimum, maximum};
  }

  std::optional<TuningControl> control(size_t index) const {
    if (!config_.ready() || index >= std::size(specs))
      return {};
    const auto &spec = specs[index];
    const auto *field = config_.field(spec.name);
    const auto value = config_.number(spec.name), saved = config_.number(spec.name, false);
    if (!field || !field->editable || !value || !saved)
      return {};
    const auto [minimum, maximum] = bounds(spec, *field);
    const double actual = raw_position(spec, displayed_value(spec, true));
    const bool disabled = read_only(spec) || minimum > maximum;
    TuningControl result;
    result.id = int(index);
    result.label = spec.label;
    result.value = formatted(spec, true);
    result.saved = formatted(spec, false);
    result.can_decrease = !disabled && actual > minimum + 1e-8;
    result.can_increase = !disabled && actual < maximum - 1e-8;
    result.changed = std::abs(*value - *saved) > 1e-8;
    result.kind = spec.kind;
    result.minimum = minimum;
    result.maximum = std::max(minimum, maximum);
    result.position = std::clamp(actual, minimum, result.maximum);
    result.step = spec.step;
    result.default_position = spec.default_position;
    result.has_default = spec.default_position >= minimum && spec.default_position <= maximum;
    result.read_only = disabled;
    if (minimum > maximum)
      result.read_only_reason = "Not supported by this bike";
    else if (disabled) {
      const std::string_view name = spec.name;
      if (name == "anti_loop_angle" || name == "anti_loop_use_button_angle")
        result.read_only_reason = config_.number("anti_loop_enabled").value_or(0) != 0 ? "Linked to wheelie angle"
                                                                                       : "Enable anti-loop to edit";
      else if (name == "throttle_tilt_angle_limit" || name == "brake_tilt_angle_limit")
        result.read_only_reason = "Enable throttle / brake tilt to edit";
    }
    result.tick_every = spec.tick_every;
    result.off_grid =
        minimum > maximum || actual < minimum - 1e-8 || actual > maximum + 1e-8 ||
        std::abs((actual - spec.minimum) / spec.step - std::round((actual - spec.minimum) / spec.step)) > 1e-7;
    if (field->kind == WfbConfigKind::Enum)
      result.options = field->enum_names;
    return result;
  }

public:

  IntegrationId integration_id() const override {
    return IntegrationId::Wfb;
  }

  uint8_t config_slot() const override {
    return 0;
  }

  bool schema_ready() const override {
    return config_.schema_ready();
  }

  bool ready() const override {
    return config_.ready();
  }

  std::string_view error() const override {
    return error_.empty() ? config_.error() : std::string_view(error_);
  }

  bool parse_schema(std::string_view xml) override {
    error_.clear();
    return config_.parse_schema(xml);
  }

  bool decode(std::span<const uint8_t> reply) override {
    error_.clear();
    return config_.decode(reply);
  }

  size_t encode(std::span<uint8_t> output) const override {
    return config_.encode(output);
  }

  void clear() override {
    error_.clear();
    config_.clear();
  }

  void reset_draft() override {
    error_.clear();
    config_.reset_draft();
  }

  bool dirty() const override {
    return config_.dirty();
  }

  std::span<const std::string_view> preset_labels() const override {
    return response_labels;
  }

  std::span<const std::string_view> preset_descriptions() const override {
    return response_descriptions;
  }

  std::span<const std::string_view> section_labels() const override {
    return sections;
  }

  int preset_index(bool draft) const override {
    const auto current = config_.number("max_current", draft), brake = config_.number("max_current_brake", draft);
    const auto pos = config_.number("ramp_alpha_pos", draft), neg = config_.number("ramp_alpha_neg", draft);
    if (!current || !brake || !pos || !neg)
      return -1;
    if (std::abs(*brake + 90) > .1)
      return 3;
    if (std::abs(*current - 90) < .1 && std::abs(*pos - .002) < .00005 && std::abs(*neg - .005) < .00005)
      return 0;
    if (std::abs(*current - 150) < .1 && std::abs(*neg - .008) < .00005) {
      if (std::abs(*pos - .003) < .00005)
        return 1;
      if (std::abs(*pos - .015) < .00005)
        return 2;
    }
    return 3;
  }

  bool select_preset(int index) override {
    if (!config_.ready() || index < 0 || size_t(index) >= std::size(response_labels))
      return false;
    const bool eco = index == 0;
    const std::array<std::pair<const char *, double>, 4> values = {{{"max_current", eco ? 90. : 150.},
                                                                    {"max_current_brake", -90.},
                                                                    {"ramp_alpha_pos", eco          ? .002
                                                                                       : index == 2 ? .015
                                                                                                    : .003},
                                                                    {"ramp_alpha_neg", eco ? .005 : .008}}};
    // Check every field and its wire-quantized value before mutating any draft.
    for (const auto &[name, value] : values) {
      const auto *field = config_.field(name);
      if (!field || !field->editable) {
        error_ = "Response preset is not supported by this bike schema.";
        return false;
      }
      const double scale = field->kind == WfbConfigKind::Double ? field->scale : 1;
      const double quantized = std::round(value * scale) / scale;
      if (value < field->minimum || value > field->maximum || quantized < field->minimum ||
          quantized > field->maximum) {
        error_ = "Response preset exceeds this bike's reported limits.";
        return false;
      }
    }
    for (const auto &[name, value] : values) {
      if (!config_.set_number(name, value))
        return false;
    }
    error_.clear();
    return true;
  }

  std::optional<int> limit_mph(bool draft) const override {
    const auto value = config_.number("speed_limit", draft);
    return value ? std::optional<int>(int(*value)) : std::nullopt;
  }

  int minimum_limit_mph() const override {
    const auto *field = config_.field("speed_limit");
    return config_.ready() && field ? std::max(5, int(std::ceil(field->minimum))) : 5;
  }

  int maximum_limit_mph() const override {
    const auto *field = config_.field("speed_limit");
    return config_.ready() && field ? std::min(35, int(std::floor(field->maximum))) : 35;
  }

  bool supports_unlimited() const override {
    const auto *field = config_.field("speed_limit");
    return config_.ready() && field && field->editable && field->minimum <= 0 && field->maximum >= 0;
  }

  bool set_limit_mph(int mph) override {
    return ((mph == 0 && supports_unlimited()) || (mph >= minimum_limit_mph() && mph <= maximum_limit_mph())) &&
           config_.set_number("speed_limit", mph);
  }

  bool set_control(int id, double position) override {
    if (!config_.ready() || id < 0 || size_t(id) >= std::size(specs) || !std::isfinite(position))
      return false;
    const auto &spec = specs[id];
    const auto item = control(size_t(id));
    if (!item || item->read_only || !show(spec) || position < item->minimum - 1e-8 || position > item->maximum + 1e-8)
      return false;
    const double steps = (position - spec.minimum) / spec.step;
    // Slint transports numbers as float; tolerate its final rounding error.
    if (std::abs(steps - std::round(steps)) > 1e-4)
      return false;
    return config_.set_number(spec.name, wire_value(spec, spec.minimum + std::round(steps) * spec.step));
  }

  bool adjust(int id, int delta) override {
    if (!config_.ready() || id < 0 || size_t(id) >= std::size(specs) || (delta != -1 && delta != 1))
      return false;
    const auto &spec = specs[id];
    const auto item = control(size_t(id));
    if (!item || item->read_only || !show(spec) || (delta < 0 && !item->can_decrease) ||
        (delta > 0 && !item->can_increase))
      return false;
    const double actual = raw_position(spec, displayed_value(spec, true));
    const double relative = (actual - spec.minimum) / spec.step;
    double next =
        spec.minimum + (delta > 0 ? std::floor(relative + 1e-8) + 1 : std::ceil(relative - 1e-8) - 1) * spec.step;
    next = std::clamp(next, item->minimum, item->maximum);
    return set_control(id, next);
  }

  std::vector<TuningControl> controls(int section) const override {
    std::vector<TuningControl> result;
    for (size_t i = 0; i < std::size(specs); ++i) {
      if (specs[i].section != section || !show(specs[i]))
        continue;
      if (auto item = control(i))
        result.push_back(std::move(*item));
    }
    return result;
  }

  std::vector<TuningControl> changed_controls() const override {
    std::vector<TuningControl> result;
    for (size_t i = 0; i < std::size(specs); ++i) {
      if (auto item = control(i); item && item->changed) {
        // Confirmation describes the persisted field, including a dormant
        // anti-loop angle when the shared button angle is currently selected.
        item->value = formatted(specs[i], true, false);
        item->saved = formatted(specs[i], false, false);
        result.push_back(std::move(*item));
      }
    }
    const auto value = limit_mph(true), saved = limit_mph(false);
    if (value && saved && *value != *saved) {
      TuningControl item;
      item.label = "Speed limit";
      item.value = *value == 0 ? "No limit" : std::to_string(*value) + " mph";
      item.saved = *saved == 0 ? "No limit" : std::to_string(*saved) + " mph";
      item.changed = true;
      result.push_back(std::move(item));
    }
    return result;
  }
};

} // namespace

std::unique_ptr<TuningBackend> make_tuning_backend(IntegrationId integration) {
  if (integration == IntegrationId::Wfb)
    return std::make_unique<WfbTuningBackend>();
  return {};
}

} // namespace bike
