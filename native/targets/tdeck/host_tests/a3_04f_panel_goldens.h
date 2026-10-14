#pragma once
// A3-04F (ALPHA3_AUDIO.md section 26): the panel after every render call of
// a3_04f_render_runtime's golden script (consecutive repeats folded), FNV-1a
// over all 320x240 pixels, with `a3_04f_render_runtime <pack> --record <this file>`.
// Re-recorded ONCE from the Alpha 4 UI Batch 1 Board (ALPHA4_UI.md): its restyle is
// the one intended change; the census (R/T/P) that guards A3-04F's efficiency is
// unchanged. Never regenerate it from a Board that changes how, not what, it draws.
#include <cstddef>
#include <cstdint>
namespace a3_04f_goldens {
// Alpha 4 A4-UI4 (ALPHA4_UI.md section 8): re-recorded for the console text alone (the DS-cited
// echoes, Look-North, no "ended" line): with the console region (x >= 182, y >= 87) masked,
// all 297 states equal the A4-UI1 recording (a4-ui4-census-masked.log).
// Alpha 4 A4-UI4 (section 8.20): re-recorded again for the console package the user chose
// (bullets, blank rows, bottom anchoring, the live prompt row and the wave cursor): 301 states,
// four more, all differing from their neighbours inside the console only; masked as above they
// fold back to 297 states, each equal to the pre-console recording (a4-ui4-console-golden-proof.log).
// A4-UI4 hardware follow-up (ALPHA4_UI.md section 8.22): re-recorded for the console's
// height alone (UiSession page_rows 12 -> the Board's row count): 301 states as before; with
// the console rectangle (x 184..318, y 88..239) blanked, the 297 masked states equal the
// previous recording's one for one (a4-ui4-hf1-golden-proof.log). Two recordings byte-identical.
// A4-UI5 (visual polish): re-recorded for the frame's new skin -- the glass below the map (x<181,
// y>=180), the cut corners (left taper x<4 y<8 and y 172..179, top-right x>=316 y<2), the location
// caption's 2 px indent (x 184..317, y 58..65), the status header rule (y=66) and the context bar's
// separators (y=215 and 227). With those regions masked all 301 states equal the HEAD recording
// one for one (a4-ui5-golden-proof.log).
// SPP1 A (docs/SHARED_PRESERVATION_PARITY_SPP1.md): re-recorded because (Z)-stats now costs the
// kernel dispatcher's world turn (K:0x3472) as the binary does. Phase 3 opens and cancels Z: the
// script's 301 states, its 2766 render calls and every phase boundary are unchanged; states
// 0..150 are identical and the first difference is state 151, the cancel that charges the turn
// (the status box's clock and the world move from there on). The fights of the census and of
// this script now start from the script's seed (B13), not from whatever the steps above drew
// (spp1-a-golden-proof.log).
constexpr const char *kSource = "SPP1 A: Z costs the dispatcher's turn (from state 151)";
constexpr size_t kRenders = 2766;
constexpr size_t kPhaseCount = 8;
constexpr size_t kPhaseStart[8] = {1, 21, 132, 145, 165, 210, 232, 261};
constexpr size_t kCount = 301;
constexpr uint64_t kPanel[301] = {
    0x4cc8fabc8571a131ull, 0xe23d825ee1e97f5full, 0x859c39e7384d49dfull, 0xe060f654cc68d2f9ull,
    0x3ae33914c1e1bca7ull, 0xc0815e538d8c5d27ull, 0xefebbb229b052761ull, 0x14689ba45536666full,
    0x19390073f1705981ull, 0xb26b3be3397a304bull, 0xa5c493583affd2a1ull, 0xeab5d4ea828e5303ull,
    0x7c84ad511f0925cbull, 0x11e545ba9f42dfcdull, 0x7c76356edbd4c26dull, 0x76971944a65cf21full,
    0xed990213d9c9cf99ull, 0xe23d825ee1e97f5full, 0x859c39e7384d49dfull, 0xe060f654cc68d2f9ull,
    0x3ae33914c1e1bca7ull, 0x979a9b1c719eda83ull, 0x00cfa737c49b8b05ull, 0x0b80fb624880c62bull,
    0xfad678ba9b09687dull, 0x9d4e71be4f49baabull, 0xc27cb9d073380bc5ull, 0x773c89c888a10335ull,
    0x41816bae6ab6610full, 0xfe6b159ad722c1e7ull, 0x883d2b52da9586f9ull, 0x0677accf00dc4c0full,
    0x5e74aa37d6c2e395ull, 0xf27b5aaeda048fd3ull, 0x775da2cd439f1a83ull, 0x0619998a04304d2dull,
    0x7c287e2c85aeeb55ull, 0x91f2d7c2e3746167ull, 0xf8533ba21f8af555ull, 0x71739362bacc0bc5ull,
    0xe93f15baef989ebfull, 0x40902301d2d0f2efull, 0x7a715a5dcd7bae51ull, 0x0aa0f79488151e45ull,
    0x2cb6319b83bf4b05ull, 0x258853d0af2e147full, 0xf4a936aeb5479905ull, 0xe78bb770cb2c58c7ull,
    0xdaf2d088f3ba455dull, 0x8bf28da99c71ed07ull, 0xb280dfaf48eceed9ull, 0xfaf3cea4005a51ebull,
    0x69de8aa936af6ff3ull, 0xf5cdc1bc0981e44dull, 0xdfa14c66839884d1ull, 0x8e4353e825526f9bull,
    0x2171bfa549e3fc5dull, 0xbcccd92190c97d9bull, 0x7bc1849daae7bc6bull, 0x226ca2b736b46a31ull,
    0x29bcd8d5d5fba941ull, 0x3b4b4d3513ec7a93ull, 0xabe7b6e212ee7c47ull, 0x299201ed84606e09ull,
    0x26f9842a626a4283ull, 0xfa5858aeb8cb7b29ull, 0xf318ed7ad19f5a61ull, 0x67f5de2c56582c9bull,
    0xf24d6e79c063c8c3ull, 0x001608ee37c86eddull, 0x013617b1c5af0c9bull, 0xe19f6cb184bdf063ull,
    0xf418e47dae44bf05ull, 0x166513bda67467cbull, 0x0ae90173ce0f684dull, 0x82c1a5641bdbccf3ull,
    0xbff8e9c4a2f96445ull, 0xf8a1aa066e2787d3ull, 0x1e79a6e9594524c5ull, 0x528b65e98021f18dull,
    0xe05a8afe09e52f17ull, 0x606d613207f4e9f1ull, 0xec1fa35123b7aa7bull, 0x350dbad757c2e1b9ull,
    0x86bdfff0a8aae475ull, 0x53e47ad45b3ee203ull, 0xa35e661b5e35eb8bull, 0xcf7f323ca198b5f3ull,
    0x15477214756c2b45ull, 0xf45702d48184aadfull, 0x6e8066e98f668985ull, 0xa695a2ddc01ecc0full,
    0x1e15009aeb962ab1ull, 0x41d7204361818ad1ull, 0xadf30a8de1380fd3ull, 0xf0352300358f8a1dull,
    0xffe293afccabcfabull, 0x264274161d03a76bull, 0xfec6b63fdd088a0dull, 0x1f6eca92f31fa237ull,
    0x3b2619918de533c9ull, 0x6f198a6b7d4fc38full, 0x4328e9f5b084d991ull, 0x0dde90d504269407ull,
    0x18a9d4c82b74675dull, 0xd490f34593ab0a2full, 0xbc1e9b9f03be4397ull, 0x0cdac255d16e55d1ull,
    0xa5e2b0ce231522d3ull, 0x44b83b01bb47dbcdull, 0x113796a6cc538073ull, 0x92d4c327d1bb863full,
    0xc6184ab77341edddull, 0x83efa51040a8597dull, 0xa37ffff48548a5c1ull, 0x4154899718f85bfbull,
    0x60d669290f2178cdull, 0xbc9cf970a9f27be3ull, 0xc8c3c27b1b422eafull, 0xba55c8e9eaa4bc01ull,
    0xfa8c9b337aff0269ull, 0xb7cf84ac8fe1eb89ull, 0x3304323fac860077ull, 0x9e2c19fadb3618f7ull,
    0x678ea28b81e81a69ull, 0x8d8b6bdf305a1ba1ull, 0x8a8e3f6a73d1b12bull, 0x7588c23e1aa56f19ull,
    0x4d7cb4456ef83c25ull, 0x88defa4f3eed16a3ull, 0x26adc6e23b2e9a65ull, 0x2823b1b5e74a1755ull,
    0xb4c396c270ec7657ull, 0x02a8e16bbbfead19ull, 0xf3c7e4d050cf5853ull, 0xe700bbf0e708fb19ull,
    0x7322515ddfc19d41ull, 0xee4f3c36b7753fbbull, 0xbb1d891169210877ull, 0xdebe25dc9b96d01dull,
    0x2cb0c5ad78780ee5ull, 0x6abb994c1ad9943full, 0x44b1b0ea92a85b3dull, 0xb76c8418bac987c7ull,
    0x89c264f969a3c2e9ull, 0x410b48bf0d2fdbccull, 0xcc597e6b81f6cfa8ull, 0x728fd32238037016ull,
    0x66482a1fb5eece52ull, 0x194ecf19faab4fd8ull, 0xc8114ed947c79856ull, 0x30c4ec976f053a6full,
    0xd60e0d0f44084d55ull, 0x29ff84ad98a28f9dull, 0x901e1d382e3b6f97ull, 0x9b25b40fb75e1775ull,
    0x6f0b05c9f429997full, 0xb592000c0decd0d1ull, 0xffb24271374038fdull, 0x29c83172877e0237ull,
    0x6db06304ba6c9ebdull, 0x06877269b6aa7395ull, 0x30c4ec976f053a6full, 0xd60e0d0f44084d55ull,
    0x29ff84ad98a28f9dull, 0xe3b0eac265026fd5ull, 0x475a6e24719d7b60ull, 0xe9bd230681474980ull,
    0xff569ff302bc71c6ull, 0x436e7bc511fabdcaull, 0xc9de5883e949d540ull, 0x8c202f333c73a374ull,
    0x6cba1d394c7689d6ull, 0x0e6e33316ff48caeull, 0x29b6b5301fa9796cull, 0x4ba46d058f353190ull,
    0x5a0e817fc067f650ull, 0x7aecf565e4a7eb68ull, 0xc28fb68213d4f844ull, 0xb6a2687c7aa56850ull,
    0x2b30c67a3b948f8cull, 0x3237a9b02b01bcf0ull, 0x104b33e0ade17109ull, 0x16f1d8e37fa3f280ull,
    0x4c234fe23b187ab1ull, 0xce60dd86a6d9d7b3ull, 0xe5b406670b3645abull, 0xfc97d93a5f637fe0ull,
    0x86ef946d4c6ab309ull, 0xac75854d7cf0294dull, 0x4cde2a36ca3445f2ull, 0x758ea4291d176eb3ull,
    0x59fa8ecf49dc3ce5ull, 0x463120c59bdca7d3ull, 0x6aa374a12bb1efdbull, 0x91cbfe6c053068adull,
    0x226cf3750db7667bull, 0xbef007b36be53623ull, 0x1632fcf8da1abde5ull, 0x5cb066b96b84a1dbull,
    0x637e932eafa4a0ddull, 0x5981027d93d338b7ull, 0xb5e009090606dcc5ull, 0x5c75cc0b37aefdd7ull,
    0xd565c8aa2eb9cf67ull, 0x2f6a4d1c567953a9ull, 0x6bda306b0b1fb9d9ull, 0x758ea4291d176eb3ull,
    0x59fa8ecf49dc3ce5ull, 0x463120c59bdca7d3ull, 0xa19367b9eb74ab3bull, 0x2314836b8e3cf0b1ull,
    0x0a5b51ca2cbc2a11ull, 0xc47103de630530e3ull, 0x827436a104bdfe8bull, 0x7481b77158bb3a0dull,
    0x9b043e8b40de2493ull, 0xc0817c9924e355b1ull, 0xe3e3edac10485f6bull, 0xe60bddc55ad133c3ull,
    0xf325f5409f79820dull, 0xecc67717b2b63863ull, 0xda7641f0d2a8ac4bull, 0x29c03d782e9213c5ull,
    0x7fb291efb400aa95ull, 0xc52eee8c6ad46147ull, 0x5a88739925eaba7dull, 0x3b7e8c05f99ea7cfull,
    0x5d51b26f7594ab37ull, 0xbe0d66a5aa80b311ull, 0xf63dc58bbb978921ull, 0x18dbc78fe8eb3cdbull,
    0xeb85c9b77afd36cbull, 0x368088b39146e2cdull, 0x83b35fb53d356d79ull, 0xb42a4cde78e52a2bull,
    0x7992991470a27313ull, 0xf5abf9ce8a3f0045ull, 0x53cd0b0811b834dfull, 0xfaeccba9e6de6395ull,
    0xbd917150619bcb73ull, 0xed9682204305d09dull, 0xe0fee0ad0d861727ull, 0x854b3992e187e213ull,
    0x26a99a468ae51a43ull, 0x227cf5145cb44fe5ull, 0xe73113eb7557cd3full, 0xadfb374712f84e59ull,
    0xff7a1203ac321913ull, 0xb330362aab512471ull, 0x4513da0b336861f9ull, 0x24db3de892ff04cfull,
    0xbb4c851eb5341addull, 0x001dd3f7409363adull, 0xf88a0e7613e887f7ull, 0x328f5c36ae83cee7ull,
    0x38c5939188a28e87ull, 0x1dcc967479cc57c5ull, 0xff7aadcd21b23b4full, 0x0586bff2686e12cdull,
    0x58cf56139336a3a5ull, 0xedce9f31964bc8e9ull, 0xcd73ac4732cca1d7ull, 0xb7313ee2e8119637ull,
    0x5d9801be29929127ull, 0x2b32f82ebf826805ull, 0xb7d4c33e93a29a23ull, 0xe4cce1547d74eb09ull,
    0x062bd7a7ff879df7ull, 0x019cc268282746b7ull, 0x1caaa9a86f021c05ull, 0x0e1af41bde0df747ull,
    0x7f39cdfeb73bf36dull, 0xf00e99a8cf304e91ull, 0x729d41f0adf1b4ddull, 0x441127a3edf88f4dull,
    0x8794edc0ad89f113ull, 0x33eb8022e3c1b9adull, 0xa6541db00b54ab41ull, 0x47a605c786d5e14full,
    0x9395631ae354b8f5ull, 0x33ef32eef7d43e63ull, 0xf16bbb1fbf5f287dull, 0x3a5aad4925abf76bull,
    0x65dfa0dbc97c638bull, 0x7d0c2c1a5b356201ull, 0x0c69dc58eeb9acbdull, 0x8058749e429bfa35ull,
    0x0a5e551e0d2c2edbull, 0x0931b761eb0a1d7bull, 0x717186ab8ffd53d9ull, 0xe8f212e04b89ba37ull,
    0xc73fa1c3dfc70239ull, 0x834bb82dded225f1ull, 0x6d129b4bcf2dcc57ull, 0xcbc6956cfd74754full,
    0xb6a151614ad64875ull, 0xe806b08b98b0c005ull, 0x0e55bb4d9313b967ull, 0xe5ed7b95d75d7343ull,
    0xee7a8362b7ec1c79ull,
};
} // namespace a3_04f_goldens
