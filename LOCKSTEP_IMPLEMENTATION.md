# Lockstep Implementation Notes

## What was added
- UE runtime module at `Source/LockStepFps`.
- Standalone relay server at `Tools/LockstepRelayServer`.
- Lockstep defaults in `Config/DefaultGame.ini` under `[Lockstep]`.

## Relay server build/run
```bash
cd Tools/LockstepRelayServer
cmake -S . -B build
cmake --build build --config Release
./build/LockstepRelayServer --host 0.0.0.0 --port 7777 --max-players 4 --fps 60
```

On Windows with multi-config generators:
```bash
cmake --build build --config Release
./build/Release/LockstepRelayServer.exe --host 0.0.0.0 --port 7777 --max-players 4 --fps 60
```

## UE side usage
1. Add `ULockstepInputCollectorComponent` to your player controller or pawn.
2. Route Enhanced Input callbacks to:
   - `SetMoveAxis`
   - `SetLookAxis`
   - `SetJumpPressed`
   - `SetActionBits`
3. Use `ULockstepSubsystem::ConnectFromConfig()` (or `Connect(host, port)`).
4. Send handshake packets from game flow:
   - `HELLO` after connect
   - `READY` after local initialization
5. Start fixed simulation via `ULockstepSimulationDriverSubsystem::StartSimulation()`.
6. Bind to `OnSimTick(FrameIndex, Inputs)` and apply inputs to your character logic inside that callback.

## Current scope
- Relay server only aggregates and broadcasts input frames.
- No rollback.
- No server-authoritative state.
- No anti-cheat.
