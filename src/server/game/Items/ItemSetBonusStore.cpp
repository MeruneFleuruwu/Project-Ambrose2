/*
 * Project Ambrose by Imjustchico
 * Builds and serves the item set bonus store: folds names for case-insensitive search, counts memory and unknown behaviors, and exposes lookups by id, name and text.
 */

#include "ItemSetBonusStore.h"

#include <algorithm>
#include <cctype>
#include <fmt/format.h>

#include <utility>

namespace
{
    std::string Fold(std::string_view text)
    {
        std::string folded;
        folded.reserve(text.size());
        for (char c : text)
            folded.push_back(std::tolower(static_cast<unsigned char>(c)));
        return folded;
    }

    std::size_t RecordMemory(ItemSetBonusRecord const& record)
    {
        std::size_t bytes = sizeof(ItemSetBonusRecord) + record.File.capacity() + record.ObjectName.capacity() + record.DisplayKey.capacity();
        for (ItemSetBonusTier const& tier : record.Tiers)
        {
            bytes += sizeof(ItemSetBonusTier) + tier.Description.capacity();
            if (tier.Requirements)
                bytes += tier.Requirements->Requirements.capacity() * sizeof(ItemTemplatePart);
            bytes += tier.Effects.capacity() * sizeof(ItemTemplatePart);
            for (ItemTemplatePart const& part : tier.Effects)
                bytes += part.ClassName.capacity() + (part.Object ? sizeof(PropertyObject) + part.Object->GetClass().Properties.size() * sizeof(PropertyValue) : 0);
        }
        return bytes;
    }
}

std::shared_ptr<ItemSetBonusStore const> ItemSetBonusStore::Build(std::vector<ItemSetBonusRecord> records)
{
    auto store = std::make_shared<ItemSetBonusStore>();
    std::sort(records.begin(), records.end(), [](ItemSetBonusRecord const& a, ItemSetBonusRecord const& b) { return a.TemplateId < b.TemplateId; });
    store->_records = std::move(records);
    store->_records.shrink_to_fit();
    store->_byId.reserve(store->_records.size());
    store->_byFoldedName.reserve(store->_records.size());
    for (std::size_t i = 0; i < store->_records.size(); ++i)
    {
        ItemSetBonusRecord const& record = store->_records[i];
        store->_byId.emplace(record.TemplateId, i);
        if (!record.ObjectName.empty())
            store->_byFoldedName.emplace(Fold(record.ObjectName), i);
        store->_memory += RecordMemory(record);
    }
    return store;
}

ItemSetBonusRecord const* ItemSetBonusStore::Find(uint32 templateId) const noexcept
{
    auto const it = _byId.find(templateId);
    return it != _byId.end() ? &_records[it->second] : nullptr;
}

ItemSetBonusRecord const* ItemSetBonusStore::FindByName(std::string_view name) const
{
    auto const it = _byFoldedName.find(Fold(name));
    return it != _byFoldedName.end() ? &_records[it->second] : nullptr;
}

ItemSetBonusRecord const* ItemSetBonusStore::FindByIdOrName(std::string_view text) const
{
    if (text.empty())
        return nullptr;
    if (std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
        return Find(static_cast<uint32>(std::stoul(std::string(text))));
    return FindByName(text);
}

std::vector<ItemSetBonusRecord const*> ItemSetBonusStore::Search(std::string_view text) const
{
    std::string const folded = Fold(text);
    std::vector<ItemSetBonusRecord const*> results;
    for (ItemSetBonusRecord const& record : _records)
        if (Fold(record.ObjectName).find(folded) != std::string::npos)
            results.push_back(&record);
    return results;
}

std::map<std::string, std::size_t> ItemSetBonusStore::CountByClass() const
{
    std::map<std::string, std::size_t> counts;
    for (ItemSetBonusRecord const& record : _records)
        ++counts[record.ClassName];
    return counts;
}

std::size_t ItemSetBonusStore::CountUnknownBehaviors() const noexcept
{
    std::size_t count = 0;
    for (ItemSetBonusRecord const& record : _records)
        count += record.UnknownBehaviors;
    return count;
}