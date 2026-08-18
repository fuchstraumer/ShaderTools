module ddMaterials;

// fix: this file held one line and no include. ddMaterialsTypes.slang declares `implementing
// ddMaterials;`, so SurfaceOutput, LightingResult and LightingInput were unreachable from every
// consumer, and the whole module gave nothing.
__include ddMaterialsTypes;
