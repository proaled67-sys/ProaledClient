#ifndef GAME_CLIENT_COMPONENTS_PROALEDCLIENT_SETTINGSPROFILES_H
#define GAME_CLIENT_COMPONENTS_PROALEDCLIENT_SETTINGSPROFILES_H

#include <engine/config.h>
#include <engine/console.h>

#include <game/client/component.h>

#include <string>
#include <vector>

enum ERushieSettingsProfileSource
{
	RUSHIESETTINGSPROFILE_SOURCE_DDNET = (int)ConfigDomain::DDNET,
	RUSHIESETTINGSPROFILE_SOURCE_TCLIENT = (int)ConfigDomain::TCLIENT,
	RUSHIESETTINGSPROFILE_SOURCE_PROALEDCLIENT = (int)ConfigDomain::PROALEDCLIENT,
	RUSHIESETTINGSPROFILE_SOURCE_BINDS = 100,
	RUSHIESETTINGSPROFILE_SOURCE_TCLIENT_BINDWHEEL = 101,
	RUSHIESETTINGSPROFILE_SOURCE_PROALEDCLIENT_BINDWHEEL = 102,
	RUSHIESETTINGSPROFILE_SOURCE_WARLIST = 103,
	RUSHIESETTINGSPROFILE_SOURCE_CHATBINDS = 104,
	RUSHIESETTINGSPROFILE_SOURCE_SKINPROFILES = 105,
};

class CRushieSettingsProfileEntry
{
public:
	int m_Source;
	std::string m_CommandLine;

	CRushieSettingsProfileEntry(int Source, std::string CommandLine);
};

class CRushieSettingsProfile
{
public:
	std::string m_Name;
	std::vector<CRushieSettingsProfileEntry> m_vEntries;

	CRushieSettingsProfile() = default;
	explicit CRushieSettingsProfile(const char *pName);

	bool HasSource(int Source) const;
	int CountForSource(int Source) const;
};

class CRushieSettingsProfiles : public CComponent
{
	static void ConAddSettingsProfile(IConsole::IResult *pResult, void *pUserData);
	static void ConAddSettingsProfileSetting(IConsole::IResult *pResult, void *pUserData);
	static void ConApplySettingsProfile(IConsole::IResult *pResult, void *pUserData);
	static void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData);

	static bool IsExcludedConfigVariable(const SConfigVariable *pVariable);
	static bool IsResetCommandForSource(int Source, const std::string &CommandLine);
	static void EscapeQuotedParam(char *pDst, const char *pSrc, int Size);

public:
	std::vector<CRushieSettingsProfile> m_vProfiles;

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;

	void AddProfile(const char *pName);
	void AddProfileSetting(int Source, const char *pCommandLine);
	CRushieSettingsProfile CaptureProfile(const char *pName, bool IncludeDdnet, bool IncludeBinds, bool IncludeTClient, bool IncludeTClientBindWheel, bool IncludeRClient, bool IncludeRClientBindWheel, bool IncludeWarlist, bool IncludeChatBinds, bool IncludeSkinProfiles) const;
	void SaveProfile(const char *pName, bool IncludeDdnet, bool IncludeBinds, bool IncludeTClient, bool IncludeTClientBindWheel, bool IncludeRClient, bool IncludeRClientBindWheel, bool IncludeWarlist, bool IncludeChatBinds, bool IncludeSkinProfiles);
	void OverrideProfile(int Index, const char *pName, bool IncludeDdnet, bool IncludeBinds, bool IncludeTClient, bool IncludeTClientBindWheel, bool IncludeRClient, bool IncludeRClientBindWheel, bool IncludeWarlist, bool IncludeChatBinds, bool IncludeSkinProfiles);
	void ApplyProfile(const CRushieSettingsProfile &Profile);
	void GetCurrentConfigDomainStats(ConfigDomain Domain, int &Modified, int &Total) const;
	void GetProfileConfigDomainStats(const CRushieSettingsProfile &Profile, ConfigDomain Domain, int &Modified, int &Total) const;

	int FindProfileByName(const char *pName) const;
	std::string MakeUniqueProfileName(const char *pBaseName, int IgnoreIndex = -1) const;
};

#endif
