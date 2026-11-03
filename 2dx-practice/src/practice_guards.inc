// Native record construction, profile achievements and cached best scores.
// Keep the result's score/grade presentation native. A practice stage is
// excluded BEFORE it can enter the account cache or a later upload batch.
bool invalid_record(int side,bool include_assist){
 return tainted.load() || free_dan_no_save() || native<bool(*)(int,bool)>(0x82d250)(side,include_assist);
}
constexpr size_t record_sites[]={
 0x6de6ba,0x88aabf,0x88d039,0x896d9f,0x896eac,
 0x915394,0x915ae0,0x915e7b,0x916158,0x9162f6,
 0x929bad,0x929e77,0x92aef6,0x92b12e,0x92b269,0x92b5a4,
 0x92f21e,0x92f3b4,0x92fd23,0x92fe35,0x931f00
};
std::vector<self_dan::CallHook> record_calls;
void prepare_practice(){
 record_calls.resize(addresses::current?addresses::current_record_sites.size():std::size(record_sites));
 for(size_t i=0;i<record_calls.size();++i){auto& h=record_calls[i];
  h.site=addresses::current?reinterpret_cast<uint8_t*>(base+addresses::current_record_sites[i]):native<uint8_t*>(record_sites[i]);h.expected_target=native<void*>(0x82d250);h.replacement=reinterpret_cast<void*>(&invalid_record);
 }
 if(!self_dan::prepare_calls(record_calls.data(),record_calls.size()))fatal("Native no-save guard conflict.");
}
void publish_practice(){if(!self_dan::install_calls(record_calls.data(),record_calls.size()))fatal("Cannot install native no-save guards.");}
