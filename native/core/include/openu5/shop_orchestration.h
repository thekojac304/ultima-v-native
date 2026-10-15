#pragma once
#include "commands.h"
#include "shops.h"
namespace openu5 {
enum class ShopPhase : uint8_t {
    Closed,
    Greeting,
    BlacksmithPause,
    Menu,
    Buy,
    Sell,
    Reagent,
    Guild,
    Ship,
    HealerNeed,
    HealerMember,
    HealerAgain,
    InnAgain,
    Tavern,
    TavernAgain,
    Wine,
    Drunk,
    RationsQuantity,
    RumorText,
    BuyDeal,
    SellDeal,
    ReagentDeal,
    GuildDeal,
    HorseDeal,
    ShipDeal,
    HealerDeal,
    InnRestDeal,
    InnLeaveMember,
    InnLeaveDeal,
    InnPickupMember,
    RumorDeal,
    BuyFull,
    ReagentFull,
    ShipAgain,
    LegacyHeal,
    LegacyCure,
    LegacyResurrect,
    TavernRoundDeal,
    TavernDrinkDeal
};
enum class ShopAction : uint8_t {
    Continue,
    Buy,
    Sell,
    SelectItem,
    SelectMember,
    Heal,
    Cure,
    Resurrect,
    Rest,
    LeaveMember,
    Pickup,
    Round,
    Drink,
    Rations,
    Rumor,
    Confirm,
    Decline,
    Cancel,
    End,
    Text
};
struct ShopInput {
    ShopAction action{};
    int32_t value = 0;
    const char16_t *text = nullptr;
    size_t length = 0;
};
struct ShopSession {
    GameState *owner = nullptr;
    const ShopRecord *record = nullptr;
    ShopType type{};
    ShopPhase phase = ShopPhase::Closed;
    int32_t location = 0, town = -1, item = -1, price = 0, quantity = 0, served = 0;
    HealerService remedy{};
    bool purchased = false, bought_in_buy = false, selling = false, buying = false,
         thrown_out = false, tavern_served = false, transactional_services = false;
    bool legacy_healer = false;
    // ShopServices::dos_transcript at begin_shop, and the 1988 console's cursor
    // column that transcript keeps (DosConsole). Presentation only.
    bool dos_transcript = false;
    uint8_t dos_column = 0;
};
enum class ShopEventKind : uint8_t { Entered, State, Result, Exited };
struct ShopEvent {
    ShopEventKind kind{};
    const ShopSession *session = nullptr;
    const ShopResult *result = nullptr;
    const TavernResult *tavern = nullptr; // Price announcement and plate/service metadata.
};
struct ShopOffer {
    int32_t item = 0, price = 0, quantity = 0;
};
// Streams the current transaction's offerings without allocating or changing RNG/state.
void shop_offerings(const GameState &, const ShopSession &, const ShopData &, void *,
                    void (*)(void *, ShopOffer));
// Resolves a zero-based UI row to the authoritative offer identifier.
bool shop_offering_at(const GameState &, const ShopSession &, const ShopData &, size_t,
                      ShopOffer &);
size_t shop_offering_count(const GameState &, const ShopSession &, const ShopData &);
// Read-only price helpers used by transactional frontends before commitment.
int32_t tavern_round_price(const GameState &, int32_t town, bool house_round = false);
// World owners reuse their map/object pools; reserve must precede payment.
struct ShopServices {
    ShopSession &session;
    const ShopData &data;
    void *context = nullptr;
    bool (*record_present)(void *, int32_t) = nullptr;
    const char *(*record)(void *, int32_t) = nullptr;
    int32_t (*tile)(void *, int32_t, int32_t) = nullptr;
    bool (*occupied)(void *, int32_t, int32_t) = nullptr;
    bool (*reserve)(void *, bool ship) = nullptr;
    void (*horse)(void *, int32_t, int32_t) = nullptr;
    void (*ship)(void *, int32_t, int32_t, int32_t tile, int32_t hull, int32_t skiffs) = nullptr;
    void (*plate)(void *, int32_t, int32_t, int32_t) = nullptr;
    void (*hour_tiles)(void *) = nullptr;
    void (*wake_npcs)(void *) = nullptr;
    // Device frontends may stage otherwise-immediate services so price/confirmation
    // can be presented without changing the reference command semantics.
    bool transactional_services = false;
    // Windows Original: the shop prints the 1988 console transcript itself
    // (native/core/spp1-findings/B-shops.md section 3) -- the Y/N and letter
    // echoes, the lists, the questions, the connective lines -- as stream
    // Messages laid out by DosConsole, and its own result strings ("It is
    // done.", "She awaits thee at the dock!", ...) are not shown. Text only:
    // no draw, price, rule or state differs from the default (the T-Deck's,
    // which never sets it).
    bool dos_transcript = false;
};
// The 1988 console's cursor: the kernel's print_string (0x1850, its word wrap
// included) and putchar (0x16ba) on the 16-column console window. Each call
// appends to `out` the text as the DOS screen shows it -- a line break wherever
// the kernel moved to the next row, the space it ate at a wrap dropped -- and
// keeps `column`. Re: re/notes/printstr-1850-derivacion.md.
struct DosConsoleText {
    char *text = nullptr;
    size_t capacity = 0, length = 0;
};
struct DosConsole {
    static constexpr uint8_t kLastColumn = 15; // window right - left (cols 24..39)
    uint8_t column = 0;
    void print(const char *text, DosConsoleText &out);
    void put(char c, DosConsoleText &out);
};
ActionResult begin_shop(CommandContext &, const NpcActor &);
ActionResult execute_shop(CommandContext &, ShopInput);

// The tavern's menu letters by subtype (SHOPPES2 [0x4D4C + town] = {0,0,0,2,3,1,0,2,0};
// food DS 0x4C1E "MCBF", drink 0x4C24 "AWRS", rations 0x4C2A "R  P", chat 0x4C30
// "CTHA"; a space is no rations). `which`: 0 food, 1 drink, 2 rations, 3 chat.
char tavern_menu_letter(int32_t town, int which);
} // namespace openu5
