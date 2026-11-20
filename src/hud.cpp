#include "hud.hpp"
#include <cstdio>
#include <string_view>

namespace orbit {
namespace {
constexpr Vec3 white{0.88f, 0.92f, 0.98f}, muted{0.42f, 0.51f, 0.64f}, teal{0.30f, 0.85f, 0.77f};
constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:/-+[]<>";
constexpr unsigned char glyphs[][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,25,21,19,19,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
    {0,0,0,0,0,6,6}, {0,6,6,0,6,6,0}, {1,2,2,4,8,8,16},
    {0,0,0,31,0,0,0}, {0,4,4,31,4,4,0}, {14,8,8,8,8,8,14},
    {14,2,2,2,2,2,14}, {2,4,8,16,8,4,2}, {8,4,2,1,2,4,8}
};
static_assert(sizeof(glyphs) / sizeof(glyphs[0]) == alphabet.size());
}

Hud::Hud() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PixelVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PixelVertex), reinterpret_cast<void*>(offsetof(PixelVertex, r)));
}
Hud::~Hud() {
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}
void Hud::rect(float x, float y, float w, float h, Vec3 c) {
    for (const auto& p : {std::pair<float,float>{x,y}, {x+w,y}, {x+w,y+h}, {x,y}, {x+w,y+h}, {x,y+h}})
        vertices_.push_back({p.first, p.second, c.x, c.y, c.z});
}
void Hud::text(float x, float y, const std::string& value, float size, Vec3 color) {
    for (char c : value) {
        const auto index = alphabet.find(c);
        if (index != std::string_view::npos)
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if (glyphs[index][row] & (1 << (4 - col)))
                        rect(x + col * size, y + row * size, size, size, color);
        x += 6 * size;
    }
}
void Hud::line(float x0, float y0, float x1, float y1, float thickness, Vec3 c) {
    const float distance = std::hypot(x1-x0, y1-y0);
    if (distance < 0.001f) return;
    const float dx = -(y1-y0) * thickness / (2*distance), dy = (x1-x0) * thickness / (2*distance);
    for (const auto& p : {std::pair<float,float>{x0+dx,y0+dy}, {x1+dx,y1+dy}, {x1-dx,y1-dy},
                          {x0+dx,y0+dy}, {x1-dx,y1-dy}, {x0-dx,y0-dy}})
        vertices_.push_back({p.first, p.second, c.x, c.y, c.z});
}

void Hud::plot(float x, float y, float width, float height, const OrbitDiagnostics& data,
               int kind, double low, double high) {
    constexpr Vec3 grid{0.11f, 0.16f, 0.21f};
    rect(x, y, width, height, {0.025f, 0.038f, 0.060f});
    for (int row = 0; row < 3; ++row) rect(x, y + row*height/2, width, 1, grid);
    char label[40];
    std::snprintf(label, sizeof(label), "%.2F", high);
    text(x+3, y+4, label, 0.9f, muted);
    std::snprintf(label, sizeof(label), "%.2F", low);
    text(x+3, y+height-10, label, 0.9f, muted);
    if (kind == 0 && low <= 2 && high >= 2) {
        const float targetY = y + height * static_cast<float>((high-2)/(high-low));
        for (float dash = x; dash < x+width; dash += 10) rect(dash, targetY, std::min(5.0f,x+width-dash), 1, teal*0.6f);
    }
    if (data.history.size() < 2) {
        text(x+8, y+height/2, "COLLECTING DATA", 1, muted);
        return;
    }
    const double first = data.history.front().time;
    const double duration = data.history.back().time-first;
    bool anyValid = false;
    for (int series = 0; series < (kind == 0 ? 1 : 2); ++series) {
        bool previousValid = false;
        float previousX = 0, previousY = 0;
        std::uint64_t previousSegment = 0;
        double phaseOrigin = 0;
        bool havePhaseOrigin = false;
        for (const auto& sample : data.history) {
            const bool valid = kind == 0 ? sample.ratioValid : kind == 1 ? true : sample.phaseValid[series];
            if (!valid) { previousValid = false; continue; }
            anyValid = true;
            if (kind == 2 && (!havePhaseOrigin || previousSegment != sample.phaseSegment[series])) {
                phaseOrigin = sample.phaseTurns[series];
                havePhaseOrigin = true;
            }
            const double value = kind == 0 ? sample.periodRatio : kind == 1 ? sample.elements[series].eccentricity
                                                                                 : sample.phaseTurns[series]-phaseOrigin;
            const float px = x + width * static_cast<float>((sample.time-first)/duration);
            const float py = y + height * static_cast<float>(std::clamp((high-value)/(high-low), 0.0, 1.0));
            if (previousValid && (kind != 2 || previousSegment == sample.phaseSegment[series]))
                line(previousX, previousY, px, py, 1.5f, kind == 0 ? teal : planets[series].color);
            previousX = px; previousY = py; previousValid = true;
            previousSegment = sample.phaseSegment[series];
        }
    }
    if (!anyValid) text(x+8, y+height/2, kind == 2 ? "PHASE UNDEFINED" : "NO BOUND PERIOD", 1, muted);
}

void Hud::diagnostics(int width, int height, const OrbitDiagnostics& data) {
    if (data.history.empty()) return;
    const float top = static_cast<float>(height-diagnosticsHeight);
    rect(panelWidth, top, static_cast<float>(width-panelWidth), diagnosticsHeight, {0.035f, 0.050f, 0.078f});
    rect(panelWidth, top, static_cast<float>(width-panelWidth), 1, {0.14f, 0.19f, 0.25f});
    char label[100];
    std::snprintf(label, sizeof(label), "ORBIT DATA / T %.1F TO %.1F S / D TO HIDE", data.history.front().time, data.history.back().time);
    text(panelWidth+16, top+13, label, 1.1f, muted);
    const auto& current = data.history.back();
    double ratioLow = 2, ratioHigh = 2, eccentricityHigh = 0.1, phaseLow = 0, phaseHigh = 0;
    std::array<double, 2> phaseOrigins{};
    std::array<std::uint64_t, 2> phaseSegments{};
    for (const auto& sample : data.history) {
        if (sample.ratioValid) {
            ratioLow = std::min(ratioLow, sample.periodRatio);
            ratioHigh = std::max(ratioHigh, sample.periodRatio);
        }
        for (int i = 0; i < 2; ++i) {
            eccentricityHigh = std::max(eccentricityHigh, sample.elements[i].eccentricity*1.1);
            if (sample.phaseValid[i]) {
                if (phaseSegments[i] != sample.phaseSegment[i]) {
                    phaseOrigins[i] = sample.phaseTurns[i];
                    phaseSegments[i] = sample.phaseSegment[i];
                }
                const double change = sample.phaseTurns[i]-phaseOrigins[i];
                phaseLow = std::min(phaseLow, change);
                phaseHigh = std::max(phaseHigh, change);
            }
        }
    }
    const double ratioPad = std::max(0.01, (ratioHigh-ratioLow)*0.1);
    const double phasePad = std::max(0.25, (phaseHigh-phaseLow)*0.05);
    const float cell = static_cast<float>(width-panelWidth-32)/3;
    for (int chart = 0; chart < 3; ++chart) {
        const float x = panelWidth+16+chart*cell, chartWidth = cell-16;
        text(x, top+39, chart == 0 ? "PERIOD RATIO OUTER/INNER" : chart == 1 ? "ECCENTRICITY" : "2:1 PHASE CHANGE / TURNS", 1, white);
        if (chart == 0) {
            if (current.ratioValid) std::snprintf(label, sizeof(label), "RATIO %.5F", current.periodRatio);
            else std::snprintf(label, sizeof(label), "RATIO UNDEFINED");
            text(x, top+61, label, 1.5f, teal);
            if (current.ratioValid) std::snprintf(label, sizeof(label), "2:1 OFFSET %+.2F PCT", (current.periodRatio/2-1)*100);
            else std::snprintf(label, sizeof(label), "UNBOUND ORBIT");
            text(x, top+84, label, 1, muted);
            plot(x, top+106, chartWidth, 94, data, chart, ratioLow-ratioPad, ratioHigh+ratioPad);
            text(x, top+216, "DASHED LINE = 2:1", 1, muted);
        } else if (chart == 1) {
            std::snprintf(label, sizeof(label), "A %.4F", current.elements[0].eccentricity);
            text(x, top+61, label, 1.4f, planets[0].color);
            std::snprintf(label, sizeof(label), "B %.4F", current.elements[1].eccentricity);
            text(x+chartWidth/2, top+61, label, 1.4f, planets[1].color);
            text(x, top+84, "0 CIRCLE / 1 ESCAPE LIMIT", 1, muted);
            plot(x, top+106, chartWidth, 94, data, chart, 0, eccentricityHigh);
            text(x, top+216, "SUN-RELATIVE OSCULATING ORBITS", 0.9f, muted);
        } else {
            for (int i = 0; i < 2; ++i) {
                if (current.phaseValid[i]) std::snprintf(label, sizeof(label), "%c %+.2F", i == 0 ? 'A' : 'B', current.phaseTurns[i]-phaseOrigins[i]);
                else std::snprintf(label, sizeof(label), "%c --", i == 0 ? 'A' : 'B');
                text(x+i*chartWidth/2, top+61, label, 1.4f, planets[i].color);
            }
            text(x, top+84, "FULL TURNS = PHASE DRIFT", 1, muted);
            plot(x, top+106, chartWidth, 94, data, chart, phaseLow-phasePad, phaseHigh+phasePad);
            text(x, top+216, "GAPS RESET PHASE TRACKING", 0.9f, muted);
        }
    }
}

void Hud::draw(int width, int height, const Simulation& sim, bool wireframe, bool orbits,
               bool shadows, float eclipseCoverage, bool showDiagnostics) {
    vertices_.clear();
    vertices_.reserve(50000);
    rect(0, 0, panelWidth, static_cast<float>(height), {0.035f, 0.050f, 0.078f});
    rect(panelWidth - 1, 0, 1, static_cast<float>(height), {0.12f, 0.17f, 0.22f});
    text(24, 30, "ORBIT / 01", 3, white);
    text(24, 66, "3-BODY-PROBLEM", 1.4f, muted);
    rect(24, 100, 222, 1, {0.14f, 0.19f, 0.25f});
    text(24, 122, "NEWTONIAN GRAVITY", 1.6f, teal);
    text(24, 151, "1 STAR / 2 PLANETS", 1.5f, white);
    text(24, 190, "01  SOL", 1.8f, {1.0f, 0.69f, 0.28f});
    char status[80];
    std::snprintf(status, sizeof(status), "MASS %.0F / MOVING STAR", sunMass);
    text(24, 214, status, 1.3f, muted);
    text(24, 252, "02  AZURE", 1.8f, planets[0].color);
    std::snprintf(status, sizeof(status), "M %.0F / INITIAL T %.1F S", planets[0].mass, initialPeriod(planets[0]));
    text(24, 276, status, 1.3f, muted);
    text(24, 314, "03  EMBER", 1.8f, planets[1].color);
    std::snprintf(status, sizeof(status), "M %.0F / INITIAL T %.1F S", planets[1].mass, initialPeriod(planets[1]));
    text(24, 338, status, 1.3f, muted);
    rect(24, 374, 222, 1, {0.14f, 0.19f, 0.25f});
    std::snprintf(status, sizeof(status), "%s / %.3G X", sim.collision ? "CONTACT" : sim.paused ? "PAUSED" : "RUNNING", sim.speed);
    text(24, 397, status, 1.7f, teal);
    std::snprintf(status, sizeof(status), "T %.1F S / %.3G S/S", sim.time, sim.speed * playbackScale);
    text(24, 426, status, 1.4f, muted);
    std::snprintf(status, sizeof(status), "G %.6F / DT 1/256", gravity);
    text(24, 451, status, 1.2f, muted);
    std::snprintf(status, sizeof(status), "ENERGY ERROR %.2E", sim.energyError());
    text(24, 473, status, 1.2f, muted);
    std::snprintf(status, sizeof(status), "ANG MOM ERROR %.2E", sim.angularMomentumError());
    text(24, 495, status, 1.2f, muted);
    std::snprintf(status, sizeof(status), "NET MOMENTUM %.2E", length(momentum(sim.bodies)));
    text(24, 517, status, 1.2f, muted);

    const float controlsY = static_cast<float>(height) - 190;
    text(24, controlsY, "CONTROLS", 1.4f, white);
    text(24, controlsY + 27, "DRAG     ROTATE VIEW", 1.3f, muted);
    text(24, controlsY + 47, "SCROLL   ZOOM", 1.3f, muted);
    text(24, controlsY + 67, "SPACE    PAUSE", 1.3f, muted);
    text(24, controlsY + 87, "[ / ]    SLOW / FAST", 1.3f, muted);
    text(24, controlsY + 107, "R        RESET ALL", 1.3f, muted);
    text(24, controlsY + 127, "W MESH O TRAILS D DATA", 1.3f, muted);
    text(24, controlsY + 147, "E ECLIPSE  S SHADOWS", 1.3f, teal);
    text(24, controlsY + 167, "N STEP WHILE PAUSED", 1.3f, muted);
    text(panelWidth + 28, 28, "THE SOLAR STUDY", 1.5f, white);
    text(static_cast<float>(width)-172, 28, "2 ORBIT / 02", 1.3f, teal);
    std::snprintf(status, sizeof(status), "3 BODY / START RATIO %.2F / %s", initialPeriodRatio,
                  sim.resonanceCandidate ? "RESONANCE CANDIDATE" : "CIRCULAR START");
    text(panelWidth + 28, 52, status, 1.2f, muted);
    if (sim.collision) text(panelWidth + 28, 128, "CONTACT DETECTED / PAUSED / R TO RESET", 1.4f, teal);
    if (eclipseCoverage > 0.001f) {
        text(panelWidth + 28, 80, "ECLIPSE / EMBER DAY CENTER", 1.5f, teal);
        std::snprintf(status, sizeof(status), "SUN BLOCKED %.0F PCT / S TO COMPARE", eclipseCoverage * 100);
        text(panelWidth + 28, 102, status, 1.3f, white);
    }
    const std::string mode = std::string(wireframe ? "WIREFRAME" : "SHADED") + " / TRAILS " + (orbits ? "ON" : "OFF")
                             + " / SHADOWS " + (shadows ? "ON" : "OFF");
    text(panelWidth + 28, static_cast<float>(height) - (showDiagnostics ? diagnosticsHeight : 0) - 32, mode, 1.3f, muted);
    if (showDiagnostics) diagnostics(width, height, sim.diagnostics);

    render(width, height);
}

void Hud::drawAlien(int width, int height, const CatalogSimulation& sim, bool surface, bool trails,
                    bool showData, const std::array<Vec3, 4>& bodyLabels) {
    vertices_.clear();
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    const Vec3 amber{1.0f,0.68f,0.36f}, dim{0.54f,0.56f,0.57f}, panel{0.027f,0.037f,0.043f};
    rect(0, 0, w, 88, panel);
    rect(28, 87, w-56, 1, {0.17f,0.18f,0.18f});
    text(30, 23, "ORBIT / "+sim.preset.id, 2.8f, white);
    text(31, 57, sim.preset.title, 1.15f, dim);
    text(w-340, 28, surface ? "[ SURFACE ]   V ORBITAL" : "V SURFACE   [ ORBITAL ]", 1.4f, amber);
    text(w-340, 57, "1 ORBIT / 01   P NEXT WORLD", 1.1f, dim);
    text(32, h-302, "TRISOLARIS", 3.8f, white);
    text(34, h-260, "ONE WORLD. THREE SUNS.", 1.4f, amber);
    const auto sample = sim.measure();
    char label[128];
    std::snprintf(label,sizeof(label),"YEAR %.3F / %s / %.3G X",sim.time,
                  sim.collision ? "CONTACT" : sim.paused ? "PAUSED" : "RUNNING",sim.speed);
    text(34, h-234, label, 1.2f, white);
    for (int i = 0; i < 3; ++i) {
        const float x = w-540 + i*170.0f;
        rect(x, 112, 3, 29, sim.preset.colors[i]);
        text(x+12, 112, sim.preset.names[i], 1.3f, white);
        std::snprintf(label,sizeof(label),"%.4F SOLAR MASSES",sim.stars[i].mass);
        text(x+12, 133, label, 0.95f, white);
    }
    if (showData) {
        const float top = h-178;
        rect(0,top,w,178,panel);
        rect(28,top,w-56,1,{0.17f,0.18f,0.18f});
        const float cell = (w-64)/3;
        const std::array<const char*,3> headings{{"INCIDENT LIGHT / EARTH = 1", "NEAREST SUN / AU", "STELLAR ENERGY ERROR"}};
        const std::array<double,3> values{{sample.flux,sample.nearest,sample.energyError}};
        for (int i=0;i<3;++i) {
            const float x=32+i*cell;
            text(x,top+18,headings[i],1.1f,dim);
            std::snprintf(label,sizeof(label),i==2 ? "%.2E" : "%.3F",values[i]);
            text(x,top+43,label,2.7f,i==0 ? amber : white);
            if(i>0) rect(x-15,top+17,1,67,{0.17f,0.18f,0.18f});
        }
        text(32,top+91,"LIGHT HISTORY",0.95f,dim);
        if(sim.history.size()>1) {
            double high=0.1;
            for(const auto& entry:sim.history) high=std::max(high,entry.flux);
            const double duration=sim.history.back().time-sim.history.front().time;
            float px=32,py=top+140;
            bool first=true;
            for(const auto& entry:sim.history) {
                const float x=32+(w-64)*static_cast<float>((entry.time-sim.history.front().time)/duration);
                const float y=top+141-34*static_cast<float>(entry.flux/high);
                if(!first) line(px,py,x,y,1.4f,amber);
                px=x;py=y;first=false;
            }
        }
        text(32,h-20,"CHOSEN MASS RATIOS / INVENTED ORBITS / DISKS ENLARGED / PLANET IS A TEST PARTICLE",0.95f,dim);
    }
    text(32,h-(showData ? 202 : 30),surface ? "DRAG LOOK / V ORBIT / [ ] SPEED / R RESET / D DATA" :
         (trails ? "DRAG ROTATE / SCROLL ZOOM / V SURFACE / O TRAILS ON" : "DRAG ROTATE / SCROLL ZOOM / V SURFACE / O TRAILS OFF"),1.1f,white);
    if (!surface) for (std::size_t i=0;i<bodyLabels.size();++i) {
        const auto p=bodyLabels[i];
        if (p.z>0 && p.x>10 && p.x<w-100 && p.y>160 && p.y<h-(showData ? 205 : 40))
            text(p.x+16,p.y-3,i<3 ? sim.preset.names[i] : "TRISOLARIS",1.15f,i<3 ? sim.preset.colors[i] : teal);
    }
    render(width,height);
}

void Hud::render(int width, int height) {
    glDisable(GL_DEPTH_TEST);
    shader_.use();
    shader_.viewport(static_cast<float>(width), static_cast<float>(height));
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(PixelVertex)), vertices_.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glEnable(GL_DEPTH_TEST);
}
} // namespace orbit
