/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "music_smtc.h"

#include <base/system.h>
#include <base/thread.h>

#include <engine/shared/http.h>
#include <engine/shared/json.h>

#include <chrono>
#include <algorithm>
#include <cctype>
#include <thread>

#if defined(CONF_FAMILY_WINDOWS)

#include <base/windows.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Control;

namespace
{
	enum class ECommand : int
	{
		NONE = 0,
		TOGGLE_PLAY_PAUSE,
		NEXT,
		PREV,
	};

	static void SetIfDifferent(std::string &Dst, const std::string &Src)
	{
		if(Dst != Src)
			Dst = Src;
	}

	static std::string Utf8FromHString(const winrt::hstring &Str)
	{
		auto Opt = windows_wide_to_utf8(Str.c_str());
		return Opt.value_or("");
	}

	static bool IsMusicSource(const std::string &SourceApp, const std::string &Title, const std::string &Artist)
	{
		std::string Source = SourceApp;
		std::transform(Source.begin(), Source.end(), Source.begin(), [](unsigned char C) { return (char)std::tolower(C); });
		const char *apMusicSources[] = {
			"spotify", "soundcloud", "yandex", "youtube.music", "music", "deezer",
			"tidal", "apple", "itunes", "foobar", "aimp", "winamp", "musicbee",
			"vlc", "groove", "zune", "mediamonkey"};
		for(const char *pSource : apMusicSources)
		{
			if(Source.find(pSource) != std::string::npos)
				return true;
		}
		// Browser media sessions do not expose the website name. Music services
		// normally provide both artist and title, unlike ordinary web video.
		return !Title.empty() && !Artist.empty();
	}
}

class CMusicSMTC::CWorker
{
public:
	CMusicSMTC *m_pSelf;
	std::atomic_int m_PendingCmd{(int)ECommand::NONE};

	CWorker(CMusicSMTC *pSelf) : m_pSelf(pSelf) {}

	void Run()
	{
		// thread_init initializes COM, but we also need WinRT apartment.
		winrt::init_apartment(winrt::apartment_type::multi_threaded);

		GlobalSystemMediaTransportControlsSessionManager Manager{nullptr};
		GlobalSystemMediaTransportControlsSession Session{nullptr};

		while(!m_pSelf->m_StopRequested.load())
		{
			// Ensure manager
			if(!Manager)
			{
				try
				{
					Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
				}
				catch(...)
				{
					UpdateUnavailable();
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					continue;
				}
			}

			// Refresh current session (might change when user changes active player)
			try
			{
				Session = Manager.GetCurrentSession();
			}
			catch(...)
			{
				Session = nullptr;
			}

			// Execute pending command on the current session
			const ECommand Cmd = (ECommand)m_PendingCmd.exchange((int)ECommand::NONE);
			if(Cmd != ECommand::NONE)
			{
				TryExecute(Session, Cmd);
			}

			// Poll info
			Poll(Session);

			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}
		winrt::uninit_apartment();
	}

private:
	void UpdateUnavailable()
	{
		std::scoped_lock Lock(m_pSelf->m_Mutex);
		m_pSelf->m_TrackInfo = {};
		m_pSelf->m_TrackInfo.m_Available = false;
		m_pSelf->m_TrackInfo.m_Playing = false;
	}

	void TryExecute(const GlobalSystemMediaTransportControlsSession &Session, ECommand Cmd)
	{
		if(!Session)
			return;
		try
		{
			switch(Cmd)
			{
			case ECommand::TOGGLE_PLAY_PAUSE:
				Session.TryTogglePlayPauseAsync().get();
				break;
			case ECommand::NEXT:
				Session.TrySkipNextAsync().get();
				break;
			case ECommand::PREV:
				Session.TrySkipPreviousAsync().get();
				break;
			default:
				break;
			}
		}
		catch(...)
		{
			// ignore
		}
	}

	void Poll(const GlobalSystemMediaTransportControlsSession &Session)
	{
		if(!Session)
		{
			UpdateUnavailable();
			return;
		}

		bool Playing = false;
		double PositionSeconds = 0.0;
		try
		{
			auto PlaybackInfo = Session.GetPlaybackInfo();
			if(PlaybackInfo)
			{
				const auto Status = PlaybackInfo.PlaybackStatus();
				Playing = Status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
			}
		}
		catch(...)
		{
			// keep default
		}
		try
		{
			PositionSeconds = std::chrono::duration<double>(Session.GetTimelineProperties().Position()).count();
		}
		catch(...)
		{
			// keep default
		}

		std::string Title;
		std::string Artist;
		std::string SourceApp;
		try
		{
			SourceApp = Utf8FromHString(Session.SourceAppUserModelId());
		}
		catch(...)
		{
			// keep empty
		}
		try
		{
			auto Props = Session.TryGetMediaPropertiesAsync().get();
			Title = Utf8FromHString(Props.Title());
			Artist = Utf8FromHString(Props.Artist());
		}
		catch(...)
		{
			// ignore
		}

		{
			std::scoped_lock Lock(m_pSelf->m_Mutex);
			m_pSelf->m_TrackInfo.m_Available = true;
			m_pSelf->m_TrackInfo.m_Playing = Playing;
			m_pSelf->m_TrackInfo.m_IsMusic = IsMusicSource(SourceApp, Title, Artist);
			m_pSelf->m_TrackInfo.m_PositionSeconds = PositionSeconds;
			SetIfDifferent(m_pSelf->m_TrackInfo.m_Title, Title);
			SetIfDifferent(m_pSelf->m_TrackInfo.m_Artist, Artist);
			SetIfDifferent(m_pSelf->m_TrackInfo.m_SourceApp, SourceApp);
		}
	}
};

CMusicSMTC::CMusicSMTC() = default;
CMusicSMTC::~CMusicSMTC() = default;

void CMusicSMTC::OnInit()
{
	StartWorker();
}

void CMusicSMTC::OnShutdown()
{
	if(m_pLyricsRequest)
		m_pLyricsRequest->Abort();
	m_pLyricsRequest.reset();
	StopWorker();
}

void CMusicSMTC::OnUpdate()
{
	const STrackInfo Track = GetTrackInfo();
	const std::string TrackKey = Track.m_Available && Track.m_IsMusic && !Track.m_Title.empty() ? Track.m_Artist + "\n" + Track.m_Title : "";
	if(TrackKey != m_LyricsTrackKey)
	{
		if(m_pLyricsRequest)
			m_pLyricsRequest->Abort();
		m_pLyricsRequest.reset();
		m_LyricsTrackKey = TrackKey;

		std::scoped_lock Lock(m_Mutex);
		m_TrackInfo.m_Lyrics.clear();
		m_TrackInfo.m_SyncedLyrics.clear();
		m_TrackInfo.m_LyricsFound = false;
		m_TrackInfo.m_LyricsLoading = false;
		if(TrackKey.empty())
			return;

		const auto Cached = m_LyricsCache.find(TrackKey);
		if(Cached != m_LyricsCache.end())
		{
			m_TrackInfo.m_Lyrics = Cached->second.first;
			m_TrackInfo.m_SyncedLyrics = Cached->second.second;
			m_TrackInfo.m_LyricsFound = !m_TrackInfo.m_Lyrics.empty() || !m_TrackInfo.m_SyncedLyrics.empty();
			return;
		}

		char aArtist[80];
		char aTitle[112];
		EscapeUrl(aArtist, Track.m_Artist.c_str());
		EscapeUrl(aTitle, Track.m_Title.c_str());
		char aUrl[256];
		str_format(aUrl, sizeof(aUrl), "https://lrclib.net/api/get?artist_name=%s&track_name=%s", aArtist, aTitle);
		m_pLyricsRequest = HttpGet(aUrl);
		m_pLyricsRequest->Header("User-Agent: KernelNet-DDNet/1.0 (lyrics HUD)");
		m_pLyricsRequest->FailOnErrorStatus(false);
		m_pLyricsRequest->MaxResponseSize(1024 * 1024);
		m_pLyricsRequest->Timeout(CTimeout{6000, 12000, 500, 8});
		m_TrackInfo.m_LyricsLoading = true;
		Http()->Run(m_pLyricsRequest);
		return;
	}

	if(!m_pLyricsRequest || !m_pLyricsRequest->Done())
		return;

	std::string PlainLyrics;
	std::string SyncedLyrics;
	if(m_pLyricsRequest->State() == EHttpState::DONE && m_pLyricsRequest->StatusCode() == 200)
	{
		json_value *pRoot = m_pLyricsRequest->ResultJson();
		if(pRoot && pRoot->type == json_object)
		{
			const json_value *pPlain = json_object_get(pRoot, "plainLyrics");
			const json_value *pSynced = json_object_get(pRoot, "syncedLyrics");
			if(pPlain && pPlain->type == json_string)
				PlainLyrics = json_string_get(pPlain);
			if(pSynced && pSynced->type == json_string)
				SyncedLyrics = json_string_get(pSynced);
		}
		if(pRoot)
			json_value_free(pRoot);
	}

	// Empty cache entries intentionally remember 404, timeouts and malformed
	// responses, so the client does not retry every frame or crash the HUD.
	m_LyricsCache[m_LyricsTrackKey] = {PlainLyrics, SyncedLyrics};
	{
		std::scoped_lock Lock(m_Mutex);
		const std::string CurrentKey = m_TrackInfo.m_Artist + "\n" + m_TrackInfo.m_Title;
		if(CurrentKey == m_LyricsTrackKey)
		{
			m_TrackInfo.m_Lyrics = PlainLyrics;
			m_TrackInfo.m_SyncedLyrics = SyncedLyrics;
			m_TrackInfo.m_LyricsFound = !PlainLyrics.empty() || !SyncedLyrics.empty();
			m_TrackInfo.m_LyricsLoading = false;
		}
	}
	m_pLyricsRequest.reset();
}

CMusicSMTC::STrackInfo CMusicSMTC::GetTrackInfo() const
{
	std::scoped_lock Lock(m_Mutex);
	return m_TrackInfo;
}

void CMusicSMTC::TogglePlayPause()
{
	if(!m_pWorker)
		return;
	((CWorker *)m_pWorker)->m_PendingCmd.store((int)ECommand::TOGGLE_PLAY_PAUSE);
}

void CMusicSMTC::Next()
{
	if(!m_pWorker)
		return;
	((CWorker *)m_pWorker)->m_PendingCmd.store((int)ECommand::NEXT);
}

void CMusicSMTC::Previous()
{
	if(!m_pWorker)
		return;
	((CWorker *)m_pWorker)->m_PendingCmd.store((int)ECommand::PREV);
}

void CMusicSMTC::StartWorker()
{
	if(m_WorkerRunning.exchange(true))
		return;
	m_StopRequested.store(false);
	m_pWorker = new CWorker(this);
	m_pThread = thread_init(CMusicSMTC::ThreadMain, m_pWorker, "smtc");
}

void CMusicSMTC::StopWorker()
{
	m_StopRequested.store(true);
	if(m_pThread)
	{
		thread_wait(m_pThread);
		m_pThread = nullptr;
	}
	if(m_pWorker)
	{
		delete (CWorker *)m_pWorker;
		m_pWorker = nullptr;
	}
	m_WorkerRunning.store(false);
}

void CMusicSMTC::ThreadMain(void *pUser)
{
	CWorker *pWorker = (CWorker *)pUser;
	pWorker->Run();
}

#else

CMusicSMTC::CMusicSMTC() = default;
CMusicSMTC::~CMusicSMTC() = default;

void CMusicSMTC::OnInit() {}
void CMusicSMTC::OnShutdown() {}
void CMusicSMTC::OnUpdate() {}

CMusicSMTC::STrackInfo CMusicSMTC::GetTrackInfo() const { return {}; }
void CMusicSMTC::TogglePlayPause() {}
void CMusicSMTC::Next() {}
void CMusicSMTC::Previous() {}

void CMusicSMTC::StartWorker() {}
void CMusicSMTC::StopWorker() {}
void CMusicSMTC::ThreadMain(void *) {}

#endif
