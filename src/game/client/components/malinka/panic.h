#ifndef GAME_CLIENT_COMPONENTS_MALINKA_PANIC_H
#define GAME_CLIENT_COMPONENTS_MALINKA_PANIC_H

#include <string_view>

// This is presentation-only state, owned by the process, not the connection.
// Commands remain registered: binds, saving and explicit execution keep working.
class CMalinkaSession
{
	bool m_Hidden = false;

public:
	void Panic() { m_Hidden = true; }
	[[nodiscard]] bool IsHidden() const { return m_Hidden; }
	[[nodiscard]] bool IsConsoleCommandHidden(std::string_view Name) const
	{
		if(!m_Hidden)
			return false;
		if(!Name.empty() && Name.front() == '+')
			Name.remove_prefix(1);
		constexpr std::string_view Prefix = "malinka_";
		if(Name.size() < Prefix.size())
			return false;
		for(std::size_t Index = 0; Index < Prefix.size(); ++Index)
		{
			const char Ch = Name[Index];
			if((Ch >= 'A' && Ch <= 'Z' ? Ch - 'A' + 'a' : Ch) != Prefix[Index])
				return false;
		}
		return true;
	}
};

#endif
