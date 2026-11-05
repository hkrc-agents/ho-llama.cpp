// Opt-in integrated official SO entry. CPU arithmetic and HTTP stay upstream.
#include "entry_support.h"
#include "intent_monitor.h"
#include <vector>

int llama_server(int argc,char **argv);

extern "C" __attribute__((visibility("default"))) void Main(NativeChildProcess_Args args){
    int code=125;
    try{
        pico::services::entry::Invocation invocation(args);
        const auto kind=invocation.launch.at("kind").get<std::string>();
        if(kind!="runner"&&kind!="standalone")throw std::runtime_error("invalid native server kind");
        std::vector<std::string> values;
        for(const auto &item:invocation.launch.at("argv")){
            auto value=item.get<std::string>();if(value.find('\0')!=std::string::npos)throw std::runtime_error("NUL in server argv");values.push_back(std::move(value));
        }
        std::vector<char *> argv;for(auto &value:values)argv.push_back(value.data());argv.push_back(nullptr);
        pico::services::entry::IntentMonitor monitor(invocation.bootstrap);
        code=llama_server(static_cast<int>(values.size()),argv.data());
        invocation.Returned(code);
    }catch(const std::exception &error){fprintf(stderr,"managed server SO failed: %s\n",error.what());}
    fflush(nullptr);_exit(code);
}
