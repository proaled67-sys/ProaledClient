#ifndef PROALEDCLIENT_MUSIC_ISLAND_H
#define PROALEDCLIENT_MUSIC_ISLAND_H
#include "engine/console.h"
#include "engine/graphics.h"
#include "engine/image.h"
#include "game/client/component.h"
#include "game/client/ui_rect.h"
#include "generated/protocol.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class CUi;
class IClient;
class ITextRender;
struct CNetObj_GameInfo;

class CMusicIsland : public CComponent
{
	static void ConShowCurMusicInfo(IConsole::IResult *pResult, void *pUserData);

	enum EControlButton
	{
		CONTROL_BUTTON_PREVIOUS = 0,
		CONTROL_BUTTON_PLAY_PAUSE,
		CONTROL_BUTTON_NEXT,
		NUM_CONTROL_BUTTONS,
	};

	struct SMusicInfo
	{
		bool m_Available = false;
		bool m_Playing = false;
		bool m_CanPlay = false;
		bool m_CanPause = false;
		bool m_CanGoPrevious = false;
		bool m_CanGoNext = false;
		std::string m_Title;
		std::string m_Artist;
		std::string m_Album;
	};

	struct SGameTimerRenderInfo
	{
		char m_aText[32]{};
		float m_TextWidth = 0.0f;
		ColorRGBA m_TextColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	};

	bool m_Extended = false;
	float m_ExtendAnim = 0.0f;

	CUIRect m_Rect;

	std::thread m_InfoWorker;
	std::atomic<bool> m_InfoWorkerRunning = false;
	std::atomic<bool> m_InfoWorkerStopRequested = false;
	std::mutex m_InfoWorkerMutex;
	std::condition_variable m_InfoWorkerCv;
	std::thread m_ImageWorker;
	std::atomic<bool> m_ImageWorkerRunning = false;
	std::atomic<bool> m_ImageWorkerStopRequested = false;
	int64_t m_NextInfoUpdateTime = 0;
	mutable std::mutex m_MusicInfoMutex;
	SMusicInfo m_MusicInfo;
	CImageInfo m_PendingMusicImage;
	std::string m_CurrentArtworkKey;
	std::string m_LastDetectedArtworkKey;
	int64_t m_LastDetectedArtworkChangeTime = 0;
	bool m_MusicImageDirty = false;
	IGraphics::CTextureHandle m_MusicImageTexture;
	int m_MusicImageWidth = 0;
	int m_MusicImageHeight = 0;
	bool m_LastNativeMousePressed = false;

	void ResetMusicInfo();
	void ResetMusicImage();
	void ResetVisualState();
	void ResetRuntimeState();
	SMusicInfo GetMusicInfo() const;
	void RenderMusicIsland();
	void RenderMusicIslandControls(CUIRect *pBase, const SMusicInfo &MusicInfo, vec2 MousePos, bool MouseClicked, bool MousePressed, float AnimProgress);
	bool DoControlButton(const CUIRect *pRect, const char *pIcon, bool Enabled, vec2 MousePos, bool MouseClicked, bool MousePressed, float AnimProgress);
	void RenderMusicIslandImage(CUIRect *pBase);
	void RenderMusicIslandVisualizer(CUIRect *pBase);
	void RenderMusicIslandMain(CUIRect *pBase);
	void UpdateMusicImageTexture();
	void StartInfoWorker();
	void StopInfoWorker();
	void StopImageWorker();
	void InfoWorkerLoop();
	void UpdateMusicInfo();
	void TriggerControlAction(EControlButton Button);
	bool CanUseMouseInteraction() const;
	vec2 MouseInteractionPos(vec2 ScreenTL, vec2 ScreenBR) const;
	static float GetStableGameTimerWidth(ITextRender *pTextRender, float FontSize, float TimeSeconds, bool ShowCentiseconds);
	static bool GetGameTimerRenderInfo(const CNetObj_GameInfo *pGameInfo, IClient *pClient, ITextRender *pTextRender, float FontSize, SGameTimerRenderInfo &RenderInfo);
	static float GetScrollingTextOffset(float Overflow, float Seconds);
	static void RenderCenteredClippedText(IGraphics *pGraphics, ITextRender *pTextRender, CUi *pUi, const CUIRect &Rect, const char *pText, float FontSize, const ColorRGBA &Color, float ScrollSeconds);

public:
	CMusicIsland();
	~CMusicIsland() override;
	int Sizeof() const override { return sizeof(*this); }

	void SetExtended(bool Extended);
	void RenderHud();

	void OnReset() override;
	void OnRender() override;
	void OnConsoleInit() override;
	void OnShutdown() override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool IsActive() const;
};

#endif //PROALEDCLIENT_MUSIC_ISLAND_H
