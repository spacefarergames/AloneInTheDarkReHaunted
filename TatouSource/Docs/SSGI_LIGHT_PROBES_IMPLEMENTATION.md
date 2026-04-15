# SSGI + Light Probes Implementation Guide

## Overview
Successfully implemented Screen-Space Global Illumination (SSGI) and Light Probe system to add real-time global illumination effects to Alone In The Dark Re-Haunted.

## Features Implemented

### 1. Screen-Space Global Illumination (SSGI)
- **Purpose**: Approximates indirect lighting by sampling nearby surfaces in screen space
- **Performance**: Half-resolution rendering for optimal performance
- **Quality**: 16-sample disc pattern with bilateral blur for clean results

### 2. Light Probes
- **Purpose**: Provides ambient lighting using Spherical Harmonics at strategic world positions
- **Storage**: 3 bands (9 coefficients) per RGB channel
- **Loading**: Per-floor and per-camera probe data files
- **Blending**: Inverse distance weighted blending of nearest probes

## Files Modified/Created

### Configuration
- `FitdLib/configRemaster.h` - Added SSGI and Light Probe settings
- `FitdLib/configRemaster.cpp` - Added load/save support for new settings

### Post-Processing System
- `FitdLib/postProcessing.h` - Extended with SSGI members and methods
- `FitdLib/postProcessing.cpp` - Implemented renderSSGI() pipeline

### Shaders
- `FitdLib/shaders/ssgi_ps.sc` - SSGI computation shader
- `FitdLib/shaders/ssgi_blur_ps.sc` - Bilateral blur shader
- `FitdLib/shaders/composite_ps.sc` - Modified to blend SSGI contribution

### Light Probe System
- `FitdLib/lightProbes.h` - Light probe system header
- `FitdLib/lightProbes.cpp` - Complete implementation

### Integration
- `FitdLib/bgfxGlue.cpp` - Added configuration synchronization and initialization

## Configuration Options

Add these to your remaster configuration:

```cpp
// SSGI Settings
postProcessing.enableSSGI = true;          // Enable SSGI
postProcessing.ssgiRadius = 300.0f;        // Sample radius (pixels)
postProcessing.ssgiIntensity = 0.6f;       // Effect intensity
postProcessing.ssgiNumSamples = 16;        // Number of samples (8-32)

// Light Probe Settings
postProcessing.enableLightProbes = true;   // Enable light probes
postProcessing.lightProbeIntensity = 0.5f; // Probe contribution intensity
```

## Rendering Pipeline

### SSGI Pass (View 206)
1. Sample depth buffer to find nearby surfaces
2. Use 16-sample disc pattern for screen-space ray marching
3. Gather colors from nearby surfaces weighted by depth
4. Output to half-resolution framebuffer

### SSGI Blur Pass (View 207)
1. Apply 9-tap bilateral blur
2. Preserve edges using depth-based weighting
3. Output to blurred half-resolution framebuffer

### Composite Pass (View 210)
1. Sample main color, bloom, SSAO, and SSGI textures
2. Blend SSGI additively: `color += ssgi * intensity`
3. Apply film grain and final tone mapping
4. Output to screen

## Light Probe Data Files

Probes are stored in: `GAMEDATA/PROBES/FLOOR_XX_CAM_YY.dat`

### File Format
```cpp
struct ProbeFileHeader {
    int numProbes;
};

struct ProbeData {
    float x, y, z;              // World position
    float sh_r[9];              // Red channel SH coefficients
    float sh_g[9];              // Green channel SH coefficients
    float sh_b[9];              // Blue channel SH coefficients
    int floor;                  // Floor index
    int camera;                 // Camera index (-1 = global)
};
```

## Runtime Loading

Light probes are automatically loaded when:
- Floor changes (via `NewNumEtage`)
- Camera changes (via camera ID)

To manually load probes:
```cpp
g_lightProbeManager->loadProbes(floorIndex, cameraIndex);
```

## Sampling Light Probes

To get ambient color at a world position:
```cpp
float r, g, b;
g_lightProbeManager->sampleProbes(worldX, worldY, worldZ, r, g, b);
```

The manager automatically:
- Finds nearest probes (up to 4)
- Weights by inverse distance
- Evaluates Spherical Harmonics
- Blends results

## Performance Considerations

1. **SSGI Resolution**: Half-resolution (width/2 × height/2) for performance
2. **Sample Count**: 16 samples provides good quality/performance balance
3. **Bilateral Blur**: Essential for clean results without destroying detail
4. **Light Probes**: Minimal overhead - SH evaluation is very fast

## BGFX View IDs Used

- View 198: SSAO
- View 199: SSAO Blur  
- View 200: Bloom Bright Pass
- View 201-204: Bloom Blur Passes
- View 206: SSGI
- View 207: SSGI Blur
- View 210: Final Composite

## Next Steps

### 1. Compile Shaders
Run bgfx shaderc to compile the new shaders:
```bash
shaderc -f ssgi_ps.sc -o ssgi_ps.bin --type fragment --platform windows -p ps_5_0 -O 3
shaderc -f ssgi_blur_ps.sc -o ssgi_blur_ps.bin --type fragment --platform windows -p ps_5_0 -O 3
```

### 2. Create Light Probe Data
- Use the game's camera positions to place probes
- Generate SH coefficients from the scene lighting
- Save to GAMEDATA/PROBES/ directory

### 3. Enable in Configuration
Update `GAMEDATA/config.txt`:
```
SSGI_ENABLED=1
SSGI_RADIUS=300
SSGI_INTENSITY=0.6
LIGHT_PROBES_ENABLED=1
LIGHT_PROBE_INTENSITY=0.5
```

### 4. Build and Test
1. Build the project
2. Run the game
3. Enable SSGI and Light Probes in settings menu
4. Verify visual quality and performance

## Troubleshooting

### SSGI not visible
- Check that enableSSGI is true in config
- Verify shaders compiled successfully
- Check intensity > 0

### Light Probes not working
- Verify probe data files exist
- Check file format matches structure
- Ensure probes loaded for current floor/camera

### Performance issues
- Reduce ssgiNumSamples (try 8 or 12)
- Reduce ssgiRadius
- Check GPU profiler for bottlenecks

## Technical Details

### Spherical Harmonics (SH)
Light probes use SH to compactly represent directional ambient lighting:
- Band 0: DC term (constant)
- Band 1: Linear terms (3 coefficients)
- Band 2: Quadratic terms (5 coefficients)
- Total: 9 coefficients per RGB channel

### Screen-Space Ray Marching
SSGI uses a disc-based sampling pattern:
```cpp
// Generate sample offsets in a disc pattern
for (int i = 0; i < numSamples; i++) {
    float angle = (i / numSamples) * 2.0 * PI;
    float radius = sqrt(i / numSamples) * ssgiRadius;
    vec2 offset = vec2(cos(angle), sin(angle)) * radius;
    // Sample at this offset...
}
```

## Credits
- Implementation: AI Assistant
- Based on: SSAO implementation from postProcessing.cpp
- Spherical Harmonics: Standard SH basis functions
- BGFX integration: Existing post-processing framework

## License
GPL - Same as Alone In The Dark Re-Haunted
