#include "bike/stable_rows.hpp"
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#define CHECK(condition)                                                                                               \
  do {                                                                                                                 \
    if (!(condition)) {                                                                                                \
      std::cerr << __LINE__ << ": " #condition "\n";                                                                   \
      std::abort();                                                                                                    \
    }                                                                                                                  \
  } while (false)

struct Item {
  std::string label, value;
};

struct Model {
  std::vector<Item> items;
  std::vector<std::string> notifications;

  size_t row_count() const {
    return items.size();
  }

  std::optional<Item> row_data(size_t i) const {
    return i < items.size() ? std::optional(items[i]) : std::nullopt;
  }

  void push_back(const Item &item) {
    notifications.push_back("added " + std::to_string(items.size()));
    items.push_back(item);
  }

  void set_row_data(size_t i, const Item &item) {
    notifications.push_back("changed " + std::to_string(i));
    items.at(i) = item;
  }

  void erase(size_t i) {
    notifications.push_back("removed " + std::to_string(i));
    items.erase(items.begin() + i);
  }
};

int main() {
  Model model;
  auto refresh = [&](const std::vector<Item> &items) {
    model.notifications.clear();
    bike::StableRows rows(model);
    for (const auto &item : items)
      rows.add(item);
  };
  const std::vector<Item> initial{{"Version", "0.1.4"}, {"Frames", "100"}, {"Uptime", "10"}};
  refresh(initial);
  CHECK((model.notifications == std::vector<std::string>{"added 0", "added 1", "added 2"}));
  refresh(initial);
  CHECK(model.notifications.empty());
  refresh({{"Version", "0.1.4"}, {"Frames", "160"}, {"Uptime", "11"}});
  CHECK((model.notifications == std::vector<std::string>{"changed 1", "changed 2"}));
  CHECK(model.items[0].label == "Version" && model.items[1].value == "160");
  refresh({{"Version", "0.1.4"}, {"Different label", "160"}});
  CHECK((model.notifications == std::vector<std::string>{"changed 1", "removed 2"}));
  refresh({{"Version", "0.1.4"}});
  CHECK((model.notifications == std::vector<std::string>{"removed 1"}));
  refresh({});
  CHECK(model.items.empty() && model.notifications == std::vector<std::string>{"removed 0"});
  refresh(initial);
  CHECK(model.items.size() == initial.size());
  std::cout << "Stable row identity, selective notifications and resizing passed\n";
}
