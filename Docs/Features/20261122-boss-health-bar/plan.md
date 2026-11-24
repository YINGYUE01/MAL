# boss-health-bar — 实施计划

关联规格：[spec.md](spec.md)

## 1. 现状（侦察结论，每条带证据）

| 项 | 结论 | 证据 |
| --- | --- | --- |
| 现有入口 | HUD 由 `AMALPlayerController::BeginPlay` 在本地控制器上调用 `UMALUISubsystem::CreateHUD(HUDWidgetClass)` 创建；HUD 当前只有 Stat / AbilityBar / Progression 三个 ViewModel，没有 Boss 血条容器或 Boss 条 C++ 类。 | `MALPlayerController.cpp:65-97`、`MALHUDWidget.h:33-45`、`MALHUDWidget.h:95-102`、`MALUISubsystem.cpp:13-38` |
| 数据来源 | Boss 的 Health / MaxHealth 在敌人自己的 `UMALAttributeSet`（敌人 ASC 是角色上的子对象，`ReplicatedUsing`）；`UMALStatViewModel` 已能把任意 ASC 的 Health/MaxHealth 转成 `HealthPercent`、`DisplayedHealthPercent`、`LaggedHealthPercent`。 | `MALEnemyCharacter.cpp:8-19`、`MALAttributeSet.h:77-82`、`MALStatViewModel.h:53-82`、`MALStatViewModel.cpp:73-112` |
| 状态归属 | 目标/脱战在 `UMALBossCombatComponent`：`CurrentTarget` 是 private `TWeakObjectPtr`，`OnTargetChanged` 只在 `SetCurrentTarget/ClearCurrentTarget` 广播；现有 `TargetLostGraceTime` 是遮挡宽限 2s，不是规格要求的 8s 脱战。 | `MALBossCombatComponent.h:361-387`、`MALBossCombatComponent.h:493-513`、`MALBossCombatComponent.h:704-781`、`MALBossCombatComponent.cpp:175-221`、`MALBossCombatComponent.cpp:1651-1685`、`MALBossCombatConfig.h:212-219` |
| 权威侧 | 首次伤害、目标丢失、死亡都在服务器；`UMALCombatEventsSubsystem` 只在非客户端绑定 `OnDamageResolved`，而且目前只把伤害数字发给受害者/攻击者 PC，不广播给所有客户端。 | `MALCombatEventsSubsystem.cpp:21-40`、`MALCombatEventsSubsystem.cpp:42-86`、`MALBossCombatComponent.cpp:175-183`、`MALCharacterBase.cpp:630-720` |
| 可复用点 | `UMALStatViewModel`（血条 + 残影 + ASC 绑定）、`UMALUserWidget`（`SetViewModelByClass` / `Show`）、`UMALHUDWidget` 的锁屏绑定模式、`FMALRetryPoll`、`AMALPlayerController` 的 Client RPC、`UMALCombatEventsSubsystem::OnDamageResolved`。 | `MALStatViewModel.h:53-82`、`MALUserWidget.h:36-66`、`MALHUDWidget.cpp:34-45`、`MALPlayerController.h:42-52`、`MALCombatEvents.h:22-48` |
| 相关资产 | 现有 `WBP_MainHUD`（C++ 无 Boss 条容器）、`WBP_EnemyHealthBar`（普通敌人头顶条）、`BP_BoosCombat`、`DA_CombatConfig`；没有 Boss 专属条资产。 | `Content/Blueprint/UI_1/WBP_MainHUD.uasset`、`Content/Blueprint/UI_1/WBP_EnemyHealthBar.uasset`、`Content/Blueprint/AI/Boss/BP_BoosCombat.uasset`、`Content/Blueprint/AI/Boss/DA_CombatConfig.uasset` |
| 相关历史 | 血条 UI 初始提交 `281ef8b 添加血条UI、伤害显示UI、Character死亡事件`；UI 最近 `0d47a43 优化Tag、技能资产`；Boss 战斗 `69dbb4d 实现BoosCombatComponent` → `dd6e6d1 完成第一阶段` → `5b4aa78 完成阶段2` → `31bb84d 完成阶段3`。 | `git log --oneline -- <path>` |
| 改动面初判 | C++ 15 个文件（正好门禁上限）；资产 4 个（1 新建 + 3 编辑）：`WBP_MainHUD`、新建 `WBP_BossBarItem`、`DA_CombatConfig`、`BP_BoosCombat`；另有验证期临时改值（`BossDisplayName`、Elite/Normal tag），不新增资产。 | 见 §3 |
| 未知项 | 无阻塞未知；3 个实现阶段确认项：`WBP_MainHUD` 根节点能否直接加 `BossBarList`、新 `WBP_BossBarItem` 的 View Binding 源名、Elite/Normal 测试 tag 的临时设置。 | 见 §1.2 |

### 1.1 必答清单补充（对应 02-recon §4）

| # | 问题 | 结论 | 证据 |
| --- | --- | --- | --- |
| 1 | 谁依赖它 / 改谁 | `AMALPlayerController` 拥有本地 HUD 与 Client RPC；`UMALHUDWidget` 是持久 HUD；`AMALEnemyCharacter` 提供 ASC、头顶条和 Boss 身份；`UMALBossCombatComponent` 提供目标状态；当前没有 Boss 条消费者。 | `MALPlayerController.h:209-211`、`MALHUDWidget.h:33-45`、`MALEnemyCharacter.h:34-48`、`MALBossCombatComponent.h:316-333` |
| 2 | 同类实现 | `UMALWorldWidgetComponent` 用 `UMALStatViewModel` 绑敌人 ASC，并用 `FMALRetryPoll` 处理 ASC 晚到；Boss 条可复用同一个 ViewModel 的 Health / Lagged 输出。 | `MALWorldWidgetComponent.cpp:26-112`、`MALStatViewModel.h:53-82` |
| 3 | 资产与命名契约 | item WBP 父类为 `UMALBossBarItemWidget`；`WBP_MainHUD` 的 `BossBarList` 控件名必须等于 C++ 成员；`BossBarItemClass` 指向 item WBP。 | `AGENTS.md` §5/§6；`ARCHITECTURE.md` §14 第 4 条（`BindWidgetOptional` 契约） |
| 4 | 新 Tag / 输入 / 复制 | 新增原生 tag `MAL.Enemy.Type.Boss/Elite/Normal`；无新输入；新增 PC 的 Client RPC（Add/Remove/Sync，Reliable）；不新增存档字段。 | `MALGameplayTags.cpp:21-82`、`MALPlayerController.h:42-52` |
| 5 | 存档 / 旧资产兼容 | 不改存档；旧敌人蓝图不设 tag 时按“非 Boss”处理；`DA_CombatConfig` 新字段有 C++ 默认值，旧资产无需迁移。 | `MALBossCombatConfig.h:147-287`、`MALCharacterBase.cpp:540-544` |
| 6 | 历史 bug / 注释警告 | `TargetLostGraceTime` 只覆盖遮挡（2s）；`OnTargetChanged` 是服务器侧；`UMALStatViewModel` 首次绑定会立即推送当前值；`UMALUserWidget` 的 View Binding 在入树后才生效；专用服务器不创建 Widget。 | `MALBossCombatComponent.cpp:148-168`、`MALStatViewModel.cpp:73-112`、`MALWorldWidgetComponent.cpp:95-112` |
| 7 | 怎么观察它生效 | 在 `MALCombatEventsSubsystem.cpp` 定义静态 `LogMALBossBar`，记录 `add/remove/sync`；PIE 看顶部条；`Saved/Logs/MAL.log` 查错误；双端 PIE 看两端一致。 | `MALBossCombatComponent.cpp:1681-1684`（现有日志风格） |
| 8 | 最可能坏在哪 | 8s 脱战与现有 2s 遮挡宽限混淆；所有客户端同步；item 入树/推 VM 顺序；Boss 头顶条隐藏；多 Boss 顺序与上限；死亡淡出与服务器移除的时序。 | 见 §1.2 |

### 1.2 需在实现阶段确认的事项

| # | 事项 | 动作 | 处理方式 |
| --- | --- | --- | --- |
| 1 | `WBP_MainHUD` 根节点能否直接加 `BossBarList` | 打开 WBP_MainHUD 看根节点 | 若根是 Canvas/Overlay，直接加；否则用 Canvas 包一层，不改现有控件 |
| 2 | `WBP_BossBarItem` 的 View Binding 源名 | 新建 item WBP 时声明 `UMALBossBarViewModel` source，并勾 Allow Setting | C++ 用 `SetViewModelByClass`，不依赖源名，降低静默失败风险 |
| 3 | Elite/Normal 测试 tag | 在 `BP_StoneGolem` 上临时打 `Normal`，必要时复制一个临时敌人打 `Elite` | 验证 A8 后恢复；不新增长期测试资产 |

## 2. 方案

服务器维护“正在交战的 Boss”列表，客户端只负责显示；不把目标状态复制给客户端让各端推断。

```text
玩家伤害 Boss
  → UMALAttributeSet 折叠伤害（服务器）
  → UMALCombatEventsSubsystem::OnDamageResolved
       ├─ Victim 是带 MAL.Enemy.Type.Boss 的 AMALEnemyCharacter
       ├─ Instigator 解析到 AMALPlayerController
       └─ 加入 ActiveBosses（服务器），向所有 PC 发 ClientAddBossBar(Boss)

客户端 HUD
  → AMALPlayerController::ActiveBossBars（按进入战斗顺序）
  → UMALHUDWidget::RefreshBossBars
  → 创建 UMALBossBarItemWidget（WBP_BossBarItem）
        · UMALBossBarViewModel : UMALStatViewModel 绑 Boss ASC
        · BossName 来自 DA_CombatConfig.BossDisplayName
        · Health/MaxHealth 继续走已复制的属性

脱战 / 死亡
  → 服务器每 0.1s 检查 BossCombat->HasValidTarget()
        ├─ 有目标：取消退场计时
        └─ 无目标 8s：ClientRemoveBossBar(Boss, false) → HUD 立即移除
  → Boss IsDead：ClientRemoveBossBar(Boss, true) → HUD 1.5s 淡出
  → Boss actor 销毁：ClientSyncBossBars(剩余列表) → HUD 清空重建（不要求淡出）
```

被否的替代方案与原因：

- 替代 A：在 `AMALEnemyCharacter` 上复制 `bBossEngaged`，客户端各自扫描敌人并推断。否决因为目标状态在服务器、客户端不应推断；违反“表现走定向 RPC”的既有约束。
- 替代 B：在 Boss 上 `NetMulticast` 广播进入/退出。否决因为中途加入没有当前列表，且每个客户端仍要自己维护顺序/上限；PC 的 Reliable Client RPC + `ClientSyncBossBars` 更直接。
- 替代 C：新增一个 `UMALBossBarSubsystem`。否决因为首版只需要一个伤害事件源和一个 HUD 列表；先扩展 `UMALCombatEventsSubsystem`，等出现第二个消费者再抽子系统（ADR D13 的失效条件）。

## 3. 改动清单

| # | 文件 / 资产 | 动作 | 为什么 |
| --- | --- | --- | --- |
| 1 | `Source/MAL/Public/Tags/MALGameplayTags.h` | 声明 `Enemy_Type`/`Boss`/`Elite`/`Normal` | 用种类 tag 决定 Boss 身份 |
| 2 | `Source/MAL/Private/Tags/MALGameplayTags.cpp` | 注册 4 个原生 tag | 同上 |
| 3 | `Source/MAL/Public/Character/MALEnemyCharacter.h` | 加 `EnemyTypeTag`、`GetBossDisplayName()`、`BeginPlay` | 身份、名字、头顶条 |
| 4 | `Source/MAL/Private/Character/MALEnemyCharacter.cpp` | Boss 时隐藏头顶条；名字回退 | 同上 |
| 5 | `Source/MAL/Public/Data/MALBossCombatConfig.h` | 加 `BossDisplayName`、`BossBarExitDelay` | 名字与 8s 可配 |
| 6 | `Source/MAL/Public/Combat/MALCombatEventsSubsystem.h` | 加 active boss 列表、退场定时器、Sync/通知 | 服务器唯一状态源 |
| 7 | `Source/MAL/Private/Combat/MALCombatEventsSubsystem.cpp` | 首次伤害加入、轮询目标/死亡、8s 退场、广播所有 PC | 同上 |
| 8 | `Source/MAL/Public/Player/MALPlayerController.h` | 加 `ActiveBossBars`、3 个 delegate、3 个 Client RPC | 客户端列表 |
| 9 | `Source/MAL/Private/Player/MALPlayerController.cpp` | 实现 RPC、列表、`BeginPlay` 同步 | 同上 |
| 10 | `Source/MAL/Public/UI/Widgets/MALHUDWidget.h` | 加 `BossBarList`、`BossBarItemClass`、上限/淡出、item map | HUD 容器 |
| 11 | `Source/MAL/Private/UI/Widgets/MALHUDWidget.cpp` | 创建/移除 item、绑定 PC delegate、`NativeDestruct` 清理 | 同上 |
| 12 | `Source/MAL/Public/UI/ViewModel/MALBossBarViewModel.h` | 继承 `UMALStatViewModel`，加 `BossName` | 复用血条/残影 |
| 13 | `Source/MAL/Private/UI/ViewModel/MALBossBarViewModel.cpp` | `SetBossName` | 同上 |
| 14 | `Source/MAL/Public/UI/Widgets/MALBossBarItemWidget.h` | item 基类：VM、绑 ASC、淡出、ASC 重试 | 每个 Boss 一条 |
| 15 | `Source/MAL/Private/UI/Widgets/MALBossBarItemWidget.cpp` | 实现绑定/淡出/重试 | 同上 |
| 16 | `Content/Blueprint/UI_1/WBP_MainHUD.uasset` | 加 `BossBarList` VerticalBox；设 item class/上限/淡出 | 顶部容器 |
| 17 | `Content/Blueprint/UI_1/Enemy/Boss/WBP_BossBarItem.uasset` | 新建 item WBP（父类 `UMALBossBarItemWidget`） | 单条布局/绑定 |
| 18 | `Content/Blueprint/AI/Boss/DA_CombatConfig.uasset` | 填 `BossDisplayName`、`BossBarExitDelay=8` | 名字/8s |
| 19 | `Content/Blueprint/AI/Boss/BP_BoosCombat.uasset` | `EnemyTypeTag=MAL.Enemy.Type.Boss` | 唯一 Boss 身份 |
| 20 | `Source/MAL/ARCHITECTURE.md` | ADR D13 + UI/网络说明 | 收尾同步 |
| 21 | `Source/MAL/README.md` | UI/网络/缺口说明 | 收尾同步 |

## 4. 数据与资产改动

| 资产 | 改动 | 命名/路径约定 |
| --- | --- | --- |
| `WBP_MainHUD` | 加 `BossBarList` VerticalBox（顶部居中、`SelfHitTestInvisible`）；设 `BossBarItemClass=WBP_BossBarItem`、`MaxBossBars=3`、`BossBarDeathFadeDuration=1.5` | `/Game/Blueprint/UI_1` |
| `WBP_BossBarItem`（新建） | 父类 `UMALBossBarItemWidget`；View Binding source `UMALBossBarViewModel`（Allow Setting）；名称/主条/残影条；数值文本首版不做 | `/Game/Blueprint/UI_1/Enemy/Boss` |
| `DA_CombatConfig` | `BossDisplayName=Hammer`（当前配置值）；`BossBarExitDelay=8` | `/Game/Blueprint/AI/Boss` |
| `BP_BoosCombat` | `EnemyTypeTag=MAL.Enemy.Type.Boss` | `/Game/Blueprint/AI/Boss` |

新增 Tag / 输入 / 枚举：

- Tag：`MAL.Enemy.Type` 根 + `MAL.Enemy.Type.Boss` / `.Elite` / `.Normal`，全部在 `UMALGameplayTags` 声明并注册。
- 输入：无新增 `IA_*` / `IMC_InputContext` / `DA_InputConfig` 条目。
- 枚举：无新增。

> 验证期临时改动：A1 把 `DA_CombatConfig.BossDisplayName` 改成 `Test Boss`；A8 把 `BP_BoosCombat.EnemyTypeTag` 依次改成 `Elite`/`Normal` 再恢复 `Boss`。不新增测试资产。

## 5. 状态、网络与存档

- 新增状态：
  - **服务器**：`UMALCombatEventsSubsystem::ActiveBosses`（`TArray<TWeakObjectPtr<AActor>>`，按加入顺序）、`BossExitTimers`、`BossPollTimer`。生命周期跟随 World；仅服务器有内容。
  - **客户端**：`AMALPlayerController::ActiveBossBars`（表现列表）、`UMALHUDWidget::BossBarItems`（widget map）、`UMALBossBarItemWidget::ViewModel`。生命周期跟随本地 PlayerController/HUD，不存档。
  - **敌人**：`AMALEnemyCharacter::EnemyTypeTag`、`UMALBossCombatConfig::BossDisplayName`/`BossBarExitDelay` 是 class/asset default，各端一致，不需要复制。
- 复制：Boss 的 Health/MaxHealth 已有 `ReplicatedUsing`；`bIsDead` 已复制；**不新增 replicated 字段**。`EnemyTypeTag` 是 CDO 默认（各端一致）；不复制。
- RPC：`AMALPlayerController` 新增 `ClientAddBossBar(AActor*)`、`ClientRemoveBossBar(AActor*, bool bFadeOut)`、`ClientSyncBossBars(const TArray<AActor*>&)`，全部 **Reliable**。理由：出现/消失/列表同步不是高频，丢失会让 UI 与服务器不一致；伤害数字仍保持 Unreliable。
- 客户端→服务器：无新增 Server RPC；进入/退出完全由服务器的 `OnDamageResolved` 与 0.1s 轮询推导。
- 存档：无新增字段，不需要迁移。
- 中途加入：服务器在 `AMALPlayerController::BeginPlay`（`HasAuthority()`）调用 `UMALCombatEventsSubsystem::SyncToController`，发 `ClientSyncBossBars`；客户端 HUD 初始化时也按当前 PC 列表刷新。
- 8s 退场：服务器每 0.1s 检查 `BossCombat->HasValidTarget()`；无目标启动 `BossBarExitDelay`（默认 8s）一次性 timer，重新有目标就取消。现有 `TargetLostGraceTime=2s` 只用于遮挡，不混用。
- 死亡：`IsDead()` 时移除并带 `bFadeOut=true`；HUD 1.5s 淡出。Actor 销毁：`OnDestroyed` 触发 `SyncAllControllers`，HUD 清空重建，不要求淡出（spec 已明确）。
- 多 Boss：服务器列表不设上限；HUD 取最后 `MaxBossBars=3`，按加入顺序追加。
- HUD 刷新只负责新增和 >3 的即时裁剪；普通移除只由 `OnBossBarRemoved` / `OnBossBarsReset` 处理，避免把正在淡出的条目提前删掉。
- 构造时序：item 先 `CreateWidget` + `AddChildToVerticalBox`，再 `SetViewModelByClass`（View Binding 入树后才生效）；Boss ASC 未到时用 `FMALRetryPoll` 重试。
- 安全性：只有 `Victim` 带 Boss tag 且 `Instigator` 解析到 `AMALPlayerController` 才进入；客户端不能伪造。
- 日志：在 `MALCombatEventsSubsystem.cpp` 定义静态 `LogMALBossBar`（`DEFINE_LOG_CATEGORY_STATIC`），记录 `add/remove/sync`；不新增文件。

## 6. 迁移与兼容

- 旧资产：`DA_CombatConfig` 新字段有 C++ 默认值；`BossDisplayName` 为空时显示 `Boss`，`BossBarExitDelay` 默认 8。旧敌人蓝图未设 `EnemyTypeTag` 时按“非 Boss”处理，不会误显示。
- 旧蓝图：`WBP_MainHUD` 新增 `BossBarList` 与属性；C++ 用 `BindWidgetOptional`，旧 HUD 不设也不会编译失败（功能关闭并打 WARN）。
- 旧存档：无变化，无迁移。
- 迁移窗口：无。后续给普通敌人补 `Normal`/`Elite` tag 只是一次数据补全，不影响存档。

## 7. 验证计划（对应 spec 验收项）

| 验收 | 验证动作 | 证据形式 |
| --- | --- | --- |
| A1 | 确认 `DA_CombatConfig.BossDisplayName=Hammer`；PIE 对 Boss 首次伤害 | 顶部出现 `Hammer` + 血条；Boss 头顶无条；截图 + `LogMALBossBar: add` |
| A2 | 继续对 Boss 造成伤害 | 主条下降、残影条延迟追上、无 NaN（数值文本首版不做）；连续截图/录屏 + 日志 |
| A3 | 停止攻击，让 Boss 失去目标 8s | 条消失；再次伤害重新出现；`remove ... fade=0` + 截图 |
| A4 | 把 Boss 打到 0 | 1.5s 淡出后消失；其他 Boss 条保留；`remove ... fade=1` + 录屏 |
| A5 | 与未带 Boss tag 的普通敌人交战 | 顶部无条；头顶条正常；截图 |
| A6 | 场上两个 `BP_BoosCombat` 实例，先后伤害 | 两条按加入顺序排列；击杀一个只淡出对应条；首版最多 3 条；截图 |
| A7 | Listen Server + 2 Clients，客户端 1 伤害，客户端 2 观察 | 两端都出现/更新/消失；服务器只加一次；双端截图/录屏 + 服务端日志 |
| A8 | 把 `BP_BoosCombat.EnemyTypeTag` 临时改 `Elite`/`Normal` | 顶部无 Boss 条；头顶条正常；验证后恢复 `Boss` |
| A9 | 血条显示时打开/关闭属性/技能菜单 | 条不被破坏，关闭后仍显示当前血量，输入恢复；截图 + 手动操作 |

静态与回归：

- `Scripts\AI\verify.cmd`：无 FAIL；已知 2 个 WARN（`/Game/Maps` editor-only、3 个 0 字节资产）记入 `verify.md`。
- `Scripts\AI\build.cmd`：exit 0，无新增 error。
- `Saved/Logs/MAL.log`：无新增 `Error:` / `Ensure condition failed`。
- 回归：普通敌人头顶条、HUD 血条/技能条、伤害数字、菜单开关、Boss 头顶条隐藏/普通敌人保留。

## 8. 切片

| 片 | 内容 | 可运行检查 |
| --- | --- | --- |
| S1 | tag + `EnemyTypeTag` + 名字/退出延迟 + 服务器 active 列表 + PC RPC/列表 + `LogMALBossBar`；不做 UI | 编译通过；伤害 Boss 后日志出现 `add`，失去目标 8s 出现 `remove fade=0`；`verify.cmd` 无 FAIL |
| S2 | `MALBossBarViewModel` + `MALBossBarItemWidget` + HUD 容器 + `WBP_MainHUD`/`WBP_BossBarItem` + `DA_CombatConfig`/`BP_BoosCombat`；单 Boss 显示 | PIE 验证 A1/A2/A5；顶部条出现、名称/血条/残影正确；普通敌人不显示；数值文本首版不做 |
| S3 | 多 Boss 顺序/上限、8s 退场、死亡淡出、同步/中途加入、双端与菜单回归 | 验证 A3/A4/A6/A7/A8/A9；2 Clients 两端一致；无新增 Error/Ensure |

> S1 的 `add/remove` 日志依赖 S2 的 `BP_BoosCombat.EnemyTypeTag`；S1 先以编译 + `verify.cmd` 判定，S2 补日志证据。

## 9. 回滚

- 代码：按 S3 → S2 → S1 顺序回退提交；新文件 `MALBossBarViewModel.*`、`MALBossBarItemWidget.*` 直接删除，其余文件移除新增成员/RPC/日志。无存档或数据结构迁移。
- 资产：改动 `Content/` 前建备份分支 `backup/boss-health-bar-assets-20261122`；回滚时删除 `WBP_BossBarItem`，恢复 `WBP_MainHUD`（移除 `BossBarList` 与属性）、`DA_CombatConfig`（清空名称/退出延迟）、`BP_BoosCombat`（清空 `EnemyTypeTag`）。确认 LFS blob 可取回再动手。
- 配置：无 ini 改动；原生 tag 随代码回退移除。资产回退要在代码回退之前完成，避免蓝图引用不存在的 tag。
- 文档：README/ARCHITECTURE 的同步内容随提交回退；功能目录保留，INDEX 标 `搁置` 并写原因。

## 10. 范围外发现（不在本功能修）

| 发现 | 建议处理 |
| --- | --- |
| `UMALBossCombatComponent::TargetLostGraceTime` 是 2s 遮挡宽限，和新 UI 的 8s 脱战是两个概念 | 保持现状；以后统一“战斗退场”定义时再收敛 |
| 普通敌人顶部条 | 不做；继续用头顶条 |
| Boss 阶段/韧性/护盾/技能预警 | 另开功能单 |
| 超过 3 个 Boss 的滚动/团队框架 | 另开功能单 |
| 新美术风格/动画/音效 | 首版复用现有 UI 材质与字体 |
| Boss 条当前/最大生命数值文本 | 后续单独补；Owner 决定首版不做 |
| `BossAI_Cpp_Reference.md` 阶段 2–3 未重写 | 单独文档单 |

## 11. 决策记录

- **ADR D13（触发）**：Boss 血条状态由服务器 `UMALCombatEventsSubsystem` 维护，经 `AMALPlayerController` 的 Reliable Client RPC 下发；不复制 `bBossEngaged` 让客户端推断。
  - 理由与代价：权威侧明确、所有客户端/中途加入一致；代价是 PC 多一个列表与 3 个 RPC，`UMALCombatEventsSubsystem` 多一份 UI 路由职责。
  - 被否替代：复制布尔（客户端推断）、Boss `NetMulticast`（无中途同步）、新 `UMALBossBarSubsystem`（首版多一层）。
  - 失效条件：Boss 数量变大、出现按玩家过滤或团队框架时，抽独立 `UMALBossBarSubsystem` 或改为复制列表。
  - 收尾同步 `ARCHITECTURE.md` §13。
- 其他决策：8s 退场用服务器 0.1s 轮询而不是绑定 `OnTargetChanged`（动态委托绑定复杂，且目标状态路径多；轮询只在有 active Boss 时运行）；多 Boss 上限在 HUD（服务器不截断，客户端可自由决定显示数量）；死亡淡出 1.5s 在客户端 item，服务器只发 `bFadeOut`；actor 销毁走 Sync，不保证淡出。

## 12. 变更：半径触发（v2，2026-11-22）

> §4/§5/§7/§8 的 v1 描述（首次受伤、8s 脱战、`BossBarExitDelay`）由本节替代；旧记录保留为历史。

### 12.1 新行为

- 每个 Boss 在 `DA_CombatConfig.BossBarActivationRadius`（默认 1500 cm）内，对**每个玩家独立**可见；玩家离开半径立即隐藏。
- Boss 只要**存活且带 `MAL.Enemy.Type.Boss` tag** 就参与检测，不要求受击或进入战斗。
- 死亡：仍在半径内的玩家看到 1.5s 淡出；已在半径外的玩家早已隐藏。
- 多 Boss 仍按进入半径的先后排列，HUD 最多 3 条；上限仍在 HUD。
- 旧的首次伤害触发、8s 脱困计时、`BossBarExitDelay` 全部移除。

### 12.2 代码改动

| # | 文件 / 资产 | 动作 | 为什么 |
| --- | --- | --- | --- |
| 1 | `Source/MAL/Public/Data/MALBossCombatConfig.h` | `BossBarExitDelay` 替换为 `BossBarActivationRadius=1500` | 半径按 Boss 配置 |
| 2 | `Source/MAL/Private/Character/MALEnemyCharacter.cpp` | 服务器 `BeginPlay` 把 Boss 注册到 `UMALCombatEventsSubsystem` | 未受击的 Boss 也要参与检测 |
| 3 | `Source/MAL/Public/Combat/MALCombatEventsSubsystem.h` | 服务器改为 KnownBosses + 每 PC 可见集合；移除伤害/8s 计时 | 每玩家独立半径 |
| 4 | `Source/MAL/Private/Combat/MALCombatEventsSubsystem.cpp` | 0.1s 轮询平面距离；只给对应 PC 发 Add/Remove/Sync；死亡发 `bFadeOut=true` | 同上 |
| 5 | `Content/Blueprint/AI/Boss/DA_CombatConfig.uasset` | 删除 `BossBarExitDelay`，新增 `BossBarActivationRadius=1500` | 资产配置 |

`AMALPlayerController`、`MALHUDWidget`、`MALBossBarViewModel`、`MALBossBarItemWidget`、`WBP_*` 均复用，不改。

### 12.3 验证（v2）

| 验收 | 动作 | 预期 |
| --- | --- | --- |
| A1 | Boss 未受击，玩家走进半径 | 顶部出现配置名称；头顶条隐藏 |
| A2 | 在半径内伤害 Boss | 主条/残影条正常；无 NaN |
| A3 | 玩家走出半径 | 立即消失；再进入重新出现 |
| A4 | 在半径内击杀 Boss | 1.5s 淡出；半径外玩家早已无条 |
| A5 | 普通敌人附近 | 无顶部条；头顶条正常 |
| A6 | 同时进入两个 Boss 半径 | 两条；离开一个只隐藏对应条；最多 3 条 |
| A7 | 2 Clients，客户端 1 在半径内、客户端 2 在半径外 | 只有客户端 1 显示；客户端 2 进入后自己显示 |
| A8 | Elite/Normal 敌人 | 无顶部条；头顶条正常 |
| A9 | 菜单开合 | 条不被破坏，输入恢复 |

### 12.4 切片与回滚

- **S4（v2）**：配置/注册/服务器每 PC 可见性；编译 + `LogMALBossBar` 日志 + PIE A1/A3/A7。HUD 不改。
- 回滚：回退 v2 提交；`DA_CombatConfig` 恢复 `BossBarExitDelay` 旧字段（或按备份分支 `backup/boss-health-bar-assets-20261122` 恢复）；v1 代码仍在 S1/S2 提交里。
- ADR D13 修订：可见性从“服务器全局 active list”改为“服务器按玩家半径计算，经 PC Client RPC 下发”；收尾同步 `ARCHITECTURE.md` §8/§11/§13。
