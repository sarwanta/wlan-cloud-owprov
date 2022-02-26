//
// Created by stephane bourque on 2022-02-20.
//

#include "RESTAPI_signup_handler.h"
#include "StorageService.h"
#include "Signup.h"

namespace OpenWifi {

    void RESTAPI_signup_handler::DoPost() {
        auto UserName = GetParameter("email","");
        Poco::toLowerInPlace(UserName);
        auto SerialNumber = GetParameter("serialNumber","");
        Poco::toLowerInPlace(SerialNumber);

        if(UserName.empty() || SerialNumber.empty()) {
            return BadRequest(RESTAPI::Errors::MissingOrInvalidParameters);
        }

        if(!Utils::ValidEMailAddress(UserName)) {
            return BadRequest(RESTAPI::Errors::InvalidEmailAddress);
        }

        if(!Utils::ValidSerialNumber(SerialNumber)) {
            return BadRequest(RESTAPI::Errors::InvalidSerialNumber);
        }

        //  if a signup already exists for this user, we should just return its value completion
        SignupDB::RecordVec SEs;
        if(StorageService()->SignupDB().GetRecords(0,100, SEs, " email='" + UserName + "' and serialNumber='"+SerialNumber+"' ")) {
            for(const auto &i:SEs) {
                if (i.statusCode == ProvObjects::SignupStatusCodes::SignupWaitingForEmail ||
                    i.statusCode == ProvObjects::SignupStatusCodes::SignupWaitingForDevice ||
                    i.statusCode == ProvObjects::SignupStatusCodes::SignupSuccess ) {
                    Logger().information(Poco::format("SIGNUP: Returning existing signup record for '%s'",i.email));
                    Poco::JSON::Object Answer;
                    i.to_json(Answer);
                    return ReturnObject(Answer);
                }
            }
        }

        //  So we do not have an outstanding signup...
        //  Can we actually claim this serial number??? if not, we need to return an error
        ProvObjects::InventoryTag   IT;
        if(StorageService()->InventoryDB().GetRecord("serialNumber",SerialNumber,IT)) {
            if(!IT.subscriber.empty()) {
                return BadRequest(RESTAPI::Errors::SerialNumberAlreadyProvisioned);
            }

            if(!(IT.devClass.empty() || IT.devClass=="subscriber" || IT.devClass=="any")) {
                return BadRequest(RESTAPI::Errors::SerialNumberNotTheProperClass);
            }
        }

        //  OK, we can claim this device, can we create a userid?
        //  Let's create one
        //  If sec.signup("email",uuid);
        auto SignupUUID = MicroService::instance().CreateUUID();
        Logger().information(Poco::format("SIGNUP: Creating signup entry for '%s', uuid=%s",UserName, SignupUUID));

        Poco::JSON::Object  Body;
        OpenAPIRequestPost  CreateUser( uSERVICE_SECURITY, "/api/v1/signup", {
                { "email", UserName },
                { "signupUUID" , SignupUUID }
        }, Body, 30000);

        Poco::JSON::Object::Ptr Answer;
        if(CreateUser.Do(Answer) == Poco::Net::HTTPServerResponse::HTTP_OK) {
            SecurityObjects::UserInfo   UI;

            UI.from_json(Answer);
            std::ostringstream os;
            Answer->stringify(os);
            Logger().information(Poco::format("SIGNUP: email: '%s' signupID: '%s' userId: '%s'", UserName, SignupUUID, UI.id));

            //  so create the Signup entry and modify the inventory
            ProvObjects::SignupEntry    SE;
            SE.info.id = SignupUUID;
            SE.info.created = SE.info.modified = SE.submitted = OpenWifi::Now();
            SE.completed = 0 ;
            SE.serialNumber = SerialNumber;
            SE.error = 0 ;
            SE.userId = UI.id;
            SE.email = UserName;
            SE.status = "waiting-for-email-verification";
            SE.statusCode = ProvObjects::SignupStatusCodes::SignupWaitingForEmail;
            StorageService()->SignupDB().CreateRecord(SE);
            Signup()->AddOutstandingSignup(SE);

            if(IT.serialNumber.empty()) {
                IT.serialNumber = SerialNumber;
                IT.info.id = MicroService::instance().CreateUUID();
                IT.info.created = IT.info.modified = OpenWifi::Now();
                IT.info.name = SerialNumber;
                Poco::JSON::Object StateDoc;
                StateDoc.set("method", "signup");
                StateDoc.set("claimer", UserName);
                StateDoc.set("claimerId", UI.id);
                StateDoc.set("signupUUID", SignupUUID);
                StateDoc.set("errorCode",0);
                StateDoc.set("date", OpenWifi::Now());
                StateDoc.set("status", "waiting for email-verification");
                std::ostringstream os2;
                StateDoc.stringify(os2);
                IT.state = os2.str();
                std::cout << "Creating inventory entry: " << SE.serialNumber << std::endl;
                StorageService()->InventoryDB().CreateRecord(IT);
            } else {
                Poco::JSON::Object StateDoc;
                StateDoc.set("method", "signup");
                StateDoc.set("claimer", UserName);
                StateDoc.set("claimerId", UI.id);
                StateDoc.set("signupUUID", SignupUUID);
                StateDoc.set("errorCode",0);
                StateDoc.set("date", OpenWifi::Now());
                StateDoc.set("status", "waiting for email-verification");
                std::ostringstream os2;
                StateDoc.stringify(os2);
                IT.state = os2.str();
                IT.info.modified = OpenWifi::Now();
                std::cout << "Updating inventory entry: " << SE.serialNumber << std::endl;
                StorageService()->InventoryDB().UpdateRecord("id",IT.info.id,IT);
            }

            Poco::JSON::Object  SEAnswer;
            SE.to_json(SEAnswer);
            return ReturnObject(SEAnswer);
        }
        return BadRequest(RESTAPI::Errors::UserAlreadyExists);
    }

    //  this will be called by the SEC backend once the password has been verified.
    void RESTAPI_signup_handler::DoPut() {
        auto SignupUUID = GetParameter("signupUUID","");
        auto Operation = GetParameter("operation","");

        if(SignupUUID.empty() || Operation.empty()) {
            return BadRequest(RESTAPI::Errors::MissingOrInvalidParameters);
        }

        ProvObjects::SignupEntry    SE;
        if(!StorageService()->SignupDB().GetRecord("id",SignupUUID,SE)) {
            return NotFound();
        }

        if(Operation == "emailVerified" && SE.statusCode==ProvObjects::SignupStatusCodes::SignupWaitingForEmail) {
            std::cout << "Verified email for : " << SE.email << std::endl;
            SE.info.modified = OpenWifi::Now();
            SE.status = "emailVerified";
            SE.statusCode = ProvObjects::SignupStatusCodes::SignupWaitingForDevice;
            StorageService()->SignupDB().UpdateRecord("id", SE.info.id, SE);
            Signup()->AddOutstandingSignup(SE);
            Poco::JSON::Object  Answer;
            SE.to_json(Answer);
            return ReturnObject(Answer);
        }

        return BadRequest("Not implemented");
    }

    void RESTAPI_signup_handler::DoGet() {
        auto EMail = GetParameter("email", "");
        auto SignupUUID = GetParameter("signupUUID", "");
        auto SerialNumber = GetParameter("serialNumber", "");
        auto List = GetBoolParameter("listOnly",false);

        Poco::JSON::Object          Answer;
        ProvObjects::SignupEntry    SE;
        if(!SignupUUID.empty()) {
            if(StorageService()->SignupDB().GetRecord("id", SignupUUID, SE)) {
                SE.to_json(Answer);
                return ReturnObject(Answer);
            }
            return NotFound();
        } else if(!EMail.empty()) {
            SignupDB::RecordVec SEs;
            if(StorageService()->SignupDB().GetRecords(0,100,SEs, " email='"+EMail+"' ")) {
                return ReturnObject("signups",SEs);
            }
            return NotFound();
        } else if(!SerialNumber.empty()) {
            SignupDB::RecordVec SEs;
            if(StorageService()->SignupDB().GetRecords(0,100,SEs, " serialNumber='"+SerialNumber+"' ")) {
                return ReturnObject("signups",SEs);
            }
            return NotFound();
        } else if(List) {
            SignupDB::RecordVec SEs;
            StorageService()->SignupDB().GetRecords(0,100,SEs);
            return ReturnObject("signups",SEs);
        }
        return BadRequest(RESTAPI::Errors::MissingOrInvalidParameters);
    }

    void RESTAPI_signup_handler::DoDelete() {
        auto EMail = GetParameter("email", "");
        auto SignupUUID = GetParameter("signupUUID", "");
        auto SerialNumber = GetParameter("serialNumber", "");

        if(!SignupUUID.empty()) {
            if(StorageService()->SignupDB().DeleteRecord("id", SignupUUID)) {
                return OK();
            }
            return NotFound();
        } else if(!EMail.empty()) {
            if(StorageService()->SignupDB().DeleteRecord("email",EMail)) {
                return OK();
            }
            return NotFound();
        } else if(!SerialNumber.empty()) {
            if(StorageService()->SignupDB().DeleteRecord("serialNumber", SerialNumber)) {
                return OK();
            }
            return NotFound();
        }
        return BadRequest(RESTAPI::Errors::MissingOrInvalidParameters);
    }

}