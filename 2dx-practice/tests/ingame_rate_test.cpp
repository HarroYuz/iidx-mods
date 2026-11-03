#include "../src/ingame_rate.cpp"
using namespace ingame_rate;
#include <cassert>
#include <iostream>
static int zero(){return 0;}
static int negative(){return -1;}
static bool valid_record(int,bool){return false;}
static void* null_song(){return nullptr;}
static int64_t qpc=0;
static int64_t qpc_frequency(){return 10000000;}
static int64_t qpc_counter(){return qpc;}
static double wall_clock(void* p){return *static_cast<double*>(p);}
static uint64_t video_deadline=0;
static int set_deadline(void*,uint64_t t){video_deadline=t;return 0;}
static void noop(){}
static void* manager(){static int x;return &x;}
static int active_side=0,input_layout=0;
static bool joined(int side){return side==active_side;}
static int layout(int){return input_layout;}
static int next_button=-1,nav_direction=0,saved_cursor=-1;
static bool button(int code){return code==next_button;}
static bool navigate(void*,int direction){return direction==nav_direction;}
static int refresh(void*){return 120;}
static void cursor_store(void*,int,int value,int){saved_cursor=value;}
static int dispatched=0;
static int dispatch(void*,int*,int id,void*,void*){dispatched=id;return 77;}
static void* getter_pair[2]{};
static void* getter(int,int){return getter_pair;}
static std::vector<int> stopped_banks;
static int preview_cancels=0;
static const char* drawn_sprite=nullptr;
static void* sprite(void*,const char* name,int,int,float,int,bool){drawn_sprite=name;return nullptr;}
static void stop_bank(void*,int bank){stopped_banks.push_back(bank);}
static void cancel_preview(){++preview_cancels;}
static const char* option_text="OFF";
static int option_side=-1,option_style=-1;
static void* format_options(void*,void* output,int side,int style,bool compact){
 assert(compact);option_side=side;option_style=style;
 std::memset(output,0,32);field<size_t>(output,0x18)=15;
 return native<void*(*)(void*,const char*,size_t)>(0x365840)(output,option_text,std::strlen(option_text));
}
static void* allocate(size_t bytes){return std::malloc(bytes);}
static void deallocate(void* data,size_t){std::free(data);}
static bool ready(){return true;}
static void* preview_voice{};
static int preview_position(){return int(voice_clock(preview_voice));}
template<class F>static void stub(size_t rva,F target){
 auto code=native<unsigned char*>(rva);DWORD old{},ignored{};
 assert(VirtualProtect(code,14,PAGE_EXECUTE_READWRITE,&old));
 const unsigned char jump[]={0xff,0x25,0,0,0,0};std::memcpy(code,jump,6);
 auto address=reinterpret_cast<uintptr_t>(target);std::memcpy(code+6,&address,8);
 assert(VirtualProtect(code,14,old,&ignored));assert(FlushInstructionCache(GetCurrentProcess(),code,14));
}
int main(){
 setbuf(stdout,nullptr);
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 auto game=LoadLibraryExW(L"bm2dx.dll",nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(game);
 base=reinterpret_cast<uintptr_t>(game);assert(addresses::select(base));percent=125;
 char temp[MAX_PATH]{};GetTempPathA(MAX_PATH,temp);config_path=std::string(temp)+"2dx-practice-test-"+std::to_string(GetCurrentProcessId())+".ini";
 std::puts("TEST prepare");
 prepare(base);publish();assert(published);
 stub(0xab7b80,&dispatch);
 assert(request(nullptr,nullptr,5,nullptr,nullptr)==77 && dispatched==5);
 for(auto& hook:calls)assert(self_dan::call_matches(hook,hook.after));
 for(auto& hook:menu_calls)assert(self_dan::call_matches(hook,hook.after));
 assert(self_dan::call_matches(result_call,result_call.after));
 for(auto patch:count_patches)assert(*reinterpret_cast<uint32_t*>(base+patch.displacement)==offsetof(Menu,count));
 assert(!tainted);selected=70;freeze_rate();assert(tainted && percent==70);
 selected=100;freeze_rate();assert(!tainted && percent==100);
 for(int rate=70;rate<=150;rate+=5){selected=rate;sync_menu();assert(menu.rows[rate_id()].value[0]==(150-rate)/5 && menu.rows[rate_id()].value[1]==(150-rate)/5);}
 assert(!course_locked);course_locked=true;assert(!editable());course_locked=false;
 assert(request(nullptr,nullptr,2,nullptr,nullptr)==77 && dispatched==2);
 for(int i=0;i<5;++i)assert(*slots[i].slot==slots[i].replacement);
 tainted=true;
 // The loaded 0917 table is the source of truth, independent of the policy.
 const std::pair<int,const char*> blocked[]={
  {5,"pc.save"},{6,"pc.save2pp"},{14,"pc.eaappliresult"},{26,"pc.setBestresult"},
  {31,"music.reg"},{32,"music.play"},{36,"music.nosave"},{40,"music.sc"},{41,"grade.raised"}};
 for(const auto& item:blocked){
  const auto* entry=practice_network::find(request_entries,item.first);assert(entry);
  assert(!std::strcmp(practice_network::method(*entry),item.second));
 }
 for(const auto& entry:request_entries){
  bool blocked_id=false;for(const auto& item:blocked)if(item.first==entry.id)blocked_id=true;
  dispatched=-1;
  assert(request(nullptr,nullptr,entry.id,nullptr,nullptr)==(blocked_id?3:77));
  assert(dispatched==(blocked_id?-1:entry.id));
 }
 // Re-login after 0.85x: retain the pending-save guard while allowing reads,
 // lifecycle and keepalive. Merely changing menu settings must not clear it.
 selected=85;freeze_rate();selected=100;
 for(int id:{57,60,2,29,30,8,9,15,43}){
  assert(request(nullptr,nullptr,id,nullptr,nullptr)==77 && dispatched==id);
 }
 assert(tainted && request(nullptr,nullptr,41,nullptr,nullptr)==3);
 freeze_rate();assert(!tainted);
 for(const auto& item:blocked)
  assert(request(nullptr,nullptr,item.first,nullptr,nullptr)==77 && dispatched==item.first);
 tainted=true;
 std::puts("TEST audio");
 alignas(16) char source[128]{},format[32]{},feeder[0x8100]{},voice[0xa0]{},connection[32]{};
 std::vector<int16_t> pcm(44100*2+32,10000);
 field<uintptr_t>(source,0)=base+addresses::rva(0xe6d080);field<void*>(source,0x28)=format;field<void*>(source,0x38)=pcm.data();field<int>(source,0x40)=44100*4;
 field<uint16_t>(format,0)=1;field<uint16_t>(format,2)=2;field<uint32_t>(format,4)=44100;field<uint16_t>(format,12)=4;field<uint16_t>(format,14)=16;
 field<void*>(feeder,8)=source;field<int>(feeder,0x18)=44100*4;field<float>(feeder,0x20)=1;field<float>(feeder,0x24)=1;field<void*>(feeder,0x40)=feeder+0x50;field<size_t>(feeder,0x48)=0x8000;
 void* connections[]={connection};field<void*>(connection,8)=feeder;field<void*>(voice,0x38)=connections;field<void*>(voice,0x40)=connections+1;field<void*>(voice,0x50)=source;field<int>(voice,0x88)=-1;
 float out[2048+32],l{},r{};
 // System voice stays at native speed and does not create resampler state.
 assert(feed(feeder,out,800,&l,&r)==800);assert(field<int>(feeder,0x34)==400);assert(streams.empty());
 tag_voice(voice,AudioRole::song);
 for(int p=70;p<=150;p+=5){
  percent=p;streams.clear();field<int>(feeder,0x34)=0;
  for(int i=1;i<=10;++i){assert(feed(feeder,out,800,&l,&r)==800);assert(std::abs(voice_clock(voice)-i*100.0*1000/44100)<1e-8);}
  voice_seek(voice,100.0);assert(field<int>(voice,0x88)==int(44100.0*(100.0*p/100)/1000));assert(streams.empty());
  field<int>(voice,0x88)=-1;
 }
 // External seek/restart resets phase and EOF state.
 percent=125;field<int>(feeder,0x34)=0;assert(feed(feeder,out,800,&l,&r)==800);assert(std::abs(voice_clock(voice)-100.0*1000/44100)<1e-8);
 std::puts("TEST preview");
 // Native loop boundaries, processed by the original feeder.
 streams.clear();field<int>(feeder,0x34)=0;field<bool>(feeder,0x28)=true;field<int>(feeder,0x2c)=0;field<int>(feeder,0x30)=400;
 for(int i=0;i<10;++i){assert(feed(feeder,out,800,&l,&r)==800);assert(std::abs(out[0]-10000.0/32767)<1e-6);}
 untag(voice);assert(sources.empty() && voices.empty());
 // Exercise native PCM with immediate preview changes, including crossing
 // 1.00 while samples remain buffered. Preview time stays in source units.
 streams.clear();field<bool>(feeder,0x28)=false;field<int>(feeder,0x34)=0;
 tag_voice(voice,AudioRole::preview);tainted=false;percent=85;
 double source_frames=0;
 for(int p:{70,75,150,95,100,105,70,100}){
  selected=p;assert(feed(feeder,out,800,&l,&r)==800);source_frames+=p;
  assert(std::abs(voice_clock(voice)-source_frames*1000/44100)<1e-8);
  assert(percent==85 && !tainted);
 }
 voice_seek(voice,100.0);assert(field<int>(voice,0x88)==4410);assert(streams.empty());field<int>(voice,0x88)=-1;
 // Real getter wrapper: a pooled gameplay voice/source becomes a system
 // voice, including bank-2 folder voices outside a playing stage.
 stub(0xa907d0,&getter);getter_pair[0]=voice;song_armed=true;
 *native<int*>(0x107ecf0)=58;get_voice(2,10);assert(voices.at(voice).role==AudioRole::song);
 *native<int*>(0x107ecf0)=23;get_voice(2,10);assert(voices.empty() && sources.empty());
 for(int bank:{0,1,2}){
  tag_voice(voice,AudioRole::song);get_voice(bank,12);field<int>(feeder,0x34)=0;
  assert(feed(feeder,out,800,&l,&r)==800 && field<int>(feeder,0x34)==400 && streams.empty());
 }
 get_voice(4,0);assert(voices.at(voice).role==AudioRole::preview);
 selected=70;field<int>(feeder,0x34)=0;feed(feeder,out,800,&l,&r);assert(!streams.empty());
 stub(0xa90400,&stop_bank);stub(0x5c5300,&manager);stub(0x5c4db0,&cancel_preview);
 enter_selection();assert((stopped_banks==std::vector<int>{2,4}) && preview_cancels==1);
 assert(!song_armed && voices.empty() && sources.empty() && streams.empty());
 assert(percent==85 && !tainted);
 // A different pooled voice reusing the same source must also purge the
 // previous role, including buffered EOF/phase state.
 alignas(16) char reused_voice[0xa0]{};field<void*>(reused_voice,0x50)=source;
 tag_voice(voice,AudioRole::song);tag_voice(reused_voice,AudioRole::system);
 assert(sources.empty() && voices.empty());
 // Execute the native selector BGM decision against a ten-second preview.
 // At 0.70 it must remain ducked beyond ten WALL seconds; at 1.50 it must
 // recover earlier. Its 9700 ms threshold always refers to source time.
 std::puts("TEST bgm");
 stub(0x972500,&zero);stub(0x5c4f10,&ready);stub(0x5c4bc0,&ready);stub(0x5c4b20,&preview_position);
 char bgm[0x50]{};preview_voice=voice;
 pcm.resize(441000*2+32,10000);field<void*>(source,0x38)=pcm.data();field<int>(source,0x40)=441000*4;field<int>(feeder,0x18)=441000*4;
 for(int p:{70,100,150}){
  selected=p;streams.clear();field<int>(feeder,0x34)=0;tag_voice(voice,AudioRole::preview);
  for(int block=1;block<=int(9.72*44100*100/p/1000)+1;++block){
   assert(feed(feeder,out,8000,&l,&r)==8000);
   const double position=block*1000.0*p/100*1000/44100;
   assert(std::abs(voice_clock(voice)-position)<1e-7);
   assert(native<int(*)(void*)>(0x972310)(bgm)==(int(position)>=1 && int(position)<=9699?2:1));
  }
 }
 untag(voice);assert(streams.empty());
 // The game's actual MSVC string assign handles SSO and heap growth. Both
 // result sides/styles get the frozen played rate, never the menu rate.
 std::puts("TEST result");
 stub(0x895b00,&format_options);stub(0xb1ab58,&allocate);stub(0xb1ab94,&deallocate);
 alignas(16) char label[32];
 for(bool scratch:{false,true})for(int side:{0,1})for(int style:{0,1})for(int p=70;p<=150;p+=5)for(const char* options:{"OFF","OFF/OFF","RANDOM","MIRROR / HARD / ASSIST / BATTLE"}){
  option_text=options;percent=p;selected=150;scratch_played=scratch;
  assert(result_options(nullptr,label,side,style,true)==label && option_side==side && option_style==style);
  const char* value=field<size_t>(label,0x18)<16?label:field<char*>(label,0);
  char suffix[16];std::snprintf(suffix,sizeof(suffix),"%d.%02dx",p/100,p%100);
  std::string expected=options;
  if(p!=100 || scratch){
   if(expected=="OFF" || expected=="OFF/OFF")expected.clear();
   if(scratch){if(!expected.empty())expected+=" / ";expected+="ALL-SCR";}
   if(p!=100){if(!expected.empty())expected+=" / ";expected+=suffix;}
  }
  assert(value==expected && field<size_t>(label,0x10)==expected.size());
  if(field<size_t>(label,0x18)>=16)std::free(field<void*>(label,0));
 }
 // Execute the actual native menu builder, initializer, navigation and input
 // against only peripheral input/profile stubs. The 17th card must survive
 // native count writes and wrap-around; inputs clamp at both boundaries.
 std::puts("TEST menu");
 // Resource creation requires initialized graphics; navigation and vector access stay native.
 if(addresses::current)stub(0x107f25b0,&noop);
 stub(0x948100,&zero);stub(0x808ee0,&zero);stub(0x805c30,&zero);
 stub(0x8841e0,&manager);stub(0x82ce30,&zero);stub(0x894860,&zero);
 stub(0x948120,&joined);stub(0x80ac70,&noop);stub(0x805bd0,&negative);
 stub(0x80a020,&navigate);stub(0x93a710,&manager);stub(0xa58aa0,&refresh);
 stub(0x593930,&button);stub(0x805c50,&zero);stub(0x807050,&noop);
 stub(0x807080,&noop);stub(0x5c49c0,&noop);stub(0x897680,&cursor_store);
 *native<int*>(0x107ecf0)=23;selected=100;initialize_menu(nullptr,9);
 assert(menu.count==rate_id()+2 && menu.layer==9 && menu.rows[rate_id()].id==rate_id() && menu.rows[rate_id()].descriptor==&rate_descriptor);
 for(int i=0;i<rate_id();++i)assert(menu.rows[i].id==i && menu.rows[i].descriptor==native<Descriptor*>(0xcea0f0)+i);
 menu.cursor[0]=0;nav_direction=1;update_menu(nullptr);assert(menu.cursor[0]==scratch_id() && saved_cursor==0);
 nav_direction=2;update_menu(nullptr);assert(menu.cursor[0]==0);
 nav_direction=0;menu.cursor[0]=rate_id();
 next_button=*native<int*>(0xceb080); // black keys decrement the native row index
 for(int p=105;p<=150;p+=5){update_menu(nullptr);assert(selected==p && menu.rows[rate_id()].value[1]==(150-p)/5);}
 update_menu(nullptr);assert(selected==150);
 next_button=*native<int*>(0xceb070); // white keys increment the native row index
 for(int p=145;p>=70;p-=5){update_menu(nullptr);assert(selected==p);}
 update_menu(nullptr);assert(selected==70);
 course_locked=true;next_button=*native<int*>(0xceb070);update_menu(nullptr);assert(selected==70);course_locked=false;
 for(int scene:{23,28,25}){*native<int*>(0x107ecf0)=scene;assert(editable());}
 *native<int*>(0x107ecf0)=56;assert(!editable());
 // SP 1P, SP 2P and DP input layouts all use black-up / white-down.
 stub(0x805c30,&layout);*native<int*>(0x107ecf0)=23;
 for(int side:{0,1})for(int input:{0,1,2}){
  active_side=side;input_layout=input;menu.cursor[side]=rate_id();selected=100;
  next_button=native<int*>(0xceb080)[input];update_menu(nullptr);assert(selected==105);
  next_button=native<int*>(0xceb070)[input];update_menu(nullptr);assert(selected==100);
 }
 active_side=0;input_layout=0;
 stub(0x349990,&sprite);
 rendering_id=scratch_id();cursor_sprite(nullptr,"sel_ad_option",0,0,1,0,true);
 assert(std::strcmp(drawn_sprite,"sel_ad_option")==0);
 rendering_id=rate_id();cursor_sprite(nullptr,"sel_ad_option",0,0,1,0,true);
 assert(std::strcmp(drawn_sprite,"rate_blank")==0);rendering_id=-1;
 *native<int*>(0x107ecf0)=23;menu.cursor[0]=scratch_id();scratch_selected=false;
 next_button=*native<int*>(0xceb080);update_menu(nullptr);assert(scratch_selected);
 selected=100;freeze_rate();assert(tainted && scratch_played);
 next_button=*native<int*>(0xceb070);update_menu(nullptr);assert(!scratch_selected);
 freeze_rate();assert(!tainted && !scratch_played);
 assert(request(nullptr,nullptr,5,nullptr,nullptr)==77);
 // Native persistence gates skip before dereferencing score/chart data;
 // normal-rate calls forward the native eligibility decision unchanged.
 std::puts("TEST persistence");
 stub(0x82d250,&valid_record);stub(0x8821e0,&manager);
 stub(0x8792f0,&noop);stub(0x9480f0,&null_song);stub(0x947f70,&zero);
 for(bool practice:{true,false}){
  tainted=practice;assert(invalid_record(0,false)==practice);
  native<void(*)(void*,int,int)>(0x92fd00)(nullptr,0,0);
  native<void(*)(void*)>(0x92fe10)(nullptr);
 }
 for(auto& hook:record_calls)assert(self_dan::call_matches(hook,hook.after));
 for(auto& hook:video_calls)assert(self_dan::call_matches(hook,hook.after));
 // Actual MF clock: independent decoder start times and 100 ns timebase.
 std::puts("TEST mf");
 stub(0xaf0304,&qpc_frequency);stub(0xaf02e8,&qpc_counter);
 stub(0xa61eb0,&noop);stub(0xa61f10,&noop);
 alignas(16) char movie[0x40]{},decoder[0x100]{};
 field<void*>(movie,0x18)=decoder;field<bool>(decoder,0x79)=true;
 field<int64_t>(decoder,0x80)=5000000000LL;
 *native<int*>(0x107ecf0)=58;song_armed=true;
 for(int rate=70;rate<=150;rate+=5){
  percent=rate;mf_start(movie,false);qpc=80000000;
  int64_t elapsed=0;assert(mf_clock(decoder,&elapsed)==&elapsed);
  assert(elapsed==30000000LL*rate/100);
  qpc=50000000;mf_clock(decoder,&elapsed);assert(elapsed==0);
  mf_stop(movie);qpc=80000000;mf_clock(decoder,&elapsed);assert(elapsed==30000000);
 }
 *native<int*>(0x107ecf0)=23;percent=70;mf_start(movie,false);
 int64_t elapsed=0;mf_clock(decoder,&elapsed);assert(elapsed==30000000);mf_stop(movie);
 // Actual WMV start and frame scheduler with substitute decoder/sink COM
 // objects. Verify the deadline sent to the decoder, not just our wrapper.
 std::puts("TEST wmv");
 alignas(16) char reader[0x18]{},context[0x90]{},decoder_com[8]{},sink_com[8]{};
 void* decoder_vtable[14]{};void* sink_vtable[6]{};
 decoder_vtable[13]=reinterpret_cast<void*>(&zero);decoder_vtable[12]=reinterpret_cast<void*>(&zero);
 sink_vtable[5]=reinterpret_cast<void*>(&set_deadline);
 field<void*>(decoder_com,0)=decoder_vtable;field<void*>(sink_com,0)=sink_vtable;
 field<void*>(context,0x18)=decoder_com;field<void*>(context,0x20)=sink_com;
 field<void*>(reader,0)=native<void*>(0xe73ab0);field<void*>(reader,8)=context;
 field<void*>(movie,0x18)=reader;field<void*>(movie,0x20)=sink_com;
 stub(0xa630a0,&noop);
 for(int scene:{58,23})for(int rate=70;rate<=150;rate+=5){
  *native<int*>(0x107ecf0)=scene;percent=rate;double wall=100000;
  field<Clock>(context,0x50)=&wall_clock;field<void*>(context,0x58)=&wall;
  field<bool>(context,0x70)=false;field<double>(context,0x68)=0;
  wmv_start(movie,false);wall+=3000;
  native<int(*)(void*)>(0x47d150)(context+8);
  // Native WMV clocks are milliseconds, converted to 100 ns deadlines.
  const uint64_t scaled_deadline=video_deadline;
  const double expected_seconds=3.0*(scene==58?rate:100)/100;
  assert(std::abs(double(scaled_deadline)-expected_seconds*10000000)<=2);
  wmv_stop(movie);assert(field<Clock>(context,0x50)==&wall_clock && field<void*>(context,0x58)==&wall);
 }
 selected=100;sync_menu();DeleteFileA(config_path.c_str());
 std::cout<<"PASS: standalone; editable native menu; ALL-SCR native cursor and compact OPTION; native PCM/preview/BGM; per-song upload and record guards; MF/WMV clocks at 17 rates\n";
 // Module intentionally stays mapped until process exit because installed relays are live.
}
