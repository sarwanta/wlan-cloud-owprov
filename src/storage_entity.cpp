//
// Created by stephane bourque on 2021-08-15.
//

#include "storage_entity.h"
#include "Utils.h"
#include "OpenWifiTypes.h"
#include "RESTAPI_utils.h"
#include "RESTAPI_SecurityObjects.h"

namespace OpenWifi {

    const std::string EntityDB::RootUUID_{"0000-0000-0000"};

    static  ORM::FieldVec    EntityDB_Fields{
        // object info
        ORM::Field{"id",64, true},
        ORM::Field{"name",ORM::FieldType::FT_TEXT},
        ORM::Field{"description",ORM::FieldType::FT_TEXT},
        ORM::Field{"notes",ORM::FieldType::FT_TEXT},
        ORM::Field{"created",ORM::FieldType::FT_BIGINT},
        ORM::Field{"modified",ORM::FieldType::FT_BIGINT},
        ORM::Field{"parent",ORM::FieldType::FT_TEXT},
        ORM::Field{"children",ORM::FieldType::FT_TEXT},
        ORM::Field{"contacts",ORM::FieldType::FT_TEXT},
        ORM::Field{"locations",ORM::FieldType::FT_TEXT},
        ORM::Field{"managementPolicy",ORM::FieldType::FT_TEXT},
        ORM::Field{"venues",ORM::FieldType::FT_TEXT},
        ORM::Field{"deviceConfiguration",ORM::FieldType::FT_TEXT}
    };

    static  ORM::IndexVec    EntityDB_Indexes{
                                { std::string("entity_name_index"),
                                  ORM::IndexEntryVec{
                                            {std::string("name"),
                                             ORM::Indextype::ASC} } }
                                };

    EntityDB::EntityDB( ORM::DBType T, Poco::Data::SessionPool & P, Poco::Logger &L) :
        DB(T, "entities", EntityDB_Fields, EntityDB_Indexes, P, L) {

        CheckForRoot();
    }

    inline bool EntityDB::CheckForRoot() {
        ProvObjects::Entity E;
        if(GetRecord("id",RootUUID(),E))
            RootExists_=true;

        return RootExists_;
    }

}

template<> void ORM::DB<    OpenWifi::EntityDBRecordType, OpenWifi::ProvObjects::Entity>::Convert(OpenWifi::EntityDBRecordType &In, OpenWifi::ProvObjects::Entity &Out) {
    Out.info.id = In.get<0>();
    Out.info.name = In.get<1>();
    Out.info.description = In.get<2>();
    Out.info.notes = OpenWifi::RESTAPI_utils::to_object_array<OpenWifi::SecurityObjects::NoteInfo>(In.get<3>());
    Out.info.created = In.get<4>();
    Out.info.modified = In.get<5>();
    Out.parent = In.get<6>();
    OpenWifi::Types::from_string(In.get<7>(), Out.children);
    OpenWifi::Types::from_string(In.get<8>(), Out.contacts);
    OpenWifi::Types::from_string(In.get<9>(), Out.locations);
    Out.managementPolicy = In.get<10>();
    OpenWifi::Types::from_string(In.get<11>(), Out.venues);
    Out.deviceConfiguration = In.get<12>();
}

template<> void ORM::DB<    OpenWifi::EntityDBRecordType, OpenWifi::ProvObjects::Entity>::Convert(OpenWifi::ProvObjects::Entity &In, OpenWifi::EntityDBRecordType &Out) {
    Out.set<0>(In.info.id);
    Out.set<1>(In.info.name);
    Out.set<2>(In.info.description);
    Out.set<3>(OpenWifi::RESTAPI_utils::to_string(In.info.notes));
    Out.set<4>(In.info.created);
    Out.set<5>(In.info.modified);
    Out.set<6>(In.parent);
    Out.set<7>(OpenWifi::Types::to_string(In.children));
    Out.set<8>(OpenWifi::Types::to_string(In.contacts));
    Out.set<9>(OpenWifi::Types::to_string(In.locations));
    Out.set<10>(In.managementPolicy);
    Out.set<11>(OpenWifi::Types::to_string(In.venues));
    Out.set<12>(In.deviceConfiguration);
}
