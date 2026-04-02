# Flat Polygon Atlas System for Mixed Models

## Overview

The flat polygon atlas system creates a separate texture atlas specifically for flat-shaded polygons that appear on models containing both textured and flat-shaded primitives (e.g., doors with handles/panels).

## How It Works

### Three Atlas Types

1. **Main Projection Atlas** (`body_<hqr>_<num>.png`)
   - For pure flat-shaded models (characters, props)
   - Contains all polygon primitives projected orthographically
   - Front-facing polygons on left half, back-facing on right half

2. **Sphere Billboard Atlas** (`spheres_<hqr>_<num>.png`)
   - For sphere/point primitives (particles, highlights)
   - 32x32 cells arranged in an 8-column grid
   - Circular gradient billboards with alpha falloff

3. **Flat Polygon Atlas** (`flat_<hqr>_<num>.png`) **[NEW]**
   - For flat-shaded polygons on mixed models
   - Only contains `primTypeEnum_Poly` primitives
   - Excludes pre-textured primitives (`processPrim_PolyTexture9/10`)
   - Same projection system as main atlas

### Detection

The system automatically detects mixed models during atlas loading:
- Scans all primitives to find both textured and flat-shaded types
- If both are present, dumps a flat poly atlas
- If only flat-shaded primitives exist, uses main atlas
- If only textured primitives exist, no atlas needed

### File Locations

All atlases are saved to: `<homePath>/atlases/`

Examples:
```
atlases/flat_LISTBODY_042.png    (door with handles)
atlases/flat_OBJETS_015.png      (textured object with flat elements)
atlases/body_LISTBODY_001.png    (pure flat character)
atlases/spheres_LISTBODY_001.png (character's sphere effects)
```

### Rendering Priority

For flat-shaded polygons (`primTypeEnum_Poly`):
1. Try flat poly atlas first (for mixed models)
2. Fall back to main atlas (for pure flat models)
3. Fall back to palette rendering (if no atlases)

## Artist Workflow

### Texturing Door Handles/Panels

1. **Auto-Generate Base Atlas**
   - Run the game once to auto-dump `flat_<hqr>_<num>.png`
   - Flat polygons are rasterized with their palette colors
   - UVs are computed and stored in memory

2. **Paint Over Atlas**
   - Open `flat_<hqr>_<num>.png` in your image editor
   - Paint wooden textures over handle areas
   - Add grain, highlights, shadows, etc.
   - Save the PNG (keep same dimensions)

3. **See Results In-Game**
   - Atlas is automatically loaded on next run
   - Flat polygons now use your painted textures
   - Main door texture remains intact
   - No code changes needed!

### Tips

- **Keep Atlas Dimensions**: Don't resize the PNG (must stay power-of-2)
- **Use Alpha Channel**: For transparency effects
- **Front/Back Split**: Left half = front-facing, right half = back-facing
- **Delete to Regenerate**: Remove atlas file to dump fresh base

## Technical Details

### UV Computation

Flat poly UVs use the same projection as the main atlas:
- Rest-pose global vertices computed with bone hierarchy
- Orthographic projection onto XY plane
- Front-facing check via polygon normal (Z component)
- Normalized to [0..1] range per polygon
- Mirrored X for back-facing polygons

### Memory Layout

```cpp
struct ModelAtlasData {
    // Main atlas (pure flat models)
    bgfx::TextureHandle texture;
    std::vector<AtlasPolyUVs> polyUVs;
    
    // Sphere atlas (billboards)
    bgfx::TextureHandle sphereTexture;
    
    // Flat poly atlas (mixed models)
    bgfx::TextureHandle flatTexture;
    std::vector<AtlasPolyUVs> flatPolyUVs;  // Only for primTypeEnum_Poly
};
```

### Auto-Dump Triggers

Flat poly atlas is auto-dumped when:
- Model has BOTH textured AND flat primitives
- Atlas file doesn't exist yet
- Called from `loadModelAtlas()` during first render

## Examples

### Door (Mixed Model)

**Primitives:**
- `processPrim_PolyTexture9` - Wood panel textures (8 polys)
- `primTypeEnum_Poly` - Metal handles/trim (12 polys)

**Atlases Generated:**
- `flat_LISTBODY_042.png` - Contains only the 12 handle/trim polygons
- No main atlas (not needed for textured models)

**Result:**
- Wood panels use pre-baked door texture
- Handles use artist-painted atlas texture
- No flat polygons drawn over door texture

### Character (Pure Flat Model)

**Primitives:**
- `primTypeEnum_Poly` - All geometry (200+ polys)

**Atlases Generated:**
- `body_LISTBODY_001.png` - Contains all 200+ polygons

**Result:**
- All geometry uses main projection atlas
- Clean UV unwrap for painting

## Future Enhancements

- **Per-Group Atlases**: Separate atlas per bone for better resolution
- **LOD Variants**: Multiple atlas resolutions for distance
- **Normal Maps**: Add normal+specular channels to flat poly atlas
- **Auto-Material**: Detect handle materials and apply metal shaders
