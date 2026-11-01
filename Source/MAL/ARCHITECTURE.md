# MAL 架构说明

> 与 [README.md](./README.md) 的分工：README 是**项目画像**（是什么、有什么、到哪一步）；本文是**架构**（怎么搭的、为什么这样搭、改从哪里下手）。
> 事实基线：UE 5.8.3，模块 `MAL`，快照 2026-11-01。所有类名/属性名/文件位置均来自当前代码，未做任何推测性描述；少量「待确认」已标注。

---

## 1. 架构一句话

**以 GAS 为唯一运行时骨架，把「战斗状态」从技能里拆出来放进角色组件，把「战斗配置」从代码里拿出来放进武器数据资产，再用一条服务器权威的结算总线把伤害、表现、成长三者解耦。**

三条主轴决定了整个架构的形状：

| 主轴 | 含义 | 直接后果 |
| --- | --- | --- |
| **能力无状态** | `UGameplayAbility` 每次激活都从干净状态开始 | 连招/命中窗/弹药的运行时状态必须在 `UMALComboComponent`、`UMALWeaponComponent` 上 |
| **武器即配置** | 一把武器 = 网格 + 命中盒 + 伤害倍率 + 蒙太奇 + 段名 + 能力集 + 技能池 | 同一个 GA 资产可驱动剑与巨剑；差异全在 `UMALWeaponDefinition` |
| **结算与表现分家** | 只有服务器结算伤害，表现通过事件总线下发 | `AttributeSet` 不认识 HUD，`Ability` 不认识 Widget |

---

## 2. 分层视图

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ L0 宿主 / 装配    AMALGameMode（固定 Pawn/PC/PS 三类）· FMALModule（Tag 预热、  │
│                   GameplayDebugger 页注册）· 各 Subsystem                       │
├──────────────────────────────────────────────────────────────────────────────┤
│ L1 输入           UMALInputConfig(Tag↔Action) · UMALInputComponent(三参事件)     │
│                   AMALPlayerController(分流 + 模态菜单 + 调试 Exec)              │
├──────────────────────────────────────────────────────────────────────────────┤
│ L2 GAS 运行时     UMALAbilitySystemComponent(授予/缓冲/SourceKey)                │
│                   UMALGameplayAbility 族 · GameplayEffect 族 · UMALAttributeSet  │
│                   UMALDamageExecutionCalculation · FMALGameplayEffectContext     │
├──────────────────────────────────────────────────────────────────────────────┤
│ L3 战斗机制       UMALWeaponComponent(命中窗/装备/弹药) · UMALComboComponent      │
│                   UMALDamageExecutor(唯一结算入口) · AMALProjectile              │
│                   AnimNotify 族(把蒙太奇帧翻译成机制调用)                        │
├──────────────────────────────────────────────────────────────────────────────┤
│ L4 数据资产       UMALWeaponDefinition · UMALAbilitySet · UMALClassDefinition    │
│                   UMALSkillTreeDefinition · UMALGameplayTags                     │
├──────────────────────────────────────────────────────────────────────────────┤
│ L5 表现 / UI      UMALCombatEventsSubsystem(结算总线) · UMALUISubsystem           │
│                   UMALDamageNumberSubsystem · 9 ViewModel · 11 Widget            │
│                   UMALAnimInstance + Linked Anim Layer                           │
├──────────────────────────────────────────────────────────────────────────────┤
│ L6 持久化 / AI    UMALSaveSubsystem(单档) · UMALProgressionComponent              │
│                   AMALEnemyAIController(代码构建行为树 + BTTask 族)               │
└──────────────────────────────────────────────────────────────────────────────┘
```

依赖方向自上而下**单向**；唯一的反向通道是委托/事件（L3→L2 的 `OnWeaponHit`、L2→L5 的伤害总线、L3→L2 的 Combo 广播），且都不携带类型耦合。

---

## 3. 对象拓扑与生命周期

### 3.1 谁持有谁

```
AMALPlayerState  (跨 respawn / 跨 map 存活，GAS 宿主)
 ├─ UMALAbilitySystemComponent          ← 授予、输入缓冲、EffectContext 工厂
 ├─ UMALAttributeSet                     ← Primary/Secondary/Standalone/Meta
 ├─ UMALProgressionAttributeSet          ← Level/Exp/AttributePoints/SkillPoints
 └─ UMALProgressionComponent             ← 加点、升级、解锁技能、触发存档

AMALCharacterBase : ACharacter           (玩家与敌人共用基类)
 ├─ UCharacterMovementComponent
 ├─ USpringArmComponent / UCameraComponent
 ├─ USkeletalMeshComponent               ← ABP_MALCharacter : UMALAnimInstance
 ├─ UMALWeaponComponent                  ← 命中窗 + 装备 + 弹药（玩家与敌人都有）
 ├─ UMALComboComponent                   ← 连招状态机
 ├─ UStaticMeshComponent   WeaponMesh        ┐ 两个组件而非一个：
 ├─ USkeletalMeshComponent WeaponSkeletalMesh ┘ 一个组件只能画静态或骨骼网格
 └─ (缓存) AbilitySystemComponent / AttributeSet 指针

AMALCharacter      : AMALCharacterBase   ← ASC/属性取自 PlayerState；ActorInfo=(PS, Pawn)
AMALEnemyCharacter : AMALCharacterBase   ← ASC/属性是自己的 DefaultSubobject；ActorInfo=(this, this)
 └─ UMALWorldWidgetComponent HealthBarWidget

AMALPlayerController
 ├─ UMALInputComponent
 └─ UMALHitFeedbackComponent             ← 本地 hit-stop（挂在 PC，不挂在 Pawn）

UMALGameplayAbility 子类实例             ← 只持配置与本次激活的临时对象（蒙太奇任务、
                                            lunge 任务、计时器句柄），不持跨刀状态
```

### 3.2 生命周期矩阵

| 对象 | 创建时机 | 销毁时机 | 存活范围 |
| --- | --- | --- | --- |
| `AMALPlayerState` + 其四件套 | 登录 | 玩家离开 | 跨 respawn、跨关卡 |
| Pawn 上的组件 | Actor 构造 | Pawn 销毁 | 一次附身 |
| Ability 实例 | `GrantAbility`（服务器） | `ClearGrantedAbilities` / 换武器 / 买技能 | 一次授予 |
| `UMALUISubsystem` / `UMALDamageNumberSubsystem` | LocalPlayer 存在时 | 登出 | per-local-player（专用服务器上不存在） |
| `UMALCombatEventsSubsystem` | World 存在 | World 销毁 | per-world |
| `UMALSaveSubsystem` | GameInstance | GameInstance | per-session |
| `UMALGameplayTags` 单例 | `StartupModule` 强制预热 | 进程结束 | per-process |

**为什么 ASC 挂 PlayerState**：玩家Pawn 会因死亡/重生被替换，而成长与已授予能力必须存活；敌人没有 PlayerState，所以基类用 `InitializeAbilitySystem()` 统一吸收两种归属（`MALCharacterBase.h:40-48`），子类不得各自重写 GAS 初始化。

---

## 4. 依赖不变量（改代码前先读这页）

这些约束在源码注释里是显式写明的，破坏它们会立刻引发历史bug：

1. **组件不认识 Ability。** `UMALComboComponent` 不调用 `MontageJumpToSection`，只广播 `OnComboAdvanced(Step, Section)`；`UMALWeaponComponent` 不认识技能类，只按 `FName` 找碰撞体。→ 于是 AI 的一次挥砍和玩家的四段连招能共用同一套组件。
2. **AttributeSet 不认识 HUD/PlayerController。** 属性折叠完成后向 `UMALCombatEventsSubsystem` 发布事件，由总线决定谁该看到（`MALCombatEventsSubsystem.h:12-24`）。
3. **AnimNotify 不认识具体能力类。** 统一经 `IMALComboAttackInterface::GetFromMesh()`（`Owner → IAbilitySystemInterface → ASC → GetAnimatingAbility`）回到能力，接口方法带默认空实现，让单段挥砍只实现自己需要的钩子。
4. **表现不走复制，走定向 RPC。** hit-stop、飘字、震屏都由服务器**决定发给谁**，而不是把状态复制出去再各自推断（`ClientShowDamageNumber` 故意为 Unreliable）。
5. **段名属于蒙太奇的持有者。** `ComboSections` 在 `UMALWeaponDefinition` 上，能力侧用 `ResolveComboSections()` 做「武器优先、自身兜底」；读错列表会静默跳到不存在的段（表现为「二段挥不出来」）。
6. **一个 SourceKey 只由一个持有者清理。** 角色 kit 在 `NAME_None`，武器层在 `WeaponAbilitySourceKey`。买技能会重建 kit，若武器能力也在 `NAME_None` 就会被一起删掉且没人加回来。
7. **枚举而非自由属性引用。** 加点走 `EMALPrimaryStat` + `MALGetPrimaryAttribute()`，UI 与组件共用同一个 switch，不可能对不齐。
8. **服务器 API 一律标 `BlueprintAuthorityOnly`**（`EquipWeapon`/`UnequipWeapon`/`CycleWeapon`/`TryConsumeAmmo`），客户端要改必须走 PC 的 Server RPC。
9. **Editor-only 代码不进公共接口。** `Private/Editor/**` 的 commandlet 依赖 `UnrealEd/AnimGraph`，`MAL.Build.cs` 用 `Target.bBuildEditor` 包起来，Game target 编不到。
10. **调试代码包在 `WITH_GAMEPLAY_DEBUGGER` 内**，Shipping 自动消失。

---

## 5. GAS 运行时架构

### 5.1 能力授予：双层 SourceKey

```
                        ┌────────────────────────────────────────────┐
UMALClassDefinition ───▶│ SourceKey = NAME_None                      │
  .GrantedAbilitySets   │  · 职业基础能力（GA_Combo1 / GA_Buff …）    │
UMALSkillTreeDefinition │  · 已购买技能节点                          │
  （武器持有，见 §5.4）──▶│ 重建者：RefreshGrantedAbilities()           │
                        └────────────────────────────────────────────┘
                        ┌────────────────────────────────────────────┐
UMALWeaponDefinition ──▶│ SourceKey = WeaponAbilitySourceKey          │
  .GrantedAbilitySets   │  · 这把武器自带的挥砍能力              │
                        │ 重建者：RefreshWeaponGrantedAbilities()     │
                        └────────────────────────────────────────────┘
```

* 授予的唯一入口是 `UMALAbilitySystemComponent::GrantAbility(Class, InputTag, Level, SourceKey)`：把 `InputTag` 写进 `DynamicAbilityTags`，并把 handle 记进 `GrantedAbilityHandles[SourceKey]`。**绕过它授予的 skill 逃出了登记表，重新授予时不会被清掉，于是叠加。**
* `RefreshGrantedAbilities()` 必须「清 spec 的同时也清授予标志」：`GrantAbilities` 自身拒跑第二次，只清 spec 会让能力永远回不来。
* 幂等性是设计目标：换武器、买技能、读档、重生都调同一对 Refresh，顺序无关。

### 5.2 输入 → 激活：Lyra 帧缓冲模式

```
EnhancedInput 触发 ──▶ UMALInputComponent：把 (InputTag, Value, ETriggerEvent) 塞进
                       FMALInputActionEvent（动态三参委托，蓝图也能绑）
                          │
                          ▼
              AMALPlayerController::HandleInputActionEvent
                          │
        ┌─────────────────┴──────────────────┐
        ▼                                    ▼
  连续输入直连                        离散技能 → UMALAbilitySystemComponent
  Move / Look / Jump                  AbilityInputTagPressed / Released
  （不经 GAS，零延迟）                        │ 写入 InputPressed/Held/ReleasedSpecHandles
        │                                    │ + LastInputValues[Tag]
        ▼                                    ▼
  bMovementLocked / bMenuOpen /     PC::PostProcessInput → ProcessAbilityInput(DeltaTime, bGamePaused)
  bDead 三态在此吞输入                       │
                                             ▼
                              TryActivateAbility(handle) 或 InputPressed/InputReleased
                              （技能内可用 GetLastInputValue(Tag) 取回触发时的值）
```

两条策略（`EMALAbilityActivationPolicy`）：`OnInputTriggered`（按下即启）与 `WhileInputActive`（按住期间有效，松手由 ASC 取消，除非技能在 `InputReleased` 里自己收尾）。

### 5.3 Tag 契约表

| 前缀 | 例 | 谁写 | 谁读 | 破坏后果 |
| --- | --- | --- | --- | --- |
| `MAL.Input.*` | `Input.LMB`、`Input.RMB`、`Input.F`、`Input.WeaponNext`、`Input.LockOn` | `UMALInputConfig` 映射 | ASC 缓冲、技能 `InputActivatedTag` | 技能被授予但无按键能触发 |
| `MAL.Ability.*` | `Ability.Attack`、`Ability.Dodge` | 资产 `AbilityTags` | AI 的 `BTTask_ActivateAbilityByTag` | 行为树唤不出技能 |
| `MAL.Status.*` | `Attacking`、`Casting`、`Dodging`、`Invulnerable`、`Dead` | `ConfigureAbility()` 写进 OwnedTags | PC 吞输入、其他技能 `ActivationBlockedTags`、动画 | 重激活阻断失效 / 移动锁死 |
| `MAL.Cooldown.*` | `Cooldown.Attack`、`Cooldown.Whirlwind` | 冷却 GE 授予 | HUD 调 `GetCooldownTimeRemaining` | **冷却条永远不动**（引擎从 owner 的 tag 反查冷却） |
| `MAL.Weapon.*` | `Weapon.Sword` | `WeaponDefinition.WeaponTag` | 技能树节点门禁 | 技能池与武器对不上 |
| `MAL.Data.*` | `Data.Damage`、`Data.DamageBonus` | `SetByCaller` | Execution / GE | 伤害恒为 0 |
| `MAL.Damage.Type.Magic` | — | GE 的 asset/dynamic tag | `UMALDamageExecutionCalculation` | 走错攻防通道 |
| `MAL.Event.*` | `Event.Hit`、`Event.WeaponHit` | Notify / Executor 发 GameplayEvent | 蓝图 `WaitGameplayEvent` | 事件驱动技能收不到帧 |
| `GameplayCue.*` | `Hit.Impact`、`Buff.DamageUp`、`Character.LevelUp` | Executor / Progression | Cue Notify 蓝图（音/画） | 命中无声、升级无反馈 |

`UMALGameplayAbility::ConfigureAbility(AbilityTag, InputTag, StatusTag, bBlocksSameStatusActivation)` 是上述契约的写入口：把「一次配置三件事」收进一个调用，并把 StatusTag 顺手设为同类激活阻断 —— 两个攻击不重叠就是靠它，而不必每个技能自己再声明一遍。

### 5.4 技能树：节点是「池」，解锁记录是「武器×技能」二元组

```
UMALSkillTreeDefinition (每个武器一份池)
   └─ FMALSkillNodeDefinition { SkillTag, DisplayName, Description, Icon,
                                AbilityClass, InputTag, RequiredLevel, Cost,
                                Tier, Prerequisites[] }
FMALUnlockedSkill { WeaponTag, SkillTag }   ← 存档与进度组件持有的购买记录
```

* 节点必须自带展示信息：**未授予的能力在 ASC 里没有 spec，面板无从读取任何东西**。
* 购买记录带 `WeaponTag` 配对：同一 `SkillTag` 出现在两把武器的池里时，扁平 tag 集合会让「在 A 上买的」静默解锁 B 的那一个。`WeaponTag` 用精确相等而非层级比较。
* 节点身份是 `SkillTag` 而非能力类：能力可以被换皮替换/改名，而玩家的存档必须继续指向同一节点。
* 换武器只是**隐藏**该武器的节点，不退点；切回来时按 `WeaponTag` 重新过滤授予。
* `UMALClassDefinition::SkillTree` 已废弃（保留字段只为旧资产能加载），技能池的唯一持有者是武器定义。

---

## 6. 战斗管线（一次近战的全过程）

```
① 蒙太奇时间轴（引擎在跑）
   AM_AttackAbility 的 notify 轨道：MAL Combo Window / MAL Weapon Collision / MAL Lunge
        │  IMALComboAttackInterface::GetFromMesh(Mesh) → 当前驱动蒙太奇的能力
        ▼
② 能力收到帧事件
   EnableWeaponCollision() → UMALWeaponComponent::StartHitWindow(ComboStep)
        │  · 客户端直接 return（权威判定）
        │  · 先 ReleaseCollision() 再绑定 → 否则委托重复绑定 Ensure Failed
        │  · 按 FName 解析碰撞体（"weapon_collision"）并强制 QueryOnly/WorldDynamic/仅 Pawn Overlap
        │  · 命中盒尺寸来自 EquippedWeapon->HitBoxExtent/RelativeLocation
        ▼
③ Overlap → FMALHitResult{HitActor, WeaponSocketName, ComboStep, HitLocation}
        │  · 每刀一个 TSet<TWeakObjectPtr<AActor>> 去重
        │  · 队伍过滤走 IGenericTeamAgentInterface（Player=1 / Enemy=2）
        ▼
④ 能力 OnWeaponHit(Hit) → BuildHitRequest()（子类扩展点）→ UMALDamageExecutor::ExecuteWeaponHit
        │  ComputeDamage()：BaseDamage × LevelScale × (1 + ComboBonusPerStep × Step) × Weapon.DamageMultiplier
        │  → SetByCaller(MAL.Data.Damage) → 应用 DamageGEClass（默认 UMALDamageGameplayEffect）
        │  → ApplyKnockback（服务器；固定距离 RootMotion 而非发射冲量，死者不推）
        │  → HitStop RPC → Impact Cue → Event.WeaponHit
        ▼
⑤ UMALDamageExecutionCalculation（服务器）
        Damage = Base × (1 + Atk/100) × (100/(100 + Def)) × (1 + DamageBonus/100)
        暴击：Roll[0,100) < CritChance → Damage × CritDamage/100
        通道：spec 上有 MAL.Damage.Type.Magic 走魔法，否则物理
        结果写进 Meta 属性 Damage，暴击/通道位写进 FMALGameplayEffectContext
        ▼
⑥ UMALAttributeSet::PostGameplayEffectExecute：Damage → Health，并发布
        FMALDamageResolvedEvent → UMALCombatEventsSubsystem（World 子系统，仅服务器）
        ▼
⑦ 总线分发（决定谁该看到）
        ├─ 攻击者 PC：ClientOnHitLanded（Reliable）→ 震屏 + 本地 hit-stop
        ├─ 受害者 PC：ClientShowDamageNumber（Unreliable）→ 池化飘字
        ├─ 击杀归属：AMALCharacterBase::HandleDamageResolved → HandleDeath(Killer)
        └─ 经验：UMALProgressionComponent::HandleDamageResolved（自己过滤自己的击杀）
```

### 6.1 hit-stop 的实现选择（本文档最容易踩的一条）

`UMALHitFeedbackComponent` **只置 `USkeletalMeshComponent::bNoSkeletonUpdate` 冻结姿势**，不动 `CustomTimeDilation`：后者会连 CharacterMovement 与 ServerMove 一起停住，别的客户端会看到攻击者抽搐。副作用是蒙太奇时间轴也会停，于是 `UMALAnimInstance::AdvanceMontagesOnly()` 手动推进蒙太奇并触发 notify —— **姿势冻住，机制时钟继续按权威走**。
`NM_ListenServer` / `NM_DedicatedServer` 直接 return，host 只保留震屏（纯表现）。

### 6.2 远程分支（已移除）

玩家弓与其射击技能已从项目中移除，`AMALProjectile` 与 `UMALWeaponDefinition::ProjectileClass` 等通用弹道基础设施保留，供未来的远程/投掷类技能复用；`UMALDamageExecutor` 的友军/尸体/免疫过滤仍然适用于任何来源的命中。

---

## 7. 动画架构

```
ABP_MALCharacter : UMALAnimInstance ──────────┐  C++ 只喂数据：Speed/Direction/VelocityZ/
   │                                          │  bIsInAir/bHasMovementInput/AimOffset，
   ├─ Ground 状态机 ──▶ Linked Anim Layer 节点 ◀┤  不参与选段
   │                       ▲                  │
   │       ABP_Locomotion_Unarmed / _Sword / _Scythe
   │            （由 UMALWeaponDefinition.LocomotionLayer 指定，
   │              OnEquippedWeaponChanged 在每台机器上重链）
   │
   └─ Montage 槽位
          全身槽 DefaultSlot      ← 近战蒙太奇（姿势整段归蒙太奇）
```

* **换武器不换 ABP**：用 Linked Anim Layer 而非 `SetAnimInstanceClass`，主图不动、每武器一个层资产，`LinkAnimClassLayers` 失败时按 tick 重试至多 30 次（约半秒，长于一次 hit-stop，短到能大声失败）。
* **无装备时的兜底**：`DefaultLocomotionLayer` 必须给一个合法层，否则 Layer 节点解析为空 → 参考姿势（T-pose）。
* Notify 是帧与机制之间唯一的通信方式（见 §4 第 3 条）。**时序约束**：`ComboWindow` notify 必须早于 hit window，否则最后一段接不上。
* 角色网格自带剑（Paragon Kwang 的 `M05_Weapon` 材质槽刚性绑在 `weapon_r`），所以武器定义带 `bHideBuiltInWeapon`：带自己网格的武器必须让内置剑隐形，换皮武器则保持可见。

---

## 8. UI / MVVM 架构

```
所有权三分，互不越界：
  UMALUISubsystem（LocalPlayer）   → 「开着什么、谁有焦点、游戏是否暂停」
      · HUDWidget（持久，PC BeginPlay 里 CreateHUD）
      · MenuStack（模态栈：Push/Pop 驱动输入模式与暂停态，bPausesGame 由面板声明）
      · 每个键只关自己那层（属性键不关暂停菜单）
  ViewModel（9 个）                → 「显示什么」
      UMALUIViewModelBase : UMVVMViewModelBase
      Stat / Progression / AbilityBar / AbilitySlot / SkillMenu / SkillNode /
      AttributeRow / DamageNumber / (MVVM 属性变更用 UE_MVVM_SET_PROPERTY_VALUE)
  Widget（11 个，全 Abstract）     → 「布局与绑定」，不做 gameplay 查询
      UMALUserWidget → UMALHUDWidget / UMALMenuWidget → AttributeMenu / SkillMenu
      叶子：AbilitySlot / AttributeRow / SkillNode / DamageNumber
      世界空间：UMALWorldWidgetComponent（敌人血条）
```

* **HUD 自己造 VM 并推进绑定**，蓝图侧只声明布局与绑定表达式，绝不自己在 tick 里查 ASC。
* **绑定时序**：客户端的 PlayerState 在 HUD 创建之后才复制到达，所以 ASC 解析通过 `FMALRetryPoll` 做短周期重试（这是全项目重试类问题的统一解法）。
* **飘字走池**：`UMALDamageNumberSubsystem` 取 widget → 放一个数 → `OnPlaybackFinished` 归还。每秒数个的场景若每次 `CreateWidget`，会在战斗中间惊动 GC。
* **技能槽外观**：C++ 暴露参数 + 蓝图事件钩子（冷却径向、染色、可用/不可用），换皮不需要改代码。
* **MVVM 与 SlateCore**：`FSlateColor` 出现在公共 ViewModel 上，所以 `SlateCore` 必须是 Public 依赖 —— 这类「为反射类型可见而升级依赖」的决定在 `MAL.Build.cs` 里逐条注明了原因。

---

## 9. AI 架构

```
AMALEnemyAIController（服务器侧大脑）
 ├─ Sight 感知 + 已结算伤害反应（被打也进目标选择）
 ├─ Blackboard：键名是 C++ 常量（MALAIBlackboardKeys）
 │    TargetActor / LastKnownLocation / HomeLocation / InAttackRange / PatrolLocation
 ├─ 行为树：代码构建，无需 BT 资产即可跑；设计师在蓝图上赋的资产优先
 │    Selector
 │      Sequence(attack) : BTTask_ActivateAbilityByTag → Wait(cooldown + 抖动)
 │      Sequence(patrol) : BTTask_PickPatrolPoint → BTTask_PatrolMoveTo → Wait(pause)
 │      Wait(0.25)   ← 重新驱动 Selector
 └─ Tick：追击仍在 Tick 里做（比一次树求值快，且无 NavMesh 时退化为直线）
```

* AI 释放技能**不引用能力类**，只填一个 `MAL.Ability.*` tag —— 换技能不动行为树，且玩家技能与敌人技能走同一个 `TryActivateAbility` 路径。
* 巡逻的移动请求只属于树：`PickPatrolPoint` 只写黑板，`PatrolMoveTo` 只走路，有目标时 `PatrolMoveTo` 失败，两条分支永不抢路径请求（历史上的 Tick 驱动巡逻正是这么打架的）。
* 敌我判定用 `IGenericTeamAgentInterface` + `TeamId`（`EditDefaultsOnly`，不复制：每台机器从 archetype 读同一个值）。未分配队伍的字符对所有人中立，而不是「未分配的彼此友好」。
* `ExperienceReward` 写在敌人类默认上，玩家自己为 0 —— 只有敌人发布奖励。

---

## 10. 属性与成长模型

```
Primary  (Vigor, Strength, Dexterity, Endurance, Spirit)     ← 玩家加点的唯一落点
   │ 派生公式在 GE_SecondaryAttribute 资产里（以注释形式记在 UMALAttributeSet 头文件）
   │ C++ 的 PreAttributeChange 只做区间钳制：资源≤Max、暴击 0~100、
   │ 攻速 0.1~5.0、伤害加成 -90%~+500%（防止堆到免疫）、Damage 不为负
   ▼
Secondary(MaxHealth=100+Vigor×25, HealthRegen, MaxStamina, StaminaRegen,
          MaxMana, ManaRegen, Physical/MagicAttack, Physical/MagicDefense,
          CritChance, CritDamage, AttackSpeed, DamageBonus)
   ▼
Standalone(Health, Mana, Stamina)                             ← 当前资源，不随 Max 重算
Meta      (Damage)                                            ← 单次 GE 内的临时值，不复制

Progression(Level, Experience, ExperienceToNextLevel, AttributePoints, SkillPoints)
   ← 独立的 UMALProgressionAttributeSet：敌人也带战斗属性集，却没有成长可复制
```

> 派生关系写在资产而非代码里，意味着改公式不需要重编；代价是 C++ 侧公式与资产不同步时没有编译期拦截，只能靠头文件注释与面板显示发现。

* **升级发点而非直接写属性**：`UMALClassDefinition` 决定 `AttributePointsPerLevel` / `SkillPointsPerLevel` / `MaxCharacterLevel` / 单属性上限 / `ExperienceToNextLevelTable` 曲线。
* **升级不回血**是显式取舍（只改 Primary，Secondary 自动跟随，Standalone 不重应用）。
* 加点/购买都必须 `Server RPC → 权威写属性 → 复制回客户端`，客户端面板显示的永远是服务器同意过的值，所以三个 RPC 全部 `Reliable`。
* 存档刻意**不存** `ExperienceToNextLevel`：它由曲线派生，曲线一改就过期；而等级与已存经验是玩家事实。

---

## 11. 网络与权威模型

| 内容 | 机制 | 位置 |
| --- | --- | --- |
| 属性值 | `ReplicatedUsing` + `GAMEPLAYATTRIBUTE_REPNOTIFY` | `UMALAttributeSet`、`UMALProgressionAttributeSet` |
| 当前武器 | `Replicated EquippedWeapon` + `OnRep_EquippedWeapon` | `UMALWeaponComponent` |
| 死亡 | `ReplicatedUsing OnRep_IsDead`（为了客户端跑同一条表现路径； loose tag 与能力取消留服务器） | `AMALCharacterBase` |
| EffectContext | `NetSerialize`（暴击位、魔法通道位到每台机器都一样） | `FMALGameplayEffectContext` |
| 客户端→服务器 | `ServerSpendAttributePoint`、`ServerUnlockSkill`、`ServerEquipWeapon`、`ServerCycleWeapon`（全部 Reliable） | `AMALPlayerController` |
| 服务器→客户端 | `ClientOnHitLanded`（Reliable）、`ClientShowDamageNumber`（**Unreliable**） | `AMALPlayerController` |
| 全量广播 | `MulticastPlayLevelUpCue(NewLevel)`（`ExecuteGameplayCue` 只在调用处生效，所以升级反馈必须多播） | `UMALProgressionComponent` |

**权威清单（只在服务器发生）**：命中判定与去重、伤害结算与暴击掷骰、属性折叠、死亡裁决与击杀归属、能力授予与取消、经验与加点、弹药消耗、击退。
**每个机器都做**：装备表现（网格/隐藏内置剑/动画层）、蒙太奇播放、动画数据、HUD 与菜单、飘字与震屏（各自接收定向 RPC）。

结论：**这套架构按 listen server 写，但不依赖多客户端共存**。没有专用服务器验证流程、没有 `OnlineSubsystem` 依赖、`AMALGameMode` 只做类绑定。要真正做到多人联机，缺的是会话与关卡流程，不是权威模型。

---

## 12. 扩展配方（decision-complete）

### 12.1 加一把新武器（例如「钉头锤」）

1. `UMALGameplayTags.h/.cpp` 增 tag（若属于新家族）：`Weapon_Mace` = `MAL.Weapon.Mace`。
2. 建 `DA_Weapon_Mace`（`UMALWeaponDefinition`）：`WeaponTag`、`WeaponMesh` 或 `WeaponSkeletalMesh`、`AttachSocketName`、`MeshRelative*`、`bHideBuiltInWeapon`、`BuiltInWeaponMaterialSlot`。
3. 同资产填命中与数值：`HitBoxExtent`、`HitBoxRelativeLocation`、`DamageMultiplier`。
4. 动画：`LocomotionLayer`（指向新建的 `ABP_Locomotion_Mace`）+ `AttackMontage` + **`ComboSections` 必须与新蒙太奇的段名逐一对应**。
5. 技能：建 `DA_AbilitySet_Mace`，把 `GA_Combo1Ability` 子类（或复用）连同 Cost/Cooldown GE 填进去，挂到 `GrantedAbilitySets`。
6. 技能池：建 `DA_SkillTree_Mace`，节点里 `SkillTag`/`AbilityClass`/`InputTag`/`RequiredLevel`/`Cost`/`Tier`/`Prerequisites`。
7. 装配：把定义加进 `BP_MALCharacter` 的 `WeaponComponent.Loadout`（当前唯一「背包」），并确认 `/Game/.../Weapons` 与 `DefaultGame.ini` 的 `PrimaryAssetTypesToScan` 目录一致（见 README §8.3 第 1 条缺口）。
8. 验证：`MALEQUIP /Game/.../DA_Weapon_Mace` → 挥砍 → 检查命中日志/飘字/段名跳转 → 换回旧武器确认技能条重建。

> 近战不需要写一行 C++：`GetAbilityMontage()` 与 `ResolveComboSections()` 已经把「武器覆盖能力」这条链做完。

### 12.2 加一个新技能

1. 决定父类：单段挥砍 → `UMALAbility_MeleeAttack`；多段连招 → `UMALAbility_Attack`；增益 → `UMALAbility_Buff`；投射物 → 直接复用 `AMALProjectile`（当前无现成远程技能可参照）。
2. 构造函数里 `ConfigureAbility(AbilityTag, InputTag, StatusTag)` —— 三件事一起，别只填 AbilityTag。
3. 若要自定义伤害字段，重写 `BuildHitRequest()`，**不要复制 `OnWeaponHit`**。
4. 蒙太奇与段名留在武器定义上，能力里只留兜底值。
5. 需要冷却：做一个授予 `MAL.Cooldown.<Skill>` 的 GE（不授予 tag 的冷却在 HUD 上不可见）。
6. 需要消耗：`UMALManaCostGameplayEffect` + `CommitAbilityOrEnd()`（失败即结束并标记取消）。
7. 蓝图侧做成 `GA_*` 资产，进某个 `DA_AbilitySet_*`（武器层或职业层，取决于「换武器后是否还在」）。
8. 若要按键触发：在 `DA_InputConfig` 里加 `InputTag ↔ IA_*`，并在 `IMC_InputContext` 绑键；**只有被路由的 tag 才可达**（空 `InputTag` 只对被动技能有意义）。

### 12.3 加一个新属性

Primary：`EMALPrimaryStat` 增枚举项（`Count` 保持最后）→ `UMALAttributeSet` 增 `FGameplayAttributeData` + `ReplicatedUsing` + `ATTRIBUTE_ACCESSORS_BASIC` + `OnRep_*` → `GetLifetimeReplicatedProps` 注册 → `MALGetPrimaryAttribute()` 的 switch → `GE_PrimaryAttribute`/`CT_LevelGrowth` 资产。
Secondary：额外确认它在 `GE_SecondaryAttribute` 里的派生项（或是直接可配），并在 `PreAttributeChange` 里给它加钳制区间；面板呈现考虑 `EMALSecondaryStat`/`EMALStatFormat`。
Meta：加在 `UMALAttributeSet` 但**不注册复制**，在 `PostGameplayEffectExecute` 折叠进 Standalone。

### 12.4 加一个新 UI 面板

1. `UMAL<Name>ViewModel : UMALUIViewModelBase`（回答「显示什么」），数据从 ASC/PlayerState 解析，用 `UE_MVVM_SET_PROPERTY_VALUE` 抛变更。
2. `UMAL<Name>MenuWidget : UMALMenuWidget`（继承即获得打开/关闭与 `bPausesGame`），控件用 `BindWidgetOptional`，蓝图侧需 `ToggleWidgetAsVariable` 才能被 C++ 看到。
3. `AMALPlayerController` 加 `Toggle<Name>Menu()` → `UISubsystem->PushMenu/PopMenu`，并在 `UMALGameplayTags` 加 `Input_<Name>Menu`（非能力输入）+ `IA_*Action` + `DA_InputConfig` 条目。
4. 面板类挂到 PC 的 `TSubclassOf<UMALMenuWidget>` 属性上（留空即禁用）。

### 12.5 加一个新敌人

1. `BP_<Name> : AMALEnemyCharacter`；ASC/属性集已在基类，不需要额外装配。
2. `TeamId = 2`（`MALTeam::Enemy`）、`ClassDefinition`、`ExperienceReward`、`CorpseLifeSpan`、`CorpseMeshCollisionProfile`。
3. 需要挥砍：`UMALAbility_StoneGolemAttack` 式的 `UMALAbility_MeleeAttack` 子类（注意它显式关闭了后摇释放 —— AI 的一刀要跑完整段蒙太奇）。
4. AI 用 `AMALEnemyAIController` + 代码树即可；要定制则建 `BT_*`/`BB_*`，**黑板键名必须与 `MALAIBlackboardKeys` 完全一致**。
5. 武器碰撞体在网格上按名字加（`weapon_collision`），蒙太奇里放 `MAL Weapon Collision` notify（`bEnable=true/false` 成对）。

---

## 13. 关键架构决策（ADR 摘要）

| # | 结论 | 理由与代价 | 被否的替代 | 失效条件 |
| --- | --- | --- | --- | --- |
| D1 | ASC/属性挂 **PlayerState**，敌人在 Character | 成长与能力跨 respawn；代价是基类必须吸收两种归属 | 全挂 Pawn（死亡即丢档） | 若引入多角色同账号热切换，需重审 |
| D2 | 战斗状态放**组件**，不放 Ability | 一次激活必须干净起步；AI 与玩家共享实现；组件保持零 GAS 依赖 | 状态留在技能实例（换武器/取消后残留） | 若需要「同一角色多条独立连招」则不成立 |
| D3 | 伤害配置放 **WeaponDefinition**，段名也放这里 | 一个 GA 驱动多种武器；段名只在自己的蒙太奇里有意义 | 段名写在技能上（跨蒙太奇静默跳错段） | 若一把武器要同时挂多套蒙太奇则需分层 |
| D4 | 技能池挂在**武器**而非职业 | 换武器即换整套 build；存档按 `WeaponTag` 归档 | 全局共享池 + 职业持有（与武器化方向冲突） | 若加入「武器无关的通用技能」，需要第三个 SourceKey |
| D5 | 授予/清理按 **SourceKey 分组** | 买技能重建 kit 不能顺手拆掉武器技能 | 扁平 handle 列表（历史上正是这样丢能力的） | 若出现第三个授予方，沿用同一模式即可 |
| D6 | 输入走 **Lyra 帧缓冲**，连续输入直连 CMC | 离散技能要缓冲与「按住的值」，连续移动要零延迟 | 全部经 GAS（移动手感延迟）/ 全部直连（技能无法缓冲） | 若需要网络预测输入则需重估 |
| D7 | 表现经**结算总线**下发，不用复制状态推断 | 服务器决定发给谁；避免客户端预测重复飘字 | 复制伤害字段让各端自算（重复与不一致） | 若做客户端权威模式需整体反转 |
| D8 | hit-stop **只冻结骨骼**，手动推进蒙太奇 | 不动 `CustomTimeDilation` 才不会停 CMC/ServerMove | 时间膨胀（别的客户端看到攻击者抽搐） | 若要求蒙太奇本身也停（叙事演出）需另加通道 |
| D9 | 换武器用 **Linked Anim Layer** | 主图不动、资产隔离、Montage 槽共存 | 切 ABP 类 / 每武器一个状态机（切换与同步代价） | 若每武器改变移动学本身则需更多分支 |
| D10 | 动画图/UMG 深层结构改动走 **commandlet** | `UEdGraphPin` 不是 UObject，脚本层看不见连线；commandlet 可 dump/apply/compile 复现 | 手工在编辑器点（不可复现、不可评审） | 资产结构再次改版时脚本要跟着改 |
| D11 | 程序化音效/图标/字体子集用**外部 Python + REAPER** | 包络与染色在代码里比 DAW 更好表达；零授权成本 | 采购素材包（授权与检索成本） | 需要真录人声/乐器时不适用 |
| D12 | 存档是**单槽扁平对象** | 只有一个角色一个档，分结构只是多一层间接 | 每系统一个子结构 | 引入多角色存档时必须重构 |

---

## 14. 结构性技术债（区别于 README 的功能缺口）

这些不是「还没做」，而是「做了但形状会带来摩擦」：

1. **`AMALPlayerController` 正在变成 god object**：输入分流、菜单切换、HUD 创建、四个 Server RPC、两个 Client RPC、五个调试 Exec、武器组件监听、技能条刷新都在它身上。下一次再加面板或再加武器时，应把「菜单栈 + 输入模式」下沉到 `UMALUISubsystem`（已经做了一半），把调试 Exec 挪进独立的 `UMALDebugComponent`。
2. **可调参数双头存在**：`AbilityMontage` / `AttackMontage(废弃)` / `Weapon.AttackMontage`，`PostHitReleaseDelay`（Melee）/ `RecoveryReleaseDelay`（Attack），`ComboSections`（技能）/ `ComboSections`（武器）。优先级链由虚函数与 `Resolve*` 维持，名字刻意不同以防遮蔽——但新人极易读错，值得一次统一梳理或至少一张覆盖表。
3. **`PostLoad` 承担属性迁移**：旧资产的 `AttackMontage` 在加载时拷进 `AbilityMontage`。迁移窗口不该无限延长；一旦全部资产确认改好，应删掉废弃属性。
4. **UI 层是 C++↔蓝图双向契约**：`BindWidgetOptional` 的名字必须与 WBP 控件名一致，`bIsVariable` 必须手动打开；这类约定目前只存在于注释与记忆里，没有校验。可考虑在 `NativeConstruct` 里对必填控件做一次显式检查并大声报错。
5. **冷却/耗时数值散在资产里**：`GE_Cooldown_Attack` 与蒙太奇长度必须满足「冷却不长于动作」，否则出现输入黑洞；目前没有任何代码或工具校验这条关系。
6. **`UMALCombatEventsSubsystem` 与 `UMALAttributeSet` 直接耦合**（后者发布事件），总线的订阅方因此只能来自属性层；若以后要支持非 GAS 来源的伤害（陷阱、剧情），需要一个显式的发布入口。
7. **AI 追击在 Tick、巡逻在树**：职责清楚但排查时要同时看两处；追击与 `PatrolMoveTo` 的互斥靠「有目标时任务失败」这一约定维持，没有断言。
8. **回归只能靠编译 + 手工 PIE**：零自动化测试。commandlet 的 `dump`/`verify` 模式是目前唯一可脚本化的检查面（`MALLocomotionLayerSetup` 的 dump 是唯一样本），值得把「武器段名 vs 蒙太奇段名」「tag 是否被路由」这类一致性检查也做成同类 verify。

---

## 15. 代码 ↔ 资产接缝速查

| C++ 侧要求 | 必须存在的资产 | 命名/位置约定 |
| --- | --- | --- |
| `AMALGameMode` 固定三类 | `BP_GameMode`（可覆盖默认） | `/Game/Blueprint/GameMode` |
| `AMALCharacterBase::ClassDefinition` | `DA_Warrior` | `/Game/Blueprint/GameAbilitySyste/Data` |
| `UMALWeaponComponent::Loadout` | `DA_Weapon_*` | `/Game/Blueprint/GameAbilitySyste/Weapons/Weapon` |
| `StartHitWindow` 按名找碰撞体 | 角色蓝图上的 `weapon_collision` 组件 | 名字必须等于 `HitBoxComponentName` |
| 蒙太奇驱动机制 | `AM_*` 上的 `MAL *` notify | 段名 `Combo1..ComboN` / `PrimaryAttack_A...` |
| 技能条显示 | `GA_*` 资产的 `DisplayName` / `Icon` | 图标由 `Scripts/generate_skill_icons.py` 生成 |
| 冷却可见 | `GE_Cooldown_*` 授予 `MAL.Cooldown.*` | `/Game/Blueprint/GameAbilitySyste/GameEffect/Cooldown` |
| 命中有声 | `GCN_Hit_Impact` | `/Game/Blueprint/GameAbilitySyste/GameplayCue`（`GameplayCueNotifyPaths=/Game`） |
| 输入可达 | `DA_InputConfig` + `IMC_InputContext` + `IA_*` | `/Game/Blueprint/Input` |
| HUD/面板 | `WBP_MainHUD`、`WBP_AttributeMenu`、`WBP_SkillMenu`、`WBP_DamageNumber`、`WBP_EnemyHealthBar` | `/Game/Blueprint/UI_1`（控件名 = C++ 成员名） |
| 动画层 | `ABP_Locomotion_*` + `ABP_LocomotionLayerInterface` | `/Game/Blueprint/Characters/MALCharacter/Locomotion` |
| AI | `BT_StoneGolem` / `BB_StoneGolem`（可缺省，代码树兜底） | `/Game/Blueprint/AI`，黑板键名严格匹配 |

---

## 16. 修改守则 Checklist

- [ ] 改 C++ 前确认没有编辑器进程锁 DLL；改完必须重启编辑器（`StartupModule` 改动尤其如此）。
- [ ] 新公共属性若涉及反射类型（`UNiagaraSystem`、`FSlateColor`、`FGenericTeamId`…），检查对应模块是否需要升到 **Public** 依赖。
- [ ] 碰 `GrantedAbilities` 相关代码时，先问「这次清理影响哪个 SourceKey」。
- [ ] 碰命中/伤害时，先问「这一段在哪些机器上跑」，并确认它落在权威侧。
- [ ] 新加 Tag 时同步更新本文 §5.3 的契约表。
- [ ] 搬迁任何注册为 Primary Asset 的目录时，同步 `Config/DefaultGame.ini` 的 `PrimaryAssetTypesToScan`。
- [ ] 动画图/UMG 结构改动写进 commandlet，保留 `dump` 模式以便评审。
- [ ] `Content/` 是 LFS 资产：任何回退/删除前先确认 blob 可取回，并保留备份分支。
- [ ] 提交前用 `Saved/Logs/MAL.log` 验证模块加载与自定义页注册（忽略可选分析 DLL 缺失噪声）。
