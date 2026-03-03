# Lockstep 多玩家创建与输入路由方案（中文记录）

## 目标
- 每个客户端只操作自己的角色。
- 通过帧同步广播输入后，所有客户端都把“某个 `playerId` 的输入”应用到“同一个 `playerId` 对应的 Actor”。
- 支持第三个玩家在开局前加入并被所有客户端创建出来。

## 核心问题（当前实现）
- 已有 `INPUT_BUNDLE` 汇聚与广播，但缺少 `playerId -> Actor` 映射层。
- 缺少玩家创建协议，因此第三玩家无法在所有客户端稳定创建。

## 协议扩展
- 新增消息类型：
  - `JoinRequest`：客户端请求加入房间。
  - `JoinAccept`：服务端同意/拒绝加入。
  - `PlayerSpawn`：服务端广播玩家创建参数（`playerId` + 出生点）。
  - `PlayerSpawnAck`：客户端确认该玩家已创建。
  - `RoomClosed`：房间结束/关闭通知。
- 新增玩家描述结构：`FLockstepPlayerDesc`
  - `PlayerId`
  - `PawnTypeId`
  - `SpawnLocation`
  - `SpawnRotation`

## Server 行为
- `HELLO` 仅分配连接身份。
- 收到 `JoinRequest` 后：
  - 未开局：分配玩家并广播 `PlayerSpawn`（包含当前玩家列表）。
  - 已开局：回 `JoinAccept` 拒绝。
- 收到 `PlayerSpawnAck`：记录该客户端已完成哪些玩家创建。
- 开局条件：
  - 所有参与玩家 `READY`。
  - 每个参与客户端都已 Ack 全部玩家创建。
- 运行中：
  - 按帧收齐所有参与玩家输入后广播 `INPUT_BUNDLE`。

## UE 客户端行为
- `ULockstepSubsystem`：
  - 发送 `JoinRequest`、`PlayerSpawnAck`。
  - 处理 `PlayerSpawn` 并触发本地创建。
- 新增 `ULockstepPlayerRegistrySubsystem`：
  - 管理 `playerId -> Actor` 映射。
  - 提供创建、注册、查询接口。
- 固定帧驱动中按输入逐条路由：
  - `for frameInput in Inputs`：查 `playerId` 对应 Actor。
  - 调用 Actor 的 `ApplyLockstepInput(frameInput)`。

## 约束
- 仅支持开局前加入。
- 断线策略先保持简单（V1 不做复杂重连）。
- 不使用 UE Replication。
