// Standalone regression fixture for the production ApplyAvoid method.
// Run scripts/test_avoid.py after building a Debug game-client with MSVC.
// The runner extracts the method verbatim; collision/core/prediction are linked
// from the client build. No duplicate Avoid algorithm or mock physics is used.

#include <engine/client.h>
#include <engine/map.h>
#include <engine/shared/config.h>

#include <game/client/prediction/entities/character.h>
#include <game/client/prediction/gameworld.h>
#include <game/client/spinbot_state.h>
#include <game/collision.h>
#include <game/layers.h>
#include <game/mapbugs.h>
#include <game/mapitems.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

int g_Failures = 0;
void Check(bool Condition, const char *Message)
{
	if(!Condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", Message);
		++g_Failures;
	}
}
class CMemoryMap : public IMap
{
public:
	static constexpr int W = 32, H = 24;
	CMapItemGroup m_Group{};
	CMapItemLayerTilemap m_Layer{};
	std::array<CTile, W * H> m_Tiles{};
	CMemoryMap()
	{
		m_Group.m_Version = 3;
		m_Group.m_NumLayers = 1;
		m_Layer.m_Layer.m_Type = LAYERTYPE_TILES;
		m_Layer.m_Version = 3;
		m_Layer.m_Flags = TILESLAYERFLAG_GAME;
		m_Layer.m_Width = W;
		m_Layer.m_Height = H;
		m_Layer.m_Data = 0;
	}
	int GetDataSize(int) const override { return sizeof(m_Tiles); }
	void *GetData(int) override { return m_Tiles.data(); }
	void *GetDataSwapped(int I) override { return GetData(I); }
	const char *GetDataString(int) override { return ""; }
	void UnloadData(int) override {}
	int NumData() const override { return 1; }
	int GetItemSize(int I) override { return I == 0 ? sizeof(m_Group) : sizeof(m_Layer); }
	void *GetItem(int I, int *T = nullptr, int *Id = nullptr) override
	{
		if(T)
			*T = I == 0 ? MAPITEMTYPE_GROUP : MAPITEMTYPE_LAYER;
		if(Id)
			*Id = 0;
		return I == 0 ? static_cast<void *>(&m_Group) : static_cast<void *>(&m_Layer);
	}
	void GetType(int T, int *S, int *N) override
	{
		*S = T == MAPITEMTYPE_GROUP ? 0 : 1;
		*N = (T == MAPITEMTYPE_GROUP || T == MAPITEMTYPE_LAYER) ? 1 : 0;
	}
	int FindItemIndex(int T, int Id) override { return Id == 0 ? (T == MAPITEMTYPE_GROUP ? 0 : (T == MAPITEMTYPE_LAYER ? 1 : -1)) : -1; }
	void *FindItem(int T, int Id) override
	{
		int I = FindItemIndex(T, Id);
		return I < 0 ? nullptr : GetItem(I);
	}
	int NumItems() const override { return 2; }
	bool Load(const char *, IStorage *, const char *, int) override { return false; }
	bool Load(IStorage *, const char *, int) override { return false; }
	void Unload() override {}
	bool IsLoaded() const override { return true; }
	IOHANDLE File() const override { return nullptr; }
	const char *FullName() const override { return "avoid-test"; }
	const char *BaseName() const override { return FullName(); }
	const char *Path() const override { return FullName(); }
	SHA256_DIGEST Sha256() const override { return {}; }
	unsigned Crc() const override { return 0; }
	int Size() const override { return sizeof(m_Tiles); }
	void Tile(int X, int Y, int Index) { m_Tiles[Y * W + X].m_Index = Index; }
};
class CGameClient
{
public:
	struct CTestClient
	{
		int m_Tick = 100;
		int PredGameTick(int) const { return m_Tick; }
	} m_Client;
	struct CTestControls
	{
		CNetObj_PlayerInput m_aInputData[NUM_DUMMIES]{};
		CNetObj_PlayerInput m_aLastData[NUM_DUMMIES]{};
	} m_Controls;
	CMemoryMap m_Map;
	CLayers m_Layers;
	CCollision m_Collision;
	CMapBugs m_Bugs;
	CTuningParams m_aTuning[256];
	CGameWorld m_PredictedWorld;
	CTestClient *Client() { return &m_Client; }
	CCollision *Collision() { return &m_Collision; }
	bool ApplyAvoid(CNetObj_PlayerInput *, int, int, bool, bool &);
	void Init(vec2 Pos, vec2 Vel, int Jumped = 0, int HookState = HOOK_IDLE)
	{
		m_Layers.Init(&m_Map, true);
		m_Collision.Init(&m_Layers);
		m_PredictedWorld.Init(&m_Collision, m_aTuning, &m_Bugs);
		m_PredictedWorld.m_WorldConfig = {};
		m_PredictedWorld.m_WorldConfig.m_IsDDRace = true;
		m_PredictedWorld.m_WorldConfig.m_PredictDDRace = true;
		m_PredictedWorld.m_WorldConfig.m_PredictTiles = true;
		m_PredictedWorld.m_WorldConfig.m_PredictFreeze = 1;
		m_PredictedWorld.m_GameTick = 100;
		CNetObj_Character Snap{};
		Snap.m_X = round_to_int(Pos.x);
		Snap.m_Y = round_to_int(Pos.y);
		Snap.m_Weapon = WEAPON_GUN;
		Snap.m_HookState = HookState;
		Snap.m_HookedPlayer = -1;
		auto *Ch = new CCharacter(&m_PredictedWorld, 0, &Snap);
		m_PredictedWorld.InsertEntity(Ch);
		auto Core = Ch->GetCore();
		Core.m_Pos = Pos;
		Core.m_Vel = Vel;
		Core.m_Jumped = Jumped;
		Core.m_Tuning = m_aTuning[0];
		Core.m_HookState = HookState;
		Core.m_HookPos = Pos + vec2(0, -100);
		Core.m_HookDir = vec2(0, -1);
		Ch->SetCore(Core);
		bool Changed = false;
		g_Config.m_ClZzAvoidEnabled = 0;
		ApplyAvoid(nullptr, 0, -1, false, Changed);
		g_Config.m_ClZzAvoidEnabled = 1;
		g_Config.m_ClZzAvoidFreeze = 1;
		g_Config.m_ClZzAvoidKill = 1;
		g_Config.m_ClZzAvoidTele = 0;
		g_Config.m_ClZzAvoidJump = 1;
		g_Config.m_ClZzAvoidTicks = 20;
		g_Config.m_ClZzAvoidHookAssist = 1;
		g_Config.m_ClZzAvoidHookAssistRange = 512;
		g_Config.m_ClZzAvoidRescueAggression = 50;
		g_Config.m_ClZzAvoidKogProfile = 0;
		g_Config.m_ClZzAvoidDirection = 0;
	}
	CNetObj_PlayerInput Run(CNetObj_PlayerInput In, int Conn = 0, bool Owned = false)
	{
		m_Controls.m_aInputData[Conn] = In;
		bool Changed = false;
		ApplyAvoid(&In, Conn, 0, Owned, Changed);
		return In;
	}
};
#include "avoid_implementation.inc"
CNetObj_PlayerInput Input(int Direction = 0)
{
	CNetObj_PlayerInput I{};
	I.m_TargetX = 1000;
	I.m_Direction = Direction;
	I.m_PlayerFlags = PLAYERFLAG_PLAYING;
	return I;
}
std::unique_ptr<CGameClient> Falling(int Jumped = 0, bool Ceiling = false)
{
	auto G = std::make_unique<CGameClient>();
	for(int X = 0; X < CMemoryMap::W; ++X)
	{
		G->m_Map.Tile(X, 12, TILE_FREEZE);
		if(Ceiling)
			G->m_Map.Tile(X, 8, TILE_SOLID);
	}
	G->Init(vec2(320, Ceiling ? 335 : 365), vec2(0, Ceiling ? 3 : 7), Jumped);
	return G;
}
int main()
{
	{
		auto G = Falling(2, true);
		g_Config.m_ClZzAvoidTicks = 4;
		auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
		auto Core = Ch->GetCore();
		Core.m_Pos.y = 348;
		Core.m_Vel.y = 4;
		Ch->SetCore(Core);
		Check(G->Run(Input()).m_Hook == 1,
			"low sensitivity still gives rescue hook enough flight time");
		G = std::make_unique<CGameClient>();
		for(int X = 0; X < CMemoryMap::W; ++X)
		{
			G->m_Map.Tile(X, 12, TILE_FREEZE);
			G->m_Map.Tile(X, 4, TILE_SOLID);
		}
		G->Init(vec2(320, 348), vec2(0, 4));
		g_Config.m_ClZzAvoidTicks = 4;
		const auto In = Input();
		const auto Out = G->Run(In);
		Check(mem_comp(&In, &Out, sizeof(In)) == 0,
			"early hook does not take over while a less invasive rescue remains available");
	}
	for(int FreezeTile : {TILE_DFREEZE, TILE_LFREEZE})
	{
		auto G = std::make_unique<CGameClient>();
		for(int X = 0; X < CMemoryMap::W; ++X)
			G->m_Map.Tile(X, 12, FreezeTile);
		G->Init(vec2(320, 365), vec2(0, 7));
		Check(G->Run(Input()).m_Jump == 1,
			"exact baseline catches extended freeze omitted by core-only gate");
	}
	{
		auto G = Falling();
		auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
		auto Core = Ch->GetCore();
		Core.m_Pos = vec2(320, 200);
		Core.m_Vel = vec2(0, 0);
		Ch->SetCore(Core);
		Check(G->Run(Input()).m_Jump == 0, "safe baseline before prediction correction");
		Core.m_Pos = vec2(320, 365);
		Core.m_Vel = vec2(0, 7);
		Ch->SetCore(Core);
		Check(G->Run(Input()).m_Jump == 1,
			"same input tick with corrected prediction does not reuse stale safe result");
	}
	{
		auto G = std::make_unique<CGameClient>();
		for(int Y = 0; Y < CMemoryMap::H; ++Y)
		{
			G->m_Map.Tile(8, Y, TILE_NOHOOK);
			G->m_Map.Tile(11, Y, TILE_NOHOOK);
		}
		G->m_Map.Tile(10, 12, TILE_FREEZE);
		G->Init(vec2(326, 352), vec2(0, 3), 2);
		g_Config.m_ClZzAvoidKogProfile = 1;
		g_Config.m_ClZzAvoidDirection = 1;
		Check(G->Run(Input()).m_Direction == -1,
			"narrow gap can escape sideways from neutral input when baseline really freezes");
	}
	{
		auto G = std::make_unique<CGameClient>();
		for(int X = 0; X < CMemoryMap::W; ++X)
			G->m_Map.Tile(X, 12, TILE_NOHOOK);
		G->m_Map.Tile(11, 11, TILE_FREEZE);
		G->m_Map.Tile(12, 11, TILE_FREEZE);
		G->Init(vec2(320, 369), vec2(10, 0));
		auto Out = G->Run(Input(1));
		Check(Out.m_Jump == 1, "ground jump rescue");
		Check(Out.m_Direction == 1, "ground keeps direction");
		Check(G->m_Controls.m_aInputData[0].m_Jump == 0, "injection does not overwrite user button");
	}
	{
		auto G = Falling();
		auto In = Input();
		auto Out = G->Run(In);
		Check(Out.m_Jump == 1, "air jump rescue");
		Check(G->Run(In).m_Jump == 1, "same tick cached pulse");
		In.m_Fire = 1;
		In.m_TargetY = 200;
		Out = G->Run(In);
		Check(Out.m_Jump == 1 && Out.m_TargetY == 200, "same tick fire keeps pulse/aim");
		G->m_Client.m_Tick++;
		Out = G->Run(Input());
		Check(Out.m_Jump == 0, "next tick releases pulse");
		Check(G->Run(Input()).m_Jump == 0, "no repeated injection during release tick");
		auto Core = G->m_PredictedWorld.GetCharacterById(0)->GetCore();
		Core.m_Jumped = 1;
		G->m_PredictedWorld.GetCharacterById(0)->SetCore(Core);
		G->m_Client.m_Tick++;
		Check(G->Run(Input()).m_Jump == 0, "await actual predicted held release");
		Core.m_Jumped = 0;
		G->m_PredictedWorld.GetCharacterById(0)->SetCore(Core);
		G->m_Client.m_Tick++;
		Check(G->Run(Input()).m_Jump == 1, "rearm after released core");
		for(int Conn = 1; Conn < NUM_DUMMIES; ++Conn)
			Check(G->Run(Input(), Conn).m_Jump == 1, "connection isolation");
		G->m_Client.m_Tick = 10;
		Check(G->Run(Input()).m_Jump == 1, "tick rollback resets jump state");
	}
	{
		auto G = Falling(2);
		Check(G->Run(Input()).m_Jump == 0, "spent air jump rejected");
		auto In = Input();
		In.m_Jump = 1;
		Check(G->Run(In).m_Jump == 1, "held user jump preserved");
	}
	{
		auto G = Falling();
		auto In = Input();
		auto Out = G->Run(In);
		Check(Out.m_Jump == 1, "initial injection");
		G->m_Client.m_Tick++;
		In.m_Jump = 1;
		Check(G->Run(In).m_Jump == 1, "user press during automatic release wins");
	}
	{
		auto G = Falling();
		g_Config.m_ClZzAvoidJump = 0;
		Check(G->Run(Input()).m_Jump == 0, "jump config respected");
	}
	{
		for(int Weapon = WEAPON_HAMMER; Weapon < NUM_WEAPONS; ++Weapon)
		{
			auto G = Falling(2, true);
			auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
			Ch->SetActiveWeapon(Weapon);
			auto In = Input();
			In.m_Fire = 1;
			In.m_WantedWeapon = Weapon + 1;
			auto Out = G->Run(In);
			Check(!Out.m_Hook && Out.m_TargetX == In.m_TargetX && Out.m_TargetY == In.m_TargetY, "fire blocks new rescue for every weapon");
			G->m_Client.m_Tick++;
			Out = G->Run(In);
			Check(!Out.m_Hook && Out.m_TargetX == In.m_TargetX, "held fire stays protected");
		}
	}
	{
		auto G = Falling(2, true);
		auto In = Input();
		Check(!G->Run(In, 0, true).m_Hook, "explicit helper ownership blocks rescue launch");
	}
	{
		auto G = Falling(2, true);
		auto Out = G->Run(Input());
		Check(Out.m_Hook == 1, "new rescue hook");
		for(int State : {HOOK_FLYING, HOOK_GRABBED})
		{
			auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
			auto Core = Ch->GetCore();
			Core.m_HookState = State;
			Ch->SetCore(Core);
			G->m_Client.m_Tick++;
			auto In = Input();
			In.m_Fire = 1;
			In.m_Hook = 1;
			In.m_TargetY = 321;
			Out = G->Run(In);
			Check(Out.m_Hook == 1 && Out.m_TargetX == 1000 && Out.m_TargetY == 321, "existing hook with fire keeps shot aim");
		}
		auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
		auto Core = Ch->GetCore();
		Core.m_HookState = HOOK_IDLE;
		Ch->SetCore(Core);
		auto In = Input();
		In.m_Fire = 1;
		In.m_Hook = 1;
		In.m_TargetY = 321;
		Out = G->Run(In);
		Check(Out.m_Hook == 0 && Out.m_TargetX == 1000, "idle rescue is not relaunched along fire target");
	}
	{
		auto G = Falling(2, true);
		Check(G->Run(Input()).m_Hook == 1, "rescue hook starts before toggle test");
		g_Config.m_ClZzAvoidHookAssist = 0;
		G->m_Client.m_Tick++;
		Check(!G->Run(Input()).m_Hook, "disabling rescue hook releases automatic hold");
		g_Config.m_ClZzAvoidHookAssist = 1;
	}
	{
		auto G = std::make_unique<CGameClient>();
		for(int X = 0; X < CMemoryMap::W; ++X)
			G->m_Map.Tile(X, 12, TILE_FREEZE);
		for(int Y = 5; Y < 13; ++Y)
		{
			G->m_Map.Tile(9, Y, TILE_SOLID);
			G->m_Map.Tile(11, Y, TILE_SOLID);
		}
		G->Init(vec2(336, 365), vec2(0, 7));
		g_Config.m_ClZzAvoidDirection = 1;
		g_Config.m_ClZzAvoidKogProfile = 1;
		auto Out = G->Run(Input(1));
		Check(Out.m_Direction == 1, "KoG profile keeps stable narrow-gap direction");
		g_Config.m_ClZzAvoidKogProfile = 0;
	}

	{
		for(int Case = 0; Case < 3; ++Case)
		{
			auto G = std::make_unique<CGameClient>();
			for(int X = 0; X < CMemoryMap::W; ++X)
				G->m_Map.Tile(X, 12, TILE_FREEZE);
			if(Case == 0)
				for(int Y = 3; Y < 12; ++Y)
					G->m_Map.Tile(12, Y, TILE_FREEZE);
			G->Init(vec2(320, 315), vec2(0, 5), Case == 1 ? 2 : 0, Case ? HOOK_GRABBED : HOOK_IDLE);
			auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
			auto Core = Ch->GetCore();
			if(Case)
			{
				Core.m_HookPos = vec2(320, 600);
				Ch->SetCore(Core);
			}
			g_Config.m_ClZzAvoidDirection = 1;
			auto In = Input(1);
			In.m_Hook = Case ? 1 : 0;
			In.m_Fire = 1;
			auto Out = G->Run(In);
			if(Case == 0)
				Check(Out.m_Jump && Out.m_Direction != 1, "direction+jump rescue");
			if(Case == 1)
				Check(!Out.m_Hook && !Out.m_Jump, "dangerous hook release");
			if(Case == 2)
				Check(!Out.m_Hook && Out.m_Jump, "jump+hook release rescue");
			Check(Out.m_TargetX == In.m_TargetX && Out.m_TargetY == In.m_TargetY, "all movement rescue preserves fire aim");
		}
	}
	{
		auto G = std::make_unique<CGameClient>();
		for(int X = 0; X < CMemoryMap::W; ++X)
		{
			G->m_Map.Tile(X, 12, TILE_FREEZE);
			G->m_Map.Tile(X, 3, TILE_SOLID);
		}
		G->Init(vec2(320, 365), vec2(0, 7));
		auto Out = G->Run(Input());
		Check(Out.m_Jump && !Out.m_Hook, "better/equal jump survives rescue-hook comparison");
	}
	{
		auto G = Falling();
		auto In = Input();
		auto Out = G->Run(In);
		bool Changed = false;
		g_Config.m_ClZzAvoidEnabled = 0;
		G->ApplyAvoid(&In, 0, 0, false, Changed);
		Check(In.m_Jump == 0, "disable releases injected jump");
		g_Config.m_ClZzAvoidEnabled = 1;
		Check(G->Run(Input()).m_Jump == 1, "disable resets injection state");
	}
	{
		auto G = Falling();
		auto Out = G->Run(Input());
		// Replay exactly the outgoing one-tick pulse and subsequent release in the
		// real prediction world. No separate approximate jump physics is involved.
		auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
		Ch->OnDirectInput(&Out);
		Ch->OnPredictedInput(&Out);
		G->m_PredictedWorld.m_GameTick++;
		G->m_PredictedWorld.Tick();
		Check((Ch->Core()->m_TriggeredEvents & COREEVENT_AIR_JUMP) != 0, "sent pulse produces real prediction air jump");
		const float FirstVelocity = Ch->Core()->m_Vel.y;
		G->m_Client.m_Tick++;
		auto Released = G->Run(Input());
		Ch->OnDirectInput(&Released);
		Ch->OnPredictedInput(&Released);
		G->m_PredictedWorld.m_GameTick++;
		G->m_PredictedWorld.Tick();
		Check(!Released.m_Jump && !(Ch->Core()->m_Jumped & 1), "sent release clears real core held bit");
		Check(Ch->Core()->m_Vel.y > FirstVelocity, "release does not repeat jump impulse");
	}
	// Replay complete rescue sequences, including attachment and release, in the
	// real world. The raw user hook stays released throughout these scenarios.
	for(int Scenario = 0; Scenario < 5; ++Scenario)
	{
		auto G = std::make_unique<CGameClient>();
		if(Scenario == 1)
		{
			for(int X = 0; X < CMemoryMap::W; ++X)
			{
				G->m_Map.Tile(X, 8, TILE_FREEZE);
				G->m_Map.Tile(X, 14, TILE_SOLID);
			}
			G->Init(vec2(320, 335), vec2(0, -9), 2);
		}
		else
		{
			for(int X = 0; X < CMemoryMap::W; ++X)
			{
				G->m_Map.Tile(X, 12, TILE_FREEZE);
				if(Scenario != 2)
					G->m_Map.Tile(X, 8, TILE_SOLID);
			}
			if(Scenario == 2 || Scenario == 3)
				for(int Y = 5; Y < 14; ++Y)
				{
					G->m_Map.Tile(Scenario == 3 ? 9 : 8, Y, TILE_SOLID);
					if(Scenario == 3)
						G->m_Map.Tile(11, Y, TILE_SOLID);
				}
			G->Init(vec2(Scenario == 3 ? 336 : 320, 335), vec2(0, 3), 2);
		}
		g_Config.m_ClZzAvoidKogProfile = Scenario >= 3;
		if(Scenario == 4)
		{
			auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
			auto Core = Ch->GetCore();
			Core.m_Tuning.m_HookDragAccel = 6.0f;
			G->m_aTuning[0].m_HookDragAccel = 6.0f;
			Ch->SetCore(Core);
		}
		bool Launched = false, Attached = false, Released = false;
		int ReleaseAt = -1;
		for(int Tick = 0; Tick < 48; ++Tick)
		{
			auto Out = G->Run(Input());
			if(Out.m_Hook)
				Launched = true;
			if(Launched && !Out.m_Hook && !Released)
			{
				Released = true;
				ReleaseAt = Tick;
			}
			auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
			Ch->OnDirectInput(&Out);
			Ch->OnPredictedInput(&Out);
			G->m_PredictedWorld.m_GameTick++;
			G->m_PredictedWorld.Tick();
			G->m_Client.m_Tick++;
			Attached |= Ch->Core()->m_HookState == HOOK_GRABBED;
			Check(!Ch->m_FreezeTime && !Ch->Core()->m_IsInFreeze,
				"rescue trajectory survives attachment and release");
		}
		std::printf("Rescue scenario %d: attached=%d released=%d release tick=%d\n", Scenario, Attached, Released, ReleaseAt);
		Check(Launched && Attached && Released, "rescue completes launch, attachment and release");
		if(Scenario == 4)
			Check(ReleaseAt <= 6, "strong pull uses a short corrective hold");
	}
	for(int Direction : {-1, 1})
	{
		auto G = std::make_unique<CGameClient>();
		for(int X = 0; X < CMemoryMap::W; ++X)
		{
			G->m_Map.Tile(X, 10, TILE_SOLID);
			G->m_Map.Tile(X, 12, TILE_SOLID);
			G->m_Map.Tile(X, 9, TILE_FREEZE);
			G->m_Map.Tile(X, 13, TILE_FREEZE);
		}
		G->Init(vec2(512, 368), vec2(Direction * 5.0f, 0), 2);
		g_Config.m_ClZzAvoidKogProfile = 1;
		g_Config.m_ClZzAvoidDirection = 1;
		for(int Tick = 0; Tick < 24; ++Tick)
		{
			auto Out = G->Run(Input(Direction));
			Check(Out.m_Direction == Direction && !Out.m_Hook && !Out.m_Jump,
				"horizontal one-tile gap preserves a stable trajectory without oscillation");
			auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
			Ch->OnDirectInput(&Out);
			Ch->OnPredictedInput(&Out);
			G->m_PredictedWorld.m_GameTick++;
			G->m_PredictedWorld.Tick();
			G->m_Client.m_Tick++;
		}
	}
	for(int Kog = 0; Kog <= 1; ++Kog)
	{
		for(int Scenario = 0; Scenario < 4; ++Scenario)
		{
			auto G = std::make_unique<CGameClient>();
			if(Scenario == 2)
				for(int X = 0; X < CMemoryMap::W; ++X)
					G->m_Map.Tile(X, 12, TILE_SOLID);
			if(Scenario == 3)
				for(int X = 0; X < CMemoryMap::W; ++X)
					G->m_Map.Tile(X, 12, TILE_FREEZE);
			G->Init(vec2(320, Scenario == 2 ? 369 : (Scenario == 3 ? 382.5f : 250)),
				vec2(Scenario == 3 ? 0 : 5, Scenario == 1 ? 6 : (Scenario == 3 ? -10 : 0)), 2);
			g_Config.m_ClZzAvoidKogProfile = Kog;
			g_Config.m_ClZzAvoidDirection = 1;
			g_Config.m_ClZzAvoidTicks = 4;
			g_Config.m_ClZzAvoidRescueAggression = 100;
			const auto In = Input(Scenario == 3 ? 0 : 1);
			for(int Tick = 0; Tick < 8; ++Tick)
			{
				auto Out = G->Run(In);
				Check(mem_comp(&In, &Out, sizeof(In)) == 0,
					"safe movement/fall/platform/escape near freeze leaves complete user input unchanged");
				auto *Ch = G->m_PredictedWorld.GetCharacterById(0);
				Ch->OnDirectInput(&Out);
				Ch->OnPredictedInput(&Out);
				G->m_PredictedWorld.m_GameTick++;
				G->m_PredictedWorld.Tick();
				G->m_Client.m_Tick++;
			}
		}
		auto G = Falling();
		g_Config.m_ClZzAvoidKogProfile = Kog;
		g_Config.m_ClZzAvoidTicks = 4;
		Check(G->Run(Input()).m_Jump == 1, "soft mode still rescues imminent unavoidable freeze");
	}
	CSpinbotInput Spin;
	auto I = Input();
	Spin.Capture(I, 0);
	Check(!Spin.CanRotate(I, true), "SpinBot respects Avoid/helper owner");
	if(g_Failures)
	{
		std::printf("Avoid regression failures: %d\n", g_Failures);
		return 1;
	}
	std::puts("Avoid real-physics regression checks passed");
}
