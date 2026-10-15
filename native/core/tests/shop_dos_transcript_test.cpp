// ORIG-DOS2: the console shops' DOS transcript (ShopServices::dos_transcript,
// Windows Original). Each shop visit below is driven through begin_shop /
// execute_shop and the console text the core streams is compared with the
// 1988 console sequence of native/core/spp1-findings/B-shops.md section 3
// (checked against SHOPPES*.OVL), every DOS string as the kernel prints it:
// print_string for a string, putchar for one character, both laid out by
// DosConsole on the 16-column console. The data and the SHOPPE.DAT records are
// synthetic ("R<n>" texts with the records' own '$' '%' '^' '&' '*' fields),
// so no game text is needed or tracked; the DOS strings are the ones
// shop_tables.inc already compiles. The rand-chosen variants are read off the
// shop's own draws (the rng trace), so the test checks that the transcript
// prints the variant the core drew -- and that it draws nothing more.
#include "openu5/display_names.h"
#include "openu5/shop_orchestration.h"
#include "openu5/ui_session.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace openu5;
namespace {
int failures = 0, checks = 0;
void check(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL " << what << "\n";
    }
}
std::string show(const std::string &s) {
    std::string r;
    for (char c : s) r += c == '\n' ? std::string("\\n") : std::string(1, c);
    return r;
}
void same(const std::string &actual, const std::string &expected, const std::string &what) {
    check(actual == expected, what);
    if (actual != expected) {
        size_t at = 0;
        while (at < actual.size() && at < expected.size() && actual[at] == expected[at]) ++at;
        std::cerr << "  expected: " << show(expected) << "\n  actual:   " << show(actual)
                  << "\n  first difference at byte " << at << ": expected ["
                  << show(expected.substr(at, 40)) << "] actual [" << show(actual.substr(at, 40)) << "]\n";
    }
}

// --- the DOS strings the 1988 shops print (DATA.OVL DS, via shop_tables.inc's
// copies; repeated here as the specification) ---
const char *kAsks[] = {"Wouldst thou buy one?", "Wilt thou take it?", "Wish ye it?", "May I get one for thee?"};
const char *kExcl[] = {"Very good!\n", "Excellent!\n", "Fine, fine!\n", "But of course!\n"};
const char *kIntro[] = {"We have:", "We stock:", "Thou canst buy:", "We've got:"};
const char *kWhich[] = {"What may I show thee?", "Which wouldst thou like to see?", "What is thine interest?",
                        "Which would ye see?"};
const char *kSellPrompt[] = {"Which item wouldst thou like to sell?", "What dost thou wish to sell?",
                             "Show me what ye got...", "What dost thou have for me to buy?"};
const char *kSellMore[] = {"What else can ye offer me?", "What else hath ye to sell?",
                           "What else doth thou wish to sell?", "What other arms wilt thou sell?"};
const char *kSellBye[] = {"Good-bye...", "Mayhap another time...", "Godspeed...", "Fare thee well..."};
const char *kBlacksmithAsk[] = {"Hail, friend! Wouldst thou Buy or Sell?", "Greetings, traveller! Wish ye to Buy, or hast thou wares to Sell?"};
const char *kReagent[] = {"Sulfur Ash", "Ginseng", "Garlic", "Spider Silk", "Blood Moss", "Black Pearl", "Nightshade", "Mandrake"};
const char *kWineLines[] = {"a) Rose.......18\n", "b) Claret....192\n", "c) Sauterne...79\n",
                            "d) Muscatel...30\n", "e) Moselle...275\n", "f) Chablis....98\n\n"};
const int kGreeting[8][4] = {{-1, -1, -1, -1}, {57, 58, 59, 60}, {92, 93, 94, 95}, {105, 106, 107, 108},
                             {127, 128, 129, 130}, {148, 149, 150, 151}, {165, 166, 167, 168}, {174, 175, 176, 177}};
const int kFarewellNone[8][4] = {{0, 1, 2, 3}, {61, 62, 63, 64}, {96, 97, 98, 99}, {109, 110, 111, 112},
                                 {131, 132, 133, 134}, {152, 153, 154, 155}, {169, 170, 171, 172}, {178, 179, 180, 181}};
const int kFarewellBought[8][4] = {{4, 5, 6, 7}, {65, 66, 67, 68}, {100, 101, 102, 103}, {113, 114, 115, 116},
                                   {135, 136, 137, 138}, {156, 157, 158, 159}, {169, 170, 171, 172}, {182, 183, 184, 185}};

// --- synthetic SHOPPE.DAT records ---
bool in(int i, int lo, int hi) { return i >= lo && i <= hi; }
std::string record_text(int i) {
    // Greetings of three lengths, for the gate's three spacings (SHOPPES
    // 0x01c6): the horse seller's ends at column 5 (a space), the healer's on a
    // line feed (column 0: "\n:"), the others past column 11 ("\n\n:").
    if (in(i, 92, 95)) return "Hi!\"";
    if (in(i, 165, 168)) return "Hi" + std::to_string(i) + " $.\"\n";
    for (const auto &row : kFarewellNone)
        for (int r : row) if (r == i) return "Bye" + std::to_string(i) + "!\"";
    for (const auto &row : kFarewellBought)
        for (int r : row) if (r == i) return "Bye" + std::to_string(i) + "!\"";
    for (const auto &row : kGreeting)
        for (int r : row) if (r == i) return "Hi" + std::to_string(i) + " $?\"";
    if (in(i, 8, 48)) return "It is " + std::to_string(i) + ", % gold.";
    if (in(i, 49, 56)) return "& for % gold?";
    if (in(i, 69, 72)) return "Menu" + std::to_string(i);
    if (in(i, 73, 76)) return "Again" + std::to_string(i);
    if (in(i, 77, 83)) return "Food % each.";
    if (i == 84) return "% gold.";
    if (in(i, 85, 88)) return "& at *" + std::to_string(i) + ".";
    if (i == 89) return "Meagre!\"";
    if (i == 90) return "Scraps!\" orders $.";
    if (i == 91) return ", no gold!\"";
    if (i == 104) return "A horse, % gold.";
    if (i == 117) return "Frigate % gold.";
    if (i == 118) return "Skiff % gold.";
    if (i == 119) return "\n\n\"Ships here.\" ";
    if (i == 122) return "\"Cheat!\" ";
    if (i == 123) return "\n\n\"Sold!\"";
    if (i == 124) return "\n\nMore, ";
    if (i == 126) return "\n\nTake it?\" ";
    if (in(i, 139, 146)) return "^ for %.";
    if (i == 147) return "\n\n\"Out!\" snarls $.\n";
    if (in(i, 160, 162)) return "Lot " + std::to_string(i) + ", %.";
    if (i == 163) return "\n\n\"Die!\"\n";
    if (i == 164) return "d\n\n\"Limited.\n\n";
    if (i == 173) return "\n\n\"No charity.\"\nsays $.";
    if (in(i, 186, 190)) return "Room % gold.";
    if (i == 191) return "\n\n\"Alone!\"\n";
    if (i == 192) return "\n\n\"Morgue.\"\n";
    if (i == 193) return "\n\n\"Guards!\"\n";
    return "R" + std::to_string(i);
}
const char *record_cstr(int i) {
    static std::map<int, std::string> cache;
    auto it = cache.find(i);
    if (it == cache.end()) it = cache.emplace(i, record_text(i)).first;
    return it->second.c_str();
}

// --- synthetic shop data ---
const ShopRecord kRecords[] = {
    {2, ShopType::Blacksmith, 0, "The Forge", "Gorn"},
    {1, ShopType::MagicSeller, 0, "Herbs", "Mira"},
    {8, ShopType::GuildMaster, 0, "Guild", "Thane"},
    {1, ShopType::Barkeeper, 0, "The Tap", "Lugh"},
    {19, ShopType::Barkeeper, 5, "Vinery", "Vera"},
    {6, ShopType::Healer, 1, "Clinic", "Ana"},
    {2, ShopType::InnKeeper, 0, "Rest Inn", "Ivo"},
    {3, ShopType::Shipwright, 0, "Docks", "Sal"},
    {6, ShopType::HorseSeller, 0, "Stable", "Hob"},
};
int32_t equipment_prices[48];
const int32_t weapons[8] = {0, 1, 27, 255, 255, 255, 255, 255};
const int32_t reagent_prices[8] = {5, 0, 7, 0, 0, 0, 0, 9};
const int32_t reagent_grants[8] = {3, 0, 4, 0, 0, 0, 0, 2};
const int32_t heal_prices[8] = {10, 12, 14, 16, 18, 20, 22, 24};
const int32_t cure_prices[8] = {5, 6, 7, 8, 9, 10, 11, 12};
const int32_t res_prices[8] = {100, 110, 120, 130, 140, 150, 160, 170};

// --- the expected console, built call by call ---
struct Dos {
    DosConsole k;
    std::string out;
    std::string keeper, shop, item, place;
    int32_t price = 0, quantity = 0;
    std::string ex(const std::string &raw) const {
        std::string r;
        for (char c : raw) {
            if (c == '$') r += keeper;
            else if (c == '#') r += shop;
            else if (c == '@') r += "morning";
            else if (c == '%') r += std::to_string(price);
            else if (c == '^') r += std::to_string(quantity);
            else if (c == '&') r += item;
            else if (c == '*') r += place;
            else r += c;
        }
        return r;
    }
    void p(const std::string &raw) { // print_string of the expanded string
        const auto text = ex(raw);
        std::vector<char> buf(text.size() * 2 + 64);
        DosConsoleText o{buf.data(), buf.size(), 0};
        k.print(text.c_str(), o);
        out.append(buf.data(), o.length);
    }
    void c(char ch) { // putchar
        char buf[8];
        DosConsoleText o{buf, sizeof buf, 0};
        k.put(ch, o);
        out.append(buf, o.length);
    }
    void rec(int i) { p(record_text(i)); }
    void typed(const std::string &text) { // the getstring's own echo: the UI's
        char buf[64];
        for (char ch : text) {
            DosConsoleText o{buf, sizeof buf, 0};
            k.put(ch, o);
        }
    }
    uint8_t col() const { return k.column; }
    // SHOPPES 0x01B6: '"' + the greeting record, then by the cursor.
    void greeting(ShopType type, int draw) {
        c('"');
        rec(kGreeting[int(type)][draw]);
        if (col() > 11) p("\n\n:");
        else if (col() == 0) p("\n:");
        else c(' ');
    }
    // SHOPPES 0x0202: "\n\n\"" + the farewell record, a line break unless at
    // column 0, "says $.\n".
    void farewell(ShopType type, bool bought, int draw) {
        p("\n\n\"");
        rec((bought ? kFarewellBought : kFarewellNone)[int(type)][draw]);
        if (col() != 0) c('\n');
        p("says $.\n");
    }
};

// --- one shop visit ---
struct Visit {
    GameState g;
    TurnState t;
    TravelState travel;
    CommandState commands;
    WorldData world;
    CommandContext c{g, t, travel, commands, world};
    ShopSession session;
    ShopData data;
    ShopServices services{session, data};
    std::string console;            // the stream Messages, joined
    std::vector<std::string> plain; // Messages that are not stream
    std::vector<std::string> results;
    std::vector<int> draws;
    int stream_events = 0;
    Dos dos;
    const ShopRecord *record = nullptr;
    explicit Visit(int which, bool transcript = true) {
        for (auto &p : equipment_prices) p = 30;
        equipment_prices[8] = 0;
        equipment_prices[27] = 5;
        record = &kRecords[which];
        data.records = kRecords;
        data.record_count = std::size(kRecords);
        data.equipment_prices = {equipment_prices, 48};
        data.weapons = {weapons, 8};
        data.reagent_prices = {reagent_prices, 8};
        data.reagent_grants = {reagent_grants, 8};
        data.heal_prices = {heal_prices, 8};
        data.cure_prices = {cure_prices, 8};
        data.resurrect_prices = {res_prices, 8};
        services.context = this;
        services.record_present = [](void *, int32_t i) { return i >= 0 && i < 194; };
        services.record = [](void *, int32_t i) -> const char * { return i >= 0 && i < 194 ? record_cstr(i) : nullptr; };
        services.tile = [](void *, int32_t, int32_t) { return 5; };
        services.occupied = [](void *, int32_t, int32_t) { return false; };
        services.reserve = [](void *, bool) { return true; };
        services.plate = [](void *, int32_t, int32_t, int32_t) {};
        services.ship = [](void *, int32_t, int32_t, int32_t, int32_t, int32_t) {};
        services.horse = [](void *, int32_t, int32_t) {};
        services.hour_tiles = [](void *) {};
        services.wake_npcs = [](void *) {};
        services.transactional_services = !transcript;
        services.dos_transcript = transcript;
        c.shop_services = &services;
        c.events = {this, [](void *p, const GameEvent &e) {
                        auto &v = *static_cast<Visit *>(p);
                        if (e.kind == GameEventKind::Message) {
                            if (e.stream) {
                                ++v.stream_events;
                                v.console += e.text ? e.text : "";
                            } else
                                v.plain.push_back(e.text ? e.text : "");
                        }
                        if (e.kind == GameEventKind::Shop && e.shop && e.shop->result && e.shop->result->message &&
                            *e.shop->result->message)
                            v.results.push_back(e.shop->result->message);
                    }};
        c.rng_trace = {this, [](void *p, const char *tag, int32_t, int32_t, int32_t value) {
                           if (tag && std::strcmp(tag, "shop") == 0) static_cast<Visit *>(p)->draws.push_back(value);
                       }};
        travel.shadowlord_here = -1; // no Falsehood drain: every draw is a shop variant
        g.position.map.location = uint8_t(record->location);
        g.position.xy = {10, 10};
        g.time.hour = 10;
        g.gold = 2000;
        g.food = 100;
        g.karma = 50;
        g.rng.seed(uint16_t(1234 + which * 7));
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &a = g.party.characters[0];
        std::snprintf(a.name, sizeof(a.name), "Avatar");
        a.gender = 0x0b;
        a.status = 'G';
        a.party_status = 0;
        a.intelligence = 15;
        a.current_hp = a.max_hp = 50;
        a.character_class = 'A';
        g.equipment_count = 48;
        dos.keeper = record->keeper;
        dos.shop = record->name;
    }
    void member(int i, const char *name, char status, uint8_t party_status = 0, uint8_t gender = 0x0c) {
        auto &m = g.party.characters[i];
        std::snprintf(m.name, sizeof(m.name), "%s", name);
        m.gender = gender;
        m.status = status;
        m.intelligence = 12;
        m.max_hp = 40;
        m.current_hp = 40;
        m.character_class = 'F';
        m.party_status = party_status;
        if (i + 1 > g.party.character_count) g.party.character_count = uint8_t(i + 1);
        if (!party_status) ++g.party.party_size;
    }
    void open() {
        NpcActor npc;
        npc.schedule.dialog = uint8_t(0x81 + int(record->type));
        npc.schedule.times[0] = uint8_t(g.time.hour + 1);
        npc.schedule.times[1] = uint8_t(g.time.hour);
        begin_shop(c, npc);
    }
    void act(ShopAction a, int32_t value = 0) { execute_shop(c, {a, value}); }
    void say(const char16_t *text) {
        ShopInput in{ShopAction::Text, 0, text, std::char_traits<char16_t>::length(text)};
        execute_shop(c, in);
    }
    int draw(size_t i) const { return i < draws.size() ? draws[i] : -1; }
};

// --- DosConsole against the kernel printer (re/notes/printstr-1850-derivacion.md) ---
std::string lay(uint8_t &column, const char *text) {
    char buf[256];
    DosConsoleText o{buf, sizeof buf, 0};
    DosConsole k;
    k.column = column;
    k.print(text, o);
    column = k.column;
    return std::string(buf, o.length);
}
void console_cases() {
    uint8_t col = 0;
    same(lay(col, "Pass\n"), "Pass\n", "print 'Pass\\n' from column 0: one row");
    check(col == 0, "print 'Pass\\n' leaves column 0");
    same(lay(col, "Ready...\n\n"), "Ready...\n\n", "print 'Ready...\\n\\n': a row and a blank row");
    same(lay(col, ""), "", "print '' prints nothing");
    col = 0;
    same(lay(col, "Hail, friend! Wouldst thou Buy or Sell?\" "),
         "Hail, friend!\nWouldst thou Buy\nor Sell?\" ", "print: the soft wrap at the last space that fits");
    check(col == 10, "print: the column after a wrapped string");
    col = 0;
    same(lay(col, "ABCDEFGHIJKLMNOP\n"), "ABCDEFGHIJKLMNOP\n",
         "print: 16 cells then LF is one row (the LF is the wrap)");
    check(col == 0, "print: column 0 after a full row");
    col = 5;
    same(lay(col, "ABCDEFGHIJKLMNOPQRST"), "\nABCDEFGHIJK\nLMNOPQRST",
         "print: a word longer than the room breaks the row (0x1981) and goes on");
    col = 14;
    same(lay(col, "12"), "12\n", "print: two cells left, two characters: they fit, then the wrap");
    col = 12;
    same(lay(col, "abc def"), "abc\ndef", "print: the cut at the space inside the room, the row owes its LF");
    col = 12;
    same(lay(col, "abcdef"), "\nabcdef", "print: no space inside the room, column > 0: a new row first");
    col = 0;
    same(lay(col, "abcdefghijklmno \nX"), "abcdefghijklmno\nX",
         "print: a row cut at its trailing space eats the LF and owes one (0x19c7)");
    {
        DosConsole k;
        char buf[64];
        DosConsoleText o{buf, sizeof buf, 0};
        for (int i = 0; i < 16; ++i) k.put('x', o);
        same(std::string(buf, o.length), "xxxxxxxxxxxxxxxx\n", "putchar: the 16th cell wraps");
        check(k.column == 0, "putchar: column 0 after the wrap");
        o.length = 0;
        k.put('a', o);
        k.put('\n', o);
        same(std::string(buf, o.length), "a\n", "putchar: LF");
        check(k.column == 0, "putchar: LF leaves column 0");
    }
}

// --- the blacksmith: buy (SHOPPES 0x12B2 / 0x0B30 / 0x09AC) ---
std::string buy_row_name(int item) { return equipment_display_name(item) ? equipment_display_name(item) : ""; }
void buy_list(Dos &d, int which) {
    d.p("\n\n");
    for (int i = 0; i < 3; ++i) {
        d.c(char('a' + i));
        d.p("...");
        d.p(buy_row_name(weapons[i]));
        if (d.col() != 0) d.c('\n');
    }
    d.c('\n');
    d.p(kWhich[which]);
    d.p("\" ");
}
void blacksmith_buy() {
    Visit v(0);
    auto &d = v.dos;
    v.open();
    d.p("\"Good @, and welcome to #!\"\n");
    same(v.console, d.out, "blacksmith: the welcome");
    v.act(ShopAction::Continue);
    d.p("\n$ says,\n\"");
    d.p(kBlacksmithAsk[v.draw(0)]);
    d.p("\" ");
    same(v.console, d.out, "blacksmith: any key, then the Buy or Sell question");
    v.act(ShopAction::Buy);
    d.p("Buy\n\n\"");
    d.p(kExcl[v.draw(1)]);
    d.p(kIntro[v.draw(2)]);
    buy_list(d, v.draw(3));
    same(v.console, d.out, "blacksmith: the Buy echo, the chatter, the list without prices, WHICH");
    v.act(ShopAction::SelectItem, 1);
    d.c('b');
    d.p("\n\n\"");
    d.price = shop_buy_price(30, 15);
    d.rec(9);
    d.p("\n\n");
    d.p(kAsks[v.draw(4)]);
    d.p("\" ");
    same(v.console, d.out, "blacksmith: the letter echo, the pitch, ASKS");
    v.act(ShopAction::Decline);
    d.p("No\n\n");
    d.p("\"Anything else,\n");
    d.p("then?");
    buy_list(d, v.draw(5));
    same(v.console, d.out, "blacksmith: N echoes 'No', then 'Anything else, then?' and the list");
    v.act(ShopAction::SelectItem, 0);
    d.c('a');
    d.p("\n\n\"");
    d.rec(8);
    d.p("\n\n");
    d.p(kAsks[v.draw(6)]);
    d.p("\" ");
    v.act(ShopAction::Confirm);
    d.p("Yes\n");
    d.p("\nSold!\n");
    d.p("\"Anything else,\n");
    d.p("sir?");
    buy_list(d, v.draw(7));
    same(v.console, d.out, "blacksmith: Y echoes 'Yes', 'Sold!', 'Anything else, sir?' (a purchase)");
    check(v.g.gold == 2000 - shop_buy_price(30, 15), "blacksmith: the price was paid");
    v.act(ShopAction::Cancel);
    d.farewell(ShopType::Blacksmith, true, v.draws.back());
    same(v.console, d.out, "blacksmith: Space leaves the list with the purchase farewell");
    check(v.session.phase == ShopPhase::Closed, "blacksmith: closed");
    check(v.plain.empty(), "blacksmith: no line outside the transcript");
    check(v.results.empty(), "blacksmith: no result strings shown");
    check(v.draws.size() == 9, "blacksmith: no draw added by the transcript");
}
void blacksmith_menu_no() {
    Visit v(0);
    v.open();
    v.act(ShopAction::Continue);
    v.act(ShopAction::End);
    auto &d = v.dos;
    d.p("\"Good @, and welcome to #!\"\n");
    d.p("\n$ says,\n\"");
    d.p(kBlacksmithAsk[v.draw(0)]);
    d.p("\" ");
    d.p("No");
    d.farewell(ShopType::Blacksmith, false, v.draw(1));
    same(v.console, d.out, "blacksmith: Space at Buy or Sell echoes 'No' and says farewell");
}
void blacksmith_sell() {
    Visit v(0);
    auto &d = v.dos;
    v.g.equipment_quantities[5] = 2;
    v.g.equipment_quantities[8] = 1;  // base price 0
    v.g.equipment_quantities[27] = 10; // ammunition
    v.open();
    v.act(ShopAction::Continue);
    v.act(ShopAction::Sell);
    d.p("\"Good @, and welcome to #!\"\n");
    d.p("\n$ says,\n\"");
    d.p(kBlacksmithAsk[v.draw(0)]);
    d.p("\" ");
    d.p("Sell\n\n\"");
    d.p(kSellPrompt[v.draw(1)]);
    d.p("\" ");
    same(v.console, d.out, "blacksmith sell: the Sell echo and the prompt");
    v.act(ShopAction::SelectItem, 5);
    d.p("\n\n\"");
    d.item = equipment_display_name(5);
    d.price = shop_sell_price(30, 15);
    d.rec(49 + v.draw(2));
    d.p("\n\nDeal?\" ");
    same(v.console, d.out, "blacksmith sell: the offer and Deal?");
    v.act(ShopAction::Confirm);
    d.p("Yes\n\n\"Done!\"\nsays $.");
    d.p("\n\n\"");
    d.p(kSellMore[v.draw(3)]);
    d.p("\" ");
    same(v.console, d.out, "blacksmith sell: Y, Done!, SELLMORE");
    v.act(ShopAction::SelectItem, 8);
    d.p("\n\n\"");
    d.p("That, I cannot buy from thee.\"\nsays $.");
    d.p("\n\n\"");
    d.p(kSellMore[v.draw(4)]);
    d.p("\" ");
    same(v.console, d.out, "blacksmith sell: an item of no value");
    v.act(ShopAction::SelectItem, 5);
    d.p("\n\n\"");
    d.rec(49 + v.draw(5));
    d.p("\n\nDeal?\" ");
    v.act(ShopAction::Decline);
    d.p("No");
    d.p("\n\n\"");
    d.p(kSellMore[v.draw(6)]);
    d.p("\" ");
    same(v.console, d.out, "blacksmith sell: N echoes 'No'");
    v.act(ShopAction::Cancel);
    d.p("\n\n\"");
    d.p(kSellBye[v.draw(7)]);
    d.p("\"\n");
    d.p("says $.\n");
    same(v.console, d.out, "blacksmith sell: ESC says good-bye, and no farewell");
    check(v.draws.size() == 8, "blacksmith sell: the sell exit draws no farewell");
    check(v.session.phase == ShopPhase::Closed, "blacksmith sell: closed");
    Visit w(0);
    w.g.equipment_quantities[27] = 10;
    w.open();
    w.act(ShopAction::Continue);
    w.act(ShopAction::Sell);
    w.act(ShopAction::SelectItem, 27);
    auto &e = w.dos;
    e.p("\"Good @, and welcome to #!\"\n");
    e.p("\n$ says,\n\"");
    e.p(kBlacksmithAsk[w.draw(0)]);
    e.p("\" ");
    e.p("Sell\n\n\"");
    e.p(kSellPrompt[w.draw(1)]);
    e.p("\" ");
    e.p("\n\n\"We don't deal in used ammunition!\"\ngrowls $.\n");
    same(w.console, e.out, "blacksmith sell: ammunition growls and ends the visit");
    check(w.session.phase == ShopPhase::Closed, "blacksmith sell: closed after ammunition");
    Visit x(0);
    x.open();
    x.act(ShopAction::Continue);
    x.act(ShopAction::Sell);
    auto &f = x.dos;
    f.p("\"Good @, and welcome to #!\"\n");
    f.p("\n$ says,\n\"");
    f.p(kBlacksmithAsk[x.draw(0)]);
    f.p("\" ");
    f.p("Sell\n\n\"");
    f.p("Thou hast nothing to sell!\"\ngrowls $.\n");
    same(x.console, f.out, "blacksmith sell: nothing to sell");
}

// --- the reagents (SHOPPES 0x075E / 0x0666 / 0x0502) ---
void reagent_list(Dos &d) {
    const int slots[] = {0, 2, 7};
    for (int n = 0; n < 3; ++n) {
        d.c(char('A' + n));
        d.p("...");
        d.p(kReagent[slots[n]]);
        d.c('\n');
    }
    d.p("\nThy interest?\" ");
}
void reagents() {
    Visit v(1);
    auto &d = v.dos;
    v.g.reagent_quantities[2] = 99;
    v.open();
    d.greeting(ShopType::MagicSeller, v.draw(0));
    same(v.console, d.out, "reagents: the greeting and its gate spacing");
    v.act(ShopAction::Confirm);
    d.p("Yes\n\n\"Fine! We sell:\n\n");
    reagent_list(d);
    same(v.console, d.out, "reagents: Y, 'Fine! We sell:', the upper-case list, Thy interest?");
    v.act(ShopAction::SelectItem, 2);
    d.p("\n\n\"Thou canst not carry any more!\"\n\n");
    same(v.console, d.out, "reagents: at 99 the cap line, no pitch and no price");
    v.act(ShopAction::Continue);
    reagent_list(d);
    v.act(ShopAction::SelectItem, 0);
    d.p("\n\n\"");
    d.quantity = 3;
    d.price = shop_buy_price(5, 15);
    d.rec(139);
    d.p(" Is this thy need?\" ");
    if (d.col() > 12) d.p("\n:");
    same(v.console, d.out, "reagents: the pitch and 'Is this thy need?'");
    v.act(ShopAction::Decline);
    d.p("No\n\n\"What else?\n\n");
    reagent_list(d);
    same(v.console, d.out, "reagents: N, 'What else?' and the list");
    v.act(ShopAction::SelectItem, 0);
    d.p("\n\n\"");
    d.rec(139);
    d.p(" Is this thy need?\" ");
    if (d.col() > 12) d.p("\n:");
    v.act(ShopAction::Confirm);
    d.p("Yes\n");
    d.p("\n\"I thank thee!\"\nsays $.\n");
    d.p("\"Anything else?\n\n");
    reagent_list(d);
    same(v.console, d.out, "reagents: Y, 'I thank thee!', 'Anything else?' and the list");
    check(v.g.reagent_quantities[0] == 3, "reagents: the grant");
    v.act(ShopAction::Cancel);
    d.farewell(ShopType::MagicSeller, true, v.draws.back());
    same(v.console, d.out, "reagents: Space leaves with the farewell");
    check(v.results.empty() && v.plain.empty(), "reagents: nothing outside the transcript");
    check(v.draws.size() == 2, "reagents: two draws, the greeting and the farewell");
    Visit w(1);
    w.open();
    w.act(ShopAction::Decline);
    w.dos.greeting(ShopType::MagicSeller, w.draw(0));
    w.dos.p("No");
    w.dos.farewell(ShopType::MagicSeller, false, w.draw(1));
    same(w.console, w.dos.out, "reagents: N at the greeting echoes 'No'");
}

// --- the guild (SHOPPES 0x04A2 / 0x03F6 / 0x02BA) ---
void guild_list_rows(Dos &d) {
    d.p("a.........Keys\n");
    d.p("b.........Gems\n");
    d.p("c......Torches\n\n");
    d.p("Thy concern?\" ");
}
void guild() {
    Visit v(2);
    auto &d = v.dos;
    v.open();
    d.greeting(ShopType::GuildMaster, v.draw(0));
    v.act(ShopAction::Confirm);
    d.p("Yes\n\n\"We sell:\n\n");
    guild_list_rows(d);
    same(v.console, d.out, "guild: Y, 'We sell:', the three rows, Thy concern?");
    v.act(ShopAction::SelectItem, 3);
    d.rec(164);
    guild_list_rows(d);
    same(v.console, d.out, "guild: D prints the limited-time record (its own echo) and the list");
    v.act(ShopAction::SelectItem, 0);
    d.c('a');
    d.p("\n\n\"");
    d.price = guild_price(0, 0, 15);
    d.rec(160);
    d.p("\n\nInterested?\" ");
    same(v.console, d.out, "guild: the letter echo, the pitch, Interested?");
    v.act(ShopAction::Decline);
    d.p("No\n\n\"What else, then?\n\n");
    guild_list_rows(d);
    v.act(ShopAction::SelectItem, 1);
    d.c('b');
    d.p("\n\n\"");
    d.price = guild_price(0, 1, 15);
    d.rec(161);
    d.p("\n\nInterested?\" ");
    v.act(ShopAction::Confirm);
    d.p("Yes\n");
    d.p("\n\"Sold!\"\nsays $.\n\n\"What else, \n");
    d.p("m'lord");
    d.p("?\n\n");
    guild_list_rows(d);
    same(v.console, d.out, "guild: Y, Sold!, 'What else, m'lord?' and the list");
    v.act(ShopAction::Cancel);
    d.farewell(ShopType::GuildMaster, true, v.draws.back());
    same(v.console, d.out, "guild: Space leaves silently, then the farewell");
    check(v.results.empty() && v.plain.empty(), "guild: nothing outside the transcript");
}

// --- the tavern (SHOPPES2 0x066C) ---
const char *alive_word(int n) {
    static const char *w[] = {"", "", "two", "three", "four", "five", "six"};
    return n >= 0 && n <= 6 ? w[n] : "";
}
void tavern_menu(Dos &d, int record) {
    d.p("Yes\n\n\"");
    d.rec(record);
    d.c('"');
    d.c(' ');
    if (d.col() > 14) d.c('\n');
}
void announce(Dos &d, int cost, int living, bool sir) {
    d.c('"');
    d.p("That will be ");
    d.p(std::to_string(cost));
    d.p(" gold for the ");
    d.p(alive_word(living));
    d.p(" of ye,\n");
    d.p(sir ? "sir" : "milady");
    d.c('.');
}
void tavern() {
    Visit v(3);
    auto &d = v.dos;
    v.member(1, "Iolo", 'G');
    v.open();
    d.greeting(ShopType::Barkeeper, v.draw(0));
    v.act(ShopAction::Confirm);
    tavern_menu(d, 69);
    same(v.console, d.out, "tavern: Y, the subtype's menu record, the quote and space");
    const auto gold = v.g.gold;
    v.act(ShopAction::Round);
    d.c('M');
    d.p("\n\n");
    announce(d, 3 * 2, 2, true);
    d.p("\nEnjoy!\"\n\n");
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: the food round echoes its letter, is announced and charged at once");
    check(v.session.phase == ShopPhase::TavernAgain && v.g.gold == gold - 6,
          "tavern: the round is charged with no question (P4)");
    v.act(ShopAction::Confirm);
    tavern_menu(d, 73);
    v.act(ShopAction::Drink);
    d.c('A');
    d.p("\n\n");
    announce(d, 2, 2, true);
    d.p("\nEnjoy!\"\n\n");
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: the house drink");
    v.act(ShopAction::Confirm);
    tavern_menu(d, 73);
    v.act(ShopAction::Rumor);
    d.c('C');
    d.p("\n\n\"");
    d.p("Of what wouldst\nthou hear my\nlore, ");
    d.p("sir");
    d.p("?\"\n\nYou respond:\n");
    same(v.console, d.out, "tavern: the rumour question");
    v.say(u"xyzzy");
    d.typed("xyzzy");
    d.p("\n\n");
    d.p("\"That, I cannot help thee with.\n\n");
    d.p("Of what wouldst\nthou hear my\nlore, ");
    d.p("sir");
    d.p("?\"\n\nYou respond:\n");
    same(v.console, d.out, "tavern: an unknown word, and the question again");
    v.say(u"honesty");
    d.typed("honesty");
    d.p("\n\n");
    d.price = 50;
    d.rec(84);
    d.p("\n\nFair 'nuff?\" ");
    same(v.console, d.out, "tavern: the rumour's price and Fair 'nuff?");
    v.act(ShopAction::Confirm);
    d.p("Yes\n\n");
    d.item = "Malik";
    d.place = "Moonglow";
    d.rec(85 + v.draws.back());
    d.p("\nsays ");
    d.p(d.keeper);
    d.p(".\n\n");
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: the paid rumour is told");
    v.act(ShopAction::Decline);
    d.p("No");
    d.farewell(ShopType::Barkeeper, true, v.draws.back());
    same(v.console, d.out, "tavern: N at 'Anything else' and the farewell");
    check(v.results.empty() && v.plain.empty(), "tavern: nothing outside the transcript");
}
void tavern_wine() {
    Visit v(4);
    auto &d = v.dos;
    v.open();
    d.greeting(ShopType::Barkeeper, v.draw(0));
    v.act(ShopAction::Confirm);
    tavern_menu(d, 70);
    v.act(ShopAction::Drink);
    d.c('W');
    d.p("\n\n");
    d.p("\"Our wine list,\n");
    d.p("sir");
    d.p(".\n\n");
    for (const char *line : kWineLines) d.p(line);
    d.p("Thy choice?\" ");
    same(v.console, d.out, "tavern: the wine list");
    v.act(ShopAction::Cancel);
    d.c('\n');
    d.c('\n');
    d.rec(89);
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: Space at the wine list is the meagre-stock record");
    check(!v.session.tavern_served, "tavern: withdrawing from the wine list serves nothing");
    v.act(ShopAction::Confirm);
    tavern_menu(d, 74);
    v.act(ShopAction::Drink);
    d.c('W');
    d.p("\n\n");
    d.p("\"Our wine list,\n");
    d.p("sir");
    d.p(".\n\n");
    for (const char *line : kWineLines) d.p(line);
    d.p("Thy choice?\" ");
    v.act(ShopAction::SelectItem, 1);
    d.c('B');
    d.p("\n\n\"Ah, a fine\nchoice, ");
    d.p("sir");
    d.c('.');
    d.p("\nEnjoy!\"");
    d.c('\n');
    d.c('\n');
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: a wine is chosen, echoed and served");
    check(v.g.gold == 2000 - 192, "tavern: the claret's price");
}
void tavern_rations() {
    Visit v(3);
    auto &d = v.dos;
    v.open();
    d.greeting(ShopType::Barkeeper, v.draw(0));
    v.act(ShopAction::Confirm);
    tavern_menu(d, 69);
    v.act(ShopAction::Rations);
    d.c('R');
    d.c('\n');
    d.c('\n');
    d.c('"');
    d.price = ration_price(0, 15);
    d.rec(77 + v.draws.back());
    d.p("\n\nHow many wouldst\nthou like?\" ");
    same(v.console, d.out, "tavern: the rations pitch and 'How many wouldst thou like?'");
    v.say(u"2");
    d.typed("2");
    d.p("\n\n");
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: rations bought, then 'Anything else'");
    check(v.g.food == 150, "tavern: two rations");
    v.act(ShopAction::Confirm);
    tavern_menu(d, 73);
    v.act(ShopAction::Rations);
    d.c('R');
    d.c('\n');
    d.c('\n');
    d.c('"');
    d.rec(77 + v.draws.back());
    d.p("\n\nHow many wouldst\nthou like?\" ");
    v.say(u"0");
    d.typed("0");
    d.p("\n\n\"Hrumph.\"");
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: zero rations, Hrumph.");
}

// SHOPPES2 0x020a: the fourth cup -- "I beg thy pardon ... had enough to
// drink?"; Y is no drink, N is "No!", the drunk timer, karma, and the round.
void tavern_drunk() {
    Visit v(3);
    auto &d = v.dos;
    v.member(1, "Iolo", 'G');
    v.open();
    d.greeting(ShopType::Barkeeper, v.draw(0));
    v.act(ShopAction::Confirm);
    tavern_menu(d, 69);
    for (int cup = 0; cup < 3; ++cup) {
        v.act(ShopAction::Drink);
        d.c('A');
        d.p("\n\n");
        announce(d, 2, 2, true);
        d.p("\nEnjoy!\"\n\n");
        d.p("\"Anything else\nfor thee?\" ");
        v.act(ShopAction::Confirm);
        tavern_menu(d, 73);
    }
    v.act(ShopAction::Drink);
    d.c('A');
    d.p("\n\n\"I beg thy\npardon, ");
    d.p("sir");
    d.p(",\"\nsays ");
    d.p(d.keeper);
    d.p(".\n\"But haven't\nye had enough\nto drink?\" ");
    same(v.console, d.out, "tavern: the fourth cup's warning");
    v.act(ShopAction::Confirm);
    d.p("Yes\n\n");
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: Y at the warning, no drink");
    v.act(ShopAction::Confirm);
    tavern_menu(d, 73);
    v.act(ShopAction::Drink);
    d.c('A');
    d.p("\n\n\"I beg thy\npardon, ");
    d.p("sir");
    d.p(",\"\nsays ");
    d.p(d.keeper);
    d.p(".\n\"But haven't\nye had enough\nto drink?\" ");
    v.act(ShopAction::Decline);
    d.p("No!");
    d.p("\n\n");
    announce(d, 2, 2, true);
    d.p("\nEnjoy!\"\n\n");
    d.p("\"Anything else\nfor thee?\" ");
    same(v.console, d.out, "tavern: N at the warning: 'No!' and the round");
    check(v.t.drunk_turns == 25 && v.g.karma == 49, "tavern: drunk, and a point of karma");
}

// --- the healer (SHOPPES 0x14F8 / 0x137C / 0x146A) ---
void healer_need(Dos &d) {
    d.p("Yes\n\n");
    d.p("\"We have powers to Cure, Heal, or Resurrect.\"\n");
    d.p("says $.\n\n\"What is the nature of thy need?\" ");
}
void healer() {
    Visit v(5);
    auto &d = v.dos;
    v.member(1, "Shamino", 'G', 0, 0x0b);
    v.g.party.characters[1].current_hp = 10;
    v.open();
    d.greeting(ShopType::Healer, v.draw(0));
    v.act(ShopAction::Confirm);
    healer_need(d);
    same(v.console, d.out, "healer: Y, the powers line and the question");
    v.act(ShopAction::Heal);
    d.p("Healing");
    d.p("\n\n\"Who needs my aid?\" ");
    same(v.console, d.out, "healer: 'Healing' and 'Who needs my aid?'");
    v.act(ShopAction::SelectMember, 1);
    d.p("\n\n\"");
    d.p("I can heal thee ");
    d.price = heal_prices[1];
    d.p("for % gold.\n\nWilt thou\npay?\" ");
    same(v.console, d.out, "healer: the offer and 'Wilt thou pay?'");
    v.act(ShopAction::Confirm);
    d.p("Yes");
    d.p("\n\n\"Is there any other way in which I may\naid thee?\" ");
    same(v.console, d.out, "healer: Y and 'Is there any other way', no 'It is done.'");
    check(v.g.party.characters[1].current_hp == 40, "healer: healed");
    v.act(ShopAction::Confirm);
    healer_need(d);
    v.act(ShopAction::Cure);
    d.p("Curing");
    d.p("\n\n\"Who needs my aid?\" ");
    v.act(ShopAction::SelectMember, 0);
    d.p("\n\n\"Thou hast no need of this art!\"\nsays $.");
    d.p("\n\n\"Is there any other way in which I may\naid thee?\" ");
    same(v.console, d.out, "healer: no need of this art");
    v.act(ShopAction::Confirm);
    healer_need(d);
    v.act(ShopAction::Resurrect);
    d.p("Resurrect");
    d.p("\n\n\"Who needs my aid?\" ");
    v.act(ShopAction::Cancel);
    d.p("No one");
    d.p("\n\n\"Is there any other way in which I may\naid thee?\" ");
    same(v.console, d.out, "healer: ESC at the roster is 'No one'");
    v.act(ShopAction::Decline);
    d.p("No");
    d.farewell(ShopType::Healer, true, v.draws.back());
    same(v.console, d.out, "healer: N and the farewell");
    check(v.results.empty() && v.plain.empty(), "healer: nothing outside the transcript");
    Visit w(5);
    w.open();
    w.act(ShopAction::Confirm);
    w.act(ShopAction::End);
    w.dos.greeting(ShopType::Healer, w.draw(0));
    healer_need(w.dos);
    w.dos.p("Nothing");
    w.dos.farewell(ShopType::Healer, false, w.draw(1));
    same(w.console, w.dos.out, "healer: Space at the nature of thy need is 'Nothing'");
}

// --- the inn (SHOPPES3 0x08B4 / 0x0072 / 0x02AE / 0x04E6) ---
void inn_menu(Dos &d) {
    d.p("Yes");
    d.p("\n\n$ asks,\n\"Art thou here\nto Pick up or\n");
    d.p("Leave a\ncompanion, or\nto Rest for the\nnight?\" ");
}
const char *kInnMore = "\"Is there\nanything more\nI can do for\nthee?\" ";
void inn() {
    Visit v(6);
    auto &d = v.dos;
    v.member(1, "Iolo", 'G', 0, 0x0b);
    v.member(2, "Jaana", 'G');
    v.member(3, "Dupre", 'G', 2, 0x0b); // already a guest here
    v.member(4, "Katrina", 'G', 2);     // a second guest
    v.open();
    d.greeting(ShopType::InnKeeper, v.draw(0));
    v.act(ShopAction::Confirm);
    inn_menu(d);
    same(v.console, d.out, "inn: Y and the menu");
    v.act(ShopAction::LeaveMember);
    d.c('L');
    d.p("\n\n");
    d.p("$ asks,\n\"Who will\nstay?\" ");
    same(v.console, d.out, "inn: Leave echoes 'L' and asks who will stay");
    v.act(ShopAction::SelectMember, 0);
    d.p("\n\n");
    d.p("Thy friend");
    d.c('s');
    d.p(" will not leave thee!\n\n");
    d.p("$ asks,\n\"Who will\nstay?\" ");
    same(v.console, d.out, "inn: the Avatar will not stay");
    v.act(ShopAction::SelectMember, 2);
    d.p("\n\n");
    d.p("\"The rate for\nour most comfortable room will be ");
    d.price = inn_monthly_rate(inn_at(2), 15);
    d.p("% gold per month, due at check-out.");
    d.p("\nWilt thou take\nit?\" ");
    same(v.console, d.out, "inn: the rate line before 'Wilt thou take it?'");
    v.act(ShopAction::Confirm);
    d.p("Yes");
    d.p("\n\n");
    d.p("\"I thank thee.\"\nsays $.\n\n");
    d.p(kInnMore);
    same(v.console, d.out, "inn: Y, 'I thank thee.', 'Is there anything more'");
    v.act(ShopAction::Confirm);
    inn_menu(d);
    v.act(ShopAction::Pickup);
    d.p("\n\n\"Who will\ncheck out?\" ");
    same(v.console, d.out, "inn: two guests, 'Who will check out?' (the register)");
    v.act(ShopAction::Cancel);
    d.p("No one\n\n");
    d.p(kInnMore);
    same(v.console, d.out, "inn: ESC at the register is 'No one'");
    v.act(ShopAction::Confirm);
    inn_menu(d);
    v.act(ShopAction::Pickup);
    d.p("\n\n\"Who will\ncheck out?\" ");
    int dupre = -1;
    for (int i = 0; i < v.g.party.character_count; ++i)
        if (std::strcmp(v.g.party.characters[i].name, "Dupre") == 0) dupre = i;
    v.act(ShopAction::SelectMember, dupre);
    d.price = inn_pickup_price(inn_at(2), 15, 0);
    d.p("\n\n\"That will be % gold, please.\"\n\n\"");
    d.p("I hope thou hast found thy stay enjoyable,\"\n");
    d.p("says $.\n\n");
    d.p(kInnMore);
    same(v.console, d.out, "inn: a guest checks out");
    v.act(ShopAction::Confirm);
    inn_menu(d);
    v.act(ShopAction::Rest);
    d.c('R');
    d.p("\n\n");
    d.c('"');
    d.price = inn_rest_price(inn_at(2), v.g.party.party_size, 15);
    d.rec(186);
    d.p("\nWilt thou take\nit?\" ");
    same(v.console, d.out, "inn: Rest echoes 'R', the room's price, 'Wilt thou take it?'");
    v.act(ShopAction::Confirm);
    d.p("Yes");
    d.p("\n\n");
    d.p("\"Have a pleasant\nnight, ");
    d.p("sir");
    d.p("!\"\nsays $.\n\n");
    d.p("Zzzzzz....\n\n");
    d.p("Morning!\n");
    same(v.console, d.out, "inn: the night, and no farewell after it");
    check(v.session.phase == ShopPhase::Closed, "inn: the rest ends the visit");
    check(v.results.empty() && v.plain.empty(), "inn: nothing outside the transcript ('It shall be done.')");
    Visit w(6);
    w.member(1, "Iolo", 'G', 0, 0x0b);
    w.member(2, "Jaana", 'G');
    w.member(3, "Dupre", 'G', 2, 0x0b);
    w.open();
    w.act(ShopAction::Confirm);
    w.act(ShopAction::Pickup);
    auto &e = w.dos;
    e.greeting(ShopType::InnKeeper, w.draw(0));
    inn_menu(e);
    e.price = inn_pickup_price(inn_at(2), 15, 0);
    e.p("\n\n\"That will be % gold, please.\"\n\n\"");
    e.p("I hope thou hast found thy stay enjoyable,\"\n");
    e.p("says $.\n\n");
    e.p(kInnMore);
    same(w.console, e.out, "inn: one guest checks out with no register");
    w.act(ShopAction::Confirm);
    inn_menu(e);
    w.act(ShopAction::Rest);
    e.c('R');
    e.p("\n\n");
    e.c('"');
    e.price = inn_rest_price(inn_at(2), w.g.party.party_size, 15);
    e.rec(186);
    e.p("\nWilt thou take\nit?\" ");
    w.act(ShopAction::Decline);
    e.p("No");
    e.p("\n\n");
    e.farewell(ShopType::InnKeeper, true, w.draws.back());
    same(w.console, e.out, "inn: N at the room, then the farewell");
}

// --- the shipwright (SHOPPES2 0x0ABC / 0x08A8) ---
void shipwright() {
    Visit v(7);
    auto &d = v.dos;
    v.open();
    d.greeting(ShopType::Shipwright, v.draw(0));
    v.act(ShopAction::Confirm);
    d.p("Yes");
    d.rec(119);
    same(v.console, d.out, "shipwright: Y and the menu record 119");
    v.act(ShopAction::SelectItem, 0);
    d.p("F\n\n");
    d.price = ship_price(0, true, 15);
    d.rec(117);
    d.rec(126);
    same(v.console, d.out, "shipwright: F, the frigate's pitch, Wilt thou take it?");
    v.act(ShopAction::Confirm);
    d.p("Yes\n\n");
    d.rec(123);
    d.rec(124);
    d.p("sir");
    d.p("?\" ");
    same(v.console, d.out, "shipwright: sold, 'anything else, sir?', no 'She awaits thee'");
    v.act(ShopAction::Decline);
    d.p("No");
    d.farewell(ShopType::Shipwright, true, v.draws.back());
    same(v.console, d.out, "shipwright: N and the farewell");
    check(v.results.empty() && v.plain.empty(), "shipwright: nothing outside the transcript");
}

// --- the horse seller (SHOPPES 0x07BE) ---
void horses() {
    Visit v(8);
    auto &d = v.dos;
    v.open();
    d.greeting(ShopType::HorseSeller, v.draw(0));
    same(v.console, d.out, "horses: a short greeting, and the gate's single space");
    v.act(ShopAction::Confirm);
    d.p("Yes\n\n\"");
    d.price = horse_price(0, 15);
    d.rec(104);
    d.p("\n\nDeal?\" ");
    same(v.console, d.out, "horses: Y, the pitch and Deal?");
    v.act(ShopAction::Confirm);
    d.p("Yes!");
    d.farewell(ShopType::HorseSeller, true, v.draws.back());
    same(v.console, d.out, "horses: 'Yes!' and the purchase farewell");
    check(v.g.gold == 2000 - horse_price(0, 15), "horses: the price");
    check(v.results.empty() && v.plain.empty(), "horses: nothing outside the transcript");
}

// --- the default profile (the T-Deck) prints no transcript ---
void default_profile() {
    Visit v(0, false);
    v.open();
    v.act(ShopAction::Continue);
    v.act(ShopAction::Buy);
    v.act(ShopAction::SelectItem, 0);
    v.act(ShopAction::Confirm);
    v.act(ShopAction::Cancel);
    check(v.stream_events == 0 && v.console.empty(), "default: no stream Message");
    check(!v.plain.empty(), "default: the core's own lines as before");
    check(!v.results.empty(), "default: the result strings as before");
    check(v.session.dos_column == 0, "default: the column is never moved");
}

// --- UiSession joins the stream as print_string does ---
void ui_stream() {
    UiTextBlock blocks[16]{};
    UiSession ui{{blocks, 16}, {}, {22, 12, 15}};
    auto stream = [&](const char *text) {
        GameEvent e;
        e.kind = GameEventKind::Message;
        e.text = text;
        e.stream = true;
        ui.consume(e);
    };
    GameEvent plain;
    plain.kind = GameEventKind::Message;
    plain.text = "Talk-North";
    ui.consume(plain);
    stream("\"Hi there\" ");
    stream("Yes\n\n\"Fine!");
    stream("\nThy interest?\" ");
    stream("No");
    stream("\n");
    stream("next");
    UiRenderedLine lines[12];
    const auto n = ui.visible_lines(lines, 12, 22);
    std::vector<std::string> rows;
    for (size_t i = 0; i < n; ++i) rows.push_back(lines[i].text);
    const std::vector<std::string> expected{"Talk-North", "\"Hi there\" Yes", "", "\"Fine!",
                                            "Thy interest?\" No", "next"};
    check(rows == expected, "ui: stream Messages go on the console's open row");
    if (rows != expected)
        for (auto &r : rows) std::cerr << "  [" << r << "]\n";
    // An open block filled to the brim: the next stream text still continues
    // its row (the two blocks chained), it does not start one.
    UiTextBlock full_blocks[8]{};
    UiSession full{{full_blocks, 8}, {}, {22, 12, 15}};
    const std::string brim(kUiTranscriptBlockBytes - 1, 'x');
    GameEvent e;
    e.kind = GameEventKind::Message;
    e.stream = true;
    e.text = brim.c_str();
    full.consume(e);
    e.text = " No";
    full.consume(e);
    const auto *first = full.transcript_at(0), *second = full.transcript_at(1);
    check(full.transcript_size() == 2 && first && second && (first->flags & UiTextContinuesAfter) &&
              (second->flags & UiTextContinuesBefore),
          "ui: a full open block chains to the next stream text");
}
// --- the rations' count: its answer's Result does not reopen the entry ---
void ui_rations_entry() {
    UiTextBlock blocks[16]{};
    UiSession ui{{blocks, 16}, {}, {22, 12, 15}};
    ShopSession s;
    s.dos_transcript = true;
    s.type = ShopType::Barkeeper;
    s.phase = ShopPhase::RationsQuantity;
    ShopEvent state{ShopEventKind::State, &s, nullptr, nullptr};
    GameEvent e;
    e.kind = GameEventKind::Shop;
    e.shop = &state;
    ui.consume(e);
    check(ui.mode() == UiMode::NumericEntry, "ui: the rations phase opens the count");
    UiAction a;
    a.kind = UiActionKind::Character;
    a.character = u'2';
    ui.handle_input(a);
    a = {};
    a.kind = UiActionKind::Confirm;
    ui.handle_input(a);
    check(ui.mode() != UiMode::NumericEntry, "ui: RETURN takes the count");
    ShopResult bought;
    ShopEvent result{ShopEventKind::Result, &s, &bought, nullptr};
    e.shop = &result;
    ui.consume(e); // emitted before the core moves on to "Anything else"
    check(ui.mode() != UiMode::NumericEntry, "ui: the purchase's Result does not reopen the count");
    s.phase = ShopPhase::TavernAgain;
    e.shop = &state;
    ui.consume(e);
    check(ui.mode() == UiMode::Shop, "ui: 'Anything else' takes the next key");
}
} // namespace

int main() {
    console_cases();
    blacksmith_buy();
    blacksmith_menu_no();
    blacksmith_sell();
    reagents();
    guild();
    tavern();
    tavern_wine();
    tavern_rations();
    tavern_drunk();
    healer();
    inn();
    shipwright();
    horses();
    default_profile();
    ui_stream();
    ui_rations_entry();
    std::cout << "shop DOS transcript: " << (checks - failures) << "/" << checks << " checks\n";
    return failures ? 1 : 0;
}
