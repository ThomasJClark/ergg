#define WIN32_LEAN_AND_MEAN

#include <filesystem>
#include <memory>
#include <thread>

#include <windows.h>

#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <elden-x/singletons.hpp>
#include <elden-x/utils/modutils.hpp>

#include "config.hpp"
#include "gui/render_overlay.hpp"
#include "renderer/renderer.hpp"

using namespace std;
namespace fs = std::filesystem;

static shared_ptr<spdlog::logger> make_logger(const fs::path &path)
{
    auto logger = make_shared<spdlog::logger>("gg");
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] %^[%l]%$ %v");
    logger->sinks().push_back(
        make_shared<spdlog::sinks::daily_file_sink_st>(path.string(), 0, 0, false, 5));
    spdlog::set_default_logger(logger);
    return logger;
}

static void enable_debug_logging(shared_ptr<spdlog::logger> logger)
{
    AllocConsole();
    FILE *stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);
    logger->sinks().push_back(make_shared<spdlog::sinks::stdout_color_sink_st>());
    logger->flush_on(spdlog::level::info);
    logger->set_level(spdlog::level::trace);
}

bool WINAPI DllMain(HINSTANCE dll_instance, unsigned int fdw_reason, void *reserved)
{
    static thread setup_thread;

    if (fdw_reason == DLL_PROCESS_ATTACH)
    {
        wchar_t dll_filename[MAX_PATH] = {0};
        GetModuleFileNameW(dll_instance, dll_filename, MAX_PATH);
        auto folder = fs::path{dll_filename}.parent_path();

        auto logger = make_logger(folder / "logs" / "ergg.log");

        gg::config::load_config(folder / "ergg.ini");

        if (gg::config::debug)
        {
            enable_debug_logging(logger);
        }

        setup_thread = thread([]() {
            try
            {
                modutils::initialize();
                this_thread::sleep_for(chrono::seconds(2));
                er::FD4::find_singletons();

                // Hook into the DirectX 12 rendering, and add a callback to render the overlay
                // UI using ImGui
                gg::renderer::initialize(gg::gui::initialize_overlay, gg::gui::render_overlay);
            }
            catch (runtime_error &e)
            {
                SPDLOG_ERROR("{}", e.what());
            }
        });

        return true;
    }
    else if (fdw_reason == DLL_PROCESS_DETACH && reserved)
    {
        setup_thread.join();
        modutils::deinitialize();
        spdlog::shutdown();
    }

    return true;
}
