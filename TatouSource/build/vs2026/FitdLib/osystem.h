///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// OS abstraction layer interface
///////////////////////////////////////////////////////////////////////////////

#ifndef _OSYSTEM_H_
#define _OSYSTEM_H_

#ifdef __cplusplus
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#ifdef FITD_DEBUGGER
#include "debugFont.h"
#endif

#include "palette.h"

#endif

#define byte char
//#ifndef bool
//#define bool char
//#endif
#define u8 unsigned char
#define true 1
#define false 0

    enum e_rendererType
    {
        OPENGL_OLD,
        OPENGL_ES,
        OPENGL_3_2,
    };

    extern enum e_rendererType g_rendererType;

	void osystem_init();

	extern int osystem_mouseRight;
	extern int osystem_mouseLeft;

	void osystem_drawBackground();
	u32 osystem_startOfFrame();
	void osystem_endOfFrame();

	void osystem_initGL(int screenWidth, int screenHeight);

	void osystem_delay(int time);
	void osystem_fadeBlackToWhite();
	void osystem_updateImage();
	void osystem_initBuffer();
	void osystem_initVideoBuffer(char *buffer, int width, int height);
	void osystem_putpixel(int x, int y, int pixel);
	void osystem_setColor(byte i, byte R, byte G, byte B);
	void osystem_setPalette(unsigned byte * palette);
	void osystem_setPalette(palette_t* palette);
	void osystem_setPalette320x200(byte * palette);
	void osystem_setDarkRoom(bool dark);
	void osystem_flip(unsigned char *videoBuffer);
	void osystem_draw320x200BufferToScreen(unsigned char *videoBuffer);
	void osystem_CopyBlockPhys(unsigned char* videoBuffer, int left, int top, int right, int bottom);
	void osystem_refreshFrontTextureBuffer();
	void osystem_drawText(int X, int Y, char *text);
	void osystem_drawTextColor(int X, int Y, char *string, unsigned char R, unsigned char G, unsigned char B);
	void osystem_drawLine(int X1,int X2,int Y1,int Y2,unsigned char color, unsigned char* palette);
	void osystem_playSampleFromName(char* sampleName);
	void osystem_playSample(char* samplePtr,int size);
	void osystem_playLoopingSample(char* samplePtr, int size);
	void osystem_stopSample();
	bool osystem_isSamplePlaying();

	void osystem_playVO(const char* voFileName);
	void osystem_playVocByIndex(int vocIndex);
	void osystem_playVocPageLines(int vocIndex, int page, int numLines);
	void osystem_stopVO();
	bool osystem_isVOPlaying();

	// VOC pre-decode cache for seamless page turns during book reading
	void osystem_preDecodeVocPage(int vocIndex, int page);
	unsigned char* osystem_tryGetPreDecodedVocPage(int vocIndex, int page, size_t* outSize);
	void osystem_clearVocCache();

	extern float g_voPitchMultiplier;
	//    void getMouseStatus(mouseStatusStruct * mouseData);

	void osystem_createMask(const std::array<u8, 320 * 200>& mask, int roomId, int maskId, int maskX1, int maskY1, int maskX2, int maskY2);
	void osystem_drawMask(int roomId, int maskId);

	void osystem_startFrame();
	void osystem_stopFrame();

	// clip
	void osystem_setClip(float left, float top, float right, float bottom);
	void osystem_clearClip();

	void osystem_cleanScreenKeepZBuffer();

	void osystem_fillPoly(float* buffer, int numPoint, unsigned char color,u8 polyType);
	void osystem_draw3dLine(float x1, float y1, float z1, float x2, float y2, float z2, unsigned char color);
	void osystem_draw3dQuad(float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4, unsigned char color, int transparency);
	void osystem_drawSphere(float X, float Y, float Z, u8 color, u8 material, float size);
	void osystem_drawPoint(float X, float Y, float Z, u8 color, u8 material, float size);
	void osystem_flushPendingPrimitives();
	void osystem_drawUILayer();

	void osystem_drawPortraitOverlay(int choice);

	void osystem_drawInventoryBackground();

	void osystem_drawFoundObjectBackground();

	void osystem_drawControllerHint();

	void osystem_drawVignette();

	void osystem_drawSystemMenuBackground();

	void osystem_drawSaveRestoreBackground();
	void osystem_updateSaveSlotPreviewTexture(unsigned char* rgbaData, int width, int height);
	void osystem_drawSaveSlotPreviewHD(float x1, float y1, float x2, float y2);
	void osystem_destroySaveSlotPreviewTexture();

	void osystem_drawHotspotOverlay(float opacity = 1.0f);

	void osystem_drawStartupMenuBackground();
	void osystem_drawLanguageSelectionBackground();
	void osystem_drawFullScreenFrame();

	// Map screen overlay
	void osystem_loadMapTexture(int floorNumber);
	void osystem_drawMapImage();
	void osystem_destroyMapTexture();

	void osystem_drawBlackScreen();

	void osystem_startBgPoly();
	void osystem_endBgPoly();
	void osystem_addBgPolyPoint(int x, int y);

	int osystem_playTrack(int trackId);
	void osystem_stopTrack();
	void osystem_playAdlib();

	// Scene preview capture for pause menu
	void osystem_updateSceneSnapshot();
	void osystem_captureScenePreview();
	void osystem_finalizeScenePreview();
	void osystem_releaseScenePreview();
	unsigned char* osystem_getScenePreviewData();
	int osystem_getScenePreviewWidth();
	int osystem_getScenePreviewHeight();

	// Freeze/unfreeze scene snapshot for menu backgrounds
	void osystem_freezeSceneForMenu();
	void osystem_unfreezeSceneForMenu();
	void osystem_drawFrozenSceneBackground();
	void osystem_drawFrozenScenePreview(float x1, float y1, float x2, float y2);

	// Page turn animation
	void osystem_pageTurnCapture();
	void osystem_pageTurnFrame(float progress, bool forward, unsigned char* newPage);

#endif
