#include <napi/native_api.h>
#include <atomic>
#include <cctype>
#include <memory>
#include <string>
#include <stdexcept>
#include <sys/stat.h>
#include "session_example.h"

namespace {
std::atomic<bool> busy{false}, cancelled{false};
struct Job { napi_async_work work{}; napi_deferred deferred{}; std::string root, id, result, error; };
std::string string_arg(napi_env env, napi_value value) {
    size_t size=0;
    if(napi_get_value_string_utf8(env,value,nullptr,0,&size)!=napi_ok || size>4096)throw std::runtime_error("invalid string argument");
    std::string text(size+1,'\0');
    if(napi_get_value_string_utf8(env,value,text.data(),text.size(),&size)!=napi_ok)throw std::runtime_error("string read failed");
    text.resize(size);return text;
}
void execute(napi_env,void * data) {
    auto & job=*static_cast<Job*>(data);
    try {
        const auto directory=job.root+"/sdk-"+job.id;
        if(mkdir(directory.c_str(),0700)!=0)throw std::runtime_error("run directory exists or is not writable");
        job.result=pico_sdk_suite(job.root+"/model.gguf",directory,cancelled);
    }catch(const std::exception & error){job.error=error.what();}
}
void complete(napi_env env,napi_status status,void * data) {
    std::unique_ptr<Job> job(static_cast<Job*>(data));
    napi_value text;
    if(status!=napi_ok || !job->error.empty()) {
        const std::string message=job->error.empty()?"async work failed":job->error;
        napi_create_string_utf8(env,message.c_str(),message.size(),&text);
        napi_value error;napi_create_error(env,nullptr,text,&error);napi_reject_deferred(env,job->deferred,error);
    }else{
        napi_create_string_utf8(env,job->result.c_str(),job->result.size(),&text);napi_resolve_deferred(env,job->deferred,text);
    }
    napi_delete_async_work(env,job->work);busy=false;
}
napi_value run(napi_env env,napi_callback_info info) {
    bool expected=false;
    if(!busy.compare_exchange_strong(expected,true)){napi_throw_error(env,nullptr,"SDK worker already active");return nullptr;}
    try {
        size_t count=2;napi_value args[2];napi_get_cb_info(env,info,&count,args,nullptr,nullptr);
        if(count!=2)throw std::runtime_error("root and fresh run id required");
        auto job=std::make_unique<Job>();job->root=string_arg(env,args[0]);job->id=string_arg(env,args[1]);
        if(job->root.empty() || job->root.front()!='/' || job->root.find('\0')!=std::string::npos || job->id.size()!=32)throw std::runtime_error("invalid application root/run id");
        for(const unsigned char c:job->id)if(!std::isxdigit(c))throw std::runtime_error("invalid run id");
        cancelled=false;napi_value promise,label;
        if(napi_create_promise(env,&job->deferred,&promise)!=napi_ok)throw std::runtime_error("promise create failed");
        napi_create_string_utf8(env,"InProcessLlamaCpu",NAPI_AUTO_LENGTH,&label);
        if(napi_create_async_work(env,nullptr,label,execute,complete,job.get(),&job->work)!=napi_ok)throw std::runtime_error("async create failed");
        if(napi_queue_async_work(env,job->work)!=napi_ok){napi_delete_async_work(env,job->work);throw std::runtime_error("async queue failed");}
        job.release();return promise;
    }catch(const std::exception & error){busy=false;napi_throw_error(env,nullptr,error.what());return nullptr;}
}
napi_value cancel(napi_env env,napi_callback_info) {const bool active=busy.load();if(active)cancelled=true;napi_value result;napi_get_boolean(env,active,&result);return result;}
napi_value running(napi_env env,napi_callback_info) {napi_value result;napi_get_boolean(env,busy.load(),&result);return result;}
napi_value init(napi_env env,napi_value exports) {
    napi_property_descriptor properties[]={
        {"run",nullptr,run,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"cancel",nullptr,cancel,nullptr,nullptr,nullptr,napi_default,nullptr},
        {"running",nullptr,running,nullptr,nullptr,nullptr,napi_default,nullptr}};
    napi_define_properties(env,exports,sizeof(properties)/sizeof(properties[0]),properties);return exports;
}
napi_module module={1,0,nullptr,init,"picosdk",nullptr,{0}};
__attribute__((constructor))void register_sdk(){napi_module_register(&module);}
}
