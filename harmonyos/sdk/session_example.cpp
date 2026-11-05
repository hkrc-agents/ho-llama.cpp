#include "session_example.h"
#include "llama.h"
#include "json.hpp"
#include <algorithm>
#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>

using json = nlohmann::ordered_json;
namespace {
std::atomic<int> live_models{0}, live_contexts{0};
void require(bool condition, const char * message) { if (!condition) throw std::runtime_error(message); }
struct Backend {
    Backend() { llama_backend_init(); }
    ~Backend() { llama_backend_free(); }
};
struct Model {
    llama_model * value;
    explicit Model(const std::string & path) {
        auto params = llama_model_default_params(); params.n_gpu_layers = 0;
        value = llama_model_load_from_file(path.c_str(), params);
        require(value != nullptr, "model load failed"); ++live_models;
    }
    ~Model() { llama_model_free(value); --live_models; }
    Model(const Model &) = delete;
};
struct Abort {
    std::atomic<bool> & external;
    std::atomic<int> polls{0}; int trip_after = 0;
    static bool check(void * opaque) {
        auto & state = *static_cast<Abort *>(opaque);
        const int count = ++state.polls;
        return state.external.load() || (state.trip_after > 0 && count >= state.trip_after);
    }
};
struct Session {
    std::shared_ptr<Model> owner;
    llama_context * value;
    Abort abort;
    std::mutex execution;
    Session(std::shared_ptr<Model> model, int threads, std::atomic<bool> & cancelled):
        owner(std::move(model)), abort{cancelled} {
        auto params = llama_context_default_params();
        params.n_ctx = 512; params.n_batch = 256; params.n_ubatch = 128;
        params.n_seq_max = 1; params.n_threads = params.n_threads_batch = threads;
        params.abort_callback = Abort::check; params.abort_callback_data = &abort;
        value = llama_init_from_model(owner->value, params);
        require(value != nullptr, "context creation failed"); ++live_contexts;
    }
    ~Session() { llama_free(value); --live_contexts; }
    Session(const Session &) = delete;
    // Each context is confined to one worker. Separate contexts may share the
    // model. Never free/reuse an aborted context as if decode had rolled back.
    int decode(const std::vector<llama_token> & tokens, int position) {
        require(!abort.external.load(), "cancelled");
        require(!tokens.empty() && tokens.size() <= 256, "invalid batch");
        auto batch = llama_batch_init(static_cast<int>(tokens.size()), 0, 1);
        require(batch.token && batch.pos && batch.n_seq_id && batch.seq_id && batch.logits, "batch allocation failed");
        batch.n_tokens = static_cast<int>(tokens.size());
        for (int i = 0; i < batch.n_tokens; ++i) {
            batch.token[i] = tokens[i]; batch.pos[i] = position + i;
            batch.n_seq_id[i] = 1; batch.seq_id[i][0] = 0;
            batch.logits[i] = i == batch.n_tokens - 1;
        }
        const int code = llama_decode(value, batch); llama_batch_free(batch); return code;
    }
};
std::vector<llama_token> tokenize(const llama_vocab * vocab, const std::string & text) {
    const int count = -llama_tokenize(vocab, text.data(), static_cast<int>(text.size()), nullptr, 0, true, true);
    require(count > 0 && count <= 200, "invalid prompt length");
    std::vector<llama_token> tokens(count);
    require(llama_tokenize(vocab, text.data(), static_cast<int>(text.size()), tokens.data(), count, true, true) == count,
            "tokenization failed"); return tokens;
}
void save(const std::string & path, const char * data, size_t size) {
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    require(fd >= 0, "exclusive result create failed");
    size_t offset = 0;
    while (offset < size) {
        const ssize_t n = write(fd, data + offset, size - offset);
        if (n <= 0) { close(fd); throw std::runtime_error("result write failed"); }
        offset += static_cast<size_t>(n);
    }
    const bool synced = fsync(fd) == 0; const bool closed = close(fd) == 0;
    require(synced && closed, "result sync/close failed");
}
json generate(std::shared_ptr<Model> model, int threads, std::atomic<bool> & cancelled,
              const std::string & directory, const std::string & prefix, bool dump) {
    Session session(std::move(model), threads, cancelled);
    std::lock_guard<std::mutex> lock(session.execution);
    const auto * vocab = llama_model_get_vocab(session.owner->value);
    auto tokens = tokenize(vocab, "The answer to 2 + 3 is");
    require(session.decode(tokens, 0) == 0, "prefill failed");
    json result = {{"pid", getpid()}, {"tid", syscall(SYS_gettid)}, {"threads", threads},
                   {"prompt_tokens", tokens}, {"vocab", llama_vocab_n_tokens(vocab)}, {"logits", json::array()}};
    // Fixed teacher-forced tokens isolate numerical drift from autoregressive
    // divergence. Persist full FP32 logits, not just top-1 token comparisons.
    int position = static_cast<int>(tokens.size());
    if (dump) {
        const auto forced = tokenize(vocab, " 5. This is correct.");
        result["forced_tokens"] = forced;
        for (int step = 0; step < 4; ++step) {
            const float * logits = llama_get_logits_ith(session.value, -1);
            require(logits != nullptr, "missing logits");
            const std::string filename = prefix + "-" + std::to_string(step) + ".f32";
            save(directory + "/" + filename, reinterpret_cast<const char *>(logits),
                 static_cast<size_t>(llama_vocab_n_tokens(vocab)) * sizeof(float));
            result["logits"].push_back(filename);
            require(session.decode({forced.at(static_cast<size_t>(step))}, position++) == 0, "teacher decode failed");
        }
    }
    auto deleter = [](llama_sampler * value) { llama_sampler_free(value); };
    std::unique_ptr<llama_sampler, decltype(deleter)> sampler(llama_sampler_init_greedy(), deleter);
    require(sampler != nullptr, "sampler allocation failed");
    std::string text; result["generated_tokens"] = json::array();
    for (int step = 0; step < 16; ++step) {
        require(!cancelled.load(), "cancelled");
        const llama_token next = llama_sampler_sample(sampler.get(), session.value, -1);
        llama_sampler_accept(sampler.get(), next);
        if (llama_vocab_is_eog(vocab, next)) break;
        std::vector<char> piece(256);
        int n = llama_token_to_piece(vocab, next, piece.data(), static_cast<int>(piece.size()), 0, true);
        if (n < 0) { piece.resize(static_cast<size_t>(-n)); n = llama_token_to_piece(vocab, next, piece.data(), static_cast<int>(piece.size()), 0, true); }
        require(n >= 0, "token decode failed"); text.append(piece.data(), static_cast<size_t>(n));
        result["generated_tokens"].push_back(next);
        require(session.decode({next}, position++) == 0, "generation decode failed");
    }
    require(!text.empty(), "no generated output"); result["text"] = text; return result;
}
}

std::string pico_sdk_suite(const std::string & path, const std::string & directory, std::atomic<bool> & cancelled) {
    json report = {{"schema", 1}, {"pid", getpid()}, {"uid", getuid()}, {"worker_tid", syscall(SYS_gettid)},
                   {"engine", "in-process libllama CPU"}, {"http", false}, {"child_service", false}, {"pass", false}};
    try {
        Backend backend;
        {
            auto model = std::make_shared<Model>(path);
            report["threads_1"] = generate(model, 1, cancelled, directory, "threads1", true);
            report["threads_2"] = generate(model, 2, cancelled, directory, "threads2", true);
            // Two contexts actually overlap on two native workers, while the
            // immutable model is shared and its lifetime remains referenced.
            json left, right; std::exception_ptr a, b;
            std::thread first([&] { try { left = generate(model, 1, cancelled, directory, "parallel1", false); } catch (...) { a = std::current_exception(); } });
            std::thread second;
            try { second = std::thread([&] { try { right = generate(model, 1, cancelled, directory, "parallel2", false); } catch (...) { b = std::current_exception(); } }); }
            catch (...) { first.join(); throw; }
            first.join(); second.join(); if (a) std::rethrow_exception(a); if (b) std::rethrow_exception(b);
            require(left["generated_tokens"] == right["generated_tokens"], "independent contexts disagree");
            report["parallel"] = {left, right};
            {
                Session aborted(model, 1, cancelled); aborted.abort.trip_after = 2;
                const auto prompt = tokenize(llama_model_get_vocab(model->value), std::string(180, 'x'));
                const int code = aborted.decode(prompt, 0);
                require(code == 2 && aborted.abort.polls >= 2, "backend abort callback did not abort execution");
                report["abort"] = {{"code", code}, {"polls", aborted.abort.polls.load()}, {"context_discarded", true}};
            }
            report["recovery"] = generate(model, 1, cancelled, directory, "recovery", false);
        }
        require(live_models == 0 && live_contexts == 0, "resources remain after model scope");
        { auto reloaded = std::make_shared<Model>(path); report["reload"] = generate(reloaded, 2, cancelled, directory, "reload", false); }
        require(live_models == 0 && live_contexts == 0, "resources remain after reload");
        report["live_models"] = live_models.load(); report["live_contexts"] = live_contexts.load(); report["pass"] = true;
    } catch (const std::exception & error) { report["error"] = error.what(); report["cancelled"] = cancelled.load(); }
    report["live_models_after"] = live_models.load(); report["live_contexts_after"] = live_contexts.load();
    const std::string serialized = report.dump(2, ' ', false, json::error_handler_t::replace);
    save(directory + "/result.json", serialized.data(), serialized.size()); return serialized;
}
