// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The window: SDL3 picture, sound and controls around Port::RunFrame().
#include "game_info.h"

#include "bindings.h"
#include "port.h"
#include "png_write.h"
#include "present.h"
#include "touch.h"

#include "../recomp-common/text.h"
#include "../runtime/game.h"
#include "../recomp-common/gamepads.h"
#include "online.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace cps2recomp;

namespace port {

#ifdef __APPLE__
void HandFrontToLauncher(); // (front_macos.mm)
#endif

namespace {

    // What each player's keys and gamepad buttons do: the player's choices (bindings.h; the launcher's
    // Controllers page), or the usual layout. A game started from the collection's launcher has the table every
    // game there shares; one with a launcher of its own, its own table.
    const BoundTable &Table() {
        static const BoundTable &table = MakeBoundTable(GamePlayers(), GameButtons(), InCollection());
        return table;
    }
    Bindings g_bindings = recomp::DefaultBindings(Table().table);
    int Players() {
        return Table().table.players;
    }

    static_assert(kBoundActions[0].bit == kUp && kBoundActions[1].bit == kDown && kBoundActions[2].bit == kLeft &&
                  kBoundActions[3].bit == kRight && kBoundActions[4].bit == kPunch1 && kBoundActions[9].bit == kKick3 &&
                  kBoundActions[10].bit == kStart && kBoundActions[11].bit == kCoin &&
                  kBoundActions[12].bit == (kPunch1 | kPunch2 | kPunch3) && kBoundActions[13].bit == (kKick1 | kKick2 | kKick3));

    constexpr int kDead = 16000; // how far a stick or a trigger must go to count

    bool KeyBound(SDL_Scancode key) {
        for (int player = 0; player < Players(); player++) {
            for (int action = 0; action < Table().table.count; action++) {
                if (g_bindings.key[player][action] == key) {
                    return true;
                }
            }
        }
        return false;
    }
    bool ButtonBound(SDL_GamepadButton button) {
        return PadInputBound(g_bindings, Table(), {button, -1});
    }

    // The camera's zoom (zoom.h) is the left stick's click held and a shoulder pressed: left for player 1, right for
    // player 2, the right stick's click for both fighters at once; the d-pad (or the stick) moves the camera. While the
    // click is held (and is not the game's), the shoulders and the directions are not the game's. On the keyboard,
    // Left Shift held, Q, E and W, and the arrows.
    bool ZoomHeld(SDL_Gamepad *gp) {
        return Zoom::Possible() && gp && !ButtonBound(SDL_GAMEPAD_BUTTON_LEFT_STICK) && SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    }
    bool Shoulder(int button) {
        return button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    }
    bool DPad(int button) {
        return button == SDL_GAMEPAD_BUTTON_DPAD_UP || button == SDL_GAMEPAD_BUTTON_DPAD_DOWN || button == SDL_GAMEPAD_BUTTON_DPAD_LEFT ||
               button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    }
    bool KeyZoomHeld(const bool *keys) {
        return Zoom::Possible() && keys[SDL_SCANCODE_LSHIFT] && !KeyBound(SDL_SCANCODE_LSHIFT);
    }
    bool KeyZoom(SDL_Scancode key) {
        return key == SDL_SCANCODE_Q || key == SDL_SCANCODE_E || key == SDL_SCANCODE_W || key == SDL_SCANCODE_UP || key == SDL_SCANCODE_DOWN ||
               key == SDL_SCANCODE_LEFT || key == SDL_SCANCODE_RIGHT;
    }

    uint16_t ReadKeyboard(const bool *keys, int player) {
        uint16_t pad = 0;
        const bool zoomHeld = KeyZoomHeld(keys);
        for (int action = 0; action < Table().table.count; action++) {
            const SDL_Scancode key = g_bindings.key[player][action];
            if (zoomHeld && KeyZoom(key)) {
                continue;
            }
            if (key != SDL_SCANCODE_UNKNOWN && keys[key]) {
                pad |= Table().bits[action];
            }
        }
        // (Return starts player 1's game too, unless it has been given something to do)
        if (player == 0 && keys[SDL_SCANCODE_RETURN] && !KeyBound(SDL_SCANCODE_RETURN)) {
            pad |= kStart;
        }
        return pad;
    }

    uint16_t ReadGamepad(SDL_Gamepad *gp, int player) {
        uint16_t pad = 0;
        const bool zoomHeld = ZoomHeld(gp);
        for (int action = 0; action < Table().table.count; action++) {
            const PadInput input = g_bindings.pad[player][action];
            if (zoomHeld && (Shoulder(input.button) || DPad(input.button))) {
                continue;
            }
            if ((input.button >= 0 && SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(input.button))) ||
                (input.axis >= 0 && SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(input.axis)) > kDead)) {
                pad |= Table().bits[action];
            }
        }
        const int x = zoomHeld ? 0 : SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX);
        const int y = zoomHeld ? 0 : SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
        if (x < -kDead) pad |= kLeft;
        if (x > kDead) pad |= kRight;
        if (y < -kDead) pad |= kUp;
        if (y > kDead) pad |= kDown;
        return pad;
    }

    // A Multi game run's window: where the game before left it (each game is a program of its own, and its window
    // would open where the system puts a new one). The game ending for the next writes it down; the next game reads
    // it, if it was written moments ago (not a run before), and opens its window there, as tall.
    struct Placement {
        int x = 0, y = 0, height = 0;
        bool maximized = false;
    };
    std::filesystem::path PlacementFile() {
        std::error_code error;
        const std::filesystem::path folder = std::filesystem::temp_directory_path(error);
        return error ? std::filesystem::path() : folder / "cps2recomp-multi-window.txt";
    }
    void KeepPlacement(SDL_Window *window) {
        const std::filesystem::path file = PlacementFile();
        if (file.empty() || (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN)) {
            return;
        }
        Placement at;
        int width = 0;
        SDL_GetWindowPosition(window, &at.x, &at.y);
        SDL_GetWindowSize(window, &width, &at.height);
        at.maximized = SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED;
        std::ofstream(file) << at.x << ' ' << at.y << ' ' << at.height << ' ' << at.maximized << '\n';
    }
    bool TakePlacement(Placement &at) {
        const std::filesystem::path file = PlacementFile();
        std::error_code error;
        if (file.empty() || !std::filesystem::exists(file, error)) {
            return false;
        }
        const auto age = std::filesystem::file_time_type::clock::now() - std::filesystem::last_write_time(file, error);
        bool fresh = !error && age < std::chrono::minutes(1);
        {
            std::ifstream in(file);
            fresh = fresh && (in >> at.x >> at.y >> at.height >> at.maximized) && at.height > 0;
        }
        std::filesystem::remove(file, error);
        return fresh;
    }

    // The menu bar's own entries (the in-game menu's have ids of their own: recomp::IsMenuEntry)
    enum { kBarQuickSave = 1, kBarQuickLoad, kBarFullscreen, kBarAbout, kBarKey, kBarScreenshot };

    // The keys that work while playing, for Help's list (each an entry that does nothing)
    const char *const kKeys[][2] = {
        {"In-game menu", "Esc, F1"}, {"Menu bar", "F10, Alt"},  {"Quick save", "F5"},        {"Quick load", "F9"},
        {"Screenshot", "F8"},
        {"Slot before, next", "F6, F7"}, {"Fullscreen", "F11"},   {"Soundtrack", "F12"},       {"Test switch", "F2"},
        {"Pause", "P"},               {"One frame", "O"},          {"Slower, faster", "-, ="}, {"Restart", "Backspace"},
    };

    recomp::BarEntry BarItem(const char *label, int id, const char *shortcut = "", bool enabled = true) {
        recomp::BarEntry e;
        e.label = label;
        e.id = id;
        e.shortcut = shortcut;
        e.enabled = enabled;
        return e;
    }

} // namespace

void BarMenus(Port &port, recomp::MenuBar &bar, bool fullscreen) {
    std::vector<recomp::BarMenu> menus = {{"Game", {}}, {"Video", {}}, {"Audio", {}}, {"Input", {}}, {"Features", {}}, {"Help", {}}};
    std::error_code error;
    recomp::BarOptions options;
    options.slotUsed = [&](int slot) { return std::filesystem::exists(port.StatePath(slot), error); };
    // (a slot kept says when: the time today, the date before)
    options.slotDetail = [&](int slot) -> std::string {
        SDL_PathInfo info{};
        SDL_Time now = 0;
        SDL_DateTime then{}, today{};
        if (!SDL_GetPathInfo(port.StatePath(slot).c_str(), &info) || info.type != SDL_PATHTYPE_FILE || !SDL_GetCurrentTime(&now) ||
            !SDL_TimeToDateTime(info.modify_time, &then, true) || !SDL_TimeToDateTime(now, &today, true)) {
            return "";
        }
        char text[24];
        if (then.year == today.year && then.month == today.month && then.day == today.day) {
            snprintf(text, sizeof(text), "%02d:%02d", then.hour, then.minute);
        } else {
            snprintf(text, sizeof(text), "%04d-%02d-%02d", then.year, then.month, then.day);
        }
        return text;
    };
    port.menu.AddBarEntries(menus, true, options);
    // (and its small picture, where one was kept with it)
    for (recomp::BarEntry &entry : menus[0].entries) {
        if (entry.label != "Save state" && entry.label != "Load state") {
            continue;
        }
        for (size_t i = 0; i < entry.children.size(); i++) {
            SDL_Surface *loaded = SDL_LoadPNG(Port::StatePicture(port.StatePath(static_cast<int>(i) + 1)).c_str());
            SDL_Surface *picture = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_XBGR8888) : nullptr;
            if (picture) {
                recomp::BarEntry &slot = entry.children[i];
                slot.pictureWidth = picture->w;
                slot.pictureHeight = picture->h;
                slot.picture.resize(static_cast<size_t>(picture->w) * picture->h);
                for (int y = 0; y < picture->h; y++) {
                    memcpy(&slot.picture[static_cast<size_t>(y) * picture->w], static_cast<const uint8_t *>(picture->pixels) + y * picture->pitch,
                           static_cast<size_t>(picture->w) * 4);
                }
                SDL_DestroySurface(picture);
            }
            if (loaded) {
                SDL_DestroySurface(loaded);
            }
        }
    }
    // Game: the quick save and load (F5, F9) after the save states
    std::vector<recomp::BarEntry> &game = menus[0].entries;
    auto slot = std::find_if(game.begin(), game.end(), [](const recomp::BarEntry &e) { return e.label == "Slot"; });
    const size_t after = slot == game.end() ? 0 : static_cast<size_t>(slot - game.begin()) + 1;
    game.insert(game.begin() + static_cast<std::ptrdiff_t>(after),
                {recomp::BarEntry::Line(), BarItem("Quick save", kBarQuickSave, "F5"),
                 BarItem("Quick load", kBarQuickLoad, "F9", options.slotUsed(port.menu.Slot())), BarItem("Screenshot", kBarScreenshot, "F8")});
    // Video: full screen (F11, Alt+Enter)
    std::vector<recomp::BarEntry> &video = menus[1].entries;
    if (!video.empty()) {
        video.push_back(recomp::BarEntry::Line());
    }
    video.push_back(BarItem("Fullscreen", kBarFullscreen, "F11"));
    video.back().mark = fullscreen ? recomp::BarEntry::Check : recomp::BarEntry::NoMark;
    // Input: the controls are the launcher's (its Controllers page)
    menus[3].entries.push_back(recomp::BarEntry::Line());
    menus[3].entries.push_back(BarItem("Controls: in the launcher", 0, "", false));
    // Help: the keys, and what this is
    recomp::BarEntry keys = BarItem("Keys", 0);
    for (const auto &key : kKeys) {
        keys.children.push_back(BarItem(key[0], kBarKey, key[1]));
    }
    menus[5].entries.push_back(std::move(keys));
    menus[5].entries.push_back(BarItem("About", kBarAbout));
    // Online, what would change the match or put the two boards out of step is dim (as the in-game menu says
    // "Not while playing online"): the save states, Reset, player 2's Start, the game's features and its speed
    if (port.online) {
        for (recomp::BarMenu &menu : menus) {
            for (recomp::BarEntry &entry : menu.entries) {
                const std::string &l = entry.label;
                if (menu.title == "Features" || l == "Save state" || l == "Load state" || l == "Quick save" || l == "Quick load" ||
                    l == "Reset" || l == "Player 2 joins") {
                    entry.enabled = false;
                }
            }
        }
    }
    // (a menu with nothing in it is left out)
    menus.erase(std::remove_if(menus.begin(), menus.end(), [](const recomp::BarMenu &m) { return m.entries.empty(); }), menus.end());
    bar.Configure(std::move(menus));

}

int BarDot(int windowHeight) {
    // (a window opens the board's lines and the bar's dots tall, so many times: a dot is as many pixels)
    return std::max(1, windowHeight / (Video::kHeight + recomp::MenuBar::kHeightDots));
}

int BarStrip(int windowHeight) {
    return recomp::MenuBar::kHeightDots * BarDot(windowHeight);
}

void BarPicture(const recomp::MenuBar &bar, int width, int height, std::vector<uint32_t> &pixels, std::vector<SDL_Rect> &regions) {
    pixels.assign(static_cast<size_t>(std::max(width, 0)) * std::max(height, 0), 0xFF000000u);
    regions.clear();
    if (width <= 0 || height <= 0) {
        return;
    }
    bar.Draw(pixels.data(), width, height);
    if (bar.BarShown()) {
        regions.push_back({0, 0, width, bar.Height()});
    }
    for (const recomp::MenuBar::Box &box : bar.Boxes()) {
        regions.push_back({static_cast<int>(box.x), static_cast<int>(box.y), static_cast<int>(std::ceil(box.w)), static_cast<int>(std::ceil(box.h))});
    }
    // (the presenter's overlay is drawn blended over the whole window: outside the regions nothing, each pixel inside
    // whole, so the bar is drawn as it is and the picture shows everywhere else)
    std::vector<uint32_t> drawn(pixels.size(), 0);
    for (const SDL_Rect &r : regions) {
        for (int y = std::max(r.y, 0); y < std::min(r.y + r.h, height); y++) {
            for (int x = std::max(r.x, 0); x < std::min(r.x + r.w, width); x++) {
                const size_t at = static_cast<size_t>(y) * width + x;
                drawn[at] = pixels[at] | 0xFF000000u;
            }
        }
    }
    pixels.swap(drawn);
}

int RunWindowed(Port &port) {
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");
    if (!port.opt.settingsPath.empty()) {
        g_bindings = LoadBindings(port.opt.settingsPath, Table());
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    // (controllers SDL doesn't know, issue #213: gamecontrollerdb.txt beside the program, in the launcher's folder or in
    // the game's own; recomp-common/gamepads.h)
    {
        const char *base = SDL_GetBasePath();
        const std::string mine = port.PlayerFolder();
        recomp::AddGamepadMappings({base ? std::filesystem::u8path(base) : std::filesystem::path(), LauncherFolder(),
                                    mine.empty() ? std::filesystem::path() : std::filesystem::u8path(mine)});
    }
    // (the window: so many times the board's 224 lines, --scale's or the Display page's)
    const int scale = port.opt.scale > 0 ? port.opt.scale : WindowSize();
    // (the window's name: game.cfg's "title" where a game has one, else its name)
#ifdef GAME_TITLE
    const char *title = GAME_TITLE;
#else
    const char *title = GAME_NAME;
#endif
    // (the picture's shape: the board's 384 x 224 shown 4:3 unless the Display page says otherwise, and as
    // much wider as the widest picture the game's sources may ask for; 4:3 for a picture that fills it)
    const int columns = TheGame().widestPicture ? std::clamp(TheGame().widestPicture(), +Video::kWidth, +Video::kMaxWidth) : Video::kWidth;
    auto windowShape = [](int across) { return PictureShape(across, Video::kHeight, DisplayShape(), 4.0f / 3.0f * across / Video::kWidth); };
    // (in a Multi game run, a window where the game before's was: a full screen one is full screen as asked)
    Placement placed;
    const bool carried = !MultiGame().empty() && TakePlacement(placed) && !port.opt.fullscreen;
    // (this fork: and above the picture the menu bar's strip, as many times its dots: none full screen)
    const int strip = port.opt.fullscreen ? 0 : recomp::MenuBar::kHeightDots * scale;
    const int windowHeight = carried ? placed.height : Video::kHeight * scale + strip;
    // The window, as an OpenGL window for a shader (shading.h: nothing else draws to it), `at` a place for it (`width`
    // 0: as wide as the picture's shape makes it)
    auto makeWindow = [&](bool opengl, const Placement *at, int width, int height, bool fullscreen) {
        const SDL_PropertiesID properties = SDL_CreateProperties();
        SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width > 0 ? width : std::lround(height * windowShape(columns)));
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        if (at) {
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, at->x);
            SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, at->y);
        }
#ifdef __ANDROID__
        fullscreen = true; // (a phone's whole screen, its bars hidden)
#endif
        SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
                              SDL_WINDOW_RESIZABLE | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0) | (at && at->maximized ? SDL_WINDOW_MAXIMIZED : 0) |
                                  (opengl ? SDL_WINDOW_OPENGL : 0) | (getenv("CPS2RECOMP_HIDDEN_WINDOW") ? SDL_WINDOW_HIDDEN : 0)); // (hidden: for tests)
        SDL_Window *made = SDL_CreateWindowWithProperties(properties);
        SDL_DestroyProperties(properties);
        // (in front: on macOS a program started by one that is not, as the launcher is behind a Multi game run's
        // last game, opens behind the other windows)
        if (made && !getenv("CPS2RECOMP_HIDDEN_WINDOW")) {
            SDL_RaiseWindow(made);
        }
        return made;
    };
    // The picture goes to the window through SDL's GPU API, or through SDL's renderer where there is no
    // device for that (and the dummy video driver of the tests has none) or when asked: --renderer sdl. With a
    // shader preset chosen (the Display page's Shader, or --shader) it goes through the preset with OpenGL instead,
    // where librashader is beside the game and makes the preset; else as without one.
    Presenter presenter;
    std::string presentError;
    const bool wantGpu = port.opt.renderer != "sdl" && strcmp(SDL_GetCurrentVideoDriver(), "dummy") != 0;
    const bool canShade = strcmp(SDL_GetCurrentVideoDriver(), "dummy") != 0;
    std::string preset = canShade ? port.ShaderPreset() : "";
    int shaderValue = FeatureValue("display_shader");
    std::string shaderError;
    if (!preset.empty() && !Shading::Available(shaderError)) {
        fprintf(stderr, "shader: %s; the picture is drawn without it\n", shaderError.c_str());
        preset.clear();
    }
    SDL_Window *window = makeWindow(!preset.empty(), carried ? &placed : nullptr, 0, windowHeight, port.opt.fullscreen);
    // (opened as the preset says, or as without one when that fails)
    auto openPresenter = [&]() {
        if (!preset.empty()) {
            if (presenter.OpenShaded(window, preset, shaderError)) {
                return true;
            }
            fprintf(stderr, "shader %s: %s; the picture is drawn without it\n", preset.c_str(), shaderError.c_str());
            preset.clear();
            // (an OpenGL window is drawn to by nothing else: one that is not, where this one is)
            Placement at;
            int width = 0;
            SDL_GetWindowPosition(window, &at.x, &at.y);
            SDL_GetWindowSize(window, &width, &at.height);
            at.maximized = SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED;
            const bool fullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
            SDL_DestroyWindow(window);
            window = makeWindow(false, &at, width, at.height, fullscreen);
            if (!window) {
                presentError = SDL_GetError();
                return false;
            }
        }
        return presenter.Open(window, wantGpu, presentError);
    };
    presenter.SetVsync(FeatureOn("display_vsync")); // (kept when it is opened again)
    if (!window || !openPresenter()) {
        fprintf(stderr, "SDL window: %s\n", window ? presentError.c_str() : SDL_GetError());
        return 1;
    }
    if (wantGpu && !presenter.UsesGpu() && !presenter.Shaded()) {
        fprintf(stderr, "no GPU device (%s): the picture goes through SDL's renderer\n", presentError.c_str());
    }
    auto sayPresenter = [&]() {
        if (presenter.Shaded() && getenv("CPS2RECOMP_SHADER_SHOT")) {
            presenter.Shader()->keepLast = true;
        }
        if (port.opt.stats) {
            fprintf(stderr, "picture: %s\n", presenter.Shaded()  ? ("OpenGL, shader " + preset).c_str()
                                             : presenter.UsesGpu() ? SDL_GetGPUDeviceDriver(presenter.Device())
                                                                   : SDL_GetRendererName(presenter.Renderer()));
        }
    };
    sayPresenter();
    // The Display page's Shader changed in the menu: another preset in the same window, or the window made again
    // for or without OpenGL, where it is and as large
    auto followShader = [&]() {
        const int value = FeatureValue("display_shader");
        if (value == shaderValue || !port.opt.shader.empty() || !canShade) {
            return;
        }
        shaderValue = value;
        std::string next = port.ShaderPreset();
        if (!next.empty() && !Shading::Available(shaderError)) {
            fprintf(stderr, "shader: %s; the picture is drawn without it\n", shaderError.c_str());
            next.clear();
        }
        if (next == preset) {
            return;
        }
        if (!next.empty() && presenter.Shaded()) {
            if (presenter.SetPreset(next, shaderError)) {
                preset = next;
                sayPresenter();
            } else {
                fprintf(stderr, "shader %s: %s; the one before is kept\n", next.c_str(), shaderError.c_str());
            }
            return;
        }
        Placement at;
        int width = 0;
        SDL_GetWindowPosition(window, &at.x, &at.y);
        SDL_GetWindowSize(window, &width, &at.height);
        at.maximized = SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED;
        const bool fullscreen = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN;
        presenter.Close();
        SDL_DestroyWindow(window);
        preset = next;
        window = makeWindow(!preset.empty(), &at, width, at.height, fullscreen);
        if (!window || !openPresenter()) {
            fprintf(stderr, "SDL window: %s\n", window ? presentError.c_str() : SDL_GetError());
            port.quit = true;
            return;
        }
        sayPresenter();
    };

    // Sound: how much is kept waiting to be played (seconds), and how much is too much. The device is asked
    // for small buffers, so that little waits there either. (CPS2RECOMP_AUDIO_LOG: what is waiting, printed
    // once a second, and nothing heard.)
    constexpr double kAudioWaiting = 0.028, kAudioMost = 0.11;
    const bool audioLog = getenv("CPS2RECOMP_AUDIO_LOG") != nullptr;
    double audioLeast = 1;
    int audioDropped = 0;
    // (--mute starts with no sound; the menu's Sound row switches it on, so the device is opened all the same)
    SDL_AudioStream *audio = nullptr;
    port.menu.muted = port.opt.mute;
    bool silent = false;
    {
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, QSound::kSampleRate};
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (audio) {
            SDL_ResumeAudioStreamDevice(audio);
            if (audioLog) {
                SDL_AudioSpec device{};
                int frames = 0;
                SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(audio), &device, &frames);
                fprintf(stderr, "audio: device %d Hz, %d channels, %d frames a buffer (%s)\n", device.freq, device.channels, frames, SDL_GetCurrentAudioDriver());
            }
        } else {
            fprintf(stderr, "Audio: %s\n", SDL_GetError());
        }
    }
    // The custom soundtrack (soundtrack.h): a stream of its own, at its files' rate, opened when it first plays
    SDL_AudioStream *music = nullptr;
    SDL_AudioSpec musicSpec{SDL_AUDIO_F32, 2, 0};
    std::vector<float> musicMade;
    bool musicSilent = false;

    std::vector<SDL_Gamepad *> gamepads;
    // The in-game menu: opened and closed by Esc, F1, a gamepad stick's click or its Start and Select
    // together (the game is left from its "Quit game"). While it is open the game stands still and the
    // controls work the menu; buttons held as it closes do not reach the game until let go.
    std::vector<uint32_t> menuBase, menuPicture, shown;
    uint32_t menuWidth = 0, menuHeight = 0;
    bool menuDirty = false;
    uint16_t previousPad = 0, heldAtClose = 0;
    unsigned previousMenu = 0;
    int triggerHeld = 0;
    bool bothHeld = false; // a gamepad's Start and Select (Back) together: the menu
    // (with a zoom to work, the left stick's click opens the menu when it is let go, unless a shoulder was pressed
    // while it was held: then it was the zoom's)
    bool stickWaiting = false;
    auto toggleMenu = [&]() {
        if (port.net) {
            // (online: no menu, the game goes on for both players; Esc gives up a wait, and in a match asks whether to
            // leave it: Port::leaving)
            if (port.connecting && !port.net->Arcade()) {
                port.LeaveOnline();
            } else {
                port.leaving = !port.leaving;
            }
            return;
        }
        if (port.menu.IsOpen()) {
            port.menu.Close();
        } else {
            port.MenuPicture(menuBase, menuWidth, menuHeight);
            port.menu.Open(true);
            menuDirty = true;
        }
    };
    // Online, the chat (Port::typing): T on the keyboard, the right stick's click or the touch screen's Chat button
    // opens a line to type, and the system's text input with it (on a Steam Deck or a phone, its own on-screen
    // keyboard); Enter sends it, Esc (or the click again) puts it away. Meanwhile the keyboard is the chat's, and after
    // it, not the game's until every key is let go (the Enter that sent it is not the game's Start).
    uint64_t openedByT = 0; // (when T opened it: its own letter, if the system sends it as text too, is not the line's)
    bool keysAfterChat = false;
    auto chatOpen = [&](bool open) {
        if (open == port.typing || (open && (!port.net || port.connecting || port.leaving))) {
            return;
        }
        port.typing = open;
        port.typed.clear();
        if (open) {
            int w = 0, h = 0;
            SDL_GetWindowSize(window, &w, &h);
            const SDL_Rect area{0, h * 3 / 4, w / 2, h / 12};
            SDL_SetTextInputArea(window, &area, 0);
            SDL_StartTextInput(window);
        } else {
            SDL_StopTextInput(window);
            keysAfterChat = true;
        }
    };
    auto quickState = [&](bool save) {
        if (port.net || port.gekkoOnline) {
            return; // (online: no save states)
        }
        std::string error;
        const std::string path = port.StatePath(port.menu.Slot());
        if (!(save ? port.SaveState(path, error) : port.LoadState(path, error))) {
            fprintf(stderr, "state %d: %s\n", port.menu.Slot(), error.c_str());
        }
    };
    // A Multi game run (the launcher's Multi game page): N or a gamepad's left shoulder for the next game, F or
    // the left trigger held to fast-forward (each when the player has not given it to the game)
    const bool multi = !MultiGame().empty();
    // (a touch screen's controls: touch.h)
    TouchControls touch;
    std::vector<uint32_t> touchPicture;
    // (the menu's Zoom row, as last seen: a change there is the camera's, and a zoom's press is the row's)
    int zoomRow = port.zoom.Choice();
    // The menu bar above the picture, and the same menus where the picture is right-clicked (port.h's Bar...).
    // While one of its menus is open the game stands still, as with the in-game menu; a click or a key that
    // works the bar does not reach the game.
    recomp::MenuBar bar;
    std::vector<uint32_t> barPixels;
    std::vector<SDL_Rect> barRegions;
    bool barDirty = true;
    int barWidth = 0, barHeight = 0;
    auto isFullscreen = [&]() { return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0; };
    // Full screen there is no strip: the bar is drawn over the top of the picture while the pointer is at the
    // screen's top edge, or one of its menus is open (not the right-click menu's, which needs no bar)
    bool barRevealed = false, barPopup = false;
    auto showBar = [&]() {
        const bool shown = !isFullscreen() || barRevealed || (bar.IsOpen() && !barPopup);
        if (shown != bar.BarShown()) {
            bar.ShowBar(shown);
            barDirty = true;
        }
    };
    // (no wider than the board's picture says: 64 of its letters)
    const std::string kFullscreenHint = "F11 leaves full screen. Menu: top edge, right click, F10";
    auto fullscreenHint = [&]() {
        port.notice = kFullscreenHint;
        port.noticeUntil = port.tick + 300;
    };
    auto toggleFullscreen = [&]() {
        const bool to = !isFullscreen();
        SDL_SetWindowFullscreen(window, to);
        barRevealed = false;
        if (to) {
            fullscreenHint();
        } else if (port.notice == kFullscreenHint) {
            port.noticeUntil = 0; // (back in a window, the hint has nothing to say)
        }
        barDirty = true;
    };
    if (port.opt.fullscreen) {
        fullscreenHint();
    }
    auto barOpened = [&](bool popup = false) {
        BarMenus(port, bar, isFullscreen());
        barPopup = popup;
        barDirty = true;
    };
    BarMenus(port, bar, isFullscreen());
    auto screenshot = [&]() {
        std::string file, error;
        if (port.Screenshot(file, error)) {
            port.notice = "Screenshot: " + file;
        } else {
            port.notice = "No screenshot: " + error;
            fprintf(stderr, "screenshot: %s\n", error.c_str());
        }
        port.noticeUntil = port.tick + 150;
    };
    auto barChose = [&](const recomp::MenuBar::Chosen &chosen) {
        barDirty = true;
        heldAtClose = previousPad;
        if (!chosen.chosen) {
            return;
        }
        if (recomp::IsMenuEntry(chosen.id)) {
            port.MenuAct(port.menu.Choose(chosen.id));
        } else if (chosen.id == kBarQuickSave || chosen.id == kBarQuickLoad) {
            quickState(chosen.id == kBarQuickSave);
        } else if (chosen.id == kBarFullscreen) {
            toggleFullscreen();
        } else if (chosen.id == kBarScreenshot) {
            screenshot();
        } else if (chosen.id == kBarAbout) {
            port.notice = std::string(title) + " - ForkAde, a native port by static recompilation (cps2recomp)";
            port.noticeUntil = port.tick + 240;
        }
        BarMenus(port, bar, isFullscreen());
    };
    // (the pointer's place in the window's pixels: events give it in the window's points)
    auto pixelsOf = [&](float x, float y, float &px, float &py) {
        const float density = SDL_GetWindowPixelDensity(window);
        px = x * (density > 0 ? density : 1.0f);
        py = y * (density > 0 ? density : 1.0f);
    };
    // (tests: --window-events, what is put to the window at which turn of the loop; a turn is a frame, or a
    // wait of the menus')
    struct Scripted {
        uint64_t loop;
        std::string what;
        float x = 0, y = 0;
        std::string key;
    };
    std::vector<Scripted> script;
    for (size_t at = 0; at < port.opt.windowEvents.size();) {
        size_t end = port.opt.windowEvents.find(';', at);
        if (end == std::string::npos) {
            end = port.opt.windowEvents.size();
        }
        const std::string one = port.opt.windowEvents.substr(at, end - at);
        const size_t colon = one.find(':');
        if (colon != std::string::npos) {
            Scripted s{strtoull(one.c_str(), nullptr, 10), {}};
            char what[8] = {};
            if (sscanf(one.c_str() + colon + 1, "%7s %f %f", what, &s.x, &s.y) >= 1) {
                s.what = what;
                // (a key's name is the rest: "Left Alt")
                const size_t space = one.find(' ', colon + 1);
                if (space != std::string::npos) {
                    s.key = one.substr(space + 1);
                }
                script.push_back(s);
            }
        }
        at = end + 1;
    }
    uint64_t loops = 0;
    // The pointer is hidden over the game after a moment still, and seen again when it moves
    uint64_t pointerMoved = SDL_GetTicksNS();
    bool pointerShown = true;
    bool altAlone = false; // (Alt went down, and nothing else since: let go, it opens the bar's Game menu)
    const double kFrameNs = 1e9 / Machine::kFramesPerSecond;
    uint64_t nextFrame = SDL_GetTicksNS();

    // Online (online.h): the session runs the frames, with this side's controls as its player's
    Online online;
    const bool isOnline = port.opt.gekko.mode != OnlineOptions::Mode::Off;
    if (isOnline) {
        std::string error;
        if (!online.Start(port, port.opt.gekko, error)) {
            fprintf(stderr, "online: %s\n", error.c_str());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, ("Online: " + error).c_str(), window);
            return 1;
        }
        const OnlineOptions::Mode mode = port.opt.gekko.mode;
        const char *who = mode == OnlineOptions::Mode::Host ? "player 1" : mode == OnlineOptions::Mode::Join ? "player 2" : "stress test";
        SDL_SetWindowTitle(window, (std::string(title) + " - online, " + who).c_str());
    }
    // (online, a frame's time is a touch longer while this side is ahead of the other's)
    auto paceFrame = [&]() {
        nextFrame += static_cast<uint64_t>(kFrameNs * (isOnline && online.FramesAhead() > 1.0f ? 1.016 : 1.0));
        const uint64_t now = SDL_GetTicksNS();
        if (nextFrame > now) {
            SDL_DelayPrecise(nextFrame - now);
        } else if (now - nextFrame > 100000000ull) {
            nextFrame = now; // (fell far behind: start counting again)
        }
    };
    bool running = true;
    while (running && !port.quit && (port.opt.frames == 0 || port.frame < port.opt.frames)) {
        for (const Scripted &s : script) {
            if (s.loop != loops) {
                continue;
            }
            SDL_Event put{};
            const SDL_WindowID id = SDL_GetWindowID(window);
            if (s.what == "m") {
                put.type = SDL_EVENT_MOUSE_MOTION;
                put.motion.windowID = id;
                put.motion.x = s.x;
                put.motion.y = s.y;
                SDL_PushEvent(&put);
            } else if (s.what == "l" || s.what == "r") {
                put.button.windowID = id;
                put.button.button = s.what == "l" ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
                put.button.x = s.x;
                put.button.y = s.y;
                put.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                put.button.down = true;
                SDL_PushEvent(&put);
                put.type = SDL_EVENT_MOUSE_BUTTON_UP;
                put.button.down = false;
                SDL_PushEvent(&put);
            } else if (s.what == "k" || s.what == "kd" || s.what == "ku") {
                put.key.windowID = id;
                put.key.scancode = SDL_GetScancodeFromName(s.key.c_str());
                put.key.key = SDL_GetKeyFromScancode(put.key.scancode, SDL_KMOD_NONE, false);
                if (s.what != "ku") {
                    put.type = SDL_EVENT_KEY_DOWN;
                    put.key.down = true;
                    SDL_PushEvent(&put);
                }
                if (s.what != "kd") {
                    put.type = SDL_EVENT_KEY_UP;
                    put.key.down = false;
                    SDL_PushEvent(&put);
                }
            } else if (s.what == "p") {
                int windowWidth = 0, windowHeight = 0;
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                std::string picked;
                for (const int p : bar.Picked()) {
                    picked += (picked.empty() ? "" : ",") + std::to_string(p);
                }
                printf("window %llu: bar %s [%s] (%zu open, strip %s), in-game menu %s, frame %llu, player 1 %04x, pointer %s, "
                       "fullscreen %s, window %dx%d, slot %d, muted %s, slow by %d, notice \"%s\", online %s\"%s\"\n",
                       static_cast<unsigned long long>(loops), bar.IsOpen() ? "open" : "closed", picked.c_str(), bar.Boxes().size(), bar.BarShown() ? "shown" : "hidden",
                       port.menu.IsOpen() ? "open" : "closed", static_cast<unsigned long long>(port.frame), port.pad[0],
                       pointerShown ? "shown" : "hidden", isFullscreen() ? "yes" : "no", windowWidth, windowHeight, port.menu.Slot(), port.menu.muted ? "yes" : "no",
                       port.menu.SlowBy(), port.tick < port.noticeUntil ? port.notice.c_str() : "", online.Failed() ? "FAILED " : "",
                       online.Message().c_str());
                fflush(stdout);
            }
        }
        loops++;
        SDL_Event ev;
        if (port.typing && (!port.net || port.connecting)) {
            chatOpen(false); // (the match over, or waited for again)
        }
        while (SDL_PollEvent(&ev)) {
            if (touch.Event(ev)) {
                continue;
            }
            if (port.typing && ev.type == SDL_EVENT_TEXT_INPUT) {
                if (openedByT && SDL_GetTicks() - openedByT < 250 && (!strcmp(ev.text.text, "t") || !strcmp(ev.text.text, "T"))) {
                    openedByT = 0;
                    continue;
                }
                openedByT = 0;
                for (const char *c = ev.text.text; *c; c++) {
                    if (*c >= 32 && *c < 127 && port.typed.size() < Netplay::kChatMost) {
                        port.typed += *c;
                    }
                }
                continue;
            }
            if (port.typing && ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.scancode == SDL_SCANCODE_RETURN || ev.key.scancode == SDL_SCANCODE_KP_ENTER) {
                    port.Say();
                    chatOpen(false);
                } else if (ev.key.scancode == SDL_SCANCODE_ESCAPE || ev.key.scancode == SDL_SCANCODE_AC_BACK) {
                    chatOpen(false);
                } else if (ev.key.scancode == SDL_SCANCODE_BACKSPACE && !port.typed.empty()) {
                    port.typed.pop_back();
                }
                continue; // (every other key the line's, as text)
            }
            switch (ev.type) {
            case SDL_EVENT_QUIT: running = false; break;
            case SDL_EVENT_WINDOW_FOCUS_LOST: altAlone = false; break;
            case SDL_EVENT_MOUSE_MOTION:
                pointerMoved = SDL_GetTicksNS();
                if (!pointerShown) {
                    SDL_ShowCursor();
                    pointerShown = true;
                }
                {
                    float x, y;
                    pixelsOf(ev.motion.x, ev.motion.y, x, y);
                    if (isFullscreen()) {
                        // (at the top edge the bar comes; below it, with none of its menus open, it goes)
                        if (y < 4) {
                            barRevealed = true;
                        } else if (y >= bar.Height() && !bar.IsOpen()) {
                            barRevealed = false;
                        }
                        showBar();
                    }
                    if (bar.IsOpen() && !port.menu.IsOpen()) {
                        barDirty = bar.Pointer(x, y) || barDirty;
                    }
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (!port.menu.IsOpen()) {
                    float x, y;
                    pixelsOf(ev.button.x, ev.button.y, x, y);
                    if (ev.button.button == SDL_BUTTON_LEFT && (bar.IsOpen() || bar.Hit(x, y))) {
                        if (!bar.IsOpen()) {
                            barOpened();
                        }
                        barChose(bar.Click(x, y));
                    } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                        barOpened(true);
                        bar.OpenPopup(x, y);
                    } else if (bar.IsOpen()) {
                        bar.Close();
                        barDirty = true;
                    }
                }
                break;
            case SDL_EVENT_KEY_UP:
                if ((ev.key.scancode == SDL_SCANCODE_LALT || ev.key.scancode == SDL_SCANCODE_RALT) && altAlone &&
                    !KeyBound(ev.key.scancode) && !port.menu.IsOpen()) {
                    if (bar.IsOpen()) {
                        bar.Close();
                        barDirty = true;
                    } else {
                        barOpened();
                        bar.OpenMenu(0);
                    }
                }
                altAlone = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!ev.key.repeat) {
                    altAlone = ev.key.scancode == SDL_SCANCODE_LALT || ev.key.scancode == SDL_SCANCODE_RALT;
                }
                if (bar.IsOpen() && ev.key.scancode == SDL_SCANCODE_ESCAPE) {
                    barChose(bar.Buttons(recomp::kMenuBack)); // (a level of the bar's menus closes)
                } else if (bar.IsOpen() && !port.menu.IsOpen() && ev.key.scancode != SDL_SCANCODE_F10) {
                    // While the bar's menu is open the keys work it, and nothing else (the gamepad: below)
                    unsigned button = 0;
                    switch (ev.key.scancode) {
                    case SDL_SCANCODE_UP: button = recomp::kMenuUp; break;
                    case SDL_SCANCODE_DOWN: button = recomp::kMenuDown; break;
                    case SDL_SCANCODE_LEFT: button = recomp::kMenuLeft; break;
                    case SDL_SCANCODE_RIGHT: button = recomp::kMenuRight; break;
                    case SDL_SCANCODE_RETURN:
                    case SDL_SCANCODE_KP_ENTER:
                    case SDL_SCANCODE_SPACE: button = (ev.key.mod & SDL_KMOD_ALT) ? 0 : recomp::kMenuConfirm; break;
                    case SDL_SCANCODE_BACKSPACE: button = recomp::kMenuBack; break;
                    default: break;
                    }
                    if (button) {
                        barChose(bar.Buttons(button));
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F10 && !KeyBound(SDL_SCANCODE_F10) && !port.menu.IsOpen()) {
                    if (bar.IsOpen()) {
                        bar.Close();
                        barDirty = true;
                    } else {
                        barOpened();
                        bar.OpenMenu(0);
                    }
                } else if (!port.menu.IsOpen() && !ev.key.repeat &&
                    (ev.key.scancode == SDL_SCANCODE_Q || ev.key.scancode == SDL_SCANCODE_E || ev.key.scancode == SDL_SCANCODE_W) &&
                    KeyZoomHeld(SDL_GetKeyboardState(nullptr))) {
                    if (ev.key.scancode == SDL_SCANCODE_W) {
                        port.zoom.Both();
                    } else {
                        port.zoom.Press(ev.key.scancode == SDL_SCANCODE_E ? 1 : 0);
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_ESCAPE || ev.key.scancode == SDL_SCANCODE_F1 || ev.key.scancode == SDL_SCANCODE_AC_BACK ||
                           (ev.key.scancode == SDL_SCANCODE_TAB && !KeyBound(SDL_SCANCODE_TAB))) {
                    // (AC_BACK: a phone's Back)
                    toggleMenu();
                } else if (KeyBound(ev.key.scancode)) {
                    // (a key the player has given to the game does only that)
                } else if (port.net && ev.key.scancode == SDL_SCANCODE_T && !ev.key.repeat) {
                    chatOpen(true);
                    openedByT = port.typing ? SDL_GetTicks() : 0;
                } else if (multi && !port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_N && !ev.key.repeat) {
                    port.Key('n');
                } else if (ev.key.scancode == SDL_SCANCODE_F5 || ev.key.scancode == SDL_SCANCODE_F9) {
                    // F5 keeps the game in the slot, F9 goes back to it; F6 / F7: the slot before, the next
                    if (!port.menu.IsOpen()) {
                        quickState(ev.key.scancode == SDL_SCANCODE_F5);
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F8 && !ev.key.repeat) {
                    if (!port.menu.IsOpen()) {
                        screenshot(); // the board's picture, as it is now
                    }
                } else if (ev.key.scancode == SDL_SCANCODE_F6 || ev.key.scancode == SDL_SCANCODE_F7) {
                    port.menu.SetSlot(port.menu.Slot() + (ev.key.scancode == SDL_SCANCODE_F7 ? 1 : -1));
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_P && !ev.key.repeat) {
                    port.Key('p'); // stops the game, and starts it again
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_O) {
                    port.Key('o'); // on by one frame (held: frame after frame)
                } else if (ev.key.scancode == SDL_SCANCODE_MINUS || ev.key.scancode == SDL_SCANCODE_EQUALS) {
                    port.Key(ev.key.scancode == SDL_SCANCODE_MINUS ? '-' : '='); // slower, faster
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_BACKSPACE && !ev.key.repeat) {
                    port.Key('k', kKeyRestart);
                } else if (!port.menu.IsOpen() && ev.key.scancode == SDL_SCANCODE_BACKSLASH && !ev.key.repeat) {
                    port.Key('k', kKeyExtra);
                } else if (ev.key.scancode == SDL_SCANCODE_F8 && !ev.key.repeat) {
                    port.Screenshot(); // (this fork: F8, as the menu bar says; F12 is the soundtrack)
                } else if (ev.key.scancode == SDL_SCANCODE_F2) {
                    port.test = !port.test; // the operator's test switch
                } else if (ev.key.scancode == SDL_SCANCODE_F11 ||
                           (ev.key.scancode == SDL_SCANCODE_RETURN && (ev.key.mod & SDL_KMOD_ALT))) {
                    toggleFullscreen();
                } else if (ev.key.scancode == SDL_SCANCODE_F12 && !ev.key.repeat) {
                    port.ToggleSoundtrack(true); // the custom soundtrack or the board's music, at once
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
                if (!port.menu.IsOpen() && Shoulder(ev.gbutton.button) && ZoomHeld(SDL_GetGamepadFromID(ev.gbutton.which))) {
                    port.zoom.Press(ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER ? 1 : 0);
                    stickWaiting = false;
                } else if (!port.menu.IsOpen() && ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK &&
                           !ButtonBound(SDL_GAMEPAD_BUTTON_RIGHT_STICK) && ZoomHeld(SDL_GetGamepadFromID(ev.gbutton.which))) {
                    port.zoom.Both(); // (not the right stick's own pause)
                    stickWaiting = false;
                } else if (ButtonBound(static_cast<SDL_GamepadButton>(ev.gbutton.button))) {
                    // (a button the player has given to the game does only that)
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK && Zoom::Possible() && !port.menu.IsOpen()) {
                    stickWaiting = true;
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK || ev.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) {
                    toggleMenu();
                } else if (port.menu.IsOpen()) {
                    break;
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
                    if (port.net) {
                        chatOpen(!port.typing);
                    } else {
                        port.Key('p');
                    }
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
                    port.Key(multi ? 'n' : 'k', kKeyRestart);
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK && stickWaiting) {
                    stickWaiting = false;
                    if (!port.menu.IsOpen()) {
                        toggleMenu();
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                if (SDL_Gamepad *gp = SDL_OpenGamepad(ev.gdevice.which)) {
                    gamepads.push_back(gp);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                for (auto it = gamepads.begin(); it != gamepads.end(); ++it) {
                    if (SDL_GetGamepadID(*it) == ev.gdevice.which) {
                        SDL_CloseGamepad(*it);
                        gamepads.erase(it);
                        break;
                    }
                }
                break;
            default: break;
            }
        }

        if (touch.TakeMenuPress()) {
            toggleMenu();
        }
        port.PumpHost(); // (a match hosted from the menu, listened for, the menu open or not)
        touch.Chat(port.net != nullptr); // (online, from the start or a match hosted from the menu)
        if (touch.TakeZoomPress()) {
            if (port.net) {
                chatOpen(!port.typing);
            } else {
                port.zoom.Both();
            }
        }
        if (Zoom::Possible()) {
            const int asked = FeatureValue("zoom");
            if (asked != zoomRow) {
                port.zoom.Choose(asked);
            }
            zoomRow = port.zoom.Choice();
            if (asked != zoomRow) {
                SetFeatureForNow("zoom", zoomRow);
                menuDirty = true;
            }
        }
        if (port.opt.moviePlay.empty() && !port.opt.autoPlay) {
            // The keyboard is every player's (each has the keys chosen for it); the first gamepad is player
            // 1's, a second player 2's, and so on
            uint16_t pads[Machine::kMaxPlayers] = {};
            int keyCount = 0;
            const bool *keys = SDL_GetKeyboardState(&keyCount);
            if (keysAfterChat) {
                keysAfterChat = std::any_of(keys, keys + keyCount, [](bool down) { return down; });
            }
            if (!(SDL_GetModState() & SDL_KMOD_ALT) && !port.typing && !keysAfterChat) {
                for (int player = 0; player < Players(); player++) {
                    pads[player] |= ReadKeyboard(keys, player);
                }
                port.service = keys[SDL_SCANCODE_9] && !KeyBound(SDL_SCANCODE_9);
            }
            bool hurry = multi && keys[SDL_SCANCODE_F] && !KeyBound(SDL_SCANCODE_F);
            // (the camera moved by the arrows with Left Shift held, and by each gamepad's d-pad or stick with its click held)
            if (!port.menu.IsOpen()) {
                int across = 0, down = 0;
                if (KeyZoomHeld(keys) && !(SDL_GetModState() & SDL_KMOD_ALT)) {
                    across += keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];
                    down += keys[SDL_SCANCODE_DOWN] - keys[SDL_SCANCODE_UP];
                }
                for (SDL_Gamepad *gp : gamepads) {
                    if (ZoomHeld(gp)) {
                        const int x = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX), y = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
                        const int right = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || x > kDead;
                        const int left = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || x < -kDead;
                        const int lower = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || y > kDead;
                        const int higher = SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) || y < -kDead;
                        across += right - left;
                        down += lower - higher;
                        // (the click was the camera's, not the menu's, once it has been moved with it)
                        if (right || left || lower || higher) {
                            stickWaiting = false;
                        }
                    }
                }
                port.zoom.Pan(std::clamp(across, -1, 1), std::clamp(down, -1, 1));
            }
            pads[0] |= touch.Read(port.menu.IsOpen());
            bool trigger = false, both = false;
            const bool stepOnTrigger = !PadInputBound(g_bindings, Table(), {-1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER});
            for (size_t i = 0; i < gamepads.size() && static_cast<int>(i) < Players(); i++) {
                pads[i] |= ReadGamepad(gamepads[i], static_cast<int>(i));
                const bool left = stepOnTrigger && SDL_GetGamepadAxis(gamepads[i], SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kDead;
                trigger = trigger || left;
                hurry = hurry || (multi && left);
                // (while the game runs, the left trigger is its player's Tag: kTag; in a Multi game run, fast forward)
                if (left && !port.paused && !multi) {
                    pads[i] |= kTag;
                }
                both = both || (SDL_GetGamepadButton(gamepads[i], SDL_GAMEPAD_BUTTON_START) &&
                                SDL_GetGamepadButton(gamepads[i], SDL_GAMEPAD_BUTTON_BACK));
            }
            // The left trigger goes on by one frame (held: frame after frame, after a moment), once the game
            // has been stopped: it does not stop it, so a touch of it while playing does nothing
            port.hurry = hurry && !port.menu.IsOpen();
            triggerHeld = trigger && port.paused ? triggerHeld + 1 : 0;
            if (!port.menu.IsOpen() && (triggerHeld == 1 || (triggerHeld > 24 && triggerHeld % 4 == 0))) {
                port.Key('o');
            }
            uint16_t all = 0;
            for (const uint16_t one : pads) {
                all |= one;
            }
            uint16_t pressed = all & ~previousPad;
            previousPad = all;
            // Start and Select held together open and close the menu, whatever they are bound to (the second
            // of them to go down is not also a press in the menu; neither reaches the game until let go)
            if (both && !bothHeld) {
                toggleMenu();
                pressed = 0;
                heldAtClose = previousPad;
            }
            bothHeld = both;
            // The menu's buttons are the same whatever the game's are set to: the arrows, the d-pad and the
            // left stick (and the game's own directions); Space, Enter and A choose; Backspace and B go back
            unsigned menuNow = 0, padMenu = 0;
            menuNow |= (previousPad & kUp) || keys[SDL_SCANCODE_UP] ? recomp::kMenuUp : 0;
            menuNow |= (previousPad & kDown) || keys[SDL_SCANCODE_DOWN] ? recomp::kMenuDown : 0;
            menuNow |= (previousPad & kLeft) || keys[SDL_SCANCODE_LEFT] ? recomp::kMenuLeft : 0;
            menuNow |= (previousPad & kRight) || keys[SDL_SCANCODE_RIGHT] ? recomp::kMenuRight : 0;
            menuNow |= keys[SDL_SCANCODE_SPACE] || (keys[SDL_SCANCODE_RETURN] && !(SDL_GetModState() & SDL_KMOD_ALT)) ? recomp::kMenuConfirm : 0;
            menuNow |= keys[SDL_SCANCODE_BACKSPACE] ? recomp::kMenuBack : 0;
            for (SDL_Gamepad *gp : gamepads) {
                menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) ? recomp::kMenuUp : 0;
                menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? recomp::kMenuDown : 0;
                menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) ? recomp::kMenuLeft : 0;
                menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? recomp::kMenuRight : 0;
                menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH) ? recomp::kMenuConfirm : 0;
                menuNow |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_EAST) ? recomp::kMenuBack : 0;
            }
            menuNow |= touch.Menu();
            // (the gamepad's part of it)
            for (SDL_Gamepad *gp : gamepads) {
                padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) ? recomp::kMenuUp : 0;
                padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? recomp::kMenuDown : 0;
                padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) ? recomp::kMenuLeft : 0;
                padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? recomp::kMenuRight : 0;
                padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH) ? recomp::kMenuConfirm : 0;
                padMenu |= SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_EAST) ? recomp::kMenuBack : 0;
            }
            unsigned menuPressed = menuNow & ~previousMenu;
            previousMenu = menuNow;
            if (both) {
                menuPressed = 0;
            }
            if (bar.IsOpen() && !port.menu.IsOpen()) {
                // (the gamepad's presses: the keys' came as events)
                if (menuPressed & padMenu) {
                    barChose(bar.Buttons(menuPressed & padMenu));
                }
                heldAtClose = previousPad;
            } else if (port.menu.IsOpen()) {
                if (menuPressed) {
                    port.MenuAct(port.menu.Press(menuPressed));
                    menuDirty = true;
                } else if (pressed) {
                    port.menu.Note(""); // (a press of something else still takes the note away)
                    menuDirty = true;
                }
                if (!port.menu.IsOpen()) {
                    heldAtClose = previousPad;
                }
            } else if (port.leaving) {
                // (online, "Leave the match?": Confirm leaves it, Back stays; this side's player stands still meanwhile)
                if (menuPressed & recomp::kMenuConfirm) {
                    port.LeaveOnline();
                } else if (menuPressed & recomp::kMenuBack) {
                    port.leaving = false;
                }
                heldAtClose = previousPad;
            }
            heldAtClose &= previousPad;
            for (int player = 0; player < Machine::kMaxPlayers; player++) {
                port.pad[player] = pads[player] & ~heldAtClose;
            }
        }

        // The window takes the picture's shape again when that changes (widescreen, or the Display page's
        // shape), and its size when the Display page's size does: a window only, not full screen or maximized
        {
            static int shownColumns = columns, shownSize = scale;
            static Shape shownShape = DisplayShape();
            const int now = TheGame().widestPicture ? std::clamp(TheGame().widestPicture(), +Video::kWidth, +Video::kMaxWidth) : columns;
            const int size = port.opt.scale > 0 ? port.opt.scale : WindowSize();
            if (now != shownColumns || DisplayShape() != shownShape || size != shownSize) {
                if (!(SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED))) {
                    int windowWidth = 0, windowHeight = 0;
                    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
                    // (the picture's part of it: below the bar's strip, in the window's points)
                    const float density = SDL_GetWindowPixelDensity(window) > 0 ? SDL_GetWindowPixelDensity(window) : 1.0f;
                    int pictureHeight = windowHeight - static_cast<int>(std::lround(BarStrip(static_cast<int>(windowHeight * density)) / density));
                    if (size != shownSize) {
                        pictureHeight = Video::kHeight * size;
                        windowHeight = pictureHeight + recomp::MenuBar::kHeightDots * size;
                    }
                    SDL_SetWindowSize(window, static_cast<int>(std::lround(pictureHeight * windowShape(now))), windowHeight);
                }
                shownColumns = now;
                shownShape = DisplayShape();
                shownSize = size;
            }
        }
        followShader();
        presenter.SetVsync(FeatureOn("display_vsync")); // (the Display page's VSync, as changed in the menu)
        if (port.quit) {
            break;
        }
        // The picture in the window as the Display page says (present.h)
        int width = 0, height = 0;
        presenter.Size(width, height);
        presenter.SetScaling(DisplayScaling());
        // (in a window, below the menu bar's strip; full screen, the whole screen)
        if (pointerShown && !bar.IsOpen() && !port.menu.IsOpen() && SDL_GetTicksNS() - pointerMoved > 2000000000ull) {
            SDL_HideCursor();
            pointerShown = false;
        }
        const bool fullscreen = isFullscreen();
        const int barStrip = fullscreen ? 0 : BarStrip(height);
        SDL_FRect where = FitPicture(width, height - barStrip, port.ShownWidth(), Video::kHeight, DisplayShape(), DisplayScaling());
        where.y += barStrip;
        touch.Layout(width, height, where);
        int touchWidth = 0, touchHeight = 0;
        if (touch.TakePicture(touchPicture, touchWidth, touchHeight) && !touchPicture.empty()) {
            presenter.SetOverlay(touchPicture.data(), touchWidth, touchHeight);
            barDirty = true; // (a touch screen's controls over it: the bar's picture again when they go)
        }
        showBar();
        if (barDirty || width != barWidth || height != barHeight) {
            bar.SetArea(width, height, BarDot(height));
            BarPicture(bar, width, height, barPixels, barRegions);
            presenter.SetOverlay(barPixels.data(), width, height); // (this fork: transparent outside its regions)
            barDirty = false;
            barWidth = width;
            barHeight = height;
        }
        if (bar.IsOpen() && !port.menu.IsOpen()) {
            // The game's picture as it was, with the bar's menu open over it
            if (shown.empty()) {
                port.Shown(shown);
            }
            presenter.Show(shown.data(), port.ShownWidth(), Video::kHeight, where);
            if (isOnline) {
                // (online, the game goes on behind the menu, as behind the in-game menu: this side's controls let go,
                // unheard; the picture shown is the one the menu opened on)
                const uint16_t none[2] = {};
                online.Tick(none);
                port.machine->audio.clear();
                paceFrame();
                continue;
            }
            SDL_Delay(8);
            nextFrame = SDL_GetTicksNS();
            continue;
        }

        if (port.menu.IsOpen()) {
            // The game's picture as it was, with the menu over it
            if (menuDirty) {
                menuDirty = false;
                menuPicture = menuBase;
                port.menu.Draw(menuPicture.data(), static_cast<int>(menuWidth), static_cast<int>(menuHeight));
            }
            // (not through a shader: the menu is drawn three times the board's size, and is not the game's picture)
            presenter.Show(menuPicture.data(), static_cast<int>(menuWidth), static_cast<int>(menuHeight), where, nullptr, false);
            if (isOnline) {
                // (online, the game goes on behind the menu, with this side's controls let go, unheard)
                const uint16_t none[2] = {};
                online.Tick(none);
                port.machine->audio.clear();
                paceFrame();
                continue;
            }
            SDL_Delay(8);
            nextFrame = SDL_GetTicksNS();
            continue;
        }

        // (the game's time to the screen's: under 1 slowed, over 1 faster than the board; online, as the board)
        const auto rate = port.menu.GameRate();
        const double pace = port.paused || isOnline ? 1.0 : static_cast<double>(rate.frames) / rate.ticks;
        bool ran;
        if (isOnline) {
            // (this side's player: the keyboard's players' keys and the gamepads together; the stress test's two)
            const bool stress = port.opt.gekko.mode == OnlineOptions::Mode::Stress;
            const uint16_t local[2] = {static_cast<uint16_t>(stress ? port.pad[0] : port.pad[0] | port.pad[1]),
                                       stress ? port.pad[1] : uint16_t{0}};
            ran = online.Tick(local) > 0;
        } else {
            ran = port.Tick();
        }

        if (audio && silent != (port.menu.muted || audioLog)) {
            silent = port.menu.muted || audioLog;
            SDL_SetAudioStreamGain(audio, silent ? 0.0f : 1.0f);
        }
        std::vector<int16_t> &made = port.machine->audio;
        if (audio && ran && !made.empty()) {
            // The game makes sound at its own pace and the sound device takes it at its own, and the two
            // clocks never quite agree. So the sound is played a touch faster when it is piling up and a
            // touch slower when it is running out (a fraction of a percent, far too little to hear), which
            // keeps a frame or two of it waiting: no gaps, no growing delay.
            // (Slowed, the sound is played slower by as much; faster, faster.)
            double waiting = SDL_GetAudioStreamQueued(audio) / (4.0 * QSound::kSampleRate) / pace;
            // (When much more than that is waiting (the game caught up after being held up, say) what waits
            // is thrown away: a click once, rather than sound that stays late for the seconds it would take
            // to play it off.)
            if (waiting > kAudioMost) {
                SDL_ClearAudioStream(audio);
                waiting = 0;
                audioDropped++;
            }
            const double ratio = (1.0 + std::clamp((waiting - kAudioWaiting) * 0.5, -0.02, 0.02)) * pace;
            SDL_SetAudioStreamFrequencyRatio(audio, static_cast<float>(ratio));
            audioLeast = std::min(audioLeast, waiting);
            if (audioLog && port.frame % 60 == 0) {
                fprintf(stderr, "audio: frame %llu, %.0f ms waiting (least %.0f), ratio %.4f, thrown away %d times\n",
                        static_cast<unsigned long long>(port.frame), waiting * 1000, audioLeast * 1000, ratio, audioDropped);
                audioLeast = 1;
            }
            SDL_PutAudioStreamData(audio, made.data(), static_cast<int>(made.size() * sizeof(int16_t)));
        }
        made.clear();
        if (ran && port.soundtrackOn && port.soundtrack.Any()) {
            // (fed as the game's sound is: the game's time's worth each frame, kept a frame or two ahead of the device)
            port.soundtrack.Make(port.machine->soundtrack.state, !FastForward() && !port.hurry, musicMade);
            if (!musicMade.empty()) {
                const int rate = port.soundtrack.Rate();
                if (!music) {
                    musicSpec.freq = rate;
                    music = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &musicSpec, nullptr, nullptr);
                    if (music) {
                        SDL_ResumeAudioStreamDevice(music);
                    } else {
                        fprintf(stderr, "Audio (soundtrack): %s\n", SDL_GetError());
                    }
                } else if (rate != musicSpec.freq) {
                    musicSpec.freq = rate;
                    SDL_SetAudioStreamFormat(music, &musicSpec, nullptr);
                }
            }
            if (music && !musicMade.empty()) {
                if (musicSilent != (port.menu.muted || audioLog)) {
                    musicSilent = port.menu.muted || audioLog;
                    SDL_SetAudioStreamGain(music, musicSilent ? 0.0f : 1.0f);
                }
                double waiting = SDL_GetAudioStreamQueued(music) / (8.0 * musicSpec.freq) / pace;
                if (waiting > kAudioMost) {
                    SDL_ClearAudioStream(music);
                    waiting = 0;
                }
                const double ratio = (1.0 + std::clamp((waiting - kAudioWaiting) * 0.5, -0.02, 0.02)) * pace;
                SDL_SetAudioStreamFrequencyRatio(music, static_cast<float>(ratio));
                SDL_PutAudioStreamData(music, musicMade.data(), static_cast<int>(musicMade.size() * sizeof(float)));
            }
        }

        // (zoomed: the part of the picture the camera looks at, the readout's panels as they are)
        Zoomed zoomed;
        const bool isZoomed = port.Windowed(shown, zoomed);
        if (isOnline) {
            // What the session says (waiting, connecting, a desync, the other player gone) in the middle, and while
            // playing the ping, delay and rollbacks in a corner
            const int across = port.ShownWidth(), lines = Video::kHeight;
            const recomp::text::Canvas canvas{shown.data(), across, lines};
            const std::string &message = online.Message();
            if (!message.empty()) {
                const float wide = recomp::text::TextWidth(message.c_str(), 1);
                const float x = (across - wide) / 2, y = lines / 2 - 6;
                canvas.Fill(x - 6, y - 5, wide + 12, 18, {20, 22, 30, 230});
                const recomp::text::Color colour = online.Failed() ? recomp::text::Color{255, 120, 110, 255} : recomp::text::Color{255, 255, 255, 255};
                canvas.Text(x, y, 1, 0, colour, message.c_str());
            }
            const std::string stats = online.Stats();
            if (!stats.empty()) {
                const float wide = recomp::text::TextWidth(stats.c_str(), 1);
                canvas.Fill(across - wide - 6, lines - 12, wide + 4, 11, {20, 22, 30, 160});
                canvas.Text(across - wide - 4, lines - 11, 1, 0, {200, 220, 255, 255}, stats.c_str());
            }
        }
        presenter.Show(shown.data(), port.ShownWidth(), Video::kHeight, where, isZoomed ? &zoomed : nullptr);

        if (!port.opt.unthrottled && !FastForward()) {
            paceFrame();
        }
    }
    // (for the checks: what a shaded window showed last, CPS2RECOMP_SHADER_SHOT=FILE.png)
    if (const char *shot = getenv("CPS2RECOMP_SHADER_SHOT"); shot && presenter.Shaded()) {
        std::vector<uint32_t> pixels;
        int width = 0, height = 0;
        if (presenter.Shader()->ReadBack(pixels, width, height)) {
            WritePng(shot, pixels.data(), static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
    }
    for (SDL_Gamepad *gp : gamepads) {
        SDL_CloseGamepad(gp);
    }
    if (audio) {
        SDL_DestroyAudioStream(audio);
    }
    if (music) {
        SDL_DestroyAudioStream(music);
    }
    // (on to a Multi game run's next game: its window where this one is; on macOS the launcher starts it, and must
    // be in front for it to come forward)
    if (ProgramEnded() == kNextGame && !MultiGame().empty()) {
        KeepPlacement(window);
#ifdef __APPLE__
        HandFrontToLauncher();
#endif
    }
    presenter.Close();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace port
