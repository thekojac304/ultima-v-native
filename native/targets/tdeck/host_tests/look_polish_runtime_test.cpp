// LOOK-POLISH1 (docs/WINDOWS_LOOK_ENHANCED_POLISH.md): on the REAL AlphaRuntime and
// the REAL tdeck_board.cpp over the fake ST7789 (the A4-UI2 harness) -- the screen
// the T-Deck and Windows Enhanced share.
//
//   L  Look at a shop sign's post names the sign (D-117, LOOKOBJ 0x0510): Britain,
//      the road at (9,21), Look North; the other directions; the road itself.
//   F  the shop row and list row formatters: prices of 1..5 digits end in one
//      column ("<n> gp"), a cell of margin, names cut with '.', counts and
//      "[equipped]" kept whole, a companion row with no price.
//   S  the shop panel's pixels: every priced row's ink ends in the same column,
//      at least 6 px before the right rule, whatever the digits or the offer's
//      quantity; the selected row is still the bar; a companion row has no ink
//      where a price would be.
//   B  Iolo's Bows (Britain's blacksmith) through the real shop: Buy and Sell.
//   R  the Ready list: "Leather Armour [equipped]" keeps its marker whole.
//
//   look_polish_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> [--dump <dir>]
#include "a4_ui2_harness.h"
#include "openu5/debug_map_picker.h"
#include "openu5/hud.h"
#include "openu5/shop_orchestration.h"

using namespace a4_ui2;

namespace {
constexpr int kRowX = 184, kRowW = 134, kRowY = 37, kRowPitch = 14, kRightRule = 318;

bool teleport_town(Run &h, uint8_t location, int x, int y) {
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::SmallMap;
    r.location = location;
    r.floor = 0;
    r.x = x;
    r.y = y;
    const bool ok = apply_debug_teleport(h.rt->command_context_for_test(), r).status == DebugTeleportStatus::Applied;
    h.rt->render(h.board, true);
    return ok;
}

std::string last_lines(Run &h, size_t n) {
    std::string out;
    const size_t count = h.rt->ui()->transcript_size();
    for (size_t i = count > n ? count - n : 0; i < count; ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i)) out += std::string(b->text) + " | ";
    return out;
}

/** The rightmost ink column of shop row `row` (ink = not the row's background). */
int row_last_ink(int row) {
    const int y0 = kRowY + row * kRowPitch;
    const uint16_t bg = px(kRowX, y0);
    int last = -1;
    for (int y = y0; y < y0 + kRowPitch; ++y)
        for (int x = kRowX; x < kRowX + kRowW; ++x)
            if (px(x, y) != bg) last = std::max(last, x);
    return last;
}
int ink_between(int row, int x0, int x1) {
    const int y0 = kRowY + row * kRowPitch;
    const uint16_t bg = px(kRowX, y0);
    int k = 0;
    for (int y = y0; y < y0 + kRowPitch; ++y)
        for (int x = x0; x <= x1; ++x) k += px(x, y) != bg;
    return k;
}
bool bar(int row) { return px(kRowX, kRowY + row * kRowPitch) == kWhite && px(317, kRowY + row * kRowPitch + 13) == kWhite; }

esp_err_t show_shop(Run &h, const tdeck::DeviceShopView &shop) {
    std::vector<uint16_t> view(size_t(openu5::kViewportPixels) * openu5::kViewportPixels, 0);
    openu5::TurnState turn{};
    openu5::HudWorldState hud{};
    return h.board.show_alpha(view.data(), *h.rt->ui(), h.rt->game(), turn, hud, pack->runes_font, nullptr, nullptr,
                              false, nullptr, false, 1, &shop);
}

std::string row_text(const tdeck::DeviceShopRow &r) {
    char line[24]{};
    tdeck::format_shop_row(line, sizeof(line), r);
    return line;
}
tdeck::DeviceShopRow offer(const char *name, int32_t price, int32_t quantity = 1, bool priced = true, bool counted = false) {
    tdeck::DeviceShopRow r{};
    std::snprintf(r.name, sizeof(r.name), "%s", name);
    r.price = price;
    r.quantity = quantity;
    r.priced = priced;
    r.counted = counted;
    return r;
}

void test_look() {
    struct Case { int x, y; RawInputKind ball; const char *seen; const char *what; };
    const Case cases[] = {
        {9, 21, RawInputKind::TrackballUp, "Thou dost see the sign of an armoury", "L1 the report: Britain (9,21) North, a post (0xE0)"},
        {10, 21, RawInputKind::TrackballUp, "Thou dost see the sign of an armoury", "L2 (10,21) North: 0xE0 then 0xE2"},
        {8, 18, RawInputKind::TrackballDown, "Thou dost see the sign of an armoury", "L3 (8,18) South onto 0xE1"},
        {11, 8, RawInputKind::TrackballRight, "Thou dost see the sign of a provisioner", "L4 (11,8) East onto 0xE1"},
        {19, 8, RawInputKind::TrackballLeft, "Thou dost see the sign of an inn", "L5 (19,8) West onto 0xE2"},
        {15, 21, RawInputKind::TrackballUp, "Thou dost see cobble", "L6 control: the road ahead is cobble"},
    };
    for (const auto &c : cases) {
        Run h({{"Kojac", 'G', 120}});
        const bool there = teleport_town(h, 2, c.x, c.y);
        h.key('l');
        h.ball(c.ball);
        h.run(200);
        const std::string tail = last_lines(h, 3);
        check(there && tail.find(c.seen) != std::string::npos && tail.find("parched desert") == std::string::npos,
              std::string(c.what) + " -> '" + tail + "'");
        if (c.x == 9) dump("look-north-armoury");
    }
}

void test_formatters() {
    // F1 one column whatever the digits: the price ends at cell 20, cell 21 blank.
    for (int32_t price : {1, 13, 109, 1009, 20190}) {
        const std::string t = row_text(offer("Bow", price));
        const std::string want = std::to_string(price) + " gp";
        check(t.size() == 22 && t[0] == ' ' && t[21] == ' ' && t.compare(21 - want.size(), want.size(), want) == 0 &&
                  t[21 - want.size() - 1] == ' ',
              "F1 price " + std::to_string(price) + " right-aligned to cell 20 with a margin cell: '" + t + "'");
    }
    // F2 the offer's quantity does not move the price (Arrows / Quarrels, 99).
    check(row_text(offer("Arrows", 13, 99)).substr(14) == row_text(offer("Sling", 13, 1)).substr(14) &&
              row_text(offer("Arrows", 13, 99)).find("x99") == std::string::npos,
          "F2 Arrows (quantity 99) prices in the same cells as Sling, and Buy shows no count: '" +
              row_text(offer("Arrows", 13, 99)) + "'");
    // F3 a long name is cut with '.', never into the price.
    const std::string longname = row_text(offer("Ring of Regeneration", 1500));
    check(longname == " Ring of Reg. 1500 gp ", "F3 a long name is cut with '.': '" + longname + "'");
    // F4 a Sell row carries the pack's count, kept whole when the name is cut.
    check(row_text(offer("Dagger", 1, 6, true, true)) == " Dagger x6       1 gp ",
          "F4 a Sell row shows its count: '" + row_text(offer("Dagger", 1, 6, true, true)) + "'");
    check(row_text(offer("Ring of Regeneration", 1500, 2, true, true)) == " Ring of. x2  1500 gp ",
          "F4 the count survives a cut name: '" + row_text(offer("Ring of Regeneration", 1500, 2, true, true)) + "'");
    // F5 a companion row (the healer's, the inn's) has no price at all.
    const std::string member = row_text(offer("Shamino", 0, 1, false));
    check(member == " Shamino              " && member.find("gp") == std::string::npos && member.find('0') == std::string::npos,
          "F5 a companion row has no price: '" + member + "'");
    // F6 the list rows: the count and the marker stay whole.
    char out[24]{};
    tdeck::fit_list_row(out, sizeof(out), "Leather Armour [equipped]", 21);
    check(std::string(out) == "Leather Armour [eq]", "F6 'Leather Armour [equipped]' in 21 cells: '" + std::string(out) + "'");
    tdeck::fit_list_row(out, sizeof(out), "Chain Coif [equipped]", 21);
    check(std::string(out) == "Chain Coif [equipped]", "F6 a row that fits is unchanged: '" + std::string(out) + "'");
    tdeck::fit_list_row(out, sizeof(out), "Flaming Oil x3", 21);
    check(std::string(out) == "Flaming Oil x3", "F6 counts that fit are unchanged");
    tdeck::fit_list_row(out, sizeof(out), "Ring of Invisibility x2 [equipped]", 21);
    check(std::string(out) == "Ring of Invi. x2 [eq]", "F6 count and marker both whole: '" + std::string(out) + "'");
    tdeck::fit_list_row(out, sizeof(out), "Chain Coif x1 [equipped]", 21);
    check(std::string(out) == "Chain Coif x1 [eq]", "F6 one worn, one in the pack: '" + std::string(out) + "'");
    tdeck::fit_list_row(out, sizeof(out), "Ring of Regeneration", 21);
    check(std::string(out) == "Ring of Regeneration", "F6 a 20-cell name with no tail is unchanged");
}

void test_panel() {
    Run h({{"Kojac", 'G', 120}});
    tdeck::DeviceShopView shop{};
    shop.active = true;
    std::snprintf(shop.title, sizeof(shop.title), "Iolo's Bows");
    std::snprintf(shop.keeper, sizeof(shop.keeper), "Gwenneth");
    std::snprintf(shop.phase, sizeof(shop.phase), "BUY 1-6/6");
    shop.gold = 9999;
    shop.row_count = shop.total = 6;
    shop.rows[0] = offer("Dagger", 1);
    shop.rows[1] = offer("Sling", 13);
    shop.rows[2] = offer("Bow", 109);
    shop.rows[3] = offer("Arrows", 13, 99);
    shop.rows[4] = offer("Crossbow", 1009);
    shop.rows[5] = offer("Quarrels", 20, 99);
    for (size_t selected : {size_t(2), size_t(3)}) {
        shop.selected_row = selected;
        const esp_err_t e = show_shop(h, shop);
        int first = -1;
        bool same = true;
        std::string cols;
        for (int r = 0; r < 6; ++r) {
            const int last = row_last_ink(r);
            cols += std::to_string(last) + " ";
            if (first < 0) first = last;
            same = same && last == first;
        }
        check(e == ESP_OK && same && kRightRule - first - 1 >= 6,
              "S1 selected " + n(long(selected)) + ": every price ends in one column, a cell before the rule (" + cols + ")");
        check(bar(int(selected)) && !bar(int(selected) == 2 ? 3 : 2), "S2 the selected row is still the bar");
        if (selected == 2) dump("shop-panel-prices");
    }
    // S3 the old two layouts would not pass S1: the quantity rows' prices sat two cells left.
    shop.selected_row = 0;
    shop.row_count = shop.total = 2;
    shop.rows[0] = offer("Shamino", 0, 1, false);
    shop.rows[1] = offer("Iolo", 0, 1, false);
    show_shop(h, shop);
    check(ink_between(0, kRowX + 13 * 6, kRowX + 21 * 6) == 0 && ink_between(1, kRowX + 13 * 6, kRowX + 21 * 6) == 0,
          "S3 a companion row has no ink where a price would be (no '0g')");
    // S4 the panel's row cache: the same name and price, now priced, is redrawn.
    shop.rows[1].priced = true;
    show_shop(h, shop);
    check(ink_between(1, kRowX + 13 * 6, kRowX + 21 * 6) > 0, "S4 a row whose only change is `priced` is redrawn");
}

void test_iolo() {
    Run h({{"Kojac", 'G', 120}, {"Shamino", 'G', 90}});
    h.rt->game().gold = 9999;
    const bool there = teleport_town(h, 2, 15, 30);
    const bool opened = there && h.rt->open_shop_for_test(0);
    h.run(100);
    h.key('\r'); // past the greeting to "Buy or Sell?"
    h.run(100);
    h.key('b');
    h.run(100);
    auto view = h.rt->shop_view_for_test();
    std::string names, cols;
    int first = -1;
    bool same = view.row_count > 0;
    for (size_t r = 0; r < view.row_count; ++r) {
        names += std::string(view.rows[r].name) + "=" + n(view.rows[r].price) + " ";
        const int last = row_last_ink(int(r));
        cols += std::to_string(last) + " ";
        if (first < 0) first = last;
        same = same && last == first;
    }
    check(opened && std::string(view.title) == "Iolo's Bows" && view.row_count == 6 &&
              std::string(view.rows[0].name) == "Dagger" && std::string(view.rows[3].name) == "Arrows" &&
              view.rows[3].quantity == 99,
          "B1 Iolo's Bows (Britain's blacksmith) Buy lists six weapons, Arrows an offer of 99: " + names);
    check(same && kRightRule - first - 1 >= 6, "B2 its prices end in one column on the panel (" + cols + ")");
    check(row_text(view.rows[0]).find(n(view.rows[0].price) + " gp") != std::string::npos,
          "B3 the Dagger's price reads '<n> gp': '" + row_text(view.rows[0]) + "'");
    dump("shop-iolo-buy");
    // Sell: the pack's counts (a fresh visit: past the greeting, then S).
    h.rt->game().equipment_quantities[16] = 6; // Daggers
    Run s({{"Kojac", 'G', 120}, {"Shamino", 'G', 90}});
    s.rt->game().equipment_quantities[16] = 6;
    const bool sell_open = teleport_town(s, 2, 15, 30) && s.rt->open_shop_for_test(0);
    s.run(100);
    s.key('\r');
    s.run(100);
    s.key('s');
    s.run(100);
    view = s.rt->shop_view_for_test();
    bool dagger = false;
    for (size_t r = 0; r < view.row_count; ++r)
        if (std::string(view.rows[r].name) == "Dagger")
            dagger = view.rows[r].counted && row_text(view.rows[r]).find("Dagger x6") != std::string::npos;
    std::string sold;
    for (size_t r = 0; r < view.row_count; ++r) sold += row_text(view.rows[r]) + "|";
    check(sell_open && dagger, "B4 Sell shows the pack's count beside the name (Dagger x6): " + sold);
    dump("shop-iolo-sell");
    // B5 a healer's companion list (Minoc, The Healers Mission): no price on a member.
    Run m({{"Kojac", 'G', 120}, {"Shamino", 'G', 90}});
    const bool healer = teleport_town(m, 5, 15, 30) && m.rt->open_shop_for_test(6);
    m.run(100);
    m.key('\r');
    m.run(100);
    m.key('h');
    m.run(100);
    view = m.rt->shop_view_for_test();
    bool none_priced = view.row_count >= 2;
    std::string members;
    for (size_t r = 0; r < view.row_count; ++r) {
        none_priced = none_priced && !view.rows[r].priced && row_text(view.rows[r]).find("gp") == std::string::npos;
        members += row_text(view.rows[r]) + "|";
    }
    check(healer && none_priced, "B5 the healer's companion rows carry no price: " + members);
    dump("shop-healer-member");
}

void test_ready() {
    Run h({{"Kojac", 'G', 120}});
    auto &g = h.rt->game();
    auto &c = g.party.characters[0];
    c.armor = 10; // Leather Armour, worn
    g.equipment_quantities[10] = 0;
    g.equipment_quantities[1] = 1; // a Chain Coif in the pack
    h.key('r');
    h.run(100);
    if (h.rt->ui()->mode() == UiMode::PartySelection) {
        h.key('\r');
        h.run(100);
    }
    const auto v = h.rt->selection_view_for_test();
    std::string rows;
    bool marker = false;
    for (size_t r = 0; r < v.row_count; ++r) {
        rows += std::string(v.rows[r]) + " | ";
        if (std::string(v.rows[r]).find("Leather") != std::string::npos)
            marker = std::string(v.rows[r]) == "Leather Armour [eq]";
    }
    check(v.active && marker, "R1 the Ready list keeps 'Leather Armour' and its marker whole in 21 cells: " + rows);
    dump("ready-list");
    // R2 the detail row: "Current: " and the worn item in its 22 cells.
    for (int i = 0; i < 3; ++i) h.down();
    h.run(50);
    const auto d = h.rt->selection_view_for_test();
    check(std::string(d.detail2) == "Current: Leather Armo.", "R2 the Ready detail fits its 22 cells: '" + std::string(d.detail2) + "'");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: look_polish_runtime <pack> <openu5-audio.bin> [--dump <dir>]\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
    g_dump = arg_after(argc, argv, "--dump");
    g_look_text = true;
    test_look();
    test_formatters();
    test_panel();
    test_iolo();
    test_ready();
    std::printf("LOOK-POLISH1 runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
