#include "../client/VoiceDsp.h"
#include <cstdio>
#include <vector>
#include <random>
#include <ctime>
using namespace voice_dsp;
static const float SR=48000; static const size_t FR=480;
static int fails=0;
#define CHECK(c,msg) do{ if(!(c)){printf("FAIL: %s\n",msg);++fails;} else printf("ok   : %s\n",msg);}while(0)

// speech-like: 140 Hz harmonic stack, 4 Hz syllable envelope, amplitude amp
static std::vector<float> voice(double sec,float amp,float f0=140.f){
  std::vector<float> v((size_t)(sec*SR));
  for(size_t i=0;i<v.size();++i){double t=i/SR; double e=0.55+0.45*std::sin(2*M_PI*4*t); double s=0;
    for(int h=1;h<=8;++h) s+=std::sin(2*M_PI*f0*h*t)/h; v[i]=(float)(amp*e*s*0.45);} return v;}
static std::vector<float> run(Processor&p,std::vector<float> in){ std::vector<float> out(in.size());
  for(size_t i=0;i+FR<=in.size();i+=FR) p.Process(&in[i],&out[i],FR); return out;}
static double rms(const std::vector<float>&v,size_t a,size_t b){double e=0;for(size_t i=a;i<b;++i)e+=v[i]*v[i];return std::sqrt(e/(b-a));}
static float pk(const std::vector<float>&v){float m=0;for(float x:v)m=std::max(m,std::fabs(x));return m;}
static double zc(const std::vector<float>&v,size_t a,size_t b){int c=0;for(size_t i=a+1;i<b;++i) if((v[i-1]<0)!=(v[i]<0))++c; return c/2.0/((b-a)/SR);}
static bool finite(const std::vector<float>&v){for(float x:v) if(!std::isfinite(x)) return false; return true;}
static Processor* mk(int e){auto*p=new Processor; p->Initialize(SR); p->SetGate(false,0.004f); p->SetEffect(e); return p;}

int main(){
  auto in=voice(2.0,0.5f);
  // 1) every effect: finite, bounded, not silent
  const char* names[]={"None","Deep","High","Robot","Radio","Giant","Chipmunk","Megaphone","Telephone","Echo","Cave","Alien","Underwater","Custom"};
  static_assert(EffectCount==14,"count");
  for(int e=0;e<EffectCount;++e){ auto*p=mk(e); if(e==EffectCustom) p->SetCustomPitch(5); auto o=run(*p,in);
    char m[96]; bool ok=finite(o)&&pk(o)<=1.0001f&&rms(o,24000,90000)>0.01; snprintf(m,96,"effect %-10s finite, <=1.0, not silent (rms %.3f pk %.3f)",names[e],rms(o,24000,90000),pk(o)); CHECK(ok,m); delete p; }
  // 2) pitch direction / amount (zero crossing rate of the fundamental region via low band)
  { auto base=zc(in,24000,90000);
    auto*pd=mk(EffectDeep); auto d=run(*pd,in); auto*ph=mk(EffectHigh); auto h=run(*ph,in);
    auto*pg=mk(EffectGiant); auto g=run(*pg,in); auto*pc=mk(EffectChipmunk); auto c=run(*pc,in);
    printf("     zcr base %.0f deep %.0f giant %.0f high %.0f chip %.0f\n",base,zc(d,24000,90000),zc(g,24000,90000),zc(h,24000,90000),zc(c,24000,90000));
    CHECK(zc(g,24000,90000)<zc(d,24000,90000) && zc(d,24000,90000)<base,"Giant < Deep < Normal pitch");
    CHECK(zc(h,24000,90000)>base && zc(c,24000,90000)>zc(h,24000,90000),"Chipmunk > High > Normal pitch"); }
  // 3) custom pitch +12 ~ 2x, -12 ~ 0.5x, 0 = identical to input path
  { auto base=zc(in,24000,90000); auto*p=mk(EffectCustom); p->SetCustomPitch(12); auto o=run(*p,in); double r=zc(o,24000,90000)/base; printf("     +12st ratio %.2f\n",r); CHECK(r>1.7&&r<2.3,"Custom +12 st ~ doubles pitch");
    auto*q=mk(EffectCustom); q->SetCustomPitch(-12); auto o2=run(*q,in); r=zc(o2,24000,90000)/base; printf("     -12st ratio %.2f\n",r); CHECK(r>0.4&&r<0.62,"Custom -12 st ~ halves pitch");
    auto*z=mk(EffectCustom); z->SetCustomPitch(0); auto o0=run(*z,in); auto*n=mk(EffectNone); auto on=run(*n,in); double d=0; for(size_t i=0;i<on.size();++i) d=std::max(d,(double)std::fabs(on[i]-o0[i])); CHECK(d<1e-6,"Custom 0 st == Normal"); 
    p->SetCustomPitch(99); CHECK(p->GetCustomPitch()==12,"pitch clamps to +12"); }
  // 4) echo: after the voice stops, a delayed copy is still audible
  { std::vector<float> x=voice(0.5,0.5f); x.resize((size_t)(1.0*SR),0.f); auto*pe=mk(EffectEcho); auto e=run(*pe,x); auto*pn=mk(EffectNone); auto n=run(*pn,x);
    double tail_e=rms(e,(size_t)(0.60*SR),(size_t)(0.80*SR)), tail_n=rms(n,(size_t)(0.60*SR),(size_t)(0.80*SR)); printf("     tail echo %.4f vs dry %.4f\n",tail_e,tail_n); CHECK(tail_e>0.01&&tail_n<1e-4,"Echo leaves a tail, normal does not"); }
  // 5) cave reverb: tail present and decays (stable)
  { std::vector<float> x=voice(0.4,0.5f); x.resize((size_t)(3.0*SR),0.f); auto*pc=mk(EffectCave); auto e=run(*pc,x);
    double t1=rms(e,(size_t)(0.5*SR),(size_t)(0.7*SR)), t2=rms(e,(size_t)(2.5*SR),(size_t)(2.9*SR)); printf("     reverb tail %.4f -> %.5f\n",t1,t2); CHECK(t1>0.005&&t2<t1*0.5,"Cave reverb has a tail that decays"); }
  // 6) low cut kills 40 Hz, keeps 400 Hz
  { std::vector<float> lo((size_t)(1*SR)),hi(lo.size()); for(size_t i=0;i<lo.size();++i){lo[i]=0.3f*std::sin(2*M_PI*40*i/SR);hi[i]=0.3f*std::sin(2*M_PI*400*i/SR);}
    auto*a=mk(EffectNone); a->SetLowCut(true); auto ol=run(*a,lo); auto*b=mk(EffectNone); b->SetLowCut(true); auto oh=run(*b,hi);
    double rl=rms(ol,24000,48000)/rms(lo,24000,48000), rh=rms(oh,24000,48000)/rms(hi,24000,48000); printf("     lowcut 40Hz x%.2f  400Hz x%.2f\n",rl,rh); CHECK(rl<0.35&&rh>0.95,"Low cut removes 40 Hz, keeps 400 Hz"); }
  // 7) compressor narrows loud/quiet ratio
  { std::vector<float> x((size_t)(2*SR)); for(size_t i=0;i<x.size();++i){ float a=(i<SR)?0.7f:0.07f; x[i]=a*std::sin(2*M_PI*300*i/SR);} 
    auto*a=mk(EffectNone); auto o0=run(*a,x); auto*b=mk(EffectNone); b->SetCompressor(true); auto o1=run(*b,x);
    double r0=rms(o0,24000,40000)/rms(o0,72000,90000), r1=rms(o1,24000,40000)/rms(o1,72000,90000); printf("     loud/quiet ratio %.1f -> %.1f\n",r0,r1); CHECK(r1<r0*0.6,"Compressor reduces the loud/quiet ratio"); CHECK(pk(o1)<=1.0001f,"compressed output stays <= 1.0"); }
  // 8) gate still works with the new chain: noise-only gets attenuated
  { std::mt19937 g(1); std::normal_distribution<float> nd(0,0.0025f); std::vector<float> n((size_t)(2*SR)); for(auto&v:n) v=nd(g);
    auto*p=new Processor; p->Initialize(SR); p->SetGate(true,0.004f); p->SetEffect(EffectNone); auto o=run(*p,n); double ri=rms(n,48000,90000),ro=rms(o,48000,90000); printf("     gate noise %.5f -> %.5f\n",ri,ro); CHECK(ro<ri*0.3,"Gate attenuates noise-only input"); }
  // 9) level meters
  { auto*p=mk(EffectNone); std::vector<float> x(FR,0.4f),o(FR); p->Process(x.data(),o.data(),FR); CHECK(std::fabs(p->GetInputLevel()-0.4f)<1e-4,"input level meter reads peak"); CHECK(p->GetOutputLevel()>0.3f,"output level meter reads"); }
  // 10) aliasing in==out with everything on, plus effect switching mid-stream and NaN input safety
  { auto*p=mk(EffectCave); p->SetLowCut(true); p->SetCompressor(true); p->SetAutoGain(true); auto x=in; for(size_t i=0;i+FR<=x.size();i+=FR){ if((i/FR)%50==0) p->SetEffect(((i/FR)/50)%EffectCount); p->Process(&x[i],&x[i],FR);} CHECK(finite(x)&&pk(x)<=1.0001f,"all stages on + live effect switching: finite and bounded"); 
    std::vector<float> bad(FR,NAN),o(FR); p->Process(bad.data(),o.data(),FR); }
  // 11) speed: realtime factor for the heaviest chain
  { auto*p=mk(EffectCave); p->SetLowCut(true); p->SetCompressor(true); p->SetAutoGain(true); auto x=voice(10,0.5f); clock_t t=clock(); run(*p,x); double s=double(clock()-t)/CLOCKS_PER_SEC; printf("     10 s of audio in %.3f s (%.0fx realtime)\n",s,10/s); CHECK(s<1.0,"heavy chain runs far faster than realtime"); }
  printf("\n%s (%d failed)\n",fails?"FAILED":"ALL PASSED",fails); return fails?1:0; }
