# MAL 项目画像

> 本文件由代码库静态分析生成（`Source/MAL` + `Config` + `Content` + `Scripts` + git 历史），面向新成员快速建立整体认知，以及后续迭代的架构基线参考。
> 数据快照时间：2026-11-01。凡属推断或需人工确认的条目均标注「待确认」。
>
> 本文回答「是什么 / 有什么 / 到哪一步」。要回答「怎么搭的 / 为什么这样搭 / 改从哪里下手」，请看同目录的 [ARCHITECTURE.md](./ARCHITECTURE.md)。

---

## 1. 一句话定位

**第三人称动作角色扮演（ARPG）战斗原型**：以 GAS 为骨架，围绕「武器决定技能」这一核心设计，做出近战连招、命中手感（hit-stop / 镜头抖动 / 飘字）、属性点与技能树成长、以及基于行为树的怪物 AI 的完整可玩闭环。

| 维度 | 结论 |
| --- | --- |
| 视角 / 移动 | 第三人称，Character + SpringArm + Camera |
| 战斗 | 近战连招（蒙太奇驱动命中窗口）+ 自增益 Buff + 旋风斩类范围技能 |
| 成长 | 经验/等级 → 属性点（5 项主属性）+ 技能点（按武器购买技能树节点） |
| 敌人 | Stone Golem 类怪物：感知 → 追击 → 技能 → 巡逻，掉落经验给击杀者 |
| 界面 | UMG + MVVM：HUD（血条/蓝条/技能条）、属性面板、技能树面板、敌人血条、伤害飘字 |
| 联机 | 按 listen-server 权威模型编写（服务器判定命中，客户端只做表现），尚无正式对战/协作流程与在线子系统 |
| 关卡 | 启动关卡 `/Game/Blueprint/Map/Untitled`（`DefaultEngine.ini:4`），另有 `Blueprint/Map/TestMap` |
| 平台 | Windows 桌面（DX12 / SM6），画质取向为 Maximum，移动端与主机未配置 |

---

## 2. 技术栈

### 2.1 引擎与构建

| 项 | 值 | 来源 |
| --- | --- | --- |
| 引擎版本 | **UE 5.8.3**（`++UE5+Release-5.8-CL-58210709`） | `Saved/Logs/MAL.log` |
| 引擎关联 | `EngineAssociation` 为 GUID（自定义/源码构建注册，而非 `"5.8"` 版本号） | `MAL.uproject:3` |
| 主模块 | `MAL`（`Type: Runtime`，`LoadingPhase: Default`，`IMPLEMENT_PRIMARY_GAME_MODULE`） | `MAL.uproject:6-12`、`Private/MAL.cpp` |
| Target | `MAL.Target.cs`（Game）、`MALEditor.Target.cs`（Editor） | — |
| 构建基线 | `BuildSettingsVersion.V7` + `EngineIncludeOrderVersion.Unreal5_8` | `Source/MAL.Target.cs:11-12` |
| RHI | 默认 `DefaultGraphicsRHI_DX12`，D3D12 走 SM6、D3D11 走 SM5 | `Config/DefaultEngine.ini:33-38` |
| 渲染特性 | 关闭静态光照；开启 Virtual Shadow、Mesh Distance Fields、`DynamicGlobalIlluminationMethod=1`、`ReflectionMethod=1`、光线追踪（含 RayTracingProxies）、Substrate | `Config/DefaultEngine.ini:7-31` |

> 注意：`BuildSettingsVersion.V7` / `Unreal5_8` 仅在 5.8+ 有效；项目历史上曾因引擎版本不匹配而把 Target 降为 `V5` / `Unreal5_5`，换引擎版本时这两行是首个要改的地方。

### 2.2 模块依赖（`Source/MAL/MAL.Build.cs`）

- **Public**：`Core`、`CoreUObject`、`Engine`、`InputCore`、`EnhancedInput`、`GameplayAbilities`、`GameplayTags`、`GameplayTasks`、`UMG`、`ModelViewViewModel`、`FieldNotification`、`Niagara`、`SlateCore`、`AIModule`、`NavigationSystem`
  - 其中 `Niagara` / `SlateCore` / `AIModule` 之所以是公共依赖，是因为公共头文件暴露了 `UNiagaraSystem`、`FSlateColor`、`IGenericTeamAgentInterface` —— 注释里逐条说明了原因（`MAL.Build.cs:23-33`）。
- **Private**：`EngineCameras`（相机抖动）。
- **Editor only**（`Target.bBuildEditor`）：`UnrealEd`、`Kismet`、`BlueprintGraph`、`AnimGraph`、`AnimGraphRuntime`、`AssetTools` —— 供动画图层 commandlet 驱动动画图编辑器 API。
- `SetupGameplayDebuggerSupport(Target)`：一次性给出 GameplayDebugger 依赖与 `WITH_GAMEPLAY_DEBUGGER` 宏。

### 2.3 插件（`MAL.uproject`）

| 插件 | 用途 | 范围 |
| --- | --- | --- |
| `GameplayAbilities` | 技能 / 效果 / 属性 / Cue | Runtime |
| `ModelViewViewModel` | UI 数据绑定（全部 ViewModel 基类） | Runtime |
| `ModelingToolsEditorMode` | 建模工具（美术侧） | Editor |
| `ModelContextProtocol` / `AllToolsets` / `MVVMToolset` / `ModelViewViewModelPreview` | AI 代理直接操作编辑器资产（含 UMG/MVVM） | Editor |

### 2.4 工程约定

- 资产与二进制走 **Git LFS**：`.gitattributes` 覆盖 `uasset/umap/图片/模型/音频/字体/dll/pdb/zip`。
- `GameplayCueNotifyPaths=/Game`（`Config/DefaultGame.ini:28-29`）：Cue 在整个 Content 下自动发现。
- `WeaponDefinition` 注册为 Primary Asset 类型 —— **但其扫描目录 `/Game/Data/Weapons` 已不存在**，见 §8 缺口。
- CoreRedirects 保留历史改名：`MALHealthSet → MALAttributeSet`、`GameplayTags.Input_Attack → Input_LMB`（`DefaultEngine.ini:104-107`）。

---

## 3. 代码规模快照

| 指标 | 数值 |
| --- | --- |
| C++ 文件 | 162（82 头文件 + 80 实现） |
| C++ 行数 | ≈ 19,981 |
| Content 资产 | 7,065 个 `.uasset` + 22 个 `.umap` |
| Content 体积 | 8.28 GB |
| 资产目录 Top3 | `GrruzamSwordPack` 1007、`Character`（ParagonKwang）1601、`UI` 741 |
| 自制蓝图 | `Content/Blueprint` 共 103 个资产 |
| 自动化测试 | 0（无 Automation 测试、无 CI 配置目录） |
| `TODO/FIXME/HACK` | 0 处（技术债写在注释与本文 §8） |

代码风格上有一个显著特征：**头文件即设计文档**。公共 API 的注释解释的是「为什么这样设计 / 不这样会出什么问题」，而不是复述签名（典型如 `MALCharacterBase.h:125-134`、`MALWeaponDefinition.h:216-227`、`MALAbilitySystemComponent.h:69-79`）。新增公共接口时应保持同样密度。

---

## 4. 源码目录地图

`Source/MAL/Public/<Domain>` 与 `Private/<Domain>` 一一对应；`Private` 额外有 `Debug`（GameplayDebugger 页）与 `Editor`（commandlet），这两类不进公共接口。

| 目录 | 头/实现 | 职责 | 核心符号 |
| --- | --- | --- | --- |
| `AbilitySystem` | 6/6 | 属性分层与伤害结算 | `UMALAttributeSet`、`UMALProgressionAttributeSet`、`UMALDamageExecutionCalculation`、`FMALGameplayEffectContext`、`EMALPrimaryStat`/`EMALSecondaryStat` |
| `AI` | 4/4 | 怪物大脑与 BT 任务 | `AMALEnemyAIController`、`UBTTask_ActivateAbilityByTag`、`UBTTask_PatrolMoveTo`、`UBTTask_PickPatrolPoint`、`MALAIBlackboardKeys` |
| `Animation` | 5/5 | 动画基类 + 战斗 AnimNotify | `UMALAnimInstance`、`UMALAnimNotify_ComboWindow`、`_WeaponCollision`、`_SendHitEvent`、`_Lunge` |
| `Character` | 3/3 | 角色基类与敌我分支 | `AMALCharacterBase`（抽象）、`AMALCharacter`（玩家）、`AMALEnemyCharacter` |
| `Combat` | 9/7 | 命中窗口、武器、伤害、弹道、事件总线 | `UMALWeaponComponent`、`UMALWeaponDefinition`、`UMALComboComponent`、`UMALDamageExecutor`、`UMALHitFeedbackComponent`、`AMALProjectile`、`UMALCombatEventsSubsystem`、`FMALHitResult` |
| `Components` | 2/2 | 自定义 ASC 与成长组件 | `UMALAbilitySystemComponent`、`UMALProgressionComponent` |
| `Data` | 3/3 | 职业 / 技能池 / 能力集数据资产 | `UMALClassDefinition`、`UMALSkillTreeDefinition`、`UMALAbilitySet` |
| `GameAbilities` | 11/11 | GA 基类与具体技能、GE、Cue、震屏 | `UMALGameplayAbility`、`UMALAbility_MeleeAttack`、`_Attack`、`_Buff`、`_StoneGolemAttack`、`UMALDamageGameplayEffect`、`_CooldownGameplayEffect`、`_ManaCostGameplayEffect`、`_BuffGameplayEffect`、`AMALGameplayCueNotify_Buff`、`UMALHitCameraShake` |
| `GameModes` | 1/1 | GameMode 默认类绑定 | `AMALGameMode : AGameModeBase`（构造里固定 Pawn/PC/PS 为 MAL 版本） |
| `Input` | 2/2 | 增强输入配置与带 Tag 的事件转发 | `UMALInputConfig`、`UMALInputComponent`（`FMALInputActionEvent` 三参委托，携带 `ETriggerEvent`） |
| `Interfaces` | 1/1 | 蒙太奇通知 ↔ 技能通信契约 | `IMALComboAttackInterface`（不可蓝图实现） |
| `Player` | 2/2 | PC（输入分流/RPC/菜单/调试命令）与 PS（GAS 宿主） | `AMALPlayerController`、`AMALPlayerState` |
| `Save` | 2/1 | 单档 Profile 读写 | `UMALSaveGame`、`UMALSaveSubsystem`（`UGameInstanceSubsystem`，槽名 `MAL_Profile`） |
| `Tags` | 1/1 | 原生 GameplayTag 单例 | `UMALGameplayTags` |
| `UI` | 23/22 | 子系统 / ViewModel / Widget 三层 | `UMALUISubsystem`、`UMALDamageNumberSubsystem`、`FMALRetryPoll`、9 个 ViewModel、11 个 Widget、`UMALWorldWidgetComponent` |
| `Private/Debug` | — | GameplayDebugger 自定义页 `MAL.AI`（槽位 6） | `FMALGameplayDebuggerCategory` |
| `Private/Editor` | 2 | 资产自动化 commandlet | `MALLocomotionLayerSetup`、`MALGenerateAIAssets` |

---

## 5. 核心链路

### 5.1 GAS 装配关系

```
AMALPlayerState ── owns ──> UMALAbilitySystemComponent + UMALAttributeSet + UMALProgressionAttributeSet + UMALProgressionComponent
        ▲                                                    ▲
        │ (Player：ASC 在 PS 上，跨 respawn 存活)             │ InitAbilityActorInfo(PlayerState, Pawn)
AMALCharacterBase ── 缓存指针 ────────────────────────────────┘
        ▲
        ├── AMALCharacter      （玩家：走 PlayerState 路径）
        └── AMALEnemyCharacter （敌人：ASC/AttributeSet 是角色自身的 DefaultSubobject，ActorInfo = (this, this)）
```

`AMALCharacterBase::InitializeAbilitySystem()` 统一解析两种情况，子类不再重复 GAS 初始化（`MALCharacterBase.h:40-48`）。

### 5.2 输入分流（Lyra 帧缓冲式）

```
IMC_InputContext + DA_InputConfig(UMALInputConfig: InputTag ↔ InputAction)
        │
UMALInputComponent  ── FMALInputActionEvent(InputTag, Value, ETriggerEvent) ──>  AMALPlayerController::HandleInputActionEvent
                                                                              │
                     ┌────────────────────────────────────────────────────────┤
                     ▼                                                        ▼
        连续输入 Move/Look/Jump 直连 CMC                          离散技能 → UMALAbilitySystemComponent
                                                                   AbilityInputTagPressed/Released（帧缓冲 + LastInputValues）
                                                                        │
                                                            PostProcessInput → ProcessAbilityInput
                                                                        │
                                                        按 DynamicAbilityTags 里的 InputTag 命中 spec
                                                                        ▼
                                            ActivateAbility / InputPressed / InputReleased（技能内可读 GetLastInputValue）
```

- 激活策略二选一：`OnInputTriggered`（按下即启）与 `WhileInputActive`（按住期间有效，松手由 ASC 取消，除非技能自己在 `InputReleased` 里收尾）。
- 菜单打开 / 移动锁定 / 死亡三种状态会在 PC 层吞掉输入；这是历史上「输入偶尔失效」问题的排查区（`MALPlayerController.cpp:218-308`）。
- 技能基类还向蓝图暴露 `OnAbilityInputPressed/Released` 动态委托，供纯蓝图技能接事件。

### 5.3 命中 → 伤害 → 表现

```
蒙太奇 NotifyState                        服务器权威
  MAL AnimNotify_WeaponCollision  ─┐
  MAL AnimNotify_ComboWindow      ─┤→ UMALWeaponComponent::StartHitWindow(ComboStep)   客户端直接 return
  MAL AnimNotify_Lunge / SendHit  ─┘        │ 按 FName("weapon_collision") 解析 HitBox（QueryOnly / WorldDynamic / 仅 Pawn Overlap）
                                            │ 每刀 TSet<TWeakObjectPtr<AActor>> 去重 + 队伍友军过滤
                                            ▼
                                     FMALHitResult → OnWeaponHit 广播
                                            ▼
                              UMALDamageExecutor（唯一伤害入口，BlueprintFunctionLibrary）
                                 BaseDamage × LevelScale × (1 + ComboBonus × Step) × Weapon.DamageMultiplier
                                            ▼
                              SetByCaller(MAL.Data.Damage) → UMALDamageGameplayEffect
                                            ▼
                              UMALDamageExecutionCalculation
                                 Damage = Base × (1 + Atk/100) × (100/(100+Def)) × (1 + Bonus/100)，暴击掷骰
                                 物理/魔法通道由 MAL.Damage.Type.Magic 选择；暴击位写回 FMALGameplayEffectContext
                                            ▼
                              UMALAttributeSet::PostGameplayEffectExecute（Meta Damage → Health）
                                            ▼
                              UMALCombatEventsSubsystem（WorldSubsystem，仅服务器）
                              ├─> AMALPlayerController::ClientShowDamageNumber（Unreliable）→ UMALDamageNumberSubsystem 池化飘字
                              └─> AMALPlayerController::ClientOnHitLanded（Reliable）→ 震屏 + UMALHitFeedbackComponent 本地 hit-stop
                                                hit-stop 只置 bNoSkeletonUpdate 冻结姿势，
                                                由 UMALAnimInstance::AdvanceMontagesOnly 维持蒙太奇时钟
```

死亡路径：`Health → 0` 时先取伤害总线里的击杀者，失败则下一 tick 走 `HandlePendingDeath()` 兜底；`bIsDead` 复制给客户端以跑同一套表现，Tag `Status.Dead`、能力取消与尸体处理留在服务器。

### 5.4 技能授予：职业层 + 武器层

```
UMALAbilitySet（Ability + InputTag + Level + Cost/Cooldown/Passive GE）
   ├── UMALClassDefinition.GrantedAbilitySets   → SourceKey = NAME_None（角色自身 kit，含已购技能）
   └── UMALWeaponDefinition.GrantedAbilitySets  → SourceKey = UMALAbilitySystemComponent::WeaponAbilitySourceKey
                     │
   ClearGrantedAbilities(Key) 只清一层：买技能重建角色 kit 时，不会顺手拆掉武器技能（历史上正是这样丢的）
```

- **武器即技能集**：`UMALWeaponDefinition` 同时携带网格/挂点/命中盒尺寸/伤害倍率/拖尾/投射物/locomotion 图层/攻击蒙太奇/连招段名/`GrantedAbilitySets`/`SkillTree`。同一套挥砍能力可驱动剑与巨剑，差异全在数据资产里。
- **段名必须由武器定义提供**：`ComboSections` 留在 `UMALWeaponDefinition` 而非能力上——用剑的段名去跳斧子的蒙太奇会静默失败，表现为「二段挥不出来」（`MALWeaponDefinition.h:216-227`）。
- 换武器 → `OnEquippedWeaponChanged`（服务器与每个客户端都触发）→ 角色 `RefreshLocomotionLayer()` + PC `RefreshAbilityBar()`。图层链接失败会按 tick 重试至多 30 次（约半秒）后放弃并报错。
- 技能树购买：`ServerUnlockSkill(SkillTag)` → `UMALProgressionComponent` 记录 → `RefreshGrantedAbilities()` + `RefreshWeaponGrantedAbilities()`；已解锁节点按所属武器的 `WeaponTag` 归档，换武器时隐藏而非退点。

### 5.5 属性与成长

| 层 | 属性 | 说明 |
| --- | --- | --- |
| Primary | Vigor / Strength / Dexterity / Endurance / Spirit | 加点目标，`EMALPrimaryStat` 枚举封闭集合，禁止 UI 直接指往 MaxHealth |
| Secondary | MaxHealth、HealthRegen、MaxStamina、StaminaRegen、MaxMana、ManaRegen、Physical/MagicAttack、Physical/MagicDefense、CritChance、CritDamage、AttackSpeed、DamageBonus | 由 Primary 派生（如 `MaxHealth = 100 + Vigor × 25`） |
| Standalone | Health / Mana / Stamina | 当前资源值 |
| Meta | Damage | 单次 GE 内的临时值，不复制，`PostGameplayEffectExecute` 折叠进 Health |
| Progression | Level、Experience、ExperienceToNextLevel、AttributePoints、SkillPoints | 独立 `UMALProgressionAttributeSet`，因为敌人也带战斗属性集却无成长可复制 |

升级发点而非直接写属性（`UMALClassDefinition`：`MaxCharacterLevel=50`、`AttributePointsPerLevel=3`、`SkillPointsPerLevel=1`、可选单属性上限、`ExperienceToNextLevelTable` 曲线）。经验来源是**已结算伤害总线上的击杀事件**，而非 victim 的死亡回调——战斗层与角色层都不必知道成长系统存在。

### 5.6 怪物 AI

`AMALEnemyAIController`（服务器侧）持有视觉感知、已结算伤害反应、Blackboard 与一棵**代码构建**的行为树；设计师在蓝图上赋的 BT 资产优先于运行时树。

```
Selector
  Sequence(attack) : BTTask_ActivateAbilityByTag → Wait(cooldown + 随机偏差)
  Sequence(patrol) : BTTask_PickPatrolPoint → BTTask_PatrolMoveTo → Wait(pause)
  Wait(0.25)        <- 重新驱动 Selector
```

追击仍在 Tick 里做（响应比一次树求值更快，且要在无 NavMesh 的地图上退化为直线跟随）。Blackboard 键名是 C++ 与资产生成器共享的常量（`TargetActor`、`LastKnownLocation`、`HomeLocation`、`InAttackRange`、`PatrolLocation`），手写的 BT 必须用同名键。
队伍识别走 `IGenericTeamAgentInterface`：玩家 `TeamId=1`、敌人 `2`（`MALTeam` 命名空间，`EditDefaultsOnly` 不复制）。

### 5.7 UI 三层

| 层 | 类 | 职责边界 |
| --- | --- | --- |
| 子系统 | `UMALUISubsystem`（LocalPlayer） | 「开着什么、谁有焦点、游戏是否暂停」——持久 HUD + 模态菜单栈 |
| 子系统 | `UMALDamageNumberSubsystem`（LocalPlayer） | 只管生命周期：从池取 widget、放一个数、动画结束收回（避免战斗中频繁 CreateWidget 惊动 GC） |
| ViewModel | `UMALUIViewModelBase` + AbilityBar/AbilitySlot/AttributeRow/DamageNumber/Progression/SkillMenu/SkillNode/Stat | 只管「显示什么」 |
| Widget | `UMALUserWidget` → HUD / Menu → AttributeMenu / SkillMenu；AbilitySlot / AttributeRow / SkillNode / DamageNumber / WorldWidgetComponent | 只管布局与绑定，不做 gameplay 查询 |

菜单栈由 `AMALPlayerController::TogglePauseMenu/ToggleAttributeMenu/ToggleSkillMenu/CloseTopMenu` 驱动，每个键只关自己的面板（属性键不关暂停菜单）；暂停时输入不 tick，所以关闭动作必须由 UI 自己发起。
`FMALRetryPoll`（`UI/Framework/MALRetryPoll.h`）是通用重试轮询：客户端 HUD 创建早于 PlayerState 复制到达，绑定要在短计时器上重试。
冷却显示依赖契约：冷却 GE 必须授予 `MAL.Cooldown.*` Tag，否则 `GetCooldownTimeRemaining()` 读不到，HUD 永远看不到进度（`MALGameplayTags.h:143-151`）。

---

## 6. 内容资产地图

### 6.1 自制（`Content/Blueprint`，103 个资产）

| 目录 | 内容 |
| --- | --- |
| `Characters/MALCharacter` | `BP_MALCharacter`、`ABP_MALCharacter`、连招/攻击/Buff 蒙太奇（`AM_*`）、`ANS_MALMotionWarp`；`Locomotion/ABP_Locomotion_{Unarmed,Sword,Scythe}` + `ABP_LocomotionLayerInterface` |
| `Characters/EnemyCharacter` | `BP_StoneGolem` |
| `GameAbilitySyste/Ability` | `GA_Combo1Ability`、`GA_Combo2Ability`、`GA_WhirlwindAbility`、`GA_BuffAbility`；`Scythe/GA_Scythe_{Ability,Combo1}` + 对应蒙太奇；`GE_ComboAttackDamage` |
| `GameAbilitySyste/GameEffect` | `GE_{Primary,Secondary,Standalone}Attribute`、`GE_Buff_DamageUp`、`GE_BuffCost`、`GE_Cooldown_{Attack,Buff,Combo2,Whirlwind}` |
| `GameAbilitySyste/GameplayCue` | `GCN_Buff_DamageUp`、`GCN_Hit_Impact`、`GCN_Character_LevelUp` |
| `GameAbilitySyste/Weapons` | 6 把武器定义 `DA_Weapon_{Axe,Dagger,Hammer,Scythe,Spear,Sword}` + 6 个武器能力集 `DA_AbilitySet_*` + 6 个技能树 `DA_SkillTree_*`（共 18 个资产；**工作区新路径**） |
| `GameAbilitySyste/Data` | `DA_Warrior`（职业定义）、`CT_Experience`、`CT_LevelGrowth` |
| `AI` | `BT_StoneGolem`、`BB_StoneGolem`、`BP_StoneGolemAIController`、`GA_StoneGolemAttack`、`DA_StoneGolem{,AbilitySet}`、`AM_StoneGolemAttack_Seq`、`BP_AISpawner` |
| `Input` | `IMC_InputContext`、`DA_InputConfig`、12 个 `IA_*`（含 `IA_WeaponNext/Prev`、`IA_AttributeMenu/SkillMenu`、`IA_LockOnAction`） |
| `UI_1` | `WBP_MainHUD`、`WBP_HexAbilitySlot`、`WBP_AttributeMenu`、`WBP_SingalAttribute`、`WBP_SkillMenu`、`WBP_SkillNode`、`WBP_DamageNumber`、`WBP_EnemyHealthBar` + 冷却径向材质 |
| `GameMode` / `PlayerController` / `PlayerState` / `Actors` | `BP_GameMode`、`BP_MALPlayerController`、`BP_MALPlayerState`、`BP_EffectActor` 与调试用 GE |

### 6.2 外购 / 免费素材（占 Content 体积绝大多数）

| 包 | 用途 | 资产数 |
| --- | --- | --- |
| `Character/ParagonKwang` + `ParagonKwang` | 玩家角色网格与动画（自带内置剑，材质槽 `M05_Weapon` 绑 `weapon_r`） | 1601 + 154 |
| `GrruzamSwordPack` | 剑类武器与动作 | 1007 |
| `UI` | 图标/字体/GuiParts/RPG 职业图标 | 741 |
| `MythicalWeapons`、`FantasyWeapons`、`HeavyHammer` | 武器网格与材质 | 540 / 175 / 199 |
| `VFX`、`SlashTrailElemental`、`Free_Spells`、`ImpactEffects` | 拖尾、命中特效、法术 | 398 / 335 / 98 / 175 |
| `StoneGolem` | 怪物网格/动画 | 265 |
| `Frank_RPG_Archer` | 弓箭手动作集（原玩家弓资源，目前未使用） | 268 |
| `ArashiAkuma`、`FullMountAttacks` | 额外角色/动作素材 | 367 / 51 |
| `Sound/*`、`Sword Combat Powerful`、`Sword Fighting Ultimate SFX` | 挥砍/命中/战斗音乐采样 | 149 + 208 |

> 这些是演示/展示关卡也一并入库的素材包（各带自己的 `Maps/*.umap`）。使用前请遵循项目既有的外部美术资源版权合规与署名管理约定。

---

## 7. 工具链与自动化

### 7.1 Python 管线

系统 Python（需 numpy/scipy/Pillow/fontTools，**不是** UE 内嵌 Python）：

| 脚本 | 做什么 | 产物 |
| --- | --- | --- |
| `Scripts/generate_hit_sfx.py` | 程序化合成近战命中/挥砍音（numpy + scipy，零素材授权） | `Content/ProceduralAudio/*.wav` |
| `Scripts/build_hurt_sfx.py` | numpy 合成起音层 + REAPER 跑 DSP 链（饱和/瞬态/EQ/变调 elastique3） | 24-bit 受伤音效 |
| `Scripts/generate_skill_icons.py` | 从 game-icons.net 下载剪影、按元素染色、合成暗金边框；加技能只需加一行表 | `Scripts/.iconcache/*.png` + 技能图标 |
| `Scripts/generate_tts.py` | 用 Windows 自带中文 SAPI5 离线配音（无云端、无 Key） | 16 kHz 语音 wav |
| `Scripts/subset_damage_fonts.py` | 把 Bebas Neue / Archivo Black 子集化到 U+0020–007E，载荷降一个量级后送进 UE | 伤害数字字体 |
| `Content/Python/create_mal_anim_bp.py` | 在编辑器内批量创建父类为 `UMALAnimInstance` 的动画蓝图 | ABP 资产 |

### 7.2 编辑器 commandlet（`Source/MAL/Private/Editor`）

用于「Python 够不着」的资产工作（动画图 pin 不是 UObject，脚本层看不见连线和），统一走 `UnrealEditor-Cmd.exe <uproject> -run=...`，并遵循 `dump → apply → compile` 三段式：

| Commandlet | 作用 |
| --- | --- |
| `-run=MALLocomotionLayerSetup -Mode=dump\|apply\|compile` | 创建 `ABP_Locomotion_*`、搭图层函数图、把地面状态机换成 Layer 节点、插入 Inertialization、把武器定义指向图层 |
| `-run=MALGenerateAIAssets` | 一次性写出 Stone Golem 的 Blackboard 与 Behavior Tree |

### 7.3 运行时调试钩子

- 控制台 Exec（`AMALPlayerController`）：`MALAddAttributePoints`、`MALSpendAttributePoint`、`MALEQUIP <资产路径>`、`MALCAM`。
- GameplayDebugger：自定义页 `MAL.AI`，固定小键盘槽位 6（0–5 已被引擎页占用），默认状态已设为 `EnabledInGameAndSimulate`；整页包在 `WITH_GAMEPLAY_DEBUGGER` 内，Shipping 自动编掉。

### 7.4 AI 代理接入

`.mcp.json` 指向 `http://127.0.0.1:8000/mcp`（`unreal-mcp`，HTTP 型），配合 `.uproject` 里的 `ModelContextProtocol` / `AllToolsets` / `MVVMToolset` 插件，让代理能直接读改编辑器资产。相关能力边界与调用规范见项目记忆（UMGToolSet 参数格式、`CompileWidgetBlueprint` 需完整对象路径、Niagara/UMG 动画资产创建限制等）。

---

## 8. 当前状态与缺口

### 8.1 开发轨迹（31 次提交，2026-10-21 → 10-30）

主题演进清晰，可作为功能落地顺序的参考：
GAS 骨架与 Character 重构 → 命中停顿/镜头抖动 → 血条与伤害飘字 → 死亡/Buff 技能 → 连招与后摇 → 音效与字体 → 技能框架重构 → 属性面板与加点 → UI 架构重构 → 技能菜单 → 武器切换（滚轮 + 技能由武器决定 + UI 同步修复）→ 按武器切换动画 → 射击技能（弓）→ 移除弓与上下半身分层。

### 8.2 工作区未提交改动（截至快照）

- `12` 个 M、`260` 个 D、`10` 个 ??：**武器数据资产整体搬迁** `Content/Blueprint/Data/Weapons/**` → `Content/Blueprint/GameAbilitySyste/Weapons/**`；同时删除了 `Content/Frank_RPG_Sword1/**` 全部动画资产。
- 新增未跟踪：镰刀技能组（`Ability/Scythe/**`、`ABP_Locomotion_Scythe`、`BS_Bow_Move`）、`GrruzamSwordPack/`。
- 存在两条备份分支：`backup/skill-menu-wip-c504277`、`backup/wip-before-reset-b5fbd80`。
- ⚠️ LFS 项目下做资产回退/删除前必须先确认对应对象可取回；历史上已确立「破坏性 git 操作前先建备份分支」的约束。

### 8.3 已知缺口（按影响排序）

| # | 缺口 | 证据 |
| --- | --- | --- |
| 1 | **Primary Asset 扫描目录失效**：`DefaultGame.ini:20` 仍指向 `/Game/Data/Weapons`，而 `Content/Data` 已不存在（资产在 `/Game/Blueprint/GameAbilitySyste/Weapons`）→ 武器定义不再被 AssetManager 登记，依赖 `GetPrimaryAssetId` 的加载/寻址路径可能失效 | 实测 `Test-Path Content/Data = False` |
| 2 | **没有背包/拾取**：`Loadout` 是组件上 `EditDefaultsOnly` 的手配列表，唯一入口是调试命令 `MALEQUIP`；换弹路径缺失（`MagazineSize` 目前恒为 `INDEX_NONE`，即无限） | `MALWeaponComponent.h:199-205`、`MALWeaponDefinition.h:183-190` |
| 3 | **锁定瞄准（LockOn）已实现**：`UMALLockOnComponent` 挂在 PC 上（角度/距离/视线筛选、只驱动 Yaw、HUD 角括号标记）；输入映射与闪避授予由 `Scripts/wire_m3_assets.py` 写入 | `Source/MAL/Public/Combat/MALLockOnComponent.h` |
| 4 | `UMALClassDefinition::SkillTree` 已废弃但字段保留（技能树随武器走）；`GrantedAbilitySets` 与武器层的边界靠 SourceKey 维持，改动需谨慎 | `MALClassDefinition.h:29-36` |
| 5 | **零自动化测试、零 CI**：验证依赖编译 + 手动 PIE + `Saved/Logs/MAL.log` 复盘，commandlet 是唯一可脚本化的回归手段 | `IMPLEMENT_SIMPLE_*` 计数为 0 |
| 6 | 联机仅到「按服务器权威编写」的程度：无房间/会话、无 `OnlineSubsystem` 依赖、无专用服务器下的 UI 兜底验证，且暂停/输入恢复等边界仍留有已知问题（见 §9 第 4 条） | `MAL.Build.cs:64-67` |
| 7 | 关卡仍是 Demo 级（`Untitled` / `TestMap` + 各素材包自带展示图），无 World Partition 正式流程、无波次/刷怪设计（只有 `BP_AISpawner`） | `DefaultEngine.ini:3-5` |
| 8 | `ANS_MALMotionWarp` 资产存在但 C++ 无 MotionWarping 依赖/引用，位移靠 RootMotion + `LaunchCharacter`；需确认该 Notify 是否仍在用 | `MAL.Build.cs` 无 `MotionWarping` |

---

## 9. 构建 / 运行 / 排查要点

1. **编译**：`Build.bat MALEditor Win64 Development -Project="d:\UE_Project\MAL\MAL.uproject" -WaitMutex`（历史耗时约 2 分钟）。MSVC 14.51 高于 5.8 推荐版本会报无害警告。
2. **编辑器锁 DLL**：编辑器运行时改动 C++ 不会热重载；UBT 会改写成 `UnrealEditor-MAL-000X.dll`。改过 `StartupModule` 必须彻底重启编辑器。
3. **日志噪声**：`Failed to load 'aqProf.dll' / 'VtuneApi*.dll' / WinPix` 属可选分析库缺失，非项目错误；`Saved/Logs/` 下大量 `*-backup-*.log` 是历史备份，不要据此判断当前状态。
4. **输入失效的经典成因**：攻击冷却（`GE_Cooldown_Attack` 约 1.2s）长于动作结束时间（约 0.5s）会在两刀间形成「输入黑洞」；另有 `bMovementLocked` 吞掉 Jump 的 `Completed`、死亡后 `DisableInput` 未恢复、暂停恢复不重发 `Started`、PIE 下视口失焦 flush 按键四个已知点。
5. **命中窗口/表现**：`FindComponentByClass<UPrimitiveComponent>()` 会先返回胶囊体，必须按名字遍历取；委托重复绑定会 Ensure Failed（`StartHitWindow` 开头先 `ReleaseCollision()`）；hit-stop 只冻结骨骼更新而不动 `CustomTimeDilation`，否则会卡住 CMC/ServerMove 让别的客户端看到攻击者抽搐。
6. **GAS 时序**：移除 GameplayTag 之后要重新校验有效性；`LinkedAnimLayer` 接口要求实现方自补接口方法并扫描资产；蒙太奇未结束 + `Status.Casting` 卡能力时需要超时兜底。
7. **Combo 窗口**：`ComboWindow` notify 必须早于 hit window，否则最后一段接不上。

更多经验以项目记忆为准（`project_architecture`、`common_pitfalls_experience`、`development_*_specification` 分类下已沉淀对应条目）。
