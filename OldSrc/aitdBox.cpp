///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// UI dialog box and frame rendering
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "hdBackground.h"

// HD frame borders disabled for now as it breaks HD background loading
// TODO: Re-enable and implement these functions when HD frame border loading is fixed
// extern void drawHDFrameBorder(FrameBorderType borderType, float x, float y, float width, float height);
// extern bool areHDFrameBorderTexturesReady();
// extern bool getHDFrameBorderSize(FrameBorderType borderType, int* outWidth, int* outHeight);

void AffSpf(int left, int top, int index, char* gfxData)
{
    char* outPtr;
    char* inPtr;

    int width;
    int height;

    int offset;

    int i;
    int j;

    if(g_gameId >= AITD3)
        return;

    inPtr = gfxData + READ_LE_U16(index * 2 + gfxData);

    inPtr +=4;

    width = READ_LE_U16(inPtr);
    inPtr+=2;
    height = READ_LE_U16(inPtr);
    inPtr+=2;

    // Bounds check: ensure sprite doesn't draw beyond screen boundaries
    if (left < 0 || top < 0 || left >= 320 || top >= 200)
        return;

    // Store original width for input stride
    int originalWidth = width;

    // Clamp width/height to stay within screen bounds
    if (left + width > 320)
        width = 320 - left;
    if (top + height > 200)
        height = 200 - top;

    if (width <= 0 || height <= 0)
        return;

    outPtr = logicalScreen + top*320 + left;
    offset = 320 - width;

    for(i=0;i<height;i++)
    {
        for(j=0;j<width;j++)
        {
            *(outPtr++) = *(inPtr++);
        }

        // Skip remaining source pixels if width was clamped
        inPtr += (originalWidth - width);
        outPtr+=offset;
    }
}

void AffSpfI(int left, int top, int index, char* gfxData)
{
	char* outPtr;
	char* inPtr;

	int width;
	int height;

	int offset;

	int i;
	int j;

	if(g_gameId >= AITD3)
		return;

	outPtr = logicalScreen + top*320 + left;
	inPtr = gfxData + READ_LE_U16(index * 2 + gfxData); // alignement unsafe

	inPtr +=4;

	width = READ_LE_U16(inPtr); // alignement unsafe
	inPtr+=2;
	height = READ_LE_U16(inPtr); // alignement unsafe
	inPtr+=2;

	// Bounds check: ensure sprite doesn't draw beyond screen boundaries
	if (left < 0 || top < 0 || left >= 320 || top >= 200)
		return;
	
	// Clamp width/height to stay within screen bounds
	int originalWidth = width;
	if (left + width > 320)
		width = 320 - left;
	if (top + height > 200)
		height = 200 - top;

	if (width <= 0 || height <= 0)
		return;

	offset = 320 - width;

	for(i=0;i<height;i++)
	{
		for(j=0;j<width;j++)
		{
			char color = *(inPtr++);
			if(color)
			{
				*(outPtr) = color;
			}
			outPtr++;
		}

		// Skip remaining source pixels if width was clamped
		inPtr += (originalWidth - width);
		outPtr+=offset;
	}
}

// HD frame borders disabled for now as it breaks HD background loading
// TODO: Re-enable when HD frame border loading is fixed
#if 0
// Draw HD frame borders if available
// Returns true if HD borders were drawn
static bool drawHDFrameBorders(int left, int top, int right, int bottom)
{
    if (!areHDFrameBorderTexturesReady())
    {
        return false;
    }

    int borderWidth, borderHeight;

    // Draw top border (stretched horizontally)
    if (getHDFrameBorderSize(FRAME_BORDER_TOP, &borderWidth, &borderHeight))
    {
        float frameWidth = (float)(right - left);
        drawHDFrameBorder(FRAME_BORDER_TOP, (float)left, (float)top, frameWidth, (float)borderHeight);
    }

    // Draw bottom border (stretched horizontally)
    if (getHDFrameBorderSize(FRAME_BORDER_BOTTOM, &borderWidth, &borderHeight))
    {
        float frameWidth = (float)(right - left);
        drawHDFrameBorder(FRAME_BORDER_BOTTOM, (float)left, (float)(bottom - borderHeight), frameWidth, (float)borderHeight);
    }

    // Draw left border (stretched vertically)
    if (getHDFrameBorderSize(FRAME_BORDER_LEFT, &borderWidth, &borderHeight))
    {
        float frameHeight = (float)(bottom - top);
        drawHDFrameBorder(FRAME_BORDER_LEFT, (float)left, (float)top, (float)borderWidth, frameHeight);
    }

    // Draw right border (stretched vertically)
    if (getHDFrameBorderSize(FRAME_BORDER_RIGHT, &borderWidth, &borderHeight))
    {
        float frameHeight = (float)(bottom - top);
        drawHDFrameBorder(FRAME_BORDER_RIGHT, (float)(right - borderWidth), (float)top, (float)borderWidth, frameHeight);
    }

    return true;
}
#endif

void AffBigCadre(int x, int y, int width, int height)
{
    int top;
    int right;
    int left;
    int bottom;

    int currentLeftPosition;
    int currentTopPosition;

    int halfWidth;
    int halfHeight;

    SetClip(0,0,319,199);

    halfWidth = width/2;
    currentLeftPosition = left = x - halfWidth;

    halfHeight = height/2;
    currentTopPosition = top = y - halfHeight;

    right = x + halfWidth;
    bottom = y + halfHeight;

    // HD frame borders disabled for now as it breaks HD background loading
    // TODO: Re-enable when HD frame border loading is fixed
    // bool useHDBorders = drawHDFrameBorders(left, top, right, bottom);
    bool useHDBorders = false;

    // Only draw original borders if HD borders are not available
    if (!useHDBorders)
    {
        AffSpf(currentLeftPosition,currentTopPosition,0,PtrCadre); // draw top left corner

        while(1) // draw top bar
        {
            currentLeftPosition += 20;

            if(right - 20 <= currentLeftPosition)
                break;

            AffSpf(currentLeftPosition,currentTopPosition,4,PtrCadre);
        }

    AffSpf(currentLeftPosition,currentTopPosition,1,PtrCadre); // draw top right corner

    currentLeftPosition = left;

    while(1) // draw left bar
    {
        currentTopPosition += 20;

        if(bottom - 20 <= currentTopPosition)
            break;

        AffSpf(currentLeftPosition,currentTopPosition,6,PtrCadre);
    }

    currentLeftPosition = right - 8;
    currentTopPosition = top + 20;

    while(bottom - 20 > currentTopPosition)
    {
        AffSpf(currentLeftPosition,currentTopPosition,7,PtrCadre);

        currentTopPosition += 20;
    }

    currentLeftPosition = left;

    AffSpf(currentLeftPosition,currentTopPosition,2,PtrCadre); // draw bottom left corner

    while(1) // draw bottom bar
    {
        currentLeftPosition += 20;

        if(right-20 <= currentLeftPosition)
            break;

        AffSpf(currentLeftPosition,currentTopPosition+12,5,PtrCadre);
    }

    AffSpf(currentLeftPosition,currentTopPosition,3,PtrCadre); // draw bottom right corner

        AffSpf(x-20,currentTopPosition+12,8,PtrCadre); // draw "in the dark"
    }

    WindowX1 = left + 8;
    WindowY1 = top + 8;
    WindowX2 = right - 9;
    WindowY2 = bottom - 9;

    AffRect(WindowX1,WindowY1,WindowX2,WindowY2,0);
    SetClip(WindowX1,WindowY1,WindowX2,WindowY2);

}
