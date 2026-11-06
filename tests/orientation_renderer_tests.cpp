#include "bike/orientation.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

struct Image {
  int width, height;
  std::vector<std::array<unsigned char, 3>> pixels;

  explicit Image(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    std::string magic;
    int maximum;
    input >> magic >> width >> height >> maximum;
    if (!input || magic != "P6" || maximum != 255 || width <= 0 || height <= 0 || width > 4096 || height > 4096)
      throw std::runtime_error("Invalid rendered fixture: " + path.string());
    input.get();
    pixels.resize(size_t(width) * height);
    input.read(reinterpret_cast<char *>(pixels.data()), std::streamsize(pixels.size() * 3));
    if (!input)
      throw std::runtime_error("Incomplete rendered fixture");
  }

  auto at(int x, int y) const {
    return pixels[size_t(y) * width + x];
  }

  bool interior(int x, int y) const {
    if (x < 2 || y < 2 || x + 2 >= width || y + 2 >= height)
      return false;
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx)
        if (at(x + dx, y + dy) != at(x, y))
          return false;
    return true;
  }
};

int main(int argc, char **argv) {
  if (argc != 2 && argc != 4)
    throw std::runtime_error("Supply the renderer fixture directory and optional native width/height");
  const int native_width = argc == 4 ? std::stoi(argv[2]) : bike::panel_width;
  const int native_height = argc == 4 ? std::stoi(argv[3]) : bike::panel_height;
  if (native_width <= 0 || native_height <= 0 || native_width > 4096 || native_height > 4096 || native_width % 2 ||
      native_height % 2)
    throw std::runtime_error("Invalid native panel dimensions");
  const std::filesystem::path directory(argv[1]);
  for (const auto orientation : {bike::Orientation::Portrait, bike::Orientation::Landscape,
                                 bike::Orientation::PortraitInverted, bike::Orientation::LandscapeInverted}) {
    const std::string name = std::to_string(static_cast<int>(orientation));
    const int logical_width = bike::display_width(orientation, native_width, native_height);
    const Image logical(directory / (std::to_string(logical_width) + "-touch-" + name + "-logical.ppm"));
    const Image panel(directory / (std::to_string(native_width) + "-touch-" + name + "-panel.ppm"));
    if (panel.width != native_width || panel.height != native_height || logical.width != logical_width ||
        logical.height != bike::display_height(orientation, native_width, native_height))
      throw std::runtime_error("Unexpected renderer dimensions");
    size_t checked = 0;
    for (int y = 0; y < panel.height; ++y)
      for (int x = 0; x < panel.width; ++x) {
        const auto point = bike::to_logical({x, y}, orientation, native_width, native_height);
        if (point.x < 0 || point.y < 0 || point.x >= logical.width || point.y >= logical.height)
          throw std::runtime_error("Touch maps outside the rendered window");
        // Uniform interiors avoid antialiased edge rounding differences while
        // retaining an independent oracle: actual unrotated/rotated pixels.
        if (!logical.interior(point.x, point.y))
          continue;
        ++checked;
        const auto expected = logical.at(point.x, point.y), actual = panel.at(x, y);
        for (int channel = 0; channel < 3; ++channel)
          if (std::abs(int(actual[channel]) - int(expected[channel])) > 2) {
            std::cerr << name << ": physical " << x << ',' << y << " maps to wrong rendered pixel\n";
            return EXIT_FAILURE;
          }
      }
    if (checked < 100000)
      throw std::runtime_error("Insufficient rendered pixel coverage");
    std::cout << name << ": " << checked << " actual rendered pixels match production touch mapping\n";
  }
}
