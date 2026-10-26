// ============================================================================
//  LavaText sprite-sheet descriptor.
//
//  One place that knows the geometry of the baked "Glass" logo animation, so the
//  component, the loader and the in-game overlay all agree on it.
//
//  The sheet is a 12x10 grid of 512x230 cells. Each cell carries a generous
//  margin because the plume needs room to travel; the ink inside a cell occupies
//  u 0.109..0.943 and v 0.057..0.917, which is what the component samples
//  through. Those numbers come from measuring the alpha bounds across all 120
//  frames, not from a guess.
//
//  The embedded bytes are straight-alpha RGBA, matching ImGui's DX11
//  backend blend (SRC_ALPHA, INV_SRC_ALPHA).
// ============================================================================
#pragma once

// 120 frames over 1.5 s = 80 fps.
#define LAVA_LOGO_FRAMES    120
#define LAVA_LOGO_COLS      12
#define LAVA_LOGO_ROWS      10
#define LAVA_LOGO_FRAME_W   512
#define LAVA_LOGO_FRAME_H   230
#define LAVA_LOGO_ATLAS_W   (LAVA_LOGO_COLS * LAVA_LOGO_FRAME_W)   // 6144
#define LAVA_LOGO_ATLAS_H   (LAVA_LOGO_ROWS * LAVA_LOGO_FRAME_H)   // 2300
#define LAVA_LOGO_DURATION  1.50f

// Ink window inside one cell, in cell-relative units.
//
// MEASURED, not guessed: the alpha/luminance bounds across all 120 frames, from
// tools/measure_window.py. Getting this tight matters more than it looks. The
// frames reserve a wide, almost entirely black margin above the word for the
// plume, and the component samples through this window — so a loose window drags
// that black margin into the drawn mark. The previous window was 427x198 against
// 419x165 of real ink, i.e. 20% too tall, which spread the mark's brightness over
// a third more area and made it read as dull. Video-game UI and print art both
// make the same mistake; the ink box has to be the ink.
//
// Re-derive with:
//     py -3 the framework/test/LavaText/tools/measure_window.py
#define LAVA_LOGO_CONTENT_U0  0.1133f
#define LAVA_LOGO_CONTENT_V0  0.1696f
#define LAVA_LOGO_CONTENT_U1  0.9297f
#define LAVA_LOGO_CONTENT_V1  0.8826f

// ----------------------------------------------------------------------------
//  Tail layer: the frozen frame split so its sparks can move on their own.
//
//  The clip locks onto its last frame and stops, sparks included. A spark cannot
//  be animated inside that one image — moving it would leave a copy behind, and
//  nothing can erase the original because the area around a spark is transparent.
//  So the frozen cell is split once, offline, into two layers that stack:
//
//      spritesheet_tail.png   512 x 460
//        rows   0..229   the frozen frame with every satellite spark removed
//        rows 230..459   those sparks alone, on transparent
//
//  The component draws the base, then blits each spark back at the position it
//  was lifted from, displaced by its wobble. Only the sparks below ever move;
//  the wordmark and the plume are in the base and cannot.
//
//  A spark is only lifted if it is small and has a bright core, and only its own
//  pixels are cut, so lifting one cannot tear a hole in the fire or drag a piece
//  of the plume along. Detection runs on the 1024x460 master: at the packed
//  cell's resolution most sparks are fused into the plume and no longer separate
//  components.
//
//  Re-derive with:
//      py -3 the framework/test/LavaText/tools/split_sparks.py
//      py -3 tools/embed_png.py spritesheet_tail.png ../../../Loader/src/LavaTextTail.h \
//            --symbol LAVA_TAIL
// ----------------------------------------------------------------------------
#define LAVA_TAIL_ROWS   2       // the layer stacks base + sparks
#define LAVA_TAIL_SPARKS 24

// Spark boxes inside the frozen cell, in cell pixels: x0, y0, x1, y1.
#define LAVA_TAIL_SPARK_BOXES                                        \
    { 317, 148, 334, 162 }, { 407,  46, 417,  54 },                  \
    { 464,  65, 476,  72 }, { 336,  67, 346,  74 },                  \
    { 383,  52, 390,  59 }, { 352, 169, 359, 177 },                  \
    { 357,  51, 364,  58 }, { 446, 200, 453, 206 },                  \
    { 422,  41, 430,  48 }, { 415, 176, 420, 182 },                  \
    { 460, 125, 465, 129 }, { 331,  58, 336,  62 },                  \
    { 367,  49, 372,  54 }, { 448,  37, 452,  42 },                  \
    { 283, 152, 287, 156 }, { 444,  90, 448,  94 },                  \
    { 432,  41, 437,  45 }, { 473, 201, 478, 205 },                  \
    { 429, 186, 434, 190 }, { 299, 154, 303, 158 },                  \
    { 269, 146, 273, 150 }, { 415, 101, 420, 106 },                  \
    { 428,  90, 435, 100 }, { 315,  71, 319,  74 }

namespace opal {
namespace media {

// The tail layer's shader resource view, published by CreateLavaTextSheet and
// read by the Title component. A plain void* slot so this descriptor header
// stays free of any D3D include, and zero-initialised so a host that never
// loads the asset simply keeps the single-image behaviour.
inline void*& LavaTailSrv() {
    static void* s_tail = nullptr;
    return s_tail;
}

}  // namespace media
}  // namespace opal
