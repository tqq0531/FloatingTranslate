#include "../src/TranslationModel.h"
#include <cassert>
#include <iostream>
#ifndef _WIN32
#include <dlfcn.h>
#endif
int main(int argc,char** argv){
#ifndef _WIN32
 if(argc!=4){std::cerr<<"models-directory runtime-library extensions-library\n";return 2;}
 void* dll=dlopen(argv[2],RTLD_NOW|RTLD_LOCAL);if(!dll){std::cerr<<dlerror()<<"\n";return 1;}
 auto get=(const OrtApiBase*(*)())dlsym(dll,"OrtGetApiBase");assert(get);Ort::InitApi(get()->GetApi(ORT_API_VERSION));
 fuyi::TranslationModel model;model.load(argv[1],argv[3]);
 for(auto text:{"你好","我今天去北京。","为什么不一起翻译了？"}){auto english=model.translate(text,[]{return false;});assert(!english.empty());if(std::string(text)=="你好")assert(english=="Hello.");if(std::string(text).find("北京")!=std::string::npos)assert(english.find("Beijing")!=std::string::npos);std::cout<<text<<" -> "<<english<<"\n";}
 assert(model.translate("已经取消的输入",[]{return true;}).empty());
 std::string longText;for(int i=0;i<500;i++)longText+="你好北京";bool rejected=false;try{model.translate(longText,[]{return false;});}catch(const std::length_error&){rejected=true;}assert(rejected);
 fuyi::TranslationModel reverse;reverse.load(std::filesystem::path(argv[1])/"en-zh",argv[3],true);
 for(auto text:{"hi","hi my name is jack","my name is jack","my name is Jack","Hi my name is Jack","I like music. I also like swimming.","Hello.","Have you worked out today?","How are you?","I want go to gym now","I want to go to the gym now"}){
  auto chinese=reverse.translate(text,[]{return false;});assert(!chinese.empty());
  if(std::string(text)=="hi my name is jack")assert(chinese=="你好，我的名字是杰克。");
  if(std::string(text)=="my name is jack")assert(chinese=="我的名字是杰克。");
  if(std::string(text)=="I like music. I also like swimming.")assert(chinese.find("音乐")!=std::string::npos && chinese.find("游泳")!=std::string::npos);
  if(std::string(text)=="hi")assert(chinese=="你好");
  if(std::string(text)=="Hi my name is Jack")assert(chinese=="你好，我的名字是杰克。");
  assert(std::any_of(chinese.begin(),chinese.end(),[](unsigned char c){return c>=0x80;}));
  std::cout<<text<<" -> "<<chinese<<"\n";
 }
 assert(reverse.translate("cancelled English",[]{return true;}).empty());
 bool reverseRejected=false;try{reverse.translate(std::string(4000,'a'),[]{return false;});}catch(const std::length_error&){reverseRejected=true;}assert(reverseRejected);
 std::cout<<"Shared C++ inference, cancellation and length checks passed\n";
#endif
}
