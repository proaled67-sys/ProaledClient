#include <game/client/components/proaledclient/clientindicator/browser_cache.h>
#include <game/client/components/proaledclient/clientindicator/client_indicator_sync.h>
#include <game/client/components/proaledclient/clientindicator/protocol.h>

#include <base/net.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>

#include <gtest/gtest.h>

#include <algorithm>

namespace
{
json_value *ParseJson(const char *pJson)
{
	return json_parse(pJson, str_length(pJson));
}
}

TEST(ClientIndicator, CollectActiveLocalClientIdsFiltersInvalidAndInactiveIds)
{
	int aLocalIds[NUM_DUMMIES] = {3, 17};
	std::array<bool, MAX_CLIENTS> aClientActive{};
	aClientActive[3] = true;

	const auto Snapshot = ProaledClientIndicatorClient::CollectActiveLocalClientIds(aLocalIds, aClientActive);

	ASSERT_EQ(Snapshot.m_vClientIds.size(), 1u);
	EXPECT_EQ(Snapshot.m_vClientIds[0], 3);
	EXPECT_TRUE(Snapshot.Contains(3));
	EXPECT_FALSE(Snapshot.Contains(17));
}

TEST(ClientIndicator, CollectActiveLocalClientIdsDeduplicatesLocalSlots)
{
	int aLocalIds[NUM_DUMMIES] = {9, 9};
	std::array<bool, MAX_CLIENTS> aClientActive{};
	aClientActive[9] = true;

	const auto Snapshot = ProaledClientIndicatorClient::CollectActiveLocalClientIds(aLocalIds, aClientActive);

	ASSERT_EQ(Snapshot.m_vClientIds.size(), 1u);
	EXPECT_EQ(Snapshot.m_vClientIds[0], 9);
	EXPECT_TRUE(Snapshot.Contains(9));
}

TEST(ClientIndicator, BrowserCacheParsesDeveloperFlag)
{
	json_value *pJson = ParseJson(R"([{"server_address":"127.0.0.1:8303","players":[{"name":"dev","developer":true,"version":"1.7.1"},{"name":"user"}]}])");
	ASSERT_TRUE(pJson);

	CBrowserCache Cache;
	EXPECT_TRUE(Cache.Load(*pJson));
	json_value_free(pJson);

	const auto &Players = Cache.Players();
	ASSERT_EQ(Players.size(), 2u);
	const auto DevIt = std::find_if(Players.begin(), Players.end(), [](const IServerBrowser::CProaledClientPlayerEntry &Entry) {
		return str_comp(Entry.m_aName, "dev") == 0;
	});
	ASSERT_NE(DevIt, Players.end());
	EXPECT_TRUE(DevIt->m_Developer);
	bool Developer = false;
	EXPECT_TRUE(Cache.HasPlayer("127.0.0.1:8303", "dev", &Developer));
	EXPECT_TRUE(Developer);
	char aVersion[32];
	EXPECT_TRUE(Cache.GetPlayerVersion("127.0.0.1:8303", "dev", aVersion, sizeof(aVersion)));
	EXPECT_STREQ(aVersion, "1.7.1");

	const auto UserIt = std::find_if(Players.begin(), Players.end(), [](const IServerBrowser::CProaledClientPlayerEntry &Entry) {
		return str_comp(Entry.m_aName, "user") == 0;
	});
	ASSERT_NE(UserIt, Players.end());
	EXPECT_FALSE(UserIt->m_Developer);
	Developer = true;
	EXPECT_TRUE(Cache.HasPlayer("127.0.0.1:8303", "user", &Developer));
	EXPECT_FALSE(Developer);
	EXPECT_FALSE(Cache.GetPlayerVersion("127.0.0.1:8303", "user", aVersion, sizeof(aVersion)));
}

TEST(ClientIndicator, BrowserCacheKeepsLegacyJsonNonDeveloper)
{
	json_value *pJson = ParseJson(R"([{"server_address":"127.0.0.1:8303","players":["legacy"]}])");
	ASSERT_TRUE(pJson);

	CBrowserCache Cache;
	EXPECT_TRUE(Cache.Load(*pJson));
	json_value_free(pJson);

	ASSERT_EQ(Cache.Players().size(), 1u);
	EXPECT_STREQ(Cache.Players()[0].m_aName, "legacy");
	EXPECT_FALSE(Cache.Players()[0].m_Developer);
}

TEST(ClientIndicator, ParseAddressTrimsConfigWhitespace)
{
	NETADDR Addr = NETADDR_ZEROED;
	ASSERT_TRUE(ProaledClientIndicator::ParseAddress("  2.27.23.203:8778  ", ProaledClientIndicator::DEFAULT_PORT, Addr));

	char aAddress[NETADDR_MAXSTRSIZE];
	net_addr_str(&Addr, aAddress, sizeof(aAddress), true);
	EXPECT_STREQ(aAddress, "2.27.23.203:8778");
}

TEST(ClientIndicator, DevAuthPacketUsesHmac)
{
	std::vector<uint8_t> vPacket;
	ProaledClientIndicator::WriteHeader(vPacket, ProaledClientIndicator::PACKET_DEV_AUTH);
	CUuid InstanceId = UUID_ZEROED;
	CUuid Nonce = UUID_ZEROED;
	Nonce.m_aData[0] = 1;
	ProaledClientIndicator::WriteUuid(vPacket, InstanceId);
	ProaledClientIndicator::WriteUuid(vPacket, Nonce);
	ProaledClientIndicator::WriteU64(vPacket, 123);
	ProaledClientIndicator::WriteString(vPacket, "127.0.0.1:8303");
	ProaledClientIndicator::WriteString(vPacket, "dev");
	ProaledClientIndicator::WriteS16(vPacket, 4);
	ProaledClientIndicator::AppendHmacSha256(vPacket, "secret");

	ProaledClientIndicator::CClientPresencePacket Packet;
	ASSERT_TRUE(ProaledClientIndicator::ReadDevAuthPacket(vPacket.data(), (int)vPacket.size(), Packet));
	EXPECT_TRUE(ProaledClientIndicator::ValidateHmacSha256("secret", vPacket.data(), (int)vPacket.size()));
	EXPECT_FALSE(ProaledClientIndicator::ValidateHmacSha256("wrong", vPacket.data(), (int)vPacket.size()));
	EXPECT_EQ(Packet.m_ClientId, 4);
	EXPECT_EQ(Packet.m_PlayerName, "dev");
}

TEST(ClientIndicator, DevAuthPacketHmacRoundTrips)
{
	std::vector<uint8_t> vPacket;
	CUuid InstanceId = {{
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
	}};
	CUuid Nonce = {{
		0x10, 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
	}};
	ProaledClientIndicator::WriteHeader(vPacket, ProaledClientIndicator::PACKET_DEV_AUTH);
	ProaledClientIndicator::WriteUuid(vPacket, InstanceId);
	ProaledClientIndicator::WriteUuid(vPacket, Nonce);
	ProaledClientIndicator::WriteU64(vPacket, 123);
	ProaledClientIndicator::WriteString(vPacket, "127.0.0.1:8303");
	ProaledClientIndicator::WriteString(vPacket, "developer");
	ProaledClientIndicator::WriteS16(vPacket, 7);
	ProaledClientIndicator::AppendHmacSha256(vPacket, "dev-secret");

	ProaledClientIndicator::CClientPresencePacket Packet;
	ASSERT_TRUE(ProaledClientIndicator::ReadDevAuthPacket(vPacket.data(), (int)vPacket.size(), Packet));
	EXPECT_EQ(Packet.m_Type, ProaledClientIndicator::PACKET_DEV_AUTH);
	EXPECT_EQ(Packet.m_ClientId, 7);
	EXPECT_EQ(Packet.m_ServerAddress, "127.0.0.1:8303");
	EXPECT_TRUE(ProaledClientIndicator::ValidateHmacSha256("dev-secret", vPacket.data(), (int)vPacket.size()));
	EXPECT_FALSE(ProaledClientIndicator::ValidateHmacSha256("wrong", vPacket.data(), (int)vPacket.size()));
}

TEST(ClientIndicator, VersionPacketProofRoundTrips)
{
	std::vector<uint8_t> vPacket;
	CUuid InstanceId = UUID_ZEROED;
	CUuid Nonce = UUID_ZEROED;
	Nonce.m_aData[0] = 7;
	ProaledClientIndicator::WriteHeader(vPacket, ProaledClientIndicator::PACKET_VERSION_ANNOUNCE);
	ProaledClientIndicator::WriteUuid(vPacket, InstanceId);
	ProaledClientIndicator::WriteUuid(vPacket, Nonce);
	ProaledClientIndicator::WriteU64(vPacket, 456);
	ProaledClientIndicator::WriteString(vPacket, "127.0.0.1:8303");
	ProaledClientIndicator::WriteString(vPacket, "user");
	ProaledClientIndicator::WriteS16(vPacket, 5);
	ProaledClientIndicator::WriteString(vPacket, "1.7.1");
	ProaledClientIndicator::AppendProof(vPacket, "shared");

	ProaledClientIndicator::CClientVersionPacket Packet;
	ASSERT_TRUE(ProaledClientIndicator::ReadClientVersionPacket(vPacket.data(), (int)vPacket.size(), Packet));
	EXPECT_TRUE(ProaledClientIndicator::ValidateProof("shared", vPacket.data(), (int)vPacket.size()));
	EXPECT_FALSE(ProaledClientIndicator::ValidateProof("wrong", vPacket.data(), (int)vPacket.size()));
	EXPECT_EQ(Packet.m_ClientId, 5);
	EXPECT_EQ(Packet.m_PlayerName, "user");
	EXPECT_EQ(Packet.m_ClientVersion, "1.7.1");
}
