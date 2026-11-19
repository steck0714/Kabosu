// PPT2NX-AI Tesla overlay
#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <functional>
#include <string>
#include <vector>
#include "ppt2ai_client.h"

// RELEASE=1 hides the debug items
#ifndef PPT2AI_RELEASE
#define PPT2AI_RELEASE 0
#endif

namespace {

constexpr const char *Version = "v1.0.0";

bool g_connected = false;    // connected, same IPC version
u32 g_sysmodule_version = 0; // 0 = no sysmodule
Ppt2AiConfig g_config = {};

const char *PhaseName(u8 phase) {
    switch (phase) {
    case Ppt2AiPhase_NoGame:     return "ゲーム未起動";
    case Ppt2AiPhase_WrongBuild: return "未対応のバージョン";
    case Ppt2AiPhase_Menu:       return "メニュー";
    case Ppt2AiPhase_CharSelect: return "キャラ選択";
    case Ppt2AiPhase_StartAnim:  return "試合開始";
    case Ppt2AiPhase_InMatch:    return "試合中";
    default:                     return "?";
    }
}

const char *StateName(const Ppt2AiStatus &s) {
    if (s.phase == Ppt2AiPhase_StartAnim || s.phase == Ppt2AiPhase_InMatch)
        switch (s.match_note) {
        case Ppt2AiMatchNote_Replay:    return "リプレイ中（AI は操作しない）";
        case Ppt2AiMatchNote_Over:      return "試合終了";
        case Ppt2AiMatchNote_OtherRule: return "AI が遊べないルール・モード";
        default:                        break;
        }
    return PhaseName(s.phase);
}

const char *OwnerName(u8 owner) {
    switch (owner) {
    case Ppt2AiSlotOwner_Ai1:   return "AI 1";
    case Ppt2AiSlotOwner_Ai2:   return "AI 2";
    case Ppt2AiSlotOwner_Other: return "ほか";
    default:                    return "なし";
    }
}

const char *SlotName(s8 slot) {
    static const char *const names[] = {"1P", "2P", "3P", "4P"};
    return slot >= 0 && slot < 4 ? names[slot] : "不参加";
}

// Character select grid, row * 10 + col
constexpr const char *CharacterNames[] = {
    "りんご", "アルル", "アミティ", "まぐろ", "カーバンクル", "ティ", "オー", "エス", "アイ", "ジェイ＆エル",
    "シグ", "りすくませんぱい", "すけとうだら", "ウィッチ", "ドラコケンタウロス", "ゼット", "クルーク", "レムレス", "ラフィーナ", "ルルー",
    "シェゾ", "マール", "エコロ", "アリィ", "ソニック", "フェーリ", "サタン", "エックス", "スクエアス", "アコール先生",
    "リデル", "セリリ", "ユウちゃん＆レイくん", "ハーピー", "レガムント", "さかな王子", "ラフィソル", "あやしいクルーク", "ラグナス", "ローザッテ",
};
constexpr int CharacterCount = sizeof(CharacterNames) / sizeof(CharacterNames[0]);

const char *CharacterName(int index) {
    return index >= 0 && index < CharacterCount ? CharacterNames[index] : "?";
}

char PieceChar(s8 piece) {
    return piece >= 0 && piece < 7 ? "SZJLTOI"[piece] : '-';
}

tsl::Color PieceColor(s8 cell) {
    switch (cell) {
    case 0:  return tsl::Color(0x4, 0xD, 0x4, 0xF); // S
    case 1:  return tsl::Color(0xE, 0x3, 0x3, 0xF); // Z
    case 2:  return tsl::Color(0x3, 0x5, 0xE, 0xF); // J
    case 3:  return tsl::Color(0xF, 0x9, 0x2, 0xF); // L
    case 4:  return tsl::Color(0xA, 0x3, 0xD, 0xF); // T
    case 5:  return tsl::Color(0xE, 0xD, 0x2, 0xF); // O
    case 6:  return tsl::Color(0x2, 0xC, 0xE, 0xF); // I
    default: return tsl::Color(0x8, 0x8, 0x8, 0xF); // garbage
    }
}

bool g_hiding = false;   // update() still runs during the fade-out
u64 g_refocus_at_ms = 0; // when to take input focus back after a tap, 0 = none

u64 NowMs() {
    return armTicksToNs(armGetSystemTick()) / 1000000;
}

bool Connect() {
    g_sysmodule_version = 0;
    g_connected = false;
    if (!ppt2aiIsRunning()) {
        ppt2aiExit();
        return false;
    }
    g_connected = R_SUCCEEDED(ppt2aiInitialize()) && R_SUCCEEDED(ppt2aiGetVersion(&g_sysmodule_version)) &&
                  g_sysmodule_version == PPT2AI_IPC_VERSION && R_SUCCEEDED(ppt2aiGetConfig(&g_config));
    if (!g_connected)
        ppt2aiExit();
    return g_connected;
}

// Called from every update(): once a second, sends a heartbeat and reconnects if the sysmodule restarted.
void Heartbeat() {
    static u64 last = 0;
    u64 now = NowMs();
    if (g_refocus_at_ms && now >= g_refocus_at_ms) {
        tsl::hlp::requestForeground(true);
        g_refocus_at_ms = 0;
    }
    if (now - last < 1000)
        return;
    last = now;
    if (g_connected && !ppt2aiIsRunning()) {
        g_connected = false;
        g_sysmodule_version = 0;
        ppt2aiExit();
    } else if (!g_connected && g_sysmodule_version == 0) {
        Connect();
    }
    if (g_connected && !g_hiding)
        ppt2aiSetOverlayVisible(true);
}

void PushConfig() {
    ppt2aiSetConfig(&g_config);
}

// pad: 0 = AI 1, 1 = AI 2
void Tap(u64 buttons, u32 ms = 100, u32 delay_ms = 0, u8 pad = 0) {
    Ppt2AiManualInput in = {};
    in.buttons = buttons;
    in.duration_ms = ms;
    in.delay_ms = delay_ms;
    in.pad = pad;
    ppt2aiSetManualInput(&in);
}

// The game gets no input while the overlay has focus, so give it back for the press.
void TapKeepOpen(u64 buttons, u32 ms = 100, u8 pad = 0) {
    tsl::hlp::requestForeground(false);
    Tap(buttons, ms, 50, pad);
    g_refocus_at_ms = NowMs() + 50 + ms + 250;
}

// L+R on each AI's controller, the 1P one first (players are numbered in join order).
void JoinNow() {
    if (!g_config.dual) {
        TapKeepOpen(HidNpadButton_L | HidNpadButton_R, 300);
        return;
    }
    u8 first = g_config.player == 1 ? 1 : 0;
    tsl::hlp::requestForeground(false);
    Tap(HidNpadButton_L | HidNpadButton_R, 300, 50, first);
    Tap(HidNpadButton_L | HidNpadButton_R, 300, 600, first ^ 1);
    g_refocus_at_ms = NowMs() + 600 + 300 + 250;
}

std::string ButtonsText(u64 b) {
    static const std::pair<u64, const char *> names[] = {
        {HidNpadButton_A, "A"}, {HidNpadButton_B, "B"}, {HidNpadButton_X, "X"}, {HidNpadButton_Y, "Y"},
        {HidNpadButton_L, "L"}, {HidNpadButton_R, "R"}, {HidNpadButton_ZL, "ZL"}, {HidNpadButton_ZR, "ZR"},
        {HidNpadButton_Plus, "+"}, {HidNpadButton_Minus, "-"}, {HidNpadButton_Up, "↑"}, {HidNpadButton_Down, "↓"},
        {HidNpadButton_Left, "←"}, {HidNpadButton_Right, "→"},
    };
    std::string s;
    for (auto &[mask, name] : names)
        if (b & mask) {
            if (!s.empty())
                s += " ";
            s += name;
        }
    return s.empty() ? "なし" : s;
}

// Same as ovlSysmodules (pm:shell)
Result StartSysmodule() {
    NcmProgramLocation loc = {};
    loc.program_id = PPT2AI_PROGRAM_ID;
    loc.storageID = NcmStorageId_None;
    u64 pid = 0;
    return pmshellLaunchProgram(0, &loc, &pid);
}

tsl::elm::ListItem *StartItem() {
    auto *item = new tsl::elm::ListItem("sysmodule を起動", "");
    item->setClickListener([item](u64 keys) {
        if (!(keys & HidNpadButton_A))
            return false;
        if (g_connected || ppt2aiIsRunning()) {
            item->setValue("実行中");
            return true;
        }
        Result rc = StartSysmodule();
        char text[32];
        if (R_SUCCEEDED(rc))
            snprintf(text, sizeof(text), "起動しました");
        else
            snprintf(text, sizeof(text), "失敗 (%x)", (unsigned)rc);
        item->setValue(text);
        return true;
    });
    return item;
}

class ErrorGui : public tsl::Gui {
public:
    void update() override;

    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("PPT2NX-AI", Version);
        auto *list = new tsl::elm::List();
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *r, s32 x, s32 y, s32 w, s32 h) {
            if (g_sysmodule_version != 0) {
                // IPC version mismatch
                char line[64];
                snprintf(line, sizeof(line), "（sysmodule %u、オーバーレイ %u）", g_sysmodule_version,
                         PPT2AI_IPC_VERSION);
                r->drawString("バージョンが違います。", false, x + 20, y + 60, 20, r->a(0xF44F));
                r->drawString(line, false, x + 20, y + 100, 16, r->a(0xFFFF));
                r->drawString("両方を同じビルドに入れ替えて再起動してください。", false, x + 20, y + 125, 16, r->a(0xFFFF));
                return;
            }
            r->drawString("sysmoduleが動いていません。", false, x + 20, y + 60, 20, r->a(0xF44F));
            r->drawString("下の「sysmodule を起動」で起動できます。", false, x + 20, y + 100, 16, r->a(0xFFFF));
            r->drawString("ぷよテト2 が起動中なら、そのあと一度", false, x + 20, y + 130, 16, r->a(0xFFFF));
            r->drawString("ぷよテト2 を終了して起動し直してください", false, x + 20, y + 155, 16, r->a(0xFFFF));
            r->drawString("（ネット対戦の制限のため）。", false, x + 20, y + 180, 16, r->a(0xFFFF));
            r->drawString("起動しないときは atmosphere/contents/", false, x + 20, y + 215, 15, r->a(0xFCCC));
            r->drawString("4200000050324149 を確認してください", false, x + 20, y + 237, 15, r->a(0xFCCC));
        }), 260);
        if (g_sysmodule_version == 0)
            list->addItem(StartItem());
        frame->setContent(list);
        return frame;
    }
};

// Pad for button taps, -1 = the AI that is 1P
int g_tap_pad = -1;

class TapGui : public tsl::Gui {
public:
    void update() override { Heartbeat(); }

    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("ボタン単発入力", "仮想コントローラーで単発入力します");
        auto *list = new tsl::elm::List();
        if (g_config.dual) {
            Ppt2AiStatus status = {};
            ppt2aiGetStatus(&status);
            if (g_tap_pad < 0)
                g_tap_pad = status.ai_player[1] == 0 ? 1 : 0;
            auto *target = new tsl::elm::ListItem("入力先", TargetText(status));
            target->setClickListener([target, status](u64 keys) {
                if (!(keys & HidNpadButton_A))
                    return false;
                g_tap_pad ^= 1;
                target->setValue(TargetText(status));
                return true;
            });
            list->addItem(target);
        }
        static const std::pair<u64, const char *> buttons[] = {
            {HidNpadButton_A, "A"}, {HidNpadButton_B, "B"}, {HidNpadButton_X, "X"}, {HidNpadButton_Y, "Y"},
            {HidNpadButton_Up, "↑"}, {HidNpadButton_Down, "↓"}, {HidNpadButton_Left, "←"}, {HidNpadButton_Right, "→"},
            {HidNpadButton_L, "L"}, {HidNpadButton_R, "R"}, {HidNpadButton_ZL, "ZL"}, {HidNpadButton_ZR, "ZR"},
            {HidNpadButton_Plus, "＋"}, {HidNpadButton_Minus, "−"},
            {HidNpadButton_L | HidNpadButton_R, "L + R"},
        };
        for (auto &[mask, name] : buttons) {
            auto *item = new tsl::elm::ListItem(name);
            u64 m = mask;
            item->setClickListener([m](u64 keys) {
                if (keys & HidNpadButton_A) {
                    TapKeepOpen(m, 100, g_config.dual && g_tap_pad == 1 ? 1 : 0);
                    return true;
                }
                return false;
            });
            list->addItem(item);
        }
        frame->setContent(list);
        return frame;
    }

private:
    static std::string TargetText(const Ppt2AiStatus &status) {
        int pad = g_tap_pad == 1 ? 1 : 0;
        return std::string(pad ? "AI 2" : "AI 1") + "（" + SlotName(status.ai_player[pad]) + "）";
    }
};

class BoardGui : public tsl::Gui {
public:
    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("盤面", "1P/2P");
        frame->setContent(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *r, s32 x, s32 y, s32 w, s32 h) {
            constexpr s32 Cell = 14, Rows = 22;
            for (int p = 0; p < 2; p++) {
                s32 bx = x + 25 + p * (PPT2AI_BOARD_COLS * Cell + 40);
                s32 by = y + 40;
                r->drawRect(bx - 2, by - 2, PPT2AI_BOARD_COLS * Cell + 4, Rows * Cell + 4, r->a(0xF333));
                r->drawRect(bx, by, PPT2AI_BOARD_COLS * Cell, Rows * Cell, r->a(0xF000));
                if (!valid_[p] || !boards_[p].valid)
                    continue;
                for (int c = 0; c < PPT2AI_BOARD_COLS; c++)
                    for (int row = 0; row < Rows; row++) {
                        s8 v = boards_[p].cells[c][row];
                        if (v >= 0)
                            r->drawRect(bx + c * Cell, by + (Rows - 1 - row) * Cell, Cell - 1, Cell - 1, r->a(PieceColor(v)));
                    }
            }
            bool in_match = status_.phase == Ppt2AiPhase_StartAnim || status_.phase == Ppt2AiPhase_InMatch;
            if (in_match && status_.match_note >= Ppt2AiMatchNote_Replay) {
                r->drawString(StateName(status_), false, x + 25, y + 40 + 22 * Cell + 35, 17, r->a(0xFCCC));
            } else if (in_match) {
                for (int p = 0; p < 2; p++) {
                    if (!valid_[p] || !boards_[p].valid)
                        continue;
                    const Ppt2AiBoard &b = boards_[p];
                    s32 bx = x + 25 + p * (PPT2AI_BOARD_COLS * Cell + 40);
                    s32 ty = y + 40 + 22 * Cell + 30;
                    const char *ai = status_.ai_player[0] == p ? "(AI 1)" : status_.ai_player[1] == p ? "(AI 2)" : "";
                    char line[64];
                    snprintf(line, sizeof(line), "%dP%s  現在 %c", p + 1, ai, PieceChar(b.current));
                    r->drawString(line, false, bx, ty, 15, r->a(0xFFFF));
                    snprintf(line, sizeof(line), "ホールド %c", PieceChar(b.hold));
                    r->drawString(line, false, bx, ty + 22, 15, r->a(0xFFFF));
                    snprintf(line, sizeof(line), "NEXT %c%c%c%c%c", PieceChar(b.queue[0]), PieceChar(b.queue[1]),
                             PieceChar(b.queue[2]), PieceChar(b.queue[3]), PieceChar(b.queue[4]));
                    r->drawString(line, false, bx, ty + 44, 15, r->a(0xFFFF));
                }
            } else {
                r->drawString(PhaseName(status_.phase), false, x + 25, y + 40 + 22 * Cell + 35, 17, r->a(0xFCCC));
            }
        }));
        return frame;
    }

    void update() override {
        Heartbeat();
        if (tick_++ % 6)
            return;
        ppt2aiGetStatus(&status_);
        for (int p = 0; p < 2; p++)
            valid_[p] = R_SUCCEEDED(ppt2aiGetBoard(p, &boards_[p]));
    }

private:
    u32 tick_ = 0;
    Ppt2AiStatus status_ = {};
    Ppt2AiBoard boards_[2] = {};
    bool valid_[2] = {};
};

// List item names are cut off with "…" when too long, so names and values are kept short.

// One AI's character fields in g_config
struct CharacterChoice {
    u8 *character, *voice_change, *random, *puyo;
};

CharacterChoice GetCharacter(int ai) {
    Ppt2AiConfig &c = g_config;
    if (ai == 1)
        return {&c.character2, &c.voice_change2, &c.random_character2, &c.puyo2};
    return {&c.character, &c.voice_change, &c.random_character, &c.puyo};
}

static_assert(CharacterCount == PPT2AI_CHARACTERS, "character names");

// Favorites: 2 bits per character, bit 0 = normal voice, bit 1 = alternate voice
int FavoriteState(int character) {
    int bit = 2 * character;
    return (g_config.favorites[bit / 8] >> (bit % 8)) & 3;
}

void SetFavoriteState(int character, int state) {
    int bit = 2 * character;
    u8 &byte = g_config.favorites[bit / 8];
    byte = (byte & ~(3 << (bit % 8))) | (state << (bit % 8));
}

const char *FavoriteText(int state) {
    static const char *const texts[] = {"", "通常", "追加ボイス", "両方"};
    return texts[state & 3];
}

const char *VoiceText(bool voice_change) {
    return voice_change ? "追加ボイス" : "通常";
}

std::pair<std::string, std::string> CharacterLabel(int ai) {
    CharacterChoice c = GetCharacter(ai);
    if (*c.random)
        return {"ランダム", "お気に入り"};
    return {CharacterName(*c.character), VoiceText(*c.voice_change)};
}

// A list item that opens another Gui.
template <class G, class... Args> tsl::elm::ListItem *MenuItem(const char *title, Args... args) {
    auto *item = new tsl::elm::ListItem(title, "＞");
    item->setClickListener([args...](u64 keys) {
        if (!(keys & HidNpadButton_A))
            return false;
        tsl::changeTo<G>(args...);
        return true;
    });
    return item;
}

// Favorites, shared by both AIs. Random picks one of them at every character select.
class FavoritesGui : public tsl::Gui {
public:
    void update() override { Heartbeat(); }

    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("お気に入りのキャラ", "AI1 / AI2 共通");
        auto *list = new tsl::elm::List();
        list->addItem(new tsl::elm::CategoryHeader("通常 / 追加ボイス / 両方 / なし"));
        for (int i = 0; i < CharacterCount; i++) {
            auto *item = new tsl::elm::ListItem(CharacterNames[i], FavoriteText(FavoriteState(i)));
            item->setClickListener([i, item](u64 keys) {
                if (!(keys & HidNpadButton_A))
                    return false;
                int state = (FavoriteState(i) + 1) & 3;
                SetFavoriteState(i, state);
                item->setValue(FavoriteText(state));
                PushConfig();
                return true;
            });
            list->addItem(item);
        }
        frame->setContent(list);
        return frame;
    }
};

// Character and voice for one AI. A on the selected character switches the voice.
class CharacterGui : public tsl::Gui {
public:
    explicit CharacterGui(int ai) : ai_(ai) {}

    void update() override { Heartbeat(); }

    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("使うキャラ", "自動選択キャラとボイス");
        auto *list = new tsl::elm::List();
        CharacterChoice choice = GetCharacter(ai_);
        list->addItem(new tsl::elm::CategoryHeader("お気に入りからランダム"));
        random_ = new tsl::elm::ListItem("ランダム");
        random_->setClickListener([this, choice](u64 keys) {
            if (!(keys & HidNpadButton_A))
                return false;
            *choice.random = 1;
            PushConfig();
            UpdateValues();
            return true;
        });
        list->addItem(random_);
        list->addItem(MenuItem<FavoritesGui>("お気に入りを選ぶ"));
        list->addItem(new tsl::elm::CategoryHeader("キャラを選択（通常 / 追加ボイス）"));
        for (int i = 0; i < CharacterCount; i++) {
            auto *item = new tsl::elm::ListItem(CharacterNames[i]);
            item->setClickListener([this, i, choice](u64 keys) {
                if (!(keys & HidNpadButton_A))
                    return false;
                if (!*choice.random && *choice.character == i) {
                    *choice.voice_change = !*choice.voice_change;
                } else {
                    *choice.character = i;
                    *choice.voice_change = 0;
                    *choice.random = 0;
                }
                PushConfig();
                UpdateValues();
                return true;
            });
            list->addItem(item);
            items_.push_back(item);
        }
        UpdateValues();
        frame->setContent(list);
        return frame;
    }

private:
    void UpdateValues() {
        CharacterChoice c = GetCharacter(ai_);
        random_->setValue(*c.random ? "選択中" : "");
        for (int i = 0; i < (int)items_.size(); i++)
            items_[i]->setValue(!*c.random && i == *c.character ? VoiceText(*c.voice_change) : "");
    }

    int ai_;
    tsl::elm::ListItem *random_ = nullptr;
    std::vector<tsl::elm::ListItem *> items_;
};

// A setting with a fixed list of choices
struct Setting {
    const char *title;
    const char *description;
    std::vector<std::pair<int, const char *>> choices; // value, label
    std::function<int()> get;
    std::function<void(int)> set;

    std::string Label(int value) const {
        for (auto &[v, label] : choices)
            if (v == value)
                return label;
        return std::to_string(value); // edited in config.ini
    }

    // Label without the note in （）
    std::string ShortLabel(int value) const {
        std::string label = Label(value);
        size_t note = label.find("（");
        return note == std::string::npos ? label : label.substr(0, note);
    }
};

// One AI's Tetris strategy fields in g_config
struct Strategy {
    u8 *style, *hold_allowed, *c4w, *all_spins, *tsd_only, *perfect_clear, *pc_enhance, *pc_opener, *pc_midgame;
};

Strategy GetStrategy(int ai) {
    Ppt2AiConfig &c = g_config;
    if (ai == 1)
        return {&c.style2,         &c.hold_allowed2, &c.c4w2,      &c.all_spins2, &c.tsd_only2,
                &c.perfect_clear2, &c.pc_enhance2,   &c.pc_opener2, &c.pc_midgame2};
    return {&c.style,         &c.hold_allowed, &c.c4w,      &c.all_spins, &c.tsd_only,
            &c.perfect_clear, &c.pc_enhance,   &c.pc_opener, &c.pc_midgame};
}

// Presets, named as in PC Zetris
const Setting &StyleSetting(int ai) {
    static const auto make = [](int ai) {
        u8 *style = GetStrategy(ai).style;
        return Setting{"プリセット", "立ち回りのプリセット",
                       {{Ppt2AiStyle_TSpinB2B, "T-Spin+ B2B（標準）"}, {Ppt2AiStyle_TSpin, "T-Spin+"}, {Ppt2AiStyle_TST, "TST"},
                        {Ppt2AiStyle_Ultra, "Ultra"}, {Ppt2AiStyle_20TSD, "20 TSD"},
                        {Ppt2AiStyle_TSpinTuned, "T-Spin+ 調整版"}},
                       [style] { return (int)*style; }, [style](int v) { *style = v; }};
    };
    static const Setting settings[2] = {make(0), make(1)};
    return settings[ai == 1 ? 1 : 0];
}

const Setting &PcMidgameSetting(int ai) {
    static const auto make = [](int ai) {
        u8 *midgame = GetStrategy(ai).pc_midgame;
        return Setting{"試合中のパフェ", "積みが 4 段以下で、おじゃまが来ていないとき",
                       {{0, "狙わない"}, {1, "NEXT 5 個まで"}, {2, "NEXT 10 個まで"}},
                       [midgame] { return (int)*midgame; }, [midgame](int v) { *midgame = v; }};
    };
    static const Setting settings[2] = {make(0), make(1)};
    return settings[ai == 1 ? 1 : 0];
}

// The label depends on all-spin, as in PC Zetris
const char *TsdOnlyText(int ai) {
    return *GetStrategy(ai).all_spins ? "All-Spin Single のみ" : "TSDのみ (TSD20発用)";
}

const std::vector<Setting> &SearchSettings() {
    static const std::vector<Setting> settings = {
        {"探索の幅", "広いほど候補は増えるが、先読みは浅くなる",
         {{30, "30"}, {50, "50"}, {70, "70"}, {100, "100（標準）"}, {120, "120"}, {150, "150（最大）"}},
         [] { return (int)g_config.search_width; }, [](int v) { g_config.search_width = v; }},
        {"2体モードの探索の幅", "2 体ともこの幅で探索する",
         {{30, "30"}, {50, "50（標準）"}, {70, "70（最大）"}},
         [] { return (int)g_config.dual_search_width; }, [](int v) { g_config.dual_search_width = v; }},
        {"NEXT の数", "AI が先読みに使うミノの数",
         {{5, "5"}, {7, "7"}, {10, "10"}, {14, "14"}, {19, "19（最大）"}},
         [] { return (int)g_config.previews; }, [](int v) { g_config.previews = v; }},
        {"最低思考時間", "新しいミノが出てから考える最低限の時間",
         {{0, "なし"}, {100, "100ms"}, {200, "200ms"}, {300, "300ms"}, {500, "500ms"}, {800, "800ms"}},
         [] { return (int)g_config.min_think_ms; }, [](int v) { g_config.min_think_ms = v; }},
    };
    return settings;
}

class OptionGui : public tsl::Gui {
public:
    explicit OptionGui(const Setting *setting) : setting_(setting) {}

    void update() override { Heartbeat(); }

    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame(setting_->title, setting_->description);
        auto *list = new tsl::elm::List();
        int current = setting_->get();
        for (auto &[value, label] : setting_->choices) {
            auto *item = new tsl::elm::ListItem(label, value == current ? "選択中" : "");
            int v = value;
            const Setting *setting = setting_;
            item->setClickListener([v, setting](u64 keys) {
                if (!(keys & HidNpadButton_A))
                    return false;
                setting->set(v);
                PushConfig();
                tsl::goBack();
                return true;
            });
            list->addItem(item);
        }
        frame->setContent(list);
        return frame;
    }

private:
    const Setting *setting_;
};

// A page whose values are changed on sub-pages; update() refreshes them.
class SettingsGui : public tsl::Gui {
public:
    void update() override {
        Heartbeat();
        for (auto &ci : character_items_) {
            auto label = CharacterLabel(ci.ai);
            if (label != ci.shown) {
                ci.item->setText(label.first);
                ci.item->setValue(label.second);
                ci.shown = label;
            }
        }
        for (auto &si : setting_items_) {
            int v = si.setting->get();
            if (v != si.shown) {
                si.item->setValue(si.setting->ShortLabel(v));
                si.shown = v;
            }
        }
    }

protected:
    void AddSettingItem(tsl::elm::List *list, const Setting *setting) {
        auto *item = new tsl::elm::ListItem(setting->title, setting->ShortLabel(setting->get()));
        item->setClickListener([setting](u64 keys) {
            if (!(keys & HidNpadButton_A))
                return false;
            tsl::changeTo<OptionGui>(setting);
            return true;
        });
        list->addItem(item);
        setting_items_.push_back({item, setting, setting->get()});
    }

    void AddCharacterItem(tsl::elm::List *list, int ai) {
        auto label = CharacterLabel(ai);
        auto *item = new tsl::elm::ListItem(label.first, label.second);
        item->setClickListener([ai](u64 keys) {
            if (!(keys & HidNpadButton_A))
                return false;
            tsl::changeTo<CharacterGui>(ai);
            return true;
        });
        list->addItem(item);
        character_items_.push_back({item, ai, label});
    }

    void AddPerfectClearItems(tsl::elm::List *list, int ai) {
        Strategy s = GetStrategy(ai);
        auto *pc = new tsl::elm::ToggleListItem("パフェ発見機", *s.perfect_clear, "オン", "オフ");
        pc->setStateChangedListener([s](bool on) { *s.perfect_clear = on; PushConfig(); });
        list->addItem(pc);
        auto *pc_enhance = new tsl::elm::ToggleListItem("火力優先のルート", *s.pc_enhance, "オン", "オフ");
        pc_enhance->setStateChangedListener([s](bool on) { *s.pc_enhance = on; PushConfig(); });
        list->addItem(pc_enhance);
        auto *pc_opener = new tsl::elm::ToggleListItem("開幕パフェを狙う", *s.pc_opener, "オン", "オフ");
        pc_opener->setStateChangedListener([s](bool on) { *s.pc_opener = on; PushConfig(); });
        list->addItem(pc_opener);
        AddSettingItem(list, &PcMidgameSetting(ai));
    }

    // Same labels and order as PC Zetris
    void AddStrategyItems(tsl::elm::List *list, int ai) {
        Strategy s = GetStrategy(ai);
        AddSettingItem(list, &StyleSetting(ai));
        auto *hold = new tsl::elm::ToggleListItem("ホールド使用", *s.hold_allowed, "オン", "オフ");
        hold->setStateChangedListener([s](bool on) { *s.hold_allowed = on; PushConfig(); });
        list->addItem(hold);
        auto *c4w = new tsl::elm::ToggleListItem("中開けREN", *s.c4w, "オン", "オフ");
        c4w->setStateChangedListener([s](bool on) { *s.c4w = on; PushConfig(); });
        list->addItem(c4w);
        auto *tsd_only = new tsl::elm::ToggleListItem(TsdOnlyText(ai), *s.tsd_only, "オン", "オフ");
        auto *all_spins = new tsl::elm::ToggleListItem("特殊回転テトリス", *s.all_spins, "オン", "オフ");
        all_spins->setStateChangedListener([s, ai, tsd_only](bool on) {
            *s.all_spins = on;
            tsd_only->setText(TsdOnlyText(ai));
            PushConfig();
        });
        list->addItem(all_spins);
        tsd_only->setStateChangedListener([s](bool on) { *s.tsd_only = on; PushConfig(); });
        list->addItem(tsd_only);
    }

private:
    struct SettingItem {
        tsl::elm::ListItem *item;
        const Setting *setting;
        int shown;
    };
    struct CharacterItem {
        tsl::elm::ListItem *item;
        int ai;
        std::pair<std::string, std::string> shown;
    };
    std::vector<SettingItem> setting_items_;
    std::vector<CharacterItem> character_items_;
};

// Settings of one AI. The page is rebuilt when Tetris / Puyo Puyo is switched.
class AiGui : public SettingsGui {
public:
    explicit AiGui(int ai) : ai_(ai) {}

    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame(ai_ ? "AI 2 の設定" : "AI 1 の設定",
                                                 ai_ ? "2体モードでもう一方を操作する AI" : "AI 1");
        auto *list = new tsl::elm::List();
        list->addItem(new tsl::elm::CategoryHeader("キャラ自動選択（キャラとボイス）"));
        AddCharacterItem(list, ai_);
        CharacterChoice c = GetCharacter(ai_);
        list->addItem(new tsl::elm::CategoryHeader("スタイル"));
        auto *game = new tsl::elm::ListItem("選ぶスタイル", *c.puyo ? "ぷよぷよ" : "テトリス");
        game->setClickListener([this, c](u64 keys) {
            if (!(keys & HidNpadButton_A))
                return false;
            *c.puyo = !*c.puyo;
            PushConfig();
            rebuild_ = true; // not inside the listener
            return true;
        });
        list->addItem(game);
        if (*c.puyo) {
            list->addItem(new tsl::elm::CategoryHeader("ぷよぷよ: 設定なし"));
        } else {
            list->addItem(new tsl::elm::CategoryHeader("テトリスの立ち回り"));
            AddStrategyItems(list, ai_);
            list->addItem(new tsl::elm::CategoryHeader("パフェ（全消し）"));
            AddPerfectClearItems(list, ai_);
        }
        frame->setContent(list);
        return frame;
    }

    void update() override {
        if (rebuild_) {
            rebuild_ = false;
            int ai = ai_;
            tsl::goBack(); // deletes this page
            tsl::changeTo<AiGui>(ai);
            return;
        }
        SettingsGui::update();
    }

private:
    int ai_;
    bool rebuild_ = false;
};

class ControllerGui : public tsl::Gui {
public:
    void update() override { Heartbeat(); }

    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("コントローラー", "AI が使う仮想コントローラー");
        auto *list = new tsl::elm::List();
        auto *join = new tsl::elm::ToggleListItem("持ちかた画面で自動参加", g_config.applet_auto_join, "オン", "オフ");
        join->setStateChangedListener([](bool on) { g_config.applet_auto_join = on; PushConfig(); });
        list->addItem(join);

        auto *lr = new tsl::elm::ListItem("今すぐ L+R を押す");
        lr->setClickListener([](u64 keys) {
            if (!(keys & HidNpadButton_A))
                return false;
            JoinNow();
            return true;
        });
        list->addItem(lr);

        auto *swap = new tsl::elm::ListItem("1P と 2P を入れ替える");
        swap->setClickListener([](u64 keys) {
            if (!(keys & HidNpadButton_A))
                return false;
            ppt2aiSwapControllers(0, 1);
            return true;
        });
        list->addItem(swap);

        auto *reconnect = new tsl::elm::ListItem("接続し直す");
        reconnect->setClickListener([](u64 keys) {
            if (!(keys & HidNpadButton_A))
                return false;
            ppt2aiReconnectController();
            return true;
        });
        list->addItem(reconnect);

        list->addItem(MenuItem<TapGui>("ボタン単発入力"));
        frame->setContent(list);
        return frame;
    }
};

// Status lines. full adds the buttons, the pieces and the last move.
void DrawStatus(tsl::gfx::Renderer *r, s32 x, s32 y, const Ppt2AiStatus &s, bool full) {
    char line[128];
    if (!g_connected) {
        r->drawString(g_sysmodule_version ? "状態: バージョンが違います" : "状態: sysmoduleが動いていません",
                      false, x + 20, y + 25, 18, r->a(0xF44F));
        r->drawString(g_sysmodule_version ? "両方を同じビルドに入れ替えてください" : "起動すると自動でつながります", false,
                      x + 20, y + 50, 15, r->a(0xFCCC));
        return;
    }
    bool dual = s.dual;
    // the AIs wait while the overlay is open
    bool paused = s.bot_suspended && s.phase >= Ppt2AiPhase_CharSelect;
    if (s.net_online == 1)
        snprintf(line, sizeof(line), "状態: ネット対戦中（AI は停止）");
    else if (s.net_online == 3)
        snprintf(line, sizeof(line), "状態: sysmodule より前からのゲーム（AI は停止）");
    else if (s.net_online == 4)
        snprintf(line, sizeof(line), "状態: ネットの状態が読めない（AI は停止）");
    else if (s.net_online)
        snprintf(line, sizeof(line), "状態: ネット対戦のあと（AI は停止）");
    else
        snprintf(line, sizeof(line), "状態: %s%s%s", StateName(s), s.attach_paused ? "（切り離し中）" : "",
                 paused ? "（閉じると AI が動く）" : "");
    r->drawString(line, false, x + 20, y + 25, 18, r->a(0xFFFF));
    s32 ly = y + 50;
    auto add_line = [&](const char *text) {
        r->drawString(text, false, x + 20, ly, 15, r->a(0xFCCC));
        ly += 22;
    };
    if (s.net_online == 3)
        add_line("ぷよテト2 を終了して起動し直すと AI が動きます");
    else if (s.net_online == 4)
        add_line("読めるようになると AI が動きます");
    else if (s.net_online == 2)
        add_line("ゲームを終了すると AI がまた動きます");
    const char *pad_text;
    if (!dual)
        pad_text = s.controller_attached ? "接続中" : "未接続";
    else if (s.controller_attached && s.controller2_attached)
        pad_text = "2 台";
    else
        pad_text = s.controller_attached ? "AI 1 だけ" : s.controller2_attached ? "AI 2 だけ" : "未接続";
    snprintf(line, sizeof(line), "ゲーム %s / 仮想コントローラー %s", s.game_attached ? "接続中" : "未接続", pad_text);
    add_line(line);
    snprintf(line, sizeof(line), "コントローラー 1P: %s  2P: %s%s", OwnerName(s.slot_owner[0]), OwnerName(s.slot_owner[1]),
             s.remote_ai == 0 ? "  手元→AI 1" : s.remote_ai == 1 ? "  手元→AI 2" : "");
    add_line(line);
    if (s.phase == Ppt2AiPhase_CharSelect && !s.net_online) {
        snprintf(line, sizeof(line), "カーソル 1P: %s  2P: %s", CharacterName(s.charsel_index[0]),
                 CharacterName(s.charsel_index[1]));
        add_line(line);
    }
    if (!full)
        return;
    std::string sent = dual ? "送信中 AI 1: " + ButtonsText(s.buttons) + "  AI 2: " + ButtonsText(s.buttons2)
                            : "送信中: " + ButtonsText(s.buttons);
    add_line(sent.c_str());
    if (s.phase != Ppt2AiPhase_InMatch && s.phase != Ppt2AiPhase_StartAnim)
        return;
    snprintf(line, sizeof(line), "フレーム %lu  現在 %c  ホールド %c", (unsigned long)s.frame, PieceChar(s.current),
             PieceChar(s.hold));
    add_line(line);
    if (dual) {
        snprintf(line, sizeof(line), "AI 1 の直前の手: 思考 %ums、%u 手先まで", s.last_think_ms, s.last_depth);
        add_line(line);
        snprintf(line, sizeof(line), "AI 2 の直前の手: 思考 %ums、%u 手先まで", s.last_think_ms2, s.last_depth2);
    } else {
        snprintf(line, sizeof(line), "直前の手: 思考 %ums、%u 手先まで", s.last_think_ms, s.last_depth);
    }
    add_line(line);
}

class StatusGui : public tsl::Gui {
public:
    tsl::elm::Element *createUI() override {
        ppt2aiGetStatus(&status_);
        auto *frame = new tsl::elm::OverlayFrame("状態の詳細", "送信中のボタン、試合中のミノ、直前の手");
        frame->setContent(new tsl::elm::CustomDrawer(
            [this](tsl::gfx::Renderer *r, s32 x, s32 y, s32 w, s32 h) { DrawStatus(r, x, y, status_, true); }));
        return frame;
    }

    void update() override {
        Heartbeat();
        if (tick_++ % 10 == 0)
            ppt2aiGetStatus(&status_);
    }

private:
    u32 tick_ = 0;
    Ppt2AiStatus status_ = {};
};

class AdvancedGui : public SettingsGui {
public:
    tsl::elm::Element *createUI() override {
        auto *frame = new tsl::elm::OverlayFrame("詳細設定", "詳細な設定");
        auto *list = new tsl::elm::List();
        list->addItem(MenuItem<StatusGui>("状態の詳細"));

        list->addItem(new tsl::elm::CategoryHeader("テトリスAIの思考"));
        for (const Setting &setting : SearchSettings())
            AddSettingItem(list, &setting);

#if !PPT2AI_RELEASE
        list->addItem(new tsl::elm::CategoryHeader("実験"));
        // depth (left 4 digits) and think time in ms (right 4 digits) in the AI's score
        auto *score = new tsl::elm::ToggleListItem("スコア欄に深さと時間", g_config.score_stats, "オン", "オフ");
        score->setStateChangedListener([](bool on) { g_config.score_stats = on; PushConfig(); });
        list->addItem(score);

        list->addItem(new tsl::elm::CategoryHeader("デバッグ"));
        auto *trace = new tsl::elm::ToggleListItem("Trace inputs", g_config.trace_inputs, "On", "Off");
        trace->setStateChangedListener([](bool on) { g_config.trace_inputs = on; PushConfig(); });
        list->addItem(trace);
        // detach the debugger (not saved)
        Ppt2AiStatus status = {};
        ppt2aiGetStatus(&status);
        auto *detach = new tsl::elm::ToggleListItem("Detach from game", status.attach_paused, "On", "Off");
        detach->setStateChangedListener([](bool on) { ppt2aiSetAttachPaused(on); });
        list->addItem(detach);
#endif

        frame->setContent(list);
        return frame;
    }
};

class MainGui : public tsl::Gui {
public:
    tsl::elm::Element *createUI() override {
        ppt2aiGetStatus(&status_);
        auto *frame = new tsl::elm::OverlayFrame("PPT2NX-AI", Version);
        auto *list = new tsl::elm::List();

        list->addItem(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer *r, s32 x, s32 y, s32 w, s32 h) {
            DrawStatus(r, x, y, status_, false);
        }), 112);

        list->addItem(new tsl::elm::CategoryHeader("AI"));
        auto *enabled = new tsl::elm::ToggleListItem("AIを有効にする", g_config.enabled, "オン", "オフ");
        enabled->setStateChangedListener([](bool on) { g_config.enabled = on; PushConfig(); });
        list->addItem(enabled);

        auto *player = new tsl::elm::ListItem("操作するプレイヤー", g_config.player ? "2P" : "1P");
        player->setClickListener([player](u64 keys) {
            if (!(keys & HidNpadButton_A))
                return false;
            g_config.player ^= 1;
            player->setValue(g_config.player ? "2P" : "1P");
            PushConfig();
            return true;
        });
        list->addItem(player);

        auto *charsel = new tsl::elm::ToggleListItem("キャラ自動選択", g_config.auto_charsel, "オン", "オフ");
        charsel->setStateChangedListener([](bool on) { g_config.auto_charsel = on; PushConfig(); });
        list->addItem(charsel);
        list->addItem(MenuItem<AiGui>("AI 1 の設定", 0));

        list->addItem(new tsl::elm::CategoryHeader("AI 2体モード"));
        auto *dual = new tsl::elm::ToggleListItem("AI 2体モード", g_config.dual, "オン", "オフ");
        dual->setStateChangedListener([](bool on) { g_config.dual = on; PushConfig(); });
        list->addItem(dual);
        list->addItem(MenuItem<AiGui>("AI 2 の設定", 1));

        list->addItem(new tsl::elm::CategoryHeader("操作速度（遅いほど思考時間が増える）", true));
        auto *speed = new tsl::elm::NamedStepTrackBar(
            "速", {"10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%"});
        speed->setProgress(std::clamp(g_config.speed / 10 - 1, 0, 9));
        speed->setValueChangedListener([](u8 step) {
            g_config.speed = (step + 1) * 10;
            PushConfig();
        });
        list->addItem(speed);

        list->addItem(new tsl::elm::CategoryHeader("ZR→A ZL→B 右スティック→十字 (試合中以外)", true));
        auto *remote = new tsl::elm::ToggleListItem("仮想コントローラーを操作", g_config.remote, "オン", "オフ");
        remote->setStateChangedListener([](bool on) { g_config.remote = on; PushConfig(); });
        list->addItem(remote);

        list->addItem(new tsl::elm::CategoryHeader("その他"));
        list->addItem(MenuItem<FavoritesGui>("お気に入りのキャラ"));
        list->addItem(MenuItem<ControllerGui>("コントローラー"));
        list->addItem(MenuItem<BoardGui>("盤面を見る"));
        list->addItem(MenuItem<AdvancedGui>("詳細設定"));

        // press twice
        list->addItem(new tsl::elm::CategoryHeader("sysmodule", true));
        auto *quit = new tsl::elm::ListItem("sysmodule を終了", "");
        quit->setClickListener([quit](u64 keys) {
            static u64 first_press_ms = 0;
            if (!(keys & HidNpadButton_A) || !g_connected)
                return false;
            u64 now = NowMs();
            if (!first_press_ms || now - first_press_ms > 3000) {
                first_press_ms = now;
                quit->setValue("もう一度 A で終了");
                return true;
            }
            first_press_ms = 0;
            quit->setValue(R_SUCCEEDED(ppt2aiExitSysmodule()) ? "終了しました" : "失敗しました");
            return true;
        });
        list->addItem(quit);
        list->addItem(StartItem());

        frame->setContent(list);
        return frame;
    }

    void update() override {
        Heartbeat();
        if (tick_++ % 10 == 0)
            ppt2aiGetStatus(&status_);
    }

private:
    u32 tick_ = 0;
    Ppt2AiStatus status_ = {};
};

void ErrorGui::update() {
    Heartbeat();
    if (g_connected)
        tsl::changeTo<MainGui>();
}

class Ppt2AiOverlay : public tsl::Overlay {
public:
    void initServices() override {
        pmdmntInitialize();
        pmshellInitialize();
        Connect();
    }

    void exitServices() override {
        if (g_connected)
            ppt2aiSetOverlayVisible(false);
        ppt2aiExit();
        pmshellExit();
        pmdmntExit();
    }

    void onShow() override {
        g_hiding = false;
        if (g_connected)
            ppt2aiSetOverlayVisible(true);
    }

    void onHide() override {
        g_hiding = true;
        g_refocus_at_ms = 0; // Tesla gives focus back on hide
        if (g_connected)
            ppt2aiSetOverlayVisible(false);
    }

    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        if (!g_connected)
            return initially<ErrorGui>();
        return initially<MainGui>();
    }
};

} // namespace

int main(int argc, char **argv) {
    return tsl::loop<Ppt2AiOverlay>(argc, argv);
}
