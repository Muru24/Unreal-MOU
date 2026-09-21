"""
고글 렌즈 UI 머티리얼 및 블러 마스크 자동 생성 스크립트
실행 방법 (언리얼 에디터 Output Log 창):
  py "Content/Python/create_goggle_materials.py"
"""

import unreal

DEST_PATH = "/Game/05_JYH/MainPlayer/UI"

def create_goggle_lens_material():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    
    # 1. M_GoggleLens_UI 생성 (푸른 틴트 + 네온 림 라인)
    mat_name = "M_GoggleLens_UI"
    mat = asset_tools.create_asset(mat_name, DEST_PATH, unreal.Material, factory)
    if not mat:
        unreal.log_error(f"Failed to create material: {mat_name}")
        return None
        
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    
    lib = unreal.MaterialEditingLibrary
    
    # --- 파라미터 노드들 생성 ---
    # AspectRatio (16:9 = 1.7778)
    p_aspect = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, -200)
    p_aspect.set_editor_property("parameter_name", "AspectRatio")
    p_aspect.set_editor_property("default_value", 1.7778)
    
    # HalfWidth (0.60)
    p_width = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, 0)
    p_width.set_editor_property("parameter_name", "HalfWidth")
    p_width.set_editor_property("default_value", 0.60)
    
    # HalfHeight (0.36)
    p_height = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, 150)
    p_height.set_editor_property("parameter_name", "HalfHeight")
    p_height.set_editor_property("default_value", 0.36)
    
    # CornerRadius (0.18)
    p_radius = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, 300)
    p_radius.set_editor_property("parameter_name", "CornerRadius")
    p_radius.set_editor_property("default_value", 0.18)
    
    # EdgeSoftness (0.10)
    p_softness = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 450)
    p_softness.set_editor_property("parameter_name", "EdgeSoftness")
    p_softness.set_editor_property("default_value", 0.10)
    
    # OuterTintColor (Cyber Deep Blue)
    p_tint_col = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, -100)
    p_tint_col.set_editor_property("parameter_name", "OuterTintColor")
    p_tint_col.set_editor_property("default_value", unreal.LinearColor(0.02, 0.10, 0.30, 1.0))
    
    # OuterTintOpacity (0.4)
    p_tint_opac = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 100)
    p_tint_opac.set_editor_property("parameter_name", "OuterTintOpacity")
    p_tint_opac.set_editor_property("default_value", 0.40)
    
    # --- UV 연산 ---
    texcoord = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1800, -350)
    center_const = lib.create_material_expression(mat, unreal.MaterialExpressionConstant2Vector, -1800, -200)
    center_const.set_editor_property("r", 0.5)
    center_const.set_editor_property("g", 0.5)
    
    sub_center = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, -1600, -300)
    lib.connect_material_expressions(texcoord, "", sub_center, "A")
    lib.connect_material_expressions(center_const, "", sub_center, "B")
    
    # Aspect ratio scaling: P.x * Aspect, P.y * 1.0
    append_aspect = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -1200, -250)
    const_one = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -1400, -100)
    const_one.set_editor_property("r", 1.0)
    lib.connect_material_expressions(p_aspect, "", append_aspect, "A")
    lib.connect_material_expressions(const_one, "", append_aspect, "B")
    
    mul_p = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1000, -300)
    lib.connect_material_expressions(sub_center, "", mul_p, "A")
    lib.connect_material_expressions(append_aspect, "", mul_p, "B")
    
    # abs(P)
    abs_p = lib.create_material_expression(mat, unreal.MaterialExpressionAbs, -850, -300)
    lib.connect_material_expressions(mul_p, "", abs_p, "")
    
    # Append HalfWidth, HalfHeight
    append_size = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -1200, 50)
    lib.connect_material_expressions(p_width, "", append_size, "A")
    lib.connect_material_expressions(p_height, "", append_size, "B")
    
    # abs(P) - Size
    sub_size = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, -700, -250)
    lib.connect_material_expressions(abs_p, "", sub_size, "A")
    lib.connect_material_expressions(append_size, "", sub_size, "B")
    
    # + Radius
    add_radius = lib.create_material_expression(mat, unreal.MaterialExpressionAdd, -550, -250)
    lib.connect_material_expressions(sub_size, "", add_radius, "A")
    lib.connect_material_expressions(p_radius, "", add_radius, "B")
    
    # max(d, 0.0)
    const_zero = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -550, -100)
    const_zero.set_editor_property("r", 0.0)
    max_zero = lib.create_material_expression(mat, unreal.MaterialExpressionMax, -400, -250)
    lib.connect_material_expressions(add_radius, "", max_zero, "A")
    lib.connect_material_expressions(const_zero, "", max_zero, "B")
    
    # length(max(d, 0)) -> Dot product with self -> Sqrt
    dot_d = lib.create_material_expression(mat, unreal.MaterialExpressionDotProduct, -250, -250)
    lib.connect_material_expressions(max_zero, "", dot_d, "A")
    lib.connect_material_expressions(max_zero, "", dot_d, "B")
    
    sqrt_d = lib.create_material_expression(mat, unreal.MaterialExpressionSquareRoot, -100, -250)
    lib.connect_material_expressions(dot_d, "", sqrt_d, "")
    
    # length - Radius = Distance
    dist = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, 50, -250)
    lib.connect_material_expressions(sqrt_d, "", dist, "A")
    lib.connect_material_expressions(p_radius, "", dist, "B")
    
    # --- 마스크 연산 ---
    # OuterMask = SmoothStep(0.0, EdgeSoftness, Dist)
    outer_mask = lib.create_material_expression(mat, unreal.MaterialExpressionSmoothStep, 250, -150)
    lib.connect_material_expressions(const_zero, "", outer_mask, "Min")
    lib.connect_material_expressions(p_softness, "", outer_mask, "Max")
    lib.connect_material_expressions(dist, "", outer_mask, "Value")
    
    # --- 컬러 합성 (No-Rim: 중앙은 완전 투명, 외곽만 푸른 틴트 방출) ---
    mul_tint = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, 500, -100)
    lib.connect_material_expressions(p_tint_col, "", mul_tint, "A")
    lib.connect_material_expressions(outer_mask, "", mul_tint, "B")
    
    # --- Opacity 합성 (No-Rim: 외곽 마스크 * 투명도) ---
    mul_tint_opac = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, 500, 150)
    lib.connect_material_expressions(outer_mask, "", mul_tint_opac, "A")
    lib.connect_material_expressions(p_tint_opac, "", mul_tint_opac, "B")
    
    # 최종 머티리얼 프로퍼티 연결
    lib.connect_material_property(mul_tint, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(mul_tint_opac, "", unreal.MaterialProperty.MP_OPACITY)
    
    lib.recompile_material(mat)
    unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_package()], False)
    unreal.log(f"Successfully created {mat_name}")
    return mat

def create_goggle_blur_mask():
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.MaterialFactoryNew()
    
    mat_name = "M_GoggleBlurMask"
    mat = asset_tools.create_asset(mat_name, DEST_PATH, unreal.Material, factory)
    if not mat:
        unreal.log_error(f"Failed to create material: {mat_name}")
        return None
        
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    
    lib = unreal.MaterialEditingLibrary
    
    # Parameters
    p_aspect = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, -200)
    p_aspect.set_editor_property("parameter_name", "AspectRatio")
    p_aspect.set_editor_property("default_value", 1.7778)
    
    p_width = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, 0)
    p_width.set_editor_property("parameter_name", "HalfWidth")
    p_width.set_editor_property("default_value", 0.60)
    
    p_height = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, 150)
    p_height.set_editor_property("parameter_name", "HalfHeight")
    p_height.set_editor_property("default_value", 0.36)
    
    p_radius = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1400, 300)
    p_radius.set_editor_property("parameter_name", "CornerRadius")
    p_radius.set_editor_property("default_value", 0.18)
    
    p_softness = lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -800, 450)
    p_softness.set_editor_property("parameter_name", "EdgeSoftness")
    p_softness.set_editor_property("default_value", 0.05)
    
    # UV Center
    texcoord = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1800, -350)
    center_const = lib.create_material_expression(mat, unreal.MaterialExpressionConstant2Vector, -1800, -200)
    center_const.set_editor_property("r", 0.5)
    center_const.set_editor_property("g", 0.5)
    
    sub_center = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, -1600, -300)
    lib.connect_material_expressions(texcoord, "", sub_center, "A")
    lib.connect_material_expressions(center_const, "", sub_center, "B")
    
    append_aspect = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -1200, -250)
    const_one = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -1400, -100)
    const_one.set_editor_property("r", 1.0)
    lib.connect_material_expressions(p_aspect, "", append_aspect, "A")
    lib.connect_material_expressions(const_one, "", append_aspect, "B")
    
    mul_p = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1000, -300)
    lib.connect_material_expressions(sub_center, "", mul_p, "A")
    lib.connect_material_expressions(append_aspect, "", mul_p, "B")
    
    abs_p = lib.create_material_expression(mat, unreal.MaterialExpressionAbs, -850, -300)
    lib.connect_material_expressions(mul_p, "", abs_p, "")
    
    append_size = lib.create_material_expression(mat, unreal.MaterialExpressionAppendVector, -1200, 50)
    lib.connect_material_expressions(p_width, "", append_size, "A")
    lib.connect_material_expressions(p_height, "", append_size, "B")
    
    sub_size = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, -700, -250)
    lib.connect_material_expressions(abs_p, "", sub_size, "A")
    lib.connect_material_expressions(append_size, "", sub_size, "B")
    
    add_radius = lib.create_material_expression(mat, unreal.MaterialExpressionAdd, -550, -250)
    lib.connect_material_expressions(sub_size, "", add_radius, "A")
    lib.connect_material_expressions(p_radius, "", add_radius, "B")
    
    const_zero = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -550, -100)
    const_zero.set_editor_property("r", 0.0)
    max_zero = lib.create_material_expression(mat, unreal.MaterialExpressionMax, -400, -250)
    lib.connect_material_expressions(add_radius, "", max_zero, "A")
    lib.connect_material_expressions(const_zero, "", max_zero, "B")
    
    dot_d = lib.create_material_expression(mat, unreal.MaterialExpressionDotProduct, -250, -250)
    lib.connect_material_expressions(max_zero, "", dot_d, "A")
    lib.connect_material_expressions(max_zero, "", dot_d, "B")
    
    sqrt_d = lib.create_material_expression(mat, unreal.MaterialExpressionSquareRoot, -100, -250)
    lib.connect_material_expressions(dot_d, "", sqrt_d, "")
    
    dist = lib.create_material_expression(mat, unreal.MaterialExpressionSubtract, 50, -250)
    lib.connect_material_expressions(sqrt_d, "", dist, "A")
    lib.connect_material_expressions(p_radius, "", dist, "B")
    
    # OuterMask = SmoothStep(0.0, EdgeSoftness, Dist)
    outer_mask = lib.create_material_expression(mat, unreal.MaterialExpressionSmoothStep, 250, -150)
    lib.connect_material_expressions(const_zero, "", outer_mask, "Min")
    lib.connect_material_expressions(p_softness, "", outer_mask, "Max")
    lib.connect_material_expressions(dist, "", outer_mask, "Value")
    
    # Retainer Box Texture Sample
    tex_sample = lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, 250, -350)
    tex_sample.set_editor_property("parameter_name", "Texture")
    
    # Emissive = Retainer Texture RGB
    lib.connect_material_property(tex_sample, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    
    # Opacity = Retainer Texture Alpha * OuterMask (so only outer area blurs)
    mul_opac = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, 500, -250)
    lib.connect_material_expressions(tex_sample, "A", mul_opac, "A")
    lib.connect_material_expressions(outer_mask, "", mul_opac, "B")
    
    lib.connect_material_property(mul_opac, "", unreal.MaterialProperty.MP_OPACITY)
    
    lib.recompile_material(mat)
    unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_package()], False)
    unreal.log(f"Successfully created {mat_name}")
    return mat

def run():
    unreal.log("=== Creating Goggle Visor Materials ===")
    create_goggle_lens_material()
    create_goggle_blur_mask()
    unreal.log("=== Goggle Visor Materials Created Successfully ===")

if __name__ == "__main__":
    run()
