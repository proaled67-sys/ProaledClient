#include "settingsprofiles.h"

#include <base/system.h>

#include <engine/console.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <array>
#include <unordered_map>
#include <utility>

// Full ai function. Pls don't use this in yours client maybe only for base

CRushieSettingsProfileEntry::CRushieSettingsProfileEntry(int Source, std::string CommandLine)
{
	m_Source = Source;
	m_CommandLine = std::move(CommandLine);
}

CRushieSettingsProfile::CRushieSettingsProfile(const char *pName)
{
	m_Name = pName;
}

bool CRushieSettingsProfile::HasSource(int Source) const
{
	for(const CRushieSettingsProfileEntry &Entry : m_vEntries)
	{
		if(Entry.m_Source == Source)
			return true;
	}
	return false;
}

int CRushieSettingsProfile::CountForSource(int Source) const
{
	int Count = 0;
	for(const CRushieSettingsProfileEntry &Entry : m_vEntries)
	{
		if(Entry.m_Source == Source &&
			!((Source == RUSHIESETTINGSPROFILE_SOURCE_BINDS && Entry.m_CommandLine == "unbindall") ||
				(Source == RUSHIESETTINGSPROFILE_SOURCE_TCLIENT_BINDWHEEL && Entry.m_CommandLine == "delete_all_bindwheel_binds") ||
				(Source == RUSHIESETTINGSPROFILE_SOURCE_PROALEDCLIENT_BINDWHEEL && Entry.m_CommandLine == "delete_all_bindwheel_binds_spec") ||
				(Source == RUSHIESETTINGSPROFILE_SOURCE_CHATBINDS && Entry.m_CommandLine == "unbindchatall") ||
				(Source == RUSHIESETTINGSPROFILE_SOURCE_SKINPROFILES && Entry.m_CommandLine == "clear_skin_profiles") ||
				(Source == RUSHIESETTINGSPROFILE_SOURCE_WARLIST && Entry.m_CommandLine == "reset_warlist")))
			Count++;
	}
	return Count;
}

bool CRushieSettingsProfiles::IsExcludedConfigVariable(const SConfigVariable *pVariable)
{
	const char *pScriptName = pVariable->m_pScriptName;
	return str_comp(pScriptName, "ui_page") == 0 ||
		str_comp(pScriptName, "ui_settings_page") == 0 ||
		str_comp(pScriptName, "tc_ui_show_ddnet") == 0 ||
		str_comp(pScriptName, "tc_ui_show_tclient") == 0 ||
		str_comp(pScriptName, "tc_ui_only_modified") == 0 ||
		str_comp(pScriptName, "tc_ui_compact_list") == 0 ||
		str_comp(pScriptName, "pc_ui_show_proaledclient") == 0;
}

void CRushieSettingsProfiles::OnConsoleInit()
{
	IConfigManager *pConfigManager = Kernel()->RequestInterface<IConfigManager>();
	if(pConfigManager)
		pConfigManager->RegisterCallback(ConfigSaveCallback, this, ConfigDomain::PROALEDCLIENTSETTINGSPROFILES);

	Console()->Register("pc_add_settings_profile", "s[name]", CFGFLAG_CLIENT, ConAddSettingsProfile, this, "Add a settings profile");
	Console()->Register("pc_add_settings_profile_setting", "i[domain] s[commandline]", CFGFLAG_CLIENT, ConAddSettingsProfileSetting, this, "Add a settings command line to the most recently added settings profile");
	Console()->Register("pc_apply_settings_profile", "s[name]", CFGFLAG_CLIENT, ConApplySettingsProfile, this, "Apply a settings profile by name");
}

void CRushieSettingsProfiles::EscapeQuotedParam(char *pDst, const char *pSrc, int Size)
{
	str_escape(&pDst, pSrc, pDst + Size);
}

bool CRushieSettingsProfiles::IsResetCommandForSource(int Source, const std::string &CommandLine)
{
	if(Source == RUSHIESETTINGSPROFILE_SOURCE_BINDS && CommandLine == "unbindall")
		return true;
	if(Source == RUSHIESETTINGSPROFILE_SOURCE_TCLIENT_BINDWHEEL && CommandLine == "delete_all_bindwheel_binds")
		return true;
	if(Source == RUSHIESETTINGSPROFILE_SOURCE_PROALEDCLIENT_BINDWHEEL && CommandLine == "delete_all_bindwheel_binds_spec")
		return true;
	return (Source == RUSHIESETTINGSPROFILE_SOURCE_CHATBINDS && CommandLine == "unbindchatall") ||
		(Source == RUSHIESETTINGSPROFILE_SOURCE_SKINPROFILES && CommandLine == "clear_skin_profiles") ||
		(Source == RUSHIESETTINGSPROFILE_SOURCE_WARLIST && CommandLine == "reset_warlist");
}

void CRushieSettingsProfiles::ConAddSettingsProfile(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CRushieSettingsProfiles *>(pUserData)->AddProfile(pResult->GetString(0));
}

void CRushieSettingsProfiles::ConAddSettingsProfileSetting(IConsole::IResult *pResult, void *pUserData)
{
	CRushieSettingsProfiles *pSelf = static_cast<CRushieSettingsProfiles *>(pUserData);
	const int Source = pResult->GetInteger(0);
	pSelf->AddProfileSetting(Source, pResult->GetString(1));
}

void CRushieSettingsProfiles::ConApplySettingsProfile(IConsole::IResult *pResult, void *pUserData)
{
	CRushieSettingsProfiles *pSelf = static_cast<CRushieSettingsProfiles *>(pUserData);
	const char *pName = pResult->GetString(0);
	const int Index = pSelf->FindProfileByName(pName);
	if(Index < 0)
	{
		char aBuf[256];
		str_format(aBuf, sizeof(aBuf), "Settings profile not found: %s", pName);
		pSelf->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "settings_profiles", aBuf);
		return;
	}

	pSelf->ApplyProfile(pSelf->m_vProfiles[Index]);
}

void CRushieSettingsProfiles::AddProfile(const char *pName)
{
	m_vProfiles.emplace_back(pName);
}

void CRushieSettingsProfiles::AddProfileSetting(int Source, const char *pCommandLine)
{
	if(m_vProfiles.empty())
		return;
	m_vProfiles.back().m_vEntries.emplace_back(Source, pCommandLine);
}

CRushieSettingsProfile CRushieSettingsProfiles::CaptureProfile(const char *pName, bool IncludeDdnet, bool IncludeBinds, bool IncludeTClient, bool IncludeTClientBindWheel, bool IncludeRClient, bool IncludeRClientBindWheel, bool IncludeWarlist, bool IncludeChatBinds, bool IncludeSkinProfiles) const
{
	CRushieSettingsProfile Profile(pName);
	struct SCollectorContext
	{
		CRushieSettingsProfile *m_pProfile;
		std::array<bool, ConfigDomain::NUM> m_aIncludedDomains;
	};
	SCollectorContext Context{};
	Context.m_pProfile = &Profile;
	Context.m_aIncludedDomains[ConfigDomain::DDNET] = IncludeDdnet;
	Context.m_aIncludedDomains[ConfigDomain::TCLIENT] = IncludeTClient;
	Context.m_aIncludedDomains[ConfigDomain::PROALEDCLIENT] = IncludeRClient;

	auto Collector = [](const SConfigVariable *pVariable, void *pUserData) {
		SCollectorContext *pContext = static_cast<SCollectorContext *>(pUserData);
		if(!pContext->m_aIncludedDomains[pVariable->m_ConfigDomain] || CRushieSettingsProfiles::IsExcludedConfigVariable(pVariable))
			return;

		char aLine[2048];
		pVariable->Serialize(aLine, sizeof(aLine));
		pContext->m_pProfile->m_vEntries.emplace_back(pVariable->m_ConfigDomain, aLine);
	};

	ConfigManager()->PossibleConfigVariables("", CFGFLAG_CLIENT | CFGFLAG_SAVE, Collector, &Context);

	if(IncludeBinds)
	{
		Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_BINDS, "unbindall");
		std::vector<std::string> vBindCommands;
		GameClient()->m_Binds.GetBindCommands(vBindCommands);
		for(const std::string &BindCommand : vBindCommands)
			Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_BINDS, BindCommand);
	}

	if(IncludeTClientBindWheel)
	{
		Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_TCLIENT_BINDWHEEL, "delete_all_bindwheel_binds");
		for(const CBindWheel::CBind &Bind : GameClient()->m_BindWheel.m_vBinds)
		{
			char aBuf[BINDWHEEL_MAX_CMD * 2] = "";
			char *pEnd = aBuf + sizeof(aBuf);
			char *pDst;
			str_append(aBuf, "add_bindwheel \"");
			pDst = aBuf + str_length(aBuf);
			EscapeQuotedParam(pDst, Bind.m_aName, (int)(pEnd - pDst));
			str_append(aBuf, "\" \"");
			pDst = aBuf + str_length(aBuf);
			EscapeQuotedParam(pDst, Bind.m_aCommand, (int)(pEnd - pDst));
			str_append(aBuf, "\"");
			Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_TCLIENT_BINDWHEEL, aBuf);
		}
	}

	if(IncludeRClientBindWheel)
	{
		Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_PROALEDCLIENT_BINDWHEEL, "delete_all_bindwheel_binds_spec");
		for(const CBindWheelSpec::CBind &Bind : GameClient()->m_BindWheelSpec.m_vBinds)
		{
			char aBuf[BINDWHEEL_MAX_CMD_PROALEDCLIENT * 2] = "";
			char *pEnd = aBuf + sizeof(aBuf);
			char *pDst;
			str_append(aBuf, "add_bindwheel_spec \"");
			pDst = aBuf + str_length(aBuf);
			EscapeQuotedParam(pDst, Bind.m_aName, (int)(pEnd - pDst));
			str_append(aBuf, "\" \"");
			pDst = aBuf + str_length(aBuf);
			EscapeQuotedParam(pDst, Bind.m_aCommand, (int)(pEnd - pDst));
			str_append(aBuf, "\"");
			Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_PROALEDCLIENT_BINDWHEEL, aBuf);
		}
	}

	if(IncludeChatBinds)
	{
		Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_CHATBINDS, "unbindchatall");
		for(const CBindChat::CBind &Bind : GameClient()->m_BindChat.m_vBinds)
		{
			char aBuf[BINDCHAT_MAX_CMD * 2] = "";
			char *pEnd = aBuf + sizeof(aBuf);
			char *pDst;
			str_append(aBuf, "bindchat \"");
			pDst = aBuf + str_length(aBuf);
			EscapeQuotedParam(pDst, Bind.m_aName, (int)(pEnd - pDst));
			str_append(aBuf, "\" \"");
			pDst = aBuf + str_length(aBuf);
			EscapeQuotedParam(pDst, Bind.m_aCommand, (int)(pEnd - pDst));
			str_append(aBuf, "\"");
			Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_CHATBINDS, aBuf);
		}
	}

	if(IncludeSkinProfiles)
	{
		Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_SKINPROFILES, "clear_skin_profiles");
		for(const CProfile &SkinProfile : GameClient()->m_SkinProfiles.m_Profiles)
		{
			char aBuf[256];
			char aBufTemp[128];
			char aEscapeBuf[256];

			str_copy(aBuf, "add_profile ", sizeof(aBuf));
			str_format(aBufTemp, sizeof(aBufTemp), "%d ", SkinProfile.m_BodyColor);
			str_append(aBuf, aBufTemp, sizeof(aBuf));
			str_format(aBufTemp, sizeof(aBufTemp), "%d ", SkinProfile.m_FeetColor);
			str_append(aBuf, aBufTemp, sizeof(aBuf));
			str_format(aBufTemp, sizeof(aBufTemp), "%d ", SkinProfile.m_CountryFlag);
			str_append(aBuf, aBufTemp, sizeof(aBuf));
			str_format(aBufTemp, sizeof(aBufTemp), "%d ", SkinProfile.m_Emote);
			str_append(aBuf, aBufTemp, sizeof(aBuf));

			EscapeQuotedParam(aEscapeBuf, SkinProfile.m_SkinName, sizeof(aEscapeBuf));
			str_format(aBufTemp, sizeof(aBufTemp), "\"%s\" ", aEscapeBuf);
			str_append(aBuf, aBufTemp, sizeof(aBuf));

			EscapeQuotedParam(aEscapeBuf, SkinProfile.m_Name, sizeof(aEscapeBuf));
			str_format(aBufTemp, sizeof(aBufTemp), "\"%s\" ", aEscapeBuf);
			str_append(aBuf, aBufTemp, sizeof(aBuf));

			EscapeQuotedParam(aEscapeBuf, SkinProfile.m_Clan, sizeof(aEscapeBuf));
			str_format(aBufTemp, sizeof(aBufTemp), "\"%s\"", aEscapeBuf);
			str_append(aBuf, aBufTemp, sizeof(aBuf));

			Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_SKINPROFILES, aBuf);
		}
	}

	if(IncludeWarlist)
	{
		Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_WARLIST, "reset_warlist");
		for(int i = 0; i < (int)GameClient()->m_WarList.m_WarTypes.size(); i++)
		{
			const CWarType &WarType = *GameClient()->m_WarList.m_WarTypes[i];
			char aBuf[1024];
			char aEscapeType[MAX_WARLIST_TYPE_LENGTH * 2];
			EscapeQuotedParam(aEscapeType, WarType.m_aWarName, sizeof(aEscapeType));
			const ColorHSLA Color = color_cast<ColorHSLA>(WarType.m_Color);
			str_format(aBuf, sizeof(aBuf), "update_war_group %d \"%s\" %d", i, aEscapeType, Color.Pack(false));
			Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_WARLIST, aBuf);
		}

		for(const CWarEntry &Entry : GameClient()->m_WarList.m_vWarEntries)
		{
			char aBuf[1024];
			char aEscapeType[MAX_WARLIST_TYPE_LENGTH * 2];
			char aEscapeName[MAX_NAME_LENGTH * 2];
			char aEscapeClan[MAX_CLAN_LENGTH * 2];
			char aEscapeReason[MAX_WARLIST_REASON_LENGTH * 2];

			EscapeQuotedParam(aEscapeType, Entry.m_pWarType->m_aWarName, sizeof(aEscapeType));
			EscapeQuotedParam(aEscapeName, Entry.m_aName, sizeof(aEscapeName));
			EscapeQuotedParam(aEscapeClan, Entry.m_aClan, sizeof(aEscapeClan));
			EscapeQuotedParam(aEscapeReason, Entry.m_aReason, sizeof(aEscapeReason));

			str_format(aBuf, sizeof(aBuf), "add_war_entry \"%s\" \"%s\" \"%s\" \"%s\"", aEscapeType, aEscapeName, aEscapeClan, aEscapeReason);
			Profile.m_vEntries.emplace_back(RUSHIESETTINGSPROFILE_SOURCE_WARLIST, aBuf);
		}
	}

	return Profile;
}

void CRushieSettingsProfiles::SaveProfile(const char *pName, bool IncludeDdnet, bool IncludeBinds, bool IncludeTClient, bool IncludeTClientBindWheel, bool IncludeRClient, bool IncludeRClientBindWheel, bool IncludeWarlist, bool IncludeChatBinds, bool IncludeSkinProfiles)
{
	m_vProfiles.push_back(CaptureProfile(pName, IncludeDdnet, IncludeBinds, IncludeTClient, IncludeTClientBindWheel, IncludeRClient, IncludeRClientBindWheel, IncludeWarlist, IncludeChatBinds, IncludeSkinProfiles));
}

void CRushieSettingsProfiles::OverrideProfile(int Index, const char *pName, bool IncludeDdnet, bool IncludeBinds, bool IncludeTClient, bool IncludeTClientBindWheel, bool IncludeRClient, bool IncludeRClientBindWheel, bool IncludeWarlist, bool IncludeChatBinds, bool IncludeSkinProfiles)
{
	if(Index < 0 || Index >= (int)m_vProfiles.size())
		return;
	m_vProfiles[Index] = CaptureProfile(pName, IncludeDdnet, IncludeBinds, IncludeTClient, IncludeTClientBindWheel, IncludeRClient, IncludeRClientBindWheel, IncludeWarlist, IncludeChatBinds, IncludeSkinProfiles);
}

void CRushieSettingsProfiles::ApplyProfile(const CRushieSettingsProfile &Profile)
{
	for(const CRushieSettingsProfileEntry &Entry : Profile.m_vEntries)
		Console()->ExecuteLine(Entry.m_CommandLine.c_str(), IConsole::CLIENT_ID_UNSPECIFIED);
}

void CRushieSettingsProfiles::GetCurrentConfigDomainStats(ConfigDomain Domain, int &Modified, int &Total) const
{
	Modified = 0;
	Total = 0;

	struct SContext
	{
		ConfigDomain m_Domain;
		int *m_pModified;
		int *m_pTotal;
	};

	SContext Context{Domain, &Modified, &Total};
	auto Collector = [](const SConfigVariable *pVariable, void *pUserData) {
		SContext *pContext = static_cast<SContext *>(pUserData);
		if(pVariable->m_ConfigDomain != pContext->m_Domain || CRushieSettingsProfiles::IsExcludedConfigVariable(pVariable))
			return;

		(*pContext->m_pTotal)++;
		if(!pVariable->IsDefault())
			(*pContext->m_pModified)++;
	};

	ConfigManager()->PossibleConfigVariables("", CFGFLAG_CLIENT | CFGFLAG_SAVE, Collector, &Context);
}

void CRushieSettingsProfiles::GetProfileConfigDomainStats(const CRushieSettingsProfile &Profile, ConfigDomain Domain, int &Modified, int &Total) const
{
	Modified = 0;
	Total = 0;

	std::unordered_map<std::string, std::string> aEntriesByScriptName;
	for(const CRushieSettingsProfileEntry &Entry : Profile.m_vEntries)
	{
		if(Entry.m_Source != (int)Domain)
			continue;

		const size_t SpacePos = Entry.m_CommandLine.find(' ');
		const std::string ScriptName = SpacePos == std::string::npos ? Entry.m_CommandLine : Entry.m_CommandLine.substr(0, SpacePos);
		aEntriesByScriptName[ScriptName] = Entry.m_CommandLine;
	}

	struct SContext
	{
		ConfigDomain m_Domain;
		int *m_pModified;
		int *m_pTotal;
		std::unordered_map<std::string, std::string> *m_pEntriesByScriptName;
	};

	SContext Context{Domain, &Modified, &Total, &aEntriesByScriptName};
	auto Collector = [](const SConfigVariable *pVariable, void *pUserData) {
		SContext *pContext = static_cast<SContext *>(pUserData);
		if(pVariable->m_ConfigDomain != pContext->m_Domain || CRushieSettingsProfiles::IsExcludedConfigVariable(pVariable))
			return;

		(*pContext->m_pTotal)++;

		const auto It = pContext->m_pEntriesByScriptName->find(pVariable->m_pScriptName);
		if(It == pContext->m_pEntriesByScriptName->end())
			return;

		char aDefaultLine[2048];
		switch(pVariable->m_Type)
		{
		case SConfigVariable::VAR_INT:
			static_cast<const SIntConfigVariable *>(pVariable)->Serialize(aDefaultLine, sizeof(aDefaultLine), static_cast<const SIntConfigVariable *>(pVariable)->m_Default);
			break;
		case SConfigVariable::VAR_COLOR:
			static_cast<const SColorConfigVariable *>(pVariable)->Serialize(aDefaultLine, sizeof(aDefaultLine), static_cast<const SColorConfigVariable *>(pVariable)->m_Default);
			break;
		case SConfigVariable::VAR_STRING:
			static_cast<const SStringConfigVariable *>(pVariable)->Serialize(aDefaultLine, sizeof(aDefaultLine), static_cast<const SStringConfigVariable *>(pVariable)->m_pDefault);
			break;
		default:
			return;
		}

		if(It->second != aDefaultLine)
			(*pContext->m_pModified)++;
	};

	ConfigManager()->PossibleConfigVariables("", CFGFLAG_CLIENT | CFGFLAG_SAVE, Collector, &Context);
}

int CRushieSettingsProfiles::FindProfileByName(const char *pName) const
{
	for(size_t i = 0; i < m_vProfiles.size(); i++)
	{
		if(str_comp(m_vProfiles[i].m_Name.c_str(), pName) == 0)
			return (int)i;
	}
	return -1;
}

std::string CRushieSettingsProfiles::MakeUniqueProfileName(const char *pBaseName, int IgnoreIndex) const
{
	const char *pSafeBaseName = pBaseName != nullptr && pBaseName[0] != '\0' ? pBaseName : "Rushie Profile";
	if(FindProfileByName(pSafeBaseName) == -1 || (IgnoreIndex >= 0 && IgnoreIndex < (int)m_vProfiles.size() && str_comp(m_vProfiles[IgnoreIndex].m_Name.c_str(), pSafeBaseName) == 0))
		return pSafeBaseName;

	for(int Suffix = 2;; Suffix++)
	{
		char aName[128];
		str_format(aName, sizeof(aName), "%s %d", pSafeBaseName, Suffix);

		bool Taken = false;
		for(size_t i = 0; i < m_vProfiles.size(); i++)
		{
			if((int)i == IgnoreIndex)
				continue;
			if(str_comp(m_vProfiles[i].m_Name.c_str(), aName) == 0)
			{
				Taken = true;
				break;
			}
		}

		if(!Taken)
			return aName;
	}
}

void CRushieSettingsProfiles::ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
{
	CRushieSettingsProfiles *pThis = static_cast<CRushieSettingsProfiles *>(pUserData);
	char aLine[4096];
	char aEscaped[4096];
	for(const CRushieSettingsProfile &Profile : pThis->m_vProfiles)
	{
		EscapeQuotedParam(aEscaped, Profile.m_Name.c_str(), sizeof(aEscaped));
		str_format(aLine, sizeof(aLine), "pc_add_settings_profile \"%s\"", aEscaped);
		pConfigManager->WriteLine(aLine, ConfigDomain::PROALEDCLIENTSETTINGSPROFILES);

		for(const CRushieSettingsProfileEntry &Entry : Profile.m_vEntries)
		{
			EscapeQuotedParam(aEscaped, Entry.m_CommandLine.c_str(), sizeof(aEscaped));
			str_format(aLine, sizeof(aLine), "pc_add_settings_profile_setting %d \"%s\"", Entry.m_Source, aEscaped);
			pConfigManager->WriteLine(aLine, ConfigDomain::PROALEDCLIENTSETTINGSPROFILES);
		}
	}
}
