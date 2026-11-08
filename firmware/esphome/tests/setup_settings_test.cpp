#include "settings.h"
#include <cassert>
#include <iostream>
using namespace esphome::clock_setup;
int main() {
  Credentials empty, saved, retained, cleared;
  assert(valid_record(empty));
  assert(!merge_credentials(empty,"user","",false,saved));
  assert(merge_credentials(empty,"user","sensitive&<\"password",false,saved));
  assert(valid_record(saved));
  assert(merge_credentials(saved,"updated-user","",false,retained));
  assert(std::string(retained.username)=="updated-user");
  assert(std::string(retained.password)=="sensitive&<\"password");
  assert(!merge_credentials(saved,"","replacement",false,retained));
  assert(!merge_credentials(saved,std::string(257,'u'),"password",false,retained));
  assert(!merge_credentials(saved,"user",std::string(257,'p'),false,retained));
  assert(!merge_credentials(saved,"user",std::string("a\0b",3),false,retained));
  assert(!merge_credentials(saved,"user\nname","password",false,retained));
  assert(merge_credentials(saved,"","",true,cleared));
  assert(!cleared.username[0] && !cleared.password[0] && valid_record(cleared));
  Credentials corrupt=saved;corrupt.version=999;assert(!valid_record(corrupt));
  corrupt=saved;std::memset(corrupt.password,'x',sizeof(corrupt.password));assert(!valid_record(corrupt));
  assert(constant_equal("abcdef","abcdef"));
  assert(!constant_equal("abcdef","abcdeg"));
  assert(!constant_equal("abcdef","abc"));
  ProviderSettings blank,libre,input;
  std::strcpy(input.address,"email@example.test");std::strcpy(input.secret,"libre-password");
  assert(merge_provider(blank,input,true,false,libre));
  ProviderSettings changed=input;changed.secret[0]=0;std::strcpy(changed.patient,"11111111-2222-3333-4444-555555555555");
  ProviderSettings retained_provider;assert(merge_provider(libre,changed,true,false,retained_provider));
  assert(std::string(retained_provider.secret)=="libre-password");
  ProviderSettings public_ns;std::strcpy(public_ns.address,"https://example.test/");std::strcpy(public_ns.mode,"Public");
  assert(merge_provider(blank,public_ns,false,false,retained_provider));
  std::strcpy(public_ns.address,"http://example.test");assert(!merge_provider(blank,public_ns,false,false,retained_provider));
  ProviderSettings forgotten;assert(merge_provider(libre,input,true,true,forgotten));assert(!forgotten.address[0]&&!forgotten.secret[0]);
  assert(std::string(saved.password)=="sensitive&<\"password"); // Separate provider merges never touch the Dexcom record.
  std::cout<<"Credential retention, explicit forgetting, bounds and stored-record validation tests passed.\n";
}
