//
// Created by stephane bourque on 2022-04-01.
//

#pragma once

#include "framework/MicroService.h"
#include "StorageService.h"
#include "APConfig.h"
#include "sdks/SDK_gw.h"
#include "framework/WebSocketClientNotifications.h"
#include "JobController.h"

namespace OpenWifi {

    [[maybe_unused]] static void GetRejectedLines(const Poco::JSON::Object::Ptr &Response, Types::StringVec & Warnings) {
        try {
            if(Response->has("results")) {
                auto Results = Response->get("results").extract<Poco::JSON::Object::Ptr>();
                auto Status = Results->get("status").extract<Poco::JSON::Object::Ptr>();
                auto Rejected = Status->getArray("rejected");
                std::transform(Rejected->begin(),Rejected->end(),std::back_inserter(Warnings), [](auto i) -> auto { return i.toString(); });
//                for(const auto &i:*Rejected)
                //                  Warnings.push_back(i.toString());
            }
        } catch (...) {
        }
    }

    class VenueDeviceConfigUpdater : public Poco::Runnable {
    public:
        VenueDeviceConfigUpdater(const std::string &UUID, const std::string &venue, Poco::Logger &L) :
            uuid_(UUID),
            venue_(venue),
            Logger_(L) {
        }

        void run() final {
            ProvObjects::InventoryTag   Device;
            started_=true;
            Utils::SetThreadName("venue-cfg");
            if(StorageService()->InventoryDB().GetRecord("id",uuid_,Device)) {
                SerialNumber = Device.serialNumber;
                // std::cout << "Starting push for " << Device.serialNumber << std::endl;
                Logger().debug(fmt::format("{}: Computing configuration.",Device.serialNumber));
                auto DeviceConfig = std::make_shared<APConfig>(Device.serialNumber, Device.deviceType, Logger(), false);
                auto Configuration = Poco::makeShared<Poco::JSON::Object>();
                try {
                    if (DeviceConfig->Get(Configuration)) {
                        std::ostringstream OS;
                        Configuration->stringify(OS);
                        auto Response = Poco::makeShared<Poco::JSON::Object>();
                        Logger().debug(fmt::format("{}: Pushing configuration.", Device.serialNumber));
                        if (SDK::GW::Device::Configure(nullptr, Device.serialNumber, Configuration, Response)) {
                            Logger().debug(fmt::format("{}: Configuration pushed.", Device.serialNumber));
                            Logger().information(fmt::format("{}: Updated.", Device.serialNumber));
                            // std::cout << Device.serialNumber << ": Updated" << std::endl;
                            updated_++;
                        } else {
                            Logger().information(fmt::format("{}: Not updated.", Device.serialNumber));
                            // std::cout << Device.serialNumber << ": Failed" << std::endl;
                            failed_++;
                        }
                    } else {
                        Logger().debug(fmt::format("{}: Configuration is bad.", Device.serialNumber));
                        bad_config_++;
                        // std::cout << Device.serialNumber << ": Bad config" << std::endl;
                    }
                } catch (...) {
                    Logger().debug(fmt::format("{}: Configuration is bad (caused an exception).", Device.serialNumber));
                    bad_config_++;
                }
            }
            done_ = true;
            // std::cout << "Done push for " << Device.serialNumber << std::endl;
            Utils::SetThreadName("free");
        }

        uint64_t        updated_=0, failed_=0, bad_config_=0;
        bool            started_ = false,
                        done_ = false;
        std::string     SerialNumber;

    private:
        std::string     uuid_;
        std::string     venue_;
        Poco::Logger    &Logger_;
        inline Poco::Logger & Logger() { return Logger_; }
    };

    class VenueConfigUpdater: public Job {
    public:
        VenueConfigUpdater(const std::string &JobID, const std::string &name, const std::vector<std::string> & parameters, uint64_t when, const SecurityObjects::UserInfo &UI, Poco::Logger &L) :
                Job(JobID, name, parameters, when, UI, L) {

        }

        inline virtual void run() {
            std::string                 VenueUUID_;

            Utils::SetThreadName("venue-update");
            VenueUUID_ = Parameter(0);

            WebSocketNotification<WebSocketNotificationJobContent> N;

            ProvObjects::Venue  Venue;
            uint64_t Updated = 0, Failed = 0 , BadConfigs = 0 ;
            if(StorageService()->VenueDB().GetRecord("id",VenueUUID_,Venue)) {
                const std::size_t MaxThreads=16;
                struct tState {
                    Poco::Thread                thr_;
                    VenueDeviceConfigUpdater    *task= nullptr;
                };

                N.content.title = fmt::format("Updating {} configurations", Venue.info.name);
                N.content.jobId = JobId();

                std::array<tState,MaxThreads> Tasks;

                for(const auto &uuid:Venue.devices) {
                    auto NewTask = new VenueDeviceConfigUpdater(uuid, Venue.info.name, Logger());
                    // std::cout << "Scheduling config push for " << uuid << std::endl;
                    bool found_slot = false;
                    while (!found_slot) {
                        for (auto &cur_task: Tasks) {
                            if (cur_task.task == nullptr) {
                                cur_task.task = NewTask;
                                cur_task.thr_.start(*NewTask);
                                found_slot = true;
                                break;
                            }
                        }

                        //  Let's look for a slot...
                        if (!found_slot) {
                            for (auto &cur_task: Tasks) {
                                if (cur_task.task != nullptr && cur_task.task->started_) {
                                    if (cur_task.thr_.isRunning())
                                        continue;
                                    if (!cur_task.thr_.isRunning() && cur_task.task->done_) {
                                        cur_task.thr_.join();
                                        Updated += cur_task.task->updated_;
                                        Failed += cur_task.task->failed_;
                                        BadConfigs += cur_task.task->bad_config_;
                                        cur_task.task->started_ = cur_task.task->done_ = false;
                                        delete cur_task.task;
                                        cur_task.task = nullptr;
                                    }
                                }
                            }
                        }
                    }
                }
                Logger().debug("Waiting for outstanding update threads to finish.");
                bool stillTasksRunning=true;
                while(stillTasksRunning) {
                    stillTasksRunning = false;
                    for(auto &cur_task:Tasks) {
                        if(cur_task.task!= nullptr && cur_task.task->started_) {
                            if(cur_task.thr_.isRunning()) {
                                stillTasksRunning = true;
                                continue;
                            }
                            if(!cur_task.thr_.isRunning() && cur_task.task->done_) {
                                cur_task.thr_.join();
                                if(cur_task.task->updated_) {
                                    Updated++;
                                    N.content.success.push_back(cur_task.task->SerialNumber);
                                } else if(cur_task.task->failed_) {
                                    Failed++;
                                    N.content.warning.push_back(cur_task.task->SerialNumber);
                                } else {
                                    BadConfigs++;
                                    N.content.error.push_back(cur_task.task->SerialNumber);
                                }
                                cur_task.task->started_ = cur_task.task->done_ = false;
                                delete cur_task.task;
                                cur_task.task = nullptr;
                            }
                        }
                    }
                }

                N.content.details = fmt::format("Job {} Completed: {} updated, {} failed to update, {} bad configurations. ",
                                                JobId(), Updated ,Failed, BadConfigs);

            } else {
                N.content.details = fmt::format("Venue {} no longer exists.",VenueUUID_);
                Logger().warning(N.content.details);
            }

            WebSocketClientNotificationVenueUpdateJobCompletionToUser(UserInfo().email, N);
            Logger().information(fmt::format("Job {} Completed: {} updated, {} failed to update , {} bad configurations.",
                                             JobId(), Updated ,Failed, BadConfigs));
            Utils::SetThreadName("free");
            Complete();
        }
    };

}