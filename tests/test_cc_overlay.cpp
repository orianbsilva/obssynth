// The CC overlay (src/app/common/cc_overlay.hpp): an absolute knob picks the value up before it moves it; a
// button presses once; learn binds the next CC and saves the map as the person's; a map survives its JSON.
#include <doctest/doctest.h>

#ifndef _WIN32 // the person's folder through XDG_DATA_HOME (setenv)
#include <unistd.h>

#include "../src/app/common/cc_overlay.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace
{
    struct Fixture
    {
        float value = 0.5f;
        int presses = 0;
        ccov::Overlay ov;
        std::string dir;
        Fixture()
        {
            dir = (std::filesystem::temp_directory_path() / ("ccov-test-" + std::to_string(::getpid()))).string();
            setenv("XDG_DATA_HOME", dir.c_str(), 1);
            ccov::Map m;
            m.name = "Test";
            m.cc = {{21, -1, "knob"}, {102, -1, "@next"}};
            ov.load("ccov-test", {m});
        }
        ~Fixture()
        {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
        }
        bool cc(int n, float v)
        {
            return ov.onCc(0, n, v, [this](const std::string &id)
                           {
                ccov::Target t;
                if (id == "knob")
                    t.v = &value;
                else if (id == "@next")
                    t.press = [this] { ++presses; };
                return t; });
        }
    };
} // namespace

TEST_CASE("CC overlay: an absolute knob picks the value up first")
{
    Fixture f;
    f.cc(21, 0.1f); // far below 0.5: nothing yet
    CHECK(f.value == doctest::Approx(0.5f));
    f.cc(21, 0.3f);
    CHECK(f.value == doctest::Approx(0.5f));
    f.cc(21, 0.6f); // crossed it: caught
    CHECK(f.value == doctest::Approx(0.6f));
    f.cc(21, 0.2f); // from now on it follows
    CHECK(f.value == doctest::Approx(0.2f));
    f.ov.retarget(); // the panel moved it elsewhere: picks up again
    f.value = 0.9f;
    f.cc(21, 0.1f);
    CHECK(f.value == doctest::Approx(0.9f));
    f.cc(21, 0.89f); // close enough
    CHECK(f.value == doctest::Approx(0.89f));
}

TEST_CASE("CC overlay: looking for the controller again keeps what the knobs picked up")
{
    Fixture f; // a regression: the panels look every 3 s, and each look made every knob pick up again
    f.cc(21, 0.5f);
    f.cc(21, 0.7f);
    CHECK(f.value == doctest::Approx(0.7f));
    f.ov.detect();
    f.cc(21, 0.75f);
    CHECK(f.value == doctest::Approx(0.75f));
}

TEST_CASE("CC overlay: a button presses once per push")
{
    Fixture f;
    f.cc(102, 1.0f);
    f.cc(102, 1.0f);
    f.cc(102, 0.0f);
    f.cc(102, 1.0f);
    CHECK(f.presses == 2);
}

TEST_CASE("CC overlay: learn binds the next CC, one CC per control, and saves the map")
{
    Fixture f;
    f.ov.arm = "knob";
    CHECK(f.cc(30, 0.7f));
    CHECK(f.ov.arm.empty());
    CHECK(f.ov.ccOf({"knob"}) == 30); // 21 no longer moves it
    f.value = 0.5f;
    f.cc(21, 0.5f);
    f.cc(21, 0.9f);
    CHECK(f.value == doctest::Approx(0.5f));
    REQUIRE(!f.ov.map()->path.empty());
    CHECK(std::filesystem::exists(f.ov.map()->path));

    ccov::Overlay again; // the person's file replaces the factory's map of that name
    ccov::Map m;
    m.name = "Test";
    m.cc = {{21, -1, "knob"}};
    again.load("ccov-test", {m});
    CHECK(again.ccOf({"knob"}) == 30);
}

TEST_CASE("CC overlay: a map survives its JSON")
{
    ccov::Map m, back;
    m.name = "Pads";
    m.match = "Launchkey";
    m.cc = {{21, 9, "pad.sel.TUNE"}, {41, -1, "pad.bank1.LEVEL"}};
    m.notes[36] = 0;
    m.notes[51] = 15;
    REQUIRE(ccov::fromJson(ccov::toJson(m), back));
    CHECK(back.name == "Pads");
    CHECK(back.match == "Launchkey");
    REQUIRE(back.cc.size() == 2);
    CHECK(back.cc[0].channel == 9);
    CHECK(back.cc[1].channel == -1);
    CHECK(back.cc[1].target == "pad.bank1.LEVEL");
    CHECK(back.notes[36] == 0);
    CHECK(back.notes[51] == 15);
    CHECK(back.notes[40] == -1);
}
#endif
