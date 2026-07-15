#include "music_island.h"

#include <engine/font_icons.h>
#include <engine/gfx/image_manipulation.h>
#include <engine/keys.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>

#include <cmath>
#include <chrono>
#include <cstring>
#include <utility>
#include <vector>

#if defined(CONF_FAMILY_WINDOWS) && defined(_MSC_VER) && __has_include(<winrt/base.h>)
#define CONF_MUSIC_ISLAND_WINRT 1
#endif

#if defined(CONF_MUSIC_ISLAND_WINRT)
#pragma comment(lib, "runtimeobject.lib")
#include <winrt/base.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#elif defined(CONF_PLATFORM_LINUX) && defined(CONF_MUSIC_ISLAND_MPRIS)
#include <gio/gio.h>
#endif

struct SMusicIslandProperties
{
	static constexpr float ms_Padding = 1.0f;
	static constexpr float ms_Rounding = 3.0f;
	static constexpr float ms_ArtworkRadiusFraction = 0.2f;
	static constexpr float ms_BaseHeight = 10.0f;
	static constexpr float ms_ExpandedInfoHeight = 8.5f;
	static constexpr float ms_ControlGap = 1.0f;
	static constexpr float ms_ControlHeight = 8.0f;
	static ColorRGBA WindowColorDark() { return ColorRGBA(0.2f, 0.2f, 0.2f, 0.9f); };
};

static ColorRGBA MusicIslandWindowColor()
{
	return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_PcShowMusicIslandColorBar, true));
}

static ColorRGBA MusicIslandGapsColor()
{
	return color_cast<ColorRGBA>(ColorHSLA(g_Config.m_PcShowMusicIslandSectionsColor, true));
}

#if defined(CONF_MUSIC_ISLAND_WINRT)
static constexpr int64_t gs_MusicIslandArtworkDebounceMs = 350;
#endif

#if defined(CONF_MUSIC_ISLAND_WINRT) || (defined(CONF_PLATFORM_LINUX) && defined(CONF_MUSIC_ISLAND_MPRIS))
static bool MusicIslandDebugEnabled()
{
	return g_Config.m_PcShowMusicIslandDebug != 0;
}

static void LogMusicIslandDebug(const char *pSys, const char *pMsg)
{
	if(!MusicIslandDebugEnabled())
		return;

	dbg_msg(pSys, "%s", pMsg);
}
#endif

static vec2 UiMouseToScreen(const CUIRect *pUiScreen, vec2 UiMousePos, vec2 ScreenTL, vec2 ScreenBR)
{
	return vec2(
		ScreenTL.x + (UiMousePos.x - pUiScreen->x) * (ScreenBR.x - ScreenTL.x) / pUiScreen->w,
		ScreenTL.y + (UiMousePos.y - pUiScreen->y) * (ScreenBR.y - ScreenTL.y) / pUiScreen->h);
}

static CUIRect HudToUiRect(const CUIRect &HudRect, const CUIRect &UiScreen, float HudWidth, float HudHeight)
{
	CUIRect UiRect;
	UiRect.x = UiScreen.x + HudRect.x * UiScreen.w / HudWidth;
	UiRect.y = UiScreen.y + HudRect.y * UiScreen.h / HudHeight;
	UiRect.w = HudRect.w * UiScreen.w / HudWidth;
	UiRect.h = HudRect.h * UiScreen.h / HudHeight;
	return UiRect;
}

static float HudToUiX(float HudX, const CUIRect &UiScreen, float HudScreenX0, float HudWidth)
{
	return UiScreen.x + (HudX - HudScreenX0) * UiScreen.w / HudWidth;
}

static float HudToUiY(float HudY, const CUIRect &UiScreen, float HudScreenY0, float HudHeight)
{
	return UiScreen.y + (HudY - HudScreenY0) * UiScreen.h / HudHeight;
}

static bool CanRenderMusicIslandForClientState(IClient *pClient)
{
	if(g_Config.m_ClEditor)
		return false;

	const int State = pClient->State();
	return State == IClient::STATE_ONLINE || State == IClient::STATE_DEMOPLAYBACK;
}

float CMusicIsland::GetStableGameTimerWidth(ITextRender *pTextRender, float FontSize, float TimeSeconds, bool ShowCentiseconds)
{
	static float s_LastFontSize = -1.0f;
	static float s_TextWidthM = 0.0f;
	static float s_TextWidthH = 0.0f;
	static float s_TextWidth0D = 0.0f;
	static float s_TextWidth00D = 0.0f;
	static float s_TextWidth000D = 0.0f;
	static float s_TextWidthMwC = 0.0f;
	static float s_TextWidthHwC = 0.0f;
	static float s_TextWidth0DwC = 0.0f;
	static float s_TextWidth00DwC = 0.0f;
	static float s_TextWidth000DwC = 0.0f;

	if(s_LastFontSize != FontSize)
	{
		s_TextWidthM = pTextRender->TextWidth(FontSize, "00:00", -1, -1.0f);
		s_TextWidthH = pTextRender->TextWidth(FontSize, "00:00:00", -1, -1.0f);
		s_TextWidth0D = pTextRender->TextWidth(FontSize, "0d 00:00:00", -1, -1.0f);
		s_TextWidth00D = pTextRender->TextWidth(FontSize, "00d 00:00:00", -1, -1.0f);
		s_TextWidth000D = pTextRender->TextWidth(FontSize, "000d 00:00:00", -1, -1.0f);
		s_TextWidthMwC = pTextRender->TextWidth(FontSize, "00:00.00", -1, -1.0f);
		s_TextWidthHwC = pTextRender->TextWidth(FontSize, "00:00:00.00", -1, -1.0f);
		s_TextWidth0DwC = pTextRender->TextWidth(FontSize, "0d 00:00:00.00", -1, -1.0f);
		s_TextWidth00DwC = pTextRender->TextWidth(FontSize, "00d 00:00:00.00", -1, -1.0f);
		s_TextWidth000DwC = pTextRender->TextWidth(FontSize, "000d 00:00:00.00", -1, -1.0f);
		s_LastFontSize = FontSize;
	}

	if(!ShowCentiseconds)
	{
		return TimeSeconds >= 3600 * 24 * 100 ? s_TextWidth000D :
			TimeSeconds >= 3600 * 24 * 10 ? s_TextWidth00D :
			TimeSeconds >= 3600 * 24 ? s_TextWidth0D :
			TimeSeconds >= 3600 ? s_TextWidthH :
			s_TextWidthM;
	}

	return TimeSeconds >= 3600 * 24 * 100 ? s_TextWidth000DwC :
		TimeSeconds >= 3600 * 24 * 10 ? s_TextWidth00DwC :
		TimeSeconds >= 3600 * 24 ? s_TextWidth0DwC :
		TimeSeconds >= 3600 ? s_TextWidthHwC :
		s_TextWidthMwC;
}

bool CMusicIsland::GetGameTimerRenderInfo(const CNetObj_GameInfo *pGameInfo, IClient *pClient, ITextRender *pTextRender, float FontSize, SGameTimerRenderInfo &RenderInfo)
{
	if(!pGameInfo || (pGameInfo->m_GameStateFlags & GAMESTATEFLAG_SUDDENDEATH))
		return false;

	const int GameTick = pClient->GameTick(g_Config.m_ClDummy);
	const int GameTickSpeed = pClient->GameTickSpeed();
	float TimeSeconds = 0.0f;
	if(pGameInfo->m_TimeLimit && pGameInfo->m_WarmupTimer <= 0)
	{
		TimeSeconds = pGameInfo->m_TimeLimit * 60.0f -
			(float)(GameTick - pGameInfo->m_RoundStartTick) / GameTickSpeed;

		if(pGameInfo->m_GameStateFlags & GAMESTATEFLAG_GAMEOVER)
			TimeSeconds = 0.0f;
	}
	else if(pGameInfo->m_GameStateFlags & GAMESTATEFLAG_RACETIME)
	{
		TimeSeconds = (float)(GameTick + pGameInfo->m_WarmupTimer) / GameTickSpeed;
	}
	else
	{
		TimeSeconds = (float)(GameTick - pGameInfo->m_RoundStartTick) / GameTickSpeed;
	}

	str_time((int64_t)(TimeSeconds * 100), g_Config.m_PcShowMilliSecondsTimer ? ETimeFormat::DAYS_CENTISECS : ETimeFormat::DAYS, RenderInfo.m_aText, sizeof(RenderInfo.m_aText));
	RenderInfo.m_TextWidth = GetStableGameTimerWidth(pTextRender, FontSize, TimeSeconds, g_Config.m_PcShowMilliSecondsTimer != 0);
	RenderInfo.m_TextColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

	if(pGameInfo->m_TimeLimit && TimeSeconds <= 60.0f && pGameInfo->m_WarmupTimer <= 0)
	{
		const float Alpha = TimeSeconds <= 10.0f && (2 * time_get() / time_freq()) % 2 ? 0.5f : 1.0f;
		RenderInfo.m_TextColor = ColorRGBA(1.0f, 0.25f, 0.25f, Alpha);
	}

	return true;
}

float CMusicIsland::GetScrollingTextOffset(float Overflow, float Seconds)
{
	if(Overflow <= 0.0f)
		return 0.0f;

	const float PauseSeconds = 0.75f;
	const float ScrollSpeed = 16.0f;
	const float TravelSeconds = Overflow / ScrollSpeed;
	if(TravelSeconds <= 0.0f)
		return 0.0f;

	const float CycleSeconds = PauseSeconds + TravelSeconds + PauseSeconds + TravelSeconds;
	float PhaseSeconds = std::fmod(Seconds, CycleSeconds);

	if(PhaseSeconds < PauseSeconds)
		return 0.0f;
	PhaseSeconds -= PauseSeconds;

	if(PhaseSeconds < TravelSeconds)
		return PhaseSeconds * ScrollSpeed;
	PhaseSeconds -= TravelSeconds;

	if(PhaseSeconds < PauseSeconds)
		return Overflow;
	PhaseSeconds -= PauseSeconds;

	return maximum(0.0f, Overflow - PhaseSeconds * ScrollSpeed);
}

static float SnapToScreenPixel(float Value, float ScreenStart, float PixelSize)
{
	if(PixelSize <= 0.0f)
		return Value;

	return ScreenStart + round_to_int((Value - ScreenStart) / PixelSize) * PixelSize;
}

static float SnapToScreenSpan(float Value, float PixelSize)
{
	if(PixelSize <= 0.0f)
		return Value;

	return maximum(PixelSize, round_to_int(Value / PixelSize) * PixelSize);
}

void CMusicIsland::RenderCenteredClippedText(IGraphics *pGraphics, ITextRender *pTextRender, CUi *pUi, const CUIRect &Rect, const char *pText, float FontSize, const ColorRGBA &Color, float ScrollSeconds)
{
	if(Rect.w <= 0.0f || Rect.h <= 0.0f || pText == nullptr || pText[0] == '\0')
		return;

	const float TextWidth = pTextRender->TextWidth(FontSize, pText, -1, -1.0f);
	const bool ShouldScroll = TextWidth > Rect.w;
	float TextX = Rect.x + (Rect.w - TextWidth) / 2.0f;
	if(ShouldScroll)
		TextX = Rect.x - GetScrollingTextOffset(TextWidth - Rect.w, ScrollSeconds);
	float TextY = Rect.y + (Rect.h - FontSize) / 2.0f;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	pGraphics->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	const float ScreenWidth = ScreenX1 - ScreenX0;
	const float ScreenHeight = ScreenY1 - ScreenY0;
	const float PixelSizeX = ScreenWidth / pGraphics->ScreenWidth();
	const float PixelSizeY = ScreenHeight / pGraphics->ScreenHeight();
	TextY = SnapToScreenPixel(TextY, ScreenY0, PixelSizeY);

	if(!ShouldScroll)
	{
		TextX = SnapToScreenPixel(TextX, ScreenX0, PixelSizeX);
		const float MaxTextX = maximum(Rect.x, Rect.x + Rect.w - TextWidth);
		TextX = std::clamp(TextX, Rect.x, MaxTextX);
		pTextRender->TextColor(Color);
		pTextRender->Text(TextX, TextY, FontSize, pText, -1.0f);
		pTextRender->TextColor(pTextRender->DefaultTextColor());
		return;
	}

	const CUIRect UiScreen = *pUi->Screen();
	const CUIRect UiRect = HudToUiRect(Rect, UiScreen, ScreenWidth, ScreenHeight);
	const float UiTextX = HudToUiX(TextX, UiScreen, ScreenX0, ScreenWidth);
	const float UiTextY = HudToUiY(TextY, UiScreen, ScreenY0, ScreenHeight);
	const float UiFontSize = FontSize * UiScreen.h / maximum(ScreenHeight, 1.0f);

	pUi->MapScreen();
	pUi->ClipEnable(&UiRect);
	pTextRender->TextColor(Color);
	pTextRender->Text(UiTextX, UiTextY, UiFontSize, pText, -1.0f);
	pTextRender->TextColor(pTextRender->DefaultTextColor());
	pUi->ClipDisable();
	pGraphics->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
}

static float GetVisualizerBarPulse(float Time, int LayerIndex)
{
	const float Phase = 0.91f * (LayerIndex + 1);
	const float Slow = 0.5f + 0.5f * std::sin(Time * (4.8f + LayerIndex * 0.35f) + Phase);
	const float Mid = 0.5f + 0.5f * std::sin(Time * (9.4f + LayerIndex * 0.8f) + Phase * 1.8f + 0.35f);
	const float Fast = 0.5f + 0.5f * std::sin(Time * (15.5f + LayerIndex * 1.15f) + Phase * 2.6f + 1.1f);
	return maximum(0.0f, minimum(1.0f, Slow * 0.5f + Mid * 0.34f + Fast * 0.16f));
}

static bool HasMusicIslandPlatformBackend()
{
#if defined(CONF_MUSIC_ISLAND_WINRT)
	return true;
#elif defined(CONF_PLATFORM_LINUX) && defined(CONF_MUSIC_ISLAND_MPRIS)
	return true;
#else
	return false;
#endif
}

#if defined(CONF_PLATFORM_LINUX) && defined(CONF_MUSIC_ISLAND_MPRIS)
constexpr const char *gs_pMprisPlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr const char *gs_pMprisObjectPath = "/org/mpris/MediaPlayer2";
constexpr const char *gs_pDbusService = "org.freedesktop.DBus";
constexpr const char *gs_pDbusPath = "/org/freedesktop/DBus";
constexpr const char *gs_pDbusInterface = "org.freedesktop.DBus";
constexpr const char *gs_pDbusPropertiesInterface = "org.freedesktop.DBus.Properties";
constexpr const char *gs_pMprisBusPrefix = "org.mpris.MediaPlayer2.";

struct SMprisPlayerState
{
	bool m_Playing = false;
	bool m_CanPlay = false;
	bool m_CanPause = false;
	bool m_CanGoPrevious = false;
	bool m_CanGoNext = false;
	std::string m_BusName;
	std::string m_Title;
	std::string m_Artist;
	std::string m_Album;
};

static void LogMprisUnavailable(const char *pReason)
{
	if(!MusicIslandDebugEnabled())
		return;

	static std::string s_LastReason;
	static int64_t s_LastLogTime = 0;

	const int64_t Now = time_get();
	if(s_LastReason == pReason && Now - s_LastLogTime < time_freq() * 5)
		return;

	s_LastReason = pReason;
	s_LastLogTime = Now;
	dbg_msg("music-island-mpris", "%s", pReason);
}

static void LogMprisSelection(const SMprisPlayerState &State, bool FallbackSelection)
{
	if(!MusicIslandDebugEnabled())
		return;

	static std::string s_LastSummary;
	static int64_t s_LastLogTime = 0;

	const int64_t Now = time_get();
	std::string Summary = "Selected ";
	Summary += FallbackSelection ? "fallback" : "active";
	Summary += " player: bus=";
	Summary += State.m_BusName;
	Summary += ", playing=";
	Summary += State.m_Playing ? "1" : "0";
	Summary += ", title=\"";
	Summary += State.m_Title;
	Summary += "\", artist=\"";
	Summary += State.m_Artist;
	Summary += "\"";

	if(s_LastSummary == Summary && Now - s_LastLogTime < time_freq() * 5)
		return;

	s_LastSummary = std::move(Summary);
	s_LastLogTime = Now;
	dbg_msg("music-island-mpris", "%s", s_LastSummary.c_str());
}

static GVariant *MprisCallSync(GDBusConnection *pConnection, const char *pBusName, const char *pInterface, const char *pMethod, GVariant *pParameters, const GVariantType *pReplyType)
{
	GError *pError = nullptr;
	GVariant *pResult = g_dbus_connection_call_sync(
		pConnection,
		pBusName,
		gs_pMprisObjectPath,
		pInterface,
		pMethod,
		pParameters,
		pReplyType,
		G_DBUS_CALL_FLAGS_NONE,
		1000,
		nullptr,
		&pError);
	if(pError != nullptr)
	{
		if(MusicIslandDebugEnabled())
		{
			dbg_msg("music-island-mpris", "D-Bus call failed: bus=%s interface=%s method=%s error=%s",
				pBusName != nullptr ? pBusName : "<null>",
				pInterface != nullptr ? pInterface : "<null>",
				pMethod != nullptr ? pMethod : "<null>",
				pError->message != nullptr ? pError->message : "<null>");
		}
		g_error_free(pError);
		return nullptr;
	}
	return pResult;
}

static std::vector<std::string> GetMprisBusNames(GDBusConnection *pConnection)
{
	std::vector<std::string> vNames;

	GError *pError = nullptr;
	GVariant *pResult = g_dbus_connection_call_sync(
		pConnection,
		gs_pDbusService,
		gs_pDbusPath,
		gs_pDbusInterface,
		"ListNames",
		nullptr,
		G_VARIANT_TYPE("(as)"),
		G_DBUS_CALL_FLAGS_NONE,
		1000,
		nullptr,
		&pError);
	if(pError != nullptr)
	{
		if(MusicIslandDebugEnabled())
			dbg_msg("music-island-mpris", "Failed to enumerate D-Bus names: %s", pError->message != nullptr ? pError->message : "<null>");
		g_error_free(pError);
		return vNames;
	}
	if(pResult == nullptr)
		return vNames;

	GVariant *pNames = nullptr;
	g_variant_get(pResult, "(@as)", &pNames);
	GVariantIter Iter;
	const char *pName = nullptr;
	g_variant_iter_init(&Iter, pNames);
	while(g_variant_iter_next(&Iter, "&s", &pName))
	{
		if(std::strncmp(pName, gs_pMprisBusPrefix, std::strlen(gs_pMprisBusPrefix)) == 0)
			vNames.emplace_back(pName);
	}

	g_variant_unref(pNames);
	g_variant_unref(pResult);
	return vNames;
}

static void ParseMprisMetadata(GVariant *pMetadata, SMprisPlayerState &State)
{
	GVariantIter Iter;
	const char *pKey = nullptr;
	GVariant *pValue = nullptr;
	g_variant_iter_init(&Iter, pMetadata);
	while(g_variant_iter_next(&Iter, "{&sv}", &pKey, &pValue))
	{
		if(std::strcmp(pKey, "xesam:title") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE_STRING))
		{
			State.m_Title = g_variant_get_string(pValue, nullptr);
		}
		else if(std::strcmp(pKey, "xesam:album") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE_STRING))
		{
			State.m_Album = g_variant_get_string(pValue, nullptr);
		}
		else if(std::strcmp(pKey, "xesam:artist") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE("as")))
		{
			gsize NumArtists = 0;
			const gchar **ppArtists = g_variant_get_strv(pValue, &NumArtists);
			if(NumArtists > 0 && ppArtists[0] != nullptr)
				State.m_Artist = ppArtists[0];
			g_free((gpointer)ppArtists);
		}
		g_variant_unref(pValue);
	}
}

static bool QueryMprisPlayer(GDBusConnection *pConnection, const char *pBusName, SMprisPlayerState &State)
{
	GVariant *pResult = MprisCallSync(
		pConnection,
		pBusName,
		gs_pDbusPropertiesInterface,
		"GetAll",
		g_variant_new("(s)", gs_pMprisPlayerInterface),
		G_VARIANT_TYPE("(a{sv})"));
	if(pResult == nullptr)
		return false;

	GVariant *pProperties = nullptr;
	g_variant_get(pResult, "(@a{sv})", &pProperties);
	GVariantIter Iter;
	const char *pKey = nullptr;
	GVariant *pValue = nullptr;
	g_variant_iter_init(&Iter, pProperties);
	while(g_variant_iter_next(&Iter, "{&sv}", &pKey, &pValue))
	{
		if(std::strcmp(pKey, "PlaybackStatus") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE_STRING))
		{
			State.m_Playing = std::strcmp(g_variant_get_string(pValue, nullptr), "Playing") == 0;
		}
		else if(std::strcmp(pKey, "CanPlay") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE_BOOLEAN))
		{
			State.m_CanPlay = g_variant_get_boolean(pValue);
		}
		else if(std::strcmp(pKey, "CanPause") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE_BOOLEAN))
		{
			State.m_CanPause = g_variant_get_boolean(pValue);
		}
		else if(std::strcmp(pKey, "CanGoPrevious") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE_BOOLEAN))
		{
			State.m_CanGoPrevious = g_variant_get_boolean(pValue);
		}
		else if(std::strcmp(pKey, "CanGoNext") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE_BOOLEAN))
		{
			State.m_CanGoNext = g_variant_get_boolean(pValue);
		}
		else if(std::strcmp(pKey, "Metadata") == 0 && g_variant_is_of_type(pValue, G_VARIANT_TYPE_VARDICT))
		{
			ParseMprisMetadata(pValue, State);
		}
		g_variant_unref(pValue);
	}

	State.m_BusName = pBusName;
	g_variant_unref(pProperties);
	g_variant_unref(pResult);
	return true;
}

static bool QueryBestMprisPlayer(SMprisPlayerState &State)
{
	GError *pError = nullptr;
	GDBusConnection *pConnection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &pError);
	if(pError != nullptr)
	{
		if(MusicIslandDebugEnabled())
			dbg_msg("music-island-mpris", "Failed to connect to session D-Bus: %s", pError->message != nullptr ? pError->message : "<null>");
		g_error_free(pError);
		return false;
	}
	if(pConnection == nullptr)
	{
		LogMprisUnavailable("Failed to connect to session D-Bus: connection is null");
		return false;
	}

	bool FoundPlayer = false;
	SMprisPlayerState FallbackState;
	const std::vector<std::string> vBusNames = GetMprisBusNames(pConnection);
	if(vBusNames.empty())
	{
		LogMprisUnavailable("No MPRIS services found on session D-Bus");
		g_object_unref(pConnection);
		return false;
	}

	for(const std::string &BusName : vBusNames)
	{
		SMprisPlayerState CandidateState;
		if(!QueryMprisPlayer(pConnection, BusName.c_str(), CandidateState))
			continue;

		if(!FoundPlayer)
		{
			FallbackState = CandidateState;
			FoundPlayer = true;
		}

		if(CandidateState.m_Playing)
		{
			State = std::move(CandidateState);
			LogMprisSelection(State, false);
			g_object_unref(pConnection);
			return true;
		}
	}

	g_object_unref(pConnection);
	if(!FoundPlayer)
	{
		LogMprisUnavailable("Found MPRIS services, but none returned valid player properties");
		return false;
	}

	State = std::move(FallbackState);
	LogMprisSelection(State, true);
	return true;
}

static void TriggerMprisControlAction(const char *pMethod)
{
	if(pMethod == nullptr)
		return;

	SMprisPlayerState PlayerState;
	if(!QueryBestMprisPlayer(PlayerState) || PlayerState.m_BusName.empty())
	{
		LogMusicIslandDebug("music-island-mpris", "No active MPRIS player for control command");
		return;
	}

	GError *pError = nullptr;
	GDBusConnection *pConnection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &pError);
	if(pError != nullptr)
	{
		if(MusicIslandDebugEnabled())
			dbg_msg("music-island-mpris", "Failed to connect to session D-Bus for control command: %s", pError->message != nullptr ? pError->message : "<null>");
		g_error_free(pError);
		return;
	}
	if(pConnection == nullptr)
	{
		LogMusicIslandDebug("music-island-mpris", "Failed to connect to session D-Bus for control command: connection is null");
		return;
	}

	GVariant *pResult = MprisCallSync(pConnection, PlayerState.m_BusName.c_str(), gs_pMprisPlayerInterface, pMethod, nullptr, nullptr);
	if(pResult != nullptr)
	{
		LogMusicIslandDebug("music-island-mpris", "Sent media control command");
		g_variant_unref(pResult);
	}
	else
	{
		LogMusicIslandDebug("music-island-mpris", "Failed to send media control command");
	}
	g_object_unref(pConnection);
}
#endif

#if defined(CONF_MUSIC_ISLAND_WINRT)
static std::string MakeArtworkKey(const std::string &Title, const std::string &Artist, const std::string &Album)
{
	std::string Key = Title;
	Key.push_back('\x1f');
	Key += Artist;
	Key.push_back('\x1f');
	Key += Album;
	return Key;
}

static winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager &CachedMusicIslandSessionManager()
{
	using namespace winrt::Windows::Media::Control;
	static thread_local GlobalSystemMediaTransportControlsSessionManager s_SessionManager = nullptr;
	return s_SessionManager;
}

static void ApplyRoundedCornersToImage(CImageInfo &Image, float RadiusFraction)
{
	if(Image.m_pData == nullptr || Image.m_Format != CImageInfo::FORMAT_RGBA || Image.m_Width == 0 || Image.m_Height == 0)
		return;

	const float Radius = minimum((float)Image.m_Width, (float)Image.m_Height) * RadiusFraction;
	const float ClampedRadius = minimum(Radius, minimum((float)Image.m_Width, (float)Image.m_Height) / 2.0f);
	if(ClampedRadius <= 0.0f)
		return;

	const float Left = ClampedRadius;
	const float Right = (float)Image.m_Width - ClampedRadius;
	const float Top = ClampedRadius;
	const float Bottom = (float)Image.m_Height - ClampedRadius;

	for(size_t y = 0; y < Image.m_Height; ++y)
	{
		const float PixelY = (float)y + 0.5f;
		for(size_t x = 0; x < Image.m_Width; ++x)
		{
			const float PixelX = (float)x + 0.5f;
			float DistanceToCorner = -1.0f;

			if(PixelX < Left && PixelY < Top)
				DistanceToCorner = std::hypot(PixelX - Left, PixelY - Top);
			else if(PixelX > Right && PixelY < Top)
				DistanceToCorner = std::hypot(PixelX - Right, PixelY - Top);
			else if(PixelX < Left && PixelY > Bottom)
				DistanceToCorner = std::hypot(PixelX - Left, PixelY - Bottom);
			else if(PixelX > Right && PixelY > Bottom)
				DistanceToCorner = std::hypot(PixelX - Right, PixelY - Bottom);

			if(DistanceToCorner < 0.0f)
				continue;

			const float Coverage = std::clamp(ClampedRadius + 0.5f - DistanceToCorner, 0.0f, 1.0f);
			if(Coverage >= 1.0f)
				continue;

			const size_t AlphaIndex = (y * Image.m_Width + x) * 4 + 3;
			Image.m_pData[AlphaIndex] = (uint8_t)std::round(Image.m_pData[AlphaIndex] * Coverage);
		}
	}
}

static bool DecodeThumbnailToImage(const winrt::Windows::Storage::Streams::IRandomAccessStreamReference &Thumbnail, CImageInfo &Image)
{
	using namespace winrt::Windows::Graphics::Imaging;

	if(!Thumbnail)
		return false;

	const auto Stream = Thumbnail.OpenReadAsync().get();
	if(!Stream || Stream.Size() == 0)
		return false;

	const auto Decoder = BitmapDecoder::CreateAsync(Stream).get();
	if(!Decoder)
		return false;

	constexpr uint32_t MaxArtworkSize = 64;
	const uint32_t Width = Decoder.PixelWidth();
	const uint32_t Height = Decoder.PixelHeight();
	if(Width == 0 || Height == 0)
		return false;

	uint32_t TargetWidth = Width;
	uint32_t TargetHeight = Height;
	BitmapTransform Transform;
	if(Width > MaxArtworkSize || Height > MaxArtworkSize)
	{
		if(Width >= Height)
		{
			TargetWidth = MaxArtworkSize;
			TargetHeight = maximum<uint32_t>(1, Height * MaxArtworkSize / Width);
		}
		else
		{
			TargetWidth = maximum<uint32_t>(1, Width * MaxArtworkSize / Height);
			TargetHeight = MaxArtworkSize;
		}
		Transform.ScaledWidth(TargetWidth);
		Transform.ScaledHeight(TargetHeight);
	}

	const auto PixelData = Decoder.GetPixelDataAsync(
		BitmapPixelFormat::Rgba8,
		BitmapAlphaMode::Straight,
		Transform,
		ExifOrientationMode::IgnoreExifOrientation,
		ColorManagementMode::DoNotColorManage)
							   .get();

	const winrt::com_array<uint8_t> Pixels = PixelData.DetachPixelData();
	const size_t ExpectedSize = (size_t)TargetWidth * TargetHeight * 4;
	if(Pixels.size() < ExpectedSize)
		return false;

	Image.Free();
	Image.m_Width = TargetWidth;
	Image.m_Height = TargetHeight;
	Image.m_Format = CImageInfo::FORMAT_RGBA;
	Image.m_pData = static_cast<uint8_t *>(malloc(ExpectedSize));
	if(Image.m_pData == nullptr)
	{
		Image.m_Width = 0;
		Image.m_Height = 0;
		Image.m_Format = CImageInfo::FORMAT_UNDEFINED;
		return false;
	}

	std::memcpy(Image.m_pData, Pixels.data(), ExpectedSize);
	ApplyRoundedCornersToImage(Image, SMusicIslandProperties::ms_ArtworkRadiusFraction);
	return true;
}

static void LogWindowsSelection(bool Playing, bool CanPlay, bool CanPause, const std::string &Title, const std::string &Artist, bool HasThumbnail)
{
	if(!MusicIslandDebugEnabled())
		return;

	static std::string s_LastSummary;
	static int64_t s_LastLogTime = 0;

	const int64_t Now = time_get();
	std::string Summary = "Selected session: playing=";
	Summary += Playing ? "1" : "0";
	Summary += ", canPlay=";
	Summary += CanPlay ? "1" : "0";
	Summary += ", canPause=";
	Summary += CanPause ? "1" : "0";
	Summary += ", title=\"";
	Summary += Title;
	Summary += "\", artist=\"";
	Summary += Artist;
	Summary += "\", thumbnail=";
	Summary += HasThumbnail ? "1" : "0";

	if(s_LastSummary == Summary && Now - s_LastLogTime < time_freq() * 5)
		return;

	s_LastSummary = std::move(Summary);
	s_LastLogTime = Now;
	dbg_msg("music-island-win", "%s", s_LastSummary.c_str());
}
#endif

CMusicIsland::CMusicIsland()
{
	OnReset();
}

CMusicIsland::~CMusicIsland()
{
	StopInfoWorker();
	StopImageWorker();
	ResetMusicImage();
}

void CMusicIsland::ResetMusicInfo()
{
	std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
	m_MusicInfo = {};
}

void CMusicIsland::ResetMusicImage()
{
	std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
	m_CurrentArtworkKey.clear();
	m_LastDetectedArtworkKey.clear();
	m_LastDetectedArtworkChangeTime = 0;
	m_MusicImageDirty = false;
	m_PendingMusicImage.Free();

	if(m_MusicImageTexture.IsValid())
		Graphics()->UnloadTexture(&m_MusicImageTexture);
	m_MusicImageWidth = 0;
	m_MusicImageHeight = 0;
}

void CMusicIsland::ResetVisualState()
{
	m_Extended = false;
	m_ExtendAnim = 0.0f;
	m_LastNativeMousePressed = false;
	m_Rect = {};
}

void CMusicIsland::ResetRuntimeState()
{
	StopInfoWorker();
	StopImageWorker();
	ResetVisualState();
	m_NextInfoUpdateTime = 0;
	ResetMusicInfo();
	ResetMusicImage();
}

CMusicIsland::SMusicInfo CMusicIsland::GetMusicInfo() const
{
	std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
	return m_MusicInfo;
}

void CMusicIsland::OnConsoleInit()
{
	Console()->Register("pc_show_cur_music_info", "", CFGFLAG_CLIENT, ConShowCurMusicInfo, this, "Print current music info");
}

void CMusicIsland::SetExtended(bool Extended)
{
	m_Extended = Extended;
}

void CMusicIsland::OnReset()
{
	ResetVisualState();
}

void CMusicIsland::OnShutdown()
{
	ResetRuntimeState();
}

bool CMusicIsland::OnInput(const IInput::CEvent &Event)
{
	if(!IsActive())
		return false;
	if(!CanRenderMusicIslandForClientState(Client()))
		return false;

	if(!CanUseMouseInteraction())
		return false;

	if(Event.m_Key != KEY_MOUSE_1 || (Event.m_Flags & (IInput::FLAG_PRESS | IInput::FLAG_RELEASE)) == 0)
		return false;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	return m_Rect.Inside(MouseInteractionPos(vec2(ScreenX0, ScreenY0), vec2(ScreenX1, ScreenY1)));
}

bool CMusicIsland::CanUseMouseInteraction() const
{
	return GameClient()->m_Chat.HasMouseCursor() ||
		GameClient()->m_Scoreboard.HasMouseCursor() ||
		GameClient()->m_RClientClickGui.HasMouseCursor();
}

vec2 CMusicIsland::MouseInteractionPos(vec2 ScreenTL, vec2 ScreenBR) const
{
	if(GameClient()->m_Chat.HasMouseCursor())
		return UiMouseToScreen(Ui()->Screen(), GameClient()->m_Chat.MouseCursorPos(), ScreenTL, ScreenBR);
	if(GameClient()->m_Scoreboard.HasMouseCursor())
		return UiMouseToScreen(Ui()->Screen(), GameClient()->m_Scoreboard.MouseCursorPos(), ScreenTL, ScreenBR);
	if(GameClient()->m_RClientClickGui.HasMouseCursor())
		return UiMouseToScreen(Ui()->Screen(), GameClient()->m_RClientClickGui.MouseCursorPos(), ScreenTL, ScreenBR);
	return vec2(-1.0f, -1.0f);
}

void CMusicIsland::OnRender()
{
}

void CMusicIsland::RenderHud()
{
	if(!IsActive())
	{
		ResetRuntimeState();
		return;
	}
	if(!CanRenderMusicIslandForClientState(Client()))
	{
		ResetVisualState();
		return;
	}

	if(HasMusicIslandPlatformBackend())
		StartInfoWorker();

	if(g_Config.m_PcShowMusicIslandImage)
		UpdateMusicImageTexture();
	else
	{
		StopImageWorker();
		ResetMusicImage();
	}

	RenderMusicIsland();
}

void CMusicIsland::RenderMusicIsland()
{
	CUIRect WindowRect;

	vec2 ScreenTL, ScreenBR;
	Graphics()->GetScreen(&ScreenTL.x, &ScreenTL.y, &ScreenBR.x, &ScreenBR.y);
	const float PixelSizeX = (ScreenBR.x - ScreenTL.x) / Graphics()->ScreenWidth();
	const float PixelSizeY = (ScreenBR.y - ScreenTL.y) / Graphics()->ScreenHeight();

	WindowRect.h = SMusicIslandProperties::ms_BaseHeight;
	constexpr float BaseFontSize = 8.0f;
	const float TimerFontSize = minimum(BaseFontSize, SMusicIslandProperties::ms_BaseHeight - SMusicIslandProperties::ms_Padding * 2.0f);
	float DesiredWidth = 0.0f;
	SGameTimerRenderInfo RenderInfo;
	if(GetGameTimerRenderInfo(GameClient()->m_Snap.m_pGameInfoObj, Client(), TextRender(), TimerFontSize, RenderInfo))
	{
		DesiredWidth = RenderInfo.m_TextWidth + 10.0f;
		if(g_Config.m_PcShowMusicIslandImage)
			DesiredWidth += 8.0f;
		if(g_Config.m_PcShowMusicIslandVisualizer)
			DesiredWidth += 8.0f;
	}

	if(g_Config.m_PcShowMusicIslandTimerFull)
	{
		const float MinWidth = (float)g_Config.m_PcShowMusicIslandFullMinWidth;
		WindowRect.w = g_Config.m_PcShowMusicIslandDynamicWidth && DesiredWidth > 0.0f ?
			maximum(MinWidth, DesiredWidth) :
			MinWidth;
	}
	else
	{
		const float MinWidth = (float)g_Config.m_PcShowMusicIslandMinWidth;
		const float MaxWidth = maximum((float)g_Config.m_PcShowMusicIslandMinWidth, (float)g_Config.m_PcShowMusicIslandMaxWidth);
		WindowRect.w = g_Config.m_PcShowMusicIslandDynamicWidth && DesiredWidth > 0.0f ?
			maximum(MinWidth, minimum(MaxWidth, DesiredWidth)) :
			MaxWidth;
	}

	WindowRect.w = SnapToScreenSpan(WindowRect.w, PixelSizeX);
	WindowRect.h = SnapToScreenSpan(WindowRect.h, PixelSizeY);
	WindowRect.w = minimum(WindowRect.w, ScreenBR.x - ScreenTL.x);
	WindowRect.x = SnapToScreenPixel(ScreenTL.x + (ScreenBR.x - ScreenTL.x - WindowRect.w) / 2.0f, ScreenTL.x, PixelSizeX);
	WindowRect.y = SnapToScreenPixel(ScreenTL.y + 2.5f, ScreenTL.y, PixelSizeY);
	WindowRect.x = std::clamp(WindowRect.x, ScreenTL.x, maximum(ScreenTL.x, ScreenBR.x - WindowRect.w));
	WindowRect.y = std::clamp(WindowRect.y, ScreenTL.y, maximum(ScreenTL.y, ScreenBR.y - WindowRect.h));

	const float MainExtraHeight = SMusicIslandProperties::ms_ExpandedInfoHeight;
	const float ControlsExtraHeight = SMusicIslandProperties::ms_ControlGap + SMusicIslandProperties::ms_ControlHeight;
	const float ExpandedExtraHeight = MainExtraHeight + ControlsExtraHeight;
	const bool UseMouse = CanUseMouseInteraction();
	const vec2 MousePos = UseMouse ? MouseInteractionPos(ScreenTL, ScreenBR) : vec2(-1.0f, -1.0f);
	const bool MousePressed = UseMouse && Input()->KeyIsPressed(KEY_MOUSE_1);
	const bool MouseClicked = UseMouse && MousePressed && !m_LastNativeMousePressed;
	const bool ShowGaps = g_Config.m_PcShowMusicIslandSections;
	const bool ShowImage = g_Config.m_PcShowMusicIslandImage != 0;
	const bool ShowVisualizer = g_Config.m_PcShowMusicIslandVisualizer != 0;

	CUIRect BaseHoverRect = WindowRect;
	CUIRect ExpandedHoverRect = WindowRect;
	ExpandedHoverRect.h += ExpandedExtraHeight;

	const bool Hovered = UseMouse && (m_ExtendAnim > 0.0f ? ExpandedHoverRect : BaseHoverRect).Inside(MousePos);
	m_Extended = Hovered;

	const float TargetAnim = Hovered ? 1.0f : 0.0f;
	const float AnimStep = Client()->RenderFrameTime() * 10.0f;
	if(m_ExtendAnim < TargetAnim)
		m_ExtendAnim = minimum(TargetAnim, m_ExtendAnim + AnimStep);
	else if(m_ExtendAnim > TargetAnim)
		m_ExtendAnim = maximum(TargetAnim, m_ExtendAnim - AnimStep);

	const float SmoothAnim = m_ExtendAnim * m_ExtendAnim * (3.0f - 2.0f * m_ExtendAnim);
	WindowRect.h += ExpandedExtraHeight * SmoothAnim;

	m_Rect = WindowRect;
	WindowRect.Draw(MusicIslandWindowColor(), IGraphics::CORNER_ALL, SMusicIslandProperties::ms_BaseHeight / 2.0f);

	CUIRect HeaderRect = WindowRect;
	CUIRect ControlsRect, GapsRect;
	if(SmoothAnim > 0.0f)
	{
		HeaderRect.HSplitTop(SMusicIslandProperties::ms_BaseHeight + MainExtraHeight * SmoothAnim, &HeaderRect, &ControlsRect);
		const float AnimatedGap = SMusicIslandProperties::ms_ControlGap * SmoothAnim;
		if(AnimatedGap > 0.0f)
		{
			if(ShowGaps)
			{
				ControlsRect.HSplitTop(AnimatedGap, &GapsRect, &ControlsRect);
				GapsRect.VMargin(5.0f, &GapsRect);
				GapsRect.HSplitTop(0.75f, &GapsRect, nullptr);
				GapsRect.Draw(MusicIslandGapsColor(), IGraphics::CORNER_NONE, 0.0f);
			}
			else
				ControlsRect.HSplitTop(AnimatedGap, nullptr, &ControlsRect);
		}
	}
	CUIRect Base = HeaderRect;
	CUIRect MusicImage, Visualizer;

	Base.VMargin(3.0f, &Base);
	Base.HMargin(SMusicIslandProperties::ms_Padding, &Base);
	if(ShowImage)
	{
		Base.VSplitLeft(8.0f, &MusicImage, &Base);
		MusicImage.HMargin(SMusicIslandProperties::ms_Padding, &MusicImage);
	}
	if(ShowVisualizer)
	{
		Base.VSplitRight(8.0f, &Base, &Visualizer);
	}
	CUIRect LeftSpacing, RightSpacing;
	Base.VSplitLeft(1.5f, &LeftSpacing, &Base);
	Base.VSplitRight(1.5f, &Base, &RightSpacing);
	if(ShowGaps && ShowImage)
	{
		GapsRect = LeftSpacing;
		GapsRect.VSplitLeft(0.75f, nullptr, &GapsRect);
		GapsRect.VSplitLeft(0.5f, &GapsRect, nullptr);
		GapsRect.Draw(MusicIslandGapsColor(), IGraphics::CORNER_NONE, 0.0f);
	}
	if(ShowGaps && ShowVisualizer)
	{
		GapsRect = RightSpacing;
		GapsRect.VSplitLeft(0.25f, nullptr, &GapsRect);
		GapsRect.VSplitLeft(0.5f, &GapsRect, nullptr);
		GapsRect.Draw(MusicIslandGapsColor(), IGraphics::CORNER_NONE, 0.0f);
	}
	const SMusicInfo MusicInfo = GetMusicInfo();
	if(ShowImage)
		RenderMusicIslandImage(&MusicImage);
	if(ShowVisualizer)
		RenderMusicIslandVisualizer(&Visualizer);
	RenderMusicIslandMain(&Base);

	if(SmoothAnim > 0.0f)
		RenderMusicIslandControls(&ControlsRect, MusicInfo, MousePos, MouseClicked, MousePressed, SmoothAnim);

	m_LastNativeMousePressed = MousePressed;
}

bool CMusicIsland::DoControlButton(const CUIRect *pRect, const char *pIcon, bool Enabled, vec2 MousePos, bool MouseClicked, bool MousePressed, float AnimProgress)
{
	const bool Hovered = Enabled && pRect->Inside(MousePos);
	const bool Active = Hovered && MousePressed;
	const float BaseAlpha = !Enabled ? 0.3f : (Active ? 1.0f : (Hovered ? 0.95f : 0.75f));
	const float IconAlpha = BaseAlpha * AnimProgress;

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH |
		ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING |
		ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor().WithAlpha(IconAlpha));
	TextRender()->TextColor(TextRender()->DefaultTextColor().WithAlpha(IconAlpha));

	const float IconSize = pRect->h * 0.68f;
	const float IconWidth = TextRender()->TextWidth(IconSize, pIcon, -1, -1.0f);
	const float IconX = pRect->x + (pRect->w - IconWidth) / 2.0f;
	const float IconY = pRect->y + (pRect->h - IconSize) / 2.0f;
	TextRender()->Text(IconX, IconY, IconSize, pIcon, -1.0f);

	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	return Enabled && Hovered && MouseClicked;
}

void CMusicIsland::RenderMusicIslandControls(CUIRect *pBase, const SMusicInfo &MusicInfo, vec2 MousePos, bool MouseClicked, bool MousePressed, float AnimProgress)
{
	CUIRect Controls = *pBase;
	Controls.Margin(vec2(14.0f, 0.7f), &Controls);
	if(Controls.w <= 0.0f || Controls.h <= 0.0f)
		return;

	const float Gap = 2.0f;
	const float ButtonWidth = (Controls.w - Gap * 2.0f) / 3.0f;
	if(ButtonWidth <= 0.0f)
		return;

	CUIRect PreviousButton;
	CUIRect PlayPauseButton;
	CUIRect NextButton;

	Controls.VSplitLeft(ButtonWidth, &PreviousButton, &Controls);
	Controls.VSplitLeft(Gap, nullptr, &Controls);
	Controls.VSplitLeft(ButtonWidth, &PlayPauseButton, &Controls);
	Controls.VSplitLeft(Gap, nullptr, &Controls);
	NextButton = Controls;

	if(DoControlButton(&PreviousButton, FontIcon::BACKWARD_STEP, MusicInfo.m_CanGoPrevious, MousePos, MouseClicked, MousePressed, AnimProgress))
		TriggerControlAction(CONTROL_BUTTON_PREVIOUS);

	const char *pPlayPauseIcon = MusicInfo.m_Playing ? FontIcon::PAUSE : FontIcon::PLAY;
	const bool CanTogglePlayback = MusicInfo.m_Playing ? MusicInfo.m_CanPause : MusicInfo.m_CanPlay;
	if(DoControlButton(&PlayPauseButton, pPlayPauseIcon, CanTogglePlayback, MousePos, MouseClicked, MousePressed, AnimProgress))
		TriggerControlAction(CONTROL_BUTTON_PLAY_PAUSE);

	if(DoControlButton(&NextButton, FontIcon::FORWARD_STEP, MusicInfo.m_CanGoNext, MousePos, MouseClicked, MousePressed, AnimProgress))
		TriggerControlAction(CONTROL_BUTTON_NEXT);
}

void CMusicIsland::StartInfoWorker()
{
	if(m_InfoWorkerRunning.load())
		return;

	if(m_InfoWorker.joinable())
		m_InfoWorker.join();

	{
		std::lock_guard<std::mutex> Guard(m_InfoWorkerMutex);
		m_InfoWorkerStopRequested.store(false);
		m_NextInfoUpdateTime = 0;
	}
	m_InfoWorkerRunning.store(true);
	m_InfoWorker = std::thread(&CMusicIsland::InfoWorkerLoop, this);
}

void CMusicIsland::StopInfoWorker()
{
	m_InfoWorkerStopRequested.store(true);
	{
		std::lock_guard<std::mutex> Guard(m_InfoWorkerMutex);
		m_NextInfoUpdateTime = 0;
	}
	m_InfoWorkerCv.notify_all();
	if(m_InfoWorker.joinable())
		m_InfoWorker.join();

	m_InfoWorkerRunning.store(false);
}

void CMusicIsland::StopImageWorker()
{
#if defined(CONF_MUSIC_ISLAND_WINRT)
	m_ImageWorkerStopRequested.store(true);
	if(m_ImageWorker.joinable())
		m_ImageWorker.join();

	m_ImageWorkerRunning.store(false);
#endif
}

void CMusicIsland::InfoWorkerLoop()
{
#if defined(CONF_MUSIC_ISLAND_WINRT)
	winrt::init_apartment(winrt::apartment_type::multi_threaded);
#endif

	while(!m_InfoWorkerStopRequested.load())
	{
		int64_t NextUpdateTime = 0;
		const int64_t Now = time_get();
		{
			std::unique_lock<std::mutex> Lock(m_InfoWorkerMutex);
			if(m_NextInfoUpdateTime == 0)
				m_NextInfoUpdateTime = Now;
			NextUpdateTime = m_NextInfoUpdateTime;
			if(NextUpdateTime > Now)
			{
				const int64_t WaitMs = maximum<int64_t>(1, (NextUpdateTime - Now) * 1000 / time_freq());
				m_InfoWorkerCv.wait_for(Lock, std::chrono::milliseconds(WaitMs), [this]() {
					return m_InfoWorkerStopRequested.load();
				});
				continue;
			}

			m_NextInfoUpdateTime = Now + time_freq();
		}

		UpdateMusicInfo();
	}

#if defined(CONF_MUSIC_ISLAND_WINRT)
	CachedMusicIslandSessionManager() = nullptr;
	winrt::uninit_apartment();
#endif
	m_InfoWorkerRunning.store(false);
}

void CMusicIsland::UpdateMusicInfo()
{
#if defined(CONF_MUSIC_ISLAND_WINRT)
	SMusicInfo NewInfo;
	const int64_t Now = time_get();
	const int64_t ArtworkDebounceTicks = time_freq() * gs_MusicIslandArtworkDebounceMs / 1000;
	const bool WantArtwork = g_Config.m_PcShowMusicIslandImage != 0;
	std::string NewArtworkKey;
	bool UpdateArtwork = false;
	bool HasThumbnail = false;
	int64_t ArtworkRetryTime = 0;
	winrt::Windows::Storage::Streams::IRandomAccessStreamReference Thumbnail = nullptr;

	try
	{
		using namespace winrt::Windows::Media::Control;

		auto &SessionManagerCache = CachedMusicIslandSessionManager();
		if(!SessionManagerCache)
			SessionManagerCache = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

		const auto SessionManager = SessionManagerCache;
		const auto Session = SessionManager.GetCurrentSession();
		if(Session)
		{
			const auto PlaybackInfo = Session.GetPlaybackInfo();
			const auto MediaProperties = Session.TryGetMediaPropertiesAsync().get();
			const auto Controls = PlaybackInfo ? PlaybackInfo.Controls() : nullptr;

			NewInfo.m_Available = true;
			NewInfo.m_Playing = PlaybackInfo && PlaybackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
			NewInfo.m_CanPlay = Controls && Controls.IsPlayEnabled();
			NewInfo.m_CanPause = Controls && Controls.IsPauseEnabled();
			NewInfo.m_CanGoPrevious = Controls && Controls.IsPreviousEnabled();
			NewInfo.m_CanGoNext = Controls && Controls.IsNextEnabled();
			NewInfo.m_Title = winrt::to_string(MediaProperties.Title());
			NewInfo.m_Artist = winrt::to_string(MediaProperties.Artist());
			NewInfo.m_Album = winrt::to_string(MediaProperties.AlbumTitle());

			if(WantArtwork)
			{
				NewArtworkKey = MakeArtworkKey(NewInfo.m_Title, NewInfo.m_Artist, NewInfo.m_Album);

				{
					std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
					if(NewArtworkKey != m_LastDetectedArtworkKey)
					{
						m_LastDetectedArtworkKey = NewArtworkKey;
						m_LastDetectedArtworkChangeTime = Now;
					}

					UpdateArtwork = NewArtworkKey != m_CurrentArtworkKey &&
						(Now - m_LastDetectedArtworkChangeTime >= ArtworkDebounceTicks);
					if(!UpdateArtwork && NewArtworkKey != m_CurrentArtworkKey)
						ArtworkRetryTime = m_LastDetectedArtworkChangeTime + ArtworkDebounceTicks;
				}

				if(UpdateArtwork)
				{
					Thumbnail = MediaProperties.Thumbnail();
					HasThumbnail = Thumbnail != nullptr;
				}
			}

			LogWindowsSelection(NewInfo.m_Playing, NewInfo.m_CanPlay, NewInfo.m_CanPause, NewInfo.m_Title, NewInfo.m_Artist, HasThumbnail);
		}
		else
		{
			LogMusicIslandDebug("music-island-win", "No active media session");
			if(WantArtwork)
			{
				std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
				UpdateArtwork = !m_CurrentArtworkKey.empty();
			}
		}
	}
	catch(const winrt::hresult_error &Error)
	{
		CachedMusicIslandSessionManager() = nullptr;
		if(MusicIslandDebugEnabled())
			dbg_msg("music-island-win", "Failed to query media session: 0x%08x", (unsigned int)Error.code().value);
		NewInfo = {};
		if(WantArtwork)
		{
			std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
			UpdateArtwork = !m_CurrentArtworkKey.empty();
		}
	}
	catch(...)
	{
		CachedMusicIslandSessionManager() = nullptr;
		LogMusicIslandDebug("music-island-win", "Failed to query media session: unknown exception");
		NewInfo = {};
		if(WantArtwork)
		{
			std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
			UpdateArtwork = !m_CurrentArtworkKey.empty();
		}
	}

	if(m_InfoWorkerStopRequested.load())
		return;

	std::string ArtworkKey;
	{
		std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
		m_MusicInfo = std::move(NewInfo);
		if(UpdateArtwork)
		{
			m_CurrentArtworkKey = std::move(NewArtworkKey);
			ArtworkKey = m_CurrentArtworkKey;
		}
	}

	if(!UpdateArtwork)
	{
		if(ArtworkRetryTime != 0)
		{
			std::lock_guard<std::mutex> Guard(m_InfoWorkerMutex);
			if(m_NextInfoUpdateTime == 0 || ArtworkRetryTime < m_NextInfoUpdateTime)
				m_NextInfoUpdateTime = ArtworkRetryTime;
		}
		m_InfoWorkerCv.notify_one();
		return;
	}

	StopImageWorker();

	if(!HasThumbnail)
	{
		LogMusicIslandDebug("music-island-win", "No thumbnail for current media session");
		std::lock_guard<std::mutex> ImageGuard(m_MusicInfoMutex);
		m_MusicImageDirty = true;
		m_PendingMusicImage.Free();
		return;
	}

	const auto AgileThumbnail = winrt::agile_ref<winrt::Windows::Storage::Streams::IRandomAccessStreamReference>(Thumbnail);

	m_ImageWorkerStopRequested.store(false);
	m_ImageWorkerRunning.store(true);
	m_ImageWorker = std::thread([this, ArtworkKey, AgileThumbnail]() {
		winrt::init_apartment(winrt::apartment_type::multi_threaded);

		CImageInfo DecodedImage;
		try
		{
			const auto ThumbnailRef = AgileThumbnail.get();
			if(ThumbnailRef)
				DecodeThumbnailToImage(ThumbnailRef, DecodedImage);
		}
		catch(const winrt::hresult_error &Error)
		{
			if(MusicIslandDebugEnabled())
				dbg_msg("music-island-win", "Failed to decode thumbnail: 0x%08x", (unsigned int)Error.code().value);
			DecodedImage.Free();
		}
		catch(...)
		{
			LogMusicIslandDebug("music-island-win", "Failed to decode thumbnail: unknown exception");
			DecodedImage.Free();
		}

		if(!m_ImageWorkerStopRequested.load())
		{
			std::lock_guard<std::mutex> ImageGuard(m_MusicInfoMutex);
			if(m_CurrentArtworkKey == ArtworkKey)
			{
				m_MusicImageDirty = true;
				m_PendingMusicImage.Free();
				m_PendingMusicImage = std::move(DecodedImage);
			}
		}
		else
		{
			DecodedImage.Free();
		}

		winrt::uninit_apartment();
		m_ImageWorkerRunning.store(false);
	});
#elif defined(CONF_PLATFORM_LINUX) && defined(CONF_MUSIC_ISLAND_MPRIS)
	SMusicInfo NewInfo;
	SMprisPlayerState PlayerState;
	if(QueryBestMprisPlayer(PlayerState))
	{
		NewInfo.m_Available = true;
		NewInfo.m_Playing = PlayerState.m_Playing;
		NewInfo.m_CanPlay = PlayerState.m_CanPlay;
		NewInfo.m_CanPause = PlayerState.m_CanPause;
		NewInfo.m_CanGoPrevious = PlayerState.m_CanGoPrevious;
		NewInfo.m_CanGoNext = PlayerState.m_CanGoNext;
		NewInfo.m_Title = std::move(PlayerState.m_Title);
		NewInfo.m_Artist = std::move(PlayerState.m_Artist);
		NewInfo.m_Album = std::move(PlayerState.m_Album);
	}
	else
	{
		LogMusicIslandDebug("music-island-mpris", "No active MPRIS player");
	}

	if(m_InfoWorkerStopRequested.load())
		return;

	{
		std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
		m_MusicInfo = std::move(NewInfo);
	}
#endif
}

void CMusicIsland::TriggerControlAction(EControlButton Button)
{
#if defined(CONF_MUSIC_ISLAND_WINRT)
	std::thread([Button]() {
		try
		{
			winrt::init_apartment(winrt::apartment_type::multi_threaded);

			using namespace winrt::Windows::Media::Control;

			const auto SessionManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
			const auto Session = SessionManager.GetCurrentSession();
			if(Session)
			{
				switch(Button)
				{
				case CONTROL_BUTTON_PREVIOUS:
					Session.TrySkipPreviousAsync().get();
					break;
				case CONTROL_BUTTON_PLAY_PAUSE:
					Session.TryTogglePlayPauseAsync().get();
					break;
				case CONTROL_BUTTON_NEXT:
					Session.TrySkipNextAsync().get();
					break;
				default:
					break;
				}

				LogMusicIslandDebug("music-island-win", "Sent media control command");
			}
			else
				LogMusicIslandDebug("music-island-win", "No active media session for control command");

			winrt::uninit_apartment();
		}
		catch(const winrt::hresult_error &Error)
		{
			if(MusicIslandDebugEnabled())
				dbg_msg("music-island-win", "Failed to send media control command: 0x%08x", (unsigned int)Error.code().value);
		}
		catch(...)
		{
			LogMusicIslandDebug("music-island-win", "Failed to send media control command: unknown exception");
		}
	}).detach();
#elif defined(CONF_PLATFORM_LINUX) && defined(CONF_MUSIC_ISLAND_MPRIS)
	const char *pMethod = nullptr;
	switch(Button)
	{
	case CONTROL_BUTTON_PREVIOUS:
		pMethod = "Previous";
		break;
	case CONTROL_BUTTON_PLAY_PAUSE:
		pMethod = "PlayPause";
		break;
	case CONTROL_BUTTON_NEXT:
		pMethod = "Next";
		break;
	default:
		break;
	}
	std::thread([pMethod]() {
		TriggerMprisControlAction(pMethod);
	}).detach();
#else
	(void)Button;
#endif
}

bool CMusicIsland::IsActive() const
{
	return g_Config.m_PcShowMusicIsland != 0 && HasMusicIslandPlatformBackend();
}

void CMusicIsland::ConShowCurMusicInfo(IConsole::IResult *pResult, void *pUserData)
{
	auto *pSelf = static_cast<CMusicIsland *>(pUserData);
	const SMusicInfo Info = pSelf->GetMusicInfo();
	dbg_msg("Music", "Available: %d, Is playing: %d, Title: %s, Artist: %s", Info.m_Available, Info.m_Playing, Info.m_Title.c_str(), Info.m_Artist.c_str());
}

void CMusicIsland::RenderMusicIslandVisualizer(CUIRect *pBase)
{
	CUIRect VisualizerRect = *pBase;
	VisualizerRect.Margin(vec2(0.35f, 0.45f), &VisualizerRect);
	if(VisualizerRect.w <= 0.0f || VisualizerRect.h <= 0.0f)
		return;

	const SMusicInfo MusicInfo = GetMusicInfo();
	const float Time = LocalTime();
	const bool Animated = MusicInfo.m_Playing;
	const float MotionScale = Animated ? 1.0f : 0.0f;
	const float AlphaScale = MusicInfo.m_Playing ? 1.0f : (MusicInfo.m_Available ? 0.65f : 0.45f);
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	const float PixelSizeX = (ScreenX1 - ScreenX0) / Graphics()->ScreenWidth();
	const float PixelSizeY = (ScreenY1 - ScreenY0) / Graphics()->ScreenHeight();

	constexpr int BarCount = 5;
	const int CenterBar = BarCount / 2;
	float BarSpacing = maximum(PixelSizeX, 0.42f);
	float BarWidth = (VisualizerRect.w - BarSpacing * (BarCount - 1)) / BarCount;
	if(BarWidth <= 0.0f)
		return;

	BarWidth = maximum(PixelSizeX, SnapToScreenSpan(BarWidth, PixelSizeX));
	BarSpacing = maximum(0.0f, (VisualizerRect.w - BarWidth * BarCount) / (BarCount - 1));
	if(PixelSizeX > 0.0f)
		BarSpacing = maximum(0.0f, std::floor(BarSpacing / PixelSizeX) * PixelSizeX);
	const float TotalWidth = BarWidth * BarCount + BarSpacing * (BarCount - 1);
	float CursorX = VisualizerRect.x + (VisualizerRect.w - TotalWidth) / 2.0f;
	const float CenterY = VisualizerRect.y + VisualizerRect.h / 2.0f;

	for(int BarIndex = 0; BarIndex < BarCount; ++BarIndex)
	{
		const float DistFactor = 1.0f - std::abs(BarIndex - CenterBar) / (float)CenterBar;
		const float ShapeStrength = Animated ? maximum(0.0f, DistFactor) : 0.0f;
		const float Pulse = Animated ? GetVisualizerBarPulse(Time, BarIndex) : 0.0f;
		const float BaseHeight = Animated ? 2.35f : 1.95f;
		const float JumpHeight = (2.2f + ShapeStrength * 1.45f) * MotionScale;
		float Height = minimum(VisualizerRect.h, BaseHeight + Pulse * JumpHeight);
		Height = maximum(PixelSizeY, SnapToScreenSpan(Height, PixelSizeY));

		const float Left = SnapToScreenPixel(CursorX, ScreenX0, PixelSizeX);
		const float Top = std::clamp(
			SnapToScreenPixel(CenterY - Height / 2.0f, ScreenY0, PixelSizeY),
			VisualizerRect.y,
			VisualizerRect.y + VisualizerRect.h - Height);

		CUIRect BarRect = {Left, Top, BarWidth, Height};
		const float BarAlpha = (0.74f + ShapeStrength * 0.2f) * AlphaScale;
		const float BarRounding = minimum(BarRect.w, BarRect.h) / 2.0f;
		BarRect.Draw4(
			ColorRGBA(0.6f, 0.96f, 1.0f, BarAlpha),
			ColorRGBA(0.6f, 0.96f, 1.0f, BarAlpha),
			ColorRGBA(0.12f, 0.72f, 1.0f, BarAlpha),
			ColorRGBA(0.12f, 0.72f, 1.0f, BarAlpha),
			IGraphics::CORNER_ALL,
			BarRounding);

		CursorX += BarWidth + BarSpacing;
	}
}

void CMusicIsland::RenderMusicIslandMain(CUIRect *pBase)
{
	constexpr float BaseFontSize = 8.0f;
	constexpr float ExpandedTimerFontSize = 6.1f;
	constexpr float TitleFontSize = 4.3f;
	constexpr float ArtistFontSize = 3.5f;
	constexpr float MetadataGap = 0.4f;
	const float AnimProgress = m_ExtendAnim * m_ExtendAnim * (3.0f - 2.0f * m_ExtendAnim);
	const SMusicInfo MusicInfo = GetMusicInfo();
	SGameTimerRenderInfo RenderInfo;
	CUIRect TextRect = *pBase;
	TextRect.VMargin(0.5f, &TextRect);
	if(TextRect.w <= 0.0f || TextRect.h <= 0.0f)
		return;

	CUIRect TimerRect = TextRect;
	CUIRect MetadataRect = TextRect;
	const float TimerFontTarget = BaseFontSize + (ExpandedTimerFontSize - BaseFontSize) * AnimProgress;
	float TimerBlockHeight = TextRect.h;
	if(AnimProgress > 0.0f)
	{
		TimerBlockHeight = minimum(TextRect.h, 4.8f + 2.2f * (1.0f - AnimProgress));
		TextRect.HSplitTop(TimerBlockHeight, &TimerRect, &MetadataRect);
	}

	float RenderFontSize = minimum(TimerFontTarget, TimerRect.h);
	if(!GetGameTimerRenderInfo(GameClient()->m_Snap.m_pGameInfoObj, Client(), TextRender(), RenderFontSize, RenderInfo))
		return;

	if(g_Config.m_PcShowMusicIslandTimerFull && RenderInfo.m_TextWidth > TimerRect.w)
	{
		const float WidthScale = TimerRect.w / RenderInfo.m_TextWidth;
		RenderFontSize = maximum(1.0f, RenderFontSize * WidthScale);
		if(!GetGameTimerRenderInfo(GameClient()->m_Snap.m_pGameInfoObj, Client(), TextRender(), RenderFontSize, RenderInfo))
			return;
	}

	const bool ShouldScroll = !g_Config.m_PcShowMusicIslandTimerFull && RenderInfo.m_TextWidth > TimerRect.w;
	const float LayoutWidth = minimum(RenderInfo.m_TextWidth, TimerRect.w);
	float TextX = TimerRect.x + (TimerRect.w - LayoutWidth) / 2.0f;
	if(ShouldScroll)
	{
		const float Overflow = RenderInfo.m_TextWidth - TimerRect.w;
		TextX = TimerRect.x - GetScrollingTextOffset(Overflow, LocalTime());
	}

	float TextY = TimerRect.y + (TimerRect.h - RenderFontSize) / 2.0f;
	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	const float ScreenWidth = ScreenX1 - ScreenX0;
	const float ScreenHeight = ScreenY1 - ScreenY0;
	const float PixelSizeX = ScreenWidth / Graphics()->ScreenWidth();
	const float PixelSizeY = ScreenHeight / Graphics()->ScreenHeight();
	TextY = SnapToScreenPixel(TextY, ScreenY0, PixelSizeY);
	if(!ShouldScroll)
	{
		TextX = SnapToScreenPixel(TextX, ScreenX0, PixelSizeX);
		const float MaxTextX = maximum(TimerRect.x, TimerRect.x + TimerRect.w - RenderInfo.m_TextWidth);
		TextX = std::clamp(TextX, TimerRect.x, MaxTextX);
		TextRender()->TextColor(RenderInfo.m_TextColor);
		TextRender()->Text(TextX, TextY, RenderFontSize, RenderInfo.m_aText, -1.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		const CUIRect UiScreen = *Ui()->Screen();
		const CUIRect UiTimerRect = HudToUiRect(TimerRect, UiScreen, ScreenWidth, ScreenHeight);
		const float UiTextX = HudToUiX(TextX, UiScreen, ScreenX0, ScreenWidth);
		const float UiTextY = HudToUiY(TextY, UiScreen, ScreenY0, ScreenHeight);
		const float UiFontSize = RenderFontSize * UiScreen.h / maximum(ScreenHeight, 1.0f);

		Ui()->MapScreen();
		Ui()->ClipEnable(&UiTimerRect);
		TextRender()->TextColor(RenderInfo.m_TextColor);
		TextRender()->Text(UiTextX, UiTextY, UiFontSize, RenderInfo.m_aText, -1.0f);
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		Ui()->ClipDisable();
		Graphics()->MapScreen(ScreenX0, ScreenY0, ScreenX1, ScreenY1);
	}

	if(AnimProgress <= 0.0f)
		return;

	const float MetadataAlpha = AnimProgress * AnimProgress;
	MetadataRect.HMargin(0.25f, &MetadataRect);
	if(MetadataRect.w <= 0.0f || MetadataRect.h <= 0.0f)
		return;

	const float TitleLineHeight = TitleFontSize + 0.6f;
	const float ArtistLineHeight = ArtistFontSize + 0.5f;
	const float TotalTextHeight = TitleLineHeight + MetadataGap + ArtistLineHeight;
	if(MetadataRect.h < TotalTextHeight)
		return;

	CUIRect TitleRect = MetadataRect;
	CUIRect ArtistRect = MetadataRect;
	const float TopOffset = (MetadataRect.h - TotalTextHeight) / 2.0f;
	TitleRect.y += TopOffset;
	TitleRect.h = TitleLineHeight;
	ArtistRect.y = TitleRect.y + TitleRect.h + MetadataGap;
	ArtistRect.h = ArtistLineHeight;

	RenderCenteredClippedText(Graphics(), TextRender(), Ui(), TitleRect, MusicInfo.m_Title.c_str(), TitleFontSize, ColorRGBA(1.0f, 1.0f, 1.0f, MetadataAlpha), LocalTime());
	RenderCenteredClippedText(Graphics(), TextRender(), Ui(), ArtistRect, MusicInfo.m_Artist.c_str(), ArtistFontSize, ColorRGBA(0.82f, 0.86f, 0.92f, MetadataAlpha), LocalTime() + 0.8f);
}

void CMusicIsland::UpdateMusicImageTexture()
{
	CImageInfo PendingImage;

	{
		std::lock_guard<std::mutex> Guard(m_MusicInfoMutex);
		if(!m_MusicImageDirty)
			return;

		PendingImage = std::move(m_PendingMusicImage);
		m_MusicImageDirty = false;
	}

	if(m_MusicImageTexture.IsValid())
		Graphics()->UnloadTexture(&m_MusicImageTexture);

	m_MusicImageWidth = 0;
	m_MusicImageHeight = 0;
	if(PendingImage.m_pData == nullptr)
		return;

	m_MusicImageWidth = (int)PendingImage.m_Width;
	m_MusicImageHeight = (int)PendingImage.m_Height;
	m_MusicImageTexture = Graphics()->LoadTextureRawMove(PendingImage, 0, "music-island-artwork");
}

void CMusicIsland::RenderMusicIslandImage(CUIRect *pBase)
{
	CUIRect ImageRect = *pBase;
	const float CubeSize = minimum(ImageRect.w, ImageRect.h);
	ImageRect.x += ImageRect.w - CubeSize;
	ImageRect.y += (ImageRect.h - CubeSize) / 2.0f;
	ImageRect.w = CubeSize;
	ImageRect.h = CubeSize;

	const bool HasArtwork = m_MusicImageTexture.IsValid() && m_MusicImageWidth > 0 && m_MusicImageHeight > 0;
	const float ImageRounding = CubeSize * SMusicIslandProperties::ms_ArtworkRadiusFraction;
	ImageRect.Draw(SMusicIslandProperties::WindowColorDark(), IGraphics::CORNER_ALL, ImageRounding);

	if(HasArtwork)
	{
		Graphics()->WrapClamp();
		Graphics()->TextureSet(m_MusicImageTexture);
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		const IGraphics::CQuadItem QuadItem(ImageRect.x, ImageRect.y, ImageRect.w, ImageRect.h);
		Graphics()->QuadsDrawTL(&QuadItem, 1);
		Graphics()->QuadsEnd();
		Graphics()->WrapNormal();
		return;
	}

	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	const float IconSize = minimum(ImageRect.w, ImageRect.h) * 0.75f;
	const float IconWidth = TextRender()->TextWidth(IconSize, FontIcon::MUSIC);
	const float IconX = ImageRect.x + (ImageRect.w - IconWidth) / 2.0f;
	const float IconY = ImageRect.y + (ImageRect.h - IconSize) / 2.0f;
	TextRender()->Text(IconX, IconY, IconSize, FontIcon::MUSIC, -1.0f);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}
