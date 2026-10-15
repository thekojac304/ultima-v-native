#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace tdeck {

inline bool ui_scale_requires_full_layout(uint8_t cached,uint8_t current) {
    return cached!=current;
}

struct DeviceTextMetrics {
    uint8_t glyph_width=5,glyph_height=7,cell_width=6,line_height=8;
};

// Three real raster geometries.  The former mapping collapsed Small and Medium
// to the same 5x7 path and made Large a horizontally stretched 10x7 glyph.
constexpr DeviceTextMetrics ui_text_metrics(uint8_t size) {
    return size==0 ? DeviceTextMetrics{4,6,5,7}
         : size>=2 ? DeviceTextMetrics{7,9,8,10}
                   : DeviceTextMetrics{5,7,6,8};
}
// A4-UI4 hardware follow-up (ALPHA4_UI.md section 8.22): the most console rows
// any text size lays out between the status box's rule (y 88,
// openu5::kHudTranscriptY) and the glass (240) -- Small's 7 px rows: 21. The
// device's UiSession shows up to this many (AlphaRuntime::session_config()).
constexpr size_t kConsoleMaxRows=size_t((240-88)/ui_text_metrics(0).line_height);
// Windows port, the DOS game's 320 x 200 screen (openu5::platform_dos_screen):
// its console, text window 2 -- set_text_window(2, 24, 11, 39, 23), INTRO.OVL
// 0x0d1a -- 16 IBM.CH cells by 13 rows at (192, 88). The T-Deck never uses it.
constexpr size_t kDosConsoleColumns=16,kDosConsoleRows=13;
static_assert(kDosConsoleRows<=kConsoleMaxRows,"the session keeps enough rows for the DOS console");

// Alpha 4 A4-UI4 (ALPHA4_UI.md section 8): the boot screens name the release
// the image is, from its PROJECT_VER ("4.0.0-alpha4-ui4-debug" -> "Alpha 4"),
// not a literal that went stale with Alpha 2. Static strings: no buffer.
inline const char *firmware_release_label(const char *version) {
    static constexpr const char *kLabels[]={"Alpha 1","Alpha 2","Alpha 3","Alpha 4","Alpha 5",
                                             "Alpha 6","Alpha 7","Alpha 8","Alpha 9"};
    if(!version||version[0]<'1'||version[0]>'9'||version[1]!='.')return "OpenU5";
    return kLabels[version[0]-'1'];
}

constexpr int kContextBarTop=215,kContextBarStatusY=217,kContextBarActionsY=229;
constexpr int kContextBarStatusH=9,kContextBarActionsH=9;
constexpr size_t kContextBarActionColumns=22;
constexpr char kTavernContextActions[]="A Round|D Drink|R Food";
static_assert(sizeof(kTavernContextActions)-1<=kContextBarActionColumns);
struct DeviceContextActionBar {
    bool active=false;
    char status[32]{},actions[48]{};
};

constexpr bool context_action_bar_does_not_overlap(int content_bottom) {
    return content_bottom<=kContextBarTop;
}

constexpr int kCharacterTitleArtY=4,kCharacterTitleArtH=110;
constexpr int kCharacterTextY=116,kCharacterTextRowH=9;
// Alpha 4 UI Batch 2 (ALPHA4_UI.md section 2.1): the title's credit block and
// prompt, IBM.CH lines centred under the art (which ends at y=113).
constexpr int kTitleCreditsY=130,kTitleCreditsStep=14,kTitlePromptY=184;
constexpr int kCharacterQuizArtH=152,kCharacterQuizTextY=154;

constexpr size_t kShopVisibleRows=6;
struct DeviceShopRow { char name[24]{}; int32_t price=0,quantity=0; };
struct DeviceShopView {
    bool active=false;
    char title[32]{},keeper[24]{},phase[64]{},input[24]{};
    DeviceContextActionBar context{};
    int32_t gold=0;
    size_t total=0,page_start=0,row_count=0,selected_row=0;
    DeviceShopRow rows[kShopVisibleRows]{};
};

// Windows port, the 1988 interaction model (platform_dos_interface()): the DOS
// game's right panel, text window 1 = cols 24..39 x rows 1..9 of its 40 x 25
// screen (ZSTATS set_text_window(1,0x18,1,0x27,9); re/notes/ztats-layout.md
// section 0), as cells: IBM.CH codes, or RUNES.CH where `runes` says so
// (set_font 0x3abe = kernel 0x1c9e), in reverse video where `inverse` says so
// (putchar 0xfd, kernel 0x17a5 -> 0x1845). The banner is the panel's title in
// the band above (kernel 0x4e50: centred on screen col 30, framed by the band
// brackets); `arrow` is the scroll glyph in the band below (kernel 0x4dea,
// fixed at col 31: 0x18 up, 0x19 down, 0x12 both). The Board draws it over the
// roster and status boxes -- the original's box, with the console under it
// left as it is. The T-Deck never composes one.
constexpr size_t kDosPanelRows=9,kDosPanelCols=16;
constexpr uint8_t kDosBlank=0x20;
// draw_list_frame (ZSTATS 0x045e): the scroll's IBM.CH box glyphs -- the top and
// bottom horizontals are different glyphs (ztats-layout.md section 4).
constexpr uint8_t kDosBoxTopLeft=0x10,kDosBoxTop=0x11,kDosBoxTopRight=0x13,kDosBoxBar=0x17,
                  kDosBoxBottomLeft=0x14,kDosBoxBottom=0x15,kDosBoxBottomRight=0x16;
constexpr uint8_t kDosArrowUp=0x18,kDosArrowDown=0x19,kDosArrowBoth=0x12;
// `layout` 0 is one box over window rows 1..9 (the item lists, Z-stats, Mix),
// the arrow in the band below it (screen row 10); 1 is the frame's own two
// boxes -- the roster box (rows 1..6), the band (row 7, where list_wares puts
// its arrow) and the status box (rows 8..9) -- as the blacksmith's "Arms"
// window leaves them (SHOPPES 0x0c80). Cells of window row 6 are not drawn in 1.
constexpr uint8_t kDosLayoutBox=0,kDosLayoutRosterAndStatus=1;
struct DeviceDosPanel {
    char banner[kDosPanelCols+1]{};
    uint8_t layout=kDosLayoutBox;
    uint8_t arrow=0;
    uint8_t cells[kDosPanelRows][kDosPanelCols]{};
    uint16_t runes[kDosPanelRows]{},inverse[kDosPanelRows]{};
};

inline void dos_panel_clear(DeviceDosPanel &panel) {
    panel=DeviceDosPanel{};
    for(auto &row:panel.cells)for(auto &cell:row)cell=kDosBlank;
}
/** Writes `text` from (row, col) without wrapping; `runes` marks the cells RUNES.CH. */
inline size_t dos_panel_put(DeviceDosPanel &panel,size_t row,size_t col,const char *text,bool runes=false) {
    size_t n=0;
    for(;text&&text[n]&&row<kDosPanelRows&&col+n<kDosPanelCols;++n){
        panel.cells[row][col+n]=uint8_t(text[n]);
        if(runes)panel.runes[row]=uint16_t(panel.runes[row]|(1u<<(col+n)));
    }
    return n;
}
/** draw_list_frame(contentRows+1): the frame in window cols 0..14, rows 0..contentRows+1. */
inline void dos_panel_list_frame(DeviceDosPanel &panel,size_t content_rows) {
    constexpr size_t last=kDosPanelCols-2; // 15 cells wide: cols 0..14
    if(content_rows+2>kDosPanelRows)content_rows=kDosPanelRows-2;
    panel.cells[0][0]=kDosBoxTopLeft;panel.cells[0][last]=kDosBoxTopRight;
    const size_t bottom=content_rows+1;
    panel.cells[bottom][0]=kDosBoxBottomLeft;panel.cells[bottom][last]=kDosBoxBottomRight;
    for(size_t c=1;c<last;++c){panel.cells[0][c]=kDosBoxTop;panel.cells[bottom][c]=kDosBoxBottom;}
    for(size_t r=1;r<=content_rows;++r){panel.cells[r][0]=kDosBoxBar;panel.cells[r][last]=kDosBoxBar;}
}
/** The arrow glyph for a window of `visible` rows at `scroll` over `total`. */
inline uint8_t dos_panel_arrow(size_t scroll,size_t visible,size_t total) {
    const bool up=scroll>0,down=scroll+visible<total;
    return up&&down?kDosArrowBoth:up?kDosArrowUp:down?kDosArrowDown:uint8_t(0);
}
/** The minimal scroll that keeps `cursor` in a window of `visible` rows. */
inline size_t dos_panel_scroll(size_t cursor,size_t scroll,size_t visible) {
    if(cursor<scroll)scroll=cursor;
    if(visible&&cursor>scroll+visible-1)scroll=cursor-visible+1;
    return scroll;
}

// One row of print_list_row (ZSTATS 0x05e2; re/notes/use-picker-panel.md
// sections 3-6 and ready-picker-panel.md section 3), into a row of `width`
// cells from `col`: [2-digit count, space-padded | "--" for 0 | nothing when
// hidden (0xff)][separator -- RUNES.CH when < 0x20 (0x0615)][the name, whose
// first byte may be a format sigil: '*' scroll (decoration + initials in
// RUNES.CH, 0x0638), '!' potion (decoration in RUNES.CH, then the colour,
// 0x0664), '(' moonstone ("Moonstone " then the phase digit in RUNES.CH,
// 0x068e)]. The DGROUP strings the routine prints come from the caller.
struct DosListRowText {
    const char *zero_fill=nullptr;       // DS 0x9778 "--"
    const char *scroll_decoration=nullptr; // DS 0x977c (0x1c " + ")
    const char *potion_decoration=nullptr; // DS 0x9782 (0x1d " + ")
    const char *moonstone=nullptr;       // DS 0x9788 "Moonstone "
};
inline void dos_list_row(DeviceDosPanel &panel,size_t row,size_t col,size_t width,int quantity,bool hidden,
                         uint8_t separator,const char *name,const char *side,const DosListRowText &text) {
    if(row>=kDosPanelRows)return;
    const size_t end=std::min(kDosPanelCols,col+width);
    size_t at=col;
    auto put=[&](uint8_t code,bool runes){
        if(at>=end)return;
        panel.cells[row][at]=code;
        if(runes)panel.runes[row]=uint16_t(panel.runes[row]|(1u<<at));
        ++at;
    };
    auto put_text=[&](const char *s,bool runes){for(;s&&*s;++s)put(uint8_t(*s),runes);};
    if(!hidden){
        char count[3]{};
        if(quantity==0&&text.zero_fill)std::snprintf(count,sizeof(count),"%.2s",text.zero_fill);
        else std::snprintf(count,sizeof(count),"%2d",std::min(99,std::max(0,quantity)));
        put(uint8_t(count[0]),false);put(uint8_t(count[1]?count[1]:' '),false);
        put(separator,separator<0x20);
    }
    const char *n=name?name:"";
    if(*n=='*'){put_text(text.scroll_decoration,true);put_text(n+1,true);}
    else if(*n=='!'){put_text(text.potion_decoration,true);put_text(side?side:n+1,false);}
    else if(*n=='('){put_text(text.moonstone,false);if(n[1])put(uint8_t(n[1]),true);}
    else put_text(n,false);
}

constexpr size_t kSelectionVisibleRows=8;
struct DeviceSelectionView {
    bool active=false,detail_panel=false;
    uint8_t mode=0;
    char title[24]{},detail[32]{},detail2[32]{};
    DeviceContextActionBar context{};
    size_t total=0,page_start=0,row_count=0,selected_row=0;
    char rows[kSelectionVisibleRows][24]{};
    // The 1988 interaction model's right panel (above). When set, the Board
    // draws it in the original's place and leaves the console as it is; the
    // fields above are then only the scenario facts' copy of the list.
    const DeviceDosPanel *dos=nullptr;
};

inline bool selection_uses_transcript(const DeviceSelectionView &view) {
    return !view.detail_panel;
}

inline void format_ready_row(char *out,size_t capacity,const char *name,
                             uint16_t quantity,bool equipped) {
    // Equipment state is authoritative over pack count: an item may be worn
    // while another copy remains in the pack, and the selector must never hide
    // that fact behind the quantity label.
    if(equipped&&quantity>0)std::snprintf(out,capacity,"%s x%u [equipped]",name,unsigned(quantity));
    else if(equipped)std::snprintf(out,capacity,"%s [equipped]",name);
    else if(quantity>1)std::snprintf(out,capacity,"%s x%u",name,unsigned(quantity));
    else if(quantity==1)std::snprintf(out,capacity,"%s",name);
    else std::snprintf(out,capacity,"%s",name);
}

// The EGA driver's XOR write mode on a palette index (c -> c ^ mask); 15 is the
// full negative. A3-HF7: the Codex's pulses XOR with 4 and 15 too.
inline uint16_t magic_xor_palette_pixel(uint16_t pixel,const uint16_t *palette,uint8_t mask=15) {
    for(int color=0;color<16;++color)if(pixel==palette[color])return palette[color^(mask&15)];
    return pixel;
}

struct DevicePartyHighlight {
    int8_t selected=-1;
    int8_t actor=-1;
    // Y-04 (#213). The roster row in REVERSE VIDEO, the binary's shared 0x2a28
    // primitive. It outranks `selected`/`actor` while it lasts, because 0x2a28
    // is an XOR over the framebuffer -- see openu5::roster_invert_row, which
    // owns the precedence rule this field is the top of.
    int8_t damage_flash=-1;
    // Windows port, the 1988 interaction model (platform_dos_interface()):
    // the roster picker's cursor row and the arena actor's row in reverse
    // video, the same 0x2a28 primitive below damage_flash (select_party_member
    // 0x2dc1, COMBAT 0x06df); and the picker's "Select:" banner (0x2db2) in the
    // status caption row. The defaults draw exactly what the T-Deck draws.
    int8_t cursor=-1;
    int8_t invert_actor=-1;
    bool select_banner=false;
};

inline bool selection_row_needs_redraw(const DeviceSelectionView &current,
                                        const DeviceSelectionView &cached,size_t row,bool first=false) {
    const bool have=row<current.row_count,had=row<cached.row_count;
    return first||have!=had||row==current.selected_row||row==cached.selected_row||
        (have&&std::strcmp(current.rows[row],cached.rows[row])!=0);
}

inline bool shop_row_needs_redraw(const DeviceShopView &current,
                                  const DeviceShopView &cached,size_t row,bool first=false) {
    const bool have=row<current.row_count,had=row<cached.row_count;
    return first||have!=had||row==current.selected_row||row==cached.selected_row||
        (have&&(std::strcmp(current.rows[row].name,cached.rows[row].name)!=0||
                current.rows[row].price!=cached.rows[row].price||
                current.rows[row].quantity!=cached.rows[row].quantity));
}

}  // namespace tdeck
