#pragma once

#include <map>
#include <string>
#include <pthread.h>
#include <linux/input.h>
#include <termios.h>
 

class Keyboard
{
	std::string path; //!<linux device path.
	std::string name; //!<device name.

    pthread_t thread; //!<key polling thread.
	pthread_mutex_t mutex; //!<The mutex protecting the keyboardState member.
    bool active; //!<flags to keep the thread running or stop it.

    int keyboardDescriptor; //!<File descriptor for keyboard device.
    int32_t keyboardState[KEY_CNT]; //!<State of the individual keys in the device.
    std::map<int32_t, bool> pressedKeys; //!<List of keys that were pressed since the list was last cleared.
    termios oldTermios; //!<Old termios state store before turning off echoing.

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
	Check current state of a key.
	\param[in] key Scancode of key to check.
	\return Returns the current state of the key. 0 for released, 1 for pressed, 2 for autorepeat.
	*/
    int32_t getKeyState(uint32_t key);
    
    /*!
    Check if a key was pressed since \clearPressedKeys was called.
    \param[in] key Scancode of key to check.
    \return Returns true if the key was pressed.
    */
    bool keyWasPressed(uint32_t key);
    
    /*!
    Clear the list of pressed keys.
    \note Call this in your loop that polls the keys.
    */
    void clearPressedKeys();

	~Keyboard();
};
