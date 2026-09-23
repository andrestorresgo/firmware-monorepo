# 04: Shape Telemetry Cards with 3-Bit Binary Indicators and Rollover

**What to build:** The geometric shape telemetry cards visually mirroring the 3 physical LED banks on Board B. Renders 3 responsive cards for Circle, Triangle, and Square using default shadcn Card styling with strictly semantic color tokens (`destructive` for Circle/Red, `primary` for Triangle/Green, `secondary`/`accent` for Square/Blue) and Lucide geometric icons. Each card displays a 3-bit Binary Bit Indicator with 3 pills arranged MSB-to-LSB ($2^2, 2^1, 2^0$), active when the bit is 1 and muted when 0. Renders the live buffer decimal count ($0$ to $5$) and the persistent database lifetime total (`total_lifetime`). Dynamically updates both live bits and cumulative totals in real-time as shape detection events and batch rollovers occur over the telemetry stream.

**Blocked by:** 03: Hybrid Telemetry Engine and Machine Pause Safety Banner

**Status:** ready-for-agent

- [ ] Build 3-bit `BinaryBitIndicator` component rendering 3 pills ($2^2, 2^1, 2^0$) with semantic active/muted states.
- [ ] Build `ShapeCard` component using standard shadcn `Card`, `Badge`, and Lucide icons (`Circle`, `Triangle`, `Square`).
- [ ] Map shapes to default tokens: Circle -> `destructive`, Triangle -> `primary` (emerald), Square -> `secondary`/`accent`.
- [ ] Display live buffer decimal count ($0$ to $5$) and cumulative lifetime total (`total_lifetime`) on each card.
- [ ] Update live bits immediately when `factory/telemetry` frames (`red_count`, `green_count`, `blue_count`) arrive.
- [ ] Increment lifetime total and reset live buffer when `factory/rollover` batch events occur.
- [ ] Verification tests for binary bit decomposition logic and real-time counter updates.

## Comments
