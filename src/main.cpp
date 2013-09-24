#include <unistd.h>
#include <string.h>
#include <memory>

#include "consolestyle.h"
#include "motiondetector.h"
#include "missilecontrol.h"
#include "keyboard.h"
#include "framebuffer.h"


const char * OPENCV_WINDOW_NAME = "Frame";
std::string inputDevice;
int cameraIndex = 0;
std::string videoFile = "";
bool drawToFramebuffer = false;
bool drawUsingOpenCV = false;
std::shared_ptr<Framebuffer> frameBuffer;
cv::Mat frame;
cv::Mat converted;


void printUsage()
{
    std::cout << "Command line options:" << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-c <INDEX>" << ConsoleStyle() << " - Capture from INDEXth camera." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-f <FILE>" << ConsoleStyle() << " - Capture from video FILE." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-df" << ConsoleStyle() << " - Display video frames in console framebuffer." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-do" << ConsoleStyle() << " - Display video frames using OpenCV." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "-k <DEVICE>" << ConsoleStyle() << " - Use keyboard DEVICE e.g. \"/dev/input/event3\"" << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "? or --help" << ConsoleStyle() << " - Show this help." << std::endl;
    std::cout << "Available keys:" << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "Cursor keys" << ConsoleStyle() << " - Control launcher." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "SPACE" << ConsoleStyle() << " - Stop launcher." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "ENTER" << ConsoleStyle() << " - Fire launcher." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "1" << ConsoleStyle() << " - Arm/unarm launcher." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "a" << ConsoleStyle() << " - Adaptive/fixed binary threshold." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "d/f" << ConsoleStyle() << " - De-/increase binary threshold." << std::endl;
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "ESC" << ConsoleStyle() << " - Quit program." << std::endl;
}

bool parseCommandLine(int argc, char * argv[])
{
    //parse command line arguments
    for(int i = 1; i < argc; ++i) {
        //read argument from list
        std::string argument = argv[i];
        //check what it is
        if (argument == "?" || argument == "--help") {
            //print help
            printUsage();
            return false;
        }
        else if (argument == "-k") {
            //read keyboard input device from next argument
            if (++i < argc) {
                inputDevice = argv[i];
            }
            else {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "Option -k needs an argument!" << ConsoleStyle() << std::endl;
                printUsage();
                return false;
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
                return false;
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
                return false;
            }
        }
        else if (argument == "-df") {
            //enable drawing of camera/video frames to console framebuffer
            if (drawUsingOpenCV) {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "-df and -do are mutually exclusive!" << ConsoleStyle() << std::endl;
                return false;
            }
            drawToFramebuffer = true;
        }
        else if (argument == "-do") {
            //enable drawing of camera/video frames with OpenCV
            if (drawToFramebuffer) {
                std::cout << ConsoleStyle(ConsoleStyle::RED) << "-do and -df are mutually exclusive!" << ConsoleStyle() << std::endl;
                return false;
            }
            drawUsingOpenCV = true;
        }
        else {
            std::cout << ConsoleStyle(ConsoleStyle::RED) << "Error: Unknown argument \"" << argument << "\"!" << ConsoleStyle() << std::endl;
            return false;
        }
    }
    return true;
}

int main(int argc, char * argv[])
{
    std::cout << ConsoleStyle(ConsoleStyle::CYAN) << "MeeZeeMissile v0.8 - Motion detection and USB launcher control." << ConsoleStyle() << std::endl;
    
    if (!parseCommandLine(argc, argv)) {
        return -1;
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
    //if the user wants to draw to framebuffer, create one
    if (drawToFramebuffer) {
        frameBuffer = std::make_shared<Framebuffer>();
	    if (!frameBuffer->isAvailable()) {
		    std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to initialize framebuffer!" << ConsoleStyle() << std::endl;
		    return -5;
	    }
	}
	if (drawUsingOpenCV) {
	    //create window
	    cv::namedWindow(OPENCV_WINDOW_NAME);
	}
	MissileControl missileControl;
	if (!missileControl.isAvailable()) {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to initialize missile control!" << ConsoleStyle() << std::endl;
		return -6;
	}

    //start detection and control loop
	while (keyboard.isAvailable()) { // && motionDetector.isAvailable())
		if (keyboard.keyWasPressed(1)) {
			break;
		}
		else if (keyboard.keyWasPressed(2)) {
		    missileControl.setArmed(!missileControl.isArmed());
		    if (missileControl.isArmed())
		        std::cout << "Launcher armed!" << std::endl;
		    else
		        std::cout << "Launcher unarmed!" << std::endl;
		}
		else if (keyboard.keyWasPressed(30)) {
		    motionDetector.setUseAdaptiveThreshold(!motionDetector.getUseAdaptiveThreshold());
		    if (motionDetector.getUseAdaptiveThreshold())
		        std::cout << "Using adaptive threshold." << std::endl;
		    else
		        std::cout << "Using fixed threshold of " << motionDetector.getBinaryThreshold() << "." << std::endl;
		}
		else if (keyboard.keyWasPressed(32) && motionDetector.getBinaryThreshold() >= 5.0) {
		    motionDetector.setBinaryThreshold(motionDetector.getBinaryThreshold() - 5.0);
		    std::cout << "Binary threshold: " << motionDetector.getBinaryThreshold() << "." << std::endl;
		}
		else if (keyboard.keyWasPressed(33) && motionDetector.getBinaryThreshold() <= 250.0) {
		    motionDetector.setBinaryThreshold(motionDetector.getBinaryThreshold() + 5.0);
		    std::cout << "Binary threshold: " << motionDetector.getBinaryThreshold() << "." << std::endl;
		}
		else if (keyboard.keyWasPressed(105)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::LEFT, 250);
		}
		else if (keyboard.keyWasPressed(106)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::RIGHT, 250);
		}
		else if (keyboard.keyWasPressed(103)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::UP, 250);
		}
		else if (keyboard.keyWasPressed(108)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::DOWN, 250);
		}
		else if (keyboard.keyWasPressed(57)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::STOP);
		}
		else if (keyboard.keyWasPressed(28)) {
		    missileControl.executeCommand(MissileControl::LauncherCommand::FIRE);
		}
		//clear list of pressed keys
		keyboard.clearPressedKeys();
		//draw image to framebuffer or OpenCV window
        if (drawToFramebuffer && frameBuffer->isAvailable()) {
            if (motionDetector.getLastFrame(frame, true) && !frame.empty()) {
                //convert frame to screen depth
                MotionDetector::convertFrame(converted, frame, CV_8U);
                //draw frame to screen
                frameBuffer->drawBuffer(frameBuffer->getWidth() - converted.size().width, 0, converted.ptr<const unsigned char>(), converted.size().width, converted.size().height, 24);
            }
        }
        if (drawUsingOpenCV) {
            if (motionDetector.getLastFrame(frame, true) && !frame.empty()) {
                cv::imshow(OPENCV_WINDOW_NAME, frame);
                cv::waitKey(5);
            }
        }
        //check if the missile launcher is directed at the center of the motion
        MotionDetector::MotionInformation motionInfo;
        if (motionDetector.getLastMotion(motionInfo) && motionInfo.motionDetected) {
            if (motionInfo.distance2 < 60) {
                missileControl.executeCommand(MissileControl::LauncherCommand::FIRE);
                std::cout << "Motion close to target. Shooting!" << std::endl;
            }
        }
        //usleep(1 * 1000);
	}

	return 0;
}

