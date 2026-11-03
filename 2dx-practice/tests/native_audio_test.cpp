#include "../src/core.hpp"
#include <windows.h>
#include <cassert>
#include <iostream>
#include <cstring>
#include "../src/addresses.hpp"
template<class T>T& at(void* p,size_t o){return *reinterpret_cast<T*>(static_cast<char*>(p)+o);}
int main(){
 auto game=LoadLibraryExW(L"bm2dx.dll",nullptr,DONT_RESOLVE_DLL_REFERENCES);assert(game);
 uintptr_t base=reinterpret_cast<uintptr_t>(game);
 assert(addresses::select(base));
 using Feed=int(*)(void*,float*,int,float*,float*);auto feed=reinterpret_cast<Feed>(base+addresses::rva(0x4cb050));
 alignas(16) char source[128]{},format[32]{},feeder[0x8100]{};
 const int length=10003;std::vector<int16_t> pcm(length*2+32);
 for(int i=0;i<length;++i){pcm[i*2]=int16_t((i%500)*40);pcm[i*2+1]=-pcm[i*2];}
 at<uintptr_t>(source,0)=base+addresses::rva(0xe6d080);at<void*>(source,0x28)=format;at<void*>(source,0x38)=pcm.data();at<int>(source,0x40)=length*4;
 at<uint16_t>(format,0)=1;at<uint16_t>(format,2)=2;at<uint32_t>(format,4)=44100;at<uint16_t>(format,12)=4;at<uint16_t>(format,14)=16;
 at<void*>(feeder,8)=source;at<int>(feeder,0x18)=length*4;at<float>(feeder,0x20)=0.75f;at<float>(feeder,0x24)=0.5f;
 at<void*>(feeder,0x40)=feeder+0x50;at<size_t>(feeder,0x48)=0x8000;
 for(int p=70;p<=150;p+=5){
  for(int block:{1,7,256,1000}){
   at<int>(feeder,0x34)=0;rate_mod::Stream stream;std::vector<float> out(block*2+32);int count=0;float l{},r{};
   for(;;){int got=stream.render(out.data(),block,p,[&](float* dst,int n){int bytes=feed(feeder,dst,n*8,&l,&r);assert(bytes>=0 && bytes%8==0);return bytes/8;});
    for(int j=0;j<got;++j){double pos=double(int64_t(count+j)*p)/100;int a=std::min(int(pos),length-1),b=std::min(a+1,length-1);float w=float(pos-std::floor(pos));float expected=(pcm[a*2]+(pcm[b*2]-pcm[a*2])*w)/32767.0f;assert(std::abs(out[j*2]-expected)<1e-5);assert(out[j*2+1]==-out[j*2]);}
    count+=got;if(got<block)break;
   }
   assert(count==(length*100+p-1)/p);assert(l==0.75f && r==0.5f);
  }
 }
 // Real native voice clock: return type is double, milliseconds from source cursor.
 char voice[0xa0]{},connection[32]{};void* connections[]={connection};
 at<void*>(connection,8)=feeder;at<void*>(voice,0x38)=connections;at<void*>(voice,0x40)=connections+1;at<void*>(voice,0x50)=source;at<int>(voice,0x88)=-1;at<int>(feeder,0x34)=4410*4;
 auto clock=reinterpret_cast<double(*)(void*)>(base+addresses::rva(0x4c30a0));assert(std::abs(clock(voice)-100.0)<1e-8);
 std::cout<<"PASS: actual game PCM feeder ABI/output at all 17 rates, four block sizes, gain and actual double-precision voice clock\n";
 FreeLibrary(game);
}
