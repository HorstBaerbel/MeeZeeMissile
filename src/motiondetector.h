#pragma once

#include <string>
#include <pthread.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>


class MotionDetector
{
public:
	struct MotionInformation
	{
		bool motionDetected;
		uint32_t x; //!<upper left position.
		uint32_t y; //!<upper left position.
		uint32_t cx; //!<center x.
		uint32_t cy; //!<center y.
		uint32_t w; //!<width.
		uint32_t h; //!<height.
		uint32_t distance2; //!<squared distance of motion center to frame center.

		MotionInformation()
			: x(0), y(0), w(0), h(0), cx(0), cy(0), distance2(0), motionDetected(false) {};
		MotionInformation(uint32_t px, uint32_t py, uint32_t width, uint32_t height)
			: x(px), y(py), w(width), h(height), cx(x + w / 2), cy(y + h / 2), distance2(0), motionDetected(false) {};
	};

private:
	MotionInformation lastMotion; //!<Information of last motion detected or not.
	bool motionChanged; //!<If the motion information has changed from the last getLastMotion() call.

	pthread_t thread; //!<frame polling thread.
	pthread_mutex_t mutex; //!<The mutex protecting the lastMotion and motionChanged members.
	bool active; //!<flags to keep the thread running or stop it.
	bool paused; //!<Flag to pause motion detection loop. No detection will be done till flas is false.

	cv::VideoCapture videoCapture; //!<OpenCV video capture object.
	uint32_t videoWidth; //!<Width of video frames.
	uint32_t videoHeight; //!<Height of video frames.
    uint32_t videoBitsPerColor; //!<Bits per color of video frames.
    uint32_t videoColors; //!<Number of channels in video frame, e.g. 3 for RGB.
	double videoFps; //!<Fps of video capture.
	uint32_t pollingInterval; //!<Time to sleep between polling frames.
	uint32_t frameNr; //!<Nr of frame captured from device.
	uint32_t framesToIgnore; //!<Nr of frames ignore after starting or unpausing motion detection.

	bool frameChanged; //!<True if the frame has changed from the last getLastFrame() call.
	cv::Mat frame; //!<Last captured frame.
	cv::Mat greyFrame; //!<Captured frame converted to grayscale.
	cv::Mat movingAverage; //!<Moving average of captured frames.
	cv::Mat averageGrey; //!<Moving average as greyscale image.
	cv::Mat difference; //!<difference between average greyscale and current greyscale frame.
	
	bool useMorphology; //!<Set to true to use OpenCV morphology filter.
	bool useAdaptiveThreshold; //!<Set to true to use adaptive threshold instead of fixed threshold.
	double binaryThreshold; //!<Threshold when converting greyscale image to binary.

	bool setupCapture(cv::VideoCapture & captureDevice, uint32_t width = 320, uint32_t height = 240, double fps = 20.0);

	static void * frameLoop(void * obj);

public:
    /*!
    Construct MotionDetector object.
    \note Does nothing without a \openVideo or \openCamera call...
    */
	MotionDetector();

    /*!
    Open video file for motion detection.
    \param[in] fileName Video file open.
    \param[in] width Optional. Preferred width of video mode.
    \param[in] height Optional. Preferred height of video mode.
    \param[in] fps Optional. Preferred frames/s of capture.
    \note You can not rely on the width/height/fps you passed being used as the actual mode! Always check!
    */
    bool openVideo(const std::string & fileName, uint32_t width = 320, uint32_t height = 240, double fps = 20.0);

    /*!
    Open camera for motion detection.
    \param[in] cameraIndex Optional. Index of camera to open.
    \param[in] width Optional. Preferred width of video mode.
    \param[in] height Optional. Preferred height of video mode.
    \param[in] fps Optional. Preferred frames/s of capture.
    \note You can not rely on the width/height/fps you passed being used as the actual mode! Always check!
    */
    bool openCamera(int cameraIndex = 0, uint32_t width = 320, uint32_t height = 240, double fps = 20.0);
    
    /*!
    Pause motion detection loop. No more frames will be analyzed till it is unpaused.
    \param[in] pause Pass true to pause motion detection loop.
    */
    void pauseDetection(bool pause);
    
    /*!
    Check if motion detection is currently paused.
    */
    bool isPaused() const;

    /*!
    Check if the motion detector is ready to be used.
    \return Returns true if the motion detector is capturing and anaysing images.
    */
	bool isAvailable() const;

	uint32_t getWidth() const;
	uint32_t getHeight() const;
	double getFps() const;

	/*!
	Returns true if the motion information has changed from the previous call to this one.
	\param[out] motionInfo Motion information struct that will be modified when the function returns true.
	\return Returns true if motion information has changed since last call.
	*/
	bool getLastMotion(MotionInformation & motionInfo);

	/*!
	Returns true if the frame has changed from the previous call to this one.
	\param[out] lastFrame last captured frame returned if the function returns true.
	\param[in] drawMotion Pass true for the function to draw a frame over the area motion was detected in.
	\return Returns true if the frame has changed since the last call.
	*/
	bool getLastFrame(cv::Mat & lastFrame, bool drawMotion = false);

    /*!
    Convert frame to other bit depth.
    \param[out] destination The conversion target.
    \param[in] source The conversion source.
    \param[in] bpp Wanted bit depth.
    */
    static bool convertFrame(cv::Mat & destination, const cv::Mat & source, int bpp);

    /*!
    Combine motion areas using OpenCV morphology algorithm.
    \param[in] enable Pass true to enable algorithm on next frame.
    */
	void setUseMorphology(bool enable);
	bool getUseMorphology() const;
	
	/*!
	Convert greyscale image to binary using adaptive threashold.
	\param[in] enable Pass true to enable adaptive threshold on next frame.
	*/
	void setUseAdaptiveThreshold(bool enable);
	bool getUseAdaptiveThreshold() const;
	
	/*!
	Set greyscale to binary conversion threshold.
	\param[in] threshold New threshold for conversion.
	*/
	void setBinaryThreshold(double threshold = 50.0);
	double getBinaryThreshold() const;

	~MotionDetector();
};
