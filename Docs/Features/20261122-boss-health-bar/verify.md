# boss-health-bar — 验证记录

关联：[spec.md](spec.md) · [plan.md](plan.md)

> v1（首次受伤 + 8s）已于 2026-11-22 通过；v2 半径触发变更验证见 §11，当前待执行。

## 0. 环境

| 项 | 值 |
| --- | --- |
| 日期 | 2026-11-22 |
| 验证者 | 新会话（`WF: verify`，与实现会话不同）+ Owner 复核 |
| 实现者会话 | 上一会话（提交 `f8bfb57`、`c592d63`、`85efb32`） |
| 引擎 | UE 5.8.3 |
| 分支 / 提交 | `feature/boss-health-bar` @ `85efb32`（v2；v1 代码在 `c592d63`） |
| 编辑器是否重启过 | 是：02:09:41 编译 → 02:10:15 重启编辑器，DLL 由 00:57:03 更新为 02:09:41 |

测试夹具（不改资产，均不落盘）：

- PIE 用 `StartTransform` 把玩家出生到 Boss 附近，解决近战自动跑位问题。
- 为压缩验证时间，在 **PIE 内存**里把 Boss `Health/MaxHealth` 临时设为 20/50/10（`ObjectTools.set_properties`，PIE 结束即失效；`git status` 确认 Content 无改动）。
- A6 临时在关卡放置第二个 `BP_BoosCombat` 与一个 `BP_Enemy_Grunt`；验证后已从场景删除，原 Boss 位置还原为 `(1213.7, -2038.5, 88)`。
- 证据截图目录：`Saved/Verification/`。

## 1. 静态检查

```text
Scripts\AI\verify.cmd
```

| 结果 | 内容 |
| --- | --- |
| PASS | 7（layout / primary-assets / gameplay-cues / naming / doc-drift / tags） |
| WARN | 2 个既有项：`/Game/Maps` editor-only 扫描（Config/DefaultGame.ini:17）；3 个 0 字节资产（GrruzamSwordPack 三张动画） |
| FAIL | 0 |

Report: `Saved/AI/verify-latest.log`

## 2. 编译

```text
Scripts\AI\build.cmd
```

- exit code：0
- 新增 error：0
- 新增 warning：0
- 日志路径：`Saved/AI/build-20261122-020941.log`
- 备注：第一次在编辑器运行时执行被 UBT 拒绝（`Unable to build while Live Coding is active`，exit 6），属流程问题；关闭编辑器后重跑通过。

## 3. PIE 用例

| 验收 | 操作 | 预期 | 实际 | 证据 |
| --- | --- | --- | --- | --- |
| A1 | 传送到 Boss 正前方，鼠标左键攻击一次 | 顶部出现血条 + 名称；Boss 头顶条隐藏（首版不要求数值文本） | 顶部出现 `Hammer` 条；数值文本首版不做（Owner 决定，spec 已同步）；Boss 头顶条 `bVisible=false`（Grunt 为 `true`） | `Saved/Verification/a1-boss-bar-visible.png`、`pie3-a1-bar.png`；`LogMALBossBar: add`（09:11:43 / 09:15:56 / 09:33:55）；运行时 `HealthBarWidget.bVisible` 读取 |
| A2 | 继续攻击 2~3 次 | 主条立即下降；残影条延迟追上；无 NaN（数值文本不在首版验收范围） | Health 50→36.56→23.12→15.73（属性读数）；主条降到 ~37%，残影条停在 ~73% 后追上；无 NaN；数值文本首版不做 | `Saved/Verification/a2-bar-73.png`、`a2-bar-hit2.png`；Boss AttributeSet `Health` 读数 |
| A3 | 停止攻击，让 Boss 丢失目标 8 秒；再打一次 | 血条消失；再次伤害后重新出现 | 09:18:14 `remove … fade=0`（距 add 约 8.3s）；09:19:49 再次伤害后 `add` | `Saved/Verification/pie3-state.png`（无条）；日志两行原文 |
| A4 | 把 Boss 打到 0 | 约 1.5s 淡出后消失，期间不再更新 | 09:26:45 `remove … fade=1`；1.5s 后顶部无条（淡出中间帧未抓到） | `Saved/Verification/a4-after-death.png`；日志原文 |
| A5 | 与普通敌人（BP_Enemy_Grunt，无 Boss tag）交战 | 顶部无血条；头顶条正常 | Grunt Health 350→329.17（命中）；`LogMALBossBar` 无新增；顶部无条；Grunt 头顶条可见 | `Saved/Verification/a5-grunt-no-bar.png`；Grunt `GetAttributeValues` |
| A6 | 两个 `BP_BoosCombat` 先后受伤；击杀一个 | 两条按入战顺序排列；单杀只淡出对应条；最多 3 条 | 服务器侧两条可共存：09:29:53 add `…1233522079`、09:30:09 add `…2066976040`；09:33:07 击杀前者 `remove fade=1`，后者未收到 remove。Owner 复核确认多 Boss 与单杀单条正常；此前单 Boss 双条为验证夹具/预览残留，不是产品路径 | `Saved/Verification/a6-two-rows.png`、`a6-two-bosses.png`；日志原文；Owner 复核 |
| A7 | Listen Server + 2 Clients，客户端 1 伤害 Boss | 两端都能看到出现/变化/死亡；服务器只结算一次 | **人工验证通过（Owner，2026-11-22）**：两端显示/更新/淡出一致；自动会话因 `StartPIE` 不暴露 NetMode 未能自动执行 | Owner 人工确认；自动步骤见 §6（未落盘截图） |
| A8 | 与 `MAL.Enemy.Type.Elite` / `.Normal` 敌人交战 | 顶部无条；头顶条正常 | **人工验证通过（Owner，2026-11-22）**：Elite/Normal 无顶部条、头顶条正常，验证后恢复 Boss tag | Owner 人工确认；代码分支 `MatchesTag(Enemy_Type_Boss)` 与 A5 同路径 |
| A9 | 血条显示中打开属性菜单、技能菜单，再关闭 | 血条不被破坏；关闭后输入恢复 | 属性菜单打开时血条保持（主条/残影继续更新）；技能菜单打开时血条保持；关闭技能菜单后一次攻击把 6.56 打到 0（命中，`remove fade=1`） | `Saved/Verification/a2-damage-2.png`、`a9-skill-menu.png`；Boss `Health` 读数 6.56→0 |

## 4. 日志

`Saved/Logs/MAL.log`（本次启动 09:10 起）

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| 新增 `Error:` | 仅 3 条既有项 | `LogAssetRegistry: Error: Package is unloadable: …/animnanami.uasset`、`bilibilinanami.uasset`、`bilililith.uasset`（与 `verify.cmd` 的 WARN 同一批） |
| 新增 `Ensure condition failed` | 无 | — |
| 验证工具噪声 | 若干 `LevelEditorSubsystem: Error: GetCurrentLevel` / `LogUtils: Error: The Editor is currently in a play mode` | 验证时在 PIE 中调用场景工具导致，非项目代码路径 |
| 关键自定义日志 | `LogMALBossBar` 正常 | add：09:11:43、09:15:56、09:19:49、09:26:37、09:29:53、09:30:09、09:33:55；`remove … fade=0`：09:18:14、09:20:32、09:21:09、09:30:14、09:30:17、09:30:31；`remove … fade=1`：09:26:45、09:33:07 |
| `WBP_BossBarItem` 蓝图编译错误 | 无新增（验证期间） | 历史 08:10「BossName is not writable」、08:50「ProgressBar_Lagged 找不到」均出现在提交前，当前重编译返回 true |

## 5. 资产核对

| 资产 | 核对项 | 实际值 | 证据 |
| --- | --- | --- | --- |
| `DA_CombatConfig` | `BossDisplayName` / `BossBarExitDelay` | `Hammer` / `8` | `ObjectTools.get_properties` |
| `BP_BoosCombat` | `EnemyTypeTag` | `MAL.Enemy.Type.Boss` | 同上（CDO 与关卡实例一致） |
| `WBP_MainHUD` | `BossBarList` / `BossBarItemClass` / `MaxBossBars` / `BossBarDeathFadeDuration` | VerticalBox 存在 / `WBP_BossBarItem` / `3` / `1.5` | `UMGToolSet.GetWidgetDescription` + CDO 属性 |
| `WBP_BossBarItem` | 控件树与编译 | `Overlay_184 > VerticalBox_0 > (BossName, Overlay_0 > ProgressBar_Lag / ProgressBar_564 / Image_194)`；`CompileWidgetBlueprint` = true；无当前/最大生命文本控件（Owner 决定首版不做） | `GetWidgetDescription` |
| PIE Boss / Grunt | `HealthBarWidget.bVisible` | `false` / `true` | `ObjectTools.get_properties`（运行时） |

## 6. 双端（A7）与 A8 — 转人工

A7（Listen Server + 2 Clients）：

1. 编辑器 Play 下拉：Net Mode = `Play As Listen Server`，Number of Clients = 2，然后 Play。
2. 客户端 1 走到 Boss 前攻击一次：预期两端顶部都出现 `Hammer` 条、两端血量同步、伤害数字各显示一次。
3. 继续打到死亡：两端都在 ~1.5s 内淡出；服务器日志 `remove … fade=1` 只出现一次。
4. 记录：双端截图 + `Saved/Logs/MAL.log` 的 `LogMALBossBar` 行。

A8（Elite/Normal）：

1. 复制 `BP_Enemy_Grunt` 为 `BP_Test_Elite`（仅本地，不提交），`EnemyTypeTag = MAL.Enemy.Type.Elite`，拖入关卡。
2. 再复制一份设 `MAL.Enemy.Type.Normal`；分别交战一次。
3. 预期：顶部不出现 Boss 条，头顶条正常；日志无 `LogMALBossBar: add`。
4. 测完删除临时蓝图与关卡实例，不要保存提交。

## 7. 回归

| 域 | 检查 | 结果 |
| --- | --- | --- |
| 战斗 | 普攻命中出伤害、飘字正常（17/27）、死亡后不再结算 | 通过 |
| 武器 | 切换武器后模型/技能条同步 | 未覆盖（本功能未触及） |
| 技能 | 冷却可见、蓝耗生效 | 未覆盖（菜单开合正常，输入恢复） |
| AI | Boss 索敌/追击/击杀 | 通过（Boss 会追击并击退玩家；死亡后停止） |
| UI | 属性菜单、技能菜单开合；HUD 血条/技能条正常；Boss 头顶条隐藏、普通敌人保留 | 通过 |
| 存档 | 加点/解锁后重开仍在 | 未覆盖（本功能不写存档） |

## 8. 既有问题（非本次引入）

| 问题 | 证据 | 是否登记 |
| --- | --- | --- |
| 3 个 0 字节 LFS 资产导致 `Package is unloadable` | `verify.cmd` WARN + `Saved/Logs/MAL.log` 09:10:33 | 已知（GrruzamSwordPack） |
| `/Game/Maps` 无对应 Content 目录（editor-only 扫描） | `Config/DefaultGame.ini:17` | 已知 |
| 关卡内 `Content/__ExternalActors__/Blueprint/Map/Untitled/A/KN/` 未跟踪目录 | `git status` | 验证前即存在 |

## 9. 结论

**通过（Owner 复核，2026-11-22）**

验证会话原始结论为「不通过」，Owner 复核后确认如下：

- **A1/A2**：当前/最大生命数值文本由 Owner 决定**首版不做**，spec §1/§3/§4 已同步；名称、主条、残影条、头顶条隐藏均通过。
- **A6**：Owner 人工复核正常；服务器两条 `add`、单杀只 `remove` 对应条；此前单 Boss 双条为验证夹具/预览残留，不是产品路径。
- **A7/A8**：Owner 人工验证通过（双端、Elite/Normal 不显示）。
- **通过项**：A1、A2、A3、A4、A5、A6、A7、A8、A9。

遗留项：

1. Boss 条当前/最大生命数值文本后续另开轻量/功能单。
2. 建议后续把 A7/A8 的人工截图/日志补进本节；当前以 Owner 人工确认为证据。

后续动作：无阻塞，进入 ⑥ 收尾。

## 10. 复盘

1. 最慢的阶段：单机 PIE 造真实伤害。Boss 12,600 血、AI 持续击退，靠鼠标点击几乎打不满血条；最终用 PIE 内存改 `Health/MaxHealth` 才把 A2/A4 做出来。
2. 被证伪的假设：改蓝图 CDO 的 `EnemyTypeTag` 能让新放置实例继承（没有）；PIE 中可给 `EditDefaultsOnly` 的关卡实例赋值（被拒绝）。另外 `StartPIE` 不暴露 NetMode，双端必须人工。
3. 下次要加的门禁：spec 里「名称与数值同屏」这种**可见元素清单**要在 plan 的资产表里逐项落到 WBP 控件名，验证第一步先对资产清单，再进 PIE；「多 Boss 两条同屏」要写进自动化脚本的必抓帧。

## 11. v2 半径触发（已通过）

> v2 变更：显示触发从「首次受伤 + 8s 脱战」改为「玩家进入 Boss 显示半径 / 离开立即消失」，每个玩家独立判断；半径来自 `DA_CombatConfig.BossBarActivationRadius`（默认 1500 cm）；Boss 存活且带 Boss tag 即可，不要求受击。

| 验收 | 操作 | 预期 | 实际 | 证据 |
| --- | --- | --- | --- | --- |
| A1 | Boss 未受击，玩家走进半径 | 顶部出现配置名称；头顶条隐藏 | 通过：顶部显示配置名称；头顶条隐藏 | Owner 人工验证；`LogMALBossBar: register ... radius=1500`（10:12:52，两个 Boss） |
| A2 | 在半径内伤害 Boss | 主条/残影条正常；无 NaN | 通过：主条/残影条正常 | Owner 人工验证 |
| A3 | 玩家走出半径 | 立即消失；再进入重新出现 | 通过：离开立即消失、再进重新出现 | Owner 人工验证 |
| A4 | 在半径内击杀 Boss | 1.5s 淡出；半径外玩家早已无条 | 通过：1.5s 淡出 | Owner 人工验证 |
| A5 | 普通敌人附近 | 无顶部条；头顶条正常 | 通过：普通敌人无顶部条、头顶条正常 | Owner 人工验证 |
| A6 | 同时进入两个 Boss 半径 | 两条；离开一个只隐藏对应条；最多 3 条 | 通过：两条共存、离开一个只隐藏对应条 | Owner 人工验证；register 日志两个 Boss |
| A7 | 2 Clients，客户端 1 在半径内、客户端 2 在半径外 | 只有客户端 1 显示；客户端 2 进入后自己显示 | 通过：两端各自判断 | Owner 人工验证 |
| A8 | Elite/Normal 敌人 | 无顶部条；头顶条正常 | 通过：Elite/Normal 无顶部条 | Owner 人工验证 |
| A9 | 菜单开合 | 条不被破坏，输入恢复 | 通过：菜单开合正常 | Owner 人工验证 |

**v2 结论：通过（Owner 人工验证，2026-11-22）**

- 编译/加载：`UnrealEditor-MAL.dll` 03:11:08，编辑器 03:11:13 启动；v2 代码已加载。
- 关键日志：`LogMALBossBar: register BP_BoosCombat... radius=1500`（10:12:52，两个 Boss）。
- A1–A9 由 Owner 人工验证；v2 截图未逐项落盘，本次以 Owner 确认为证据。
- 旧 v1（首次受伤 + 8s）的证据保留在前 10 节，仅作历史。