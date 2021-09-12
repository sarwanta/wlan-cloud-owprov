//
// Created by stephane bourque on 2021-08-23.
//

#include "RESTAPI_location_list_handler.h"
#include "Utils.h"
#include "RESTAPI_ProvObjects.h"
#include "StorageService.h"
#include "RESTAPI_errors.h"

namespace OpenWifi{

    void RESTAPI_location_list_handler::DoGet() {
        if(!QB_.Select.empty()) {
            auto DevUUIDS = Utils::Split(QB_.Select);
            ProvObjects::LocationVec Locations;
            for(const auto &i:DevUUIDS) {
                ProvObjects::Location E;
                if(Storage()->LocationDB().GetRecord("id",i,E)) {
                    Locations.push_back(E);
                } else {
                    BadRequest(RESTAPI::Errors::UnknownId + " ("+i+")");
                    return;
                }
            }
            ReturnObject("locations", Locations);
            return;
        } else if(QB_.CountOnly) {
            Poco::JSON::Object  Answer;
            auto C = Storage()->LocationDB().Count();
            ReturnCountOnly(C);
            return;
        } else {
            ProvObjects::LocationVec Locations;
            Storage()->LocationDB().GetRecords(QB_.Offset,QB_.Limit,Locations);
            ReturnObject("locations", Locations);
            return;
        }
    }
}