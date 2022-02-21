//
// Created by stephane bourque on 2021-10-23.
//

#include "framework/MicroService.h"

#include "RESTAPI/RESTAPI_entity_handler.h"
#include "RESTAPI/RESTAPI_contact_handler.h"
#include "RESTAPI/RESTAPI_location_handler.h"
#include "RESTAPI/RESTAPI_venue_handler.h"
#include "RESTAPI/RESTAPI_inventory_handler.h"
#include "RESTAPI/RESTAPI_managementPolicy_handler.h"
#include "RESTAPI/RESTAPI_managementPolicy_list_handler.h"
#include "RESTAPI/RESTAPI_inventory_list_handler.h"
#include "RESTAPI/RESTAPI_entity_list_handler.h"
#include "RESTAPI/RESTAPI_configurations_handler.h"
#include "RESTAPI/RESTAPI_configurations_list_handler.h"
#include "RESTAPI/RESTAPI_webSocketServer.h"
#include "RESTAPI/RESTAPI_contact_list_handler.h"
#include "RESTAPI/RESTAPI_location_list_handler.h"
#include "RESTAPI/RESTAPI_venue_list_handler.h"
#include "RESTAPI/RESTAPI_managementRole_list_handler.h"
#include "RESTAPI/RESTAPI_map_handler.h"
#include "RESTAPI/RESTAPI_map_list_handler.h"
#include "RESTAPI_iptocountry_handler.h"
#include "RESTAPI/RESTAPI_signup_handler.h"

namespace OpenWifi {

    Poco::Net::HTTPRequestHandler * RESTAPI_ExtRouter(const char *Path, RESTAPIHandler::BindingMap &Bindings,
                                                            Poco::Logger & L, RESTAPI_GenericServer & S, uint64_t TransactionId) {
        return  RESTAPI_Router<
                    RESTAPI_system_command,
                    RESTAPI_entity_handler,
                    RESTAPI_entity_list_handler,
                    RESTAPI_contact_handler,
                    RESTAPI_contact_list_handler,
                    RESTAPI_location_handler,
                    RESTAPI_location_list_handler,
                    RESTAPI_venue_handler,
                    RESTAPI_venue_list_handler,
                    RESTAPI_inventory_handler,
                    RESTAPI_inventory_list_handler,
                    RESTAPI_managementPolicy_handler,
                    RESTAPI_managementPolicy_list_handler,
                    RESTAPI_managementRole_list_handler,
                    RESTAPI_configurations_handler,
                    RESTAPI_configurations_list_handler,
                    RESTAPI_map_handler,
                    RESTAPI_map_list_handler,
                    RESTAPI_webSocketServer,
                    RESTAPI_iptocountry_handler,
                    RESTAPI_signup_handler
                >(Path,Bindings,L, S, TransactionId);
    }

    Poco::Net::HTTPRequestHandler * RESTAPI_IntRouter(const char *Path, RESTAPIHandler::BindingMap &Bindings,
                                                            Poco::Logger & L, RESTAPI_GenericServer & S, uint64_t TransactionId) {
        return RESTAPI_Router_I<
                RESTAPI_system_command ,
                RESTAPI_inventory_handler,
                RESTAPI_configurations_handler,
                RESTAPI_configurations_list_handler,
                RESTAPI_iptocountry_handler,
                RESTAPI_signup_handler
        >(Path, Bindings, L, S, TransactionId);
    }
}