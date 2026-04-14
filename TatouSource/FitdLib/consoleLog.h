///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Shared console color codes and per-subsystem log tag definitions
///////////////////////////////////////////////////////////////////////////////

#pragma once

// ── ANSI escape codes ──────────────────────────────────────────────────────
#define CON_RESET   "\033[0m"
#define CON_BOLD    "\033[1m"
#define CON_DIM     "\033[2m"
#define CON_RED     "\033[31m"
#define CON_GREEN   "\033[32m"
#define CON_YELLOW  "\033[33m"
#define CON_CYAN    "\033[36m"
#define CON_MAGENTA "\033[35m"
#define CON_WHITE   "\033[97m"
#define CON_BRED    "\033[1;31m"
#define CON_BGREEN  "\033[1;32m"
#define CON_BYELLOW "\033[1;33m"
#define CON_BCYAN   "\033[1;36m"

// ── Tag macro generator ────────────────────────────────────────────────────
// TAG  = bold cyan   "[abbr] " reset   — general info
// OK   = bold green  "[abbr] " reset   — success
// WARN = bold yellow "[abbr] " yellow  — warning  (message text stays yellow; append CON_RESET before \n)
// ERR  = bold red    "[abbr] " red     — error    (message text stays red;    append CON_RESET before \n)
#define CON_MAKE_TAG(abbr)   CON_BCYAN   "[" abbr "] " CON_RESET
#define CON_MAKE_OK(abbr)    CON_BGREEN  "[" abbr "] " CON_RESET
#define CON_MAKE_WARN(abbr)  CON_BYELLOW "[" abbr "] " CON_YELLOW
#define CON_MAKE_ERR(abbr)   CON_BRED    "[" abbr "] " CON_RED

// ── Per-subsystem tags ─────────────────────────────────────────────────────

// TrueType font system (fontTTF.cpp)
#define TTF_TAG   CON_MAKE_TAG("TTF")
#define TTF_OK    CON_MAKE_OK("TTF")
#define TTF_WARN  CON_MAKE_WARN("TTF")
#define TTF_ERR   CON_MAKE_ERR("TTF")

// Remaster config (configRemaster.cpp)
#define CFG_TAG   CON_MAKE_TAG("CFG")
#define CFG_OK    CON_MAKE_OK("CFG")
#define CFG_WARN  CON_MAKE_WARN("CFG")
#define CFG_ERR   CON_MAKE_ERR("CFG")

// HD background loading (hdBackground.cpp)
#define HDBG_TAG  CON_MAKE_TAG("HDBG")
#define HDBG_OK   CON_MAKE_OK("HDBG")
#define HDBG_WARN CON_MAKE_WARN("HDBG")
#define HDBG_ERR  CON_MAKE_ERR("HDBG")

// Life script interpreter (life.cpp)
#define LIFE_TAG  CON_MAKE_TAG("LIFE")
#define LIFE_OK   CON_MAKE_OK("LIFE")
#define LIFE_WARN CON_MAKE_WARN("LIFE")
#define LIFE_ERR  CON_MAKE_ERR("LIFE")

// Main engine (main.cpp)
#define MAIN_TAG  CON_MAKE_TAG("MAIN")
#define MAIN_OK   CON_MAKE_OK("MAIN")
#define MAIN_WARN CON_MAKE_WARN("MAIN")
#define MAIN_ERR  CON_MAKE_ERR("MAIN")

// Polygon renderer (polys.cpp)
#define POLY_TAG  CON_MAKE_TAG("POLY")
#define POLY_OK   CON_MAKE_OK("POLY")
#define POLY_WARN CON_MAKE_WARN("POLY")
#define POLY_ERR  CON_MAKE_ERR("POLY")

// BGFX graphics init (bgfxGlue.cpp)
#define BGFX_TAG  CON_MAKE_TAG("BGFX")
#define BGFX_OK   CON_MAKE_OK("BGFX")
#define BGFX_WARN CON_MAKE_WARN("BGFX")
#define BGFX_ERR  CON_MAKE_ERR("BGFX")

// PAK file system (pak.cpp)
#define PAK_TAG   CON_MAKE_TAG("PAK")
#define PAK_OK    CON_MAKE_OK("PAK")
#define PAK_WARN  CON_MAKE_WARN("PAK")
#define PAK_ERR   CON_MAKE_ERR("PAK")

// Variable evaluator (evalVar.cpp)
#define EVAL_TAG  CON_MAKE_TAG("EVAL")
#define EVAL_OK   CON_MAKE_OK("EVAL")
#define EVAL_WARN CON_MAKE_WARN("EVAL")
#define EVAL_ERR  CON_MAKE_ERR("EVAL")

// Audio system (osystemAL.cpp)
#define AL_TAG    CON_MAKE_TAG("AL")
#define AL_OK     CON_MAKE_OK("AL")
#define AL_WARN   CON_MAKE_WARN("AL")
#define AL_ERR    CON_MAKE_ERR("AL")

// OS / SDL layer (osystemSDL.cpp)
#define SDL_TAG   CON_MAKE_TAG("SDL")
#define SDL_OK    CON_MAKE_OK("SDL")
#define SDL_WARN  CON_MAKE_WARN("SDL")
#define SDL_ERR   CON_MAKE_ERR("SDL")

// Input / controller (input.cpp)
#define INP_TAG   CON_MAKE_TAG("INP")
#define INP_OK    CON_MAKE_OK("INP")
#define INP_WARN  CON_MAKE_WARN("INP")
#define INP_ERR   CON_MAKE_ERR("INP")

// 3D/primitive renderer (renderer.cpp)
#define RNDR_TAG  CON_MAKE_TAG("RNDR")
#define RNDR_OK   CON_MAKE_OK("RNDR")
#define RNDR_WARN CON_MAKE_WARN("RNDR")
#define RNDR_ERR  CON_MAKE_ERR("RNDR")

// Resource garbage collector (resourceGC.cpp)
#define GC_TAG    CON_MAKE_TAG("GC")
#define GC_OK     CON_MAKE_OK("GC")
#define GC_WARN   CON_MAKE_WARN("GC")
#define GC_ERR    CON_MAKE_ERR("GC")

// Main loop / stuck-player recovery (mainLoop.cpp)
#define MLOOP_TAG  CON_MAKE_TAG("MLOOP")
#define MLOOP_OK   CON_MAKE_OK("MLOOP")
#define MLOOP_WARN CON_MAKE_WARN("MLOOP")
#define MLOOP_ERR  CON_MAKE_ERR("MLOOP")

// BGFX renderer (rendererBGFX.cpp)
#define RBGFX_TAG  CON_MAKE_TAG("RBGFX")
#define RBGFX_OK   CON_MAKE_OK("RBGFX")
#define RBGFX_WARN CON_MAKE_WARN("RBGFX")
#define RBGFX_ERR  CON_MAKE_ERR("RBGFX")

// Track / path system (track.cpp)
#define TRACK_TAG  CON_MAKE_TAG("TRACK")
#define TRACK_OK   CON_MAKE_OK("TRACK")
#define TRACK_WARN CON_MAKE_WARN("TRACK")
#define TRACK_ERR  CON_MAKE_ERR("TRACK")

// Music system (music.cpp)
#define MUSIC_TAG  CON_MAKE_TAG("MUSIC")
#define MUSIC_OK   CON_MAKE_OK("MUSIC")
#define MUSIC_WARN CON_MAKE_WARN("MUSIC")
#define MUSIC_ERR  CON_MAKE_ERR("MUSIC")

// HQR archive (hqr.cpp)
#define HQR_TAG   CON_MAKE_TAG("HQR")
#define HQR_OK    CON_MAKE_OK("HQR")
#define HQR_WARN  CON_MAKE_WARN("HQR")
#define HQR_ERR   CON_MAKE_ERR("HQR")

// HD archive (hdArchive.cpp)
#define HDAR_TAG  CON_MAKE_TAG("HDAR")
#define HDAR_OK   CON_MAKE_OK("HDAR")
#define HDAR_WARN CON_MAKE_WARN("HDAR")
#define HDAR_ERR  CON_MAKE_ERR("HDAR")

// Inventory (inventory.cpp)
#define INV_TAG   CON_MAKE_TAG("INV")
#define INV_OK    CON_MAKE_OK("INV")
#define INV_WARN  CON_MAKE_WARN("INV")
#define INV_ERR   CON_MAKE_ERR("INV")

// Animation actions (animAction.cpp)
#define AACT_TAG  CON_MAKE_TAG("AACT")
#define AACT_OK   CON_MAKE_OK("AACT")
#define AACT_WARN CON_MAKE_WARN("AACT")
#define AACT_ERR  CON_MAKE_ERR("AACT")

// Object system (object.cpp)
#define OBJ_TAG   CON_MAKE_TAG("OBJ")
#define OBJ_OK    CON_MAKE_OK("OBJ")
#define OBJ_WARN  CON_MAKE_WARN("OBJ")
#define OBJ_ERR   CON_MAKE_ERR("OBJ")

// File access (fileAccess.cpp)
#define FACC_TAG  CON_MAKE_TAG("FILE")
#define FACC_OK   CON_MAKE_OK("FILE")
#define FACC_WARN CON_MAKE_WARN("FILE")
#define FACC_ERR  CON_MAKE_ERR("FILE")

// HD background renderer (hdBackgroundRenderer.cpp)
#define HDBGR_TAG  CON_MAKE_TAG("HDBGR")
#define HDBGR_OK   CON_MAKE_OK("HDBGR")
#define HDBGR_WARN CON_MAKE_WARN("HDBGR")
#define HDBGR_ERR  CON_MAKE_ERR("HDBGR")

// FM OPL emulator (fmopl.cpp)
#define FMOPL_TAG  CON_MAKE_TAG("FMOPL")
#define FMOPL_OK   CON_MAKE_OK("FMOPL")
#define FMOPL_WARN CON_MAKE_WARN("FMOPL")
#define FMOPL_ERR  CON_MAKE_ERR("FMOPL")

// Voice Over playback (osystemAL.cpp)
#define VO_TAG    CON_MAKE_TAG("VO")
#define VO_OK     CON_MAKE_OK("VO")
#define VO_WARN   CON_MAKE_WARN("VO")
#define VO_ERR    CON_MAKE_ERR("VO")

// Steam overlay (steamOverlay.cpp)
#define STEAM_TAG  CON_MAKE_TAG("STEAM")
#define STEAM_OK   CON_MAKE_OK("STEAM")
#define STEAM_WARN CON_MAKE_WARN("STEAM")
#define STEAM_ERR  CON_MAKE_ERR("STEAM")

// Game data auto-copy (gameDataCopy.cpp)
#define DATA_TAG   CON_MAKE_TAG("DATA")
#define DATA_OK    CON_MAKE_OK("DATA")
#define DATA_WARN  CON_MAKE_WARN("DATA")
#define DATA_ERR   CON_MAKE_ERR("DATA")

// Sequence player (sequence.cpp)
#define SEQ_TAG   CON_MAKE_TAG("SEQ")
#define SEQ_OK    CON_MAKE_OK("SEQ")
#define SEQ_WARN  CON_MAKE_WARN("SEQ")
#define SEQ_ERR   CON_MAKE_ERR("SEQ")

// ── Windows: enable ANSI virtual terminal processing ───────────────────────
#ifdef _WIN32
#include <windows.h>
static inline void enableConsoleColors()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE)
    {
        DWORD mode = 0;
        GetConsoleMode(hOut, &mode);
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}
#else
static inline void enableConsoleColors() {}
#endif
