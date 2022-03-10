//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//

#include "framework/MicroService.h"

#include "RESTAPI_configurations_handler.h"
#include "RESTObjects/RESTAPI_ProvObjects.h"
#include "StorageService.h"
#include "framework/ConfigurationValidator.h"
#include "RESTAPI/RESTAPI_db_helpers.h"
#include "DeviceTypeCache.h"

namespace OpenWifi{

    void RESTAPI_configurations_handler::DoGet() {
        std::string UUID = GetBinding("uuid","");
        ProvObjects::DeviceConfiguration   Existing;
        if(UUID.empty() || !DB_.GetRecord("id", UUID, Existing)) {
            return NotFound();
        }

        Poco::JSON::Object  Answer;
        std::string Arg;
        if(HasParameter("expandInUse",Arg) && Arg=="true") {
            Storage::ExpandedListMap    M;
            std::vector<std::string>    Errors;
            Poco::JSON::Object          Inner;
            if(StorageService()->ExpandInUse(Existing.inUse,M,Errors)) {
                for(const auto &[type,list]:M) {
                    Poco::JSON::Array   ObjList;
                    for(const auto &i:list.entries) {
                        Poco::JSON::Object  O;
                        i.to_json(O);
                        ObjList.add(O);
                    }
                    Inner.set(type,ObjList);
                }
            }
            Answer.set("entries", Inner);
            return ReturnObject(Answer);
        } else if(HasParameter("computedAffected",Arg) && Arg=="true") {
            Types::UUIDvec_t DeviceSerialNumbers;
            DB_.GetListOfAffectedDevices(UUID,DeviceSerialNumbers);
            return ReturnObject("affectedDevices", DeviceSerialNumbers);
        } else if(QB_.AdditionalInfo) {
            AddExtendedInfo(Existing,Answer);
        }
        Existing.to_json(Answer);
        ReturnObject(Answer);
    }

    void RESTAPI_configurations_handler::DoDelete() {
        std::string UUID = GetBinding("uuid","");
        ProvObjects::DeviceConfiguration   Existing;
        if(UUID.empty() || !DB_.GetRecord("id", UUID, Existing)) {
            return NotFound();
        }

        if(!Existing.inUse.empty()) {
            return BadRequest(RESTAPI::Errors::StillInUse);
        }

        DB_.DeleteRecord("id", UUID);
        MoveUsage(StorageService()->PolicyDB(),DB_,Existing.managementPolicy,"",Existing.info.id);
        RemoveMembership(StorageService()->VenueDB(),&ProvObjects::Venue::configurations,Existing.venue,Existing.info.id);
        RemoveMembership(StorageService()->EntityDB(),&ProvObjects::Entity::configurations,Existing.entity,Existing.info.id);
        for(const auto &i:Existing.variables)
            RemoveMembership(StorageService()->VariablesDB(),&ProvObjects::VariableBlock::configurations,i,Existing.info.id);

        return OK();
    }

    bool RESTAPI_configurations_handler::ValidateConfigBlock(const ProvObjects::DeviceConfiguration &Config, std::string & Error) {
        static const std::vector<std::string> SectionNames{ "globals", "interfaces", "metrics", "radios", "services", "unit" };

        for(const auto &i:Config.configuration) {
            Poco::JSON::Parser  P;
            if(i.name.empty()) {
                std::cout << "Name is empty" << std::endl;
                BadRequest(RESTAPI::Errors::NameMustBeSet);
                return false;
            }

            try {
                auto Blocks = P.parse(i.configuration).extract<Poco::JSON::Object::Ptr>();
                auto N = Blocks->getNames();
                for (const auto &j: N) {
                    if (std::find(SectionNames.cbegin(), SectionNames.cend(), j) == SectionNames.cend()) {
                        BadRequest(RESTAPI::Errors::ConfigBlockInvalid);
                        return false;
                    }
                }
            } catch (const Poco::JSON::JSONException &E ) {
                Error = "Block: " + i.name + " failed parsing: " + E.message();
                return false;
            }

            try {
                if (ValidateUCentralConfiguration(i.configuration, Error)) {
                    // std::cout << "Block: " << i.name << " is valid" << std::endl;
                } else {
                    Error =  "Block: " + i.name + "  Rejected config:" + i.configuration ;
                    return false;
                }
            } catch(...) {
                std::cout << "Exception in validation" << std::endl;
                return false;
            }

        }
        return true;
    }

#define __DBG__ std::cout << __LINE__ << std::endl;

    void RESTAPI_configurations_handler::DoPost() {
        __DBG__
        auto UUID = GetBinding("uuid","");
        if(UUID.empty()) {
            return BadRequest(RESTAPI::Errors::MissingUUID);
        }

        std::string Arg;
        __DBG__
        if(HasParameter("validateOnly",Arg) && Arg=="true") {
            auto Body = ParseStream();
            if(!Body->has("configuration")) {
                return BadRequest("Must have 'configuration' element.");
            }
            auto Config=Body->get("configuration").toString();
            Poco::JSON::Object  Answer;
            std::string Error;
            auto Res = ValidateUCentralConfiguration(Config,Error);
            Answer.set("valid",Res);
            Answer.set("error", Error);
            return ReturnObject(Answer);
        }

        __DBG__
        ProvObjects::DeviceConfiguration NewObject;
        auto RawObject = ParseStream();
        __DBG__
        if (!NewObject.from_json(RawObject)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        __DBG__
        if(!ProvObjects::CreateObjectInfo(RawObject,UserInfo_.userinfo,NewObject.info)) {
            return BadRequest(RESTAPI::Errors::NameMustBeSet);
        }

        __DBG__
        if(!NewObject.entity.empty() && !StorageService()->EntityDB().Exists("id",NewObject.entity)) {
            return BadRequest(RESTAPI::Errors::EntityMustExist);
        }

        __DBG__
        if(!NewObject.venue.empty() && !StorageService()->VenueDB().Exists("id",NewObject.venue)) {
            return BadRequest(RESTAPI::Errors::VenueMustExist);
        }

        __DBG__
        if(!NewObject.managementPolicy.empty() && !StorageService()->PolicyDB().Exists("id",NewObject.managementPolicy)) {
            return BadRequest(RESTAPI::Errors::UnknownManagementPolicyUUID);
        }

        __DBG__
        NewObject.inUse.clear();
        if(NewObject.deviceTypes.empty() || !DeviceTypeCache()->AreAcceptableDeviceTypes(NewObject.deviceTypes, true)) {
            return BadRequest(RESTAPI::Errors::InvalidDeviceTypes);
        }

        std::string Error;
        __DBG__
        if(!ValidateConfigBlock(NewObject,Error)) {
            return BadRequest(RESTAPI::Errors::ConfigBlockInvalid + ", error: " + Error);
        }
        __DBG__

        if(DB_.CreateRecord(NewObject)) {
            __DBG__
            MoveUsage(StorageService()->PolicyDB(),DB_,"",NewObject.managementPolicy,NewObject.info.id);
            __DBG__
            AddMembership(StorageService()->VenueDB(),&ProvObjects::Venue::configurations,NewObject.venue, NewObject.info.id);
            __DBG__
            AddMembership(StorageService()->EntityDB(),&ProvObjects::Entity::configurations,NewObject.entity, NewObject.info.id);
            __DBG__

            ConfigurationDB::RecordName AddedRecord;
            DB_.GetRecord("id", NewObject.info.id, AddedRecord);
            Poco::JSON::Object  Answer;
            AddedRecord.to_json(Answer);
            __DBG__
            return ReturnObject(Answer);
        }
        __DBG__
        InternalError(RESTAPI::Errors::RecordNotCreated);
    }

    void RESTAPI_configurations_handler::DoPut() {
        auto UUID = GetBinding("uuid","");
        ProvObjects::DeviceConfiguration    Existing;
        if(UUID.empty() || !DB_.GetRecord("id", UUID, Existing)) {
            return NotFound();
        }

        ProvObjects::DeviceConfiguration    NewConfig;
        auto RawObject = ParseStream();
        if (!NewConfig.from_json(RawObject)) {
            return BadRequest(RESTAPI::Errors::InvalidJSONDocument);
        }

        if(!UpdateObjectInfo(RawObject, UserInfo_.userinfo, Existing.info)) {
            return BadRequest(RESTAPI::Errors::NameMustBeSet);
        }

        if(!NewConfig.deviceTypes.empty() && !DeviceTypeCache()->AreAcceptableDeviceTypes(NewConfig.deviceTypes, true)) {
            return BadRequest(RESTAPI::Errors::InvalidDeviceTypes);
        }

        if(!NewConfig.deviceTypes.empty())
            Existing.deviceTypes = NewConfig.deviceTypes;

        std::string Error;
        if(!ValidateConfigBlock( NewConfig,Error)) {
            return BadRequest(RESTAPI::Errors::ConfigBlockInvalid + ", error: " + Error);
        }

        if(RawObject->has("configuration")) {
            Existing.configuration = NewConfig.configuration;
        }

        std::string FromPolicy, ToPolicy;
        if(!CreateMove(RawObject,"managementPolicy",&ConfigurationDB::RecordName::managementPolicy, Existing, FromPolicy, ToPolicy, StorageService()->PolicyDB()))
            return BadRequest(RESTAPI::Errors::EntityMustExist);

        std::string FromEntity, ToEntity;
        if(!CreateMove(RawObject,"entity",&ConfigurationDB::RecordName::entity, Existing, FromEntity, ToEntity, StorageService()->EntityDB()))
            return BadRequest(RESTAPI::Errors::EntityMustExist);

        std::string FromVenue, ToVenue;
        if(!CreateMove(RawObject,"venue",&ConfigurationDB::RecordName::venue, Existing, FromVenue, ToVenue, StorageService()->VenueDB()))
            return BadRequest(RESTAPI::Errors::VenueMustExist);

        Types::UUIDvec_t FromVariables, ToVariables;
        if(RawObject->has("variables")) {
            for(const auto &i:NewConfig.variables) {
                if(!i.empty() && !StorageService()->VariablesDB().Exists("id",i)) {
                    return BadRequest(RESTAPI::Errors::VariableMustExist);
                }
            }
            for(const auto &i:Existing.variables)
                FromVariables.emplace_back(i);
            for(const auto &i:NewConfig.variables)
                ToVariables.emplace_back(i);
            FromVariables = Existing.variables;
            ToVariables = NewConfig.variables;
            Existing.variables = ToVariables;
        }

        AssignIfPresent(RawObject,  "rrm", Existing.rrm);
        AssignIfPresent(RawObject,  "firmwareUpgrade",Existing.firmwareUpgrade);
        AssignIfPresent(RawObject,  "firmwareRCOnly", Existing.firmwareRCOnly);

        if(DB_.UpdateRecord("id",UUID,Existing)) {
            ManageMembership(StorageService()->VariablesDB(),&ProvObjects::VariableBlock::configurations, FromVariables, ToVariables, Existing.info.id);
            ManageMembership(StorageService()->VenueDB(), &ProvObjects::Venue::configurations, FromVenue, ToVenue, Existing.info.id);
            ManageMembership(StorageService()->EntityDB(), &ProvObjects::Entity::configurations, FromEntity, ToEntity, Existing.info.id);
            MoveUsage(StorageService()->PolicyDB(),DB_,FromPolicy,ToPolicy,Existing.info.id);

            ProvObjects::DeviceConfiguration    D;
            DB_.GetRecord("id",UUID,D);
            Poco::JSON::Object  Answer;
            D.to_json(Answer);
            return ReturnObject(Answer);
        }
        InternalError(RESTAPI::Errors::RecordNotUpdated);
    }
}