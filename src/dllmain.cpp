#define SPDLOG_USE_STD_FORMAT 1

#include <filesystem>
#include <memory>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <elden-x/singletons.hpp>
#include <elden-x/utils/modutils.hpp>

#include "config.hpp"
#include "gui/render_overlay.hpp"
#include "logs.hpp"
#include "renderer/renderer.hpp"

using namespace std;
namespace fs = std::filesystem;

class overlay_sink : public spdlog::sinks::sink {
protected:
    unique_ptr<spdlog::formatter> formatter;

public:
    overlay_sink()
        : formatter(make_unique<spdlog::pattern_formatter>("%v")) {}

    virtual void log(const spdlog::details::log_msg &msg) {
        string str;
        formatter->format(msg, str);
        gg::logs::log(move(str));
    }

    virtual void flush() {}
    virtual void set_pattern(const string &pattern) {}
    virtual void set_formatter(unique_ptr<spdlog::formatter> sink_formatter) {}
};

static shared_ptr<spdlog::logger> make_logger(const fs::path &path) {
    auto logger = make_shared<spdlog::logger>("gg");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] %^[%l]%$ %v");
    logger->sinks().push_back(
        make_shared<spdlog::sinks::daily_file_sink_st>(path.string(), 0, 0, false, 5));
    logger->sinks().push_back(make_shared<overlay_sink>());
    spdlog::set_default_logger(logger);
    return logger;
}

static void enable_debug_logging(shared_ptr<spdlog::logger> logger) {
    AllocConsole();
    FILE *stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);
    logger->sinks().push_back(make_shared<spdlog::sinks::stdout_color_sink_st>());
    logger->flush_on(spdlog::level::info);
    logger->set_level(spdlog::level::trace);
}

bool WINAPI DllMain(HINSTANCE instance, unsigned int reason, void *reserved) {
    static thread setup_thread;

    if (reason == DLL_PROCESS_ATTACH) {
        gg::config::set_handle(instance);
        auto logger = make_logger(gg::config::mod_folder / "logs" / "ergg.log");
        gg::config::load();

        if (gg::config::debug) {
            enable_debug_logging(logger);
        }

        setup_thread = thread([]() {
            try {
                modutils::initialize();
                this_thread::sleep_for(chrono::seconds(2));
                er::FD4::find_singletons();
                gg::renderer::initialize(gg::gui::initialize_overlay, gg::gui::render_overlay);
            } catch (runtime_error &e) {
                SPDLOG_ERROR("{}", e.what());
            }
        });

        return true;
    } else if (reason == DLL_PROCESS_DETACH && reserved) {
        setup_thread.join();
        modutils::deinitialize();
        spdlog::shutdown();
    }

    return true;
}

// Set up an empty ModEngine2 extension simply to avoid "is not a modengine extension" log
// warnings
extern "C" __declspec(dllexport) bool modengine_ext_init(void *connector, void **extension) {
    static struct dummy_modengine_extension {
        virtual ~dummy_modengine_extension() = default;
        virtual void on_attach(){};
        virtual void on_detach(){};
        virtual const char *id() { return "gg"; };
    } modengine_extension;

    *extension = &modengine_extension;
    return true;
}
