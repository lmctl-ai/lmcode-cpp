#include "../stdlib/invocation.hpp"
void invocation_header_smoke(){auto r=dslvm::InvocationResults::from_command({"program"},std::nullopt);dslvm::Registry registry;r.select(0,registry,{});r.complete(0);}
