#pragma once
// Fixture entrypoints use the same UTF-8 argv contract as production adapters.
#ifdef _WIN32
int wmain(int argc,wchar_t** wide){
  _setmode(_fileno(stdout),_O_BINARY);
  try{
    std::vector<std::string> words;for(int i=0;i<argc;++i){const auto bytes=std::filesystem::path(wide[i]).u8string();words.emplace_back(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
    std::vector<char*> argv;for(auto&word:words)argv.push_back(word.data());argv.push_back(nullptr);
    return run_cli(argc,argv.data());
  }catch(const std::exception&){return dslvm::failure_frame("cli",dslvm::Error("CLI_USAGE","invalid argument encoding"));}
}
#else
int main(int argc,char**argv){return run_cli(argc,argv);}
#endif
