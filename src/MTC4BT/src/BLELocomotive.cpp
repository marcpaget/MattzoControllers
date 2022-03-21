#include "MController.h"
#include "BLELocomotive.h"
#include "SBrickHub.h"
#include "PUHub.h"
#include "MCLightController.h"
#include "log4MC.h"

BLELocomotive::BLELocomotive(BLELocomotiveConfiguration *config, MController *controller)
    : _config{config}, _controller{controller}
{
    initHubs();
}

bool BLELocomotive::IsEnabled()
{
    return _config->_enabled;
}

bool BLELocomotive::AllHubsConnected()
{
    if (!IsEnabled())
    {
        return false;
    }

    for (BLEHub *hub : Hubs)
    {
        if (!hub->IsConnected())
        {
            return false;
        }
    }

    return true;
}

void BLELocomotive::Drive(const int16_t minSpeed, const int16_t pwrPerc)
{
    if (!AllHubsConnected())
    {
        // Ignore drive command.
        log4MC::vlogf(LOG_INFO, "Loco: %s ignored drive command because not all its hubs are connected (yet).", _config->_name.c_str());
        return;
    }

    for (int i = 0; i < Hubs.size(); i++)
    {
        BLEHub *hub = Hubs.at(i);
        int16_t currentPwrPerc = hub->GetCurrentDrivePwrPerc();
        hub->Drive(minSpeed, pwrPerc);

        if (currentPwrPerc == 0 && pwrPerc > 0)
        {
            // If we go from stand still (0%) to moving (not 0%), we need to let the controller know so it can possibly handle this event.
            _controller->HandleTrigger(_config->_address, MCTriggerSource::Loco, "dirchanged", "", "forward");
        }

        if (currentPwrPerc == 0 && pwrPerc < 0)
        {
            // If we go from stand still (0%) to moving (not 0%), we need to let the controller know so it can possibly handle this event.
            _controller->HandleTrigger(_config->_address, MCTriggerSource::Loco, "dirchanged", "", "backward");
        }

        if (currentPwrPerc != 0 && pwrPerc == 0)
        {
            // If we go from moving (not 0%) to stand still (0%), we need to let the controller know so it can possibly handle this event.
            _controller->HandleTrigger(_config->_address, MCTriggerSource::Loco, "dirchanged", "", "stopped");
        }
    }
}

void BLELocomotive::TriggerEvent(MCTriggerSource source, std::string eventType, std::string eventId, std::string value)
{
    if (!AllHubsConnected())
    {
        // Ignore trigger.
        log4MC::vlogf(LOG_INFO, "Loco: %s ignored trigger because not all its hubs are connected (yet).", _config->_name.c_str());

        // Return success anyway, because we don't want the controller to handle the trigger.
        return;
    }

    for (MCLocoEvent *event : _config->_events)
    {
        if (event->HasTrigger(source, eventType, eventId, value))
        {
            for (MCLocoAction *action : event->GetActions())
            {
                ChannelType portType = action->GetChannel()->GetChannelType();
                switch (portType)
                {
                case ChannelType::BleHubChannel:
                {
                    // Ask hub to execute action.
                    BLEHub *hub = getHubByAddress(action->GetChannel()->GetParentAddress());
                    if (hub)
                    {
                        hub->Execute(action);
                    }
                    break;
                }
                case ChannelType::EspPinChannel:
                {
                    // Ask controller to execute action.
                    _controller->Execute(action);
                    break;
                }
                }
            }
        }
    }
}

void BLELocomotive::BlinkLights(int durationInMs)
{
    if (!AllHubsConnected())
    {
        // Ignore blink request.
        // log4MC::vlogf(LOG_INFO, "Loco: %s ignored blink lights request because not all its hubs are connected (yet).", _config->_name.c_str());
        return;
    }

    // Handle blink on lights attached to channels of our hubs.
    for (int i = 0; i < Hubs.size(); i++)
    {
        Hubs.at(i)->BlinkLights(durationInMs);
    }
}

void BLELocomotive::EmergencyBrake(const bool enabled)
{
    if (!AllHubsConnected())
    {
        // Ignore e-brake command (we're not driving anyway).
        return;
    }

    // Handle e-brake on all channels of our hubs.
    for (BLEHub *hub : Hubs)
    {
        hub->EmergencyBrake(enabled);
    }
}

std::string BLELocomotive::GetLocoName()
{
    return _config->_name;
}

uint BLELocomotive::GetLocoAddress()
{
    return _config->_address;
}

uint BLELocomotive::GetHubCount()
{
    return Hubs.size();
}

BLEHub *BLELocomotive::GetHub(uint index)
{
    return Hubs.at(index);
}

void BLELocomotive::initHubs()
{
    for (BLEHubConfiguration *hubConfig : _config->_hubs)
    {
        switch (hubConfig->HubType)
        {
        case BLEHubType::SBrick:
            Hubs.push_back(new SBrickHub(hubConfig));
            break;
        case BLEHubType::PU:
            Hubs.push_back(new PUHub(hubConfig));
            break;
        }
    }

    // log4MC::vlogf(LOG_INFO, "Loco: %s hub config initialized.", _config->_name.c_str());
}

BLEHub *BLELocomotive::getHubByAddress(std::string address)
{
    for (BLEHub *hub : Hubs)
    {
        if (hub->GetAddress().compare(address) == 0)
        {
            return hub;
        }
    }

    return nullptr;
}