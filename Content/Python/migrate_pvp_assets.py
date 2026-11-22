"""
PVPDemo 第三方美术资产迁移脚本 (UE 5.9)

将 /Game/InfimaGames/TacticalFPSAnimations 中的美术表现类资产，
按功能模块复制到 /Game/PVPDemo/，并统一改为 PVP 前缀命名。

源包只读，绝不修改；采用 duplicate_asset 保证资产注册与引用正确。

在编辑器 Output Log (Python 模式) 中执行方式：
  1) 完整流程：py "D:/UE/Unreal Projects/PVPDemo/Content/Python/migrate_pvp_assets.py"
  2) 仅扫描：先 import，再调用 m.scan()
  3) 仅迁移：m.migrate()
  4) 仅校验：m.verify()

产物目录：<Project>/Saved/MigrationReports/
"""

import json
import os
import re
from collections import Counter

import unreal

# ---------------------------------------------------------------------------
# 配置
# ---------------------------------------------------------------------------
SOURCE_ROOT = "/Game/InfimaGames/TacticalFPSAnimations"
TARGET_ROOT = "/Game/PVPDemo"

PROJECT_DIR = os.path.abspath(unreal.Paths.project_dir())
REPORT_DIR = os.path.join(PROJECT_DIR, "Saved", "MigrationReports")
MANIFEST_PATH = os.path.join(REPORT_DIR, "migration_manifest.json")
REPORT_PATH = os.path.join(REPORT_DIR, "migration_report.json")
SUMMARY_PATH = os.path.join(REPORT_DIR, "migration_summary.txt")

# 硬排除的资产类名（地图、输入、数据、枚举、结构体、控件蓝图等纯逻辑资产）
HARD_EXCLUDE_CLASSES = (
    "World", "MapBuildData", "UserDefinedStruct", "UserDefinedEnum",
    "DataTable", "DataAsset", "InputAction", "InputMappingContext",
    "WidgetBlueprint", "AnimBlueprint", "MovieScene",
)

# 纳入的美术表现类类名子串（作为前缀白名单之外的补充）
INCLUDE_CLASS_SUBSTRINGS = (
    "Texture", "StaticMesh", "SkeletalMesh", "Material", "Sound",
    "Anim", "Niagara", "Skeleton", "PhysicsAsset", "PoseAsset",
    "Font", "Curve", "IKRig", "IKRetarget", "Blend", "Submix",
    "Rig", "PaperSprite", "Particle",
)

# 硬排除的文件名前缀（逻辑资产）
EXCLUDE_NAME_PREFIXES = (
    "BP_", "ABP_", "WBP_", "BPI_", "IA_", "IMC_",
    "E_", "DA_", "DT_", "L_", "S_",
)

# 美术类型前缀白名单：即使类名是 Blueprint（如 AN_/ANS_ 动画通知、
# CR_ 控制 Rig），这些前缀也属于动画美术资产，应纳入迁移。
ART_NAME_PREFIXES = (
    "T_", "TC_", "SM_", "SK_", "SKM_", "SKEL_", "M_", "MI_", "MF_",
    "A_", "AM_", "AN_", "ANS_", "BS_", "AC_", "PHYS_", "PA_",
    "IK_", "RTG_", "CR_", "NS_", "NE_", "Font", "SC_", "SEF_",
    "Manny_",
)

# UE 官方资产类型前缀（用于非 TFA 资产的 PVP 插入）
KNOWN_TYPE_PREFIXES = {
    "T", "TC", "SM", "SK", "SKM", "SKEL", "M", "MI", "MF",
    "A", "AM", "AN", "ANS", "BS", "AC", "PHYS", "PA", "IK",
    "RTG", "CR", "NS", "NE", "Font", "SC", "SEF", "S",
}

# 按资产类名回退推导的类型前缀
CLASS_TYPE_PREFIX = {
    "Texture2D": "T", "TextureCube": "TC", "TextureRenderTarget2D": "T",
    "StaticMesh": "SM", "SkeletalMesh": "SK", "Skeleton": "SKEL",
    "Material": "M", "MaterialInstanceConstant": "MI", "MaterialFunction": "MF",
    "SoundWave": "A", "SoundCue": "A", "SoundClass": "SC", "SoundMix": "SEF",
    "AnimSequence": "A", "AnimMontage": "AM", "AnimComposite": "AC",
    "BlendSpace": "BS", "BlendSpace1D": "BS", "AnimNotifyState": "ANS",
    "PoseAsset": "PA", "PhysicsAsset": "PHYS",
    "NiagaraSystem": "NS", "NiagaraEmitter": "NE",
    "Font": "Font", "CurveFloat": "CR", "CurveVector": "CR",
    "IKRigDefinition": "IK",
}


# ---------------------------------------------------------------------------
# 工具函数
# ---------------------------------------------------------------------------
def _ensure_report_dir():
    if not os.path.isdir(REPORT_DIR):
        os.makedirs(REPORT_DIR)


def _write_json(path, data):
    _ensure_report_dir()
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, ensure_ascii=False)


def _write_text(path, text):
    _ensure_report_dir()
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(text)


def _class_name(asset_data):
    """从 AssetData 提取资产类名，兼容不同 UE5 小版本。"""
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        pass
    try:
        return str(asset_data.asset_class_path).split(".")[-1]
    except Exception:
        pass
    try:
        return str(asset_data.get_editor_property("asset_class")).split(".")[-1]
    except Exception:
        return "Unknown"


def _is_art_asset(name, class_name):
    """硬排除优先，然后按美术类型前缀白名单 / 类名子串纳入。"""
    for prefix in EXCLUDE_NAME_PREFIXES:
        if name.startswith(prefix):
            return False
    for bad in HARD_EXCLUDE_CLASSES:
        if bad.lower() in class_name.lower():
            return False
    for prefix in ART_NAME_PREFIXES:
        if name.startswith(prefix):
            return True
    for good in INCLUDE_CLASS_SUBSTRINGS:
        if good.lower() in class_name.lower():
            return True
    return False


def _make_new_name(orig_name, class_name):
    """TFA -> PVP；非 TFA 资产在类型前缀后插入 PVP。"""
    if "PVP" in orig_name:
        return orig_name, "already_pvp"

    replaced, count = re.subn(
        r"(^|_)TFA(?=_|$)",
        lambda m: m.group(1) + "PVP",
        orig_name,
    )
    if count:
        return replaced, "tfa_to_pvp"

    if orig_name.startswith("Manny_"):
        prefix = CLASS_TYPE_PREFIX.get(class_name, "A")
        return "%s_PVP_%s" % (prefix, orig_name), "manny_normalized"

    match = re.match(r"^([A-Za-z]+)_(.+)$", orig_name)
    if match and match.group(1) in KNOWN_TYPE_PREFIXES:
        return "%s_PVP_%s" % (match.group(1), match.group(2)), "prefix_inserted"

    prefix = CLASS_TYPE_PREFIX.get(class_name, "A")
    return "%s_PVP_%s" % (prefix, orig_name), "class_fallback"


def _map_target_folder(rel_dir):
    """源目录（相对 SOURCE_ROOT）→ 功能模块目标目录。"""
    rel = rel_dir.strip("/")
    if not rel:
        return "Common"

    if rel.startswith("Weapons/"):
        return rel
    if rel.startswith("Common/Characters/Mannequins"):
        suffix = rel[len("Common/Characters/Mannequins"):].strip("/")
        return "Characters/Mannequins" + ("/" + suffix if suffix else "")
    if rel == "Common/Core/Animation":
        return "Characters/Animations"
    if rel.startswith("Common/Core/Characters"):
        suffix = rel[len("Common/Core/Characters"):].strip("/")
        return "Characters" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/Core/Weapons"):
        suffix = rel[len("Common/Core/Weapons"):].strip("/")
        return "Weapons/Common" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/Environment"):
        suffix = rel[len("Common/Environment"):].strip("/")
        return "Environment" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/Props"):
        suffix = rel[len("Common/Props"):].strip("/")
        return "Props" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/VFX"):
        suffix = rel[len("Common/VFX"):].strip("/")
        return "VFX" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/AudioClasses"):
        suffix = rel[len("Common/AudioClasses"):].strip("/")
        return "Audio/Classes" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/Audio"):
        suffix = rel[len("Common/Audio"):].strip("/")
        return "Audio/Common" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/Materials"):
        suffix = rel[len("Common/Materials"):].strip("/")
        return "Common/Materials" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/Core/UI"):
        suffix = rel[len("Common/Core/UI"):].strip("/")
        return "UI" + ("/" + suffix if suffix else "")
    if rel.startswith("Common/Core"):
        suffix = rel[len("Common/Core"):].strip("/")
        return "Common" + ("/" + suffix if suffix else "")

    return "Common/" + rel


def _collect_source_assets():
    """通过 AssetRegistry 读取元数据，不加载完整资产。"""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    return list(registry.get_assets_by_path(SOURCE_ROOT, recursive=True))


def _build_manifest():
    """扫描、分类、命名并生成迁移清单。"""
    if not unreal.EditorAssetLibrary.does_directory_exist(SOURCE_ROOT):
        unreal.log_error("源目录不存在: %s" % SOURCE_ROOT)
        return None

    asset_datas = _collect_source_assets()
    unreal.log("源包资产总数: %d" % len(asset_datas))

    included = []
    excluded = []
    seen_targets = {}
    rule_counter = Counter()
    module_counter = Counter()

    for asset_data in asset_datas:
        package_name = str(asset_data.package_name)
        folder = str(asset_data.package_path)
        name = str(asset_data.asset_name)
        class_name = _class_name(asset_data)

        if not _is_art_asset(name, class_name):
            excluded.append({
                "source": package_name,
                "name": name,
                "class": class_name,
                "reason": "logic_or_excluded",
            })
            continue

        new_name, rule = _make_new_name(name, class_name)
        rel_dir = folder[len(SOURCE_ROOT):].strip("/")
        target_folder = _map_target_folder(rel_dir)
        target_path = "%s/%s/%s" % (TARGET_ROOT, target_folder, new_name)

        entry = {
            "source": package_name,
            "source_name": name,
            "class": class_name,
            "target": target_path,
            "target_folder": target_folder,
            "new_name": new_name,
            "naming_rule": rule,
        }

        if target_path in seen_targets:
            entry["status"] = "conflict_skip"
            entry["conflict_with"] = seen_targets[target_path]
        else:
            seen_targets[target_path] = package_name
            entry["status"] = "pending"

        included.append(entry)
        rule_counter[rule] += 1
        module_counter[target_folder.split("/")[0]] += 1

    manifest = {
        "source_root": SOURCE_ROOT,
        "target_root": TARGET_ROOT,
        "source_total": len(asset_datas),
        "included_total": len(included),
        "excluded_total": len(excluded),
        "naming_rules": dict(rule_counter),
        "modules": dict(module_counter),
        "assets": included,
        "excluded": excluded,
    }
    return manifest


# ---------------------------------------------------------------------------
# 公开阶段：扫描
# ---------------------------------------------------------------------------
def scan():
    """仅扫描并输出迁移清单，不修改任何资产。"""
    manifest = _build_manifest()
    if manifest is None:
        return None

    _write_json(MANIFEST_PATH, manifest)

    lines = [
        "PVPDemo 资产迁移扫描清单",
        "=" * 40,
        "源包资产总数 : %d" % manifest["source_total"],
        "纳入迁移     : %d" % manifest["included_total"],
        "排除(逻辑类) : %d" % manifest["excluded_total"],
        "",
        "命名规则分布 :",
    ]
    for rule, count in sorted(manifest["naming_rules"].items()):
        lines.append("  %-20s %d" % (rule, count))
    lines.append("")
    lines.append("功能模块分布 :")
    for module, count in sorted(manifest["modules"].items()):
        lines.append("  %-20s %d" % (module, count))
    conflicts = [a for a in manifest["assets"] if a["status"] == "conflict_skip"]
    lines.append("")
    lines.append("目标重名冲突 : %d" % len(conflicts))
    for entry in conflicts[:20]:
        lines.append("  %s <- %s" % (entry["target"], entry["source"]))
    lines.append("")
    lines.append("清单文件: %s" % MANIFEST_PATH)

    _write_text(SUMMARY_PATH, "\n".join(lines))
    unreal.log("\n".join(lines))
    return manifest


# ---------------------------------------------------------------------------
# 公开阶段：迁移
# ---------------------------------------------------------------------------
def migrate():
    """按清单复制资产到目标目录，幂等执行（已存在则跳过）。"""
    manifest = _build_manifest()
    if manifest is None:
        return None

    todo = [
        a for a in manifest["assets"]
        if a["status"] == "pending"
        and not unreal.EditorAssetLibrary.does_asset_exist(a["target"])
    ]
    already = [a for a in manifest["assets"] if a["status"] != "pending"]
    existing = [
        a for a in manifest["assets"]
        if a["status"] == "pending"
        and unreal.EditorAssetLibrary.does_asset_exist(a["target"])
    ]

    results = {
        "source_total": manifest["source_total"],
        "included_total": manifest["included_total"],
        "to_process": len(todo),
        "success": [],
        "failed": [],
        "skipped_existing": [a["target"] for a in existing],
        "conflict_skip": [a["target"] for a in already],
    }

    if todo:
        with unreal.ScopedSlowTask(len(todo), "迁移 PVP 美术资产...") as slow_task:
            try:
                slow_task.make_dialog(True)
            except Exception:
                pass
            for index, entry in enumerate(todo):
                if slow_task.should_cancel():
                    unreal.log_warning("用户取消，已处理 %d/%d" % (index, len(todo)))
                    break
                slow_task.enter_progress_frame(
                    1, "[%d/%d] %s" % (index + 1, len(todo), entry["new_name"])
                )
                target_dir = entry["target"].rsplit("/", 1)[0]
                if not unreal.EditorAssetLibrary.does_directory_exist(target_dir):
                    unreal.EditorAssetLibrary.make_directory(target_dir)
                try:
                    with unreal.ScopedEditorTransaction("Duplicate %s" % entry["new_name"]):
                        ok = unreal.EditorAssetLibrary.duplicate_asset(
                            entry["source"], entry["target"]
                        )
                    if ok:
                        unreal.EditorAssetLibrary.save_asset(
                            entry["target"], only_if_is_dirty=False
                        )
                        results["success"].append(entry["target"])
                    else:
                        results["failed"].append({
                            "target": entry["target"],
                            "error": "duplicate_asset returned False",
                        })
                except Exception as exc:
                    results["failed"].append({
                        "target": entry["target"],
                        "error": str(exc),
                    })
                    unreal.log_error("复制失败 %s: %s" % (entry["target"], exc))

    results["success_count"] = len(results["success"])
    results["failed_count"] = len(results["failed"])
    results["skipped_existing_count"] = len(results["skipped_existing"])
    results["conflict_count"] = len(results["conflict_skip"])
    _write_json(REPORT_PATH, results)

    unreal.log(
        "迁移完成: 成功 %d / 失败 %d / 已存在跳过 %d / 冲突跳过 %d"
        % (
            results["success_count"],
            results["failed_count"],
            results["skipped_existing_count"],
            results["conflict_count"],
        )
    )
    return results


# ---------------------------------------------------------------------------
# 公开阶段：校验
# ---------------------------------------------------------------------------
def verify():
    """校验目标资产数量、命名规范与源包完整性。"""
    manifest = _build_manifest()
    if manifest is None:
        return None

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    target_assets = list(registry.get_assets_by_path(TARGET_ROOT, recursive=True))
    target_paths = {str(ad.package_name) for ad in target_assets}

    expected = {a["target"] for a in manifest["assets"] if a["status"] == "pending"}
    missing = sorted(expected - target_paths)
    extra = sorted(target_paths - expected)

    bad_names = []
    for entry in manifest["assets"]:
        if entry["target"] in target_paths:
            name = entry["new_name"]
            # 必须含 PVP 标记且不再残留厂商前缀 TFA
            if "PVP" not in name or "TFA" in name:
                bad_names.append(entry["target"])

    source_assets = _collect_source_assets()
    source_intact = len(source_assets) == manifest["source_total"]

    report = {
        "source_count": len(source_assets),
        "source_intact": source_intact,
        "target_count": len(target_assets),
        "expected_count": len(expected),
        "missing_count": len(missing),
        "missing": missing[:100],
        "extra_count": len(extra),
        "extra": extra[:100],
        "bad_name_count": len(bad_names),
        "bad_names": bad_names[:100],
        "passed": (
            source_intact
            and len(missing) == 0
            and len(extra) == 0
            and len(bad_names) == 0
        ),
    }
    _write_json(os.path.join(REPORT_DIR, "migration_verify.json"), report)

    unreal.log(
        "校验%s: 源包 %d(完整=%s), 目标 %d/%d, 缺失 %d, 多余 %d, 命名异常 %d"
        % (
            "通过" if report["passed"] else "未通过",
            report["source_count"],
            report["source_intact"],
            report["target_count"],
            report["expected_count"],
            report["missing_count"],
            report["extra_count"],
            report["bad_name_count"],
        )
    )
    return report


def main():
    unreal.log("=== PVPDemo 美术资产迁移开始 ===")
    scan()
    migrate()
    verify()
    unreal.log("=== PVPDemo 美术资产迁移结束 ===")


if __name__ == "__main__":
    main()
