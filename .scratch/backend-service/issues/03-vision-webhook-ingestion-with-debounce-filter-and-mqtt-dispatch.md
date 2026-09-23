# 03: Vision Webhook Ingestion with Debounce Filter and MQTT Dispatch

**What to build:** An authenticated external webhook ingestion endpoint and event dispatch pipeline for shape detection events. Exposes `POST /api/v1/detections` secured with Bearer token authorization (`VISION_BEARER_TOKEN`). Maps incoming shape names (`circle`, `triangle`, `square`) and numeric identifiers (1, 2, 3) to internal constants. Evaluates each detection against an in-memory, thread-safe sliding window debounce cache: detections occurring within 2 seconds for an identical shape return HTTP 200 with status `debounced` and are dropped. Valid, non-debounced shape detections generate an atomic detection ID and publish an MQTT payload to HiveMQ topic `factory/detections` formatted as `{"shape_id": <int>, "shape_name": "<str>", "detection_id": <int>}` for relay to Board A.

**Blocked by:** 01: Project Scaffolding, Database Migrations, and Health Probe

**Status:** ready-for-human

- [x] HTTP endpoint `POST /api/v1/detections` rejects unauthorized requests without a valid `Authorization: Bearer <TOKEN>` header (HTTP 401).
- [x] Accepts JSON payloads with shape names (`circle`, `triangle`, `square`) or shape IDs (1, 2, 3).
- [x] Thread-safe in-memory debounce cache (`sync.Mutex` and timestamp tracking) dropping duplicate detections for the same shape occurring within 2 seconds.
- [x] Debounced duplicate requests return HTTP 200 OK with `{"status": "debounced", "shape_id": <int>, "message": "Duplicate detection dropped within 2s debounce window"}`.
- [x] Valid detections generate an incrementing or timestamped `detection_id` and publish to HiveMQ topic `factory/detections` matching Board A's parser.
- [x] Returns HTTP 200 OK with `{"status": "dispatched", "shape_id": <int>, "detection_id": <int>}` on successful processing.
- [x] Unit tests verify token authentication, shape mapping, 2-second debounce sliding window, and MQTT message serialization.

## Comments

- Implemented `DetectionService` in `internal/service/detection.go` supporting flexible shape normalization (`shape_id`, `shape_name`, or `shape`), thread-safe sliding window debounce filtering per shape (2-second duration), atomic monotonic `detection_id` generation, and MQTT publishing to `factory/detections`.
- Implemented `BearerAuthMiddleware` in `internal/api/middleware.go` with timing-safe `subtle.ConstantTimeCompare` verification against `VISION_BEARER_TOKEN`.
- Implemented `DetectionHandler` in `internal/api/detection_handler.go` and mounted under `/api/v1/detections` in `internal/api/router.go`.
- Added `TopicDetections` and `ShapeDetectionMessage` in `internal/mqtt/types.go` matching Board A's parser in `auth_protocol.cpp`.
- Initialized `DetectionService` and wired it into `api.NewRouter` in `cmd/server/main.go`.
- Unit tests written and verified for service logic, debounce timing, middleware authentication, handler status codes, and router dispatching with `-race`.
