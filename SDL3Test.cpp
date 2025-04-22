// main.cpp
// FPS Aim Trainer - SDL 3.2.10, single‑file example
//
// Build in VS2022:
//  - Add SDL3 include/lib paths under Project → Properties.
//  - Link against SDL3.lib.
//  - Copy SDL3.dll into your Debug folder.
//  - Paste this file into Source Files → main.cpp, then build & run.

#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>

// clamp macro
#define CLAMP(v, lo, hi) (((v)<(lo))?(lo):((v)>(hi))?(hi):(v))

static const int   WINDOW_WIDTH = 1280;
static const int   WINDOW_HEIGHT = 720;
static const Uint32 GAME_DURATION_MS = 60000;
static const Uint32 COUNTDOWN_DURATION_MS = 3000;
static const char* DATA_FILE = "aimtrainer_data.json";

// --- Shared config ---
struct GameConfig {
    float sensitivity;
    float fov;
    bool  challengeMode;
    std::vector<double> gridshotScores;
    std::vector<double> trackingScores;
    int cross_r, cross_g, cross_b;
    int cross_gap, cross_len;
} config;

// --- Base class for modes ---
class GameMode {
public:
    virtual void start() = 0;
    virtual void toggleChallengeMode() = 0;
    virtual ~GameMode() {}
};

// --- JSON load/save ---
static void trim(std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    if (a == std::string::npos) s.clear();
    else s = s.substr(a, b - a + 1);
}

namespace JSONStorage {
    bool loadConfig(GameConfig& cfg) {
        std::ifstream in(DATA_FILE);
        if (!in.is_open()) {
            cfg.sensitivity = 1.0f;
            cfg.fov = 90.0f;
            cfg.challengeMode = false;
            cfg.cross_r = 0;
            cfg.cross_g = 255;
            cfg.cross_b = 0;
            cfg.cross_gap = 5;
            cfg.cross_len = 15;
            return false;
        }
        std::string txt((std::istreambuf_iterator<char>(in)), {});
        in.close();
        txt.erase(std::remove_if(txt.begin(), txt.end(),
            [](char c) {return c == '\n' || c == '\r' || c == '\t';}), txt.end());

        auto parseFloat = [&](const char* key, float def) {
            size_t p = txt.find(key);
            if (p == std::string::npos) return def;
            size_t c = txt.find(':', p);
            size_t e = txt.find_first_of(",}", c + 1);
            std::string num = txt.substr(c + 1, e - c - 1);
            trim(num);
            return num.empty() ? def : std::stof(num);
            };
        auto parseInt = [&](const char* key, int def) {
            size_t p = txt.find(key);
            if (p == std::string::npos) return def;
            size_t c = txt.find(':', p);
            size_t e = txt.find_first_of(",}", c + 1);
            std::string num = txt.substr(c + 1, e - c - 1);
            trim(num);
            return num.empty() ? def : std::stoi(num);
            };
        auto parseBool = [&](const char* key, bool def) {
            size_t p = txt.find(key);
            if (p == std::string::npos) return def;
            size_t c = txt.find(':', p);
            size_t e = txt.find_first_of(",}", c + 1);
            std::string val = txt.substr(c + 1, e - c - 1);
            trim(val);
            return val == "true";
            };
        auto parseArray = [&](const char* key, std::vector<double>& out) {
            out.clear();
            size_t p = txt.find(key);
            if (p == std::string::npos) return;
            size_t b = txt.find('[', p), e = txt.find(']', b);
            if (b == std::string::npos || e == std::string::npos) return;
            std::string sub = txt.substr(b + 1, e - b - 1);
            size_t pos = 0;
            while (pos < sub.size()) {
                size_t c = sub.find(',', pos);
                std::string num = (c == std::string::npos
                    ? sub.substr(pos)
                    : sub.substr(pos, c - pos));
                trim(num);
                if (!num.empty()) out.push_back(std::stod(num));
                if (c == std::string::npos) break;
                pos = c + 1;
            }
            };

        cfg.sensitivity = parseFloat("\"sensitivity\"", 1.0f);
        cfg.fov = parseFloat("\"fov\"", 90.0f);
        cfg.challengeMode = parseBool("\"challengeMode\"", false);
        cfg.cross_r = parseInt("\"cross_r\"", 0);
        cfg.cross_g = parseInt("\"cross_g\"", 255);
        cfg.cross_b = parseInt("\"cross_b\"", 0);
        cfg.cross_gap = parseInt("\"cross_gap\"", 5);
        cfg.cross_len = parseInt("\"cross_len\"", 15);
        parseArray("\"gridshot_high_scores\"", cfg.gridshotScores);
        parseArray("\"tracking_high_scores\"", cfg.trackingScores);
        return true;
    }

    bool saveConfig(const GameConfig& cfg) {
        std::ofstream out(DATA_FILE);
        if (!out.is_open()) return false;
        out << "{\n";
        out << "  \"sensitivity\": " << cfg.sensitivity << ",\n";
        out << "  \"fov\": " << cfg.fov << ",\n";
        out << "  \"challengeMode\": " << (cfg.challengeMode ? "true" : "false") << ",\n";
        out << "  \"cross_r\": " << cfg.cross_r << ",\n";
        out << "  \"cross_g\": " << cfg.cross_g << ",\n";
        out << "  \"cross_b\": " << cfg.cross_b << ",\n";
        out << "  \"cross_gap\": " << cfg.cross_gap << ",\n";
        out << "  \"cross_len\": " << cfg.cross_len << ",\n";
        out << "  \"gridshot_high_scores\": [";
        for (size_t i = 0;i < cfg.gridshotScores.size();++i) {
            out << cfg.gridshotScores[i]
                << (i + 1 < cfg.gridshotScores.size() ? ", " : "");
        }
        out << "],\n";
        out << "  \"tracking_high_scores\": [";
        for (size_t i = 0;i < cfg.trackingScores.size();++i) {
            out << cfg.trackingScores[i]
                << (i + 1 < cfg.trackingScores.size() ? ", " : "");
        }
        out << "]\n}\n";
        return true;
    }
}

// --- Utility ---
struct Rect { int x, y, w, h; };
static bool pointInRect(int px, int py, const Rect& r) {
    return px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h;
}
static void drawRect(SDL_Renderer* ren, int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_FRect fr{ float(x),float(y),float(w),float(h) };
    SDL_RenderFillRect(ren, &fr);
}
static void drawLine(SDL_Renderer* ren, int x1, int y1, int x2, int y2, SDL_Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_RenderLine(ren, float(x1), float(y1), float(x2), float(y2));
}

// --- MainMenu ---
class MainMenu {
public:
    Rect btns[4];
    int hover = -1;
    MainMenu() {
        int bw = 200, bh = 40;
        int cx = WINDOW_WIDTH / 2 - bw / 2;
        int sy = WINDOW_HEIGHT / 2 - 2 * bh - 20;
        for (int i = 0;i < 4;++i) btns[i] = { cx,sy + i * 50,bw,bh };
    }
    void updateHover(int mx, int my) {
        hover = -1;
        for (int i = 0;i < 4;++i)
            if (pointInRect(mx, my, btns[i])) { hover = i;break; }
    }
    void render(SDL_Renderer* ren) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDebugText(ren,
            WINDOW_WIDTH / 2 - 60, WINDOW_HEIGHT / 2 - 150,
            "FPS AIM TRAINER");
        const char* labels[4] = {
            "Gridshot Mode","Tracking Mode","Settings","Credits"
        };
        SDL_Color base{ 80,80,80,255 }, hov{ 100,100,100,255 };
        for (int i = 0;i < 4;++i) {
            drawRect(ren, btns[i].x, btns[i].y,
                btns[i].w, btns[i].h,
                (hover == i ? hov : base));
            int tx = btns[i].x + btns[i].w / 2 - (int)strlen(labels[i]) * 4;
            int ty = btns[i].y + btns[i].h / 2 - 4;
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDebugText(ren, tx, ty, labels[i]);
        }
        double bestG = 0, bestT = 0;
        if (!config.gridshotScores.empty())
            bestG = *std::max_element(
                config.gridshotScores.begin(),
                config.gridshotScores.end());
        if (!config.trackingScores.empty())
            bestT = *std::max_element(
                config.trackingScores.begin(),
                config.trackingScores.end());
        char bufG[32], bufT[32];
        sprintf(bufG, "Best: %.0f", bestG);
        sprintf(bufT, "Best: %.0f", bestT);
        SDL_RenderDebugText(ren,
            btns[0].x + btns[0].w + 5,
            btns[0].y + btns[0].h / 2 - 4,
            bufG);
        SDL_RenderDebugText(ren,
            btns[1].x + btns[1].w + 5,
            btns[1].y + btns[1].h / 2 - 4,
            bufT);
    }
};

// --- GridshotMode ---
class GridshotMode : public GameMode {
    struct Target { double yaw, pitch;bool active; } t[9];
    int score = 0, streak = 0;
    Uint32 timeRem = 0, countdown = 0;
    bool running = false, challengeMode = false;
    SDL_Color tgtCol{ 200,50,50,255 };
public:
    double targAngRad = 2.0;
    int    targPixRad = 20;

    bool isInCountdown()const { return countdown > 0; }
    bool isRunning()    const { return running; }
    int  getScore()     const { return score; }

    void start()override {
        score = streak = 0;
        timeRem = GAME_DURATION_MS;
        countdown = COUNTDOWN_DURATION_MS;
        running = true;
        std::vector<int> idx(9);
        for (int i = 0;i < 9;++i) idx[i] = i;
        std::random_shuffle(idx.begin(), idx.end());
        int initial = challengeMode ? 2 : 5;
        for (int i = 0;i < 9;++i) {
            t[i].active = (i < initial);
            if (i < initial) {
                int row = idx[i] / 3, col = idx[i] % 3;
                double span = 30.0;
                t[i].yaw = (col - 1) * (span / 2.0);
                t[i].pitch = (1 - row) * (span / 2.0);
            }
        }
    }

    bool handleClick(double cy, double cp) {
        for (int i = 0;i < 9;++i) {
            if (!t[i].active) continue;
            double dy = t[i].yaw - cy;
            if (dy > 180) dy -= 360; else if (dy < -180) dy += 360;
            double dp = t[i].pitch - cp;
            if (fabs(dy) <= targAngRad && fabs(dp) <= targAngRad) {
                score++;streak++;
                t[i].active = false;
                std::vector<int> freeIdx;
                for (int k = 0;k < 9;++k)
                    if (k != i && !t[k].active) freeIdx.push_back(k);
                if (!freeIdx.empty()) {
                    int ni = freeIdx[rand() % freeIdx.size()];
                    t[ni].active = true;
                    int row = ni / 3, col = ni % 3;
                    double span = 30.0;
                    t[ni].yaw = (col - 1) * (span / 2.0);
                    t[ni].pitch = (1 - row) * (span / 2.0);
                }
                return true;
            }
        }
        streak = 0;
        return false;
    }

    void update(Uint32 d, double cy, double cp) {
        if (!running) return;
        if (countdown > 0) {
            countdown = (d > countdown ? 0 : countdown - d);
            return;
        }
        if (timeRem > 0) {
            timeRem = (d > timeRem ? 0 : timeRem - d);
            if (timeRem == 0) running = false;
        }
    }

    void render(SDL_Renderer* ren, double cy, double cp) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        if (countdown > 0) {
            int sec = (countdown + 500) / 1000;
            char buf[8];sprintf(buf, "%d", sec);
            SDL_SetRenderScale(ren, 4.0f, 4.0f);
            SDL_RenderDebugText(ren,
                WINDOW_WIDTH / 8 - 4, WINDOW_HEIGHT / 8 - 8, buf);
            SDL_SetRenderScale(ren, 1.0f, 1.0f);
            return;
        }
        double hF = config.fov, asp = double(WINDOW_WIDTH) / WINDOW_HEIGHT;
        double vF = (180.0 / M_PI) * 2 * atan(
            tan(hF * M_PI / 180.0 / 2) * (1.0 / asp));
        int boxRad = challengeMode
            ? (targPixRad / 2)
            : targPixRad;
        for (int i = 0;i < 9;++i) {
            if (!t[i].active) continue;
            double dy = t[i].yaw - cy;
            if (dy > 180) dy -= 360; else if (dy < -180) dy += 360;
            double dp = t[i].pitch - cp;
            if (fabs(dy) > hF / 2 + 5 || fabs(dp) > vF / 2 + 5) continue;
            double xN = tan(dy * M_PI / 180.0) / tan(hF * M_PI / 180.0 / 2);
            double yN = tan(dp * M_PI / 180.0) / tan(vF * M_PI / 180.0 / 2);
            int x = int(xN * (WINDOW_WIDTH / 2) + WINDOW_WIDTH / 2);
            int y = int(-yN * (WINDOW_HEIGHT / 2) + WINDOW_HEIGHT / 2);
            drawRect(ren,
                x - boxRad, y - boxRad,
                2 * boxRad, 2 * boxRad,
                tgtCol);
        }
        // crosshair
        SDL_Color cc{ (Uint8)config.cross_r,
                     (Uint8)config.cross_g,
                     (Uint8)config.cross_b,255 };
        int gap = config.cross_gap, len = config.cross_len;
        drawLine(ren,
            WINDOW_WIDTH / 2 - len, WINDOW_HEIGHT / 2,
            WINDOW_WIDTH / 2 - gap, WINDOW_HEIGHT / 2, cc);
        drawLine(ren,
            WINDOW_WIDTH / 2 + gap, WINDOW_HEIGHT / 2,
            WINDOW_WIDTH / 2 + len, WINDOW_HEIGHT / 2, cc);
        drawLine(ren,
            WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - len,
            WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - gap, cc);
        drawLine(ren,
            WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + gap,
            WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + len, cc);

        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        char hud[64];
        sprintf(hud, "Score:%d Streak:%d Time:%ds",
            score, streak, timeRem / 1000);
        SDL_RenderDebugText(ren, 10, 10, hud);
        if (challengeMode)
            SDL_RenderDebugText(ren, 10, 30, "CHALLENGE MODE");
    }

    void toggleChallengeMode() override {
        challengeMode = !challengeMode;
    }
};

// --- TrackingMode (unchanged except inherits challengeMode) ---
class TrackingMode : public GameMode {
    double yaw = 0, pitch = 0, yv = 20, pv = 15;
    double score = 0, streak = 0, best = 0;
    Uint32 timeRem = 0, countdown = 0;
    bool running = false, challengeMode = false;
    SDL_Color tgtCol{ 200,50,200,255 };
public:
    double targAngRad = 3.0;
    int    targPixRad = 20;
    bool isInCountdown()const { return countdown > 0; }
    bool isRunning()    const { return running; }
    double getScore()   const { return score; }
    void start() override {
        score = streak = best = 0;
        timeRem = GAME_DURATION_MS;
        countdown = COUNTDOWN_DURATION_MS;
        running = true;
        yaw = pitch = 0;
        yv = 20 * ((rand() % 2) ? 1 : -1);
        pv = 15 * ((rand() % 2) ? 1 : -1);
    }
    void update(Uint32 d, double cy, double cp) {
        if (!running) return;
        if (countdown > 0) {
            countdown = (d > countdown ? 0 : countdown - d);
            return;
        }
        double dt = d / 1000.0;
        double factor = challengeMode ? 2.0 : 1.0;
        yaw += yv * factor * dt;
        pitch += pv * factor * dt;
        double hF = config.fov, asp = double(WINDOW_WIDTH) / WINDOW_HEIGHT;
        double vF = (180.0 / M_PI) * 2 * atan(
            tan(hF * M_PI / 180.0 / 2) * (1.0 / asp));
        double maxY = hF / 2 - 5, maxP = vF / 2 - 5;
        if (yaw < -maxY) { yaw = -maxY; yv = -yv; }
        if (yaw > maxY) { yaw = maxY; yv = -yv; }
        if (pitch < -maxP) { pitch = -maxP; pv = -pv; }
        if (pitch > maxP) { pitch = maxP; pv = -pv; }
        double dy = yaw - cy; if (dy > 180) dy -= 360; else if (dy < -180) dy += 360;
        double dp = pitch - cp;
        bool on = fabs(dy) <= targAngRad && fabs(dp) <= targAngRad;
        if (on) {
            score += dt * factor;
            streak += dt * factor;
            if (streak > best) best = streak;
        }
        else streak = 0;
        if (timeRem > 0) {
            timeRem = (d > timeRem ? 0 : timeRem - d);
            if (timeRem == 0) running = false;
        }
    }
    void render(SDL_Renderer* ren, double cy, double cp) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        if (countdown > 0) {
            int sec = (countdown + 500) / 1000;
            char buf[8];sprintf(buf, "%d", sec);
            SDL_SetRenderScale(ren, 4.0f, 4.0f);
            SDL_RenderDebugText(ren,
                WINDOW_WIDTH / 8 - 4, WINDOW_HEIGHT / 8 - 8, buf);
            SDL_SetRenderScale(ren, 1.0f, 1.0f);
            return;
        }
        double dy = yaw - cy; if (dy > 180) dy -= 360; else if (dy < -180) dy += 360;
        double dp = pitch - cp;
        double hF = config.fov, asp = double(WINDOW_WIDTH) / WINDOW_HEIGHT;
        double vF = (180.0 / M_PI) * 2 * atan(
            tan(hF * M_PI / 180.0 / 2) * (1.0 / asp));
        if (fabs(dy) <= hF / 2 && fabs(dp) <= vF / 2) {
            double xN = tan(dy * M_PI / 180.0) / tan(hF * M_PI / 180.0 / 2);
            double yN = tan(dp * M_PI / 180.0) / tan(vF * M_PI / 180.0 / 2);
            int x = int(xN * (WINDOW_WIDTH / 2) + WINDOW_WIDTH / 2);
            int y = int(-yN * (WINDOW_HEIGHT / 2) + WINDOW_HEIGHT / 2);
            drawRect(ren,
                x - targPixRad, y - targPixRad,
                2 * targPixRad, 2 * targPixRad,
                tgtCol);
        }
        SDL_Color cc{ (Uint8)config.cross_r,
                     (Uint8)config.cross_g,
                     (Uint8)config.cross_b,255 };
        int gap = config.cross_gap, len = config.cross_len;
        drawLine(ren, WINDOW_WIDTH / 2 - len, WINDOW_HEIGHT / 2,
            WINDOW_WIDTH / 2 - gap, WINDOW_HEIGHT / 2, cc);
        drawLine(ren, WINDOW_WIDTH / 2 + gap, WINDOW_HEIGHT / 2,
            WINDOW_WIDTH / 2 + len, WINDOW_HEIGHT / 2, cc);
        drawLine(ren, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - len,
            WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 - gap, cc);
        drawLine(ren, WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + gap,
            WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2 + len, cc);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        char hud[64];
        sprintf(hud, "OnTarget:%.1fs Best:%.1fs Time:%ds",
            score, best, timeRem / 1000);
        SDL_RenderDebugText(ren, 10, 10, hud);
        if (challengeMode)
            SDL_RenderDebugText(ren, 10, 30, "CHALLENGE MODE");
    }
    void toggleChallengeMode() override {
        challengeMode = !challengeMode;
    }
};

// --- SettingsMenu ---
class SettingsMenu {
public:
    float sensVal, fovVal;
    int gapVal, lenVal, rVal, gVal, bVal;
    bool challengeVal;
    Rect sensBar, fovBar, gapBar, lenBar, rBar, gBar, bBar;
    Rect sensKnob, fovKnob, gapKnob, lenKnob, rKnob, gKnob, bKnob;
    Rect applyBtn, resetBtn, challengeBtn;
    int dragging, hoverBtn;
    bool hoverChallenge;

    SettingsMenu() {
        sensVal = config.sensitivity;
        fovVal = config.fov;
        gapVal = config.cross_gap;
        lenVal = config.cross_len;
        rVal = config.cross_r;
        gVal = config.cross_g;
        bVal = config.cross_b;
        challengeVal = config.challengeMode;

        sensBar = { WINDOW_WIDTH / 2 - 150,WINDOW_HEIGHT / 2 - 80,300,6 };
        fovBar = { WINDOW_WIDTH / 2 - 150,WINDOW_HEIGHT / 2 - 40,300,6 };
        gapBar = { WINDOW_WIDTH / 2 - 150,WINDOW_HEIGHT / 2,   300,6 };
        lenBar = { WINDOW_WIDTH / 2 - 150,WINDOW_HEIGHT / 2 + 40,300,6 };
        rBar = { WINDOW_WIDTH / 2 - 150,WINDOW_HEIGHT / 2 + 80,300,6 };
        gBar = { WINDOW_WIDTH / 2 - 150,WINDOW_HEIGHT / 2 + 120,300,6 };
        bBar = { WINDOW_WIDTH / 2 - 150,WINDOW_HEIGHT / 2 + 160,300,6 };

        sensKnob.w = sensKnob.h =
            fovKnob.w = fovKnob.h =
            gapKnob.w = gapKnob.h =
            lenKnob.w = lenKnob.h =
            rKnob.w = rKnob.h =
            gKnob.w = gKnob.h =
            bKnob.w = bKnob.h = 12;

        applyBtn = { WINDOW_WIDTH / 2 - 100,WINDOW_HEIGHT / 2 + 200,80,30 };
        resetBtn = { WINDOW_WIDTH / 2 + 20, WINDOW_HEIGHT / 2 + 200,80,30 };
        challengeBtn = { WINDOW_WIDTH / 2 - 100,WINDOW_HEIGHT / 2 + 250,200,30 };

        dragging = 0;hoverBtn = -1;hoverChallenge = false;
        updateKnobs();
    }

    void updateKnobs() {
        auto place = [&](const Rect& bar, Rect& knob, float val, float mn, float mx) {
            float n = (val - mn) / (mx - mn);
            n = CLAMP(n, 0.f, 1.f);
            knob.x = bar.x + int(n * bar.w) - knob.w / 2;
            knob.y = bar.y - knob.h / 2 + bar.h / 2;
            };
        place(sensBar, sensKnob, sensVal, 0.001f, 3.0f);
        place(fovBar, fovKnob, fovVal, 60.0f, 130.0f);
        place(gapBar, gapKnob, float(gapVal), 0.0f, 50.0f);
        place(lenBar, lenKnob, float(lenVal), 0.0f, 100.0f);
        place(rBar, rKnob, float(rVal), 0.0f, 255.0f);
        place(gBar, gKnob, float(gVal), 0.0f, 255.0f);
        place(bBar, bKnob, float(bVal), 0.0f, 255.0f);
    }

    void handleMouseDown(int mx, int my) {
        auto start = [&](const Rect& bar, const Rect& knob, int code) {
            if (pointInRect(mx, my, bar) || pointInRect(mx, my, knob)) {
                dragging = code;return true;
            }
            return false;
            };
        if (start(sensBar, sensKnob, 1))return;
        if (start(fovBar, fovKnob, 2))return;
        if (start(gapBar, gapKnob, 3))return;
        if (start(lenBar, lenKnob, 4))return;
        if (start(rBar, rKnob, 5))return;
        if (start(gBar, gKnob, 6))return;
        if (start(bBar, bKnob, 7))return;

        if (pointInRect(mx, my, applyBtn))hoverBtn = 1;
        else if (pointInRect(mx, my, resetBtn))hoverBtn = 2;
        else if (pointInRect(mx, my, challengeBtn)) {
            challengeVal = !challengeVal;
            return;
        }
    }
    void handleMouseUp() { dragging = 0; }
    void handleMouseMove(int mx, int my) {
        hoverBtn = -1;
        hoverChallenge = pointInRect(mx, my, challengeBtn);
        if (pointInRect(mx, my, applyBtn))hoverBtn = 1;
        else if (pointInRect(mx, my, resetBtn))hoverBtn = 2;

        if (dragging == 1) {
            float n = (mx - sensBar.x) / float(sensBar.w);
            n = CLAMP(n, 0.f, 1.f);
            float lm = log10f(0.001f), lM = log10f(3.0f);
            sensVal = powf(10.0f, lm + n * (lM - lm));
            updateKnobs();
        }
        if (dragging == 2) {
            float n = (mx - fovBar.x) / float(fovBar.w);
            n = CLAMP(n, 0.f, 1.f);
            fovVal = 60.0f + n * 70.0f;
            updateKnobs();
        }
        auto setVal = [&](int code, int mx, const Rect& bar, int& out, float mn, float mxv) {
            if (dragging != code)return;
            float n = (mx - bar.x) / float(bar.w);
            n = CLAMP(n, 0.f, 1.f);
            out = int(mn + n * (mxv - mn));
            updateKnobs();
            };
        setVal(3, mx, gapBar, gapVal, 0.0f, 50.0f);
        setVal(4, mx, lenBar, lenVal, 0.0f, 100.0f);
        setVal(5, mx, rBar, rVal, 0.0f, 255.0f);
        setVal(6, mx, gBar, gVal, 0.0f, 255.0f);
        setVal(7, mx, bBar, bVal, 0.0f, 255.0f);
    }

    void apply() {
        config.sensitivity = sensVal;
        config.fov = fovVal;
        config.cross_gap = gapVal;
        config.cross_len = lenVal;
        config.cross_r = rVal;
        config.cross_g = gVal;
        config.cross_b = bVal;
        config.challengeMode = challengeVal;
        JSONStorage::saveConfig(config);
    }

    void reset() {
        sensVal = config.sensitivity;
        fovVal = config.fov;
        gapVal = config.cross_gap;
        lenVal = config.cross_len;
        rVal = config.cross_r;
        gVal = config.cross_g;
        bVal = config.cross_b;
        challengeVal = config.challengeMode;
        updateKnobs();
    }

    void render(SDL_Renderer* ren) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        char buf[64];
        // Sensitivity
        SDL_RenderDebugText(ren,
            sensBar.x, sensBar.y - 15, "Mouse Sensitivity");
        drawRect(ren,
            sensBar.x, sensBar.y, sensBar.w, sensBar.h, { 200,200,200,255 });
        drawRect(ren,
            sensKnob.x, sensKnob.y, sensKnob.w, sensKnob.h, { 255,255,255,255 });
        sprintf(buf, "%.3f", sensVal);
        SDL_RenderDebugText(ren,
            sensBar.x + sensBar.w + 10, sensBar.y - 4, buf);
        // FOV
        SDL_RenderDebugText(ren,
            fovBar.x, fovBar.y - 15, "Field of View");
        drawRect(ren,
            fovBar.x, fovBar.y, fovBar.w, fovBar.h, { 200,200,200,255 });
        drawRect(ren,
            fovKnob.x, fovKnob.y, fovKnob.w, fovKnob.h, { 255,255,255,255 });
        sprintf(buf, "%.0f", fovVal);
        SDL_RenderDebugText(ren,
            fovBar.x + fovBar.w + 10, fovBar.y - 4, buf);
        // Gap
        SDL_RenderDebugText(ren,
            gapBar.x, gapBar.y - 15, "Crosshair Gap");
        drawRect(ren,
            gapBar.x, gapBar.y, gapBar.w, gapBar.h, { 200,200,200,255 });
        drawRect(ren,
            gapKnob.x, gapKnob.y, gapKnob.w, gapKnob.h, { 255,255,255,255 });
        sprintf(buf, "%d", gapVal);
        SDL_RenderDebugText(ren,
            gapBar.x + gapBar.w + 10, gapBar.y - 4, buf);
        // Length
        SDL_RenderDebugText(ren,
            lenBar.x, lenBar.y - 15, "Crosshair Length");
        drawRect(ren,
            lenBar.x, lenBar.y, lenBar.w, lenBar.h, { 200,200,200,255 });
        drawRect(ren,
            lenKnob.x, lenKnob.y, lenKnob.w, lenKnob.h, { 255,255,255,255 });
        sprintf(buf, "%d", lenVal);
        SDL_RenderDebugText(ren,
            lenBar.x + lenBar.w + 10, lenBar.y - 4, buf);
        // R
        SDL_RenderDebugText(ren,
            rBar.x, rBar.y - 15, "Crosshair R");
        drawRect(ren,
            rBar.x, rBar.y, rBar.w, rBar.h, { 200,200,200,255 });
        drawRect(ren,
            rKnob.x, rKnob.y, rKnob.w, rKnob.h, { 255,255,255,255 });
        sprintf(buf, "%d", rVal);
        SDL_RenderDebugText(ren,
            rBar.x + rBar.w + 10, rBar.y - 4, buf);
        // G
        SDL_RenderDebugText(ren,
            gBar.x, gBar.y - 15, "Crosshair G");
        drawRect(ren,
            gBar.x, gBar.y, gBar.w, gBar.h, { 200,200,200,255 });
        drawRect(ren,
            gKnob.x, gKnob.y, gKnob.w, gKnob.h, { 255,255,255,255 });
        sprintf(buf, "%d", gVal);
        SDL_RenderDebugText(ren,
            gBar.x + gBar.w + 10, gBar.y - 4, buf);
        // B
        SDL_RenderDebugText(ren,
            bBar.x, bBar.y - 15, "Crosshair B");
        drawRect(ren,
            bBar.x, bBar.y, bBar.w, bBar.h, { 200,200,200,255 });
        drawRect(ren,
            bKnob.x, bKnob.y, bKnob.w, bKnob.h, { 255,255,255,255 });
        sprintf(buf, "%d", bVal);
        SDL_RenderDebugText(ren,
            bBar.x + bBar.w + 10, bBar.y - 4, buf);
        // Challenge Mode toggle
        SDL_Color baseBtn{ 100,100,100,255 }, hovBtn{ 150,150,150,255 };
        drawRect(ren,
            challengeBtn.x, challengeBtn.y,
            challengeBtn.w, challengeBtn.h,
            hoverChallenge ? hovBtn : baseBtn);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        sprintf(buf, "Challenge Mode: %s",
            challengeVal ? "ON" : "OFF");
        SDL_RenderDebugText(ren,
            challengeBtn.x + 10, challengeBtn.y + 10, buf);
        // Apply & Reset
        drawRect(ren,
            applyBtn.x, applyBtn.y,
            applyBtn.w, applyBtn.h,
            hoverBtn == 1 ? hovBtn : baseBtn);
        drawRect(ren,
            resetBtn.x, resetBtn.y,
            resetBtn.w, resetBtn.h,
            hoverBtn == 2 ? hovBtn : baseBtn);
        SDL_RenderDebugText(ren,
            applyBtn.x + 20, applyBtn.y + 10, "Apply");
        SDL_RenderDebugText(ren,
            resetBtn.x + 20, resetBtn.y + 10, "Reset");
    }
};

// --- CreditsScreen ---
class CreditsScreen {
public:
    void render(SDL_Renderer* ren) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDebugText(ren,
            WINDOW_WIDTH / 2 - 50, WINDOW_HEIGHT / 2 - 4,
            "FPS Aim Trainer v1.0");
        SDL_RenderDebugText(ren,
            WINDOW_WIDTH / 2 - 80, WINDOW_HEIGHT / 2 + 12,
            "by Xavier Seron, Ceaser Fandino, David Rodriguez");
        SDL_RenderDebugText(ren,
            WINDOW_WIDTH / 2 - 60, WINDOW_HEIGHT / 2 + 40,
            "(Click or press any key)");
    }
};

int main(int argc, char* argv[]) {
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer(
        "FPS Aim Trainer",
        WINDOW_WIDTH, WINDOW_HEIGHT,
        0, &window, &renderer))
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Window/Renderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    JSONStorage::loadConfig(config);
    if (config.sensitivity < 0.001f) config.sensitivity = 1.0f;
    if (config.fov < 60.0f)          config.fov = 90.0f;
    MainMenu      menu;
    GridshotMode  grid;
    TrackingMode  track;
    SettingsMenu  settings;
    CreditsScreen credits;
    enum State { MAIN, GRID, TRACK, SETT, CRED } state = MAIN;
    double camYaw = 0, camPitch = 0;
    SDL_SetWindowRelativeMouseMode(window, false);
    bool quit = false;
    Uint32 prev = SDL_GetTicks();
    while (!quit) {
        Uint32 now = SDL_GetTicks();
        Uint32 delta = now - prev; if (delta > 33) delta = 33;
        prev = now;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) quit = true;
            else if (state == MAIN) {
                if (e.type == SDL_EVENT_MOUSE_MOTION)
                    menu.updateHover(e.motion.x, e.motion.y);
                else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    int mx = e.button.x, my = e.button.y;
                    if (pointInRect(mx, my, menu.btns[0])) {
                        state = GRID;
                        SDL_SetWindowRelativeMouseMode(window, true);
                        if (config.challengeMode) grid.toggleChallengeMode();
                        grid.start();
                    }
                    else if (pointInRect(mx, my, menu.btns[1])) {
                        state = TRACK;
                        SDL_SetWindowRelativeMouseMode(window, true);
                        if (config.challengeMode) track.toggleChallengeMode();
                        track.start();
                    }
                    else if (pointInRect(mx, my, menu.btns[2])) {
                        state = SETT;
                        settings = SettingsMenu();
                        SDL_SetWindowRelativeMouseMode(window, false);
                    }
                    else if (pointInRect(mx, my, menu.btns[3])) {
                        state = CRED;
                        SDL_SetWindowRelativeMouseMode(window, false);
                    }
                }
                else if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE)
                    quit = true;
            }
            else if (state == GRID) {
                if (e.type == SDL_EVENT_MOUSE_MOTION &&
                    !grid.isInCountdown()) {
                    camYaw += e.motion.xrel * config.sensitivity;
                    camPitch -= e.motion.yrel * config.sensitivity;
                    camPitch = CLAMP(camPitch, -89.0, 89.0);
                    if (camYaw < 0) camYaw += 360; if (camYaw >= 360) camYaw -= 360;
                }
                else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                    !grid.isInCountdown() &&
                    e.button.button == SDL_BUTTON_LEFT) {
                    grid.handleClick(camYaw, camPitch);
                }
                else if (e.type == SDL_EVENT_KEY_DOWN &&
                    e.key.key == SDLK_ESCAPE) {
                    state = MAIN;
                    SDL_SetWindowRelativeMouseMode(window, false);
                }
            }
            else if (state == TRACK) {
                if (e.type == SDL_EVENT_MOUSE_MOTION &&
                    !track.isInCountdown()) {
                    camYaw += e.motion.xrel * config.sensitivity;
                    camPitch -= e.motion.yrel * config.sensitivity;
                    camPitch = CLAMP(camPitch, -89.0, 89.0);
                    if (camYaw < 0) camYaw += 360; if (camYaw >= 360) camYaw -= 360;
                }
                else if (e.type == SDL_EVENT_KEY_DOWN &&
                    e.key.key == SDLK_ESCAPE) {
                    state = MAIN;
                    SDL_SetWindowRelativeMouseMode(window, false);
                }
            }
            else if (state == SETT) {
                if (e.type == SDL_EVENT_MOUSE_MOTION)
                    settings.handleMouseMove(e.motion.x, e.motion.y);
                else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                    settings.handleMouseDown(e.button.x, e.button.y);
                else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                    settings.handleMouseUp();
                    int mx = e.button.x, my = e.button.y;
                    if (pointInRect(mx, my, settings.applyBtn)) {
                        settings.apply();
                        state = MAIN;
                    }
                    else if (pointInRect(mx, my, settings.resetBtn)) {
                        settings.reset();
                    }
                }
                else if (e.type == SDL_EVENT_KEY_DOWN &&
                    e.key.key == SDLK_ESCAPE) {
                    settings.reset();
                    state = MAIN;
                }
            }
            else if (state == CRED) {
                if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                    e.type == SDL_EVENT_KEY_DOWN) {
                    state = MAIN;
                }
            }
        }
        if (state == GRID && grid.isRunning()) {
            grid.update(delta, camYaw, camPitch);
            if (!grid.isRunning()) {
                config.gridshotScores.push_back(grid.getScore());
                JSONStorage::saveConfig(config);
                state = MAIN;
                SDL_SetWindowRelativeMouseMode(window, false);
            }
        }
        else if (state == TRACK && track.isRunning()) {
            track.update(delta, camYaw, camPitch);
            if (!track.isRunning()) {
                config.trackingScores.push_back(track.getScore());
                JSONStorage::saveConfig(config);
                state = MAIN;
                SDL_SetWindowRelativeMouseMode(window, false);
            }
        }
        switch (state) {
        case MAIN:  menu.render(renderer); break;
        case GRID:  grid.render(renderer, camYaw, camPitch); break;
        case TRACK: track.render(renderer, camYaw, camPitch); break;
        case SETT:  settings.render(renderer); break;
        case CRED:  credits.render(renderer); break;
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(1);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
