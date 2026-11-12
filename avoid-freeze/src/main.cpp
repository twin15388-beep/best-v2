#include "platform/layout_check.h"
#include "platform/win32_reader.h"
#include "policy/avoid_policy.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <stdexcept>
#include <thread>
#include <vector>

namespace
{
struct Options
{
    std::uint32_t process_id{};
    std::uint32_t run_ms{30000};
    bool observe_only{};
};

std::uint32_t ParseDecimal(std::wstring_view text, std::uint32_t maximum)
{
    if(text.empty() || text.size() > 10)
        throw std::invalid_argument("expected a bounded decimal number");
    std::uint64_t value{};
    for(const wchar_t character : text)
    {
        if(character < L'0' || character > L'9')
            throw std::invalid_argument("expected a bounded decimal number");
        value = value * 10 + static_cast<std::uint64_t>(character - L'0');
        if(value > maximum)
            throw std::invalid_argument("numeric argument exceeds its documented bound");
    }
    return static_cast<std::uint32_t>(value);
}

Options ParseArguments(int count, wchar_t **arguments)
{
    Options options;
    for(int index = 1; index < count; ++index)
    {
        const std::wstring_view argument(arguments[index]);
        if(argument == L"--observe-only")
            options.observe_only = true;
        else if(argument == L"--pid" || argument == L"--run-ms")
        {
            if(index + 1 >= count)
                throw std::invalid_argument("missing numeric argument");
            const auto maximum = argument == L"--pid" ? UINT32_MAX : 300000U;
            const std::uint32_t number = ParseDecimal(arguments[++index], maximum);
            if(number == 0 || (argument == L"--run-ms" && number < 1000))
                throw std::invalid_argument("numeric argument is outside its documented range");
            if(argument == L"--pid")
                options.process_id = number;
            else
                options.run_ms = number;
        }
        else
            throw std::invalid_argument("unsupported argument; this milestone is observe-only");
    }
    if(!options.observe_only || options.process_id == 0)
        throw std::invalid_argument("usage: AvoidFreeze.exe --observe-only --pid N [--run-ms 1000..300000]");
    return options;
}

const char *DecisionName(avoidfreeze::DecisionKind kind)
{
    switch(kind)
    {
    case avoidfreeze::DecisionKind::unavailable: return "unavailable";
    case avoidfreeze::DecisionKind::already_frozen: return "already_frozen";
    case avoidfreeze::DecisionKind::immune: return "immune";
    case avoidfreeze::DecisionKind::no_predicted_hazard: return "no_predicted_hazard";
    case avoidfreeze::DecisionKind::suggested_direction: return "suggested_direction";
    case avoidfreeze::DecisionKind::no_safe_direction: return "no_safe_direction";
    }
    return "invalid";
}

std::string JsonString(std::string_view text)
{
    std::string escaped;
    escaped.reserve(text.size() + 2);
    escaped.push_back('"');
    for(const char raw_character : text)
    {
        const auto character = static_cast<unsigned char>(raw_character);
        if(character == '"' || character == '\\')
        {
            escaped.push_back('\\');
            escaped.push_back(static_cast<char>(character));
        }
        else if(character < 0x20)
            escaped += " ";
        else
            escaped.push_back(static_cast<char>(character));
    }
    escaped.push_back('"');
    return escaped;
}

int DirectionValue(int direction)
{
    if(direction < -1 || direction > 1)
        return 0;
    return direction;
}

int RunObserveOnly(const Options &options)
{
    avoidfreeze::win32::Win32Reader reader(options.process_id);
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds(options.run_ms);
    auto last_tick_advance = start;
    int previous_tick = -1;
    std::uint64_t samples{};
    std::uint64_t predicted_hazard_samples{};
    std::uint64_t advice_samples{};
    std::uint64_t unavailable_samples{};
    std::string last_decision{"warming"};
    std::string last_reason{"waiting for a fresh predicted tick"};
    int last_direction{};
    int last_suggestion{};
    int last_hazard_tick{};

    std::cout << "AvoidFreeze 0.1.0 experimental standalone milestone; observe-only; input=disabled\n"
              << "The program reads the supported process only; it does not patch files, write process memory, inject DLLs, or send input.\n";

    bool blocked = false;
    std::string reason;
    try
    {
        while(std::chrono::steady_clock::now() < deadline)
        {
            auto frame = reader.Capture();
            const auto now = std::chrono::steady_clock::now();
            if(frame.player.tick != previous_tick)
            {
                previous_tick = frame.player.tick;
                last_tick_advance = now;
                const auto decision = avoidfreeze::Analyze(frame);
                ++samples;
                last_decision = DecisionName(decision.kind);
                last_reason = decision.reason;
                last_direction = DirectionValue(decision.current_direction);
                last_suggestion = DirectionValue(decision.suggested_direction);
                last_hazard_tick = decision.predicted_hazard_tick;
                if(decision.predicted_hazard_tick > 0)
                    ++predicted_hazard_samples;
                if(decision.kind == avoidfreeze::DecisionKind::suggested_direction)
                    ++advice_samples;
                if(decision.kind == avoidfreeze::DecisionKind::unavailable)
                {
                    ++unavailable_samples;
                    blocked = true;
                    reason = decision.reason;
                    std::cout << "BLOCKED " << JsonString(reason) << '\n';
                    break;
                }
            }
            else if(now - last_tick_advance > std::chrono::milliseconds(500))
            {
                ++unavailable_samples;
                blocked = true;
                last_decision = "stale_tick";
                reason = "prediction tick did not advance within the 500 ms local freshness bound";
                std::cout << "BLOCKED " << JsonString(reason) << '\n';
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    catch(const std::exception &error)
    {
        ++unavailable_samples;
        blocked = true;
        last_decision = "unavailable";
        reason = error.what();
        std::cout << "BLOCKED " << JsonString(reason) << '\n';
    }

    if(reason.empty())
        reason = samples == 0 ? "no fresh snapshot was accepted" : last_reason;
    if(samples == 0)
        blocked = true;
    std::cout << "RESULT {\"mode\":\"observe-only-advice\",\"samples\":" << samples
              << ",\"predicted_hazard_samples\":" << predicted_hazard_samples
              << ",\"advice_samples\":" << advice_samples
              << ",\"unavailable_samples\":" << unavailable_samples
              << ",\"last_decision\":" << JsonString(last_decision)
              << ",\"last_reason\":" << JsonString(reason)
              << ",\"current_direction\":" << last_direction
              << ",\"suggested_direction\":" << last_suggestion
              << ",\"hazard_tick\":" << last_hazard_tick
              << ",\"writes_process_memory\":false,\"injects_dll\":false,\"sends_input\":false"
              << ",\"gameplay_outcome_verified\":false}\n";
    return blocked ? 1 : 0;
}
} // namespace

int wmain(int count, wchar_t **arguments)
{
    try
    {
        if(count == 2 && std::wstring_view(arguments[1]) == L"--self-test-layout")
        {
            avoidfreeze::win32::ValidateMeasuredLayout();
            std::cout << "native-layout-self-test=passed; process access=none; game input=none\n";
            return 0;
        }
        const auto options = ParseArguments(count, arguments);
        return RunObserveOnly(options);
    }
    catch(const std::exception &error)
    {
        std::cout << "BLOCKED " << JsonString(error.what()) << '\n';
        return 1;
    }
}
