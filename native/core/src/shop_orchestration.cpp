#include "openu5/shop_orchestration.h"
#include "openu5/display_names.h"
#include "openu5/platform_profile.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string_view>
namespace openu5 {
namespace {
#include "shop_greetings.inc"
// The DOS strings of the console shops (DATA.OVL DS; shops.cpp compiles the
// same file), for the DOS transcript (ShopServices::dos_transcript).
namespace tables {
#include "shop_tables.inc"
}
// SHOPPES 0x06ac: the magic seller's list names (DS 0x79b4..0x7a00).
constexpr const char *kDosReagentNames[] = {"Sulfur Ash", "Ginseng", "Garlic", "Spider Silk",
                                            "Blood Moss", "Black Pearl", "Nightshade", "Mandrake"};
// SHOPPES2 0x006a: the living count's word, nothing for one (DS 0x9aa8..0x9abe).
constexpr const char *kDosAliveWords[] = {"", "", "two", "three", "four", "five", "six"};
Rand shop_rand(CommandContext &c) {
    return {&c, [](void *p, int32_t lo, int32_t hi) -> int32_t {
        auto &ctx = *static_cast<CommandContext *>(p);
        const auto value = ctx.game.rng.next(lo, hi).value;
        if (ctx.rng_trace.emit)
            ctx.rng_trace.emit(ctx.rng_trace.context, "shop", lo, hi, value);
        return value;
    }};
}
bool space(char16_t c) {
    return (c >= 9 && c <= 13) || c == 32 || c == 160 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
           c == 0x205f || c == 0x3000 || c == 0xfeff;
}
std::u16string_view trimmed(const ShopInput &input) {
    if (!input.text)
        return {};
    size_t a = 0, b = input.length;
    while (a < b && space(input.text[a]))
        ++a;
    while (b > a && space(input.text[b - 1]))
        --b;
    return {input.text + a, b - a};
}
int rumor_index(std::u16string_view text) {
    char16_t buffer[15]{};
    size_t n = std::min<size_t>(15, text.size());
    for (size_t i = 0; i < n; ++i) {
        auto c = text[i];
        buffer[i] = c >= u'A' && c <= u'Z' ? char16_t(c + 32) : c;
    }
    for (int i = 0; i < 26; ++i) {
        for (size_t at = 0; at + 4 <= n; ++at) {
            bool match = true;
            for (size_t k = 0; k < 4; ++k)
                if (buffer[at + k] != RUMOR_KEYWORDS[i][k])
                    match = false;
            if (match) {
                if (at == 0 || buffer[at - 1] == u' ')
                    return i;
                break;
            }
        }
    }
    return -1;
}
struct ShopRun {
    CommandContext &c;
    ShopServices &v;
    ShopSession &s;
    ActionResult out{};
    Rand rand;
    // ORIG-DOS2 (B-shops.md recommendation 11). The 1988 console transcript:
    // what SHOPPES*.OVL prints, laid out by DosConsole and streamed as Message
    // events (GameEvent::stream). Only with ShopSession::dos_transcript; the
    // draws, prices and rules are the shop's own, untouched.
    char console[1024]{};
    size_t console_length = 0;
    int32_t dos_price = -1; // '%' for the strings whose price the session does not hold
    bool dos() const { return s.dos_transcript; }
    void flush() {
        if (!console_length) return;
        console[console_length] = 0;
        GameEvent e;
        e.kind = GameEventKind::Message;
        e.text = console;
        e.stream = true;
        ++out.event_count;
        if (c.events.emit)
            c.events.emit(c.events.context, e);
        console_length = 0;
    }
    void lay(const char *text, char single) {
        if (console_length + (text ? 2 * std::strlen(text) : 0) + 8 >= sizeof(console)) flush();
        DosConsoleText o{console, sizeof(console), console_length};
        DosConsole k;
        k.column = s.dos_column;
        if (text) k.print(text, o);
        else k.put(single, o);
        s.dos_column = k.column;
        console_length = o.length;
    }
    // print_string of a DOS string, '$' '#' '@' '%' '^' '&' '*' expanded
    // (SHOPPES 0x0026) as for a record.
    void say(const char *raw) {
        if (!dos() || !raw) return;
        char expanded[768]{};
        expand(raw, expanded, sizeof(expanded));
        lay(expanded, 0);
    }
    void say_priced(const char *raw, int32_t price) {
        dos_price = price;
        say(raw);
        dos_price = -1;
    }
    void sayc(char ch) { // putchar
        if (dos()) lay(nullptr, ch);
    }
    // The getstring's own echo (the frontend shows what is typed): the cursor
    // moves, nothing more is printed.
    void typed(std::u16string_view text) {
        if (!dos()) return;
        char sink[8];
        DosConsole k;
        k.column = s.dos_column;
        for (auto ch : text) {
            DosConsoleText o{sink, sizeof(sink), 0};
            k.put(ch >= u' ' && ch <= u'~' ? char(ch) : '?', o);
        }
        s.dos_column = k.column;
    }
    void say_number(int32_t n) {
        char digits[16];
        std::snprintf(digits, sizeof(digits), "%ld", long(n));
        say(digits);
    }
    bool present(int i) const {
        if (i < 0) return false;
        const char *raw = v.record ? v.record(v.context, i) : nullptr;
        return raw || (v.record_present && v.record_present(v.context, i));
    }
    const char *keeper() const { return s.record && s.record->keeper ? s.record->keeper : "Merchant"; }
    // TALK 0x0108: the buyer -- the active player, else the first member who
    // is Good or Poisoned -- whose sex picks "milady" / "m'lady" (0x0C).
    bool dos_lady() const {
        const auto &p = c.game.party;
        int buyer = -1;
        if (p.active_character != 255 && p.active_character < p.character_count)
            buyer = p.active_character;
        else {
            const auto members = party_members(p);
            for (int i = 0; i < members.count && buyer < 0; ++i) {
                const auto st = p.characters[members.indices[i]].status;
                if (st == 'G' || st == 'P') buyer = members.indices[i];
            }
        }
        return buyer >= 0 && p.characters[buyer].gender == 0x0c;
    }
    // SHOPPES2 0x00ac: the tavern's "sir" / "milady" is member 0's (0x0B sir).
    bool tavern_sir() const {
        return c.game.party.character_count && c.game.party.characters[0].gender == 0x0b;
    }
    int loc() const { return s.location; }
    uint8_t intel() const {
        return c.game.party.character_count ? c.game.party.characters[0].intelligence : 15;
    }
    const char *item_name() const {
        if (s.item < 0) return "service";
        switch (s.phase) {
        case ShopPhase::Reagent:
        case ShopPhase::ReagentDeal:
        case ShopPhase::ReagentFull: return reagent_display_name(s.item);
        case ShopPhase::Guild:
        case ShopPhase::GuildDeal: return guild_item_display_name(s.item);
        case ShopPhase::Ship:
        case ShopPhase::ShipDeal:
        case ShopPhase::ShipAgain: return ship_service_display_name(s.item);
        case ShopPhase::Wine: return wine_display_name(s.item);
        // SPP1 B (P2): the rumour's '&' is its subject (DS 0x4CA8 list, SHOPPES2 0x0612).
        case ShopPhase::RumorDeal:
            return size_t(s.item) < std::size(RUMOR_SUBJECTS) ? RUMOR_SUBJECTS[s.item] : "someone";
        case ShopPhase::HealerMember:
        case ShopPhase::HealerDeal:
        case ShopPhase::InnLeaveMember:
        case ShopPhase::InnLeaveDeal:
        case ShopPhase::InnPickupMember:
            return member(s.item) ? c.game.party.characters[s.item].name : "companion";
        default: return equipment_display_name(s.item);
        }
    }
    void expand(const char *raw, char *expanded, size_t capacity, bool open_quote = false) const {
        if (!capacity) return;
        size_t at = 0;
        const char *keeper = s.record && s.record->keeper ? s.record->keeper : "Merchant";
        const char *shop = s.record && s.record->name ? s.record->name : "Shop";
        const char *day = c.game.time.hour < 12 ? "morning" : c.game.time.hour < 18 ? "afternoon" : "evening";
        const char *item = item_name();
        char price[24]{}, quantity[24]{};
        std::snprintf(price, sizeof(price), "%ld", long(dos_price >= 0 ? dos_price : s.price));
        std::snprintf(quantity, sizeof(quantity), "%ld", long(s.quantity));
        const char *place = s.item >= 0 && size_t(s.item) < std::size(RUMOR_GOSSIP_MAP)
                                ? RUMOR_PLACES[RUMOR_GOSSIP_MAP[s.item]]
                                : "Britannia";
        auto add = [&](const char *text) {
            while (text && *text && at + 1 < capacity) expanded[at++] = *text++;
        };
        if (open_quote) add("\"");
        for (const char *p = raw ? raw : ""; *p && at + 1 < capacity; ++p) {
            switch (*p) {
            case '$': add(keeper); break;
            case '#': add(shop); break;
            case '@': add(day); break;
            case '%':
                if (p[1] == 's') { add(item ? item : "item"); ++p; }
                else if (p[1] == 'd' || p[1] == 'u') { add(price); ++p; }
                else if (p[1] == '%') { expanded[at++] = '%'; ++p; }
                else add(price);
                break;
            case '&': add(item ? item : "item"); break;
            case '^': add(quantity); break;
            case '*': add(place); break;
            default: expanded[at++] = *p; break;
            }
        }
        expanded[at] = 0;
    }
    bool rec(int i, bool open_quote = false) {
        if (i < 0) return false;
        const char *raw = v.record ? v.record(v.context, i) : nullptr;
        const bool present = raw || (v.record_present && v.record_present(v.context, i));
        if (!present) return false;
        if (!raw || !*raw) return true;
        char expanded[768]{};
        if (dos()) {
            // The record is one print (SHOPPES 0x017A); its framing is the caller's.
            expand(raw, expanded, sizeof(expanded));
            lay(expanded, 0);
            return true;
        }
        expand(raw, expanded, sizeof(expanded), open_quote);
        message(expanded);return true;
    }
    bool rec_priced(int i, int32_t price) {
        dos_price = price;
        const bool shown = rec(i);
        dos_price = -1;
        return shown;
    }
    void event(ShopEventKind kind, const ShopResult *result = nullptr, const TavernResult *tavern = nullptr) {
        flush();
        ShopEvent p{kind, &s, result, tavern};
        GameEvent e;
        e.kind = GameEventKind::Shop;
        e.shop = &p;
        ++out.event_count;
        if (c.events.emit)
            c.events.emit(c.events.context, e);
    }
    void message(const char *text) {
        flush();
        GameEvent e;
        e.kind = GameEventKind::Message;
        e.text = text;
        ++out.event_count;
        if (c.events.emit)
            c.events.emit(c.events.context, e);
    }
    void phase(ShopPhase p) {
        s.phase = p;
        // Pending offers live only until the reference clears its pending* record.
        if (p != ShopPhase::BuyDeal && p != ShopPhase::SellDeal &&
            p != ShopPhase::ReagentDeal && p != ShopPhase::GuildDeal &&
            p != ShopPhase::HorseDeal && p != ShopPhase::ShipDeal &&
            p != ShopPhase::HealerDeal &&
            !(s.transactional_services && p == ShopPhase::InnRestDeal) &&
            p != ShopPhase::InnLeaveDeal && p != ShopPhase::RumorDeal &&
            !(s.transactional_services && p == ShopPhase::TavernRoundDeal) &&
            !(s.transactional_services && p == ShopPhase::TavernDrinkDeal) &&
            !(s.transactional_services && p == ShopPhase::RationsQuantity)) {
            s.item = -1;
            s.price = s.quantity = 0;
        }
        if (dos()) dos_prompt(p);
        event(ShopEventKind::State);
        out.status =
            p == ShopPhase::Closed ? CommandStatus::Success : CommandStatus::AwaitingResponse;
    }
    // The lists and questions a loop prints each time it comes round
    // (B-shops.md section 3); the variant-drawn ones are their callers'.
    void dos_prompt(ShopPhase p) {
        switch (p) {
        case ShopPhase::Reagent: {
            // SHOPPES 0x0666: 'A'+n "..." name LF per slot on sale, then
            // "\nThy interest?\" ".
            char letter = 'A';
            for (int i = 0; i < 8; ++i) {
                if (v.data.reagent_prices.at(s.town * 8 + i) <= 0) continue;
                sayc(letter++);
                say("...");
                say(kDosReagentNames[i]);
                sayc('\n');
            }
            say(tables::msg_reagentInterest);
            break;
        }
        case ShopPhase::Guild:
            // SHOPPES 0x03f6: the three fixed rows and "Thy concern?\" ".
            say(tables::msg_guildRowKeys);
            say(tables::msg_guildRowGems);
            say(tables::msg_guildRowTorches);
            say(tables::msg_guildConcern);
            break;
        case ShopPhase::Wine:
            // SHOPPES2 0x0286: the wine list (DS 0x9b76..0x9bfa).
            say(tables::msg_wineListOpen);
            say(tavern_sir() ? tables::msg_tavernSir : tables::msg_tavernMilady);
            say(tables::msg_wineListAfterName);
            for (const char *line : WINE_MENU_LINES) say(line);
            say(tables::msg_wineThyChoice);
            break;
        case ShopPhase::TavernAgain: say(tables::msg_tavernAnythingElse); break;   // SHOPPES2 0x079a
        case ShopPhase::HealerAgain: say(tables::msg_healerAnyOther); break;       // SHOPPES 0x15a8
        case ShopPhase::InnAgain: say(tables::msg_innMore); break;                 // SHOPPES3 0x099e
        case ShopPhase::HealerMember: say(tables::msg_healerWho); break;           // SHOPPES 0x1397
        case ShopPhase::InnLeaveMember: say(tables::msg_innWhoStay); break;        // SHOPPES3 0x0306
        case ShopPhase::InnPickupMember: say(tables::msg_innWhoCheckOut); break;   // SHOPPES3 0x051f
        default: break;
        }
    }
    void result(ShopResult r) {
        char expanded[768]{};
        // The DOS transcript has printed the 1988 lines; the core's own result
        // strings ("It is done.", "She awaits thee at the dock!", ...) are not.
        if (dos()) r.message = nullptr;
        if (r.message) { expand(r.message, expanded, sizeof(expanded)); r.message = expanded; }
        event(ShopEventKind::Result, &r);
    }
    void tavern_result(const TavernResult &source) {
        auto r = source;
        char expanded[768]{};
        if (dos()) r.message = nullptr;
        if (r.message) { expand(r.message, expanded, sizeof(expanded)); r.message = expanded; }
        event(ShopEventKind::Result, &r, &r);
    }
    bool names() const {
        return s.record && s.record->keeper && *s.record->keeper && s.record->name &&
               *s.record->name;
    }
    void drain(bool yes) {
        if (yes)
            shop_post_purchase_drain(c.game, c.travel.shadowlord_here, rand);
    }
    void mark(bool yes) {
        if (yes) {
            s.purchased = true;
            if (s.type == ShopType::Blacksmith && s.buying)
                s.bought_in_buy = true;
        }
    }
    // `dos_farewell` false: the 1988 visit ends with no farewell here (an inn
    // night, the lone Avatar, the morgue, the guards: SHOPPES3 passes -1 to
    // 0x0202); the core still draws its variant (B-shops S7), only the text is
    // not printed.
    void end(bool dos_farewell = true) {
        bool silent = s.thrown_out || (s.type == ShopType::Blacksmith && s.selling);
        if (!silent && names() && (v.record || v.record_present)) {
            const int variant=rand(0, 3);
            const int index=(s.purchased?SHOPPE_FAREWELL_PURCHASE:SHOPPE_FAREWELL_NO_PURCHASE)[int(s.type)][variant];
            if (!dos())
                rec(index,true);
            else if (dos_farewell && present(index)) {
                // SHOPPES 0x0202: "\n\n\"" + the record, LF unless at column 0, "says $.\n".
                say(tables::msg_farewellQuoteOpen);
                rec(index);
                if (s.dos_column) sayc('\n');
                say("says $.\n");
            }
        }
        s.phase = ShopPhase::Closed;
        s.item = -1;
        s.price = s.quantity = 0;
        event(ShopEventKind::Exited);
        out.status = CommandStatus::Success;
    }
    int first() {
        for (int i = 0; i < c.game.party.character_count; ++i)
            if (c.game.party.characters[i].party_status == 0)
                return i;
        return -1;
    }
    bool room() {
        auto inn = inn_at(loc());
        return inn.present && inn_guest_count(c.game, loc()) < inn.capacity;
    }
    bool member(int i) const { return i >= 0 && i < c.game.party.character_count; }
    bool selling_any() {
        for (int i = 0; i < c.game.equipment_count; ++i)
            if (c.game.equipment_quantities[i] > 0)
                return true;
        return false;
    }
    // The 1988 name of a blacksmith row (SHOPPES 0x0be7): the long name, or the
    // short one (DS 0x1962) when it is 13 or longer.
    const char *buy_row_name(int item) const {
        const char *name = equipment_display_name(item);
        if (name && std::strlen(name) >= 13) {
            const char *short_name = platform_dos_text(DosText::EquipmentShort, item);
            if (short_name && *short_name) name = short_name;
        }
        return name ? name : "";
    }
    // `again`: back from a pitch (SHOPPES 0x0a8e): "Anything else," and
    // "sir?" / "milady?" after a purchase this visit, "then?" if none.
    void buy_list(bool again = true) {
        if (dos() && again) {
            say(tables::msg_buyAnythingElse);
            say(s.bought_in_buy ? (dos_lady() ? tables::msg_buyMilady : tables::msg_buySir) : tables::msg_buyThen);
        }
        const int which = rand(0, 3);
        if (dos()) {
            // SHOPPES 0x0b40: "\n\n", 'a'+i "..." name and LF unless at column 0,
            // LF, WHICH and "\" " -- no prices.
            say("\n\n");
            char letter = 'a';
            for (int j = 0; j < 8; ++j) {
                const int index = s.town * 8 + j;
                if (size_t(index) >= v.data.weapons.count) break;
                const int id = v.data.weapons.at(index);
                if (id == 255) continue;
                sayc(letter++);
                say("...");
                say(buy_row_name(id));
                if (s.dos_column) sayc('\n');
            }
            sayc('\n');
            say(BLACKSMITH_BUY_WHICH[which]);
            say("\" ");
        }
        phase(ShopPhase::Buy);
    }
    // SHOPPES 0x1266: "\n\n\"" + Good-bye + "\"\n", then "says $." while
    // something is left to sell, "Thou hast nothing to sell!" when not. The
    // sell flow ends with no farewell.
    void sell_exit() {
        const int bye = rand(0, 3);
        if (dos()) {
            say("\n\n\"");
            say(BLACKSMITH_SELL_BYES[bye]);
            say(tables::msg_sellByeClose);
            say(selling_any() ? "says $.\n" : tables::msg_sellNothing);
        }
        end();
    }
    void after_sell() {
        if (!selling_any())
            sell_exit();
        else {
            phase(ShopPhase::Sell);
            const int more = rand(0, 3);
            if (dos()) {
                say("\n\n\"");
                say(BLACKSMITH_SELL_MORE[more]);
                say(tables::msg_sellQuoteClose);
                flush();
            }
        }
    }
    // SHOPPES2 0x0000: the cost helper prints two line feeds after the
    // tavern's letter (the food round, the drink and its wine list).
    void tavern_cost_lines() {
        sayc('\n');
        sayc('\n');
    }
    // SHOPPES2 0x06aa / 0x07b8: "Yes\n\n\"" + the subtype's menu record (its
    // revisit form after "Anything else"), '"', ' ', and LF past column 14.
    void tavern_menu(bool revisit) {
        if (!dos()) return;
        const int subtype = s.town >= 0 && size_t(s.town) < std::size(TAVERN_SUBTYPE) ? TAVERN_SUBTYPE[s.town] : 0;
        say(tables::msg_tavernYesOpen);
        rec((revisit ? TAVERN_MENU_REVISIT_INDEX : TAVERN_MENU_INDEX)[subtype]);
        sayc('"');
        sayc(' ');
        if (s.dos_column > 14) sayc('\n');
    }
    // SHOPPES2 0x052a: the rumour's question, from "Of what wouldst".
    void rumor_question() {
        say(tables::msg_rumorAsk);
        say(tavern_sir() ? tables::msg_tavernSir : tables::msg_tavernMilady);
        say(tables::msg_rumorRespond);
    }
    // SHOPPES 0x1526: "Yes\n\n", the powers line, "says $." and the question.
    void healer_need_text() {
        if (!dos()) return;
        char powers[96];
        std::snprintf(powers, sizeof(powers), "%s\n", tables::msg_healer);
        say(tables::msg_healerGateYes);
        say(powers);
        say(tables::msg_healerSaysNature);
    }
    // SHOPPES3 0x002c: "\n\n", then -- the inn full -- "I am sorry, sir, but
    // we have no room available."
    bool inn_room_text() {
        say("\n\n");
        if (room()) return true;
        say(tables::msg_innSorry);
        say(dos_lady() ? tables::msg_innMilady : tables::msg_innSir);
        say(tables::msg_innSorryClose);
        return false;
    }
    void tavern_again(bool served) {
        if (served) {
            s.tavern_served = true;
            mark(true);
        }
        phase(ShopPhase::TavernAgain);
    }
    void menu() {
        switch (s.type) {
        case ShopType::MagicSeller:
            phase(ShopPhase::Reagent);
            break;
        case ShopType::GuildMaster:
            phase(ShopPhase::Guild);
            break;
        case ShopType::Shipwright:
            phase(ShopPhase::Ship);
            break;
        case ShopType::Barkeeper:
            phase(ShopPhase::Tavern);
            break;
        case ShopType::Healer:
            phase(s.legacy_healer ? ShopPhase::Menu : ShopPhase::HealerNeed);
            break;
        default:
            phase(ShopPhase::Menu);
            break;
        }
    }
    bool spot(int &x, int &y) {
        if (!v.tile || !v.occupied)
            return false;
        const int dx[] = {0, 0, 1, -1}, dy[] = {1, -1, 0, 0};
        for (int i = 0; i < 4; ++i) {
            x = int(c.game.position.xy.x) + dx[i];
            y = int(c.game.position.xy.y) + dy[i];
            if (v.occupied(v.context, x, y))
                continue;
            int t = v.tile(v.context, x, y);
            if (t == 0x44 || t == 0x45 || t == 5)
                return true;
        }
        return false;
    }
    void horse() {
        int x = 0, y = 0;
        if (!v.horse || !v.reserve) {
            out.status = CommandStatus::Unsupported;
            return;
        }
        if (!spot(x, y)) {
            result({false, "The stables are closed."});
            end();
            return;
        }
        if (!v.reserve(v.context, false)) {
            out.status = CommandStatus::NeedsStorage;
            return;
        }
        auto before = c.game.gold;
        auto r = buy_horse(c.game, s.town, intel());
        result(r);
        if (!r.ok && dos()) {
            // SHOPPES 0x0932: DS 0x7a7e + 0x7a9e.
            say("\n\n\"Thou couldst not afford to ");
            say("feed it!\"\nyells $.\n");
        }
        if (r.ok) {
            drain(true);
            v.horse(v.context, x, y);
        } else if (s.phase == ShopPhase::HorseDeal)
            s.thrown_out = true;
        mark(c.game.gold < before);
        end();
    }
    void healer_member(int i) {
        if (!member(i))
            return;
        auto &ch = c.game.party.characters[i];
        bool needs = s.remedy == HealerService::Heal ? ch.status != 'D' && ch.current_hp < ch.max_hp
                     : s.remedy == HealerService::Cure ? ch.status == 'P'
                                                       : ch.status == 'D';
        if (!needs) {
            say(tables::msg_healerNoNeed); // DS 0x3d5a
            result({false, "Thou hast no need of this art!"});
            phase(ShopPhase::HealerAgain);
            return;
        }
        s.item = i;
        s.price = (s.remedy == HealerService::Heal   ? v.data.heal_prices
                   : s.remedy == HealerService::Cure ? v.data.cure_prices
                                                     : v.data.resurrect_prices)
                      .at(s.town);
        if (loc() == 5 && s.remedy != HealerService::Resurrect) {
            // SHOPPES 0x15e5 / 0x1655: Minoc's free Cure and Heal.
            say("\n\n\"");
            say(tables::msg_healerLight);
            result(healer_heal(c.game, i, s.remedy, 0));
            phase(ShopPhase::HealerAgain);
            return;
        }
        if (dos()) {
            // SHOPPES 0x1478: the remedy's pitch, then "for % gold ... pay?".
            say("\n\n\"");
            if (s.remedy == HealerService::Resurrect) {
                say("I can raise this unfortunate person from "); // DS 0x818c
                say("the dead ");                                   // DS 0x81b6
            } else
                say(s.remedy == HealerService::Cure ? tables::msg_healerCurePitch : tables::msg_healerHealPitch);
            say(tables::msg_healerPay);
        }
        phase(ShopPhase::HealerDeal);
    }
    void pickup(int i) {
        if (!member(i) || c.game.party.characters[i].party_status != loc())
            return;
        if (dos()) {
            // SHOPPES3 0x070c: no question; the bill is read out first.
            const auto buyer = first();
            if (buyer >= 0)
                say_priced(tables::msg_innPickupPay,
                           inn_pickup_price(inn_at(loc()), c.game.party.characters[buyer].intelligence,
                                            c.game.party.characters[i].months_at_inn));
        }
        auto r = inn_pickup(c.game, first(), i, loc());
        mark(r.ok);
        drain(r.ok);
        result(r);
        if (dos() && r.ok) {
            say(r.died ? tables::msg_innPickupDied : tables::msg_innPickupEnjoy);
            say(tables::msg_innPickupSays);
        }
        if (r.reason == ShopFailure::Gold) {
            if (dos()) rec(INN_PICKUP_BROKE_INDEX); // record 193: the guards (no farewell follows)
            end(false);
        } else
            phase(ShopPhase::InnAgain);
    }
    void round(bool house) {
        if (!house && (!v.tile || !v.plate)) {
            out.status = CommandStatus::Unsupported;
            return;
        }
        int x = c.game.position.xy.x, y = c.game.position.xy.y;
        auto r = buy_tavern_round(c.game, s.town, house ? 0 : v.tile(v.context, x, y - 1),
                                  house ? 0 : v.tile(v.context, x, y + 1), house);
        if (dos()) {
            // SHOPPES2 0x0000 / 0x00dc: the food round's two line feeds (the
            // drink printed its own), then '"' "That will be N gold for the
            // <two..six> of ye,\n" sir/milady '.'.
            if (!house) tavern_cost_lines();
            sayc('"');
            say(tables::msg_tavernThatWillBe);
            say_number(r.cost);
            say(tables::msg_tavernGoldForThe);
            say(r.living >= 0 && r.living <= 6 ? kDosAliveWords[r.living] : "");
            say(tables::msg_tavernOfYe);
            say(r.sir ? tables::msg_tavernSir : tables::msg_tavernMilady);
            sayc('.');
            if (r.ok)
                say(tables::msg_roundEnjoy);
            else {
                say("\"\n\n\"CAN'T PAY?\nBeat it!\"\nyells "); // DS 0x9af6
                say(keeper());
                sayc('.');
                sayc('\n');
            }
        }
        tavern_result(r);
        if (!r.ok) {
            s.thrown_out = true;
            end();
            return;
        }
        if (r.plate_dy)
            v.plate(v.context, x, y + r.plate_dy, r.plate_tile);
        if (r.counts_as_service)
            ++s.served;
        drain(true);
        tavern_again(true);
    }
    void drink() {
        if (dos()) tavern_cost_lines(); // SHOPPES2 0x026f, before the 'W' test
        if (TAVERN_SUBTYPE[s.town] == TAVERN_SUBTYPE_WINE)
            phase(ShopPhase::Wine);
        else
            round(true);
    }
    void buy_done(ShopResult r, bool fallback = false) {
        result(r);
        if (!r.ok && r.reason == ShopFailure::Gold) {
            const int broke = rand(0, 3);
            if (dos()) {
                // SHOPPES 0x0a7b: "\n\"" BROKE "\"\nyells $.\n", thrown out.
                say(tables::msg_buyBrokeOpen);
                say(BLACKSMITH_BUY_BROKE[broke]);
                say(tables::msg_buyBrokeYells);
            } else
                message(BLACKSMITH_BUY_BROKE[broke]);
            s.thrown_out = true;
            end();
            return;
        }
        if (!r.ok && r.reason == ShopFailure::Cap && !fallback) {
            say(tables::msg_buyFull);     // DS 0x7b76
            say(tables::msg_buyFullSays); // DS 0x7b9a, then any key
            phase(ShopPhase::BuyFull);
            return;
        }
        if (r.ok) say(tables::msg_buySold); // DS 0x7bb4
        drain(r.ok);
        mark(r.ok);
        buy_list();
    }
    void reagent_done(ShopResult r, bool fallback = false) {
        result(r);
        if (!r.ok && (!fallback || r.reason == ShopFailure::Gold)) {
            if(r.reason==ShopFailure::Gold)rec(MAGIC_SELLER_BROKE_INDEX);
            s.thrown_out = true;
            end();
            return;
        }
        if (r.ok) {
            say(tables::msg_reagentThanks);       // DS 0x7988
            say(tables::msg_reagentAnythingElse); // DS 0x79a2
        }
        drain(r.ok);
        mark(r.ok);
        phase(ShopPhase::Reagent);
    }
    void guild_done(ShopResult r) {
        result(r);
        if (!r.ok) {
            if(r.reason==ShopFailure::Gold)rec(GUILD_BROKE_INDEX);
            s.thrown_out = true;
            end();
            return;
        }
        // SHOPPES 0x0388: "\n\"Sold!\"\nsays $.\n\n\"What else, \n" m'lady/m'lord "?\n\n".
        say(tables::msg_guildSold);
        say(dos_lady() ? tables::msg_guildMlady : tables::msg_guildMlord);
        say(tables::msg_guildQtail);
        drain(true);
        mark(true);
        phase(ShopPhase::Guild);
    }
    // The greeting gate's 'Y' (and the healer's and the inn's "anything
    // more?"): the type's echo and the opening of its flow (B-shops.md 3.x).
    void gate_yes() {
        switch (s.type) {
        case ShopType::MagicSeller: say(tables::msg_reagentsSell); break;            // DS 0x7a2c
        case ShopType::GuildMaster: say(tables::msg_guildYes); break;                // DS 0x7916
        case ShopType::Healer: healer_need_text(); break;                            // SHOPPES 0x1526
        case ShopType::InnKeeper: say(tables::msg_innYes); say(tables::msg_innMenu); break; // SHOPPES3 0x08f8
        case ShopType::Barkeeper: tavern_menu(false); break;
        case ShopType::Shipwright: say(tables::msg_shipYes); rec(SHIP_MENU_INDEX); break; // SHOPPES2 0x0ae6
        default: break;
        }
    }
    void confirm() {
        switch (s.phase) {
        case ShopPhase::Greeting:
        case ShopPhase::HealerAgain:
        case ShopPhase::InnAgain:
            if (s.type == ShopType::HorseSeller) {
                s.price = horse_price(s.town, intel());
                if (dos() && present(HORSE_PITCH_INDEX)) say(tables::msg_horseYesOpen); // DS 0x7a62
                if (rec(HORSE_PITCH_INDEX)) {
                    say(tables::msg_sellDealPrompt); // DS 0x7a6a "\n\nDeal?\" "
                    phase(ShopPhase::HorseDeal);
                    break;
                }
                horse();
            } else {
                if (dos()) gate_yes();
                menu();
            }
            break;
        case ShopPhase::TavernAgain:
            tavern_menu(true); // SHOPPES2 0x07b8
            phase(ShopPhase::Tavern);
            break;
        case ShopPhase::ShipAgain:
            // SHOPPES2 0x0861: "Yes" (DS 0x9fdc) and the menu record again.
            say(tables::msg_shipYes);
            if (dos()) rec(SHIP_MENU_INDEX);
            phase(ShopPhase::Ship);
            break;
        case ShopPhase::Drunk:
            say(tables::msg_drunkYes); // DS 0x9b6c: no drink
            tavern_again(true);
            break;
        case ShopPhase::BuyDeal:
            say(tables::msg_buyYes); // DS 0x7b70
            buy_done(buy_equipment(c.game, s.item, s.price));
            break;
        case ShopPhase::SellDeal:
            say(tables::msg_sellDealYes); // DS 0x7d76
            result(sell_equipment(c.game, s.item, s.price));
            after_sell();
            break;
        case ShopPhase::ReagentDeal:
            say("Yes\n"); // DS 0x7982
            reagent_done(buy_reagent(c.game, s.item, s.quantity, s.price));
            break;
        case ShopPhase::GuildDeal:
            say("Yes\n"); // DS 0x789a
            guild_done(buy_guild_item(c.game, s.town, s.item, intel()));
            break;
        case ShopPhase::HorseDeal:
            say(tables::msg_horseYesExcl); // DS 0x7a78 "Yes!"
            horse();
            break;
        case ShopPhase::HealerDeal: {
            say(tables::msg_healerPayYes); // DS 0x8098
            bool paid = c.game.gold >= s.price;
            auto r = healer_heal(c.game, s.item, s.remedy,
                                 paid                           ? s.price
                                 : s.price <= 100 && loc() == 7 ? 0
                                                                : s.price);
            if (paid) {
                drain(r.ok);
                mark(r.ok);
            }
            // SHOPPES 0x14ce: no gold and no charity -- record 173.
            if (dos() && !r.ok && r.reason == ShopFailure::Gold) rec(HEALER_SORRY_INDEX);
            result(r);
            phase(ShopPhase::HealerAgain);
            break;
        }
        case ShopPhase::ShipDeal: {
            if (!v.ship || !v.reserve) {
                out.status = CommandStatus::Unsupported;
                break;
            }
            if (!v.reserve(v.context, true)) {
                out.status = CommandStatus::NeedsStorage;
                break;
            }
            say(tables::msg_shipTakeYes); // DS 0x9ff0 / 0x9ffe
            auto r = buy_ship(c.game, s.town, s.item == 0, intel());
            result(r);
            if (!r.ok) {
                if (dos()) {
                    rec(SHIP_CHEAT_INDEX);        // record 122
                    say(tables::msg_shipYells);   // DS 0x9fc2
                }
                s.thrown_out = true;
                end();
                break;
            }
            mark(true);
            v.ship(v.context, r.x, r.y, s.item == 0 ? 0x125 : 0x129, 99, s.item == 0 ? 2 : 0);
            if (dos()) {
                // SHOPPES2 0x080e: record 123, then record 124 + "sir" (0x085a's
                // test never holds) + "?\" ".
                rec(SHIP_SOLD_INDEX);
                rec(SHIP_ELSE_INDEX);
                say(tables::msg_innSir);
                say(tables::msg_shipElseClose);
            }
            phase(ShopPhase::ShipAgain);
            break;
        }
        case ShopPhase::InnLeaveDeal: {
            say(tables::msg_innYes);
            say("\n\n"); // DS 0x4f3a
            auto r = inn_leave(c.game, s.item, loc());
            result(r);
            if (dos() && r.ok) {
                say(tables::msg_innThank); // DS 0x4f3d
                // SHOPPES3 0x03d8: a companion left is the visit's purchase
                // (the farewell's table, B-shops S11) -- in the 1988 text only.
                s.purchased = true;
            }
            phase(ShopPhase::InnAgain);
            break;
        }
        case ShopPhase::InnRestDeal: {
            if (!v.hour_tiles || !v.wake_npcs) {
                out.status = CommandStatus::Unsupported;
                break;
            }
            say(tables::msg_innYes);
            say("\n\n"); // DS 0x4de0
            bool poisoned[16]{};
            for (int i = 0; i < c.game.party.character_count && i < 16; ++i)
                poisoned[i] = c.game.party.characters[i].status == 'P';
            const int buyer = first();
            const bool broke = buyer >= 0 && room() &&
                               c.game.gold < inn_rest_price(inn_at(loc()), c.game.party.party_size,
                                                            c.game.party.characters[buyer].intelligence);
            auto r = inn_rest(c.game, buyer, loc());
            mark(r.ok);
            drain(r.ok);
            result(r);
            if (dos() && broke) {
                say(tables::msg_innBroke);        // DS 0x4de3
                say(tables::msg_innBrokeScreams); // DS 0x4e07
            }
            if (dos() && r.ok) {
                // SHOPPES3 0x0127..0x0296.
                say(tables::msg_innPleasant);
                say(dos_lady() ? tables::msg_innMilady : tables::msg_innSir);
                say(tables::msg_innPleasantClose);
                say(tables::msg_innZzz);
            }
            if (r.ok) {
                c.game.position.xy = {uint8_t(r.x), uint8_t(r.y)};
                inn_night_pass(c.game, c.turn, rand, {v.context, v.hour_tiles}, c.sky);
                v.wake_npcs(v.context);
                ++c.game.position.xy.x;
            }
            if (dos() && r.ok) {
                say(tables::msg_innMorning);
                const auto members = party_members(c.game.party);
                for (int k = 0; k < members.count; ++k) {
                    const int i = members.indices[k];
                    if (i < 16 && poisoned[i] && c.game.party.characters[i].status == 'D') {
                        sayc('\n');
                        say(c.game.party.characters[i].name);
                        say(tables::msg_innPassedAway);
                    }
                }
            }
            end(!r.ok); // the morning ends the visit with no farewell (SHOPPES3 0x0958)
            break;
        }
        case ShopPhase::TavernRoundDeal:
            round(false);
            break;
        case ShopPhase::TavernDrinkDeal:
            round(true);
            break;
        case ShopPhase::RumorDeal: {
            say(tables::msg_rumorYes); // DS 0x9f72
            auto r = pay_rumor(c.game, s.item);
            result(r);
            if (r.ok) {
                mark(true);
                // SPP1 B (P2). SHOPPES2 0x0612-0x0659: the gossip in the form drawn here --
                // records 85..88, '&' the subject, '*' the place -- then "says " + keeper + "."
                // (DS 0x9F82 / 0x9F8A). The draw was always made; the text was never shown by
                // any front end.
                if (v.record_present) {
                    const int form = rand(0, 3);
                    rec(85 + form);
                    if (dos()) {
                        say(tables::msg_rumorSays);
                        say(keeper());
                        say(tables::msg_rumorSaysClose);
                    } else {
                        char says[96]{};
                        std::snprintf(says, sizeof(says), "says %s.", keeper());
                        message(says);
                    }
                }
            } else if (dos()) {
                // SHOPPES2 0x05f6: "\"Sorry, " sir/milady + record 91. (The 1988
                // visit ends here, B-shops S15; the core's goes on.)
                say(tables::msg_rumorSorry);
                say(tavern_sir() ? tables::msg_tavernSir : tables::msg_tavernMilady);
                rec(RUMOR_BROKE_INDEX);
            }
            tavern_again(true);
            break;
        }
        case ShopPhase::Menu:
            if (s.type == ShopType::HorseSeller)
                horse();
            break;
        default:
            break;
        }
    }
    void decline() {
        switch (s.phase) {
        case ShopPhase::Menu:
            if (s.type == ShopType::HorseSeller) end();
            break;
        case ShopPhase::BuyDeal:
            say(tables::msg_buyNo); // DS 0x7b6a
            buy_list();
            break;
        case ShopPhase::SellDeal:
            say("No"); // DS 0x7d72
            after_sell();
            break;
        case ShopPhase::ReagentDeal:
            say(tables::msg_reagentDealNo); // DS 0x7970
            phase(ShopPhase::Reagent);
            break;
        case ShopPhase::GuildDeal:
            say(tables::msg_guildDealNo); // DS 0x7882
            phase(ShopPhase::Guild);
            break;
        case ShopPhase::HealerDeal:
            say("No"); // DS 0x809c
            phase(ShopPhase::HealerAgain);
            break;
        case ShopPhase::InnLeaveDeal:
            say("No");
            say("\n\n"); // DS 0x4f3a
            phase(ShopPhase::InnAgain);
            break;
        case ShopPhase::RumorDeal:
            say(tables::msg_rumorNo); // DS 0x9f6c
            tavern_again(true);
            break;
        case ShopPhase::TavernRoundDeal:
        case ShopPhase::TavernDrinkDeal:
            phase(ShopPhase::Tavern);
            break;
        case ShopPhase::Drunk:
            say(tables::msg_drunkNo); // DS 0x9b72 "No!"
            c.turn.drunk_turns = DRUNK_TIMER_TURNS;
            if (c.game.karma)
                --c.game.karma;
            drink();
            break;
        case ShopPhase::InnRestDeal:
            say("No");
            say("\n\n"); // DS 0x4de0
            end();
            break;
        case ShopPhase::Greeting:
        case ShopPhase::HealerAgain:
        case ShopPhase::InnAgain:
        case ShopPhase::TavernAgain:
        case ShopPhase::HorseDeal:
        case ShopPhase::ShipDeal:
        case ShopPhase::ShipAgain:
            say("No"); // every gate's own "No" (DS 0x7a44, 0x7928, 0x80a0, 0x505f, 0x9fb6, ...)
            end();
            break;
        default:
            break;
        }
    }
    // SHOPPES 0x12c6: after any key, "\n$ says,\n\"" + ASK[rand(0,1)] + "\" ".
    void blacksmith_ask() {
        const int variant = rand(0, 1);
        if (dos()) {
            say(tables::msg_blacksmithSays);
            say(variant ? "Greetings, traveller! Wish ye to Buy, or hast thou wares to Sell?" // DS 0x7f70
                        : "Hail, friend! Wouldst thou Buy or Sell?");                         // DS 0x7f48
            say("\" ");                                                                       // DS 0x8042
        } else {
            char line[256]{};
            std::snprintf(line, sizeof(line), "%s says,\n\"%s", keeper(),
                          variant ? "Greetings, traveller! Wish ye to Buy, or hast thou wares to Sell?\""
                                  : "Hail, friend! Wouldst thou Buy or Sell?\"");
            message(line);
        }
        phase(ShopPhase::Menu);
    }
    void select(int i) {
        switch (s.phase) {
        case ShopPhase::Buy: {
            bool stock = false;
            char letter = 'a';
            for (int j = 0; j < 8; ++j) {
                if (size_t(s.town * 8 + j) >= v.data.weapons.count) continue;
                const int id = v.data.weapons.at(s.town * 8 + j);
                if (id == i && i != 255) stock = true;
                if (!stock && id != 255) ++letter;
            }
            if (!stock || i < 0 || i >= 256)
                return;
            s.item = i;
            s.price = shop_buy_price(v.data.equipment_prices.at(i), intel());
            const int pitch = i < int(std::size(BLACKSMITH_BUY_PITCH_INDEX)) ? BLACKSMITH_BUY_PITCH_INDEX[i] : -1;
            // SHOPPES 0x0b9c / 0x09ac: the lower-case letter, "\n\n\"", the pitch,
            // "\n\n", ASKS and "\" ".
            sayc(letter);
            if (present(pitch)) say("\n\n\"");
            if (pitch < 0 || !rec(pitch))
                buy_done(buy_equipment(c.game, i, s.price), true);
            else {
                const int ask = rand(0, 3);
                if (dos()) {
                    say("\n\n");
                    say(BLACKSMITH_BUY_ASKS[ask]);
                    say("\" ");
                }
                phase(ShopPhase::BuyDeal);
            }
            break;
        }
        case ShopPhase::Sell: {
            if (i < 0 || i >= c.game.equipment_count || c.game.equipment_quantities[i] <= 0)
                return;
            if (i == 0x1b || i == 0x1d) {
                say(tables::msg_sellAmmo); // DS 0x7d32: no good-bye follows
                result({false, ""});
                end();
                return;
            }
            say("\n\n\""); // DS 0x7d64
            if (v.data.equipment_prices.at(i) <= 0) {
                say(tables::msg_sellCannotBuy); // DS 0x7d8c
                result({false, ""});
                after_sell();
                return;
            }
            s.item = i;
            s.price = shop_sell_price(v.data.equipment_prices.at(i), intel());
            bool pitch = v.record_present && rec(BLACKSMITH_SELL_OFFER_INDEX[rand(0, 7)]);
            if (pitch) {
                say(tables::msg_sellDealPrompt); // DS 0x7d68
                phase(ShopPhase::SellDeal);
            } else {
                result(sell_equipment(c.game, i, s.price));
                after_sell();
            }
            break;
        }
        case ShopPhase::Reagent:
            if (i < 0 || i >= 8 || v.data.reagent_prices.at(s.town * 8 + i) <= 0)
                return;
            s.item = i;
            s.quantity = v.data.reagent_grants.at(s.town * 8 + i);
            s.price = shop_buy_price(v.data.reagent_prices.at(s.town * 8 + i), intel());
            if (dos() && c.game.reagent_quantities[i] >= 99 && present(MAGIC_SELLER_PITCH_INDEX[i])) {
                // SHOPPES 0x0502: at 99 the cap line comes first -- no pitch,
                // no price (B-shops P3) -- then any key.
                say(tables::msg_reagentFull);
                phase(ShopPhase::ReagentFull);
                break;
            }
            if (present(MAGIC_SELLER_PITCH_INDEX[i])) say("\n\n\""); // DS 0x7952
            if (!rec(MAGIC_SELLER_PITCH_INDEX[i]))
                reagent_done(buy_reagent(c.game, i, s.quantity, s.price), true);
            else if (c.game.reagent_quantities[i] >= 99)
                phase(ShopPhase::ReagentFull);
            else {
                if (dos()) {
                    say(tables::msg_reagentNeed); // DS 0x7956
                    if (s.dos_column > 12) say("\n:"); // DS 0x796c
                }
                phase(ShopPhase::ReagentDeal);
            }
            break;
        case ShopPhase::Guild:
            if (i == 3 && rec(GUILD_EASTEREGG_INDEX)) { // record 164 carries its own 'd'
                phase(ShopPhase::Guild);
                return;
            }
            if (i < 0 || i >= 3)
                return;
            s.item = i;
            s.price = guild_price(s.town, i, intel());
            sayc(char('a' + i)); // SHOPPES 0x0440: the lower-case echo
            if (present(GUILD_PITCH_INDEX[i])) say("\n\n\""); // DS 0x786e
            if (rec(GUILD_PITCH_INDEX[i])) {
                say(tables::msg_guildInterested); // DS 0x7872
                phase(ShopPhase::GuildDeal);
            } else
                guild_done(buy_guild_item(c.game, s.town, i, intel()));
            break;
        case ShopPhase::Ship:
            if (i < 0 || i > 1)
                return;
            s.item = i;
            s.price = ship_price(s.town, i == 0, intel());
            if (dos()) {
                // SHOPPES2 0x08f0 / 0x09d4: "F\n\n" / "S\n\n", the pitch, record 126.
                say(i == 0 ? tables::msg_shipEchoF : tables::msg_shipEchoS);
                rec(i == 0 ? SHIP_PITCH_FRIGATE_INDEX : SHIP_PITCH_SKIFF_INDEX);
                rec(SHIP_TAKE_INDEX);
            }
            phase(ShopPhase::ShipDeal);
            break;
        case ShopPhase::Wine: {
            if (i < 0 || i >= 6)
                return;
            if (dos()) {
                // SHOPPES2 0x0302: the key, "\n\n\"Ah, a fine\nchoice, " sir '.'.
                sayc(char('A' + i));
                say(tables::msg_wineFineChoice);
                say(tavern_sir() ? tables::msg_tavernSir : tables::msg_tavernMilady);
                sayc('.');
            }
            auto r = buy_wine(c.game, i);
            result(r);
            if (!r.ok) {
                if (dos()) {
                    say(tables::msg_wineCantPay); // DS 0x9c20
                    say(keeper());
                    sayc('.');
                    sayc('\n');
                }
                s.thrown_out = true;
                end();
                return;
            }
            if (dos()) {
                say(tables::msg_wineEnjoy); // DS 0x9c40
                sayc('\n');
                sayc('\n');
            }
            mark(true);
            ++s.served;
            drain(true);
            tavern_again(true);
            break;
        }
        default:
            break;
        }
    }
};
} // namespace
ActionResult begin_shop(CommandContext &c, const NpcActor &npc) {
    ActionResult result;
    if (!c.shop_services || c.combat || c.dungeon) {
        result.status = CommandStatus::InvalidContext;
        return result;
    }
    auto &v = *c.shop_services;
    auto &s = v.session;
    if (s.phase != ShopPhase::Closed) {
        result.status = CommandStatus::AwaitingResponse;
        return result;
    }
    if (npc.schedule.dialog < 0x81 || npc.schedule.dialog > 0x88) {
        result.status = CommandStatus::Rejected;
        return result;
    }
    ShopRun r{c, v, s, {}, shop_rand(c)};
    if (!shop_is_open(npc.schedule.times, uint8_t(c.game.time.hour))) {
        r.message("A merchant says:\n\"Come see me at\nmy shoppe, when\nit's open!\"\n");
        r.out.status = CommandStatus::Rejected;
        return r.out;
    }
    auto type = ShopType(npc.schedule.dialog - 0x81);
    if ((c.turn.transport_tile & 0xfc) == 0x10 && type != ShopType::HorseSeller) {
        r.message("A merchant says:\n\"GET THAT HORSE OUT OF HERE!\"\n");
        r.out.status = CommandStatus::Rejected;
        return r.out;
    }
    auto town = shop_town_index(type, c.game.position.map.location);
    if (town < 0) {
        result.status = CommandStatus::Rejected;
        return result;
    }
    s = {};
    s.owner = &c.game;
    s.type = type;
    s.transactional_services = v.transactional_services;
    s.dos_transcript = v.dos_transcript;
    s.location = c.game.position.map.location;
    s.town = town;
    s.record = shop_lookup(v.data, s.location, type);
    if (type == ShopType::HorseSeller) {
        int x = 0, y = 0;
        if (!v.tile || !v.occupied) {
            r.out.status = CommandStatus::Unsupported;
            return r.out;
        }
        if (!r.spot(x, y)) {
            r.message("The stables are closed.\n");
            return r.out;
        }
    }
    r.event(ShopEventKind::Entered);
    bool greeted = r.names();
    if (greeted && type != ShopType::Blacksmith) {
        if (v.record || v.record_present) {
            const int index = SHOPPE_GREETING_INDEX[int(type)][r.rand(0, 3)];
            if (r.dos() && r.present(index)) r.sayc('"'); // SHOPPES 0x01b7
            greeted = r.rec(index, true);
            if (greeted && r.dos()) {
                // 0x01c6: the gate's spacing by the cursor (DS 0x784c / 0x7850).
                if (s.dos_column > 11) r.say("\n\n:");
                else if (!s.dos_column) r.say("\n:");
                else r.sayc(' ');
            }
        } else
            greeted = false;
    }
    if (greeted && type == ShopType::Blacksmith) {
        if (r.dos()) {
            char welcome[64];
            std::snprintf(welcome, sizeof(welcome), "%s\n", tables::msg_blacksmithWelcome); // DS 0x8018
            r.say(welcome);
        } else {
            const char *day=c.game.time.hour<12?"morning":c.game.time.hour<18?"afternoon":"evening";
            char greeting[192]{};std::snprintf(greeting,sizeof(greeting),"\"Good %s, and welcome to %s!\"",day,s.record->name);
            r.message(greeting);
        }
    }
    s.legacy_healer = !greeted && type == ShopType::Healer;
    if (greeted)
        r.phase(type == ShopType::Blacksmith ? ShopPhase::BlacksmithPause : ShopPhase::Greeting);
    else
        r.menu();
    r.flush();
    return r.out;
}
ActionResult execute_shop(CommandContext &c, ShopInput input) {
    ActionResult error;
    error.status = CommandStatus::InvalidContext;
    if (!c.shop_services)
        return error;
    auto &v = *c.shop_services;
    auto &s = v.session;
    if (s.owner != &c.game || s.phase == ShopPhase::Closed || c.combat || c.dungeon)
        return error;
    ShopRun r{c, v, s, {}, shop_rand(c)};
    r.out.status = CommandStatus::AwaitingResponse;
    if (input.action == ShopAction::Confirm || input.action == ShopAction::Decline) {
        if (s.phase == ShopPhase::BlacksmithPause) {
            r.blacksmith_ask();
            return r.out;
        }
        if (s.phase == ShopPhase::BuyFull) {
            r.buy_list();
            return r.out;
        }
        if (s.phase == ShopPhase::ReagentFull) {
            r.phase(ShopPhase::Reagent);
            return r.out;
        }
    }
    auto &g = c.game;
    switch (input.action) {
    case ShopAction::End:
        if (s.phase == ShopPhase::Menu && s.type == ShopType::Blacksmith)
            r.say("No"); // DS 0x8056: Space at "Buy or Sell?"
        else if (s.phase == ShopPhase::HealerNeed)
            r.say("Nothing"); // DS 0x81c0
        r.end();
        break;
    case ShopAction::Confirm:
        r.confirm();
        break;
    case ShopAction::Decline:
        r.decline();
        break;
    case ShopAction::Continue:
        if (s.phase == ShopPhase::BlacksmithPause) {
            r.blacksmith_ask();
        } else if (s.phase == ShopPhase::BuyFull)
            r.buy_list();
        else if (s.phase == ShopPhase::ReagentFull)
            r.phase(ShopPhase::Reagent);
        else if (s.phase == ShopPhase::Reagent || s.phase == ShopPhase::HealerNeed) {
            if (s.phase == ShopPhase::HealerNeed) r.say("Nothing"); // DS 0x81c0
            r.end();
        }
        break;
    case ShopAction::Buy:
        if (s.phase == ShopPhase::Menu && s.type == ShopType::Blacksmith) {
            s.buying = true;
            s.selling = false;
            s.bought_in_buy = false;
            s.thrown_out = false;
            // SHOPPES 0x1306: "Buy\n\n\"" + EXCL[r] + INTRO[r], then the list.
            r.say(tables::msg_buyEcho);
            const int exclaim = r.rand(0, 3), intro = r.rand(0, 3);
            r.say(BLACKSMITH_BUY_EXCLAIMS[exclaim]);
            r.say(BLACKSMITH_BUY_INTROS[intro]);
            r.buy_list(false);
        }
        break;
    case ShopAction::Sell:
        if (s.phase == ShopPhase::Menu && s.type == ShopType::Blacksmith) {
            s.selling = true;
            s.buying = false;
            r.say(tables::msg_sellEcho); // DS 0x804e
            if (!r.selling_any()) {
                r.say(tables::msg_sellNothing); // DS 0x7f20
                r.end();
            } else {
                const int prompt = r.rand(0, 3);
                r.say(BLACKSMITH_SELL_PROMPTS[prompt]);
                r.say(tables::msg_sellQuoteClose); // DS 0x7ef0, then the "Arms" window
                r.phase(ShopPhase::Sell);
            }
        }
        break;
    case ShopAction::SelectItem:
        r.select(input.value);
        break;
    case ShopAction::Heal:
    case ShopAction::Cure:
    case ShopAction::Resurrect:
        if (s.phase != ShopPhase::HealerNeed &&
            !(s.phase == ShopPhase::Menu && s.type == ShopType::Healer))
            break;
        s.remedy = input.action == ShopAction::Heal   ? HealerService::Heal
                   : input.action == ShopAction::Cure ? HealerService::Cure
                                                      : HealerService::Resurrect;
        if (s.legacy_healer) {
            r.phase(ShopPhase(int(ShopPhase::LegacyHeal) + int(s.remedy)));
            break;
        }
        // SHOPPES 0x1596 / 0x1622 / 0x169a: the remedy's echo.
        r.say(s.remedy == HealerService::Heal ? "Healing" : s.remedy == HealerService::Cure ? "Curing" : "Resurrect");
        if (g.party.party_size <= 1)
            r.healer_member(0);
        else
            r.phase(ShopPhase::HealerMember);
        break;
    case ShopAction::SelectMember:
        if (s.phase >= ShopPhase::LegacyHeal && s.phase <= ShopPhase::LegacyResurrect) {
            auto offered = party_members(g.party);
            bool found = false;
            for (int i = 0; i < offered.count; ++i)
                if (offered.indices[i] == input.value)
                    found = true;
            if (!found)
                break;
            auto price = (s.remedy == HealerService::Heal   ? v.data.heal_prices
                          : s.remedy == HealerService::Cure ? v.data.cure_prices
                                                            : v.data.resurrect_prices)
                             .at(s.town);
            auto tx = healer_heal(g, input.value, s.remedy, price);
            if (!tx.ok && tx.reason == ShopFailure::Gold) {
                if (price <= 100 && s.location == 7)
                    tx = healer_heal(g, input.value, s.remedy, 0);
                else {
                    r.result(tx);
                    r.menu();
                    break;
                }
            }
            r.drain(tx.ok);
            r.mark(tx.ok);
            r.result(tx);
            r.menu();
        } else if (s.phase == ShopPhase::HealerMember && r.member(input.value) &&
                   g.party.characters[input.value].party_status == 0)
            r.healer_member(input.value);
        else if (s.phase == ShopPhase::InnPickupMember)
            r.pickup(input.value);
        else if (s.phase == ShopPhase::InnLeaveMember && r.member(input.value) &&
                 g.party.characters[input.value].party_status == 0) {
            r.say("\n\n"); // DS 0x4ea9
            if (input.value == 0) {
                // SHOPPES3 0x0335: and the question again.
                r.say("Thy friend");
                if (g.party.party_size > 2) r.sayc('s');
                r.say(" will not leave thee!\n\n");
                r.phase(ShopPhase::InnLeaveMember);
            } else if (g.party.characters[input.value].status == 'D') {
                if (r.dos()) r.rec(INN_CORPSE_INDEX); // record 192, no farewell follows
                r.end(false);
            } else {
                s.item = input.value;
                if (r.dos()) {
                    // SHOPPES3 0x03ad: the rate line, then "Wilt thou take it?".
                    const auto buyer = r.first();
                    r.say(tables::msg_innRateOpen);
                    r.say_priced(tables::msg_innRateMonthly,
                                 buyer >= 0 ? inn_monthly_rate(inn_at(s.location), g.party.characters[buyer].intelligence) : 0);
                    r.say(tables::msg_innWiltTake);
                }
                r.phase(ShopPhase::InnLeaveDeal);
            }
        }
        break;
    case ShopAction::Rest:
    case ShopAction::LeaveMember:
    case ShopAction::Pickup:
        if (s.phase != ShopPhase::Menu || s.type != ShopType::InnKeeper)
            break;
        if (input.action == ShopAction::Pickup) {
            if (g.party.party_size >= 6 || inn_guest_count(g, s.location) == 0) {
                // SHOPPES3 0x04f4 / 0x0506 (Pick up has no echo).
                r.say(g.party.party_size >= 6 ? tables::msg_innPickupFull : tables::msg_innPickupNobody);
                r.phase(ShopPhase::InnAgain);
            } else if (inn_guest_count(g, s.location) == 1) {
                for (int i = 0; i < g.party.character_count; ++i)
                    if (g.party.characters[i].party_status == s.location) {
                        r.pickup(i);
                        break;
                    }
            } else
                r.phase(ShopPhase::InnPickupMember);
        } else if (r.sayc(input.action == ShopAction::Rest ? 'R' : 'L'), r.dos() ? !r.inn_room_text() : !r.room())
            r.phase(ShopPhase::InnAgain);
        else if (input.action == ShopAction::Rest) {
            if (s.transactional_services) {
                const auto inn = inn_at(s.location);
                const auto buyer = r.first();
                s.item = 0;
                s.quantity = 1;
                s.price = buyer >= 0 ? inn_rest_price(inn, g.party.party_size,
                                                      g.party.characters[buyer].intelligence)
                                     : 0;
            }
            if (r.dos()) {
                // SHOPPES3 0x008e: '"' + the inn's record (186..190, '%' the night
                // for the party) + "\nWilt thou take\nit?\" ".
                const auto buyer = r.first();
                r.sayc('"');
                r.rec_priced(INN_PITCH_INDEX[std::clamp<int32_t>(s.town, 0, int32_t(std::size(INN_PITCH_INDEX)) - 1)],
                             buyer >= 0 ? inn_rest_price(inn_at(s.location), g.party.party_size,
                                                         g.party.characters[buyer].intelligence)
                                        : 0);
                r.say(tables::msg_innWiltTake);
            }
            r.phase(ShopPhase::InnRestDeal);
        } else if (g.party.party_size <= 1) {
            if (r.dos()) r.rec(INN_ALONE_INDEX); // record 191, no farewell follows
            r.end(false);
        } else
            r.phase(ShopPhase::InnLeaveMember);
        break;
    case ShopAction::Round:
        if (s.phase == ShopPhase::Tavern) {
            r.sayc(tavern_menu_letter(s.town, 0)); // SHOPPES2 0x01d8
            if (s.transactional_services) {
                s.item = 0;
                s.quantity = 1;
                s.price = tavern_round_price(g, s.town, false);
                r.phase(ShopPhase::TavernRoundDeal);
            } else
                r.round(false);
        }
        break;
    case ShopAction::Drink:
        if (s.phase == ShopPhase::Tavern) {
            r.sayc(tavern_menu_letter(s.town, 1)); // SHOPPES2 0x0200
            if (s.served == DRUNK_CUP_GATE) {
                // SHOPPES2 0x0211: "I beg thy pardon ... had enough to drink?"
                r.say(tables::msg_drunkPardon);
                r.say(r.tavern_sir() ? tables::msg_tavernSir : tables::msg_tavernMilady);
                r.say(tables::msg_drunkSays);
                r.say(r.keeper());
                r.say(tables::msg_drunkEnough);
                r.phase(ShopPhase::Drunk);
            }
            else if (TAVERN_SUBTYPE[s.town] == TAVERN_SUBTYPE_WINE)
                r.drink();
            else if (s.transactional_services) {
                s.item = 1;
                s.quantity = 1;
                s.price = tavern_round_price(g, s.town, true);
                r.phase(ShopPhase::TavernDrinkDeal);
            } else
                r.drink();
        }
        break;
    case ShopAction::Rations:
        if (s.phase == ShopPhase::Tavern &&
            (TAVERN_SUBTYPE[s.town] == 0 || TAVERN_SUBTYPE[s.town] == 3)) {
            // SHOPPES2 0x0380: the letter, two line feeds, '"' + the pitch
            // (77..83, '%' the price) + "How many wouldst thou like?".
            r.sayc(tavern_menu_letter(s.town, 2));
            r.sayc('\n');
            r.sayc('\n');
            const int pitch = r.rand(0, 6);
            if (s.transactional_services) {
                s.item = 2;
                s.price = ration_price(s.town, r.intel());
                s.quantity = 25;
            }
            if (r.dos()) {
                r.sayc('"');
                r.rec_priced(TAVERN_RATIONS_PITCH_INDEX[pitch], ration_price(s.town, r.intel()));
                r.say(tables::msg_tavernHowMany);
            }
            r.phase(ShopPhase::RationsQuantity);
        }
        break;
    case ShopAction::Rumor:
        if (s.phase == ShopPhase::Tavern && s.tavern_served) {
            r.sayc(tavern_menu_letter(s.town, 3)); // SHOPPES2 0x051d
            r.say(tables::msg_rumorAskOpen);       // DS 0x9efc
            r.rumor_question();
            r.phase(ShopPhase::RumorText);
        }
        break;
    case ShopAction::Text: {
        if (!input.text && input.length) {
            r.out.status = CommandStatus::Rejected;
            break;
        }
        auto text = trimmed(input);
        if (r.dos() && (s.phase == ShopPhase::RumorText || s.phase == ShopPhase::RationsQuantity))
            r.typed({input.text, input.text ? input.length : 0});
        if (s.phase == ShopPhase::RumorText) {
            r.say("\n\n"); // DS 0x9f36
            if (text.empty()) {
                r.tavern_again(true);
                break;
            }
            int i = rumor_index(text);
            if (i < 0) {
                // SHOPPES2 0x0591: and the question again, from "Of what wouldst".
                r.say(tables::msg_rumorUnknown);
                r.rumor_question();
                r.phase(ShopPhase::RumorText);
                break;
            }
            s.item = i;
            s.price = RUMOR_PRICES[i];
            if (r.dos()) {
                r.rec(RUMOR_PRICE_PITCH_INDEX);  // record 84, '%' the price
                r.say(tables::msg_rumorFairNuff); // DS 0x9f5c
            }
            r.phase(ShopPhase::RumorDeal);
        } else if (s.phase == ShopPhase::RationsQuantity) {
            size_t i = 0;
            bool negative = false;
            if (!text.empty() && (text[0] == u'+' || text[0] == u'-')) {
                negative = text[0] == u'-';
                ++i;
            }
            int64_t qty = 0;
            while (i < text.size() && text[i] >= u'0' && text[i] <= u'9') {
                qty = std::min<int64_t>(2147483647, qty * 10 + text[i++] - u'0');
            }
            if (negative || qty == 0) {
                r.say(tables::msg_tavernHrumph); // DS 0x9c6a
                r.result({false, "Hrumph."});
                r.tavern_again(false);
                break;
            }
            auto tx = buy_rations(g, s.town, r.intel(), int32_t(qty));
            r.result(tx);
            r.say("\n\n"); // DS 0x9c76 / 0x9cd0
            if (tx.bought < qty && !tx.full) {
                if (tx.bought == 0) {
                    const bool scraps = g.food < 3;
                    if (g.food < 3)
                        g.food = uint16_t(g.food + r.rand(0, 1) + 1);
                    if (r.dos() && scraps)
                        r.rec(TAVERN_SCRAPS_INDEX); // record 90
                    else if (r.dos()) {
                        r.say(tables::msg_tavernNoNeed);
                        r.say(tables::msg_tavernYells);
                        r.say(r.keeper());
                        r.say(".\n"); // DS 0x9cac
                    }
                    s.thrown_out = true;
                    r.end();
                } else {
                    // SHOPPES2 0x04d6: "Thou canst afford only N!" (no drain).
                    r.say(tables::msg_tavernAffordOnly);
                    r.say_number(tx.bought);
                    r.say(tables::msg_tavernAffordClose);
                    r.tavern_again(true);
                }
            } else {
                r.drain(true);
                r.tavern_again(true);
            }
        }
        break;
    }
    case ShopAction::Cancel:
        if (s.phase == ShopPhase::RumorText) {
            r.say("\n\n"); // as an empty answer
            r.tavern_again(true);
        } else if (s.phase == ShopPhase::RationsQuantity) {
            r.say(tables::msg_tavernHrumph); // as no rations
            r.tavern_again(false);
        } else if (s.phase == ShopPhase::HealerMember) {
            r.say("No one"); // DS 0x8072
            r.phase(ShopPhase::HealerAgain);
        } else if (s.phase == ShopPhase::InnLeaveMember || s.phase == ShopPhase::InnPickupMember) {
            r.say(s.phase == ShopPhase::InnLeaveMember ? tables::msg_innNobody : tables::msg_innPickupNoOne);
            r.phase(ShopPhase::InnAgain);
        } else if (s.phase == ShopPhase::Sell)
            r.sell_exit();
        else if (s.phase == ShopPhase::Wine) {
            // SHOPPES2 0x02d2: Space at the wine list -- two line feeds and
            // record 89 (nothing is served).
            r.sayc('\n');
            r.sayc('\n');
            if (r.dos()) r.rec(TAVERN_MEAGRE_INDEX);
            r.tavern_again(false);
        }
        else if (s.phase == ShopPhase::BlacksmithPause) {
            r.blacksmith_ask();
        } else if (s.phase == ShopPhase::BuyFull)
            r.buy_list();
        else if (s.phase == ShopPhase::ReagentFull)
            r.phase(ShopPhase::Reagent);
        else if (s.phase == ShopPhase::Menu || s.phase == ShopPhase::Buy ||
                 s.phase == ShopPhase::Reagent || s.phase == ShopPhase::Guild ||
                 s.phase == ShopPhase::Ship || s.phase == ShopPhase::Tavern ||
                 s.phase == ShopPhase::HealerNeed || s.phase >= ShopPhase::LegacyHeal)
            r.end();
        break;
    }
    r.flush();
    return r.out;
}
} // namespace openu5

namespace openu5 {
// putchar 0x16ba: a cell, the column, and the wrap past the right edge
// (0x1735-0x1745); a line feed or carriage return returns to column 0.
void DosConsole::put(char c, DosConsoleText &out) {
    auto emit = [&](char ch) {
        if (out.text && out.length + 1 < out.capacity) out.text[out.length++] = ch;
        if (out.text && out.capacity) out.text[out.length] = 0;
    };
    if (c == '\n') {
        emit('\n');
        column = 0;
        return;
    }
    if (c == '\r') {
        column = 0;
        return;
    }
    emit(c);
    if (++column > kLastColumn) {
        emit('\n');
        column = 0;
    }
}
// print_string 0x1850: one pass per segment (up to LF, CR, NUL or the room
// left on the row plus one); a segment that does not fit is cut at its last
// space (the space is eaten, and a row cut short owes a line feed, 0x19c7), and
// a word with no space in it breaks the row only when the cursor is not at
// column 0 (0x1981), its leading blanks then skipped.
void DosConsole::print(const char *text, DosConsoleText &out) {
    if (!text || !*text) return;
    char row[kLastColumn + 2];
    size_t at = 0;
    bool owed = false;
    for (;;) {
        if (owed) put('\n', out);
        owed = false;
        bool broke = false;
        const int room = int(kLastColumn) - int(column);
        int length = 0;
        size_t cut = at;
        while (text[cut] != '\n' && text[cut] != '\r' && text[cut] && length <= room)
            row[length++] = text[cut++];
        if (!length) {
            if (!text[cut]) return;
            put(text[cut], out);
            at = cut + 1;
            continue;
        }
        int last;
        if (length <= room) {
            last = length - 1;
            at = cut;
        } else {
            int n = length;
            size_t back = cut;
            while (text[back] != ' ' && n && text[back] != '\n' && text[back] && text[back] != '\r') {
                --n;
                --back;
            }
            if (!n) {
                at = cut;
                if (column) {
                    put('\n', out);
                    broke = true;
                }
                last = length - 1; // 0x19ca: the restored length is room + 1
            } else {
                last = n - 1;
                while (last >= 0 && (row[last] == ' ' || row[last] == '\r' || row[last] == '\n')) --last;
                at = back;
                if (text[at]) ++at;
                if (last < room) owed = true;
                if (last > room) --last;
            }
        }
        int start = 0;
        if (broke)
            while (start <= last && row[start] == ' ') ++start;
        for (int i = start; i <= last; ++i) put(row[i], out);
        if (!text[at] && !owed) return;
    }
}
int32_t tavern_round_price(const GameState &g, int32_t town, bool house_round) {
    int32_t living = 0;
    const auto members = party_members(g.party);
    for (int i = 0; i < members.count; ++i)
        if (g.party.characters[members.indices[i]].status != 'D')
            ++living;
    if (house_round)
        return living;
    return town >= 0 && size_t(town) < std::size(TAVERN_ROUND_PRICE)
               ? TAVERN_ROUND_PRICE[town] * living
               : 0;
}

void shop_offerings(const GameState &g, const ShopSession &s, const ShopData &d, void *ctx,
                    void (*emit)(void *, ShopOffer)) {
    if (!emit || s.owner != &g || s.town < 0)
        return;
    uint8_t intel = g.party.character_count ? g.party.characters[0].intelligence : 15;
    switch (s.phase) {
    case ShopPhase::Buy:
        for (int j = 0; j < 8; ++j) {
            int index = s.town * 8 + j;
            if (size_t(index) >= d.weapons.count)
                break;
            int id = d.weapons.at(index);
            if (id != 255)
                emit(ctx, {id, shop_buy_price(d.equipment_prices.at(id), intel),
                           (id == 27 || id == 29) ? 99 : 1});
        }
        break;
    case ShopPhase::Sell:
        for (int i = 0; i < g.equipment_count; ++i)
            if (g.equipment_quantities[i] > 0)
                emit(ctx, {i, shop_sell_price(d.equipment_prices.at(i), intel),
                           g.equipment_quantities[i]});
        break;
    case ShopPhase::Reagent:
        for (int i = 0; i < 8; ++i)
            if (d.reagent_prices.at(s.town * 8 + i) > 0)
                emit(ctx, {i, shop_buy_price(d.reagent_prices.at(s.town * 8 + i), intel),
                           d.reagent_grants.at(s.town * 8 + i)});
        break;
    case ShopPhase::Guild:
        for (int i = 0; i < 3; ++i)
            emit(ctx, {i, guild_price(s.town, i, intel), i + 3});
        break;
    case ShopPhase::Ship:
        for (int i = 0; i < 2; ++i)
            emit(ctx, {i, ship_price(s.town, i == 0, intel), 1});
        break;
    case ShopPhase::Wine:
        for (int i = 0; i < 6; ++i)
            emit(ctx, {i, wine_price(i), 1});
        break;
    case ShopPhase::RationsQuantity:
        if (s.transactional_services)
            emit(ctx, {2, s.price, 25});
        break;
    case ShopPhase::InnPickupMember:
        for (int i = 0; i < g.party.character_count; ++i)
            if (g.party.characters[i].party_status == s.location)
                emit(ctx, {i, 0, 1});
        break;
    case ShopPhase::HealerMember:
    case ShopPhase::InnLeaveMember:
    case ShopPhase::LegacyHeal:
    case ShopPhase::LegacyCure:
    case ShopPhase::LegacyResurrect: {
        auto members = party_members(g.party);
        for (int j = 0; j < members.count; ++j)
            emit(ctx, {members.indices[j], 0, 1});
        break;
    }
    default:
        break;
    }
}
bool shop_offering_at(const GameState &g, const ShopSession &s, const ShopData &d, size_t row,
                      ShopOffer &offer) {
    struct Capture {
        size_t wanted = 0, seen = 0;
        ShopOffer offer{};
        bool found = false;
    } capture{row};
    shop_offerings(g, s, d, &capture, [](void *context, ShopOffer candidate) {
        auto &value = *static_cast<Capture *>(context);
        if (value.seen++ == value.wanted) {
            value.offer = candidate;
            value.found = true;
        }
    });
    if (capture.found)
        offer = capture.offer;
    return capture.found;
}
size_t shop_offering_count(const GameState &g, const ShopSession &s, const ShopData &d) {
    size_t count = 0;
    shop_offerings(g, s, d, &count,
                   [](void *context, ShopOffer) { ++*static_cast<size_t *>(context); });
    return count;
}
} // namespace openu5
