// IPC between the PPT2NX-AI sysmodule and the overlay.
#pragma once
#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PPT2AI_SERVICE_NAME "ppt2ai"
#define PPT2AI_IPC_VERSION  11
#define PPT2AI_PROGRAM_ID   0x4200000050324149ULL

typedef enum {
    Ppt2AiCmd_GetVersion          = 0,
    Ppt2AiCmd_GetStatus           = 1,
    Ppt2AiCmd_GetConfig           = 2,
    Ppt2AiCmd_SetConfig           = 3,
    Ppt2AiCmd_SetManualInput      = 4,
    Ppt2AiCmd_GetBoard            = 5,
    Ppt2AiCmd_ReconnectController = 6,
    Ppt2AiCmd_SetAttachPaused     = 7, // detach from the game (not saved)
    Ppt2AiCmd_SetOverlayVisible   = 8, // on show / hide, and every second while shown
    Ppt2AiCmd_SwapControllers     = 9, // u8[2]: player slots (0 = 1P)
    Ppt2AiCmd_Exit                = 10, // release everything and exit
} Ppt2AiCmd;

typedef enum {
    Ppt2AiPhase_NoGame     = 0, // the game is not running, or could not be attached
    Ppt2AiPhase_WrongBuild = 1, // unsupported game version
    Ppt2AiPhase_Menu       = 2,
    Ppt2AiPhase_CharSelect = 3,
    Ppt2AiPhase_StartAnim  = 4,
    Ppt2AiPhase_InMatch    = 5,
} Ppt2AiPhase;

// Ppt2AiStatus::match_note
typedef enum {
    Ppt2AiMatchNote_None      = 0,
    Ppt2AiMatchNote_Plays     = 1, // the AI plays this match
    Ppt2AiMatchNote_Replay    = 2,
    Ppt2AiMatchNote_Over      = 3, // results
    Ppt2AiMatchNote_OtherRule = 4, // a rule or mode the AI does not play
} Ppt2AiMatchNote;

#define PPT2AI_BOARD_COLS 10
#define PPT2AI_BOARD_ROWS 28
#define PPT2AI_CHARACTERS 40      // character select grid, 4 x 10
#define PPT2AI_FAVORITE_BYTES 10  // 2 bits per character
#define PPT2AI_PIECE_NONE (-1)

// Pieces: S=0 Z=1 J=2 L=3 T=4 O=5 I=6
typedef struct {
    u32 version;
    u8  phase;                 // Ppt2AiPhase
    u8  enabled;
    u8  controller_attached;
    u8  game_attached;
    u32 last_error;            // last failed Result, 0 = none
    u64 main_base;
    u64 buttons;               // buttons held on the virtual controller

    // AI 1's player (StartAnim / InMatch)
    u32 frame;
    s8  current;
    s8  hold;
    s8  x, y, rotation;
    s8  queue[5];
    u8  combo, b2b;

    // character select
    u8  charsel_count;
    u8  charsel_index[4];
    u8  charsel_step;
    u8  attach_paused;
    u8  bot_suspended;         // overlay open or game input blocked
    u8  game_input_blocked;
    u8  net_online;            // 1 online, 2 was online, 3 the game started before the sysmodule,
                               // 4 network state unknown; the AIs stay off in all of these
    u8  match_note;            // Ppt2AiMatchNote
    u8  reserved[1];

    // last move (InMatch)
    u16 last_think_ms;
    u8  last_depth;
    u8  dual;                  // AI 2 is on; the fields ending in 2 are AI 2's
    u8  controller2_attached;
    u8  last_depth2;
    u16 last_think_ms2;
    u64 buttons2;
    s8  ai_player[2];          // player slot of each AI (0 = 1P), -1 = none
    u8  slot_owner[4];         // Ppt2AiSlotOwner of 1P..4P
    s8  remote_ai;             // the AI the user's controller drives, -1 = none
    u8  reserved3[1];
} Ppt2AiStatus;

typedef enum {
    Ppt2AiSlotOwner_Unknown = 0,
    Ppt2AiSlotOwner_Ai1     = 1,
    Ppt2AiSlotOwner_Ai2     = 2,
    Ppt2AiSlotOwner_Other   = 3, // e.g. a real controller
} Ppt2AiSlotOwner;

typedef struct {
    u8  enabled;
    u8  player;                // AI 1's player slot (0 = 1P, 1 = 2P)
    u8  auto_charsel;
    u8  character;             // row * 10 + col
    u8  mode_select_right;     // Right presses before picking Puyo Puyo / Tetris
    u8  applet_auto_join;      // press L+R in the controller applet
    u8  reserved0;
    u8  trace_inputs;
    u32 press_ms;              // menu button press / release times
    u32 release_ms;
    u32 applet_join_delay_ms;

    // Tetris AI
    u16 search_width;          // 10..150 (PC Zetris "Intelligence")
    u8  previews;              // 1..19
    u8  speed;                 // 10..100 % (PC Zetris "Speed")
    u16 min_think_ms;

    // AI 1's Tetris strategy (PC Zetris names)
    u8  style;                 // Ppt2AiStyle
    u8  c4w;                   // 中開けREN
    u8  hold_allowed;          // ホールド使用
    u8  perfect_clear;         // パフェ発見機
    u8  pc_enhance;            // 火力優先のパフェルートを選択
    u8  all_spins;             // 特殊回転テトリス
    u8  tsd_only;              // TSDのみ (TSD20発用)
    u8  das_frames;
    u8  arr_frames;

    // AI 2
    u8  dual;
    u8  dual_search_width;     // both AIs' search width when dual, 10..70
    u8  character2;
    u8  style2;
    u8  hold_allowed2;
    u8  c4w2;
    u8  all_spins2;
    u8  tsd_only2;
    u8  reserved1;
    u8  softdrop_spins_only;
    u8  ai_cores;              // search on the game's idle cores 1 and 2
    u8  voice_change;          // alternate voice (X before A at character select)
    u8  voice_change2;
    u8  favorites[PPT2AI_FAVORITE_BYTES]; // bit 2 * character + voice
    u8  random_character;      // a random favorite at every character select
    u8  random_character2;
    u8  score_stats;           // debug: depth and think time in the score
    u8  puyo;                  // play Puyo Puyo instead of Tetris
    u8  puyo2;
    u8  reserved2;
    u8  remote;                // outside matches the user's ZR / ZL / right stick drive an AI's controller
    u8  pc_opener;             // 開幕パフェを狙う
    u8  pc_midgame;            // 試合中のパフェ: 0 off, 1 next 5, 2 next 10
    u8  perfect_clear2;
    u8  pc_enhance2;
    u8  pc_opener2;
    u8  pc_midgame2;
} Ppt2AiConfig;

typedef enum {
    Ppt2AiStyle_TSpinB2B = 0,  // T-Spin+ B2B (default)
    Ppt2AiStyle_TSpin    = 1,  // T-Spin+
    Ppt2AiStyle_TST      = 2,  // TST
    Ppt2AiStyle_Ultra    = 3,  // Ultra
    Ppt2AiStyle_20TSD    = 4,  // 20 TSD
    Ppt2AiStyle_TSpinTuned = 5, // T-Spin+ 調整版
    Ppt2AiStyle_Count    = 6,
} Ppt2AiStyle;

typedef struct {
    u64 buttons;               // HidNpadButton_*
    s32 lx, ly, rx, ry;
    u32 duration_ms;           // 0 = release now
    u32 delay_ms;
    u8  pad;                   // 0 = AI 1, 1 = AI 2
    u8  reserved[7];
} Ppt2AiManualInput;

typedef struct {
    s8 cells[PPT2AI_BOARD_COLS][PPT2AI_BOARD_ROWS]; // [col][row], row 0 = bottom, -1 = empty
    u8 valid;                  // 0 outside a match
    s8 current;
    s8 hold;
    s8 queue[5];
} Ppt2AiBoard;

#ifdef __cplusplus
}
#endif
