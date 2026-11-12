// Independent-process host services. No game math, collision, tick or weapon
// methods are reimplemented here. Exact pinned upstream sources implement them.
#include <engine/shared/config.h>
#include <base/log.h>
#include <base/logger.h>
#include <base/str.h>
#include <game/client/components/bestclient/ego_tiles_prediction.h>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>

CConfig g_Config = [] {
    CConfig Config{};
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Flags, Desc) Config.m_##Name = Def;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Flags, Desc) Config.m_##Name = Def;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Flags, Desc) str_copy(Config.m_##Name, Def, Len);
#define SET_CONFIG_DOMAIN(Domain)
#include <engine/shared/config_includes.h>
#undef SET_CONFIG_DOMAIN
#undef MACRO_CONFIG_STR
#undef MACRO_CONFIG_COL
#undef MACRO_CONFIG_INT
    return Config;
}();

void log_log_v(LEVEL Level, const char *System, const char *Format, va_list Args)
{
    std::fprintf(stderr, "[%d][%s] ", static_cast<int>(Level), System);
    std::vfprintf(stderr, Format, Args);
    std::fputc('\n', stderr);
}
void log_log(LEVEL Level, const char *System, const char *Format, ...)
{
    va_list Args; va_start(Args, Format); log_log_v(Level, System, Format, Args); va_end(Args);
}
void log_log_color_v(LEVEL Level, LOG_COLOR, const char *System, const char *Format, va_list Args)
{
    log_log_v(Level, System, Format, Args);
}
void log_log_color(LEVEL Level, LOG_COLOR, const char *System, const char *Format, ...)
{
    va_list Args; va_start(Args, Format); log_log_v(Level, System, Format, Args); va_end(Args);
}
void log_global_logger_finish() { std::fflush(stderr); }

// Fail closed, never silently erase ego-tile behavior. This acquisition only
// supports maps independently confirmed to have no such prediction quads.
// The owned world's pointer remains null; a mistakenly enabled call is fatal
// to that candidate simulation and must never produce a "safe" result.
void CEgoTilesPrediction::HandleCharacter(CCharacter *) const
{
    throw std::runtime_error("Unsupported ego-tile prediction: discard candidate");
}
