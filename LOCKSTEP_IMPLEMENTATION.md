# Lockstep 实现说明

## 已新增内容
- UE 运行时模块：`Source/LockStepFps`。
- 独立 relay server：`Tools/LockstepRelayServer`。
- Lockstep 默认配置：`Config/DefaultGame.ini` 下的 `[Lockstep]`。

## Relay server 构建与运行
```bash
cd Tools/LockstepRelayServer
cmake -S . -B build
cmake --build build --config Release
./build/LockstepRelayServer --host 0.0.0.0 --port 7777 --max-players 4 --fps 60
```

Windows 多配置生成器下：
```bash
cmake --build build --config Release
./build/Release/LockstepRelayServer.exe --host 0.0.0.0 --port 7777 --max-players 4 --fps 60
```

## UE 侧使用方式
1. 在 `PlayerController` 或 `Pawn` 上添加 `ULockstepInputCollectorComponent`。
2. 将 Enhanced Input 回调接到：
   - `SetMoveAxis`
   - `SetLookAxis`
   - `SetJumpPressed`
   - `SetActionBits`
3. 调用 `ULockstepSubsystem::ConnectFromConfig()`（或 `Connect(host, port)`）。
4. 在游戏流程中发送握手包：
   - 连接后发送 `HELLO`
   - 本地初始化完成后发送 `READY`
5. 通过 `ULockstepSimulationDriverSubsystem::StartSimulation()` 启动固定步长仿真。
6. 绑定 `OnSimTick(FrameIndex, Inputs)`，在该回调中将输入应用到角色逻辑。

## 当前范围
- Relay server 仅做输入帧汇聚与广播。
- 不做回滚。
- 不做服务器权威状态。
- 不做反作弊。
