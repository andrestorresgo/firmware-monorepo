# 04: Shape Telemetry Cards with 3-Bit Binary Indicators and Rollover

**What to build:** The geometric shape telemetry cards visually mirroring the 3 physical LED banks on Board B. Renders 3 responsive cards for Circle, Triangle, and Square using default shadcn Card styling with strictly semantic color tokens (`destructive` for Circle/Red, `primary` for Triangle/Green, `secondary`/`accent` for Square/Blue) and Lucide geometric icons. Each card displays a 3-bit Binary Bit Indicator with 3 pills arranged MSB-to-LSB ($2^2, 2^1, 2^0$), active when the bit is 1 and muted when 0. Renders the live buffer decimal count ($0$ to $5$) and the persistent database lifetime total (`total_lifetime`). Dynamically updates both live bits and cumulative totals in real-time as shape detection events and batch rollovers occur over the telemetry stream.

**Blocked by:** 03: Hybrid Telemetry Engine and Machine Pause Safety Banner

**Status:** closed

- [x] Build 3-bit `BinaryBitIndicator` component rendering 3 pills ($2^2, 2^1, 2^0$) with semantic active/muted states.
- [x] Build `ShapeCard` component using standard shadcn `Card`, `Badge`, and Lucide icons (`Circle`, `Triangle`, `Square`).
- [x] Map shapes to default tokens: Circle -> `destructive`, Triangle -> `primary` (emerald), Square -> `secondary`/`accent`.
- [x] Display live buffer decimal count ($0$ to $5$) and cumulative lifetime total (`total_lifetime`) on each card.
- [x] Update live bits immediately when `factory/telemetry` frames (`red_count`, `green_count`, `blue_count`) arrive.
- [x] Increment lifetime total and reset live buffer when `factory/rollover` batch events occur.
- [x] Verification tests for binary bit decomposition logic and real-time counter updates.

## Comments
- Implemented `getThreeBitDecomposition` pure function in `src/lib/binary.ts` decomposing buffer counts (0–5) into 3 bits arranged MSB-to-LSB ($2^2, 2^1, 2^0$) with extensive unit test coverage.
- Built `BinaryBitIndicator` rendering 3 pills with semantic active/muted states matching Board B's physical LED banks.
- Built `ShapeCard` with standard shadcn `Card`, `Badge`, Lucide icons, capacity progress bar, Observation Delay hold indicator, and cumulative lifetime total tracking.
- Built `ShapeTelemetryGrid` rendering the responsive 3-column layout integrated into `AuthenticatedWorkspace` in `App.tsx`.
- Verified across 76 passing tests in `dashboard-ui`.
