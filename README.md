# mod-profession-craft-cd

Removes **profession craft** cooldowns for Classic, TBC, and WotLK recipes
(Mooncloth, Arcanite, specialty cloths, alchemy transmutes/research, inscription
research, JC prisms, enchanting spheres, Glacial Bag, **Salt Shaker**, etc.).

## Purpose / scope

| Layer | Role |
|-------|------|
| Module SQL | Sets `spell_cooldown_overrides` RecoveryTime / CategoryRecoveryTime to **0** for allowlisted craft spells; zeros Salt Shaker `item_template` CDs |
| `ProfessionCraftCd.Enabled` | Pushes the zero CD to the 3.3.5 client after each craft (see below) |
| Core `SkillGain.Crafting = 3` | Companion skill-up rate — set in `worldserver.conf` (not this module) |

**Does not** change:

- Wormhole / engineering gadget uses
- Hearthstone, potions, combat spells
- Gathering professions

## Client cooldown

Zeroing `spell_cooldown_overrides` only changes the server. The 3.3.5 client
reads its own **Spell.dbc**, starts the 20 h (category 310) timer when the craft
lands (`SMSG_SPELL_GO`) and greys out the recipe, including every other
category-310 transmute.

With `ProfessionCraftCd.Enabled = 1`, after each craft whose override is 0/0 the
module sends:

1. `SMSG_SPELL_COOLDOWN` for the crafted spell with a **1 ms** cooldown (0 would
   mean "use Spell.dbc"), replacing the client timer
2. `SMSG_CLEAR_COOLDOWN` for each known spell in the same category, skipping any
   that still has a real server-side cooldown

It sends both right after `SMSG_SPELL_GO` and again 150 ms later. No core patch is
needed: this uses `SpellMgr::HasSpellCooldownOverride` and stock `Player` packet
helpers.

### Optional: patched Spell.dbc (tooltips)

Tooltips still say "20 hr cooldown" because they come from the client's
Spell.dbc. To fix them, zero the same spells in a client DBC:

```bash
scripts/patch-profession-craft-dbc.sh /path/to/azerothcore/data/dbc/Spell.dbc
```

The script reads the spell IDs from this module's SQL and writes
`DBFilesClient/Spell.dbc`. Pack it into a client patch MPQ (e.g.
`Data/patch-P.MPQ`) as `DBFilesClient\Spell.dbc`, then delete the client's
`Cache/` folder so item tooltips (Salt Shaker) refresh. If another client patch
already ships a Spell.dbc, patch that file, because only the newest MPQ's
Spell.dbc is used.

## Allowlist (summary)

- **Classic:** Mooncloth, Transmute Arcanite / Elemental Fire, and period transmutes; Salt Shaker (item 15846 / spell 19566)
- **TBC:** Primal Mooncloth / Spellcloth / Shadowcloth; Primal Might; Earthstorm / Skyfire diamonds; primal elemental transmutes
- **WotLK (still CD in stock 3.3.5 DBC):** Alchemy category-310 transmutes + Eternal Might + epic gem transmutes; Northrend Alchemy Research; Minor / Northrend Inscription Research; Brilliant Glass / Icy Prism; Prismatic / Void Sphere; Glacial Bag

Moonshroud / Ebonweave / Spellweave are already zero in stock 3.3.5 DBC.

Also forces Classic/TBC historical crafts (and Salt Shaker item CDs) to zero so
optional Individual Progression `zz_optional_restore_crafting_cd_timers.sql`
cannot re-enable them if applied.

## Configuration

See `conf/professionCraftCd.conf.dist`:

| Key | Default | Meaning |
|-----|---------|---------|
| `ProfessionCraftCd.Enabled` | 1 | Push zero craft CDs to the client after each craft |

## Install

```bash
cd modules
git clone https://github.com/buildthehomelab/wow-mod-profession-craft-cd.git mod-profession-craft-cd
```

Clone into `mod-profession-craft-cd` (no `wow-` prefix): AzerothCore derives the
loader symbol from the folder name. Fork of
[VenomekPL/mod-profession-craft-cd](https://github.com/VenomekPL/mod-profession-craft-cd).

Companion core setting:

```
SkillGain.Crafting = 3
```

Reload: apply module world SQL (worldserver updater), then **restart** worldserver
(`spell_cooldown_overrides` and `item_template` load at startup).

## License

MIT (AzerothCore module skeleton)
