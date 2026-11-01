#include <game/client/spinbot_state.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

// Also runnable without GTest: compile with SPINBOT_STANDALONE_TEST defined.
#if !defined(SPINBOT_STANDALONE_TEST)
#include <gtest/gtest.h>
#endif

namespace
{
void Check(bool Condition)
{
	if(!Condition)
	{
		std::fputs("SpinBot regression check failed\n", stderr);
		std::abort();
	}
}

void CheckAngle(double Actual, double Expected)
{
	Check(std::abs(std::remainder(Actual - Expected, 360.0)) < 0.000001);
	Check(Actual >= 0.0 && Actual < 360.0);
}

void TestPhase()
{
	for(int Fps : {30, 60, 144, 240, 1000})
	{
		CSpinbotPhase Phase;
		Phase.Advance(0.0, 137.0, true);
		for(int Frame = 1; Frame <= 10 * Fps; ++Frame)
			Phase.Advance(Frame / (double)Fps, 137.0, true);
		CheckAngle(Phase.Angle(), 290.0);
	}
	CSpinbotPhase Phase;
	Phase.Advance(0.0, 90.0, true);
	Phase.Advance(1.0, 90.0, true);
	CheckAngle(Phase.Angle(), 90.0); // +Y points down: clockwise.
	Phase.Advance(1.0, -90.0, true);
	Phase.Advance(3.0, -90.0, true);
	CheckAngle(Phase.Angle(), 270.0);
	Phase.Advance(3.0, 720.0, false);
	Phase.Advance(10000.0, 720.0, false);
	Phase.Advance(10000.0, 720.0, true);
	CheckAngle(Phase.Angle(), 270.0); // No accumulated pause time.
	Phase.Advance(10000.25, 720.0, true);
	CheckAngle(Phase.Angle(), 90.0);
	Phase.Advance(10000.25, 180.0, true);
	Phase.Advance(10000.75, 180.0, true);
	CheckAngle(Phase.Angle(), 180.0); // Live speed change preserves phase.
	Phase.Advance(10000.75, 180.0, true);
	CheckAngle(Phase.Angle(), 180.0); // Repeated calls at the same instant.
	Phase.Advance(1.0, 180.0, true);
	CheckAngle(Phase.Angle(), 180.0); // Defensive clock rollback.

	CSpinbotPhase Connections[3];
	for(int Conn = 0; Conn < 3; ++Conn)
	{
		Connections[Conn].Advance(0.0, 90.0, true);
		Connections[Conn].Advance(Conn + 1.0, 90.0, true);
	}
	CheckAngle(Connections[0].Angle(), 90.0);
	CheckAngle(Connections[1].Angle(), 180.0);
	CheckAngle(Connections[2].Angle(), 270.0);
}

void TestInputPriority()
{
	CSpinbotInput Policy;
	CNetObj_PlayerInput Input{};
	Input.m_PlayerFlags = PLAYERFLAG_PLAYING;
	Input.m_TargetX = 42;
	Input.m_TargetY = -76;
	Input.m_Direction = -1;
	Input.m_Jump = 1;
	Input.m_WantedWeapon = 3;
	Policy.Capture(Input, 0);
	Check(Policy.CanRotate(Input, false));
	Check(!Policy.CanRotate(Input, true)); // Explicit avoid/unfreeze ownership.
	Input.m_TargetX++;
	Check(!Policy.CanRotate(Input, false)); // Legacy helper target change.
	Input.m_TargetX--;
	Input.m_Hook = 1;
	Check(!Policy.CanRotate(Input, false));
	Input.m_Hook = 0;
	Input.m_Fire = 1;
	Check(!Policy.CanRotate(Input, false)); // Helper-triggered shot.
	Policy.Capture(Input, 1);
	Check(!Policy.CanRotate(Input, false)); // Held auto-fire.
	Input.m_Fire = 2;
	Policy.Capture(Input, 0);
	Check(!Policy.CanRotate(Input, false)); // Quick click and release.
	Policy.Capture(Input, 2);
	Check(Policy.CanRotate(Input, false)); // Released: rotation resumes.
	Input.m_Fire = 0;
	Policy.Capture(Input, 63);
	Check(!Policy.CanRotate(Input, false)); // Fire counter wrap.
	Policy.Capture(Input, 0);
	Input.m_PlayerFlags = PLAYERFLAG_IN_MENU;
	Check(!Policy.CanRotate(Input, false));
	Input.m_PlayerFlags = PLAYERFLAG_CHATTING;
	Check(!Policy.CanRotate(Input, false));
	Check(Input.m_Direction == -1 && Input.m_Jump == 1 && Input.m_WantedWeapon == 3);
}
}

#if defined(SPINBOT_STANDALONE_TEST)
int main()
{
	TestPhase();
	TestInputPriority();
	std::puts("SpinBot phase and input priority tests passed");
}
#else
TEST(Spinbot, Phase) { TestPhase(); }
TEST(Spinbot, InputPriority) { TestInputPriority(); }
#endif
