#pragma once

#include <string>
#include <vector>
#include <libusb.h>


class MissileControl
{
public:
	enum LauncherModel {LAUNCHER_UNKNOWN, LAUNCHER_M_S, LAUNCHER_CHEEKY}; //!<Supported USB launcher models.
	enum LauncherCommand {NONE, STOP, LEFT, RIGHT, UP, DOWN, LEFTUP, RIGHTUP, LEFTDOWN, RIGHTDOWN, FIRE}; //!<Supported launcher commands.

private:
	static const uint32_t controlInterval; //!<sleep time for the control thread in ms.
	static const uint32_t usbControlTimeout; //!<timeout in ms for usb control transfer functions.

    pthread_t thread; //!<launcher control thread.
	pthread_mutex_t mutex; //!<The mutex protecting the member variables.
    bool active; //!<flags to keep the thread running or stop it.

	libusb_context * usbContext; //!<libusb context.
	libusb_device_handle * usbLauncher; //!<Launcher USB device handle.
	
	struct LauncherInfo {
		LauncherModel model;
		uint16_t usbVendorId;
		uint16_t usbProductId;
		std::string description;

		LauncherInfo() 
			: model(LAUNCHER_UNKNOWN), usbVendorId(0), usbProductId(0) {};
		LauncherInfo(LauncherModel launcherModel, uint16_t vendorId, uint16_t productId, const std::string & desc)
			: model(launcherModel), usbVendorId(vendorId), usbProductId(productId), description(desc) {};
	};
	std::vector<LauncherInfo> supportedLaunchers;
	LauncherInfo launcherInfo;
	
	LauncherCommand currentCommand; //!<The current command sent to the launcher.
	int currentRemainingTime; //!<The time remaining till a stop command must be issued.
	bool armed; //!<If true the launcher is armed and will shoot if a fire command is executed.

	static void * controlLoop(void * obj);

public:
    /*!
    Detect and claim first supported USB missile launcher.
    */
	MissileControl();

    /*!
    Check if launcher control is available.
    \return Returns true if a launcher was found and can be controlled via \executeCommand.
    */	
	bool isAvailable() const;

	/*!
	Executes a launcher command.
	\param[in] command The command to issue to the launcher.
	\param[in] duration Optional. Duration in ms the command should be executed before a STOP command is issued. With duration == INT_MIN no stop command will be issued.
	\return Returns true if the command was issued, false if not.
	\note The minimum duration is the loop delay of about 20ms
	*/
	bool executeCommand(LauncherCommand command, int durationMs = INT_MIN);
	
	/*!
	Set the state of the launcher to armed. It will shoot if a FIRE command is executed.
	\param[in] arm Arm or unarm launcher.
    */
	void setArmed(bool arm);
	bool isArmed() const;

	~MissileControl();
};

