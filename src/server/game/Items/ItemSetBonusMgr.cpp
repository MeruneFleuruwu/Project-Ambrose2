/*
 * Project Ambrose by Imjustchico
 * Reads the item set bonus templates from the user's install in name order as a BINd TemplateManifest, keeps each set bonus whose class is or derives from ItemSetBonusTemplate, validates them whole as one set, and swaps the store in so a reader keeps the set it started with, a build that fails keeping the set that was serving and naming every set bonus at fault, and reloading on its own.
 */

#include "ItemSetBonusMgr.h"
#include "ConfigMgr.h"
#include "ItemTemplateRecord.h"
#include "KiwadArchive.h"
#include "Log.h"
#include "ReloadMgr.h"
#include "TemplateExtractor.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <chrono>
#include <map>
#include <utility>

namespace
{
    constexpr char const* SetBonusLog = "server.loading";

    std::vector<std::string> SummarizeFailures(std::vector<ItemSetBonusRecord> const& all, std::vector<std::string> const& errors)
    {
        std::map<std::string, std::size_t> byProblem;
        for (std::string const& error : errors)
        {
            std::size_t const colon = error.find(':');
            std::string const key = colon != std::string::npos ? error.substr(0, colon) : error;
            ++byProblem[key];
        }
        std::vector<std::string> lines;
        for (auto const& [problem, count] : byProblem)
            lines.push_back(fmt::format("  {} set bonus(es): {}", count, problem));
        return lines;
    }
}

ItemSetBonusMgr& ItemSetBonusMgr::Instance()
{
    static ItemSetBonusMgr instance;
    return instance;
}

std::filesystem::path ItemSetBonusMgr::GameData() const
{
    std::lock_guard const lock(_installMutex);
    return _install / "Data" / "GameData";
}

void ItemSetBonusMgr::SetInstall(std::filesystem::path root)
{
    std::lock_guard const lock(_installMutex);
    _install = std::move(root);
}

void ItemSetBonusMgr::RegisterReloadTargets()
{
    sReloadMgr.Register(std::string(Target), [this](std::vector<std::string>& errors) { return Load(errors); });
}

bool ItemSetBonusMgr::Load(std::vector<std::string>& errors)
{
    std::filesystem::path install;
    {
        std::lock_guard const lock(_installMutex);
        install = _install;
    }
    if (install.empty())
    {
        errors.emplace_back("no Wizard101 install is in use, so no item set bonus can be read");
        return false;
    }
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    if (!catalog)
    {
        errors.emplace_back("no type dump is loaded, so no item set bonus can be read");
        return false;
    }
    auto const started = std::chrono::steady_clock::now();
    std::string error;
    std::optional<TemplateExtraction> const extraction = TemplateExtractor::ExtractFromInstall(install, ConfigMgr::TypeDumpPath, error);
    if (!extraction)
    {
        errors.push_back(fmt::format("cannot extract templates from install: {}", error));
        return false;
    }
    std::vector<ItemSetBonusRecord> records;
    records.reserve(extraction->GetSetBonusCount());
    for (ExtractedTemplate const& found : extraction->Templates)
        if (found.SetBonus)
            records.push_back(std::move(*found.SetBonus));
    std::shared_ptr<ItemSetBonusStore const> store = ItemSetBonusStore::Build(std::move(records));
    if (!store)
        return false;
    _setBonuses.Replace(std::move(store));
    auto const took = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    LOG_INFO(SetBonusLog, "Read {} item set bonus templates from {} in {} ms", _setBonuses.Get()->Size(), Folder, took.count());
    return true;
}

void ItemSetBonusMgr::Clear()
{
    _setBonuses.Replace(std::make_shared<ItemSetBonusStore const>());
}