#ifndef GAME_CLIENT_SPINBOT_STATE_H
#define GAME_CLIENT_SPINBOT_STATE_H

#include <cmath>
#include <generated/protocol.h>

// The input is captured before Controls' weapon helpers. Only aim is replaced;
// all action counters and weapon/movement fields remain owned by the pipeline.
class CSpinbotInput
{
	CNetObj_PlayerInput m_LocalInput = {};
	bool m_Action = false;

public:
	void Capture(const CNetObj_PlayerInput &Input, int PreviousFire)
	{
		m_LocalInput = Input;
		m_Action = Input.m_Hook || (Input.m_Fire & 1) || Input.m_Fire != PreviousFire;
	}

	bool CanRotate(const CNetObj_PlayerInput &Input, bool HelperOwnsAim) const
	{
		return (Input.m_PlayerFlags & PLAYERFLAG_PLAYING) && !HelperOwnsAim && !m_Action &&
			!Input.m_Hook && !(Input.m_Fire & 1) && Input.m_Fire == m_LocalInput.m_Fire &&
			Input.m_TargetX == m_LocalInput.m_TargetX && Input.m_TargetY == m_LocalInput.m_TargetY;
	}
};

// Independent of rendering and the network tick rate. Paused time is discarded.
class CSpinbotPhase
{
	double m_Angle = 0.0;
	double m_LastTime = -1.0;
	double m_Speed = 0.0;
	bool m_Running = false;

public:
	void Advance(double Now, double DegreesPerSecond, bool Running)
	{
		if(m_LastTime >= 0.0 && Now >= m_LastTime && m_Running && Running)
		{
			m_Angle = std::fmod(m_Angle + (Now - m_LastTime) * m_Speed, 360.0);
			if(m_Angle < 0.0)
				m_Angle += 360.0;
		}
		m_LastTime = Now;
		m_Speed = DegreesPerSecond;
		m_Running = Running;
	}
	double Angle() const { return m_Angle; }
};

#endif
