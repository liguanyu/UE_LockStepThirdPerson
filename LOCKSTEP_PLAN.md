# Lockstep Frame Sync Plan (Custom, Non-UE Replication)

## Summary
- Use a centralized C/S topology where the server only aggregates and broadcasts input frames; no authoritative state simulation.
- Fixed-step simulation with input delay buffering; all clients advance the same frame only when all inputs are present.
- Keep existing physics (non-deterministic); no rollback or hard correction in v1. Optional lightweight desync logging only if cheap.

## Important Interfaces / Types
- `ULockstepSubsystem` (GameInstanceSubsystem)
  - Session lifecycle, network connection, frame progression, input buffer management.
- `ULockstepSimulationDriver` (WorldSubsystem or Actor)
  - Fixed-step tick; drives per-frame input application and gameplay simulation.
- `ULockstepInputCollector`
  - Collects Enhanced Input each render frame and writes into future frame buffer.
- `FLockstepInputFrame`
  - `FrameIndex`, `PlayerId`, `MoveAxis`, `LookAxis`, `JumpPressed`, `ActionBits`, `Timestamp`.
- `FLockstepPacket`
  - Types: `HELLO`, `WELCOME`, `READY`, `START`, `INPUT`, `INPUT_BUNDLE`, `PING`, `PONG`.
  - Fields: `SessionId`, `ClientId`, `FrameIndexRange`, optional `Ack`.

## Design Details

### 1) Session & Connection
- Client sends `HELLO`.
- Server replies `WELCOME` with `ClientId`, `StartFrame`, `Seed`, `FixedFPS`.
- Client sends `READY` after setup.
- Server broadcasts `START` (or first `INPUT_BUNDLE`) when all clients are ready.

### 2) Fixed-step & Input Buffer
- Config: `Lockstep.FPS=60` (configurable) and `Lockstep.InputDelayFrames=3` (configurable).
- Each render frame:
  - Sample local input and write into `LocalInputBuffer[FrameIndex + InputDelay]`.
- Server aggregates inputs for each frame and broadcasts `INPUT_BUNDLE` once all inputs for that frame are present.
- Clients advance the simulation only when they have all players’ inputs for the next frame.

### 3) Input Application Point
- Do not drive gameplay with raw Enhanced Input events.
- Only use lockstep input in fixed `SimTick`.
- `BP_ThirdPersonCharacter` should expose `ApplyInputFrame(FLockstepInputFrame)`.
- `BP_ThirdPersonPlayerController` should push Enhanced Input into `ULockstepInputCollector`, not into movement directly.

### 4) Networking (Custom, Non-UE Replication)
- Use `Sockets` module directly (UDP preferred).
- `INPUT` messages from clients to server; `INPUT_BUNDLE` from server to all clients.
- Optional simple ACK/bitmask for loss recovery, otherwise keep minimal.

### 5) Non-deterministic Physics Handling
- Keep current physics; do not enforce determinism.
- No rollback or hard correction in v1.
- Optional lightweight desync logging: hash a few key actor states every N frames only if cheap.

### 6) Config Knobs (DefaultGame.ini)
- `Lockstep.FPS=60`
- `Lockstep.InputDelayFrames=3`
- `Lockstep.DebugChecksum=false`
- `Lockstep.MaxPlayers=4`
- `Lockstep.ServerHost=127.0.0.1`
- `Lockstep.ServerPort=7777`

### 7) Blueprint Touch Points (High-level)
- `BP_ThirdPersonPlayerController`:
  - Enhanced Input events -> `ULockstepInputCollector`.
- `BP_ThirdPersonCharacter`:
  - Movement/Jump driven from `ApplyInputFrame` in lockstep tick.
- Level:
  - Place `ALockstepDriver` actor (or auto-spawn) to ensure simulation ticks.

## Test Scenarios
1. Local 2 clients + 1 server
   - Verify identical frame advancement and input delay.
2. Simulated latency/loss (50–150ms)
   - Ensure frame progression and no deadlock.
3. Disconnect/reconnect
   - Rejoin at latest frame and continue.
4. Interactables (JumpPad/WobbleTarget)
   - Ensure effects trigger correctly under lockstep input.

## Assumptions / Defaults
- Server is relay-only; no anti-cheat.
- Physics stays as-is; consistency not guaranteed.
- UDP with minimal reliability layer (or TCP if simpler).
- Default max players = 4.
