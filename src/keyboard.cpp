#include "keyboard.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <limits>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <linux/input.h>

#include "consolestyle.h"

//Inspired by "logkeys" found on Google code: http://code.google.com/p/logkeys/
#define EXE_GREP "/bin/grep"
#define COMMAND_STR_DEVICES (EXE_GREP " -E 'Handlers|EV=' /proc/bus/input/devices | " EXE_GREP " -B1 'EV=120013' | " EXE_GREP " -Eo 'event[0-9]+' ")

Keyboard::Keyboard(std::string devicePath)
	: path(devicePath), thread(0), mutex(PTHREAD_MUTEX_INITIALIZER), active(false), keyboardDescriptor(0) 
{
    //if the caller didn't pass a keyboard device event node, so auto-detect it
    if (devicePath.empty()) {
	    //extract input number from /proc/bus/input/devices  
    	FILE * pipe = popen(COMMAND_STR_DEVICES, "r");
	    if (pipe) {
	        //read returned data from system command
	        std::stringstream output;
	        char buffer[128];
	        while (!feof(pipe)) {
		        if (fgets(buffer, 128, pipe) != nullptr) {
                    output << buffer;
                }
            }
	        pclose(pipe);
	        //find first device entry that has an "event..." handler entry
	        std::string line;
	        while(std::getline(output, line)) {
		        const std::string::size_type start = line.find("event");
		        if (start != std::string::npos) {
		            //search end of "event..." string
		            const std::string::size_type end = line.find_first_of(" \t\f\v\n\r");
		            const std::string::size_type length = (end == std::string::npos ? line.size() - start : end - start);
		            //extract string
		            devicePath = "/dev/input/" + line.substr(start, length);
		            std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Auto-detected keyboard at \"" << devicePath << "\"." << ConsoleStyle() << std::endl;
		            break;
		        }
	        }
	    }
	    else {
		    std::cout << ConsoleStyle(ConsoleStyle::RED) << "Pipe error!" << ConsoleStyle() << std::endl;
        }
    }
    //if device path is still empty, use a default
    if (devicePath.empty()) {
        devicePath = "/dev/input/event0";
    }

    //store old termios
    tcgetattr(keyboardDescriptor, &oldTermios);
    termios newTermios = oldTermios;
    //turn off ECHO
    newTermios.c_lflag &= ~ICANON;
    newTermios.c_lflag &= ~ECHO;
    //newTermios.c_cc[VMIN] = 1;
    //newTermios.c_cc[VTIME] = 0;
    //tcsetattr(0, TCSANOW, &newTermios);
    //open keyboard device
    keyboardDescriptor = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (keyboardDescriptor > 0) {
		//get keyboard name
		char temp[256];
		ioctl(keyboardDescriptor, EVIOCGNAME(256), temp);
		temp[255] = '\0';
		name = temp;
		std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Opened keyboard \"" << name << "\" at \"" << devicePath << "\"." << ConsoleStyle() << std::endl;
		//clear keyboard state
		memset(keyboardState, 0, sizeof(keyboardState));
		//start key poll thread
        active = true;
        if (pthread_create(&thread, 0, &Keyboard::keyLoop, this) == 0) {
			std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Started keyboard thread." << ConsoleStyle() << std::endl;
		}
		else {
			std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to start keyboard thread!" << ConsoleStyle() << std::endl;
			thread = 0;
			active = false;
			close(keyboardDescriptor);
			keyboardDescriptor = 0;
		}
    }
	else {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to open keyboard at \"" << devicePath << "\"!" << ConsoleStyle() << std::endl;
	}
}

void * Keyboard::keyLoop(void * obj)
{
	Keyboard * keyboard = reinterpret_cast<Keyboard *>(obj);
	//start thread loop
    while (keyboard != nullptr && keyboard->active) {
		//read keyboard event
		input_event inputEvent;
		if (read(keyboard->keyboardDescriptor, reinterpret_cast<void *>(&inputEvent), sizeof(inputEvent)) > 0) {
			if (inputEvent.type & EV_KEY) {
				//copy key value to keyboard state array. block mutex before
				pthread_mutex_lock(&keyboard->mutex);
				keyboard->keyboardState[inputEvent.code] = inputEvent.value;
				pthread_mutex_unlock(&keyboard->mutex);
				//std::cout << "Key " << inputEvent.code << " = " << inputEvent.value << std::endl;
				//the values are: 0 for released, 1 for pressed, 2 for autorepeat
			}
		}
		//sleep 20ms between polling keyboard
		usleep(20 * 1000);
	}
}

bool Keyboard::isAvailable() const
{
	return (keyboardDescriptor > 0 && active);
}

bool Keyboard::isKeyDown(uint32_t key)
{
	bool result = false;
	//block mutex for member variables
	pthread_mutex_lock(&mutex);
	if (active && key < KEY_CNT) {
		result = (keyboardState[key] > 0);
	}
	pthread_mutex_unlock(&mutex);
	return result;
}

Keyboard::~Keyboard()
{
	std::cout << "Closing keyboard." << std::endl;
	if (thread != 0) {
		active = false;
		pthread_join(thread, 0);
		thread = 0;
	}
	if (keyboardDescriptor > 0) {
		close(keyboardDescriptor);
		keyboardDescriptor = 0;
	}
    //turn on ECHO again
    //tcsetattr(0, TCSAFLUSH/*TCSANOW*/, &oldTermios);
    //flush streams
    //tcflush(TCIOFLUSH, TCION);
}

