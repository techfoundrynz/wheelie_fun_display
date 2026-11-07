#pragma once
#include "integration.hpp"
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace bike
{

struct TuningControl {
  int id = -1;
  std::string label, value, saved;
  bool can_decrease = false, can_increase = false, changed = false;
  int kind = 0; // 0 numeric, 1 boolean, 2 enum, 3 slider.
  double minimum = 0, maximum = 0, position = 0, step = 1, default_position = 0;
  bool has_default = false, read_only = false, off_grid = false;
  int tick_every = 1;
  std::vector<std::string> options;
  std::string read_only_reason;
};

// A verified vehicle integration supplies tuning semantics. The display manager
// owns connection/session safety and the standard VESC custom-config exchange.
class TuningBackend {
public:

  static constexpr size_t max_schema_bytes = 128 * 1024, max_packet_bytes = 1024;

  virtual ~TuningBackend() = default;

  virtual IntegrationId integration_id() const = 0;

  virtual uint8_t config_slot() const = 0;

  virtual bool schema_ready() const = 0;

  virtual bool ready() const = 0;

  virtual std::string_view error() const = 0;

  virtual bool parse_schema(std::string_view xml) = 0;

  virtual bool decode(std::span<const uint8_t> reply) = 0;

  virtual size_t encode(std::span<uint8_t> output) const = 0;

  virtual void clear() = 0;

  virtual void reset_draft() = 0;

  virtual bool dirty() const = 0;

  virtual std::span<const std::string_view> preset_labels() const = 0;

  virtual std::span<const std::string_view> preset_descriptions() const = 0;

  virtual std::span<const std::string_view> section_labels() const = 0;

  // -1 means unknown; labels.size() means a valid non-preset/custom response.
  virtual int preset_index(bool draft = true) const = 0;

  virtual bool select_preset(int index) = 0;

  virtual std::optional<int> limit_mph(bool draft = true) const = 0;

  virtual bool set_limit_mph(int mph) = 0;

  virtual int minimum_limit_mph() const = 0;

  virtual int maximum_limit_mph() const = 0;

  virtual bool supports_unlimited() const = 0;

  virtual bool adjust(int id, int delta) = 0;

  virtual bool set_control(int id, double position) = 0;

  virtual std::vector<TuningControl> controls(int section) const = 0;

  virtual std::vector<TuningControl> changed_controls() const = 0;

};

// Returns no backend until an integration has explicitly implemented tuning.
std::unique_ptr<TuningBackend> make_tuning_backend(IntegrationId integration);

} // namespace bike
