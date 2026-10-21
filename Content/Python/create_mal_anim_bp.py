# Copyright Epic Games, Inc. All Rights Reserved.
#
# 在 Unreal Editor 中运行本脚本，创建父类为 UMALAnimInstance 的动画蓝图资产。
#
# 运行方式（任选其一）：
#   1) 编辑器菜单: Tools -> Execute Python Script... 选择本文件
#   2) Output Log 的 Python 命令行:
#        import create_mal_anim_bp
#        create_mal_anim_bp.main()
#
# 前置条件：
#   - 项目中已存在一个 Skeleton 资产（导入骨骼网格体后自动生成）。
#   - 已启用 "Python Editor Script Plugin"。
#   - C++ 模块 MAL 已编译（否则找不到 /Script/MAL.MALAnimInstance）。

import unreal

# ===================== 配置区 =====================
# 修改为你的骨架资产路径（Object Path，形如 /Game/目录/资产名.资产名）
SKELETON_PATH = "/Game/ParagonKwang/Characters/Heroes/Kwang/Meshes/Kwang_Skeleton.Kwang_Skeleton"

# 生成的动画蓝图存放位置与名称
ABP_PACKAGE_PATH = "/Game/Blueprint/Character"
ABP_ASSET_NAME = "ABP_MALCharacter"

# 动画蓝图的 C++ 父类（UMALAnimInstance 去掉 U 前缀）
PARENT_CLASS_PATH = "/Script/MAL.MALAnimInstance"
# =================================================


def _ensure_directory(package_path):
    """确保目标目录存在。"""
    if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
        unreal.EditorAssetLibrary.make_directory(package_path)


def _load_skeleton():
    skeleton = unreal.load_asset(SKELETON_PATH)
    if not skeleton:
        unreal.log_error(
            "[create_mal_anim_bp] 找不到 Skeleton 资产: {0}\n"
            "请先导入骨骼网格体，并把脚本顶部的 SKELETON_PATH 改成正确的路径。".format(SKELETON_PATH)
        )
        return None
    if not isinstance(skeleton, unreal.Skeleton):
        unreal.log_error(
            "[create_mal_anim_bp] {0} 不是 Skeleton 类型资产。".format(SKELETON_PATH)
        )
        return None
    return skeleton


def _load_parent_class():
    parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
    if not parent_class:
        unreal.log_error(
            "[create_mal_anim_bp] 找不到父类 {0}，请确认 MAL 模块已编译。".format(PARENT_CLASS_PATH)
        )
        return None
    return parent_class


def _set_prop(obj, name, value):
    """设置 UE 反射属性。

    UE Python 中带 Edit/BlueprintVisible 标记的属性才能用 obj.prop 直接访问，
    其余反射 UPROPERTY 需用 set_editor_property。这里两种都尝试，失败则打印可用属性。
    """
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception:
        pass

    try:
        setattr(obj, name, value)
        return True
    except Exception:
        pass

    available = [p for p in dir(obj) if not p.startswith("_")]
    unreal.log_error(
        "[create_mal_anim_bp] 无法设置 {0}.{1}。可用属性/方法: {2}".format(
            type(obj).__name__, name, available
        )
    )
    return False


def main():
    skeleton = _load_skeleton()
    if not skeleton:
        return None

    parent_class = _load_parent_class()
    if not parent_class:
        return None

    _ensure_directory(ABP_PACKAGE_PATH)

    full_path = "{0}/{1}".format(ABP_PACKAGE_PATH, ABP_ASSET_NAME)

    # 若已存在同名资产，先删除以便重建（避免 create_asset 失败）
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        unreal.log_warning("[create_mal_anim_bp] 已存在 {0}，将覆盖重建。".format(full_path))
        unreal.EditorAssetLibrary.delete_asset(full_path)

    factory = unreal.AnimBlueprintFactory()
    if not _set_prop(factory, "parent_class", parent_class):
        return None
    _set_prop(factory, "target_skeleton", skeleton)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    abp = asset_tools.create_asset(
        ABP_ASSET_NAME, ABP_PACKAGE_PATH, unreal.AnimBlueprint, factory
    )

    if not abp:
        unreal.log_error("[create_mal_anim_bp] 创建动画蓝图失败。")
        return None

    unreal.EditorAssetLibrary.save_asset(full_path, only_if_is_dirty=False)
    unreal.log(
        "[create_mal_anim_bp] 成功创建动画蓝图: {0} (父类={1}, 骨架={2})".format(
            full_path, PARENT_CLASS_PATH, SKELETON_PATH
        )
    )
    unreal.log(
        "[create_mal_anim_bp] 下一步：打开 {0} 编写 AnimGraph（状态机/BlendSpace），"
        "并在角色蓝图的 SkeletalMesh 上把 AnimClass 指定为该动画蓝图。".format(ABP_ASSET_NAME)
    )
    return abp


if __name__ == "__main__":
    main()
