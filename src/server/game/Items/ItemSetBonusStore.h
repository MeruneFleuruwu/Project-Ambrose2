/*
 * Project Ambrose by Imjustchico
 * One generation of the item set bonus templates the install holds, in template id order: found by id, by object name whatever its case, where two names differing only in case keep the lower id, by whichever of id or name a command was given, and searched by the text an object name holds, with the memory its records take and how many there are.
 */

#ifndef AMBROSE_ITEMSETBONUSSTORE_H
#define AMBROSE_ITEMSETBONUSSTORE_H

#include "ItemTemplateRecord.h"
#include "Types.h"

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class ItemSetBonusStore
{
public:
    ItemSetBonusStore() = default;
    ItemSetBonusStore(ItemSetBonusStore const&) = delete;
    ItemSetBonusStore& operator=(ItemSetBonusStore const&) = delete;

    static std::shared_ptr<ItemSetBonusStore const> Build(std::vector<ItemSetBonusRecord> records);

    ItemSetBonusRecord const* Find(uint32 templateId) const noexcept;
    ItemSetBonusRecord const* FindByName(std::string_view name) const;
    ItemSetBonusRecord const* FindByIdOrName(std::string_view text) const;
    std::vector<ItemSetBonusRecord const*> Search(std::string_view text) const;

    std::vector<ItemSetBonusRecord> const& GetAll() const noexcept { return _records; }
    std::size_t Size() const noexcept { return _records.size(); }
    std::size_t GetMemoryUsage() const noexcept { return _memory; }
    std::map<std::string, std::size_t> CountByClass() const;
    std::size_t CountUnknownBehaviors() const noexcept;

private:
    std::vector<ItemSetBonusRecord> _records;
    std::unordered_map<uint32, std::size_t> _byId;
    std::unordered_map<std::string, std::size_t> _byFoldedName;
    std::size_t _memory = 0;
};

#endif