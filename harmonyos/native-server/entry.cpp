// OHOS official native-child entry: reuse the upstream server in this process.
#ifndef __OHOS__
#error "This entry is only for OHOS native child processes"
#endif
#include <AbilityKit/native_child_process.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

int llama_server(int argc, char ** argv);

namespace {
using nlohmann::json;
constexpr size_t max_params = 65536;

int create_output(const std::string & path) {
    if (path.empty() || path[0] != '/' || path.find('\0') != std::string::npos) {
        throw std::runtime_error("output path must be absolute and NUL-free");
    }
    // Each invocation owns new files. Never truncate a user file or follow a symlink.
    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        throw std::runtime_error("cannot create output: " + std::string(strerror(errno)));
    }
    if (fd <= STDERR_FILENO) {
        int moved = fcntl(fd, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
        close(fd);
        if (moved < 0) throw std::runtime_error("cannot reserve output descriptor");
        fd = moved;
    }
    return fd;
}

void receipt(int fd, const char * phase, int rc) {
    std::string line = json{{"schema", 1}, {"phase", phase}, {"pid", getpid()},
        {"ppid", getppid()}, {"uid", getuid()}, {"gid", getgid()}, {"rc", rc},
        {"address_space_policy", "not-set"}}.dump() + "\n";
    fprintf(stderr, "PICO_LLAMA_NATIVE_CHILD %s", line.c_str());
    if (fd >= 0) {
        size_t pos = 0;
        while (pos < line.size()) {
            ssize_t n = write(fd, line.data() + pos, line.size() - pos);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) throw std::runtime_error("receipt write failed");
            pos += static_cast<size_t>(n);
        }
        if (fsync(fd)) throw std::runtime_error("receipt sync failed");
    }
    fflush(stderr);
}

int run(NativeChildProcess_Args args) {
    int receipt_fd = -1;
    try {
        // RLIMIT_AS permanently disabled; validate arguments and lifecycle only.
        if (!args.entryParams || strnlen(args.entryParams, max_params + 1) > max_params) {
            throw std::runtime_error("missing or oversized entryParams");
        }
        auto params = json::parse(args.entryParams);
        if (!params.is_object() || params.size() != 4 || params.at("schema") != 1 ||
            !params.at("argv").is_array() || params.at("argv").size() > 256) {
            throw std::runtime_error("expected schema/argv/log_path/receipt_path");
        }
        std::vector<std::string> storage{"llama-server"};
        for (const auto & item : params.at("argv")) {
            auto value = item.get<std::string>();
            if (value.find('\0') != std::string::npos) throw std::runtime_error("NUL in argv");
            storage.push_back(std::move(value));
        }
        auto log_path = params.at("log_path").get<std::string>();
        auto receipt_path = params.at("receipt_path").get<std::string>();
        if (log_path == receipt_path) throw std::runtime_error("outputs must differ");
        receipt_fd = create_output(receipt_path);
        int log_fd = create_output(log_path);
        if (dup2(log_fd, STDOUT_FILENO) < 0 || dup2(log_fd, STDERR_FILENO) < 0) {
            close(log_fd);
            throw std::runtime_error("log redirection failed");
        }
        if (log_fd > STDERR_FILENO) close(log_fd);
        // Do not consume the host's input stream or interpret argv through a shell.
        int input_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (input_fd < 0 || dup2(input_fd, STDIN_FILENO) < 0) {
            if (input_fd >= 0) close(input_fd);
            throw std::runtime_error("stdin redirection failed");
        }
        if (input_fd > STDERR_FILENO) close(input_fd);
        std::vector<char *> argv;
        for (auto & value : storage) argv.push_back(value.data());
        argv.push_back(nullptr);
        receipt(receipt_fd, "enter", 0);
        int rc = llama_server(static_cast<int>(storage.size()), argv.data());
        receipt(receipt_fd, "return", rc);
        close(receipt_fd);
        return rc;
    } catch (const std::exception & error) {
        fprintf(stderr, "PICO_LLAMA_NATIVE_CHILD error pid=%ld: %s\n", (long)getpid(), error.what());
        try { receipt(receipt_fd, "error", 125); } catch (...) {}
        if (receipt_fd >= 0) close(receipt_fd);
        return 125;
    }
}
} // namespace

extern "C" __attribute__((visibility("default"))) void Main(NativeChildProcess_Args args) {
    int rc = run(args);
    fflush(nullptr);
    // A dedicated official child process owns this invocation, never the UI process.
    _exit(rc);
}
