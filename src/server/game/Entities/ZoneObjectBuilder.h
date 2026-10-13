/*
 * Project Ambrose by Imjustchico
 * Builds the game object a zone places, the one MSG_NEWOBJECT carries: the class its template's class makes the client build, the CoreObject header of that template, the ids the zone instance gave it, where the zone data stands it, faces it and how large it is, and one behavior for each its template names in the template's own order, built as the class the client makes for that behavior or left empty where the client takes it empty; a placement that carries a header of its own takes that header instead of the one its template's class gives, which the pet takes, because the same pet template travels as an item among the wizard's things and as a pet beside it, and the two are not the same core type.
 */

#ifndef AMBROSE_ZONEOBJECTBUILDER_H
#define AMBROSE_ZONEOBJECTBUILDER_H

#include "CoreObjectSerializer.h"
#include "ObjectSchemaMgr.h"
#include "ObjectTemplateMgr.h"
#include "PropertyValue.h"

#include <optional>
#include <string>

struct ZoneObjectPlacement
{
    uint64 GlobalId = 0;
    uint64 PermId = 0;
    uint16 MobileId = 0;
    PropertyTypes::Vector3D Location;
    PropertyTypes::Vector3D Orientation;
    float Scale = 1.0f;
    std::optional<CoreObjectHeader> CoreHeader = {};
};

class ZoneObjectBuilder
{
public:
    ZoneObjectBuilder() = delete;

    static PropertyObjectPtr Build(TypeCatalogPtr const& catalog, CoreObjectTypeTable const& types, BehaviorClientClasses const& behaviors, ObjectTemplate const& objectTemplate,
        ZoneObjectPlacement const& placement, std::string& problem);
};

#endif
