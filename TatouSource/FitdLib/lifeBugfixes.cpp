#include "common.h"
#include "lifeBugfixes.h"

#include <cstdio>
#include <cstring>
#include <vector>
#include <iterator>

// Offsets below are byte offsets into the authoritative AITD1 LISTLIFE entries,
// verified against the original resources and LifeDISA output.
namespace
{
int loadedResourceSize(hqrEntryStruct<char>* archive, int key)
{
    if (!archive)
        return 0;
    for (const auto& entry : archive->entries)
        if (entry.key == key && entry.ptr)
            return entry.size;
    return 0;
}

unsigned short readWord(const char* data, int offset)
{
    return static_cast<unsigned short>(
        static_cast<unsigned char>(data[offset]) |
        (static_cast<unsigned short>(static_cast<unsigned char>(data[offset + 1])) << 8));
}

void writeWord(char* data, int offset, unsigned short value)
{
    data[offset] = static_cast<char>(value & 0xFF);
    data[offset + 1] = static_cast<char>((value >> 8) & 0xFF);
}

struct WordPatch
{
    int byteOffset;
    unsigned short original;
    unsigned short replacement;
    const char* reason;
};

// LIFE 550 (player floor LIFE).
//  - 848: z of `changeroom E5R3 -300 -4010 6000` (case 40, cellar -> tunnels).
//    The original arrival lies 10 units inside F5:R3:SZ1, the type-10 return
//    zone (z 5990..6990), so the player could bounce straight back to 4/0.
//  - 1694 and 1810: variable index of `if_egal var_33 0` in case 9 (entering
//    the tunnels from Jeremy's Study) and case 11 (from the cellar). var_33 is
//    written only by the endgame, so every entry re-placed the worm, even after
//    it had been lured in room 5/4 (var_172 = 1, set once). From the cellar
//    that re-blocked 5/3; from the Study, after rooms 8/9 had deleted the lured
//    worm, it brought it back in front of the cellar passage with the bridge
//    already down. Guarding both placements with var_172 keeps a lured worm
//    out of the way.
constexpr WordPatch kLife550Patches[] = {
    { 848, 6000, 5800, "cellar->tunnel arrival z moved out of return zone" },
    { 1694, 33, 172, "worm Study placement guarded by lure flag var_172" },
    { 1810, 33, 172, "worm cellar placement guarded by lure flag var_172" },
};

bool applyWordPatch(char* data, const WordPatch& patch)
{
    const unsigned short found = readWord(data, patch.byteOffset);
    if (found == patch.replacement)
        return true; // HQR cache entry already patched.
    if (found != patch.original)
    {
        std::printf("[LIFE BUGFIX] LIFE 550 offset %d mismatch: expected %u, found %u; skipped (%s)\n",
            patch.byteOffset, patch.original, found, patch.reason);
        return false;
    }
    writeWord(data, patch.byteOffset, patch.replacement);
    return true;
}

// LIFE 522 (endgame cleanup) gets `var var_172 1` inserted before its final
// endlife, so the worm can never be re-placed at the cellar after the endgame.
// The two leading `goto 330` jumps (word offsets at bytes 8 and 20) are moved
// past the inserted instruction to keep targeting endlife.
constexpr int kLife522Length = 332;
constexpr int kLife522InsertAt = 330;
constexpr unsigned char kLife522Insert[] = { 0x13, 0x00, 0xAC, 0x00, 0xFF, 0xFF, 0x01, 0x00 };
constexpr unsigned char kLife522Tail[] = { 0x13, 0x00, 0x21, 0x00, 0xFF, 0xFF, 0x01, 0x00 }; // var var_33 1

std::vector<char> gLife522Patched;
bool gLife522MismatchReported = false;

char* patchLife522(char* data, int size)
{
    // An already patched LISTLIFE (the same 340 bytes, e.g. from the AITD Randomizer) is used as it is
    if (size >= kLife522Length + static_cast<int>(sizeof(kLife522Insert)) &&
        readWord(data, 8) == 160 + sizeof(kLife522Insert) / 2 &&
        readWord(data, 20) == 154 + sizeof(kLife522Insert) / 2 &&
        std::memcmp(data + kLife522InsertAt, kLife522Insert, sizeof(kLife522Insert)) == 0)
        return data;

    // Recheck each loaded entry: an HQR reset can load different game data.
    const bool matches =
        readWord(data, 8) == 160 &&
        readWord(data, 20) == 154 &&
        std::memcmp(data + kLife522InsertAt - sizeof(kLife522Tail), kLife522Tail, sizeof(kLife522Tail)) == 0;
    if (!matches)
    {
        if (!gLife522MismatchReported)
        {
            gLife522MismatchReported = true;
            std::printf("[LIFE BUGFIX] LIFE 522 layout mismatch; endgame lure-flag patch skipped\n");
        }
        return data;
    }
    if (gLife522Patched.empty())
    {
        const int insertWords = static_cast<int>(sizeof(kLife522Insert) / 2);
        gLife522Patched.assign(data, data + kLife522Length);
        gLife522Patched.insert(gLife522Patched.begin() + kLife522InsertAt,
            std::begin(kLife522Insert), std::end(kLife522Insert));
        writeWord(gLife522Patched.data(), 8, static_cast<unsigned short>(160 + insertWords));
        writeWord(gLife522Patched.data(), 20, static_cast<unsigned short>(154 + insertWords));
    }
    return gLife522Patched.data();
}

// LIFE 460 (worm, office route: waits for the player in room 5/4, then starts
// the TRACK 20 chase) gets `test_col 1` in front of `trackmode 3 20`. Entering
// the tunnels from the cellar first runs LIFE 458 case 1 `test_col 0`, which is
// saved with the worm (world flag 0x20) when it unloads. The later chase then
// has no wall collision, misses OBJ 219's stop zone and walks off forever.
// The if_egal jump (word at byte 10) moves 7 -> 9 words to keep targeting
// endlife. An already patched LISTLIFE (the same 32 bytes) is used as it is.
constexpr int kLife460Length = 28;
constexpr unsigned char kLife460Original[] = {
    0x04, 0x00, 0x1F, 0x80, 0x01, 0x00, 0xFF, 0xFF, 0x04, 0x00, 0x07, 0x00, // if_egal obj_1.room #4 @26
    0x0F, 0x00, 0x03, 0x00, 0x14, 0x00,                                     // trackmode 3 20
    0x0D, 0x00, 0xC2, 0x00,                                                 // anim_repeat 194
    0x1F, 0x00, 0xCD, 0x01,                                                 // life 461
    0x0C, 0x00,                                                             // endlife
};
constexpr unsigned char kLife460Patched[] = {
    0x04, 0x00, 0x1F, 0x80, 0x01, 0x00, 0xFF, 0xFF, 0x04, 0x00, 0x09, 0x00, // if_egal obj_1.room #4 @30
    0x36, 0x00, 0x01, 0x00,                                                 // test_col 1
    0x0F, 0x00, 0x03, 0x00, 0x14, 0x00,                                     // trackmode 3 20
    0x0D, 0x00, 0xC2, 0x00,                                                 // anim_repeat 194
    0x1F, 0x00, 0xCD, 0x01,                                                 // life 461
    0x0C, 0x00,                                                             // endlife
};
static_assert(sizeof(kLife460Original) == kLife460Length, "LIFE 460 original length");
static_assert(sizeof(kLife460Patched) == kLife460Length + 4, "LIFE 460 grows by test_col 1");
constexpr int kLife460InsertAt = 12;

std::vector<char> gLife460Patched;
bool gLife460MismatchReported = false;

char* patchLife460(char* data, int size)
{
    // Bytes 12..15 tell the two layouts apart without reading past a 28-byte entry
    if (std::memcmp(data + kLife460InsertAt, kLife460Patched + kLife460InsertAt, 4) == 0)
    {
        if (size >= static_cast<int>(sizeof(kLife460Patched)) &&
            std::memcmp(data, kLife460Patched, sizeof(kLife460Patched)) == 0)
            return data; // already patched
    }
    else if (std::memcmp(data, kLife460Original, sizeof(kLife460Original)) == 0)
    {
        if (gLife460Patched.empty())
            gLife460Patched.assign(kLife460Patched, kLife460Patched + sizeof(kLife460Patched));
        return gLife460Patched.data();
    }
    if (!gLife460MismatchReported)
    {
        gLife460MismatchReported = true;
        std::printf("[LIFE BUGFIX] LIFE 460 layout mismatch; worm chase wall-collision patch skipped\n");
    }
    return data;
}
}

char* lifeBugfixPatchLife(int lifeNum, char* lifeData)
{
    if (g_gameId != AITD1 || !lifeData)
        return lifeData;

    const int size = loadedResourceSize(listLife, lifeNum);
    if ((lifeNum == 550 && size < 1812) ||
        (lifeNum == 522 && size < kLife522Length) ||
        (lifeNum == 460 && size < kLife460Length))
        return lifeData;

    if (lifeNum == 550)
    {
        for (const WordPatch& patch : kLife550Patches)
            applyWordPatch(lifeData, patch);
    }
    else if (lifeNum == 522)
    {
        return patchLife522(lifeData, size);
    }
    else if (lifeNum == 460)
    {
        return patchLife460(lifeData, size);
    }
    return lifeData;
}

bool lifeBugfixForcesBytecode(int lifeNum)
{
    return g_gameId == AITD1 && (lifeNum == 550 || lifeNum == 522 || lifeNum == 460);
}

// TRACK 40 (scripted walk from the cellar 4/0 into the tunnels 5/3). The
// original runs MARK 1 (LIFE 550 teleports the player at MARK 1), MARK 2 and
// ANGLE at walking speed before STOP, one instruction per frame. The player
// keeps walking east for those frames with hard collision off and can end up
// inside wall F5:R3:HC9. Moving STOP in front of MARK 1 stops him before the
// teleport, so he stays at the scripted arrival point. Bytes 14..31 only; the
// track length and every other instruction are unchanged.
namespace
{
constexpr int kTrack40PatchOffset = 14;
constexpr unsigned char kTrack40Original[] = {
    0x04, 0x00, 0x01, 0x00,                         // MARK 1
    0x04, 0x00, 0x02, 0x00,                         // MARK 2
    0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ANGLE 0 0 0
    0x07, 0x00,                                     // STOP
};
constexpr unsigned char kTrack40Patched[] = {
    0x07, 0x00,                                     // STOP
    0x04, 0x00, 0x01, 0x00,                         // MARK 1
    0x04, 0x00, 0x02, 0x00,                         // MARK 2
    0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // ANGLE 0 0 0
};
static_assert(sizeof(kTrack40Original) == sizeof(kTrack40Patched), "TRACK 40 patch must keep length");

// TRACK 41 (scripted walk from the tunnels 5/3 back up to the cellar 4/0) has
// the same defect: LIFE 550 teleports at MARK 1, then MARK 2, MARK 3 and
// ANGLE 768 run at walking speed before STOP. The player keeps walking north
// with hard collision off and can end up inside cellar wall F4:R0:HC2
// (z 5000..5300). Moving STOP in front of MARK 1 keeps him at the scripted
// cellar arrival point. Bytes 14..35 only.
constexpr int kTrack41PatchOffset = 14;
constexpr unsigned char kTrack41Original[] = {
    0x04, 0x00, 0x01, 0x00,                         // MARK 1
    0x04, 0x00, 0x02, 0x00,                         // MARK 2
    0x04, 0x00, 0x03, 0x00,                         // MARK 3
    0x13, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, // ANGLE 0 768 0
    0x07, 0x00,                                     // STOP
};
constexpr unsigned char kTrack41Patched[] = {
    0x07, 0x00,                                     // STOP
    0x04, 0x00, 0x01, 0x00,                         // MARK 1
    0x04, 0x00, 0x02, 0x00,                         // MARK 2
    0x04, 0x00, 0x03, 0x00,                         // MARK 3
    0x13, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, // ANGLE 0 768 0
};
static_assert(sizeof(kTrack41Original) == sizeof(kTrack41Patched), "TRACK 41 patch must keep length");

struct TrackPatch
{
    int track;
    int offset;
    const unsigned char* original;
    const unsigned char* patched;
    size_t size;
    const char* name;
    bool mismatchReported;
};

TrackPatch gTrackPatches[] = {
    { 40, kTrack40PatchOffset, kTrack40Original, kTrack40Patched, sizeof(kTrack40Patched), "cellar->tunnel arrival STOP", false },
    { 41, kTrack41PatchOffset, kTrack41Original, kTrack41Patched, sizeof(kTrack41Patched), "tunnel->cellar arrival STOP", false },
};
}

char* trackBugfixPatchTrack(int trackNum, char* trackData)
{
    if (g_gameId != AITD1 || !trackData)
        return trackData;

    for (TrackPatch& patch : gTrackPatches)
    {
        if (patch.track != trackNum)
            continue;
        if (loadedResourceSize(listTrack, trackNum) < patch.offset + static_cast<int>(patch.size))
            return trackData;
        char* site = trackData + patch.offset;
        if (std::memcmp(site, patch.patched, patch.size) == 0)
            return trackData; // HQR cache entry already patched.
        if (std::memcmp(site, patch.original, patch.size) != 0)
        {
            if (!patch.mismatchReported)
            {
                patch.mismatchReported = true;
                std::printf("[LIFE BUGFIX] TRACK %d layout mismatch; %s patch skipped\n", patch.track, patch.name);
            }
            return trackData;
        }
        std::memcpy(site, patch.patched, patch.size);
    }
    return trackData;
}
