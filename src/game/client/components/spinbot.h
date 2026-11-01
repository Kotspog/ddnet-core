#ifndef GAME_CLIENT_COMPONENTS_SPINBOT_H
#define GAME_CLIENT_COMPONENTS_SPINBOT_H

#include <base/vmath.h>
#include <engine/client/enums.h>
#include <engine/shared/protocol.h>
#include <game/client/component.h>
#include <game/client/spinbot_state.h>
#include <generated/protocol.h>

class CSpinbot : public CComponent
{
	struct CConnectionState
	{
		CSpinbotPhase m_Phase;
		CSpinbotInput m_Input;
		bool m_Spinning = false;
	};
	CConnectionState m_aState[NUM_DUMMIES];
	bool CanSpin(int Conn) const;
	void Advance(int Conn, bool Running);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;
	bool WantsInput() const;
	void CaptureLocalInput(int Conn, const CNetObj_PlayerInput &Input, int PreviousFire);
	void Apply(int Conn, CNetObj_PlayerInput &Input, bool HelperOwnsAim);
	vec2 DisplayAim(int Conn, vec2 LocalAim) const;
};

#endif
