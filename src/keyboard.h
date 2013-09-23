#pragma once

#include <string>
#include <pthread.h>
#include <linux/input.h>
#include <termios.h>
 

class Keyboard
{
	std::string path; //!<linux device path
	std::string name; //!<device name

    pthread_t thread; //!<key polling thread
	pthread_mutex_t mutex; //!<The mutex protecting the keyboardState member
    bool active; //!<flags to keep the thread running or stop it

    int keyboardDescriptor; //!<File descriptor for keyboard device
    int32_t keyboardState[KEY_CNT]; //!<State of the individual keys in the device
    termios oldTermios; //!<Old termios state store before turning off echoing

	static void * keyLoop(void * obj);

public:
    /*!
    Create keyboard interface and start polling keyboard.
    \param[in] devicePath Optional. Device path to open keyboard at, e.g. "/dev/input/event3".
    \note If \devicePath is empty, the keyboard will be autodetected.
    */
	Keyboard(std::string devicePath);

    /*!
    Check if keyboard interface is available.
    \return Returns true if the keyboard interface can be used.
    */
	bool isAvailable() const;
	
	/*!
	Check if key is being pressed atm.
	\param[in] key Scancode of key to check.
	\return Returns true if keyboard interface is active and key is pressed atm.
	*/
    bool isKeyDown(uint32_t key);

	~Keyboard();
};
