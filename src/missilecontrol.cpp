#include "missilecontrol.h"

#include <iostream>
#include <cstring>
#include <unistd.h>

#include "consolestyle.h"


//Byte sequence to send to the device. The first two init commands are for the M&S launcher only
//                                    'U' 'S' 'B' 'C'
static uint8_t SEQUENCE_INITA[]     = {85, 83, 66, 67,  0,  0,  4,  0};
static uint8_t SEQUENCE_INITB[]     = {85, 83, 66, 67,  0, 64,  2,  0};
//General commands
static uint8_t SEQUENCE_STOP[]      = { 0,  0,  0,  0,  0,  0,  8,  8};
static uint8_t SEQUENCE_LEFT[]      = { 0,  1,  0,  0,  0,  0,  8,  8};
static uint8_t SEQUENCE_RIGHT[]     = { 0,  0,  1,  0,  0,  0,  8,  8};
static uint8_t SEQUENCE_UP[]        = { 0,  0,  0,  1,  0,  0,  8,  8};
static uint8_t SEQUENCE_DOWN[]      = { 0,  0,  0,  0,  1,  0,  8,  8};
static uint8_t SEQUENCE_LEFTUP[]    = { 0,  1,  0,  1,  0,  0,  8,  8};
static uint8_t SEQUENCE_RIGHTUP[]   = { 0,  0,  1,  1,  0,  0,  8,  8};
static uint8_t SEQUENCE_LEFTDOWN[]  = { 0,  1,  0,  0,  1,  0,  8,  8};
static uint8_t SEQUENCE_RIGHTDOWN[] = { 0,  0,  1,  0,  1,  0,  8,  8};
static uint8_t SEQUENCE_FIRE[]      = { 0,  0,  0,  0,  0,  1,  8,  8};
static uint8_t * sequences[] = {nullptr, SEQUENCE_STOP, 
								SEQUENCE_LEFT, SEQUENCE_RIGHT, SEQUENCE_UP, SEQUENCE_DOWN, 
								SEQUENCE_LEFTUP, SEQUENCE_RIGHTUP, SEQUENCE_LEFTDOWN, SEQUENCE_RIGHTDOWN, 
								SEQUENCE_FIRE};

const uint32_t MissileControl::controlInterval = 20;
const uint32_t MissileControl::usbControlTimeout = 500;


MissileControl::MissileControl()
	: thread(0), mutex(PTHREAD_MUTEX_INITIALIZER), active(false), 
	  usbContext(nullptr), usbLauncher(nullptr),
	  currentCommand(NONE), currentRemainingTime(INT_MIN)
{
	std::cout << "Initializing missile control..." << std::endl;

	//add supported launcher models
	supportedLaunchers.push_back(LauncherInfo(LAUNCHER_M_S, 0x1130, 0x0202, "M&S"));
	supportedLaunchers.push_back(LauncherInfo(LAUNCHER_CHEEKY, 0x1941, 0x8021, "Dream Cheeky"));

	//print supported devices
	std::cout << "Supported launchers:" << std::endl;
	for (auto slIt = supportedLaunchers.cbegin(); slIt != supportedLaunchers.cend(); ++slIt) {
		std::cout << slIt->description << " - Vendor 0x" << std::hex << slIt->usbVendorId << ", Product 0x" << slIt->usbProductId << std::dec << std::endl;
	}

	//initialize libusb
    int errnum = 0;
    if (errnum = libusb_init(&usbContext)) {
        std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to initialise libusb. Error: " << libusb_error_name(errnum) << "." << ConsoleStyle() << std::endl;
        return;
    }
    //libusb_get_version() is not available in libusb.h atm... not sure why...
    /*std::cout << "libusb version is " << libusb_get_version()->major << "." << libusb_get_version()->minor << "."
                                      << libusb_get_version()->micro << "." << libusb_get_version()->nano << " "
                                      << libusb_get_version()->describe << "." << std::endl;*/
    libusb_set_debug(usbContext, 3);
    //get all USB devices in system
    libusb_device * * devices;
    size_t deviceCount = libusb_get_device_list(usbContext, &devices);
    std::cout << "Found " << deviceCount << " USB devices." << std::endl;
    //iterate through USB devices looking for launcher
    for(int i = 0; i < deviceCount; i++)
    {
        //get ith device and try to get descriptor
        libusb_device * device = devices[i];
        libusb_device_descriptor deviceDescriptor;
        if (errnum = libusb_get_device_descriptor(device, &deviceDescriptor)) {
            continue;
        }
        //check vendor and product id
		auto slIt = supportedLaunchers.cbegin();
		for (; slIt != supportedLaunchers.cend(); ++slIt) {
			if (deviceDescriptor.idVendor == slIt->usbVendorId && deviceDescriptor.idProduct == slIt->usbProductId) {
				break;
			}
		}
        if (slIt != supportedLaunchers.cend()) {
            std::cout << slIt->description << " launcher found on Bus" << libusb_get_bus_number(device)
                                           << ", Adress " << libusb_get_device_address(device)
                                           << ", Speed " << libusb_get_device_speed(device) << "." << std::endl;
            //try to open device
            if (errnum = libusb_open(device, &usbLauncher)) {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "Unable to open device. Error: " << libusb_error_name(errnum) << "." << ConsoleStyle() << std::endl;
                continue;
            }
            //check if the kernel driver uses the device interfaces 0/1
            if (libusb_kernel_driver_active(usbLauncher, 0)) {
                //if the kernel driver is active, try to detach it from the device
                if (errnum = libusb_detach_kernel_driver(usbLauncher, 0)) {
                    std::cout << ConsoleStyle(ConsoleStyle::RED) << "Unable to detach kernel driver form device interface 0. Error: " << libusb_error_name(errnum) << "." << ConsoleStyle() << std::endl;
                    libusb_close(usbLauncher);
                    usbLauncher = nullptr;
                    continue;
                }
            }
            if (libusb_kernel_driver_active(usbLauncher, 1)) {
                //if the kernel driver is active, try to detach it from the device
                if (errnum = libusb_detach_kernel_driver(usbLauncher, 1)) {
                    std::cout << ConsoleStyle(ConsoleStyle::RED) << "Unable to detach kernel driver form device interface 1. Error: " << libusb_error_name(errnum) << "." << ConsoleStyle() << std::endl;
                    libusb_close(usbLauncher);
                    usbLauncher = nullptr;
                    continue;
                }
            }
            //set configuration
            if (errnum = libusb_set_configuration(usbLauncher, 1)) {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "Unable to set device configuration. Error: " << libusb_error_name(errnum) << "." << ConsoleStyle() << std::endl;
                libusb_close(usbLauncher);
                usbLauncher = nullptr;
                continue;
            }
            //now claim interface 0
            if (errnum = libusb_claim_interface(usbLauncher, 0)) {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "Unable to claim device interface 0. Error: " << libusb_error_name(errnum) << "." << ConsoleStyle() << std::endl;
                libusb_close(usbLauncher);
                usbLauncher = nullptr;
                continue;
            }
            
            //libusb_set_altinterface(launcher, 0); needed?!

			//worked. store model
			launcherInfo = *slIt;

			std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Missile control available." << ConsoleStyle() << std::endl;
            break;
        }
    }
    libusb_free_device_list(devices, 1);

	//now if a launcher was found, create a thread for it
	if (launcherInfo.model != LAUNCHER_UNKNOWN && usbLauncher != nullptr) {
		active = true;
		if (pthread_create(&thread, 0, &MissileControl::controlLoop, this) == 0) {
			std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Started control thread." << ConsoleStyle() << std::endl;
		}
		else {
			std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to start control thread!" << ConsoleStyle() << std::endl;
			thread = 0;
			active = false;
			libusb_release_interface(usbLauncher, 0);
			libusb_close(usbLauncher);
            usbLauncher = nullptr;
		}
	}
}

void * MissileControl::controlLoop(void * obj)
{
	MissileControl * control = reinterpret_cast<MissileControl *>(obj);
	//start thread loop
	while (control != nullptr && control->active) {
		//block/unblock mutex while reading/modifying command
		pthread_mutex_lock(&control->mutex);
		//check if a command was issued
		if (control->currentCommand != NONE) {
			//check if a STOP command is needed now
			if (control->currentRemainingTime != INT_MIN && control->currentRemainingTime <= 0) {
				control->currentCommand = STOP;
				control->currentRemainingTime = INT_MIN;
			}
			if (control->currentRemainingTime != INT_MIN && control->currentRemainingTime > 0) {
				control->currentRemainingTime -= controlInterval;
			}
			//copy command sequences to command buffer
			uint8_t commandBuffer[64];
			memset(commandBuffer, 0, sizeof(commandBuffer));		
			memcpy(commandBuffer, sequences[control->currentCommand], 8);
			//send command to device
			int errnum = 0;
			if (control->launcherInfo.model == LAUNCHER_M_S) {
				//needed for M&S launchers
				if ((errnum = libusb_control_transfer(control->usbLauncher, LIBUSB_DT_HID, LIBUSB_REQUEST_SET_CONFIGURATION, LIBUSB_RECIPIENT_ENDPOINT, 0x01, SEQUENCE_INITA, sizeof(SEQUENCE_INITA), usbControlTimeout) <= 0) ||
					(errnum = libusb_control_transfer(control->usbLauncher, LIBUSB_DT_HID, LIBUSB_REQUEST_SET_CONFIGURATION, LIBUSB_RECIPIENT_ENDPOINT, 0x01, SEQUENCE_INITB, sizeof(SEQUENCE_INITB), usbControlTimeout) <= 0) ||
					(errnum = libusb_control_transfer(control->usbLauncher, LIBUSB_DT_HID, LIBUSB_REQUEST_SET_CONFIGURATION, LIBUSB_RECIPIENT_ENDPOINT, 0x01, commandBuffer, 64, usbControlTimeout) <= 0)) {
				    //                                                   0x21,                      0x09,               0x02, 0x01
						std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to send command to device. Error: " << libusb_error_name(errnum) << "." << ConsoleStyle() << std::endl;
						control->currentCommand = NONE;
						control->currentRemainingTime = INT_MIN;
				}
			}
			else if (control->launcherInfo.model == LAUNCHER_CHEEKY) {
				//sufficient for Dream Cheeky launchers
				if (errnum = libusb_control_transfer(control->usbLauncher, LIBUSB_DT_HID, LIBUSB_REQUEST_SET_CONFIGURATION, LIBUSB_RECIPIENT_ENDPOINT, 0x00, commandBuffer, 8, usbControlTimeout) <= 0) {
					std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to send command to device. Error: " << libusb_error_name(errnum) << "." << ConsoleStyle() << std::endl;
					control->currentCommand = NONE;
					control->currentRemainingTime = INT_MIN;
				}
			}
			//if the command was to fire or stop, switch command to NONE
			if (control->currentCommand == STOP || control->currentCommand == FIRE) {
				control->currentCommand = NONE;
				control->currentRemainingTime = INT_MIN;
			}
		}
		//unlock mutex again
		pthread_mutex_unlock(&control->mutex);
		//sleep controlInterval between launcher commands
		usleep(controlInterval * 1000);
	}
}

bool MissileControl::executeCommand(LauncherCommand command, int durationMs)
{
	if (isAvailable()) {
		//if the command is fire or stop, the duration is set to INT_MIN anyway
		if (command == NONE || command == FIRE || command == STOP) {
			durationMs = INT_MIN;
		}
		//else if the duration if smaller than 0, set it to INT_MIN too
		else if (durationMs < 0) {
		    durationMs = INT_MIN;
		}
		pthread_mutex_lock(&mutex);
		currentCommand = command;
		currentRemainingTime = durationMs;
		pthread_mutex_unlock(&mutex);
		return true;
	}
	return false;
}

bool MissileControl::isAvailable() const
{
	return (usbContext != nullptr && usbLauncher != nullptr && active);
}

MissileControl::~MissileControl()
{
	std::cout << "Shutting down missile control." << std::endl;
	if (thread != 0) {
		active = false;
		pthread_join(thread, 0);
		thread = 0;
	}
    if (usbContext) {
        if (usbLauncher != nullptr) {
            libusb_release_interface(usbLauncher, 0);
            libusb_close(usbLauncher);
            usbLauncher = nullptr;
        }
        libusb_exit(usbContext);
        usbContext = nullptr;
    }
}
          
/*
for dream cheeky          
	memset(data, 0, 8);
	if (!strcmp(cmd, "up")) {
		data[0] = 0x01;
	} else if (!strcmp(cmd, "down")) {
		data[0] = 0x02;
	} else if (!strcmp(cmd, "left")) {
		data[0] = 0x04;
	} else if (!strcmp(cmd, "right")) {
		data[0] = 0x08;
	} else if (!strcmp(cmd, "fire")) {
		data[0] = 0x10;
	} else if (strcmp(cmd, "stop")) {
		fprintf(stderr, "Unknown command: 
        */

