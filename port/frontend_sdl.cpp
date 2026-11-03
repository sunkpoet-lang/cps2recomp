// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The window: SDL3 picture, sound and controls around Port::RunFrame().
#include "game_info.h"

#include "bindings.h"
#include "port.h"
#include "present.h"

#include "../recomp-common/text.h"
#include "../runtime/game.h"
#include "online.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

using namespace cps2recomp;

namespace port {

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

    uint16_t ReadKeyboard(const bool *keys, int player) {
        uint16_t pad = 0;
        for (int action = 0; action < Table().table.count; action++) {
            const SDL_Scancode key = g_bindings.key[player][action];
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
        for (int action = 0; action < Table().table.count; action++) {
            const PadInput input = g_bindings.pad[player][action];
            if ((input.button >= 0 && SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(input.button))) ||
                (input.axis >= 0 && SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(input.axis)) > kDead)) {
                pad |= Table().bits[action];
            }
        }
        const int x = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX);
        const int y = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
        if (x < -kDead) pad |= kLeft;
        if (x > kDead) pad |= kRight;
        if (y < -kDead) pad |= kUp;
        if (y > kDead) pad |= kDown;
        return pad;
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
    // (and above the picture the menu bar's strip, as many times its dots: none full screen)
    const int strip = port.opt.fullscreen ? 0 : recomp::MenuBar::kHeightDots * scale;
    SDL_Window *window = SDL_CreateWindow(title, static_cast<int>(std::lround(Video::kHeight * scale * windowShape(columns))), Video::kHeight * scale + strip,
                                          SDL_WINDOW_RESIZABLE | (port.opt.fullscreen ? SDL_WINDOW_FULLSCREEN : 0) |
                                              (getenv("CPS2RECOMP_HIDDEN_WINDOW") ? SDL_WINDOW_HIDDEN : 0)); // (hidden: for tests)
    // The picture goes to the window through SDL's GPU API, or through SDL's renderer where there is no
    // device for that (and the dummy video driver of the tests has none) or when asked: --renderer sdl
    Presenter presenter;
    std::string presentError;
    const bool wantGpu = port.opt.renderer != "sdl" && strcmp(SDL_GetCurrentVideoDriver(), "dummy") != 0;
    if (!window || !presenter.Open(window, wantGpu, presentError)) {
        fprintf(stderr, "SDL window: %s\n", window ? presentError.c_str() : SDL_GetError());
        return 1;
    }
    if (wantGpu && !presenter.UsesGpu()) {
        fprintf(stderr, "no GPU device (%s): the picture goes through SDL's renderer\n", presentError.c_str());
    }
    if (port.opt.stats) {
        fprintf(stderr, "picture: %s\n", presenter.UsesGpu() ? SDL_GetGPUDeviceDriver(presenter.Device()) : SDL_GetRendererName(presenter.Renderer()));
    }

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
    auto toggleMenu = [&]() {
        if (port.menu.IsOpen()) {
            port.menu.Close();
        } else {
            port.MenuPicture(menuBase, menuWidth, menuHeight);
            port.menu.Open(true);
            menuDirty = true;
        }
    };
    auto quickState = [&](bool save) {
        if (port.online) {
            return; // (no save states online)
        }
        std::string error;
        const std::string path = port.StatePath(port.menu.Slot());
        if (!(save ? port.SaveState(path, error) : port.LoadState(path, error))) {
            fprintf(stderr, "state %d: %s\n", port.menu.Slot(), error.c_str());
        }
    };
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
    const bool isOnline = port.opt.online.mode != OnlineOptions::Mode::Off;
    if (isOnline) {
        std::string error;
        if (!online.Start(port, port.opt.online, error)) {
            fprintf(stderr, "online: %s\n", error.c_str());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, ("Online: " + error).c_str(), window);
            return 1;
        }
        const OnlineOptions::Mode mode = port.opt.online.mode;
        const char *who = mode == OnlineOptions::Mode::Host ? "player 1" : mode == OnlineOptions::Mode::Join ? "player 2" : "stress test";
        SDL_SetWindowTitle(window, (std::string(title) + " - online, " + who).c_str());
    }
    // (online, a frame's time is a touch longer while this side is ahead of the other's)
    auto pace = [&]() {
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
        while (SDL_PollEvent(&ev)) {
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
                } else if (ev.key.scancode == SDL_SCANCODE_ESCAPE || ev.key.scancode == SDL_SCANCODE_F1 || (ev.key.scancode == SDL_SCANCODE_TAB && !KeyBound(SDL_SCANCODE_TAB))) {
                    toggleMenu();
                } else if (KeyBound(ev.key.scancode)) {
                    // (a key the player has given to the game does only that)
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
                if (ButtonBound(static_cast<SDL_GamepadButton>(ev.gbutton.button))) {
                    // (a button the player has given to the game does only that)
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK || ev.gbutton.button == SDL_GAMEPAD_BUTTON_GUIDE) {
                    toggleMenu();
                } else if (port.menu.IsOpen()) {
                    break;
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_RIGHT_STICK) {
                    port.Key('p');
                } else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) {
                    port.Key('k', kKeyRestart);
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

        if (port.opt.moviePlay.empty() && !port.opt.autoPlay) {
            // The keyboard is every player's (each has the keys chosen for it); the first gamepad is player
            // 1's, a second player 2's, and so on
            uint16_t pads[Machine::kMaxPlayers] = {};
            const bool *keys = SDL_GetKeyboardState(nullptr);
            if (!(SDL_GetModState() & SDL_KMOD_ALT)) {
                for (int player = 0; player < Players(); player++) {
                    pads[player] |= ReadKeyboard(keys, player);
                }
                port.service = keys[SDL_SCANCODE_9] && !KeyBound(SDL_SCANCODE_9);
            }
            bool trigger = false, both = false;
            const bool stepOnTrigger = !PadInputBound(g_bindings, Table(), {-1, SDL_GAMEPAD_AXIS_LEFT_TRIGGER});
            for (size_t i = 0; i < gamepads.size() && static_cast<int>(i) < Players(); i++) {
                pads[i] |= ReadGamepad(gamepads[i], static_cast<int>(i));
                const bool left = stepOnTrigger && SDL_GetGamepadAxis(gamepads[i], SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > kDead;
                trigger = trigger || left;
                // (while the game runs, the left trigger is its player's Tag: kTag)
                if (left && !port.paused) {
                    pads[i] |= kTag;
                }
                both = both || (SDL_GetGamepadButton(gamepads[i], SDL_GAMEPAD_BUTTON_START) &&
                                SDL_GetGamepadButton(gamepads[i], SDL_GAMEPAD_BUTTON_BACK));
            }
            // The left trigger goes on by one frame (held: frame after frame, after a moment), once the game
            // has been stopped: it does not stop it, so a touch of it while playing does nothing
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
        showBar();
        if (barDirty || width != barWidth || height != barHeight) {
            bar.SetArea(width, height, BarDot(height));
            BarPicture(bar, width, height, barPixels, barRegions);
            presenter.SetOverlay(barPixels, width, height, barRegions);
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
                pace();
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
            presenter.Show(menuPicture.data(), static_cast<int>(menuWidth), static_cast<int>(menuHeight), where);
            if (isOnline) {
                // (online, the game goes on behind the menu, with this side's controls let go, unheard)
                const uint16_t none[2] = {};
                online.Tick(none);
                port.machine->audio.clear();
                pace();
                continue;
            }
            SDL_Delay(8);
            nextFrame = SDL_GetTicksNS();
            continue;
        }

        const int slowBy = port.paused || isOnline ? 1 : port.menu.SlowBy();
        bool ran;
        if (isOnline) {
            // (this side's player: the keyboard's players' keys and the gamepads together; the stress test's two)
            const bool stress = port.opt.online.mode == OnlineOptions::Mode::Stress;
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
            // (Slowed, the sound is played slower by as much.)
            double waiting = SDL_GetAudioStreamQueued(audio) / (4.0 * QSound::kSampleRate) * slowBy;
            // (When much more than that is waiting (the game caught up after being held up, say) what waits
            // is thrown away: a click once, rather than sound that stays late for the seconds it would take
            // to play it off.)
            if (waiting > kAudioMost) {
                SDL_ClearAudioStream(audio);
                waiting = 0;
                audioDropped++;
            }
            const double ratio = (1.0 + std::clamp((waiting - kAudioWaiting) * 0.5, -0.02, 0.02)) / slowBy;
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
            port.soundtrack.Make(port.machine->soundtrack.state, !FastForward(), musicMade);
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
                double waiting = SDL_GetAudioStreamQueued(music) / (8.0 * musicSpec.freq) * slowBy;
                if (waiting > kAudioMost) {
                    SDL_ClearAudioStream(music);
                    waiting = 0;
                }
                const double ratio = (1.0 + std::clamp((waiting - kAudioWaiting) * 0.5, -0.02, 0.02)) / slowBy;
                SDL_SetAudioStreamFrequencyRatio(music, static_cast<float>(ratio));
                SDL_PutAudioStreamData(music, musicMade.data(), static_cast<int>(musicMade.size() * sizeof(float)));
            }
        }

        port.Shown(shown);
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
        presenter.Show(shown.data(), port.ShownWidth(), Video::kHeight, where);

        if (!port.opt.unthrottled && !FastForward()) {
            pace();
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
    presenter.Close();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

} // namespace port
