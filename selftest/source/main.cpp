// ppt2-ai selftest
//
// Runs the lemon-tea bot on a synthetic 7-bag piece sequence, headless, and
// reports node count, search depth, wait time and memory use.
// Goal: prove that the bot builds with devkitA64/libnx, that std::thread works,
// and how fast it is on the real Switch CPU. It does NOT touch the game.
//
// Results are shown on screen and written to sdmc:/ppt2ai-selftest.txt.
#include <switch.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "bot/thread.h"

using namespace LemonTea;

namespace {

constexpr int kPieces = 40;        // pieces to play
constexpr int kThinkMs = 200;      // thinking time before asking for a move
constexpr int kSettleMs = 150;     // time for the bot thread to process advance()

FILE *g_log = nullptr;

void out(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    if (g_log) {
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fflush(g_log);
    }
    consoleUpdate(NULL);
}

// Small deterministic RNG so every run plays the same game.
uint32_t g_rng = 12345;
uint32_t Rnd() {
    g_rng = g_rng * 1664525u + 1013904223u;
    return g_rng >> 8;
}

std::vector<PieceType> g_bag;
int g_bag_i = 7;

PieceType NextPiece() {
    if (g_bag_i >= 7) {
        g_bag = {PIECE_I, PIECE_J, PIECE_L, PIECE_O, PIECE_S, PIECE_T, PIECE_Z};
        for (int i = 6; i > 0; --i)
            std::swap(g_bag[i], g_bag[Rnd() % (i + 1)]);
        g_bag_i = 0;
    }
    return g_bag[g_bag_i++];
}

u64 UsedMemoryKb() {
    u64 used = 0;
    if (R_FAILED(svcGetInfo(&used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0)))
        return 0;
    return used / 1024;
}

void RunSelfTest() {
    out("bot: lemon-tea (MIT)\n");
    out("pieces=%d think=%dms\n\n", kPieces, kThinkMs);
    u64 mem_before = UsedMemoryKb();

    Board board;
    std::vector<PieceType> queue;
    for (int i = 0; i < 5; ++i)
        queue.push_back(NextPiece());
    Bag bag; // fresh bag, as it was before the queue was drawn

    Thread thread;
    if (!thread.start(board, PIECE_NONE, queue, bag, 0, 0, {DEFAULT_HEURISTIC()})) {
        out("FAIL: Thread::start\n");
        return;
    }

    long long nodes = 0;
    long long depth = 0;
    double max_wait_ms = 0;
    int played = 0;
    auto t0 = std::chrono::steady_clock::now();

    for (int k = 0; k < kPieces; ++k) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kThinkMs));

        Plan plan;
        auto a = std::chrono::steady_clock::now();
        int tries = 0;
        bool got = false;
        while (!(got = thread.request(0, plan))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (++tries > 5000)
                break;
        }
        if (!got) {
            out("FAIL: no move after 5 s (piece %d)\n", k);
            break;
        }
        double wait_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
        if (wait_ms > max_wait_ms)
            max_wait_ms = wait_ms;
        nodes += plan.node;
        depth += plan.depth;
        ++played;

        // Feed the bot the pieces the placement consumed (2 on the first hold).
        std::vector<PieceType> fresh;
        int consumed = plan.result.next - plan.root.next;
        for (int c = 0; c < consumed; ++c)
            fresh.push_back(NextPiece());
        if (!thread.advance(plan.placement, fresh)) {
            out("FAIL: advance (piece %d)\n", k);
            break;
        }
        // advance() is asynchronous: give the bot thread time before request().
        std::this_thread::sleep_for(std::chrono::milliseconds(kSettleMs));

        if ((k + 1) % 10 == 0)
            out("  %d pieces: node=%d depth=%d\n", k + 1, plan.node, plan.depth);
    }

    double total_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    thread.stop();

    if (played > 0) {
        out("\nOK: %d pieces in %.1f s\n", played, total_s);
        out("avg node  %lld\n", nodes / played);
        out("avg depth %.1f\n", (double)depth / played);
        out("max request wait %.0f ms\n", max_wait_ms);
    }
    out("memory used: %llu -> %llu KB\n", (unsigned long long)mem_before, (unsigned long long)UsedMemoryKb());
}

} // namespace

int main(int, char **) {
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    g_log = fopen("sdmc:/ppt2ai-selftest.txt", "w");
    out("ppt2-ai selftest\n\n");

    RunSelfTest();

    out("\nPress + to exit\n");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus)
            break;
        consoleUpdate(NULL);
    }

    if (g_log)
        fclose(g_log);
    consoleExit(NULL);
    return 0;
}
