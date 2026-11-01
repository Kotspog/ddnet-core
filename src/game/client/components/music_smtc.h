/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MUSIC_SMTC_H
#define GAME_CLIENT_COMPONENTS_MUSIC_SMTC_H

#include <game/client/component.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class CHttpRequest;

class CMusicSMTC : public CComponent
{
public:
	struct STrackInfo
	{
		bool m_Available = false;
		bool m_Playing = false;
		bool m_IsMusic = false;
		std::string m_Title;
		std::string m_Artist;
		std::string m_SourceApp;
		std::string m_Lyrics;
		std::string m_SyncedLyrics;
		double m_PositionSeconds = 0.0;
		bool m_LyricsLoading = false;
		bool m_LyricsFound = false;
	};

	CMusicSMTC();
	~CMusicSMTC() override;

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnUpdate() override;

	STrackInfo GetTrackInfo() const;

	void TogglePlayPause();
	void Next();
	void Previous();

private:
	class CWorker;

	static void ThreadMain(void *pUser);

	void StartWorker();
	void StopWorker();

	mutable std::mutex m_Mutex;
	STrackInfo m_TrackInfo;

	std::atomic_bool m_StopRequested{false};
	std::atomic_bool m_WorkerRunning{false};
	void *m_pThread = nullptr;
	void *m_pWorker = nullptr;
	std::shared_ptr<CHttpRequest> m_pLyricsRequest;
	std::string m_LyricsTrackKey;
	std::unordered_map<std::string, std::pair<std::string, std::string>> m_LyricsCache;
};

#endif // GAME_CLIENT_COMPONENTS_MUSIC_SMTC_H
