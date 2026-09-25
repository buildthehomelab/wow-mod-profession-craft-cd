#include "AllSpellScript.h"
#include "Config.h"
#include "DBCStores.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "WorldPacket.h"

namespace
{
    bool sEnabled = true;

    // Only overrides that zero the CD; other spell_cooldown_overrides rows keep their timers.
    bool HasZeroCooldownOverride(uint32 spellId)
    {
        if (!sSpellMgr->HasSpellCooldownOverride(spellId))
            return false;

        SpellCooldownOverride const cd = sSpellMgr->GetSpellCooldownOverride(spellId);
        return !cd.RecoveryTime && !cd.CategoryRecoveryTime;
    }

    // The 3.3.5 client starts the Spell.dbc Recovery / CategoryRecovery timer on SPELL_GO
    // and never looks at spell_cooldown_overrides. A 1 ms SMSG_SPELL_COOLDOWN replaces the
    // cast spell's timer (0 would mean "use Spell.dbc"), and SMSG_CLEAR_COOLDOWN drops the
    // category CD the client put on siblings (all category-310 transmutes share one).
    void PushZeroCooldownToClient(Player* player, SpellInfo const* spellInfo)
    {
        WorldPacket data;
        player->BuildCooldownPacket(data, SPELL_COOLDOWN_FLAG_NONE, spellInfo->Id, 1);
        player->SendDirectMessage(&data);

        uint32 const category = spellInfo->GetCategory();
        if (!category)
            return;

        SpellCategoryStore::const_iterator itr = sSpellsByCategoryStore.find(category);
        if (itr == sSpellsByCategoryStore.end())
            return;

        for (auto const& [byItem, siblingId] : itr->second)
        {
            if (!byItem && !player->HasSpell(siblingId))
                continue;

            // Keep a real server-side CD (sibling outside the allowlist) visible on the client.
            if (player->HasSpellCooldown(siblingId))
                continue;

            player->SendClearCooldown(siblingId, player);
        }
    }
}

class ProfessionCraftCd_World : public WorldScript
{
public:
    ProfessionCraftCd_World() : WorldScript("ProfessionCraftCd_World") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sEnabled = sConfigMgr->GetOption<bool>("ProfessionCraftCd.Enabled", true);

        if (sEnabled)
            LOG_INFO("server.loading",
                "ProfessionCraftCd: pushing zero spell_cooldown_overrides to the client after crafts");
    }
};

class ProfessionCraftCd_AllSpell : public AllSpellScript
{
public:
    ProfessionCraftCd_AllSpell() : AllSpellScript("ProfessionCraftCd_AllSpell", { ALLSPELLHOOK_ON_CAST }) { }

    void OnSpellCast(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        if (!sEnabled || !caster || !caster->IsPlayer() || !spellInfo)
            return;
        if (!HasZeroCooldownOverride(spellInfo->Id))
            return;

        Player* player = caster->ToPlayer();
        PushZeroCooldownToClient(player, spellInfo);

        // Runs after SendSpellGo, but push again once the client has applied SPELL_GO.
        ObjectGuid const guid = player->GetGUID();
        uint32 const spellId = spellInfo->Id;
        player->m_Events.AddEventAtOffset([guid, spellId]()
        {
            Player* p = ObjectAccessor::FindConnectedPlayer(guid);
            if (!p)
                return;
            if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId))
                PushZeroCooldownToClient(p, info);
        }, 150ms);
    }
};

void AddProfessionCraftCdScripts()
{
    new ProfessionCraftCd_World();
    new ProfessionCraftCd_AllSpell();
}
