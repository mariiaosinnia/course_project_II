#include "LoginScreen.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace {
Color accent() { return Color::MediumPurple1; }
Color accent_soft() { return Color::Violet; }
Color soft_text() { return Color::GrayDark; }
Color section_text() { return Color::White; }
}

LoginScreen::LoginScreen(
    ScreenInteractive& screen,
    std::function<void(const std::string&)> on_connect)
    : screen_(screen)
    , on_connect_(std::move(on_connect))
{
    auto input = Input(&username_, "");

    auto btn = Button("join", [this] {
        if (username_.empty()) return;
        if (on_connect_) on_connect_(username_);
        set_status("connecting...");
    });

    auto container = Container::Vertical({input, btn});

    component_ = Renderer(container, [this, container] {
        // Рядок статусу
        Element status_line = text("  no account needed ♥")
            | color(soft_text());

        if (!status_.empty()) {
            status_line = text("  " + status_)
                | color(status_is_error_ ? accent() : soft_text());
        }

        return vbox({
            // Header
            vbox({
                hbox({
                    text("  /ᐠ｡ꞈ｡ᐟ\\  ") | color(accent_soft()),
                    text("StudyRoom") | bold | color(section_text()),
                }),
                text("       focus together") | color(soft_text()),
            }),
            separator(),

            filler(),

            // Форма
            vbox({
                text("  callsign") | color(soft_text()),
                text(""),
                hbox({
                    text("  "),
                    container->ChildAt(0)->Render()
                        | border
                        | color(soft_text())
                        | size(WIDTH, EQUAL, 28),
                }),
                text(""),
                container->ChildAt(1)->Render() | center,
            }),

            filler(),
            separator(),

            // Footer
            status_line,

        }) | border;
    });
}

void LoginScreen::set_status(const std::string& status, bool is_error)
{
    status_ = status;
    status_is_error_ = is_error;
    screen_.PostEvent(Event::Custom);
}

Component LoginScreen::component()
{
    return component_;
}
