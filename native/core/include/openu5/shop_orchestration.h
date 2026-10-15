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
    // The last values the shop's own rand(lo,hi) draws returned, newest last.
    // Read-only bookkeeping for a frontend that prints the variant the binary
    // printed (Windows Original's console shops: the blacksmith's EXCL / INTRO /
    // WHICH at SHOPPES 0x0b30); it changes no draw and no rule.
    int8_t recent_draws[4]{-1, -1, -1, -1};
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
};
ActionResult begin_shop(CommandContext &, const NpcActor &);
ActionResult execute_shop(CommandContext &, ShopInput);

// Windows Original (the 1988 console shops; ORIGINAL_MODE_FIDELITY.md section
// 3.11): the DOS strings the shop loops print around their lettered lists,
// which the core does not emit (the T-Deck shows its panel instead). Read-only,
// no RNG; `variant` picks the rand-chosen form (ShopSession::recent_draws).
// Re/notes: native/core/spp1-findings/B-shops.md section 3.
enum class ShopConsoleText : uint8_t {
    BuyEcho,          // DS 0x8046 "Buy\n\n\""
    BuyExclaim,       // [0x3D4A + r]
    BuyIntro,         // [0x3D52 + r]
    BuyWhich,         // [0x3CB6 + r]
    BuyAnythingElse,  // DS 0x7BBC
    BuyThen,          // DS 0x7BDC
    BuySir,           // DS 0x7BD6
    BuyMilady,        // DS 0x7BCE
    ReagentsSell,     // DS 0x7A2C
    ReagentInterest,  // DS 0x7A1A
    GuildYes,         // DS 0x7916
    GuildRowKeys,     // DS 0x78D4
    GuildRowGems,     // DS 0x78E4
    GuildRowTorches,  // DS 0x78F4
    GuildConcern,     // DS 0x7906
    WineListOpen,     // DS 0x9B76
    WineListAfterName,// DS 0x9B88
    WineThyChoice,    // DS 0x9BFA
    TavernSir,
    TavernMilady,
    BuyAsk,           // [0x3CA6 + r], "Wouldst thou buy one?" ...
    SellDeal,         // DS 0x7D68, the sell offer's "Deal?"
    ReagentNeed,      // DS 0x7956, "Is this thy need?"
    GuildInterested,  // DS 0x7872, "Interested?"
    RumorFairNuff,    // the paid rumour's "Fair 'nuff?"
    InnWiltTake,      // the room's "Wilt thou take it?"
    HealerPay,        // "for % gold ... Wilt thou pay?" ('%' = the price)
    HealerCurePitch,  // "I can cure thy poisoned body "
    HealerHealPitch,  // "I can heal thee "
    HealerResPitch,   // "I can raise this unfortunate person from the dead "
    HealerYes,        // DS 0x80A4 "Yes" and a blank line
    HealerPowers,     // DS 0x80AA "We have powers to Cure, Heal, or Resurrect."
    HealerSaysNature, // DS 0x80DA "says $." ... "What is the nature of thy need?"
    HealerWho,        // DS 0x805A "Who needs my aid?"
    HealerAnyOther,   // DS 0x81C8 "Is there any other way in which I may aid thee?"
    InnYes,           // "Yes"
    InnMenu,          // "$ asks, Art thou here to Pick up or Leave a companion, ..."
    InnMore,          // "Is there anything more I can do for thee?"
    InnWhoStay,       // "$ asks, Who will stay?"
    InnWhoCheckOut,   // "Who will check out?"
    TavernYesOpen,    // DS 0x9F92 "Yes", a blank line and the opening quote
    TavernAnythingElse, // DS 0x9F9A "Anything else for thee?"
};
const char *shop_console_text(ShopConsoleText text, int variant = 0);
// The tavern's menu letters by subtype (SHOPPES2 [0x4D4C + town] = {0,0,0,2,3,1,0,2,0};
// food DS 0x4C1E "MCBF", drink 0x4C24 "AWRS", rations 0x4C2A "R  P", chat 0x4C30
// "CTHA"; a space is no rations). `which`: 0 food, 1 drink, 2 rations, 3 chat.
char tavern_menu_letter(int32_t town, int which);
} // namespace openu5
