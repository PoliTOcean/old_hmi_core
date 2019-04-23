//
// Created by pettinz.
//

#include "JoystickPublisher.h"
#include "Joystick.h"

#include "json.hpp"

using namespace std;

const string JoystickPublisher::DFLT_ADDRESS    { "tcp://localhost:1883" };
const string JoystickPublisher::DFLT_CLIENT_ID  { "JoystickPublisher" };
const string JoystickPublisher::DFLT_TOPIC      { "JoystickTopic" };

void JoystickPublisher::callback(std::map<int,int> axes, std::map<int,int>buttons) {
    map<string, map<int,int>> c_map { {"axes", axes}, {"buttons", buttons} };
    nlohmann::json j_map(c_map);

    this->publish(DFLT_TOPIC, j_map.dump());
}

int main(int argc, const char *argv[])
{
    JoystickPublisher publisher(JoystickPublisher::DFLT_ADDRESS, JoystickPublisher::DFLT_CLIENT_ID);

    try {
        Joystick joystick;

        publisher.connect();
        joystick.startListening(&JoystickPublisher::callback, &publisher)->join();
        publisher.disconnect();
    } catch (Joystick::JoystickException& e) {
        cerr << e.what() << endl;
    } catch (mqtt::exception& e) {
        cerr << e.what() << endl;
    }

    return 0;
}