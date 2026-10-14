// Windows port, the 1988 interaction model (PlatformProfile::dos_interface,
// native/targets/windows/ORIGINAL_MODE_FIDELITY.md) -- the shared UiSession
// half, driven directly with a test profile whose dos_interface is a bool the
// test toggles. Every rule that differs is run in BOTH settings: the DOS model
// (Windows Original) and the default profile (the T-Deck, byte for byte).
//
//   L  the live prompt: UiSession::live_prompt_text() and the console's last
//      rows (visible_lines) carry the prompt and what has been typed, the
//      trailing space kept; nothing on the T-Deck, nothing while the console
//      is not ready or its layout is off, nothing outside a prompt
//   C  console_cursor(): Inline for every prompt mode in the DOS model
//   Y  Y/N reads only Y or N (Confirm / Cancel are read again); the answered
//      prompt keeps its row ("Save game? Yes", the guard's "No!", the troll's
//      key) -- the T-Deck takes Confirm / a mapped Cancel and keeps no row
//   T  text entry: ESC erases, Backspace / Delete / Left take one back, only
//      RETURN ends, capitals kept, the answer committed on its row -- the
//      T-Deck cancels on ESC and keeps case
//   D  getdir: ESC / RETURN / Backspace ignored, Space is "Pass" on the echo
//      row and then the request's cancel; Talk's direction on its echo row
//   S  roster / list pickers: arrows clamp, a digit only highlights (the Camp
//      guard's too), Space chooses, Z-stats' '0' is index -2, PgUp / PgDn move
//      seven rows -- the T-Deck wraps, ignores digits / Space / '0'
//   R  the camp / bed hours are one getkey
//   N  the typed spell name keeps its syllables on ESC (-1) and its rows
//   E  Yell's empty word is "Nothing"; RETURN / ESC at the command prompt are
//      "What?" -- since SPP2 (D-112) printed by the core, which runs the overworld's turn
//   I  spell_from_initials: a repeated initial counts once (CAST2 0x02b2) --
//      a pure function, the same in both settings
//
//   win_dos_interface_tests
#include "openu5/dialogue_orchestration.h"
#include "openu5/magic.h"
#include "openu5/platform_profile.h"
#include "openu5/spell_entry.h"
#include "openu5/ui_session.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}

// The process-global profile: installed once, its predicate reads this bool.
bool g_dos = false;
bool dos_predicate(void *) { return g_dos; }
void install_test_profile() {
    PlatformProfile p{};
    p.dos_interface = &dos_predicate;
    set_platform_profile(p);
}
const char *tag() { return g_dos ? "[DOS] " : "[T-Deck] "; }

UiAction ch(char16_t c) { UiAction a; a.kind = UiActionKind::Character; a.character = c; return a; }
UiAction kind(UiActionKind k) { UiAction a; a.kind = k; return a; }
UiAction dir(Direction d) { UiAction a; a.kind = UiActionKind::Direction; a.direction = d; return a; }
UiAction highlight(int32_t i) { UiAction a; a.kind = UiActionKind::HighlightIndex; a.index = i; return a; }

// Records every intent, copying the borrowed text at dispatch time.
struct Spy {
    struct Seen {
        UiIntent intent;
        std::u16string text;
    };
    std::vector<Seen> seen;
    static void send(void *p, const UiIntent &i) {
        Seen s{i, {}};
        const bool command = i.kind == UiIntentKind::Command;
        const char16_t *t = command ? i.command.text : i.value.text;
        const size_t n = command ? i.command.text_length : i.value.text_length;
        if (t && n) s.text.assign(t, n);
        s.intent.command.text = nullptr;
        s.intent.value.text = nullptr;
        static_cast<Spy *>(p)->seen.push_back(s);
    }
    size_t size() const { return seen.size(); }
    const Seen *back() const { return seen.empty() ? nullptr : &seen.back(); }
};

// A session with the console layout on and the console ready, as the runtime
// keeps it, and one line already in the transcript.
struct Fixture {
    UiTextBlock blocks[64]{};
    Spy spy;
    UiSession ui;
    Fixture() : ui{{blocks, 64}, {&spy, Spy::send}, {38, 40, 63}} {
        ui.set_console_layout(true);
        ui.set_console_ready(true);
        ui.append(UiTextChannel::Message, "Welcome");
    }
    bool press(const UiAction &a) { return ui.handle_input(a); }
    void type(const char *s) {
        for (; *s; ++s) press(ch(char16_t(*s)));
    }
    std::string live() const {
        char out[256];
        std::memset(out, 'x', sizeof(out));
        const size_t n = ui.live_prompt_text(out, sizeof(out));
        return n || out[0] == 0 ? std::string(out, n) : std::string("<unterminated>");
    }
    std::vector<std::string> rows() const {
        UiRenderedLine buf[64]{};
        const size_t n = ui.visible_lines(buf, 64, 38);
        std::vector<std::string> out;
        for (size_t i = 0; i < n; ++i) out.emplace_back(buf[i].text);
        return out;
    }
    std::string row_from_end(size_t k) const {
        const auto r = rows();
        return k < r.size() ? r[r.size() - 1 - k] : std::string("<none>");
    }
    std::string block_from_end(size_t k) const {
        const size_t n = ui.transcript_size();
        if (k >= n) return "<none>";
        const auto *b = ui.transcript_at(n - 1 - k);
        return b ? std::string(b->text) : std::string("<null>");
    }
    UiTextChannel channel_from_end(size_t k) const {
        const size_t n = ui.transcript_size();
        const auto *b = k < n ? ui.transcript_at(n - 1 - k) : nullptr;
        return b ? b->channel : UiTextChannel::Debug;
    }
    std::u16string input() const { return std::u16string(ui.input_buffer(), ui.input_length()); }
    size_t cursor() const {
        UiSelectionView v{};
        return ui.selection_view(v) ? v.cursor : size_t(999);
    }
    bool modal_response(UiRequestId r, bool accepted) const {
        const auto *b = spy.back();
        return b && b->intent.kind == UiIntentKind::ModalResponse && b->intent.request == r &&
               b->intent.value.accepted == accepted;
    }
    bool command(CommandKind k) const {
        const auto *b = spy.back();
        return b && b->intent.kind == UiIntentKind::Command && b->intent.command.kind == k;
    }
};

// Pickers' rows.
const char *kNames[] = {"Avatar", "Iolo", "Shamino"};
size_t three(void *) { return 3; }
UiSelectionItem three_item(void *, size_t i) { return {kNames[i % 3], true}; }
UiSelectionItem second_disabled(void *, size_t i) { return {kNames[i % 3], i != 1}; }
size_t twenty(void *) { return 20; }
UiSelectionItem twenty_item(void *, size_t) { return {"Item", true}; }
const UiSelectionSource kThree{nullptr, three, three_item};
const UiSelectionSource kSecondDisabled{nullptr, three, second_disabled};
const UiSelectionSource kTwenty{nullptr, twenty, twenty_item};

// ---------------------------------------------------------------------------
void test_live_prompt() {
    std::printf("\nL  the live prompt row\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        check(platform_dos_interface() == dos, t + "L0 platform_dos_interface() follows the profile's predicate");
        {
            Fixture f;
            check(f.live().empty(), t + "L1 no prompt is open: live_prompt_text() is empty");
            // The conversation's prompt, as the core raises it.
            DialogueOutput prompt;
            prompt.kind = DialogueOutputKind::Prompt;
            prompt.question = false;
            DialogueEvent de{DialogueEventKind::Output, &prompt};
            GameEvent ge;
            ge.kind = GameEventKind::Dialogue;
            ge.dialogue = &de;
            f.ui.consume(ge);
            check(f.ui.mode() == UiMode::TextEntry && f.ui.request() == UiRequestId::Dialogue,
                  t + "L2 a conversation prompt opens a text entry");
            f.type("job");
            if (dos) {
                check(std::strcmp(f.ui.prompt(), "Your interest?\n:") == 0 && f.live() == "Your interest?\n:JOB",
                      t + "L3 live_prompt_text() is the prompt then the typed word in capitals: [" + f.live() + "]");
                check(f.row_from_end(1) == "Your interest?" && f.row_from_end(0) == ":JOB",
                      t + "L4 the console's last two rows are \"Your interest?\" / \":JOB\": [" + f.row_from_end(1) +
                          "] [" + f.row_from_end(0) + "]");
                check(f.ui.console_cursor() == UiConsoleCursor::Inline, t + "L5 the cursor waits Inline after it");
                f.ui.set_console_ready(false);
                check(f.live().empty() && f.row_from_end(0) != ":JOB" &&
                          f.ui.console_cursor() == UiConsoleCursor::None,
                      t + "L6 console not ready: no live prompt, no prompt row, no cursor");
                f.ui.set_console_ready(true);
                f.ui.set_console_layout(false);
                check(f.live().empty(), t + "L7 console layout off: no live prompt");
                f.ui.set_console_layout(true);
                const size_t before = f.ui.transcript_size();
                f.press(kind(UiActionKind::Confirm));
                check(f.ui.transcript_size() == before + 1 && f.block_from_end(0) == "Your interest?\n:JOB" &&
                          f.channel_from_end(0) == UiTextChannel::CommandEcho && f.command(CommandKind::DialogueText) &&
                          f.spy.back()->text == u"JOB",
                      t + "L8 RETURN commits \"Your interest?\\n:JOB\" to the transcript and sends DialogueText JOB");
            } else {
                check(std::strcmp(f.ui.prompt(), "Your interest?") == 0 && f.live().empty() &&
                          f.input() == u"job",
                      t + "L3 the device prompt \"Your interest?\", nothing live, the word as typed (lower case)");
                check(f.row_from_end(0) == "Welcome" && f.ui.console_cursor() == UiConsoleCursor::None,
                      t + "L4 the console's last row is still \"Welcome\"; no cursor (the context bar reads it)");
                const size_t before = f.ui.transcript_size();
                f.press(kind(UiActionKind::Confirm));
                check(f.ui.transcript_size() == before && f.command(CommandKind::DialogueText) &&
                          f.spy.back()->text == u"job",
                      t + "L5 RETURN sends DialogueText as typed and commits no prompt row");
            }
        }
        {
            Fixture f;
            f.ui.begin_yes_no(UiRequestId::Custom, "Save game? ", false);
            if (dos)
                check(f.live() == "Save game? " && f.row_from_end(0) == "Save game? ",
                      t + "L9 a Y/N prompt's row keeps its trailing space (the cursor blinks after it): [" +
                          f.row_from_end(0) + "]");
            else
                check(f.live().empty() && f.row_from_end(0) == "Welcome", t + "L9 no Y/N prompt row on the console");
        }
    }
}

// ---------------------------------------------------------------------------
void test_console_cursor() {
    std::printf("\nC  console_cursor() by mode\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        const auto prompt_cursor = dos ? UiConsoleCursor::Inline : UiConsoleCursor::None;
        Fixture f;
        check(f.ui.console_cursor() == UiConsoleCursor::NewCommand, t + "C1 exploration: NewCommand");
        f.ui.begin_text(UiRequestId::Custom, "Name: ", 8);
        const bool text = f.ui.console_cursor() == prompt_cursor;
        f.ui.cancel_modal();
        f.ui.begin_number(UiRequestId::Custom, "How many? ", 0, 99, 2);
        const bool number = f.ui.console_cursor() == prompt_cursor;
        f.ui.cancel_modal();
        f.ui.begin_yes_no(UiRequestId::Custom, "Sure? ", false);
        const bool yes_no = f.ui.console_cursor() == prompt_cursor;
        f.ui.cancel_modal();
        f.ui.begin_selection(UiMode::PartySelection, UiRequestId::Party, "Player: ", kThree);
        const bool party = f.ui.console_cursor() == prompt_cursor;
        f.ui.cancel_modal();
        f.ui.begin_selection(UiMode::InventorySelection, UiRequestId::Inventory, "Item: ", kThree);
        const bool inventory = f.ui.console_cursor() == prompt_cursor;
        f.ui.cancel_modal();
        f.ui.begin_selection(UiMode::EquipmentSelection, UiRequestId::Equipment, "Item: ", kThree);
        const bool equipment = f.ui.console_cursor() == prompt_cursor;
        f.ui.cancel_modal();
        Command attack;
        attack.kind = CommandKind::CombatAttack;
        f.ui.begin_target(UiRequestId::Target, "Aim", attack, 5, 5);
        const bool aim = f.ui.console_cursor() == prompt_cursor;
        f.ui.cancel_modal();
        check(text && number && yes_no && party && inventory && equipment && aim,
              t + std::string("C2 text, number, Y/N, roster, item and equipment pickers and the arena aim: ") +
                  (dos ? "Inline" : "None") + " (" + std::to_string(text) + std::to_string(number) +
                  std::to_string(yes_no) + std::to_string(party) + std::to_string(inventory) +
                  std::to_string(equipment) + std::to_string(aim) + ")");
        f.press(ch('l'));
        check(f.ui.mode() == UiMode::TargetSelection && f.ui.console_cursor() == UiConsoleCursor::Inline,
              t + "C3 a world getdir (Look-): Inline in both");
        f.ui.cancel_modal();
    }
}

// ---------------------------------------------------------------------------
void test_yes_no() {
    std::printf("\nY  Y/N\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        Fixture f;
        f.ui.begin_yes_no(UiRequestId::Custom, "Save game? ", true);
        const size_t n0 = f.spy.size(), log0 = f.ui.transcript_size();
        f.press(kind(UiActionKind::Confirm));
        if (dos) {
            check(f.ui.mode() == UiMode::YesNo && f.spy.size() == n0, t + "Y1 RETURN is read again (no answer)");
            f.press(kind(UiActionKind::Cancel));
            f.press(kind(UiActionKind::Back));
            f.press(ch('x'));
            check(f.ui.mode() == UiMode::YesNo && f.spy.size() == n0,
                  t + "Y2 ESC (even with cancel_means_no), Backspace and 'x' are read again");
            f.press(ch('Y'));
            check(f.modal_response(UiRequestId::Custom, true) && f.spy.back()->intent.value.yes &&
                      f.ui.mode() == UiMode::Exploration,
                  t + "Y3 'Y' answers yes");
            check(f.ui.transcript_size() == log0 + 1 && f.block_from_end(0) == "Save game? Yes" &&
                      f.channel_from_end(0) == UiTextChannel::CommandEcho,
                  t + "Y4 the prompt keeps its row with the answer: [" + f.block_from_end(0) + "]");
            f.ui.begin_yes_no(UiRequestId::Custom, "Save game? ", false);
            f.press(ch('n'));
            check(f.modal_response(UiRequestId::Custom, true) && !f.spy.back()->intent.value.yes &&
                      f.block_from_end(0) == "Save game? No",
                  t + "Y5 'n' answers no: [" + f.block_from_end(0) + "]");
            f.ui.begin_yes_no(UiRequestId::GuardTribute, "\nDost thou pay?\n\n:", false);
            f.press(ch('n'));
            check(f.block_from_end(0) == "\nDost thou pay?\n\n:No!" && f.command(CommandKind::BlackthornAction) &&
                      f.spy.back()->intent.command.member == 0,
                  t + "Y6 the guard's tribute refused is \"No!\" (TALK 0x00c3)");
            f.ui.begin_yes_no(UiRequestId::TrollToll, "Dost thou pay?", false);
            f.press(ch('y'));
            check(f.block_from_end(0) == "Dost thou pay?Y", t + "Y7 the troll's toll echoes the key (MAINOUT 0x1b80): [" +
                                                                   f.block_from_end(0) + "]");
        } else {
            check(f.modal_response(UiRequestId::Custom, true) && f.spy.back()->intent.value.yes &&
                      f.ui.mode() == UiMode::Exploration && f.ui.transcript_size() == log0,
                  t + "Y1 RETURN answers yes; no prompt row is committed");
            f.ui.begin_yes_no(UiRequestId::Custom, "Sure?", true);
            f.press(kind(UiActionKind::Cancel));
            check(f.modal_response(UiRequestId::Custom, true) && !f.spy.back()->intent.value.yes,
                  t + "Y2 ESC is No where the prompt maps it (cancel_means_no)");
            f.ui.begin_yes_no(UiRequestId::Custom, "Sure?", false);
            const size_t n1 = f.spy.size();
            f.press(kind(UiActionKind::Cancel));
            check(f.ui.mode() == UiMode::YesNo && f.spy.size() == n1, t + "Y3 ESC is ignored where it is not mapped");
            f.press(ch('n'));
            check(f.modal_response(UiRequestId::Custom, true) && !f.spy.back()->intent.value.yes &&
                      f.ui.transcript_size() == log0,
                  t + "Y4 'n' answers no; still no prompt row");
        }
    }
}

// ---------------------------------------------------------------------------
void test_text_entry() {
    std::printf("\nT  text and number entry\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        Fixture f;
        f.ui.begin_text(UiRequestId::Custom, "Name: ", 8);
        const size_t n0 = f.spy.size();
        f.type("ab");
        if (dos) {
            check(f.input() == u"AB", t + "T1 letters are kept in capitals (getkey 0x2032)");
            f.press(kind(UiActionKind::Cancel));
            check(f.input().empty() && f.ui.mode() == UiMode::TextEntry && f.spy.size() == n0,
                  t + "T2 ESC erases the line and keeps reading (0x3b4e)");
            f.type("xyz");
            f.press(kind(UiActionKind::Back));
            const bool back = f.input() == u"XY";
            f.press(kind(UiActionKind::DeleteCharacter));
            const bool del = f.input() == u"X";
            f.press(dir(Direction::West));
            const bool left = f.input().empty();
            f.press(kind(UiActionKind::DeleteCharacter));
            check(back && del && left && f.ui.mode() == UiMode::TextEntry && f.spy.size() == n0,
                  t + "T3 Backspace, Delete and the left arrow take one back; on an empty line nothing ends it");
            f.type("ok");
            f.press(kind(UiActionKind::Confirm));
            check(f.modal_response(UiRequestId::Custom, true) && f.spy.back()->text == u"OK" &&
                      f.block_from_end(0) == "Name: OK" && f.channel_from_end(0) == UiTextChannel::CommandEcho,
                  t + "T4 only RETURN ends it; the row \"Name: OK\" stays: [" + f.block_from_end(0) + "]");
            f.ui.begin_number(UiRequestId::Custom, "How many? ", 0, 99, 2);
            f.type("42");
            const size_t n1 = f.spy.size();
            f.press(kind(UiActionKind::Cancel));
            check(f.input().empty() && f.ui.mode() == UiMode::NumericEntry && f.spy.size() == n1,
                  t + "T5 a number's ESC erases it too (input_number 0x3c0e)");
            f.type("7");
            f.press(kind(UiActionKind::Confirm));
            check(f.modal_response(UiRequestId::Custom, true) && f.spy.back()->intent.value.number == 7 &&
                      f.block_from_end(0) == "How many? 7",
                  t + "T6 the number's answer keeps its row: [" + f.block_from_end(0) + "]");
        } else {
            check(f.input() == u"ab", t + "T1 letters are kept as typed");
            f.press(kind(UiActionKind::Cancel));
            check(f.ui.mode() == UiMode::Exploration && f.modal_response(UiRequestId::Custom, false),
                  t + "T2 ESC cancels the entry (accepted == false)");
            f.ui.begin_text(UiRequestId::Custom, "Name: ", 8);
            f.press(kind(UiActionKind::DeleteCharacter));
            check(f.ui.mode() == UiMode::Exploration && f.modal_response(UiRequestId::Custom, false),
                  t + "T3 Delete on an empty line cancels too");
            f.ui.begin_number(UiRequestId::Custom, "How many? ", 0, 99, 2);
            f.type("4");
            f.press(kind(UiActionKind::Cancel));
            check(f.ui.mode() == UiMode::Exploration && f.modal_response(UiRequestId::Custom, false),
                  t + "T4 a number's ESC cancels");
        }
    }
}

// ---------------------------------------------------------------------------
void test_getdir() {
    std::printf("\nD  getdir\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        Fixture f;
        f.press(ch('l'));
        check(f.ui.mode() == UiMode::TargetSelection && f.ui.request() == UiRequestId::Direction &&
                  f.block_from_end(0) == "Look-",
              t + "D1 'l' opens the getdir after \"Look-\"");
        const size_t n0 = f.spy.size();
        if (dos) {
            f.press(kind(UiActionKind::Cancel));
            f.press(kind(UiActionKind::Confirm));
            f.press(kind(UiActionKind::Back));
            check(f.ui.mode() == UiMode::TargetSelection && f.spy.size() == n0 && f.block_from_end(0) == "Look-",
                  t + "D2 ESC, RETURN and Backspace are no answer (0x3600-0x3620)");
            f.press(ch(u' '));
            check(f.ui.mode() == UiMode::Exploration && f.block_from_end(0) == "Look-Pass" &&
                      f.spy.size() == n0 + 1 && f.modal_response(UiRequestId::Direction, false),
                  t + "D3 Space prints \"Pass\" on the echo row, then the request's cancel: [" + f.block_from_end(0) + "]");
            f.press(ch('t'));
            f.press(dir(Direction::North));
            check(f.block_from_end(0) == "Talk-North" && f.command(CommandKind::Talk) &&
                      f.spy.back()->intent.command.has_direction,
                  t + "D4 Talk's direction lands on its echo row (TALK 0x0422 -> 0x35EC): [" + f.block_from_end(0) + "]");
        } else {
            f.press(ch(u' '));
            check(f.ui.mode() == UiMode::TargetSelection && f.spy.size() == n0, t + "D2 Space is no key here");
            f.press(kind(UiActionKind::Cancel));
            check(f.ui.mode() == UiMode::Exploration && f.spy.size() == n0 + 1 &&
                      f.modal_response(UiRequestId::Direction, false) && f.block_from_end(0) == "Look-",
                  t + "D3 ESC cancels the getdir; no \"Pass\"");
            f.press(ch('t'));
            f.press(dir(Direction::North));
            check(f.block_from_end(1) == "Talk-" && f.block_from_end(0) == "north" && f.command(CommandKind::Talk),
                  t + "D4 Talk's direction is a row of its own: [" + f.block_from_end(1) + "] [" + f.block_from_end(0) + "]");
        }
    }
}

// ---------------------------------------------------------------------------
void test_selection() {
    std::printf("\nS  pickers\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        Fixture f;
        f.ui.begin_selection(UiMode::PartySelection, UiRequestId::Party, "Player: ", kThree);
        const size_t n0 = f.spy.size();
        f.press(kind(UiActionKind::Previous));
        const size_t after_previous = f.cursor();
        if (dos) {
            f.press(dir(Direction::North));
            check(after_previous == 0 && f.cursor() == 0, t + "S1 Previous / Up on the first row stay (clamp)");
            f.press(kind(UiActionKind::Next));
            f.press(kind(UiActionKind::Next));
            f.press(kind(UiActionKind::Next));
            f.press(dir(Direction::South));
            check(f.cursor() == 2, t + "S2 Next / Down stop on the last row (clamp, 0x3f14 / 0x3f54)");
            f.press(ch('1'));
            const bool digit = f.cursor() == 0 && f.ui.mode() == UiMode::PartySelection;
            f.press(highlight(2));
            const bool hl = f.cursor() == 2 && f.ui.mode() == UiMode::PartySelection;
            f.press(ch('9'));
            f.press(ch('a'));
            f.press(kind(UiActionKind::Back));
            check(digit && hl && f.cursor() == 2 && f.ui.mode() == UiMode::PartySelection && f.spy.size() == n0,
                  t + "S3 a digit and HighlightIndex only move the highlight; '9', letters and Backspace are dropped");
            f.press(ch(u' '));
            check(f.modal_response(UiRequestId::Party, true) && f.spy.back()->intent.value.index == 2 &&
                      f.ui.mode() == UiMode::Exploration,
                  t + "S4 Space chooses the highlighted row (0x2e62)");
            f.ui.begin_selection(UiMode::PartySelection, UiRequestId::Status, "Player: ", kThree);
            f.press(ch('0'));
            check(f.modal_response(UiRequestId::Status, true) && f.spy.back()->intent.value.index == -2,
                  t + "S5 Z-stats' '0' answers index -2 (the Equipment page, 0x2e67)");
            f.ui.begin_selection(UiMode::PartySelection, UiRequestId::Party, "Player: ", kSecondDisabled);
            f.press(highlight(1));
            const size_t n1 = f.spy.size();
            f.press(kind(UiActionKind::Confirm));
            const bool held = f.ui.mode() == UiMode::PartySelection && f.spy.size() == n1;
            f.press(kind(UiActionKind::Cancel));
            check(held && f.modal_response(UiRequestId::Party, false),
                  t + "S6 RETURN on a disabled row chooses nothing; ESC cancels");
            f.ui.begin_selection(UiMode::PartySelection, UiRequestId::CampGuard, "\nWho will stand guard? ", kThree);
            const size_t n2 = f.spy.size();
            f.press(ch('3'));
            const bool guard_held = f.cursor() == 2 && f.ui.mode() == UiMode::PartySelection && f.spy.size() == n2;
            f.press(kind(UiActionKind::Confirm));
            check(guard_held && f.command(CommandKind::Rest) && f.spy.back()->intent.command.member == 2 &&
                      f.spy.back()->intent.command.watch_requested &&
                      f.block_from_end(0) == "\nWho will stand guard? ",
                  t + "S7 the Camp guard's digit only highlights too; RETURN posts it, no name echoed");
            f.ui.begin_selection(UiMode::InventorySelection, UiRequestId::Inventory, "Item: ", kTwenty);
            f.press(kind(UiActionKind::PageDown));
            const size_t p1 = f.cursor();
            f.press(kind(UiActionKind::PageDown));
            f.press(kind(UiActionKind::PageDown));
            const size_t p3 = f.cursor();
            f.press(kind(UiActionKind::PageUp));
            check(p1 == 7 && p3 == 19 && f.cursor() == 12 && f.ui.scroll_offset_lines() == 0,
                  t + "S8 PgDn / PgUp move a list seven rows, clamped (ZSTATS 0x0f2e): " + std::to_string(p1) + " " +
                      std::to_string(p3) + " " + std::to_string(f.cursor()));
            f.ui.cancel_modal();
        } else {
            f.press(kind(UiActionKind::Next));
            check(after_previous == 2 && f.cursor() == 0, t + "S1 Previous wraps to the last row, Next back to the first");
            f.press(ch('2'));
            const bool digit_ignored = f.cursor() == 0;
            f.press(highlight(1));
            const bool hl = f.cursor() == 1;
            f.press(ch(u' '));
            check(digit_ignored && hl && f.ui.mode() == UiMode::PartySelection && f.spy.size() == n0,
                  t + "S2 a digit and Space are ignored; HighlightIndex moves the cursor");
            f.ui.cancel_modal();
            f.ui.begin_selection(UiMode::PartySelection, UiRequestId::Status, "Player: ", kThree);
            const size_t n1 = f.spy.size();
            f.press(ch('0'));
            check(f.ui.mode() == UiMode::PartySelection && f.spy.size() == n1, t + "S3 Z-stats' '0' is ignored");
            f.ui.cancel_modal();
            f.ui.begin_selection(UiMode::PartySelection, UiRequestId::CampGuard, "Who will stand guard?", kThree);
            f.press(ch('3'));
            check(f.command(CommandKind::Rest) && f.spy.back()->intent.command.member == 2 &&
                      f.ui.mode() == UiMode::Exploration,
                  t + "S4 the Camp guard's digit posts that member at once");
            f.ui.begin_selection(UiMode::InventorySelection, UiRequestId::Inventory, "Use item", kTwenty);
            f.press(kind(UiActionKind::PageDown));
            check(f.cursor() == 0, t + "S5 PgDn pages the transcript, not the list");
            f.ui.cancel_modal();
        }
    }
}

// ---------------------------------------------------------------------------
void test_rest_hours() {
    std::printf("\nR  the hours\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        Fixture f;
        f.ui.set_camp_prompt_context(true, false);
        f.press(ch('h'));
        check(f.ui.mode() == UiMode::NumericEntry && f.ui.request() == UiRequestId::RestHours &&
                  f.block_from_end(0) == "Hole up & camp!",
              t + "R1 'h' at a camp site asks the hours after \"Hole up & camp!\"");
        const size_t n0 = f.spy.size();
        f.press(ch('5'));
        if (dos) {
            check(std::strcmp(f.ui.prompt(), "") == 0 && f.ui.mode() == UiMode::Exploration &&
                      f.command(CommandKind::Rest) && f.spy.back()->intent.command.hours == 5 && f.spy.size() == n0 + 1,
                  t + "R2 one digit is the answer at once (kernel 0x3db0-0x3dd3)");
            check(f.block_from_end(0) == "\nFor how many hours? (1-9) 5",
                  t + "R3 the camp's prompt keeps its row with the digit");
            f.ui.set_camp_prompt_context(false, false);
            f.press(ch('h'));
            const bool bed_prompt = std::strcmp(f.ui.prompt(), "For how many hours? ") == 0 &&
                                    f.live() == "For how many hours? ";
            f.press(ch('3'));
            check(bed_prompt && f.command(CommandKind::Rest) && f.spy.back()->intent.command.hours == 3,
                  t + "R4 the bed's \"For how many hours? \" is one key too (CMDS 0x055a)");
            f.ui.set_camp_prompt_context(true, true);
            f.press(ch('h'));
            f.press(ch('4'));
            check(f.ui.mode() == UiMode::YesNo && f.ui.request() == UiRequestId::CampWatch &&
                      std::strcmp(f.ui.prompt(), "\nWilt thou set a watch? ") == 0,
                  t + "R5 with a watch to set, the digit goes on to \"Wilt thou set a watch? \"");
        } else {
            check(f.ui.mode() == UiMode::NumericEntry && f.spy.size() == n0 && f.input() == u"5",
                  t + "R2 a digit is typed into the number; nothing is sent yet");
            f.press(kind(UiActionKind::Confirm));
            check(f.command(CommandKind::Rest) && f.spy.back()->intent.command.hours == 5 &&
                      f.block_from_end(0) == "Hole up & camp!",
                  t + "R3 RETURN sends it; no prompt row is committed");
        }
    }
}

// ---------------------------------------------------------------------------
void test_spell_name() {
    std::printf("\nN  the typed spell name\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        Fixture f;
        f.ui.begin_text(UiRequestId::SpellName, "Spell name:\n:");
        f.type("il");
        const bool echo = f.input() == u"IN LOR ";
        if (dos)
            check(echo && f.row_from_end(1) == "Spell name:" && f.row_from_end(0) == ":IN LOR ",
                  t + "N1 the syllables are typed on the ':' row, trailing space kept: [" + f.row_from_end(0) + "]");
        else
            check(echo && f.live().empty() && f.row_from_end(0) == "Welcome", t + "N1 the echo, but no console row");
        const size_t log0 = f.ui.transcript_size();
        f.press(kind(UiActionKind::Cancel));
        const auto *b = f.spy.back();
        const bool accepted = f.modal_response(UiRequestId::SpellName, true);
        if (dos)
            check(accepted && b->intent.value.number == kSpellEntryNone && b->text == u"IN LOR " &&
                      f.ui.transcript_size() == log0,
                  t + "N2 ESC ends it as nothing typed (-1) but keeps the syllables for the row (0x01d8); "
                      "the owner prints the rows");
        else
            check(accepted && b->intent.value.number == 0 && b->text.empty() && f.ui.transcript_size() == log0,
                  t + "N2 ESC ends it with the echo cleared");
    }
}

// ---------------------------------------------------------------------------
void test_command_prompt() {
    std::printf("\nE  Yell and the command prompt\n");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        Fixture f;
        f.press(ch('y'));
        const size_t n0 = f.spy.size();
        if (dos) {
            check(f.ui.mode() == UiMode::TextEntry && f.ui.request() == UiRequestId::YellText &&
                      f.block_from_end(0) == "Yell what?" && std::strcmp(f.ui.prompt(), ":") == 0,
                  t + "E1 'y' echoes \"Yell what?\" and reads the word after ':' (CMDS 0x1458)");
            f.press(kind(UiActionKind::Confirm));
            check(f.ui.mode() == UiMode::Exploration && f.spy.size() == n0 && f.block_from_end(0) == ":Nothing" &&
                      f.channel_from_end(0) == UiTextChannel::CommandEcho,
                  t + "E2 an empty word prints \"Nothing\" on the ':' row and utters nothing (CMDS 0x1467)");
            f.press(ch('y'));
            f.type("ort");
            f.press(kind(UiActionKind::Confirm));
            check(f.command(CommandKind::Yell) && f.spy.back()->text == u"ORT" && f.block_from_end(0) == ":ORT",
                  t + "E3 a word is uttered in capitals, its row kept");
            // SPP2 (D-112): the core prints the "What?" and knows the context's turn (MAINOUT
            // 0x0bb8 -> 0x0c12 runs the world turn; TOWN 0x1544 -> 0x154b none), so the session sends
            // the control code itself (spp2_runtime G6 / G7 run it through the real runtime).
            const size_t n1 = f.spy.size();
            f.press(kind(UiActionKind::Confirm));
            const bool ret = f.command(CommandKind::ControlKey) && f.spy.back()->intent.command.item == 0x0d && f.spy.size() == n1 + 1;
            f.press(kind(UiActionKind::Cancel));
            check(ret && f.command(CommandKind::ControlKey) && f.spy.back()->intent.command.item == 0x1b && f.spy.size() == n1 + 2,
                  t + "E4 RETURN and ESC at the command prompt send the control codes 0x0d / 0x1b to the core (\"What?\", MAINOUT 0x0bb8)");
        } else {
            check(f.ui.mode() == UiMode::TextEntry && f.ui.request() == UiRequestId::YellText &&
                      f.block_from_end(0) == "Yell" && std::strcmp(f.ui.prompt(), "Yell what?") == 0,
                  t + "E1 'y' echoes \"Yell\" and asks \"Yell what?\"");
            f.press(kind(UiActionKind::Confirm));
            check(f.command(CommandKind::Yell) && f.spy.back()->text.empty() && f.block_from_end(0) == "Yell",
                  t + "E2 an empty word is still sent to the core; no \"Nothing\"");
            const size_t n1 = f.spy.size();
            f.press(kind(UiActionKind::Confirm));
            check(f.command(CommandKind::Pass) && f.spy.size() == n1 + 1 && f.block_from_end(0) == "Pass",
                  t + "E3 RETURN at the command prompt is Pass");
            const size_t log1 = f.ui.transcript_size();
            const bool routed = f.press(kind(UiActionKind::Cancel));
            check(!routed && f.ui.transcript_size() == log1 && f.spy.size() == n1 + 1,
                  t + "E4 ESC at the command prompt is not a command");
        }
    }
}

// ---------------------------------------------------------------------------
void test_spell_initials() {
    std::printf("\nI  spell_from_initials\n");
    int32_t in_lor = -1;
    for (int32_t s = 0; s < 48; ++s) {
        const auto *d = spell_definition(SpellId(s));
        if (d && d->name && std::strcmp(d->name, "In Lor") == 0) in_lor = s;
    }
    check(in_lor >= 0, "I0 In Lor is in the spell table");
    for (const bool dos : {true, false}) {
        g_dos = dos;
        const std::string t = tag();
        check(spell_from_initials("IL", 2) == in_lor && spell_from_initials("LI", 2) == in_lor,
              t + "I1 \"IL\" and \"LI\" are In Lor");
        check(spell_from_initials("IIL", 3) == in_lor && spell_from_initials("ILI", 3) == in_lor &&
                  spell_from_initials("iil", 3) == in_lor,
              t + "I2 a repeated initial counts once: \"IIL\" / \"ILI\" / \"iil\" are In Lor (CAST2 0x02b2-0x02cb)");
        check(spell_from_initials("II", 2) == kSpellEntryNoMatch && spell_from_initials("III", 3) == kSpellEntryNoMatch,
              t + "I3 \"II\" collapses to \"I\", which is no spell");
        check(spell_from_initials("", 0) == kSpellEntryNone, t + "I4 nothing typed is -1");
    }
}
} // namespace

int main() {
    install_test_profile();
    test_live_prompt();
    test_console_cursor();
    test_yes_no();
    test_text_entry();
    test_getdir();
    test_selection();
    test_rest_hours();
    test_spell_name();
    test_command_prompt();
    test_spell_initials();
    // The profile is process-global: leave the default (the T-Deck's) behind.
    g_dos = false;
    set_platform_profile(PlatformProfile{});
    check(!platform_dos_interface() && platform_profile().dos_interface == nullptr,
          "Z the default profile is restored (no dos_interface)");
    std::printf("win_dos_interface: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
