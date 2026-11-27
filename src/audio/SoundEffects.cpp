#include "audio/SoundEffects.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <cstdint>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif
namespace lf {
struct SoundEffects::Impl {
    static constexpr int count=int(SoundCue::Count);
    std::array<std::vector<std::vector<short>>,count> samples;
    std::array<int,count> previous;
    Rng variationRng{uint32_t(std::chrono::steady_clock::now().time_since_epoch().count())};
    Impl(){previous.fill(-1);}
    int lastCue=-1,lastVariant=-1;
#ifdef _WIN32
    HWAVEOUT device=nullptr;WAVEHDR header{};bool prepared=false;int priority=0;
    ~Impl(){if(device){waveOutReset(device);if(prepared)waveOutUnprepareHeader(device,&header,sizeof(header));waveOutClose(device);}}
#endif
};
SoundEffects::SoundEffects():impl_(new Impl){}
SoundEffects::~SoundEffects()=default;
void SoundEffects::init(const std::string& directory) {
    const char* files[]={"click","cast","splash","bite","reel","catch","purchase","coins","linebreak","dayend","equip","open","close","reject","unlock","accept","complete","poison","release","secret","nobait"};
    for(int i=0;i<Impl::count;++i)for(int variant=0;variant<5;++variant){
        std::string suffix=variant?"_"+std::to_string(variant+1):"";
        std::ifstream f(directory+"/"+files[i]+suffix+".wav",std::ios::binary);
        char h[44]{};if(!f.read(h,44))continue;
        // These original assets use canonical mono PCM16 / 22050 Hz WAV.
        auto u16=[&](int o){return (unsigned char)h[o]|((unsigned char)h[o+1]<<8);};
        auto u32=[&](int o){return uint32_t(u16(o))|(uint32_t(u16(o+2))<<16);};
        if(std::string(h,4)!="RIFF" || std::string(h+8,4)!="WAVE" || std::string(h+36,4)!="data" || u16(20)!=1 || u16(22)!=1 || u32(24)!=22050 || u16(34)!=16)continue;
        unsigned bytes=u32(40);if(!bytes || bytes>22050*4 || bytes%2)continue;
        std::vector<short> s(bytes/2);if(f.read(reinterpret_cast<char*>(s.data()),bytes))impl_->samples[i].push_back(std::move(s));
    }
#ifdef _WIN32
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;format.nSamplesPerSec=22050;format.wBitsPerSample=16;format.nBlockAlign=2;format.nAvgBytesPerSec=44100;
    if(waveOutOpen(&impl_->device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)impl_->device=nullptr;
#endif
}
SoundEffects::Status SoundEffects::status() const {
    Status result;for(const auto& samples:impl_->samples)if(!samples.empty()){++result.loaded;result.variants+=int(samples.size());}
    result.lastCue=impl_->lastCue;result.lastVariant=impl_->lastVariant;
#ifdef _WIN32
    result.device=impl_->device!=nullptr;result.busy=impl_->prepared && !(impl_->header.dwFlags&WHDR_DONE);result.priority=impl_->priority;
#endif
    return result;
}
void SoundEffects::update(const std::vector<SoundCue>& events,float volume) {
#ifdef _WIN32
    auto& p=*impl_;if(!p.device)return;
    volume=std::clamp(volume,0.f,1.f);DWORD gain=DWORD(volume*65535);waveOutSetVolume(p.device,gain|(gain<<16));
    if(volume==0){waveOutReset(p.device);return;}
    auto priority=[](int cue) {
        switch(SoundCue(cue)) {
        case SoundCue::Coins:case SoundCue::DayEnd:case SoundCue::Secret:case SoundCue::NoBait:return 4;
        case SoundCue::Reel:return 0;
        case SoundCue::Click:case SoundCue::Open:case SoundCue::Close:return 1;
        case SoundCue::Bite:case SoundCue::Catch:case SoundCue::LineBreak:case SoundCue::Poison:return 3;
        default:return 2;
        }
    };
    int best=-1;for(auto cue:events){int i=int(cue);if(i>=0 && i<Impl::count && !p.samples[i].empty() && (best<0 || priority(i)>=priority(best)))best=i;}
    if(best<0)return;
    bool busy=p.prepared && !(p.header.dwFlags&WHDR_DONE);
    if(busy && priority(best)<=p.priority)return; // reel/click never mask a bite or receipt
    if(busy)waveOutReset(p.device);
    if(p.prepared){waveOutUnprepareHeader(p.device,&p.header,sizeof(p.header));p.prepared=false;}
    auto& variants=p.samples[best];int count=int(variants.size()),chosen=0;
    if(count>1){chosen=p.variationRng.irange(0,count-(p.previous[best]>=0?2:1));if(p.previous[best]>=0 && chosen>=p.previous[best])++chosen;}
    p.previous[best]=chosen;p.lastCue=best;p.lastVariant=chosen;auto& sample=variants[chosen];
    p.header={};p.header.lpData=reinterpret_cast<LPSTR>(sample.data());p.header.dwBufferLength=DWORD(sample.size()*2);
    if(waveOutPrepareHeader(p.device,&p.header,sizeof(p.header))!=MMSYSERR_NOERROR)return;
    p.prepared=true;p.priority=priority(best);waveOutWrite(p.device,&p.header,sizeof(p.header));
#else
    (void)events;(void)volume;
#endif
}
}
