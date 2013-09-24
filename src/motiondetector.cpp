#include "motiondetector.h"

#include <iostream>
#include <unistd.h>

//#define DO_TIMING
#ifdef DO_TIMING
    #include <time.h>
#endif

#include "consolestyle.h"


MotionDetector::MotionDetector()
	: motionChanged(false),
	  thread(0), mutex(PTHREAD_MUTEX_INITIALIZER), active(false), paused(false),
	  videoWidth(0), videoHeight(0), videoFps(0.0), videoBitsPerColor(0), videoColors(0),
      pollingInterval(0), frameNr(0), framesToIgnore(0), frameChanged(false),
      useMorphology(false), useAdaptiveThreshold(false), binaryThreshold(70.0)
{
}

bool MotionDetector::openVideo(const std::string & fileName, uint32_t width, uint32_t height, double fps)
{
	if(videoCapture.open(fileName)) {
		std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Opened video file \"" << fileName << "\" for motion detection." << ConsoleStyle() << std::endl;
		return setupCapture(videoCapture, width, height, fps);
	}
	else {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to open video file \"" << fileName << "\" for motion detection!" << ConsoleStyle() << std::endl;
	}
    return false;
}

bool MotionDetector::openCamera(int cameraIndex, uint32_t width, uint32_t height, double fps)
{
	if(videoCapture.open(cameraIndex)) {
		std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Opened camera #" << cameraIndex << " for motion detection." << ConsoleStyle() << std::endl;
		return setupCapture(videoCapture, width, height, fps);
	}
	else {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to open camera #" << cameraIndex << " for motion detection!" << ConsoleStyle() << std::endl;
	}
    return false;
}

bool MotionDetector::setupCapture(cv::VideoCapture & captureDevice, uint32_t width, uint32_t height, double fps)
{
	//try capturing at wanted resolution and fps
	captureDevice.set(CV_CAP_PROP_FRAME_WIDTH, width);
	captureDevice.set(CV_CAP_PROP_FRAME_HEIGHT, height);
	captureDevice.set(CV_CAP_PROP_FPS, fps);
	//poll first frame for checking values
	cv::Mat frame;
	if (captureDevice.grab() && captureDevice.retrieve(frame)) {
		//get frame values
		videoWidth = frame.size().width;
		videoHeight = frame.size().height;
        videoColors = frame.channels();
        switch (frame.depth()) {
            case CV_8U:
            case CV_8S:
                videoBitsPerColor = 8;
                break;
            case CV_16U:
            case CV_16S:
                videoBitsPerColor = 16;
                break;
            case CV_32S:
            case CV_32F:
                videoBitsPerColor = 32;
                break;
            case CV_64F:
                videoBitsPerColor = 64;
                break;
        }
		const double newFps = captureDevice.get(CV_CAP_PROP_FPS);
		if (newFps > 0.0) {
			videoFps = newFps;
		}
		else {
			std::cout << ConsoleStyle(ConsoleStyle::YELLOW) << "Failed to properly set fps!" << ConsoleStyle() << std::endl;
			videoFps = fps;
		}
		std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Capturing at " << videoWidth << "x" << videoHeight << "@" << videoBitsPerColor * videoColors << "bpp with " << videoFps << " frames/s now." << ConsoleStyle() << std::endl;
		//calculate polling interval. It's a bit less than the frame interval, to not skip any frames
		pollingInterval = 1000.0 / videoFps * 0.9;
		//calculate the number of frames to ignore before starting detection
        framesToIgnore = 3.0 * videoFps;
		//set up images needed for motion detection
		const cv::Size imageSize(videoWidth, videoHeight);
		greyFrame = cv::Mat(imageSize, CV_8U);
		movingAverage = cv::Mat(imageSize, CV_32F);
		averageGrey = cv::Mat(imageSize, CV_8U);
		difference = cv::Mat(imageSize, CV_8U);
		//start frame polling thread
		active = true;
		if (pthread_create(&thread, 0, &MotionDetector::frameLoop, this) == 0) {
			std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Started frame polling thread." << ConsoleStyle() << std::endl;
            return true;
		}
		else {
			std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to start frame polling thread!" << ConsoleStyle() << std::endl;
			thread = 0;
			active = false;
			captureDevice.release();
		}
	}
	else {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to grab first frame!" << ConsoleStyle() << std::endl;
		captureDevice.release();
	}
    return false;
}

void MotionDetector::pauseDetection(bool pause)
{
    paused = pause;
    //if the detection was unpaused, clear the number of frames to clear the average start over with a fresh frame
    if (!paused) {
        frameNr = 0;
    }
}

bool MotionDetector::isPaused() const
{
    return paused;
}

bool MotionDetector::isAvailable() const
{
	return (videoCapture.isOpened() && active);
}

uint32_t MotionDetector::getWidth() const
{
	return videoWidth;
}

uint32_t MotionDetector::getHeight() const
{
	return videoHeight;
}

double MotionDetector::getFps() const
{
	return videoFps;
}

void MotionDetector::setUseMorphology(bool enable)
{
	useMorphology = enable;
}

bool MotionDetector::getUseMorphology() const
{
    return useMorphology;
}

void MotionDetector::setUseAdaptiveThreshold(bool enable)
{
    useAdaptiveThreshold = enable;
}

bool MotionDetector::getUseAdaptiveThreshold() const
{
    return useAdaptiveThreshold;
}

void MotionDetector::setBinaryThreshold(double threshold)
{
    binaryThreshold = threshold;
}

double MotionDetector::getBinaryThreshold() const
{
    return binaryThreshold;
}

bool MotionDetector::getLastMotion(MotionInformation & motionInfo)
{
	bool result = false;
	//block mutex for member variables
	pthread_mutex_lock(&mutex);
	if (motionChanged) {
		motionInfo = lastMotion;
		motionChanged = false;
		result = true;
	}
	pthread_mutex_unlock(&mutex);
	return result;
}

bool MotionDetector::getLastFrame(cv::Mat & lastFrame, bool drawMotion)
{
	bool result = false;
	//block mutex for member variables
	pthread_mutex_lock(&mutex);
	if (frameChanged) {
		lastFrame = frame.clone();
		//overlay frame motion rect if caller wants to and if motion was detected
		if (drawMotion && lastMotion.motionDetected) {
			cv::rectangle(lastFrame, cv::Point(lastMotion.x, lastMotion.y), cv::Point(lastMotion.x + lastMotion.w, lastMotion.y + lastMotion.h), CV_RGB(255, 0, 0));
			//mark center of motion
			cv::line(lastFrame, cv::Point(lastMotion.cx - 3, lastMotion.cy), cv::Point(lastMotion.cx + 3, lastMotion.cy), CV_RGB(255, 0, 0));
			cv::line(lastFrame, cv::Point(lastMotion.cx, lastMotion.cy - 3), cv::Point(lastMotion.cx, lastMotion.cy + 3), CV_RGB(255, 0, 0));
			//mark center of frame
			const int cx = lastFrame.size().width / 2;
			const int cy = lastFrame.size().height / 2;
			cv::line(lastFrame, cv::Point(cx - 3, cy), cv::Point(cx + 3, cy), CV_RGB(0, 255, 0));
			cv::line(lastFrame, cv::Point(cx, cy - 3), cv::Point(cx, cy + 3), CV_RGB(0, 255, 0));
		}
		frameChanged = false;
		result = true;
	}
	pthread_mutex_unlock(&mutex);
	return result;
}

bool MotionDetector::convertFrame(cv::Mat & destination, const cv::Mat & source, int bpp)
{
    source.convertTo(destination, bpp);
}

void * MotionDetector::frameLoop(void * obj)
{
	MotionDetector * detector = reinterpret_cast<MotionDetector *>(obj);
	//start thread loop
    while (detector != nullptr && detector->active) {
	    //block mutex for member variables
    	pthread_mutex_lock(&detector->mutex);
		//grab frame from camera/video if there are any
		if (!detector->paused && detector->videoCapture.grab()) {
			//frame's there, retrieve it
			if (detector->videoCapture.retrieve(detector->frame)) {
#ifdef DO_TIMING
                timespec startTime;
                clock_gettime(CLOCK_THREAD_CPUTIME_ID, &startTime);
#endif
				//convert image to greyscale
				cv::cvtColor(detector->frame, detector->greyFrame, CV_BGR2GRAY);
				//check if first frame
				if (detector->frameNr++ == 0) {
					//on first frame only copy image to running average
					detector->greyFrame.convertTo(detector->movingAverage, CV_32F);//, 1.0, 0.0);
				}
				else if (detector->frameNr < detector->framesToIgnore) {
				    //accumulate frames, but nothing more
					cv::accumulateWeighted(detector->greyFrame, detector->movingAverage, 0.10);
				}
				else {
					//accumulate frames
					cv::accumulateWeighted(detector->greyFrame, detector->movingAverage, 0.050);
					//convert moving average back to 8bit
					detector->movingAverage.convertTo(detector->averageGrey, CV_8U);
					//calculate difference between average and current frame
					cv::absdiff(detector->averageGrey, detector->greyFrame, detector->difference);
					//convert to binary image
					if (detector->useAdaptiveThreshold) {
					    cv::adaptiveThreshold(detector->difference, detector->difference, 255.0, cv::ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 3, -5);
					}
					else {
					    cv::threshold(detector->difference, detector->difference, detector->binaryThreshold, 255.0, CV_THRESH_BINARY);
					}
					//detector->difference.convertTo(detector->frame, CV_8U);
					//use different paths if the user wants to use morphology functions
					std::vector<std::vector<cv::Point>> contours;
					std::vector<cv::Vec4i> hierarchy;
					if (detector->useMorphology) {
						//perform morphological close operation to fill in the gaps in the binary image
						//cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(10, 10));
						cv::morphologyEx(detector->difference, detector->difference, cv::MORPH_CLOSE, cv::Mat(), cv::Point(-1, -1), 8);
						//create contours from binary image
						cv::findContours(detector->difference, contours, hierarchy, CV_CHAIN_APPROX_TC89_L1, CV_CHAIN_APPROX_SIMPLE);
					}
					else {
						//dilate and erode to get better blobs in the binary image
						cv::dilate(detector->difference, detector->difference, cv::Mat(), cv::Point(-1, -1), 12);
						cv::erode(detector->difference, detector->difference, cv::Mat(), cv::Point(-1, -1), 8);
						//create contours from binary image
						//CV_RETR_EXTERNAL, CV_RETR_CCOMP, CV_CHAIN_APPROX_TC89_L1, CV_CHAIN_APPROX_TC89_KCOS
						cv::findContours(detector->difference, contours, hierarchy, CV_CHAIN_APPROX_TC89_L1, CV_CHAIN_APPROX_SIMPLE);
					}
					//analyze contours and find biggest contour
					cv::Rect biggestRect;
					auto biggestContour = contours.cend();
					for(auto cIt = contours.cbegin(); cIt != contours.cend(); ++cIt) {
						//bounding rectangle around the contour
						cv::Rect rect = cv::boundingRect(*cIt);
						if (rect.area() > biggestRect.area()) {
							biggestRect = rect;
							biggestContour = cIt;
						}
					}
					//store biggest contour if one exists
					if (biggestContour != contours.cend() && biggestRect.area() > 40) {
						detector->lastMotion.motionDetected = true;
						detector->lastMotion.x = biggestRect.x;
						detector->lastMotion.y = biggestRect.y;
						detector->lastMotion.w = biggestRect.width;
						detector->lastMotion.h = biggestRect.height;
						detector->lastMotion.cx = biggestRect.x + biggestRect.width / 2;
						detector->lastMotion.cy = biggestRect.y + biggestRect.height / 2;
						//calculate distance to frame center
                        int dx = (detector->frame.size().width / 2 - detector->lastMotion.cx);
                        int dy = (detector->frame.size().height / 2 - detector->lastMotion.cy);
                        detector->lastMotion.distance2 = dx * dx + dy * dy;
					}
					else {
						detector->lastMotion.motionDetected = false;
					}
					detector->motionChanged = true;
					detector->frameChanged = true;
				}
#ifdef DO_TIMING
                timespec endTime;
                clock_gettime(CLOCK_THREAD_CPUTIME_ID, &endTime);
                static float miliseconds = 0;
                miliseconds += (endTime.tv_nsec - startTime.tv_nsec) / (1000.0f * 1000.0f);
                if (detector->frameNr % 10 == 0) {
                    std::cout << "Processing time " << miliseconds / 10.0f << "ms." << std::endl;
                    miliseconds = 0;
                }
#endif
			}
		}
		//unlock mutex again
		pthread_mutex_unlock(&detector->mutex);
		//sleep between polling camera frames
		usleep(detector->pollingInterval * 1000);
	}
}

MotionDetector::~MotionDetector()
{
	std::cout << "Shutting down motion detector." << std::endl;
	if (thread != 0) {
		active = false;
		pthread_join(thread, 0);
		thread = 0;
	}
	if (videoCapture.isOpened()) {
		videoCapture.release();
	}
	motionChanged = false;
	frameChanged = false;
}

