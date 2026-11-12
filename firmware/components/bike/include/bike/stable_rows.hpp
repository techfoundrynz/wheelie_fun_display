#pragma once
#include <cstddef>

namespace bike
{

// Keep model row identities stable across refreshes. A reset reconstructs every
// repeated UI component, even when only a counter or one value changed.
template <class Model> class StableRows {
  Model &model;
  size_t count = 0;

public:

  explicit StableRows(Model &target) : model(target) {
  }

  template <class Item> void add(const Item &item) {
    const auto previous = model.row_data(count);
    if (!previous)
      model.push_back(item);
    else if (previous->label != item.label || previous->value != item.value)
      model.set_row_data(count, item);
    ++count;
  }

  ~StableRows() {
    while (model.row_count() > count)
      model.erase(model.row_count() - 1);
  }
};

} // namespace bike
