// v0.5.0 UI polish: real handheld Board rendering, navigation and key preservation.
#include "a4_ui2_harness.h"
#include "openu5/platform_profile.h"
using namespace a4_ui2;
namespace {
void size(Run&h,int value){
 h.key('m',true);h.down();h.down();h.down();h.key('\r');h.down();h.down();h.down();
 for(int i=0;i<3&&h.rt->device_settings().ui_size!=value;++i)h.ball(RawInputKind::TrackballRight);
 h.mic();h.mic();
 check(h.rt->device_settings().ui_size==value,"Text size can be changed and persisted through the hardware menu");
}
uint64_t region(int y,int height){
 uint64_t hash=1469598103934665603ull;
 for(int row=y;row<y+height;++row)for(int x=0;x<320;++x)hash=(hash^px(x,row))*1099511628211ull;
 return hash;
}
void menus(int value){
 Run h({{"Avatar",'G',100}});size(h,value);h.return_to_title();h.key(' ');
 auto v=h.rt->frontend_view();
 check(v.line_count==8&&std::string(v.lines[5])=="Return to the View"&&std::string(v.lines[7])=="DOS Save Transfer","All eight entries retained, with historical View wording and DOS transfer label");
 const auto metrics=tdeck::ui_text_metrics(uint8_t(value));
 for(int i=0;i<8;++i){
  check(h.rt->frontend_view().selected_line==i,"Title menu moves to each expected row");
  const int y=116+i*(metrics.line_height+3)+(i>=6?5:0);
  check(lit(8,y-1,304,metrics.line_height+2)>304&&y+metrics.line_height+1<=224,"Selected title row is visible across the menu width and above the footer");
  const auto retained=region(114,126);h.render(true);
  check(region(114,126)==retained,"Retained title redraw equals a fresh full redraw");
  if(i==6)check(std::string(h.rt->frontend_view().footer).find("Alt+M: System Menu")!=std::string::npos,"Title Settings selection advertises the handheld menu shortcut");
  h.down();
 }
 dump(("host-rendered-menu-size-"+n(value)).c_str());
 h.key('s');
 check(std::string(h.rt->frontend_view().subtitle)=="Display | Alt+M: System Menu","Title Settings shows category and the handheld menu reminder");
 for(int i=0;i<7;++i){
  const auto page=h.rt->frontend_view();
  check(page.selected_line==i&&std::strlen(page.lines[i])+2<=304/metrics.cell_width,"Every Settings row fits the selected font width");
  const auto retained=region(0,240);h.render(true);
  check(region(0,240)==retained,"Retained Settings, including values and category, equals a full redraw");
  h.down();
 }
 dump(("host-rendered-settings-size-"+n(value)).c_str());
 h.mic();h.key('p');
 check(std::string(h.rt->frontend_view().title)=="DOS Save Transfer"&&std::string(h.rt->frontend_view().lines[0])=="Import DOS Save"&&std::string(h.rt->frontend_view().lines[1])=="Export DOS Save","Handheld transfer page uses concise DOS terminology");
 dump(("host-rendered-dos-transfer-size-"+n(value)).c_str());
 h.mic();h.key('r');const auto frame=region(136,64);h.run(1200);
 check(h.rt->frontend_state()==FrontendState::AttractDemo&&region(136,64)!=frame,"Original View advances automatically without player input");
 dump(("host-rendered-view-size-"+n(value)).c_str());
 h.mic();check(h.rt->frontend_state()==FrontendState::MainMenu,"Mic returns from View to the menu");
 h.key('u');const auto scene=std::string(h.rt->frontend_view().lines[0]);h.run(6000);
 check(std::string(h.rt->frontend_view().lines[0])==scene,"The Summoning waits for input rather than an invented timer");
 h.key('\r');check(std::string(h.rt->frontend_view().lines[0])!=scene,"Enter advances the Summoning");
 h.mic();check(h.rt->frontend_state()==FrontendState::MainMenu,"Mic cancels the Summoning");
}
}
int main(int argc,char**argv){
 if(argc<2||!load_pack(argv[1]))return 2;
 g_dump=arg_after(argc,argv,"--dump");
 if(argc>2&&std::string(argv[2])!="--dump"&&!load_real_art(argv[2]))return 2;
 for(int s=0;s<3;++s)menus(s);
 std::printf("UI polish handheld runtime: %d/%d checks\n",checks-failures,checks);
 return failures?1:0;
}
