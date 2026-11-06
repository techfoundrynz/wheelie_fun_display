# Wheelie Fun Display theme guide

This guide describes the styles used by the Slint firmware and shared simulator. The implementation lives in `ui/theme.slint` and the reusable components in `ui/`.

## Colour

Use a black background with raised dark cards. Reserve colour for live state, selection, warnings and the speed gauge.

| Theme token | Colour | Use |
| --- | --- | --- |
| `bg` | `#000000` | Screen and page backgrounds |
| `surface` | `#1D1D1D` | Cards, rows and panels |
| `raised` | `#2A2A2A` | Raised controls and selected surfaces |
| `sunken` | `#141414` | Recessed or disabled surfaces |
| `control` | `#444444` | Control borders and value fields |
| `track` | `#555555` | Gauge and progress tracks |
| `accent` | `#69D2DF` | Selection, active icons and battery fill |
| `text` | `#FFFFFF` | Main text and values |
| `secondary` | `#BBBBBB` | Secondary labels and units |
| `muted` | `#AAAAAA` | Inactive labels |
| `dim` | `#808080` | Unselected settings cog and subdued details |
| `danger` | `#FF4444` | Faults and overheating |
| `warning` | `#FFC300` | Pending or cautionary state |
| `gauge` | `#FFD700` | Speed arc |
| `eco` | `#198E13` | Eco preset |
| `cruise` | `#0E77AC` | Standard preset |
| `sport` | `#AC0E0E` | Sport preset |

The battery bar uses dark text on cyan. Other cards keep light text on dark surfaces. Never use colour alone to communicate a fault or unavailable action.

## Typography

Use the bundled Roboto font with a clear weight hierarchy:

| Weight | Use |
| --- | --- |
| 500 | Body text, supporting values and units |
| 700 | Buttons, section labels and the connection header |
| 800 | Page headings and prominent numeric values |

Keep literal font weights in Slint so the embedded compiler includes the required faces. The firmware font plan includes 14, 16, 18, 20, 24, 28, 36, 48, 60, 72 and 84 px bitmaps. Large sizes use numeric glyph coverage for the speed readout. The browser simulator reads the same font plan.

The connection header uses an 18 px label with a 10 px state dot. A fresh controller fault or temperature warning takes priority over the connection label and uses the danger colour. Otherwise show the most specific connection/probe/telemetry state; a phone reserving the link is an expected operational state. Ignore expired fault text, and keep an unpaired display labelled Disconnected rather than replacing it with a generic telemetry message. Long header text may wrap within the fixed 48 px row, while the Settings cog keeps its own hit area. Prominent page titles use 24 or 28 px. Metric values reduce to 18 px in compact landscape layouts so large readings retain their units. The speed readout scales with the gauge diameter, with units beneath it.

Use `°` for angles and `°C` / `°F` for temperatures. Speed and temperature units have independent preferences.

## Header and navigation

The top row contains connection state on the left and the settings cog on the right. Vertically centre both controls. Bound long status text to the available width and elide it before it overlaps the cog.

The header has no vehicle wordmark or fallback brand heading. Integration identity remains available on the Bike page; its verified capabilities determine the available controls.

The bottom navigation contains Ride, Tune and Bike. Use cyan for the active item and a quieter surface for inactive items. Primary-page swipes keep the header and navigation stationary.

## Cards and controls

Use shared `Action`, `IconAction`, `Metric`, `SettingsRow` and `InfoRow` components. Prefer consistent padding and spacing over per-page approximations.

- Button outlines are 2 px, using `Theme.control-border`.
- Compact metric cards use 8 px padding; larger cards retain their standard padding.
- A settings row has a clear label, current value and a chevron icon.
- Disabled controls use subdued text and borders and reject pointer input.
- Ride presets, limit steppers and Apply actions are unavailable without a connected bike. Sending changes also requires fresh, stationary telemetry and the normal access checks.

## Speed gauge

Use the gold speed arc over a dark grey track. The stroke scales from a 16 px reference at a 260 px diameter. The speed digits scale to 28% of the diameter and stay centred above the unit label.

Portrait layouts use the space recovered from the single-row connection header for a larger gauge, with the battery meter below it. Anchor metric cards above the bottom navigation. Keep status messages in the header; do not add a second status line to the Ride body. Do not solve a clipped power value by removing its unit or shortening it to an ellipsis.

## Responsive layout and touch

The supported rotations are 0°, 90°, 180° and 270°. Portrait and landscape layouts share components and theme tokens. A rotation cancels the current gesture and requires a new contact.

Read the panel resolution and safe inset from the hardware profile. Keep header controls, cards, dialogs, keyboard keys and PIN entry inside `SafeArea`. Rounded panels need additional corner clearance; rectangular panels can use the smaller inset defined by their profile.

Check both supported display sizes in both aspect ratios: 410 × 502, 502 × 410, 480 × 800 and 800 × 480. Include long connection text, large power values, independent units, scrolling, PIN entry and keyboard editing.

## Motion and overlays

Primary page transitions use the shared page deck and a 250 ms easing curve. Swipes follow the finger, snap back when cancelled and stop at the first and last page. Vertical scrolling must remain distinct from horizontal navigation.

Dialogs and PIN entry block the controls underneath. A sleeping locked screen is black. The wake contact restores the configured brightness and is consumed before another action is accepted.

## Documentation and web tools

The documentation site, simulator shell and firmware installer share the stylesheet, Roboto font, navigation and design tokens in `docs/site/`. Their app branding is **Wheelie Fun Display**. The shared bike artwork in `docs/site/bike.svg` depicts a 1970s-style minibike. Its orientation flips the bike horizontally, then rotates it 30° clockwise; headers and the favicon use the same oriented geometry.

Documentation screenshots use deterministic, connected-looking values generated during the Pages build. They are illustrative previews; the interactive simulator identifies its hardware and services as simulated outside the display canvas.

Keep third-party font, icon and artwork notices with the relevant assets. This guide does not change their licenses.
