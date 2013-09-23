#include <unistd.h>
#include <string.h>

#include "consolestyle.h"
#include "motiondetector.h"
#include "missilecontrol.h"
#include "keyboard.h"
#include "framebuffer.h"


void printUsage()
{
    std::cout << "Command line options:" << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-c <INDEX>" << ConsoleStyle() << " - Capture from INDEXth camera." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-f <FILE>" << ConsoleStyle() << " - Capture from video FILE." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-d" << ConsoleStyle() << " - Display video frames in framebuffer." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-k" << ConsoleStyle() << " <DEVICE> - Use keyboard DEVICE e.g. \"/dev/input/event3\"" << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "? or --help" << ConsoleStyle() << " - This help." << std::endl;
    std::cout << "Available keys:" << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "Cursor keys" << ConsoleStyle() << " - Control launcher." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "SPACE" << ConsoleStyle() << " - Stop launcher." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "ENTER" << ConsoleStyle() << " - Fire launcher." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "ESC" << ConsoleStyle() << " - Quit program." << std::endl;
}

int main(int argc, char * argv[])
{
    std::string inputDevice;
    int cameraIndex = 0;
    std::string videoFile = "";
    bool drawFrames = false;

    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "MeeZeeMissile v0.8 - Motion detection and USB launcher control." << ConsoleStyle() << std::endl;

    //parse command line arguments
    for(int i = 1; i < argc; ++i) {
        //read argument from list
        std::string argument = argv[i];
        //check what it is
        if (argument == "?" || argument == "--help") {
            //print help
            printUsage();
            return 0;
        }
        else if (argument == "-k") {
            //read keyboard input device from next argument
            if (++i < argc) {
                inputDevice = argv[i];
            }
            else {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "Option -k needs an argument!" << ConsoleStyle() << std::endl;
                printUsage();
                return -1;
            }
        }
        else if (argument == "-c") {
            //read camera index from next argument
            if (++i < argc) {
                std::stringstream ss(argv[i]);
                ss >> cameraIndex;
            }
            else {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "Option -c needs an argument!" << ConsoleStyle() << std::endl;
                printUsage();
                return -1;
            }
        }
        else if (argument == "-f") {
            //read video file from next argument
            if (++i < argc) {
                videoFile = argv[i];
            }
            else {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "Option -f needs an argument!" << ConsoleStyle() << std::endl;
                printUsage();
                return -1;
            }
        }
        else if (argument == "-d") {
            //enable drawing of camera/video frames
            drawFrames = true;
        }
        else {
            std::cout << ConsoleStyle(ConsoleStyle::RED) << "Error: Unknown argument \"" << argument << "\"!" << ConsoleStyle() << std::endl;
            return -1;
        }
    }

    //check for proper priviliges
    if (geteuid() != 0) {
        std::cout << ConsoleStyle(ConsoleStyle::RED) << "You might need root privileges for raw keyboard and framebuffer access!" << ConsoleStyle() << std::endl;
        return -2;
    }
    
    //initialize interfaces
    Keyboard keyboard(inputDevice);
    if (!keyboard.isAvailable()) {
    std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to initialize keyboard interface!" << ConsoleStyle() << std::endl;
        return -3;
    }
    MotionDetector motionDetector;
    if (videoFile.empty() && cameraIndex >= 0) {
        if (!motionDetector.openCamera(cameraIndex) || !motionDetector.isAvailable()) {
            std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to initialize motion detector!" << ConsoleStyle() << std::endl;
            return -4;
        }
    }
    else if (!videoFile.empty()) {
        if (!motionDetector.openVideo(videoFile) || !motionDetector.isAvailable()) {
            std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to initialize motion detector!" << ConsoleStyle() << std::endl;
            return -4;
        }
    }
    Framebuffer frameBuffer;
	if (!frameBuffer.isAvailable()) {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to initialize framebuffer!" << ConsoleStyle() << std::endl;
		return -5;
	}
	/*MissileControl missileControl;
	if (!missileControl.isAvailable()) {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to initialize missile control!" << ConsoleStyle() << std::endl;
		return -6;
	}*/

    cv::Mat frame;
    cv::Mat converted;

    //start detection and control loop
	while (keyboard.isAvailable()) // && motionDetector.isAvailable())
	{
		if (keyboard.isKeyDown(1)) {
			break;
		}
		/*else if (keyboard.isKeyDown(105)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::LEFT, 100);
		}
		else if (keyboard.isKeyDown(106)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::RIGHT, 100);
		}
		else if (keyboard.isKeyDown(103)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::UP, 100);
		}
		else if (keyboard.isKeyDown(108)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::DOWN, 100);
		}
		else if (keyboard.isKeyDown(57)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::STOP);
		}
		else if (keyboard.isKeyDown(28)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::FIRE);
		}*/
		MotionDetector::MotionInformation motionInfo;
		if (motionDetector.getLastMotion(motionInfo) && motionInfo.motionDetected) {
		    std::cout << "Motion at (" << motionInfo.cx << "," << motionInfo.cy << ")." << std::endl;
		}
        if (drawFrames) {
            if (motionDetector.getLastFrame(frame, true) && !frame.empty()) {
                //convert frame to screen depth
                MotionDetector::convertFrame(converted, frame, CV_8U);
                //draw frame to screen
                frameBuffer.drawBuffer(frameBuffer.getWidth() - converted.size().width, 0, converted.ptr<const unsigned char>(), converted.size().width, converted.size().height, 24);
            }
        }
	}

	return 0;
}

