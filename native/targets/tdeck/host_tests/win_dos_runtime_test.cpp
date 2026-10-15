// Windows port, the 1988 interaction model (PlatformProfile::dos_interface,
// native/targets/windows/ORIGINAL_MODE_FIDELITY.md) on the REAL AlphaRuntime
// and the REAL tdeck_board.cpp over the fake ST7789 (the A4-UI2 harness), the
// input sent the way the Windows front end sends it: UiActions through
// AlphaRuntime::handle_action, never raw T-Deck keys. Read off the PIXELS.
//
//   W  a DOS profile (dos_interface, typed spell names and (Q)uit & Save on, as
//      the Windows Original profile has them), a party of three with no active
//      member, (C)ast: the kernel 0x4988 "Player: " picker
//      W1 opens on the roster: no selector panel (its rules at y=23 / y=43 are
//         not drawn), no context bar (no rule at y=215)
//      W2 roster row 0 is in reverse video, the only one (select_party_member
//         0x2d7a starts on member 1, 0x2dc1's 0x2a28 XOR)
//      W3 the status caption row reads "Select:" (0x2db2), in place of the
//         location caption
//      W4 "Player: " is the console's last row, the cursor after it
//      W5 HighlightIndex(1) (a digit on the desktop) moves the reverse row to
//         row 1, choosing nothing
//      W6 RETURN: "Player: Iolo" on the console, then "Spell name:" / ":" as the
//         last two rows with the cursor drawn after ':'; still no context bar;
//         the roster and the caption are back as they were
//   T  the T-Deck control in the same binary, the default profile, the same
//      UiActions: the selector panel (rules at y=23 / y=43) with its context
//      bar (y=215), no roster highlight, the list's bar following
//      HighlightIndex, and RETURN goes on to the spell LIST
//
//   win_dos_runtime <openu5-alpha1-resources.bin> [--dump <dir>]
#include "a4_ui2_harness.h"
#include "openu5/hud.h"
#include "openu5/platform_profile.h"

using namespace a4_ui2;

namespace {
constexpr uint16_t kCyan = 0x07ff;
constexpr int kLeft = openu5::kHudRightX;                        // 184
constexpr int kRight = openu5::kHudRightX + openu5::kHudRightW - 2; // 317: x=318 is the box's right rule
constexpr int kWidth = kRight - kLeft + 1;                       // 134

// The Windows profile's 1988 switches, on a bool the test sets.
bool g_dos = false;
struct DosProfile {
    DosProfile() {
        g_dos = true;
        PlatformProfile p{};
        p.back_key = "Esc";
        p.move_keys = "Arrows";
        p.menu_key = "F10";
        p.typed_spell_names = [](void *) { return g_dos; };
        p.quit_save_key = [](void *) { return g_dos; };
        p.dos_interface = [](void *) { return g_dos; };
        // The DOS tables the Windows profile reads from the pack (dos_text.cpp);
        // here only the class glyph, RUNES.CH 0x01 for every item. No names:
        // the lists then keep the device's own, which the checks do not read.
        p.dos_text = [](void *, uint8_t table, int) -> const char * {
            return table == uint8_t(DosText::EquipmentGlyph) ? "\x01" : nullptr;
        };
        set_platform_profile(p);
    }
    ~DosProfile() {
        g_dos = false;
        set_platform_profile(PlatformProfile{});
    }
    DosProfile(const DosProfile &) = delete;
    DosProfile &operator=(const DosProfile &) = delete;
};

UiAction ch(char16_t c) { UiAction a; a.kind = UiActionKind::Character; a.character = c; return a; }
UiAction kind(UiActionKind k) { UiAction a; a.kind = k; return a; }
UiAction highlight(int32_t i) { UiAction a; a.kind = UiActionKind::HighlightIndex; a.index = i; return a; }

/** One desktop input: handle_action, as native/targets/windows routes a key, then 100 ms of frames. */
bool act(Run &h, const UiAction &a) {
    openu5_host_virtual_clock_us() += 100000;
    const bool routed = h.rt->handle_action(a, tdeck::DeviceShortcut::None, Run::now(), "win_dos_runtime");
    h.run(100);
    return routed;
}

// --- the panel ----------------------------------------------------------------
/** White pixels of row y across the right panel's interior (x 184..317). */
int white_in_row(int y) {
    int k = 0;
    for (int x = kLeft; x <= kRight; ++x) k += px(x, y) == kWhite;
    return k;
}
/** A white rule spans row y (the selector's fill_rect(182, y, 137, 1), the context bar's at 215). */
bool rule(int y) { return white_in_row(y) == kWidth; }
/** Roster row `row` (y 4+8*row .. +7) is in reverse video: mostly lit, the glyphs punched out. */
bool inverted(int row) {
    int black = 0, total = 0;
    for (int y = 4 + row * 8; y < 12 + row * 8; ++y)
        for (int x = kLeft; x <= kRight; ++x, ++total) black += px(x, y) == kBlack;
    return black * 2 < total;
}
int inverted_rows() {
    int k = 0;
    for (int r = 0; r < 6; ++r) k += inverted(r);
    return k;
}
uint64_t region_hash(int x0, int y0, int w, int hgt) {
    uint64_t v = 1469598103934665603ull;
    for (int y = y0; y < y0 + hgt; ++y)
        for (int x = x0; x < x0 + w; ++x) v = (v ^ px(x, y)) * 1099511628211ull;
    return v;
}
/** The status caption row (y 58..65): its ink's box and whether every inked pixel is cyan. */
struct Caption { int l = -1, t = -1, r = -1, b = -1; bool any = false, cyan = true; };
Caption caption() {
    Caption c;
    c.any = ink_box(kLeft, 58, kWidth, 8, c.l, c.t, c.r, c.b);
    for (int y = 58; y < 66; ++y)
        for (int x = kLeft; x <= kRight; ++x)
            if (px(x, y) != kBlack && px(x, y) != kCyan) c.cyan = false;
    return c;
}
/** The selector list's reverse-video bar on list row `row` (a4_ui4_console's bar(row)). */
bool list_bar(int row) { return px(184, 44 + row * 14) == kWhite && px(317, 44 + row * 14 + 13) == kWhite; }

// --- the console --------------------------------------------------------------
struct Console {
    std::vector<std::string> rows;
    size_t capacity = 0;
    int cell_w = 0, line_h = 0;
    std::string from_end(size_t k) const { return k < rows.size() ? rows[rows.size() - 1 - k] : "<none>"; }
};
Console console(Run &h) {
    Console c;
    const auto ui_size = h.rt->device_settings().ui_size;
    const auto m = tdeck::ui_text_metrics(ui_size);
    size_t cols = 0;
    tdeck::world_transcript_geometry(ui_size, false, cols, c.capacity);
    c.cell_w = m.cell_width;
    c.line_h = m.line_height;
    UiRenderedLine buf[tdeck::kAlphaTranscriptLines]{};
    const size_t k = h.rt->ui()->visible_lines(buf, c.capacity, cols);
    for (size_t i = 0; i < k; ++i) c.rows.emplace_back(buf[i].text);
    return c;
}
/** The wait cursor is drawn at `column` of the console's last row, and the cell holds ink. */
bool cursor_after(Run &h, const Console &c, int column, std::string &why) {
    const auto &cur = h.board.console_cursor_state();
    const int want_x = kLeft + column * c.cell_w;
    const int want_y = openu5::kHudTranscriptY + int(c.capacity - 1) * c.line_h;
    const int ink = cur.visible ? lit(cur.x, cur.y, cur.width, cur.height) : 0;
    why = "cursor visible=" + n(cur.visible) + " at " + n(cur.x) + "," + n(cur.y) + " want " + n(want_x) + "," +
          n(want_y) + " ink=" + n(ink);
    return cur.visible && cur.x == want_x && cur.y == want_y && cur.width == c.cell_w && ink > 0;
}

const std::vector<Member> kParty = {{"Avatar", 'G', 120}, {"Iolo", 'G', 110}, {"Shamino", 'G', 100}};

// ---------------------------------------------------------------------------
void test_dos() {
    std::printf("\nW  the 1988 interaction model (DOS profile)\n");
    DosProfile profile;
    Run h(kParty);
    h.run(100);
    const uint64_t caption_before = region_hash(kLeft, 58, kWidth, 8);
    const uint64_t roster_before = region_hash(kLeft, 4, kWidth, 48);
    const bool world_clean = platform_dos_interface() && !rule(23) && !rule(43) && !rule(215) && inverted_rows() == 0;
    check(world_clean, "W0 the DOS profile is live; the world panel has no selector rules, no context bar, no reverse row");

    const bool routed = act(h, ch(u'c'));
    const bool picker = routed && h.rt->ui()->mode() == UiMode::PartySelection &&
                        h.rt->ui()->request() == UiRequestId::CastMember &&
                        std::strcmp(h.rt->ui()->prompt(), "Player: ") == 0;
    check(picker, "W1a (C)ast with three eligible members and no active one opens the \"Player: \" picker");
    dump("w1-dos-picker");
    check(!rule(23) && !rule(43) && !h.rt->selection_view_for_test().active,
          "W1b no selector panel: the rules at y=23 / y=43 are not drawn (white " + n(white_in_row(23)) + " / " +
              n(white_in_row(43)) + " of " + n(kWidth) + "), nothing composed");
    check(!rule(215) && !h.rt->context_bar_for_test().active,
          "W1c no context bar: no rule at y=215 (white " + n(white_in_row(215)) + " of " + n(kWidth) + ")");
    const auto hl = h.rt->party_highlight_for_test();
    check(inverted(0) && inverted_rows() == 1 && hl.cursor == 0 && hl.select_banner,
          "W2 roster row 0 (y 4..11) is in reverse video, the only one (" + n(inverted_rows()) + " inverted)");
    const auto cap = caption();
    check(cap.any && cap.cyan && cap.l >= 228 && cap.l <= 231 && cap.r >= 258 && cap.r <= 270 &&
              region_hash(kLeft, 58, kWidth, 8) != caption_before,
          "W3 the caption row reads \"Select:\" (cyan ink from x " + n(cap.l) + " to " + n(cap.r) +
              ", the seven-space indent, 7 compact cells), not the location");
    std::string why;
    const auto c1 = console(h);
    const bool player_cursor = cursor_after(h, c1, 8, why);
    check(c1.from_end(0) == "Player: " && player_cursor,
          "W4 \"Player: \" is the console's last row [" + c1.from_end(0) + "], the cursor after it (" + why + ")");

    act(h, highlight(1));
    const auto hl1 = h.rt->party_highlight_for_test();
    check(h.rt->ui()->mode() == UiMode::PartySelection && inverted(1) && !inverted(0) && inverted_rows() == 1 &&
              hl1.cursor == 1,
          "W5 HighlightIndex(1) moves the reverse row to row 1 and chooses nothing");
    dump("w5-dos-highlight");

    act(h, kind(UiActionKind::Confirm));
    dump("w6-dos-spell-name");
    const auto c2 = console(h);
    const bool spell = h.rt->ui()->mode() == UiMode::TextEntry && h.rt->ui()->request() == UiRequestId::SpellName;
    check(spell && c2.from_end(2) == "Player: Iolo" && c2.from_end(1) == "Spell name:" && c2.from_end(0) == ":",
          "W6a RETURN picks Iolo: the console ends \"Player: Iolo\" / \"Spell name:\" / \":\" [" + c2.from_end(2) +
              "] [" + c2.from_end(1) + "] [" + c2.from_end(0) + "]");
    const int last_y = openu5::kHudTranscriptY + int(c2.capacity - 1) * c2.line_h;
    const bool colon_cursor = cursor_after(h, c2, 1, why);
    check(colon_cursor && lit(kLeft, last_y, c2.cell_w, c2.line_h) > 0 &&
              lit(kLeft + 2 * c2.cell_w, last_y, kWidth - 2 * c2.cell_w, c2.line_h) == 0,
          "W6b the ':' is drawn in the last row's first cell and the cursor right after it (" + why + ")");
    check(!rule(215) && !h.rt->context_bar_for_test().active,
          "W6c the spell name is read on the console: still no context bar (white " + n(white_in_row(215)) + ")");
    check(inverted_rows() == 0 && region_hash(kLeft, 58, kWidth, 8) == caption_before &&
              region_hash(kLeft, 4, kWidth, 48) == roster_before,
          "W6d the picker closed: no reverse row, the roster and the location caption as before");
}

// ---------------------------------------------------------------------------
// windows/original-dos-interaction (2026-10-14): the item list is the 1988
// right panel over the boxes, the console left whole under it -- never the
// handheld's right-column list.
//   W7 a DOS profile, Avatar with a worn helm and a spare shield: (R)eady,
//      RETURN on Avatar
//      W7a a DeviceDosPanel is composed: the name in the band, the frame, the
//          worn helm's `--` and RUNES.CH glyph, the shield's count
//      W7b its pixels: the band (y 3) and the box's top rule (y 7) left of the
//          banner's notch, the console's top rule at y 87; none of the
//          selector's rules (y 23 / 43 / 160) and no context bar (y 215)
//      W7c the console is the world's: "Item: " is its last row, the cursor after it
//      W7d ESC: "Item: Done", the panel gone, the roster and caption back
//   T7 the T-Deck control: the same keys draw the selector list (y 23 / 43)
//      and compose no DOS panel
bool band_row(int y, int x0, int x1) {
    for (int x = x0; x <= x1; ++x)
        if (px(x, y) != 0x0015) return false;
    return true;
}
bool white_run(int y, int x0, int x1) {
    for (int x = x0; x <= x1; ++x)
        if (px(x, y) != kWhite) return false;
    return true;
}
void equip_avatar(Run &h) {
    auto &g = h.rt->game();
    g.equipment_quantities[0] = 0; // Leather Helm: worn, none left in the pack
    g.equipment_quantities[4] = 2; // two Small Shields in the pack
    g.party.characters[0].helmet = 0;
}

void test_dos_lists() {
    std::printf("\nW7 the 1988 item list (DOS profile)\n");
    DosProfile profile;
    Run h(kParty);
    equip_avatar(h);
    h.run(100);
    const uint64_t roster_before = region_hash(kLeft, 4, kWidth, 48);
    act(h, ch(u'r'));
    act(h, kind(UiActionKind::Confirm));
    dump("w7-dos-ready");
    const auto view = h.rt->selection_view_for_test();
    const tdeck::DeviceDosPanel *dos = view.active ? view.dos : nullptr;
    const bool composed = dos && std::strcmp(dos->banner, "Avatar") == 0 && dos->cells[0][0] == tdeck::kDosBoxTopLeft &&
                          dos->cells[1][1] == '-' && dos->cells[1][2] == '-' && ((dos->runes[1] >> 3) & 1U) &&
                          dos->cells[2][2] == '2' && dos->cells[2][3] == ' ' && dos->inverse[1] == 0x3ffe;
    check(h.rt->ui()->mode() == UiMode::EquipmentSelection && composed,
          "W7a (R)eady composes the 1988 panel: \"Avatar\" in the band, the frame, \"--\" + the RUNES.CH glyph on "
          "the worn helm, \"2\" on the shields, the bar on cols 1..13");
    check(band_row(3, 184, 200) && white_run(7, 183, 200) && white_run(87, 183, 317),
          "W7b the band (y 3) and the box's top rule (y 7) left of the notch, the console's rule at y 87");
    check(!rule(23) && !rule(43) && !rule(160) && !rule(215),
          "W7b' none of the handheld selector's rules (y 23 / 43 / 160) and no context bar (y 215)");
    std::string why;
    const auto c = console(h);
    check(c.from_end(0) == "Item: " && cursor_after(h, c, 6, why),
          "W7c the console is whole under the panel: its last row is \"Item: \" [" + c.from_end(0) +
              "], the cursor after it (" + why + ")");
    act(h, kind(UiActionKind::Cancel));
    dump("w7-dos-ready-closed");
    const auto c2 = console(h);
    // (A blank row, then the command prompt's new, empty row, follow it.)
    check(h.rt->ui()->mode() == UiMode::Exploration && !h.rt->selection_view_for_test().active &&
              (c2.from_end(0) == "Item: Done" || c2.from_end(1) == "Item: Done" || c2.from_end(2) == "Item: Done") &&
              region_hash(kLeft, 4, kWidth, 48) == roster_before && white_run(86, 183, 317) && !white_run(87, 183, 317),
          "W7d ESC: \"Item: Done\" [" + c2.from_end(3) + "] [" + c2.from_end(2) + "] [" + c2.from_end(1) + "] [" + c2.from_end(0) +
              "], the panel gone, the roster back, the console's rule at y 86 again");
}

void test_tdeck_lists() {
    std::printf("\nT7 the T-Deck control: the same keys, the handheld list\n");
    Run h(kParty);
    equip_avatar(h);
    h.run(100);
    act(h, ch(u'r'));
    act(h, kind(UiActionKind::Confirm));
    const auto view = h.rt->selection_view_for_test();
    check(h.rt->ui()->mode() == UiMode::EquipmentSelection && view.active && !view.dos && rule(23) && rule(43) &&
              !white_run(87, 183, 317),
          "T7 (R)eady draws the selector list (rules at y 23 / 43) and composes no DOS panel");
}

// ---------------------------------------------------------------------------
void test_tdeck() {
    std::printf("\nT  the T-Deck control (default profile)\n");
    Run h(kParty);
    h.run(100);
    const bool routed = act(h, ch(u'c'));
    check(!platform_dos_interface() && routed && h.rt->ui()->mode() == UiMode::PartySelection &&
              h.rt->ui()->request() == UiRequestId::CastMember,
          "T1 the same (C)ast opens the CastMember picker");
    dump("t1-tdeck-picker");
    const auto view = h.rt->selection_view_for_test();
    check(rule(23) && rule(43) && view.active, "T2 the selector panel is drawn: its rules at y=23 and y=43");
    check(rule(215) && view.context.active, "T3 its context bar is there: the rule at y=215");
    // (Row 5's y 44..51 is the list's own bar here, below the panel's rule at 43.)
    const auto hl = h.rt->party_highlight_for_test();
    check(!inverted(0) && !inverted(1) && !inverted(2) && hl.cursor < 0 && !hl.select_banner,
          "T4 no roster row in reverse video (the panel covers the roster), no \"Select:\" banner");
    act(h, highlight(1));
    UiSelectionView v{};
    check(h.rt->ui()->selection_view(v) && v.cursor == 1 && list_bar(1) && !list_bar(0),
          "T5 HighlightIndex(1) moves the LIST's bar to row 1");
    act(h, kind(UiActionKind::Confirm));
    const auto c = console(h);
    check(h.rt->ui()->mode() == UiMode::SpellSelection && c.from_end(0) != ":",
          "T6 RETURN goes on to the spell list, not a typed name (last console row [" + c.from_end(0) + "])");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: win_dos_runtime <openu5-alpha1-resources.bin> [--dump <dir>]\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
    g_dump = arg_after(argc, argv, "--dump");
    test_dos();
    test_tdeck();
    test_dos_lists();
    test_tdeck_lists();
    check(!platform_dos_interface() && platform_profile().dos_interface == nullptr,
          "Z the default profile is restored after the DOS scope");
    std::printf("win_dos_runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
