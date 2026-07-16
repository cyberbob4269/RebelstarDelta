# Rebelstar Delta - editor startup: import free NASA/SSS textures + build materials
# Force-refresh Earth 8k pack so pale/stale 2k day maps get replaced.
import unreal
import os

RAW = r"C:\UE\RebelstarDelta\Content\RawTextures"
TEX_PATH = "/Game/RD/Textures"
MAT_PATH = "/Game/RD/Materials"


def _log(msg: str) -> None:
    unreal.log(f"[RebelstarDelta] {msg}")


def _ensure_dir(package_path: str) -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
        unreal.EditorAssetLibrary.make_directory(package_path)


def _delete_if_exists(asset_path: str) -> None:
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
        _log(f"deleted stale {asset_path}")


def import_texture(filename: str, asset_name: str, srgb: bool = True, force: bool = False):
    src = os.path.join(RAW, filename)
    if not os.path.isfile(src):
        _log(f"missing texture file: {src}")
        return None
    dest = f"{TEX_PATH}/{asset_name}"
    if force:
        _delete_if_exists(dest)
    elif unreal.EditorAssetLibrary.does_asset_exist(dest):
        _log(f"texture exists: {dest}")
        return unreal.EditorAssetLibrary.load_asset(dest)

    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = TEX_PATH
    task.destination_name = asset_name
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.factory = unreal.TextureFactory()

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        tex = unreal.EditorAssetLibrary.load_asset(dest)
        try:
            if tex:
                tex.set_editor_property("srgb", srgb)
                # Keep full res for Earth beauty shots
                try:
                    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_World)
                    tex.set_editor_property("never_stream", True)
                except Exception:
                    pass
                unreal.EditorAssetLibrary.save_asset(dest)
        except Exception:
            pass
        _log(f"imported {dest} from {filename}")
        return tex
    _log(f"import may have used different name for {filename}")
    return None


def create_unlit_emissive_material(name: str, texture, emissive_boost: float = 1.0, force: bool = False) -> None:
    mat_path = f"{MAT_PATH}/{name}"
    if force:
        _delete_if_exists(mat_path)
    elif unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        _log(f"material exists: {mat_path}")
        return

    factory = unreal.MaterialFactoryNew()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = asset_tools.create_asset(name, MAT_PATH, unreal.Material, factory)
    if not mat:
        _log(f"failed to create material {name}")
        return
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        try:
            mat.set_editor_property("two_sided", True)
        except Exception:
            pass

        ts = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionTextureSample, -400, 0
        )
        ts.texture = texture
        ts.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

        mul = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionMultiply, -180, 0
        )
        boost = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant, -400, 180
        )
        boost.r = float(emissive_boost)
        unreal.MaterialEditingLibrary.connect_material_expressions(ts, "RGB", mul, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(boost, "", mul, "B")
        unreal.MaterialEditingLibrary.connect_material_property(
            mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )

        unreal.MaterialEditingLibrary.layout_material_expressions(mat)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(mat_path)
        _log(f"created unlit material {mat_path} boost={emissive_boost}")
    except Exception as e:
        _log(f"unlit material failed {name}: {e}")


def create_lit_textured_material(name: str, texture, force: bool = False) -> None:
    mat_path = f"{MAT_PATH}/{name}"
    if force:
        _delete_if_exists(mat_path)
    elif unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        _log(f"material exists: {mat_path}")
        return

    factory = unreal.MaterialFactoryNew()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = asset_tools.create_asset(name, MAT_PATH, unreal.Material, factory)
    if not mat:
        return
    try:
        ts = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionTextureSample, -350, 0
        )
        ts.texture = texture
        unreal.MaterialEditingLibrary.connect_material_property(
            ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
        )
        const_r = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant, -350, 200
        )
        const_r.r = 0.85
        unreal.MaterialEditingLibrary.connect_material_property(
            const_r, "", unreal.MaterialProperty.MP_ROUGHNESS
        )
        unreal.MaterialEditingLibrary.layout_material_expressions(mat)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(mat_path)
        _log(f"created material {mat_path}")
    except Exception as e:
        _log(f"material edit failed {name}: {e}")


def create_cloud_material(name: str, texture, force: bool = False) -> None:
    mat_path = f"{MAT_PATH}/{name}"
    if force:
        _delete_if_exists(mat_path)
    elif unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        return
    factory = unreal.MaterialFactoryNew()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = asset_tools.create_asset(name, MAT_PATH, unreal.Material, factory)
    if not mat:
        return
    try:
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        try:
            mat.set_editor_property("two_sided", True)
        except Exception:
            pass
        ts = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionTextureSample, -350, 0
        )
        ts.texture = texture
        mul = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionMultiply, -150, -80
        )
        white = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, -350, -200
        )
        white.constant = unreal.LinearColor(1.4, 1.4, 1.45, 1.0)
        unreal.MaterialEditingLibrary.connect_material_expressions(ts, "RGB", mul, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(white, "", mul, "B")
        unreal.MaterialEditingLibrary.connect_material_property(
            mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
        )
        # Opacity from red channel (cloud maps are greyscale)
        op_mul = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionMultiply, -150, 120
        )
        op_scale = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionConstant, -350, 200
        )
        op_scale.r = 0.85
        unreal.MaterialEditingLibrary.connect_material_expressions(ts, "R", op_mul, "A")
        unreal.MaterialEditingLibrary.connect_material_expressions(op_scale, "", op_mul, "B")
        unreal.MaterialEditingLibrary.connect_material_property(
            op_mul, "", unreal.MaterialProperty.MP_OPACITY
        )
        unreal.MaterialEditingLibrary.layout_material_expressions(mat)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(mat_path)
        _log(f"created cloud material {mat_path}")
    except Exception as e:
        _log(f"cloud material failed {name}: {e}")


def setup_moon_materials() -> str:
    _ensure_dir(TEX_PATH)
    _ensure_dir(MAT_PATH)

    # Force-refresh Earth 8k so we never keep a pale 2k day map
    FORCE_EARTH = True

    moon = import_texture("moon_2k_sss.jpg", "T_Moon_Color")
    if not moon:
        moon = import_texture("moon_texture.jpg", "T_Moon_Color")

    earth_day = import_texture("earth_8k_day.jpg", "T_Earth_Day", force=FORCE_EARTH)
    if not earth_day:
        earth_day = import_texture("earth_bluemarble.jpg", "T_Earth_Day", force=True)
    if not earth_day:
        earth_day = import_texture("earth_2k_day.jpg", "T_Earth_Day")

    earth_night = import_texture("earth_8k_night.jpg", "T_Earth_Night", force=FORCE_EARTH)
    if not earth_night:
        earth_night = import_texture("earth_2k_night.jpg", "T_Earth_Night", force=True)

    earth_clouds = import_texture("earth_8k_clouds.jpg", "T_Earth_Clouds", force=FORCE_EARTH)
    if not earth_clouds:
        earth_clouds = import_texture("earth_2k_clouds.jpg", "T_Earth_Clouds", force=True)

    mw = import_texture("milkyway_2k.jpg", "T_MilkyWay")

    if moon:
        create_lit_textured_material("M_RD_MoonGround", moon)
        create_unlit_emissive_material("M_RD_MoonGlobe", moon, 1.0)

    # Bright day map — unlit so it reads in void of space
    if earth_day:
        create_unlit_emissive_material("M_RD_Earth", earth_day, 2.2, force=FORCE_EARTH)
        create_unlit_emissive_material("M_RD_EarthDay", earth_day, 2.4, force=FORCE_EARTH)
    if earth_night:
        # City lights need high boost to read at distance
        create_unlit_emissive_material("M_RD_EarthNight", earth_night, 8.0, force=FORCE_EARTH)
    if earth_clouds:
        create_cloud_material("M_RD_EarthClouds", earth_clouds, force=FORCE_EARTH)
        create_unlit_emissive_material("M_RD_EarthCloudsSolid", earth_clouds, 1.1, force=FORCE_EARTH)
    if mw:
        create_unlit_emissive_material("M_RD_MilkyWay", mw, 1.0)

    return "OK: Earth 8k day/night/clouds force-refreshed under /Game/RD/"


try:
    result = setup_moon_materials()
    _log(result)
except Exception as e:
    unreal.log_warning(f"[RebelstarDelta] texture setup: {e}")
