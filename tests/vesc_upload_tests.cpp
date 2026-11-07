#include "bike/can_packet.hpp"
#include "bike/vesc_upload.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

#define CHECK(c)                                                                                                       \
  do {                                                                                                                 \
    if (!(c)) {                                                                                                        \
      std::cerr << __LINE__ << ": " #c "\n";                                                                           \
      std::exit(1);                                                                                                    \
    }                                                                                                                  \
  } while (0)

struct Storage : bike::UpdateStorage {
  std::vector<uint8_t> bytes;
  int writes = 0, commits = 0, aborts = 0;
  bool fail_write = false, fail_read = false, fail_commit = false;

  size_t capacity() const override {
    return 2048;
  }

  bool begin(size_t size) override {
    bytes.assign(size, 255);
    return true;
  }

  bool write(size_t at, std::span<const uint8_t> value) override {
    if (fail_write || at + value.size() > bytes.size())
      return false;
    ++writes;
    std::copy(value.begin(), value.end(), bytes.begin() + at);
    return true;
  }

  bool read(size_t at, std::span<uint8_t> value) override {
    if (fail_read || at + value.size() > bytes.size())
      return false;
    std::copy_n(bytes.begin() + at, value.size(), value.begin());
    return true;
  }

  bool finish() override {
    if (fail_commit)
      return false;
    ++commits;
    return true;
  }

  void abort() override {
    ++aborts;
  }
};

std::vector<uint8_t> wire(std::span<const uint8_t> image) {
  const auto crc = bike::crc16(image);
  std::vector<uint8_t> result{uint8_t(image.size() >> 24), uint8_t(image.size() >> 16), uint8_t(image.size() >> 8),
                              uint8_t(image.size()),       uint8_t(crc >> 8),           uint8_t(crc)};
  result.insert(result.end(), image.begin(), image.end());
  return result;
}

int main() {
  Storage storage;
  bike::VescUpload upload(storage);
  std::vector<uint8_t> image(1024, 255);
  image[0] = 0xe9;
  image[400] = 0x42;
  auto packet = wire(image);
  CHECK(!upload.begin(0));
  CHECK(!upload.begin(2049));
  CHECK(!upload.write(0, packet));
  CHECK(!upload.finish());
  CHECK(upload.begin(image.size()));
  CHECK(!upload.write(6, std::span(packet).subspan(6, 10)));
  CHECK(!upload.write(0, std::span(packet).first(5)));
  CHECK(upload.write(0, std::span(packet).first(390)));
  CHECK(upload.write(0, std::span(packet).first(390)));
  CHECK(storage.writes == 1); // Lost ACK must not cause another flash write.
  auto changed = packet;
  changed[20] ^= 1;
  CHECK(!upload.write(0, std::span(changed).first(390)));
  CHECK(!upload.write(3, std::span(packet).first(4)));
  CHECK(!upload.write(1030, std::span(packet).first(1)));
  CHECK(upload.write(406, std::span(packet).subspan(406, 10)));
  CHECK(upload.finish()); // Sparse and trailing FF chunks are legal.
  CHECK(storage.bytes == image && storage.commits == 1 && !upload.active());

  CHECK(upload.begin(image.size()));
  CHECK(upload.write(0, std::span(packet).first(390)));
  CHECK(!upload.finish()); // Non-FF bytes are missing.
  CHECK(storage.commits == 1 && !upload.active());

  for (int failure = 0; failure < 4; ++failure) {
    CHECK(upload.begin(image.size()));
    storage.fail_write = failure == 0;
    const bool wrote = upload.write(0, packet);
    CHECK(wrote == (failure != 0));
    storage.fail_write = false;
    if (!failure) {
      upload.abort();
      continue;
    }
    if (failure == 1)
      storage.bytes[25] ^= 1;
    storage.fail_read = failure == 2;
    storage.fail_commit = failure == 3;
    CHECK(!upload.finish());
    CHECK(!upload.active());
    CHECK(storage.commits == 1);
    storage.fail_read = storage.fail_commit = false;
  }
  CHECK(upload.begin(image.size()));
  CHECK(upload.write(0, packet));
  upload.abort();
  CHECK(!upload.finish());
  CHECK(storage.commits == 1);
  CHECK(upload.begin(packet.size()));
  CHECK(upload.write(0, packet));
  CHECK(upload.finish());

  for (size_t length : {size_t(1), size_t(5), size_t(390), size_t(1024)}) {
    std::vector<uint8_t> command(length, 3);
    command[0] = length == 1 ? 0 : 3;
    bike::CanReassembler endpoint, replies;
    std::vector<bike::CanFrame> frames;
    CHECK(bike::send_can_packet(command, 12, 254, false, [&](const auto &f) {
      frames.push_back(f);
      return true;
    }));
    std::span<const uint8_t> received;
    for (const auto &frame : frames) {
      CHECK(replies.feed(frame, 254, -1, 10).empty());
      const auto value = endpoint.feed(frame, 254, -1, 10, false);
      if (!value.empty())
        received = value;
    }
    CHECK(std::equal(command.begin(), command.end(), received.begin(), received.end()));
    CHECK(endpoint.sender() == 12);
    frames.clear();
    received = {};
    CHECK(bike::send_can_packet(command, 254, 12, true, [&](const auto &f) {
      frames.push_back(f);
      return true;
    }));
    for (const auto &frame : frames) {
      const auto value = replies.feed(frame, 12, 254, 20);
      if (!value.empty())
        received = value;
    }
    CHECK(std::equal(command.begin(), command.end(), received.begin(), received.end()));
  }
  std::cout << "VESC upload integrity, retries and native CAN requests passed\n";
}
