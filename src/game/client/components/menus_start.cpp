/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_start.h"

#include <base/system.h>

#include <engine/client/updater.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/components/tclient/zz_theme.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#include <algorithm>
#include <cmath>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

void CMenusStart::RenderStartMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);
	const SZZThemePalette Palette = GetZZThemePalette(g_Config.m_ClZzTheme,
		color_cast<ColorRGBA>(ColorHSLA(g_Config.m_UiColor, true)));
	static int64_t s_LastFrame = 0;
	static float s_Appear = 1.0f;
	const int64_t Now = time_get();
	if(s_LastFrame == 0 || Now - s_LastFrame > time_freq() / 4)
		s_Appear = 0.0f;
	s_LastFrame = Now;
	const float Speed = std::clamp(g_Config.m_ClZzAnimationSpeed / 100.0f, 0.25f, 2.0f);
	s_Appear = std::min(1.0f, s_Appear + std::clamp(Client()->RenderFrameTime(), 0.0f, 0.05f) * 2.7f * Speed);
	const float Fade = g_Config.m_ClZzClickGuiAnimations ?
		s_Appear * s_Appear * (3.0f - 2.0f * s_Appear) : 1.0f;
	const float Jelly = g_Config.m_ClZzClickGuiAnimations ?
		1.0f - std::exp(-6.4f * s_Appear) * std::cos(10.5f * s_Appear) : 1.0f;

	const CUIRect Screen = *Ui()->Screen();
	Screen.Draw(ColorRGBA(0.006f, 0.010f, 0.026f, 0.10f), IGraphics::CORNER_NONE, 0.0f);

	// A quiet wordmark keeps the background visible and gives the action card room.
	CUIRect Wordmark(MainView.x, MainView.y + 45.0f + (1.0f - Jelly) * 14.0f, MainView.w, 52.0f);
	SLabelProperties WordmarkProps;
	WordmarkProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, Fade));
	Ui()->DoLabel(&Wordmark, "KernelNet", 31.0f, TEXTALIGN_MC, WordmarkProps);
	GameClient()->m_Menus.SetMenuRenderAlpha(Fade);

	const float VMargin = MainView.w / 2 - 190.0f;
	int NewPage = -1;

	const float CardW = std::min(356.0f, MainView.w - 36.0f);
	const float CardH = 239.0f;
	const float CardX = MainView.x + (MainView.w - CardW) * 0.5f + (1.0f - Jelly) * 18.0f;
	const float CardY = MainView.y + 139.0f + (1.0f - Jelly) * 10.0f;
	CUIRect ActionCard(CardX, CardY, CardW, CardH);
	ActionCard.Draw(ColorRGBA(0.84f, 0.90f, 1.0f, 0.10f * Fade), IGraphics::CORNER_ALL, 18.0f);
	CUIRect CardSurface = ActionCard;
	CardSurface.Margin(0.7f, &CardSurface);
	CardSurface.Draw(ZZThemeLerp(Palette.m_Panel, ColorRGBA(0.018f, 0.030f, 0.055f, 1.0f), 0.42f).WithAlpha(0.72f * Fade), IGraphics::CORNER_ALL, 17.3f);

	CUIRect Inner = ActionCard;
	Inner.Margin(14.0f, &Inner);
	CUIRect PlayButton, Row, LeftButton, RightButton;
	Inner.HSplitTop(44.0f, &PlayButton, &Inner);
	static CButtonContainer s_PlayButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_PlayButton, Localize("Play", "Start menu"), 0, &PlayButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 13.0f, 0.5f, Palette.m_Accent.WithAlpha(0.34f)))
		NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;

	Inner.HSplitTop(9.0f, nullptr, &Inner);
	Inner.HSplitTop(38.0f, &Row, &Inner);
	Row.VSplitLeft((Row.w - 8.0f) * 0.5f, &LeftButton, &RightButton);
	RightButton.VSplitLeft(8.0f, nullptr, &RightButton);
	static CButtonContainer s_DemoButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_DemoButton, Localize("Demos"), 0, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 11.0f, 0.5f, Palette.m_PanelHover.WithAlpha(0.42f)))
		NewPage = CMenus::PAGE_DEMOS;
	static CButtonContainer s_MapEditorButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_MapEditorButton, Localize("Editor"), 0, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 11.0f, 0.5f, GameClient()->Editor()->HasUnsavedData() ? Palette.m_Accent.WithAlpha(0.28f) : Palette.m_PanelHover.WithAlpha(0.42f)))
	{
		g_Config.m_ClEditor = 1;
		Input()->MouseModeRelative();
	}

	Inner.HSplitTop(7.0f, nullptr, &Inner);
	Inner.HSplitTop(38.0f, &Row, &Inner);
	Row.VSplitLeft((Row.w - 8.0f) * 0.5f, &LeftButton, &RightButton);
	RightButton.VSplitLeft(8.0f, nullptr, &RightButton);
	static CButtonContainer s_LocalServerButton;
	const bool LocalServerRunning = GameClient()->m_LocalServer.IsServerRunning();
	if(GameClient()->m_Menus.DoButton_Menu(&s_LocalServerButton, LocalServerRunning ? Localize("Stop server") : Localize("Run server"), 0, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 11.0f, 0.5f, LocalServerRunning ? ColorRGBA(0.16f, 0.48f, 0.30f, 0.38f) : Palette.m_PanelHover.WithAlpha(0.42f)))
	{
		if(LocalServerRunning)
			GameClient()->m_LocalServer.KillServer();
		else
			GameClient()->m_LocalServer.RunServer({});
	}
	static CButtonContainer s_SettingsButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_SettingsButton, Localize("Settings"), 0, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 11.0f, 0.5f, Palette.m_PanelHover.WithAlpha(0.42f)))
		NewPage = CMenus::PAGE_SETTINGS;

	Inner.HSplitTop(10.0f, nullptr, &Inner);
	Inner.HSplitTop(27.0f, &Row, &Inner);
	Row.VSplitLeft((Row.w - 8.0f) * 0.5f, &LeftButton, &RightButton);
	RightButton.VSplitLeft(8.0f, nullptr, &RightButton);
	static CButtonContainer s_TelegramButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_TelegramButton, Localize("Telegram"), 0, &LeftButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 8.0f, 0.15f, ColorRGBA(0.08f, 0.14f, 0.22f, 0.42f)))
		Client()->ViewLink("https://t.me/KernelDDNet");
	static CButtonContainer s_NewsButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_NewsButton, Localize("News"), 0, &RightButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 8.0f, 0.15f, ColorRGBA(0.08f, 0.14f, 0.22f, 0.42f)))
		Client()->ViewLink("https://t.me/KernelDDNet");

	Inner.HSplitTop(8.0f, nullptr, &Inner);
	CUIRect QuitButton;
	Inner.HSplitTop(30.0f, &QuitButton, &Inner);
	static CButtonContainer s_QuitButton;
	if(GameClient()->m_Menus.DoButton_Menu(&s_QuitButton, Localize("Quit"), 0, &QuitButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 9.0f, 0.25f, ColorRGBA(0.30f, 0.075f, 0.10f, 0.38f)))
	{
		if(GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
			GameClient()->m_Menus.ShowQuitPopup();
		else
			Client()->Quit();
	}

	// render version
	CUIRect CurVersion, ConsoleButton;
	MainView.HSplitBottom(45.0f, nullptr, &CurVersion);
	CurVersion.VSplitRight(40.0f, &CurVersion, nullptr);
	CurVersion.HSplitTop(20.0f, &ConsoleButton, &CurVersion);
	CurVersion.HSplitTop(5.0f, nullptr, &CurVersion);
	ConsoleButton.VSplitRight(40.0f, nullptr, &ConsoleButton);
	Ui()->DoLabel(&CurVersion, GAME_RELEASE_VERSION, 14.0f, TEXTALIGN_MR);

#if defined(CONF_AUTOUPDATE)
	CUIRect UpdateToDateText;
	MainView.HSplitTop(15.0f, &UpdateToDateText, nullptr);
	UpdateToDateText.VSplitRight(40.0f, &UpdateToDateText, nullptr);
	if(!GameClient()->m_TClient.NeedUpdate() && GameClient()->m_TClient.m_FetchedTClientInfo)
	{
		Ui()->DoLabel(&UpdateToDateText, TCLocalize("(On Latest)"), 14.0f, TEXTALIGN_MR);
	}
	else
	{
		Ui()->DoLabel(&UpdateToDateText, TCLocalize("(Fetching Update Info)"), 14.0f, TEXTALIGN_MR);
	}
#endif
	static CButtonContainer s_ConsoleButton;
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	if(GameClient()->m_Menus.DoButton_Menu(&s_ConsoleButton, FontIcon::TERMINAL, 0, &ConsoleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f)))
	{
		GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	CUIRect VersionUpdate;
	MainView.HSplitBottom(20.0f, nullptr, &VersionUpdate);
	VersionUpdate.VMargin(VMargin, &VersionUpdate);
#if defined(CONF_AUTOUPDATE)
	CUIRect UpdateButton;
	VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &UpdateButton);
	VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

	char aBuf[128];
	const IUpdater::EUpdaterState State = Updater()->GetCurrentState();
	const bool NeedUpdate = GameClient()->m_TClient.NeedUpdate();

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		static CButtonContainer s_VersionUpdate;
		if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Update now"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			Updater()->InitiateUpdate();
		}
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		static CButtonContainer s_VersionUpdate;
		if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Restart"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			Client()->Restart();
		}
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		Ui()->RenderProgressBar(UpdateButton, Updater()->GetCurrentPercent() / 100.0f);
	}

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		str_format(aBuf, sizeof(aBuf), Localize("TClient %s is out!"), GameClient()->m_TClient.m_aVersionStr);
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::CLEAN)
	{
		aBuf[0] = '\0';
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		char aCurrentFile[64];
		Updater()->GetCurrentFile(aCurrentFile, sizeof(aCurrentFile));
		str_format(aBuf, sizeof(aBuf), Localize("Downloading %s:"), aCurrentFile);
	}
	else if(State == IUpdater::FAIL)
	{
		str_copy(aBuf, Localize("Update failed! Check log…"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		str_copy(aBuf, Localize("DDNet Client updated!"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
#elif defined(CONF_INFORM_UPDATE)
	if(str_comp(Client()->LatestVersion(), "0") != 0 && false)
	{
		CUIRect DownloadButton;
		VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &DownloadButton);
		VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

		static CButtonContainer s_DownloadButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_DownloadButton, Localize("Download"), 0, &DownloadButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		{
			Client()->ViewLink("https://ddnet.org/downloads/");
		}

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		SLabelProperties UpdateLabelProps;
		UpdateLabelProps.SetColor(ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML, UpdateLabelProps);
	}
#endif

	if(NewPage != -1)
	{
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}
	GameClient()->m_Menus.SetMenuRenderAlpha(1.0f);
}

bool CMenusStart::CheckHotKey(int Key) const
{
	return !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() && // no modifier
	       Input()->KeyPress(Key) &&
	       !GameClient()->m_GameConsole.IsActive();
}
