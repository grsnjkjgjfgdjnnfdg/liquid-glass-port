// ============================================================================
// imgui_obfuscate.h — build-time ImGui symbol renaming to break signature scans
//
// BE and EAC scan for known ImGui byte patterns and function prologues in DLLs
// injected into the game process. By renaming internal functions at compile time,
// the binary loses its predictable ImGui fingerprint. User-facing API calls in
// DX11Overlay.h / main.cpp are transparently routed through the renamed symbols
// because this header is included in imconfig.h, which EVERY ImGui TU includes.
//
// Includes the VERSION string change — the most signature-scannable string in
// the vanilla Dear ImGui binary.
// ============================================================================

// Replace version string to break the most trivial signature scan
#undef  IMGUI_VERSION
#define IMGUI_VERSION "UI-Framework v4.2.0"

// ---- Internal functions with most distinctive bytecode patterns ----
// These are called frequently in hot paths (rendering, layout, input) and
// have well-known prologue signatures in vanilla ImGui.

#define ItemAdd                LcUI_GfxItemAdd
#define ItemSize               LcUI_GfxItemSize  
#define ItemHoverable          LcUI_GfxItemHover
#define PushClipRect           LcUI_PushClip
#define PopClipRect            LcUI_PopClip
#define GetCurrentClipRect     LcUI_CurrClip
#define RenderFrame            LcUI_RenderFrame
#define RenderFrameBorder      LcUI_RenderBorder
#define RenderCheckMark        LcUI_RenderCheck
#define RenderArrow            LcUI_RenderArrow
#define RenderBullet           LcUI_RenderBullet
#define RenderRectFilledWithHole LcUI_RectHole
#define RenderTextClipped      LcUI_TextClipped
#define RenderText             LcUI_TextDraw
#define ShadeVertsLinearColorGradientKeepAlpha LcUI_ShadeVerts
#define ShadeVertsLinearUV     LcUI_ShadeVertsUV
#define ColorConvertU32ToFloat4 LcUI_ColCvtU32
#define ColorConvertFloat4ToU32 LcUI_ColCvtF32

// ---- Window management internals ----
#define FindWindowByID         LcUI_FindWinID
#define FindWindowByName       LcUI_FindWinName
#define FocusWindow            LcUI_FocusWindow
#define SetWindowPos           LcUI_SetWinPos
#define SetWindowSize          LcUI_SetWinSize
#define SetWindowHiddenAndSkipItems LcUI_SetWinHide
#define BeginPopupEx           LcUI_BeginPopupEx
#define OpenPopupEx            LcUI_OpenPopupEx
#define ClosePopupToLevel      LcUI_ClosePopupLv
#define BeginTooltipEx         LcUI_BeginTooltipEx

// ---- Input/scrolling internals ----
#define UpdateMouseMovingWindowNewFrame LcUI_MouseWinNF
#define UpdateMouseMovingWindowEndFrame LcUI_MouseWinEF
#define ScrollToBringRectIntoView LcUI_ScrollToRect
#define SetScrollX             LcUI_SetScrollX
#define SetScrollY             LcUI_SetScrollY
#define Scrollbar              LcUI_Scrollbar
#define GetScrollMaxX          LcUI_ScrollMaxX
#define GetScrollMaxY          LcUI_ScrollMaxY

// ---- Layout/alignment internals ----
#define LayoutType             LcUI_LayoutType
#define AlignWithLayout        LcUI_AlignLayout
#define CalcWindowNextAutoFitSize LcUI_AutoFitSize

// ---- Drawing/vertex submission ----
#define PrimReserve            LcUI_PrimReserve
#define PrimUnreserve          LcUI_PrimUnreserve
#define PrimQuadUV             LcUI_PrimQuadUV
#define PrimRect               LcUI_PrimRect
#define PrimRectUV             LcUI_PrimRectUV
#define PrimWriteVtx           LcUI_PrimWriteVtx
#define PrimWriteIdx           LcUI_PrimWriteIdx

// ---- Text/font rendering internals (signable via FreeType patterns) ----
#define AddTextComplexity      LcUI_TextComplex
#define RenderTextDefault      LcUI_TextRenderDef

// ---- Core lifecycle (renamed to avoid COM/vtable conflicts) ----
// NOTE: Initialize/Shutdown NOT renamed — they conflict with Windows COM
// IWICBitmapScaler::Initialize, IWICFormatConverter::Initialize, etc.
#define CreateContext          LcUI_CreateCtx
#define DestroyContext         LcUI_DestroyCtx
#define GetCurrentContext      LcUI_CurrCtx
#define SetCurrentContext      LcUI_SetCtx
#define NewFrame               LcUI_NewFrameCore
#define Render                 LcUI_RenderCore
#define EndFrame               LcUI_EndFrameCore
#define UpdateInputEvents      LcUI_InputEvents

// ---- Settings/IO internals ----
#define LoadIniSettingsFromDisk LcUI_LoadIni
#define SaveIniSettingsToDisk  LcUI_SaveIni
#define LoadIniSettingsFromMemory LcUI_LoadIniMem

// ---- Debug internals (not renamed — Show* prefix conflicts with Win32 ShowWindow) ----

// ---- Navigation/focus internals ----
#define NavInitWindow          LcUI_NavInitWin
#define NavMoveRequestSubmit   LcUI_NavMoveSubmit
#define NavMoveRequestButPendingApply LcUI_NavMoveApply

// ---- Tables internals (large codegen, easily signatured) ----
#define BeginTable             LcUI_BeginTable
#define EndTable               LcUI_EndTable
#define TableSetupColumn       LcUI_TableCol
#define TableSetupScrollFreeze LcUI_TableScrollFrz
#define TableGetSortSpecs      LcUI_TableSortSpc
#define TableSetColumnEnabled  LcUI_TableColEn
#define TableGetRowIndex       LcUI_TableRowIdx
#define TableNotifyItem        LcUI_TableNotify

// ---- Misc utilities ----
#define GetKeyIndex            LcUI_KeyIdx
#define IsKeyDown              LcUI_KeyDown
#define IsKeyPressed           LcUI_KeyPressed
#define IsMouseDown            LcUI_MouseDown
#define IsMouseClicked         LcUI_MouseClick
#define GetMousePos            LcUI_MousePos
#define GetMouseDragDelta      LcUI_MouseDragD
#define SetItemUsingMouseWheel LcUI_ItemMouseWhl
#define SetActiveIdUsingAllKeyboardKeys LcUI_ActIdKeys

// ---- Platform/tool internals (rarely used but distinctive) ----
#define GetPlatformIO          LcUI_GetPlatformIO
#define GetMainViewport        LcUI_MainViewport
