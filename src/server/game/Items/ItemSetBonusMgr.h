/*
 * Project Ambrose by Imjustchico
 * Every item set bonus template the user's install holds (sItemSetBonusMgr), read from the templates TemplateManifest.xml lists under ObjectData/ through the template store's manifest, decoded once on every hardware thread, each whose class is or derives from ItemSetBonusTemplate kept as a typed set bonus record, and swapped in as one checked set through the reload target item_set_bonus, which follows templates. A set in which a set bonus fails to load keeps the one serving and reports every set bonus that failed, grouped by what went wrong, and a caller holds the set it was given for as long as it needs it.
 */

#ifndef AMBROSE_ITEMSETBONUSMGR_H
#define AMBROSE_ITEMSETBONUSMGR_H

#include "ItemSetBonusStore.h"
#include "ReloadableStore.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

class ItemSetBonusMgr
{
public:
    static constexpr std::string_view Target = "item_set_bonus";
    static constexpr std::string_view Folder = "ObjectData/";

    static ItemSetBonusMgr& Instance();

    ItemSetBonusMgr() = default;
    ItemSetBonusMgr(ItemSetBonusMgr const&) = delete;
    ItemSetBonusMgr& operator=(ItemSetBonusMgr const&) = delete;

    void SetInstall(std::filesystem::path root);
    void RegisterReloadTargets();
    bool Load(std::vector<std::string>& errors);
    std::shared_ptr<ItemSetBonusStore const> GetSetBonuses() const { return _setBonuses.Get(); }
    uint64 GetGeneration() const noexcept { return _setBonuses.GetGeneration(); }
    void Clear();

private:
    std::filesystem::path GameData() const;

    mutable std::mutex _installMutex;
    std::filesystem::path _install;
    ReloadableStore<ItemSetBonusStore> _setBonuses;
};

#define sItemSetBonusMgr ItemSetBonusMgr::Instance()

#endif