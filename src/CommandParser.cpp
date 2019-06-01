#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>

#include "MqttClient.h"

#include <json.hpp>

#include "PolitoceanConstants.h"
#include "PolitoceanExceptions.hpp"
#include "mqttLogger.h"

using namespace Politocean;
using namespace Politocean::Constants;
using namespace Politocean::Constants::Commands;
/**************************************************************
 * Listener class for Joystick device
 *************************************************************/

typedef struct button_t
{
    int id;
    unsigned int value;

    button_t(int id, int value) : id(id), value(value) {}

    bool operator==(const button_t &o) const {
        return id == o.id && value == o.value;
    }

} button_t;

class Listener {
    std::queue<button_t> buttons_;

    bool isButtonUpdated_ = false;
    bool isAxesUpdated_ = false;

    std::vector<int> axes_;

    std::mutex mutexAxs_, mutexBtn_;


public:
    void listenForButtons(const std::string& payload);
    void listenForAxes(const std::string& payload);

    button_t button();

    std::vector<int> axes();

    bool isButtonUpdated();
    bool isAxesUpdated();
};

void Listener::listenForButtons(const std::string& payload)
{
	std::lock_guard<std::mutex> lock(mutexBtn_);
    int btn  = static_cast<int>(std::stoi(payload));

    buttons_.push( button_t(btn & 0x7F, (btn >> 7) & 0x01) );

    isButtonUpdated_ = true;
}

button_t Listener::button()
{
   	std::lock_guard<std::mutex> lock(mutexBtn_);
    if (!buttons_.empty())
    {
        button_t button = buttons_.front();
        buttons_.pop();
        return button;
    }
    return button_t(-1, 0);
}

bool Listener::isButtonUpdated()
{
    return isButtonUpdated_ && !buttons_.empty();
}

bool Listener::isAxesUpdated()
{
    return isAxesUpdated_ && !axes_.empty();
}

void Listener::listenForAxes(const std::string& payload)
{
   	std::lock_guard<std::mutex> lock(mutexAxs_);
    auto c_map = nlohmann::json::parse(payload);
    axes_ = c_map.get<std::vector<int>>();

    isAxesUpdated_ = true;
}

std::vector<int> Listener::axes(){
   	std::lock_guard<std::mutex> lock(mutexAxs_);
    isAxesUpdated_ = false;
    return axes_;
}

/**************************************************************
 * Talker class for Joystick publisher
 *************************************************************/

class Talker {
    /**
     * @buttonTalker	: talker thread for button value
     */
    std::thread *buttonTalker_, *axesTalker_;

    /**
     * @isTalking_ : it is true if the talker is isTalking
     */
    bool isTalking_ = false;

public:
    void startTalking(MqttClient& publisher, Listener& listener);
    void stopTalking();

    bool isTalking();
};

void Talker::startTalking(MqttClient& publisher, Listener& listener)
{

    if (isTalking_)
        return ;

    isTalking_ = true;

    axesTalker_ = new std::thread([&](){
        std::map<int, int> prevAxes;
        prevAxes.insert( std::pair<int, int>(Axes::X, 0));
        prevAxes.insert( std::pair<int, int>(Axes::Y, 0));
        prevAxes.insert( std::pair<int, int>(Axes::RZ, 0));
        prevAxes.insert( std::pair<int, int>(Axes::SHOULDER, 0));
        prevAxes.insert( std::pair<int, int>(Axes::WRIST, 0));
        prevAxes.insert( std::pair<int, int>(Axes::HAND, 0));
        
        while(publisher.is_connected())
        {            
            if(!listener.isAxesUpdated())
            {
            	std::this_thread::sleep_for(std::chrono::milliseconds(Timing::Milliseconds::JOYSTICK));
                continue;
            }
                
            std::vector<int> axes = listener.axes();

            if(axes[Axes::X] != prevAxes.at(Axes::X)
                || axes[Axes::Y] != prevAxes.at(Axes::Y)
                || axes[Axes::RZ] != prevAxes.at(Axes::RZ)){

                std::vector<int> atmega_axes = {
                    axes[Axes::X],
                    axes[Axes::Y],
                    axes[Axes::RZ]
                };
                nlohmann::json atmega = atmega_axes;
                publisher.publish(Topics::AXES, atmega.dump());
                
                prevAxes[Axes::X] = axes[Axes::X];
                prevAxes[Axes::Y] = axes[Axes::Y];
                prevAxes[Axes::RZ] = axes[Axes::RZ];
            }
            
            if(axes[Axes::SHOULDER] != prevAxes.at(Axes::SHOULDER)){
                int shoulder_axes = axes[Axes::SHOULDER];
                nlohmann::json shoulder = shoulder_axes;
                publisher.publish(Topics::SHOULDER_VELOCITY, shoulder.dump());

                prevAxes[Axes::SHOULDER] = axes[Axes::SHOULDER];
            }

            if(axes[Axes::WRIST] != prevAxes.at(Axes::WRIST)){
                int shoulder_wrist = axes[Axes::WRIST];
                nlohmann::json wrist = shoulder_wrist;
                publisher.publish(Topics::WRIST_VELOCITY, wrist.dump());

                prevAxes[Axes::WRIST] = axes[Axes::WRIST];
            }

            if(axes[Axes::HAND] != prevAxes.at(Axes::HAND)){
                int shoulder_hand = axes[Axes::HAND];
                nlohmann::json hand = shoulder_hand;
                publisher.publish(Topics::HAND_VELOCITY, hand.dump());

                prevAxes[Axes::HAND] = axes[Axes::HAND];
            }

        }
    });

    buttonTalker_ = new std::thread([&]() {

        map<int, bool> state = {{Buttons::MOTORS,false}};
        while (publisher.is_connected())
        {

            if (!listener.isButtonUpdated())
            {
			    std::this_thread::sleep_for(std::chrono::milliseconds(Timing::Milliseconds::JOYSTICK));
                continue;
            }
            
            button_t button = listener.button();
            int id      = button.id;
            int value   = button.value;
            

            string action = Actions::NONE;

            string topic = "";

            // Parsing button by identifier
            switch (id)
            {
                case Buttons::START_AND_STOP:
                    topic = Topics::COMMANDS;
                    if (value)
                        action = Actions::ATMega::START_AND_STOP;
                    break;
                
                case Buttons::MOTORS:
                    topic = Topics::COMMANDS;
                    if (value && !state[id])
                    {
                        action = Actions::ON;
                        state[id] = true;
                    }
                    else if (value && state[id])
                    {
                        action = Actions::OFF;
                        state[id] = false;
                    }
                    break;

                case Buttons::RESET:
                    topic = Topics::COMMANDS;
                    if (value)
                        action = Actions::RESET;
                    break;

                case Buttons::VUP:
                    topic = Topics::COMMANDS;
                    value ? action = Actions::ATMega::VUP_ON : action = Actions::ATMega::VUP_OFF;
                    break;
                
                case Buttons::VUP_FAST:
                    topic = Topics::COMMANDS;
                    value ? action = Actions::ATMega::VUP_FAST_ON : action = Actions::ATMega::VUP_FAST_OFF;
                    break;

                case Buttons::VDOWN:
                    topic = Topics::COMMANDS;
                    value ? action = Actions::ATMega::VDOWN_ON : action = Actions::ATMega::VDOWN_OFF;
                    break;

                case Buttons::SLOW:
                    topic = Topics::COMMANDS;
                    if (value)
                        action = Actions::ATMega::SLOW;
                    break;
                
                case Buttons::MEDIUM_FAST:
                    topic = Topics::COMMANDS;
                    if (value)
                        action = Actions::ATMega::MEDIUM;
                    else
                        action = Actions::ATMega::FAST;
                    break;

                case Buttons::SHOULDER_ENABLE:
                    topic = Topics::SHOULDER;
                    if (value)
                        action = Actions::ON;
                    break;

                case Buttons::SHOULDER_DISABLE:
                    topic = Topics::SHOULDER;
                    if (value)
                        action = Actions::OFF;
                    break;

                case Buttons::WRIST_ENABLE:
                    topic = Topics::WRIST;
                    if (value)
                        action = Actions::ON;
                    break;

                case Buttons::WRIST_DISABLE:
                    topic = Topics::WRIST;
                    if (value)
                        action = Actions::OFF;
                    break;

                case Buttons::WRIST:
                    topic = Topics::WRIST;
                    if (value)
                        action = Actions::START;
                    else
                        action = Actions::STOP;
                    break;

                case Buttons::SHOULDER_UP:
                    topic = Topics::SHOULDER;
                    if (value)
                        action = Actions::Stepper::UP;
                    else
                        action = Actions::STOP;
                    break;

                case Buttons::SHOULDER_DOWN:
                    topic = Topics::SHOULDER;
                    if(value)
                        action = Actions::Stepper::DOWN;
                    else
                        action = Actions::STOP;
                    break;

                case Buttons::HAND:
                    topic = Topics::HAND;
                    if(value)
                        action = Actions::START;
                    else
                        action = Actions::STOP;
                    break;
                
                case Buttons::HEAD_ENABLE:
                    topic = Topics::HEAD;
                    if (value)
                        action = Actions::ON;
                    break;
                
                case Buttons::HEAD_DISABLE:
                    topic = Topics::HEAD;
                    if (value)
                        action = Actions::OFF;
                    break;

                case Buttons::HEAD_UP:
                    topic = Topics::HEAD;
                    if (value)
                        action = Actions::Stepper::UP;
                    else
                        action = Actions::STOP;
                    break;
                
                case Buttons::HEAD_DOWN:
                    topic = Topics::HEAD;
                    if (value)
                        action = Actions::Stepper::DOWN;
                    else
                        action = Actions::STOP;
                    break;
                    
                default: 
                    break;
            }

            if(action != Actions::NONE)
                publisher.publish(topic, action);
        }

        isTalking_ = false;
    });
}

void Talker::stopTalking()
{
    if (!isTalking_)
        return ;

    isTalking_ = false;
    buttonTalker_->join();
}

bool Talker::isTalking()
{
    return isTalking_;
}

int main(int argc, const char* argv[])
{
    logger::enableLevel(logger::INFO);

    Talker talker;

    MqttClient& hmiClient = MqttClient::getInstance(Constants::Hmi::CMD_ID, Constants::Hmi::IP_ADDRESS);
    Listener listener;

    mqttLogger& ptoLogger = mqttLogger::getInstance(hmiClient);
    ptoLogger.setPublishLevel(logger::CONFIG);

    hmiClient.subscribeTo(Topics::JOYSTICK_BUTTONS, &Listener::listenForButtons, &listener);
    hmiClient.subscribeTo(Topics::JOYSTICK_AXES, &Listener::listenForAxes, &listener);

    talker.startTalking(MqttClient::getInstance(Constants::Hmi::CMD_ID, Constants::Rov::IP_ADDRESS), listener);
    
    hmiClient.wait();

    talker.stopTalking();

    return 0;
}