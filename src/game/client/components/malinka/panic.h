#ifndef GAME_CLIENT_COMPONENTS_MALINKA_PANIC_H
#define GAME_CLIENT_COMPONENTS_MALINKA_PANIC_H

// This state belongs to the client process, not the server connection or config.
class CMalinkaSession
{
	bool m_Hidden = false;

public:
	void Panic() { m_Hidden = true; }
	[[nodiscard]] bool IsHidden() const { return m_Hidden; }
};

#endif
