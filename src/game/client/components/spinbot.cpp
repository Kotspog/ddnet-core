#include "spinbot.h"

#include <base/time.h>
#include <engine/client.h>
#include <engine/shared/config.h>
#include <game/client/gameclient.h>

bool CSpinbot::CanSpin(int Conn) const
{
	return Conn >= 0 && Conn < NUM_DUMMIES && Conn == g_Config.m_ClDummy &&
		g_Config.m_ClSpinbot && Client()->State() == IClient::STATE_ONLINE &&
		GameClient()->m_Snap.m_pLocalCharacter && !GameClient()->m_Snap.m_SpecInfo.m_Active &&
		!GameClient()->m_Menus.IsActive() && !GameClient()->m_Chat.IsActive() &&
		!GameClient()->m_GameConsole.IsActive() && !g_Config.m_ClZzClickGui &&
		!g_Config.m_ClEditor && !GameClient()->m_Spectator.IsActive() &&
		!GameClient()->m_Emoticon.IsActive() && !GameClient()->m_BindWheel.IsActive() &&
		!GameClient()->m_Motd.IsActive() && !GameClient()->m_ImportantAlert.IsActive() &&
		!GameClient()->m_KeyBinder.IsActive() &&
		(!GameClient()->m_Snap.m_pGameInfoObj ||
			!(GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_PAUSED));
}

bool CSpinbot::WantsInput() const
{
	return CanSpin(g_Config.m_ClDummy);
}

void CSpinbot::Advance(int Conn, bool Running)
{
	const double Speed = g_Config.m_ClSpinbotSpeed * (g_Config.m_ClSpinbotCounterClockwise ? -1.0 : 1.0);
	m_aState[Conn].m_Phase.Advance(time_get() / (double)time_freq(), Speed, Running);
	m_aState[Conn].m_Spinning = Running;
}

void CSpinbot::OnReset()
{
	for(auto &State : m_aState)
		State = CConnectionState{};
}

void CSpinbot::OnRender()
{
	for(int Conn = 0; Conn < NUM_DUMMIES; ++Conn)
	{
		const auto &Input = GameClient()->m_Controls.m_aInputData[Conn];
		Advance(Conn, CanSpin(Conn) && m_aState[Conn].m_Spinning &&
			!Input.m_Hook && !(Input.m_Fire & 1));
	}
}

void CSpinbot::CaptureLocalInput(int Conn, const CNetObj_PlayerInput &Input, int PreviousFire)
{
	// Preserve even a press/release occurring between two outgoing packets.
	m_aState[Conn].m_Input.Capture(Input, PreviousFire);
}

void CSpinbot::Apply(int Conn, CNetObj_PlayerInput &Input, bool HelperOwnsAim)
{
	auto &State = m_aState[Conn];
	// Controls' helpers run before OnSnapInput. Comparing with the pre-helper
	// target also protects silent FNG and subtick aim, without touching either.
	const bool Running = CanSpin(Conn) && State.m_Input.CanRotate(Input, HelperOwnsAim);
	Advance(Conn, Running);
	if(!Running)
		return;

	const double Radians = State.m_Phase.Angle() * (pi / 180.0);
	Input.m_TargetX = round_to_int(std::cos(Radians) * 1000.0);
	Input.m_TargetY = round_to_int(std::sin(Radians) * 1000.0);
}

vec2 CSpinbot::DisplayAim(int Conn, vec2 LocalAim) const
{
	if(!CanSpin(Conn) || g_Config.m_ClSpinbotAssist || !m_aState[Conn].m_Spinning)
		return LocalAim;
	const auto &Input = GameClient()->m_Controls.m_aInputData[Conn];
	if(Input.m_Hook || (Input.m_Fire & 1))
		return LocalAim;
	const double Radians = m_aState[Conn].m_Phase.Angle() * (pi / 180.0);
	return vec2(std::cos(Radians), std::sin(Radians)) * maximum(length(LocalAim), 1.0f);
}
