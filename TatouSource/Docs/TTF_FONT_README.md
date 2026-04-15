# TTF Font Rendering Feature

This feature adds smooth TTF font rendering overlay on top of the original game's bitmap fonts.

## Configuration

Add these settings to your `fitd_remaster.cfg` file:

```ini
# TTF Font Settings
font.enableTTF = true
font.path = "BLKCHCRY.TTF"
font.size = 16
font.hideOriginal = true
```

### Settings Explanation:

- **font.enableTTF**: Enable/disable TTF font rendering (default: false)
- **font.path**: Path to the TTF font file (default: "BLKCHCRY.TTF")
  - Place your TTF font file in the game directory
  - Recommended: BLKCHCRY.TTF (BlackChancery) or similar gothic/horror fonts
- **font.size**: Font size in pixels (default: 16)
  - Adjust to match the original font height
  - Typical values: 14-18
- **font.hideOriginal**: Hide the original bitmap font rendering (default: true)
  - Set to false to see both fonts (useful for testing alignment)

## Font Recommendations

For best results matching the gothic horror aesthetic of Alone in the Dark:

1. **BLKCHCRY.TTF** (Black Chancery) - Gothic, elegant
2. **MORPHEUS.TTF** - Classic horror game font
3. **NOSIFER.TTF** - Dripping horror style
4. Any other TTF font that matches the game's atmosphere

## Technical Details

- The TTF rendering matches the exact placement of the original font system
- Text is rendered using ImGui's font system with high-quality anti-aliasing
- Supports shadows for selected messages (menu items, etc.)
- Uses the game's original palette colors for consistency

## Troubleshooting

**"Failed to load TTF font"**: 
- Ensure the font file exists in the game directory
- Check the path in the config file
- Try using an absolute path if relative path doesn't work

**Font appears too large/small**:
- Adjust the `font.size` setting
- Compare with original by setting `font.hideOriginal = false`

**Font doesn't match game style**:
- Try a different TTF font
- Adjust font size for better proportions

## Building

After adding the TTF font files, regenerate the Visual Studio solution:

```bash
cd build/vs2026
cmake ..
```

Or in Visual Studio, right-click the ZERO_CHECK project and select "Build".
