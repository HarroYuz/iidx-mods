// Native blue ADVANCED OPTION menu. Keep every version's built-in cards and
// append practice cards in an expanded, private copy of the native menu object.
struct Descriptor {int count;unsigned char shared;unsigned char reserved[3];};
struct Row {int id,pad;Descriptor* descriptor;int value[2];};
struct Menu {unsigned char navigation[0x20];int cursor[2],tick,layer;Row rows[21];int count,pad;void* native_graphics[3]{};};
static_assert(sizeof(Row)==0x18 && offsetof(Menu,count)==0x228);
Menu menu{};Descriptor rate_descriptor{17,1,{}},scratch_descriptor{2,1,{}};
bool menu_copied=false;int rendering_id=-1;
std::string config_path="practice-mod/settings.ini";
int rate_id(){return addresses::menu_rows();}
int scratch_id(){return rate_id()+1;}
bool custom_course(){
 auto mod=GetModuleHandleW(L"2dx-free-dan.dll");
 BOOL(*query)()=nullptr;
 if(mod){const auto proc=GetProcAddress(mod,"SelfDanCourseActiveV1");static_assert(sizeof(query)==sizeof(proc));std::memcpy(&query,&proc,sizeof(query));}
 return query && query();
}
// FireNet's playlist/settings/recording plugin uses the native EA path,
// already protected by the per-song record and request guards. Loading it
// must not disable practice options. Independent exporters remain conflicts.
bool external_uploader(){return GetModuleHandleW(L"IIDXScore.dll") || GetModuleHandleW(L"2dxtra.dll") || GetModuleHandleW(L"2dx-rate.dll");}
bool custom_menu_locked(){
 if(!custom_course())return false;
 const auto mod=GetModuleHandleW(L"2dx-free-dan.dll");
 const auto proc=mod?GetProcAddress(mod,"SelfDanOptionsEditableV1"):nullptr;
 BOOL(*query)(){};static_assert(sizeof(query)==sizeof(proc));std::memcpy(&query,&proc,sizeof(query));
 // Older free-dan versions do not expose per-song selection state.
 return !query || !query();
}
bool editable(){
 const int scene=*native<int*>(0x107ecf0);
 return !course_locked && !custom_menu_locked() && (scene==23 || scene==28 || scene==25) && !external_uploader();
}
void sync_menu(){menu.rows[scratch_id()].value[0]=menu.rows[scratch_id()].value[1]=scratch_selected.load()?0:1;menu.rows[rate_id()].value[0]=menu.rows[rate_id()].value[1]=(150-selected.load())/5;}
int build_menu(void* object,int mode){
 const int count=native<int(*)(void*,int)>(0x80a090)(object,mode);
 if(count!=addresses::menu_rows() || object!=&menu)fatal("Advanced menu layout changed.");
 menu.rows[rate_id()]={rate_id(),0,&rate_descriptor,{0,0}};menu.rows[scratch_id()]={scratch_id(),0,&scratch_descriptor,{1,1}};sync_menu();return count+2;
}
void initialize_menu(void*,int layer){
 if(!menu_copied){std::memcpy(&menu,native<void*>(0xa7e77c0),addresses::menu_count_offset());menu_copied=true;}
 native<void(*)(void*,int)>(0x809f60)(&menu,layer);
}
void update_menu(void*){sync_menu();native<void(*)(void*)>(0x80adb0)(&menu);}
void draw_menu(void*){sync_menu();native<void(*)(void*)>(0x8078c0)(&menu);rendering_id=-1;}
void set_option(void* object,int side,int id,int value){
 if(id<rate_id()){native<void(*)(void*,int,int,int)>(0x80a180)(object,side,id,value);return;}
 if(id==scratch_id() && editable() && value>=0 && value<2){
  scratch_selected=value==0;
  char path[MAX_PATH];GetFullPathNameA(config_path.c_str(),MAX_PATH,path,nullptr);
  WritePrivateProfileStringA("practice","all_scratch",scratch_selected?"1":"0",path);
 }
 if(id==rate_id() && editable() && value>=0 && value<17){
  selected=150-value*5;
  char number[8];std::snprintf(number,sizeof(number),"%d",selected.load());
  char path[MAX_PATH];GetFullPathNameA(config_path.c_str(),MAX_PATH,path,nullptr);
  WritePrivateProfileStringA("practice","rate_percent",number,path);
  log("in-game rate selected");
 }
 sync_menu();
}
void save_cursor(void* manager,int side,int value,int style){
 // The extra card belongs to the mod, never to the server's option schema.
 native<void(*)(void*,int,int,int)>(0x897680)(manager,side,value>=rate_id()?0:value,style);
}
const char* title(void* object,int id){
 rendering_id=id;
 if(id<rate_id())return native<const char*(*)(void*,int)>(0x808760)(object,id);
 if(id==scratch_id())return scratch_selected?"d_op_allscr_on":"d_op_allscr_off";
 static char name[32];std::snprintf(name,sizeof(name),"d_op_rate_%03d",selected.load());return name;
}
const char* description(void* object,int id){
 if(id<rate_id())return native<const char*(*)(void*,int)>(0x8085f0)(object,id);
 if(external_uploader())return "txt_d_op_rate_conflict";
 if(course_locked || custom_menu_locked())return "txt_d_op_rate_locked";
 if(id==scratch_id())return "txt_d_op_allscr";
 return (selected.load()!=100 || scratch_selected.load())?"txt_d_op_rate_nosave":"txt_d_op_rate";
}
const char* preview(void* object,int id,int value,int side){
 if(id<rate_id())return native<const char*(*)(void*,int,int,int)>(0x809890)(object,id,value,side);
 if(id==scratch_id())return "allscr_preview";
 return (selected.load()!=100 || scratch_selected.load())?"rate_preview_nosave":"rate_preview";
}
void* cursor_sprite(void* manager,const char* name,int x,int y,float scale,int layer,bool flag){
 if(rendering_id==rate_id())name="rate_blank";
 return native<void*(*)(void*,const char*,int,int,float,int,bool)>(0x349990)(manager,name,x,y,scale,layer,flag);
}
self_dan::CallHook menu_calls[12];
struct CountPatch {size_t displacement;};
std::vector<CountPatch> count_patches;
void prepare_menu(){
 const size_t sites[]={0x80593e,0x806252,0x806b8b,0x806ba5,0x809f98,0x80af7a,0x80afb0,0x80ae65,0x807c3c,0x807ad0,0x807b0a,0x8083f8};
 const size_t targets[]={0x809f60,0x809f60,0x80adb0,0x8078c0,0x80a090,0x80a180,0x80a180,0x897680,0x808760,0x8085f0,0x809890,0x349990};
 void* wrappers[]={reinterpret_cast<void*>(&initialize_menu),reinterpret_cast<void*>(&initialize_menu),reinterpret_cast<void*>(&update_menu),reinterpret_cast<void*>(&draw_menu),reinterpret_cast<void*>(&build_menu),reinterpret_cast<void*>(&set_option),reinterpret_cast<void*>(&set_option),reinterpret_cast<void*>(&save_cursor),reinterpret_cast<void*>(&title),reinterpret_cast<void*>(&description),reinterpret_cast<void*>(&preview),reinterpret_cast<void*>(&cursor_sprite)};
 for(int i=0;i<12;++i){auto& h=menu_calls[i];h.site=native<uint8_t*>(sites[i]);h.expected_target=native<void*>(targets[i]);h.replacement=wrappers[i];h.opcode=i==3?0xe9:0xe8;}
 if(!self_dan::prepare_calls(menu_calls,12))fatal("Advanced menu call conflict.");
 count_patches.clear();
 for(auto operand:addresses::menu_count_operands())count_patches.push_back({operand});
 for(auto patch:count_patches)if(*reinterpret_cast<uint32_t*>(base+patch.displacement)!=addresses::menu_count_offset())fatal("Advanced menu count layout mismatch.");
 if(addresses::current)for(const auto& patch:addresses::menu_tail_operands)
  if(*reinterpret_cast<uint32_t*>(base+patch.rva)!=patch.before)fatal("Advanced menu graphics layout mismatch.");
 char path[MAX_PATH];GetFullPathNameA(config_path.c_str(),MAX_PATH,path,nullptr);
 const int saved=GetPrivateProfileIntA("practice","rate_percent",100,path);
 scratch_selected=GetPrivateProfileIntA("practice","all_scratch",0,path)!=0 && !external_uploader();
 selected=rate_mod::valid_rate(saved) && !external_uploader()?saved:100;
}
void publish_menu(){
 // Owning scene thread, before any menu input/draw. Fail closed on conflicts.
 for(auto patch:count_patches){
  auto address=reinterpret_cast<uint8_t*>(base+patch.displacement);DWORD old{},ignored{};
  if(*reinterpret_cast<uint32_t*>(address)!=addresses::menu_count_offset() || !VirtualProtect(address,4,PAGE_EXECUTE_READWRITE,&old))fatal("Advanced menu count conflict.");
  const uint32_t expanded=offsetof(Menu,count);std::memcpy(address,&expanded,sizeof(expanded));
  if(!VirtualProtect(address,4,old,&ignored) || !FlushInstructionCache(GetCurrentProcess(),address,4))fatal("Advanced menu protection failed.");
 }
 if(addresses::current)for(const auto& patch:addresses::menu_tail_operands){
  auto address=reinterpret_cast<uint32_t*>(base+patch.rva);DWORD old{},ignored{};
  if(*address!=patch.before || !VirtualProtect(address,4,PAGE_EXECUTE_READWRITE,&old))fatal("Advanced menu graphics conflict.");
  *address=patch.after;
  if(!VirtualProtect(address,4,old,&ignored) || !FlushInstructionCache(GetCurrentProcess(),address,4))fatal("Advanced menu graphics protection failed.");
 }
 if(!self_dan::install_calls(menu_calls,12))fatal("Advanced menu publication failed.");
}
