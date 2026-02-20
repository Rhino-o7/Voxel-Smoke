#pragma once

namespace yc {

class GameClock {
public:
    void setPaused(bool paused) { this->paused = paused; }
    bool isPaused() const { return paused; }

    // 0 = stop time, 1 = normal, 2 = double speed, etc.
    void setTimeScale(double scale) { timeScale = (scale < 0.0) ? 0.0 : scale; }
    double getTimeScale() const { return timeScale; }

    // Advance using real delta seconds; returns scaled dt actually applied to sim time.
    double tick(double realDtSec) {
        if (paused) return 0.0;
        const double scaled = realDtSec * timeScale;
        simTimeSec += scaled;
        return scaled;
    }

    double getSimTimeSec() const { return simTimeSec; }

    void reset() {
        simTimeSec = 0.0;
        paused = false;
        timeScale = 1.0;
    }

    void setSimTimeSec(double value) { simTimeSec = value; }

private:
    double simTimeSec = 0.0;
    double timeScale = 1.0;
    bool paused = false;
};

}