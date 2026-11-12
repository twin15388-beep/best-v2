// Minimal standalone services required by the pinned public physics objects.
// These definitions belong only to the companion executable, never the game.
#include <engine/shared/config.h>
#include <base/log.h>
#include <base/logger.h>
#include <base/str.h>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>

CConfig g_Config = [] {
    CConfig config{};
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) config.m_##Name = Def;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) config.m_##Name = Def;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) str_copy(config.m_##Name, Def, Len);
#define SET_CONFIG_DOMAIN(Domain)
#include <engine/shared/config_includes.h>
#undef SET_CONFIG_DOMAIN
#undef MACRO_CONFIG_STR
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_INT
    return config;
}();

void log_log_v(LEVEL level, const char *system, const char *format, va_list args)
{
    std::fprintf(stderr, "[%d][%s] ", static_cast<int>(level), system);
    std::vfprintf(stderr, format, args);
    std::fputc('\n', stderr);
}

void log_log(LEVEL level, const char *system, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    log_log_v(level, system, format, args);
    va_end(args);
}

void log_log_color_v(LEVEL level, LOG_COLOR, const char *system, const char *format, va_list args)
{
    log_log_v(level, system, format, args);
}

void log_log_color(LEVEL level, LOG_COLOR color, const char *system, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    log_log_color_v(level, color, system, format, args);
    va_end(args);
}

void log_global_logger_finish()
{
    std::fflush(stderr);
}
