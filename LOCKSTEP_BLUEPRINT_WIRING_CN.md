# Lockstep 蓝图接线清单（中文）

这份清单只回答一件事：基于当前已经实现的 C++ lockstep 框架，你还需要在 UE 蓝图里做哪些接线，才能让“每个客户端操作自己的角色，但所有客户端都能同步看到其他玩家角色的动作”真正跑起来。

本文按实际操作顺序写，并且每一步都说明原因。

## 1. 总体目标

你最终要达成的是下面这条链路：

1. 本地玩家操作键盘/鼠标。
2. Enhanced Input 不再直接驱动角色，而是先写入 `ULockstepInputCollectorComponent`。
3. 当前客户端把“未来某一帧的输入”发给 relay server。
4. server 收齐该帧所有玩家输入后，广播 `INPUT_BUNDLE`。
5. 每个客户端收到后，在固定逻辑帧里逐条取出输入。
6. 根据 `playerId` 找到对应 Actor。
7. 把该输入应用到对应 Actor。

如果中间某一步没接上，就会出现下面几种典型问题：

- 本地自己能动，但远端看不到：说明你还是在走旧的本地输入直驱逻辑。
- 所有人输入都作用到同一个角色：说明没有按 `playerId -> Actor` 做路由。
- 收到了输入但角色不动：说明角色没有实现 `ApplyLockstepInput`。

---

## 2. 需要重点用到的 C++ 类

接线前先明确这几个类分别干什么：

1. `ULockstepInputCollectorComponent`
路径：[LockstepInputCollectorComponent.h](/mnt/g/ue_proj/LockStepFps/Source/LockStepFps/Public/LockstepInputCollectorComponent.h)

作用：
- 暂存本地玩家当前采样到的输入值。
- 供后续构造 `FLockstepInputFrame` 使用。

2. `ULockstepSubsystem`
路径：[LockstepSubsystem.h](/mnt/g/ue_proj/LockStepFps/Source/LockStepFps/Public/LockstepSubsystem.h)

作用：
- 连接 relay server。
- 发 `HELLO / JoinRequest / Ready / Input / PlayerSpawnAck`。
- 收 `WELCOME / PlayerSpawn / START / INPUT_BUNDLE`。

3. `ULockstepSimulationDriverSubsystem`
路径：[LockstepSimulationDriverSubsystem.h](/mnt/g/ue_proj/LockStepFps/Source/LockStepFps/Public/LockstepSimulationDriverSubsystem.h)

作用：
- 用固定帧率推进逻辑帧。
- 在每个逻辑帧把该帧输入逐条路由到对应 Actor。

4. `ULockstepPlayerRegistrySubsystem`
路径：[LockstepPlayerRegistrySubsystem.h](/mnt/g/ue_proj/LockStepFps/Source/LockStepFps/Public/LockstepPlayerRegistrySubsystem.h)

作用：
- 保存 `playerId -> Actor` 映射。
- 本地玩家绑定已有 Pawn。
- 远端玩家按 `PlayerSpawn` 创建新 Actor。

5. `ULockstepControllablePawnInterface`
路径：[LockstepControllablePawnInterface.h](/mnt/g/ue_proj/LockStepFps/Source/LockStepFps/Public/LockstepControllablePawnInterface.h)

作用：
- 定义角色如何接收某个玩家某一帧输入。
- 你需要在角色蓝图里实现 `ApplyLockstepInput`。

---

## 3. `BP_ThirdPersonCharacter` 需要做什么

目标：
- 让角色成为“可被 lockstep 输入驱动的角色”。

### 3.1 给角色实现 `ULockstepControllablePawnInterface`

操作：

1. 打开 `BP_ThirdPersonCharacter`。
2. 在 `Class Settings` 里找到 `Interfaces`。
3. 添加接口：`LockstepControllablePawnInterface`。

原因：

- `ULockstepSimulationDriverSubsystem` 在固定帧里是通过这个接口调用角色的。
- 如果你不加这个接口，代码会找到 Actor，但不会真正执行输入。

---

### 3.2 实现 `ApplyLockstepInput`

操作：

1. 在 `BP_ThirdPersonCharacter` 的 `My Blueprint` 中，找到接口函数 `ApplyLockstepInput`。
2. 打开它的事件图。
3. 从输入参数 `InputFrame` 中取出：
   - `MoveAxis`
   - `LookAxis`
   - `bJumpPressed`
   - `ActionBits`
4. 用这些值驱动角色：
   - `MoveAxis.X / MoveAxis.Y` -> `Add Movement Input`
   - `LookAxis.X / LookAxis.Y` -> `Add Controller Yaw Input` / `Add Controller Pitch Input`
   - `bJumpPressed == true` -> `Jump`
   - 如果你后续定义开火/蹲下/冲刺等，再从 `ActionBits` 解析

原因：

- 这是“server 广播来的输入最终如何落到角色身上”的最后一步。
- 所有玩家角色，包括本地角色和远端角色，最终都应该走这个函数。

注意：

1. 第一版建议只接移动、视角、跳跃。
2. `LookAxis` 是否真的适合直接给远端角色加 controller 输入，要看你当前蓝图结构。
   - 如果当前第三人称角色相机只服务本地玩家，那么远端角色可能不需要完整应用 `LookAxis`。
   - 第一版更稳妥的方式是：远端先只应用移动和跳跃，本地角色再额外应用视角。

---

### 3.3 不要再让角色直接响应本地输入事件

操作：

1. 检查 `BP_ThirdPersonCharacter` 里是否有直接来自 Input 的逻辑：
   - `IA_Move`
   - `IA_Look`
   - `IA_Jump`
2. 如果这些输入事件直接连到了：
   - `Add Movement Input`
   - `Jump`
   - `Add Controller Yaw/Pitch Input`
3. 把这些“直接驱动角色”的连线断开，或者加一个条件开关让它们不再执行。

原因：

- 如果你保留旧逻辑，就会出现“双驱动”：
  - 一份来自本地实时输入
  - 一份来自 lockstep 固定帧输入
- 结果通常是：
  - 本地角色移动变快
  - 跳跃重复触发
  - 本地与远端表现不一致

---

## 4. `BP_ThirdPersonPlayerController` 需要做什么

目标：
- 不再直接控制角色，而是只负责采样本地输入并送入 lockstep 系统。

### 4.1 添加 `ULockstepInputCollectorComponent`

操作：

1. 打开 `BP_ThirdPersonPlayerController`。
2. 添加组件：`LockstepInputCollectorComponent`。
3. 给它一个清晰名字，例如 `LockstepInputCollector`。

原因：

- 这个组件用来存“当前帧采样到的输入值”。
- 后续发包时，输入帧数据要从这里取。

---

### 4.2 把 Enhanced Input 事件改成“写组件”，不要再直接动角色

操作：

1. 找到 `IA_Move` 的回调。
2. 从输入值转成 `Vector2D`，调用组件的 `SetMoveAxis`。
3. 找到 `IA_Look` / `IA_MouseLook` 的回调，调用 `SetLookAxis`。
4. 找到 `IA_Jump`：
   - Pressed -> `SetJumpPressed(true)`
   - Released -> `SetJumpPressed(false)` 或按你的设计决定是否只做单帧触发

原因：

- PlayerController 在 lockstep 模式下不应该“直接控制角色”，只应该“采样输入并缓存”。
- 真正控制角色的时机是固定逻辑帧，而不是输入回调到来的瞬间。

注意：

1. 如果 `Jump` 你希望是“按下一次只触发一帧”，那么你后面需要在构造 `InputFrame` 之后把它清掉。
2. 如果你暂时把 `Jump` 当成“按住为 true”，也能跑，但语义会更接近持续按钮而不是单次触发。

---

### 4.3 在合适时机构造并发送本地输入帧

操作建议：

1. 在 `PlayerController` 的 `Tick` 或者你专门的驱动逻辑中：
   - 获取 `ULockstepSimulationDriverSubsystem` 当前帧号
   - 获取 `ULockstepSubsystem` 的 `ClientId`
   - 从 `LockstepInputCollectorComponent` 调用 `BuildInputFrame(CurrentFrame, ClientId, InputDelayFrames)`
   - 调用 `ULockstepSubsystem::SubmitLocalInputFrame`

原因：

- 目前 C++ 已有：
  - 输入采集组件
  - 提交输入到 server 的接口
- 但“什么时候构造并提交本地输入帧”仍需要你在蓝图里接上。

注意：

1. 不要在 `ApplyLockstepInput` 里反过来提交输入，这会把“收包应用”和“本地采样发送”混在一起。
2. 本地输入发送应发生在本地渲染/采样流程里；输入应用应发生在 lockstep 逻辑帧里。

---

## 5. 游戏启动流程怎么接

目标：
- 客户端进入房间后完成握手、加入、创建玩家、准备、开局。

推荐把这段接在 `BP_ThirdPersonPlayerController` 的 `BeginPlay`，或者专门的大厅蓝图里。

### 5.1 连接 server

操作：

1. `BeginPlay`
2. 获取 `GameInstance`
3. `GetSubsystem(LockstepSubsystem)`
4. 调用 `ConnectFromConfig()`
5. 调用 `SendHello()`

原因：

- `HELLO` 只负责让 server 认识这个网络连接并分配 `ClientId`。

---

### 5.2 收到 `WELCOME` 后申请加入

操作：

1. 绑定 `ULockstepSubsystem::OnPacketReceived`
2. 当 `Packet.Type == Welcome` 时：
   - 调用 `SendJoinRequest()`

原因：

- 现在协议里把“连接身份”和“房间玩家身份”拆开了。
- `HELLO/WELCOME` 是网络连接层。
- `JoinRequest/JoinAccept/PlayerSpawn` 是玩家加入房间层。

---

### 5.3 收到 `PlayerSpawn` 后等待本地创建完成

操作：

1. `OnPacketReceived` 或 `OnPlayerSpawn` 里监听 `PlayerSpawn`
2. C++ 会自动做两件事：
   - 本地玩家绑定已有 Pawn
   - 远端玩家按参数创建 Actor
3. 创建成功后 C++ 会自动发 `PlayerSpawnAck`

原因：

- 这一步你不一定要手动写蓝图逻辑。
- 但你要知道：只有所有客户端都确认“所有玩家都创建完成”，server 才会允许开局。

---

### 5.4 合适时机发送 `Ready`

操作：

1. 当你确认本地已经完成这些准备后，发送 `SendReady()`：
   - 本地 UI 初始化完成
   - 本地玩家 Pawn 存在
   - `PlayerSpawn` 都处理完
2. 推荐在收到所有玩家 `PlayerSpawn` 并延迟一小帧后发 `Ready`

原因：

- `Ready` 表示“我这边可以开始逐帧推进了”。
- 发太早会导致刚开局时角色还没绑好。

---

### 5.5 收到 `START` 后启动逻辑帧

操作：

1. 在 `OnPacketReceived` 里检测 `Packet.Type == Start`
2. 获取 `WorldSubsystem(LockstepSimulationDriverSubsystem)`
3. 调用 `StartSimulation()`

原因：

- 只有收到 `START` 才说明：
  - 全部玩家已加入
  - 全部玩家已创建
  - 全部玩家已 `Ready`
- 从这一刻开始再跑固定逻辑帧，才不会丢帧或乱序。

---

## 6. 本地玩家和远端玩家分别怎么处理

### 6.1 本地玩家

当前实现里：

1. 当收到 `PlayerSpawn`
2. 如果 `PlayerDesc.PlayerId == ClientId`
3. 优先尝试把 `GetFirstPlayerController()->GetPawn()` 注册成这个 `playerId` 对应 Actor

原因：

- 本地角色通常已经由 GameMode 或默认 Pawn 流程创建好了。
- 没必要再额外 Spawn 一份，否则会出现两个本地角色。

你需要做的检查：

1. 本地 Pawn 是否确实在 `PlayerSpawn` 到来时已经存在
2. 如果不存在，是否要改为：
   - 等 Pawn 创建后再注册
   - 或者在蓝图里主动延迟 `Ready`

---

### 6.2 远端玩家

当前实现里：

1. `ULockstepPlayerRegistrySubsystem` 会读取默认类路径：
   - `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter_C`
2. 按 `PlayerSpawn` 的出生点在本地 `SpawnActor`
3. 注册到 `playerId -> Actor`

你需要做的检查：

1. 这个类路径是否与你当前项目真实角色蓝图一致
2. 远端角色是否也实现了 `ApplyLockstepInput`
3. 远端角色是否会被本地摄像机/控制器错误接管

原因：

- 远端玩家的目标是“能被输入驱动地在你本地表现出来”，不是“成为本地可操控角色”。

---

## 7. 最容易出错的点

### 7.1 把 `ClientId` 当成 `PlayerId`

当前实现里两者被设计得接近，但概念上不同：

- `ClientId`：网络连接身份
- `PlayerId`：游戏玩家身份

原因：

- 以后如果你要支持观战、断线重连、AI 接管，这两个字段就必须分开理解。

当前第一版里：

- server 在 `JoinRequest` 成功后给每个 client 分配一个 `playerId`
- 输入最终以 `playerId` 为准路由到角色

---

### 7.2 本地仍保留旧输入链路

症状：

- 本地角色特别灵敏或移动速度翻倍
- 别人看你和你自己看到的轨迹不一样

原因：

- 你同时保留了：
  - 本地直接输入驱动
  - lockstep 固定帧驱动

解决：

- 把本地输入事件改成只写 `LockstepInputCollectorComponent`

---

### 7.3 远端角色没实现接口

症状：

- 日志出现 `Actor for PlayerId=... does not implement lockstep interface`

原因：

- `ULockstepSimulationDriverSubsystem` 找到了 Actor，但这个 Actor 没法接收 lockstep 输入。

解决：

- 确保远端创建出来的角色蓝图就是 `BP_ThirdPersonCharacter`
- 且这个蓝图已经实现 `ULockstepControllablePawnInterface`

---

### 7.4 收到了输入但找不到角色

症状：

- 日志出现 `No actor bound for PlayerId=...`

原因：

- `PlayerSpawn` 没成功
- 本地玩家 Pawn 没注册成功
- `playerId -> Actor` 映射丢了

解决：

1. 检查 `PlayerSpawn` 是否到达
2. 检查本地 Pawn 是否存在于注册时机
3. 检查 `PlayerPawnClassPath` 是否正确

---

## 8. 推荐联调顺序

### 第一步：只测 1 个客户端

目标：

- 确认本地不再走旧输入直驱
- 确认 `ApplyLockstepInput` 可以驱动本地角色

你应该看到：

1. 输入采集正常
2. 输入帧正常发出
3. 固定帧中应用输入

---

### 第二步：测 2 个客户端

目标：

- 确认每个客户端都生成两个角色
- 确认自己的输入只作用于自己的 `playerId`
- 确认对方输入只作用于对方 Actor

重点观察：

1. server 是否广播了完整 `PlayerSpawn`
2. 两边的 `playerId -> Actor` 数量是否一致
3. 是否出现“两个输入都打到同一 Actor”的问题

---

### 第三步：测 3 个客户端

目标：

- 验证第三玩家加入与创建流程完整
- 验证三人输入按帧汇聚后，分别作用于三个人的 Actor

重点观察：

1. 三个客户端是否都收到 3 个玩家描述
2. 三个客户端是否都创建出 3 个角色
3. 第三玩家动作是否能被前两人正确看到

---

## 9. 你现在最先应该做的实际操作

如果你要最短路径把它跑起来，就按这个顺序做：

1. 在 `BP_ThirdPersonCharacter` 实现 `LockstepControllablePawnInterface` 的 `ApplyLockstepInput`
2. 把原来直接驱动角色的输入逻辑断开
3. 在 `BP_ThirdPersonPlayerController` 添加 `LockstepInputCollectorComponent`
4. 把 `IA_Move / IA_Look / IA_Jump` 改成写入采集组件
5. 在 `BeginPlay` 接：
   - `ConnectFromConfig`
   - `SendHello`
6. 在 `OnPacketReceived` 接：
   - `Welcome -> SendJoinRequest`
   - `Start -> StartSimulation`
7. 在合适时机调用 `SendReady`
8. 用 2 个客户端先测

---

## 10. 为什么这套接线能满足你的需求

你要的是：

- 每个客户端只操作自己的角色
- 但别的客户端也能看到该角色动作

这套接线满足它的原因是：

1. 本地输入只由本地玩家产生。
2. server 不广播状态，只广播“某帧每个玩家的输入”。
3. 每个客户端都维护同一份 `playerId -> Actor` 映射。
4. 每条输入都只应用到对应 `playerId` 的角色。

所以最终不是“两个客户端都在操作同一个角色”，而是：

- 每个客户端都在本地模拟整个战场
- 但每个玩家输入只驱动自己那一个角色

这正是 lockstep 多单位/多角色同步的基本形式。



## 11.就照你这张图改，核心原则只有一句：

IA_Move / IA_Look / IA_Jump 这些输入事件，不能再直接连到 Move / Aim / Jump。
它们现在应该只负责“采样输入并写入 LockstepInputCollectorComponent”；真正执行角色动作，要放到 ApplyLockstepInput 里。

你这张图的最小改法

1. 在 BP_ThirdPersonCharacter 里加组件
   添加 LockstepInputCollectorComponent。
2. 在 BP_ThirdPersonCharacter 的 Class Settings 里加接口
   添加 LockstepControllablePawnInterface。
   接口定义在 LockstepControllablePawnInterface.h。
3. 把图里的这些连线断开
   当前你图里是：

- IA_Move -> Move
- IA_Look -> Aim
- IA_MouseLook -> Aim
- IA_Jump -> Jump / StopJumping

这些都要断开。

4. 改成下面这样接

Move 区域：

- EnhancedInputAction IA_Move
    - Triggered 不再连 Move
    - Action Value X/Y 组一个 Vector2D
    - 调 LockstepInputCollectorComponent -> SetMoveAxis
- Event Primary Thumbstick
    - 同样接到 SetMoveAxis

Look 区域：

- EnhancedInputAction IA_Look
    - Action Value X/Y -> SetLookAxis
- EnhancedInputAction IA_MouseLook
    - Action Value X/Y -> SetLookAxis
- Event Secondary Thumbstick
    - Axis X/Y -> SetLookAxis

Jump 区域：

- IA_Jump.Started -> SetJumpPressed(true)
- IA_Jump.Completed -> SetJumpPressed(false)
- Event Touch Jump Start -> SetJumpPressed(true)
- Event Touch Jump End -> SetJumpPressed(false)

也就是说，这个 EventGraph 现在只负责“写输入值”，不负责“执行动作”。

然后新增真正执行动作的地方

在 BP_ThirdPersonCharacter 里实现接口函数 ApplyLockstepInput：

1. Break FLockstepInputFrame
2. 用 MoveAxis.X / MoveAxis.Y 去调用你现有的 Move 函数
3. 用 LookAxis.X / LookAxis.Y 去调用你现有的 Aim 函数
4. bJumpPressed == true 时调用 Jump
5. bJumpPressed == false 时调用 StopJumping

也就是把你图里原来输入事件后面那套 Move / Aim / Jump 逻辑，整体搬到 ApplyLockstepInput 里。

为什么要这么改

因为现在逻辑应该变成：

- 输入事件：只采样
- 网络层：发给 server，server 汇总后广播
- 固定帧：收到 INPUT_BUNDLE
- ULockstepSimulationDriverSubsystem 按 playerId 找到对应 Actor
- 调这个 Actor 的 ApplyLockstepInput

这样才是“每个人只操作自己的角色，但所有客户端都同步模拟”。

还有一个你别漏掉的点

只改这张图还不够，你还要在别处把本地输入帧发出去。最少还需要一段逻辑：

- 从 LockstepInputCollectorComponent 调 BuildInputFrame(...)
- 调 ULockstepSubsystem::SubmitLocalInputFrame(...)

否则你只是把输入存起来了，没有发给 server。

如果你愿意，我下一步可以直接按你这张图的布局，给你写一份“节点到节点怎么连”的超具体版本，比如：

- 左上 IA_Look 那块该删哪根线
- 换成哪些节点
- ApplyLockstepInput 里面具体放哪些节点
