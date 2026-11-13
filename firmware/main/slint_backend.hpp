#pragma once
#include "board.hpp"
#include <cstdint>
#include <functional>

void init_slint_backend(board::Display display);

void render_slint_frame();

// UI task only. Observe valid touch contacts before normal pointer dispatch.
void set_slint_contact_observer(std::function<void(bool)> observer);

void set_slint_orientation(bike::Orientation orientation); // UI task, between frames

void set_slint_sleeping(bool asleep); // Wake contact is swallowed until release.

bool take_slint_wake();

void cancel_slint_input(); // Deferred: safe to request from a UI callback.

uint64_t slint_last_activity_ms();

struct RenderStats {
  uint64_t frames = 0;
  uint32_t frame_us = 0, wait_us = 0, max_frame_us = 0, chunks = 0, pixels = 0;
};

RenderStats slint_render_stats();

uint32_t slint_loop_delay_ms(uint32_t iteration_us);
