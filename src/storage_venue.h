//
//	License type: BSD 3-Clause License
//	License copy: https://github.com/Telecominfraproject/wlan-cloud-ucentralgw/blob/master/LICENSE
//
//	Created by Stephane Bourque on 2021-03-04.
//	Arilia Wireless Inc.
//


#ifndef OWPROV_STORAGE_VENUE_H
#define OWPROV_STORAGE_VENUE_H

#include "orm.h"
#include "RESTAPI_ProvObjects.h"

namespace OpenWifi {
    typedef Poco::Tuple<
        std::string,
        std::string,
        std::string,
        std::string,
        uint64_t,
        uint64_t,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string,
        std::string
    > VenueDBRecordType;

    class VenueDB : public ORM::DB<VenueDBRecordType, ProvObjects::Venue> {
    public:
        VenueDB( ORM::DBType T, Poco::Data::SessionPool & P, Poco::Logger &L);
        bool CreateShortCut(ProvObjects::Venue &V);
        bool GetByIP(const std::string &IP, std::string & uuid);
    private:
    };
}



#endif //OWPROV_STORAGE_VENUE_H
