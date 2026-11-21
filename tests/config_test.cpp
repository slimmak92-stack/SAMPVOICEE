#include "../client/PluginConfig.h"
#include <cstdio>
#include <fstream>
#include <vector>
static int f=0;
#define CHECK(c,m) do{ if(c) printf("ok   : %s\n",m); else {printf("FAIL: %s\n",m); ++f;} }while(0)
int main(){
  // new fields round-trip
  PluginConfig::Reset();
  PluginConfig::SetVoiceEffect(13); PluginConfig::SetLowCut(true); PluginConfig::SetCompressor(true); PluginConfig::SetCustomPitch(7);
  PluginConfig::SetGateSensitivity(62); PluginConfig::SetIconStyle(2);
  CHECK(PluginConfig::Save("/tmp/new.cfg"),"save");
  PluginConfig::Reset(); CHECK(!PluginConfig::GetLowCut()&&PluginConfig::GetCustomPitch()==0,"reset restores defaults");
  CHECK(PluginConfig::Load("/tmp/new.cfg"),"load");
  CHECK(PluginConfig::GetVoiceEffect()==13,"effect 13 (Custom pitch) accepted");
  CHECK(PluginConfig::GetLowCut()&&PluginConfig::GetCompressor()&&PluginConfig::GetCustomPitch()==7,"lowcut / compressor / pitch persisted");
  CHECK(PluginConfig::GetGateSensitivity()==62&&PluginConfig::GetIconStyle()==2,"older extension block still persisted");
  // pitch clamp
  PluginConfig::SetCustomPitch(99); CHECK(PluginConfig::GetCustomPitch()==12,"pitch clamps"); PluginConfig::SetCustomPitch(-99); CHECK(PluginConfig::GetCustomPitch()==-12,"pitch clamps low");
  // backward compatibility: strip the second block (last 4+1+1+4 = 10 bytes) -> still loads, new fields default
  std::ifstream in("/tmp/new.cfg",std::ios::binary); std::vector<char> b((std::istreambuf_iterator<char>(in)),{}); in.close();
  std::ofstream o("/tmp/old.cfg",std::ios::binary|std::ios::trunc); o.write(b.data(),b.size()-10); o.close();
  PluginConfig::Reset(); CHECK(PluginConfig::Load("/tmp/old.cfg"),"previous-build config (no 2nd block) still loads");
  CHECK(PluginConfig::GetVoiceEffect()==13&&PluginConfig::GetGateSensitivity()==62,"old values intact"); CHECK(!PluginConfig::GetLowCut()&&!PluginConfig::GetCompressor()&&PluginConfig::GetCustomPitch()==0,"new fields default on old file");
  // a truncated/older still-older file: strip both extension blocks and the effect
  std::ofstream o2("/tmp/older.cfg",std::ios::binary|std::ios::trunc); o2.write(b.data(),b.size()-10-4-1-4-1-1-4-1-1-4-4-4-4-4); o2.close();
  PluginConfig::Reset(); printf("     older-file load -> %d\n",(int)PluginConfig::Load("/tmp/older.cfg"));
  // effect index out of range is rejected, not applied
  std::vector<char> c=b; // find effect int: it sits right before ext magic 'ETX1' = 0x31585445
  for(size_t i=4;i+8<c.size();++i){ if(*(int*)&c[i]==0x31585445){ int bad=99; memcpy(&c[i-4],&bad,4); break; } }
  std::ofstream o3("/tmp/bad.cfg",std::ios::binary|std::ios::trunc); o3.write(c.data(),c.size()); o3.close();
  PluginConfig::Reset(); PluginConfig::Load("/tmp/bad.cfg"); CHECK(PluginConfig::GetVoiceEffect()==0,"out-of-range effect index falls back to Normal");
  printf("\n%s\n",f?"FAILED":"ALL PASSED"); return f; }
