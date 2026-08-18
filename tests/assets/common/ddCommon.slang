module ddCommon;

// fix: ddTonemapping.slang declares `implementing ddCommon;` and this file did not include it. A file
// that names a module it never joins is unreachable, so TonemapParams and ApplyTonemapping did not
// exist for any consumer, and the defect in Reinhard could never be found by a compile.
__include ddCommonFunctions;
__include ddCommonTypes;
__include ddTonemapping;
__include ddFullscreenQuad;
