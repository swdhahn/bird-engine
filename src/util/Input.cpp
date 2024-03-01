//
// Created by student on 10/5/23.
//

#include "Input.h"

namespace bird {

    Action::Action(std::string name) : m_name(name) {

    }

    Action& Action::addKey(int keyCode) {
        m_keys.push_back(keyCode);
        return *this;
    }

    float Action::getValue() {
        return m_value;
    }

    void Action::setValue(float value) {
        m_value = value;
    }

    std::vector<int>& Action::getKeys() {
        return m_keys;
    }

    std::string Action::getName() {
        return m_name;
    }

    Input::Input(Window* window) : m_window(window) {

    }

    void Input::addAction(Action action) {
        m_actions.push_back(action);
    }

    float Input::getActionValue(std::string name) {
        for(int i = 0; i < m_actions.size(); i++) {
            if(m_actions[i].getName() == name) {
                return m_actions[i].getValue();
            }
        }

        std::cerr << ("Action could not be found: " + std::string(name)) << std::endl;
        return 0;
    }

    void Input::tick() {
        for(int i = 0; i < m_actions.size(); i++) {
            Action& a = m_actions[i];
            bool flag = true;
            for(int j = 0; j < a.getKeys().size(); j++) {
                int key = glfwGetKey(m_window->getGLFWWindow(), a.getKeys()[j]);
                if(key == GLFW_PRESS) {
                    a.setValue(1.0f);
                    flag = false;
                    break;
                }
            }
            //joystick
            //mouse click
            //joystick buttons
            if(flag) {
                a.setValue(0);
            }
        }
    }
    
} // bird