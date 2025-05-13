#include <sys/stat.h>  // For mkdir

#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>

#include "defines.hpp"
#include "utils.hpp"

// --- Motion Detection Loop ---
void motionDetectionLoop() {
  cv::Mat frame, gray, prevGray, diff;
  bool transientMotionDetected = false;

  // Ensure recordings directory exists
  mkdir(RECORDINGS_DIR.c_str(),
        0755);  // Read/write/execute for owner, read/execute for group/others

  std::cout << "[MotionDetector] Starting motion detection loop." << std::endl;

  while (true) {
    if (gIsLiveStreamingActive.load(std::memory_order_relaxed)) {
      if (transientMotionDetected) {
        std::cout
            << "[MotionDetector] Pausing motion detection for live stream."
            << std::endl;
        prevGray.release();  // Reset previous frame to avoid false positive
                             // after stream
        transientMotionDetected = false;
      }
      std::this_thread::sleep_for(
          std::chrono::milliseconds(200));  // Sleep while live stream is active
      continue;
    }

    if (!transientMotionDetected) {
      std::cout << "[MotionDetector] Resuming motion detection." << std::endl;
      transientMotionDetected = true;
      // prevGray will be initialized on the next valid frame
    }

    cv::Mat currentFrame;
    {
      std::lock_guard<std::mutex> lock(gCameraMutex);
      if (!gCap.isOpened()) {
        std::cerr << "[MotionDetector] Error: Camera not accessible in loop."
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        continue;
      }
      gCap >> frame;  // Grab frame
    }

    if (frame.empty()) {
      std::cerr << "[MotionDetector] Warning: Empty frame captured."
                << std::endl;
      std::this_thread::sleep_for(
          std::chrono::milliseconds(100));  // Wait a bit before retrying
      continue;
    }

    // It's good practice to clone the frame if it's going to be used by
    // VideoWriter while the original `frame` might be overwritten by the next
    // camera read.
    currentFrame = frame.clone();

    cv::cvtColor(currentFrame, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray,
                     cv::Size(GAUSSIAN_BLUR_SIZE, GAUSSIAN_BLUR_SIZE), 0);

    if (!prevGray.empty()) {
      cv::absdiff(prevGray, gray, diff);
      cv::threshold(diff, diff, THRESHOLD_VALUE, 255, cv::THRESH_BINARY);
      int nonZeroCount = cv::countNonZero(diff);

      if (nonZeroCount > MIN_NON_ZERO_COUNT) {
        auto nowChrono = std::chrono::system_clock::now();
        std::time_t nowC = std::chrono::system_clock::to_time_t(nowChrono);
        std::tm nowTm = *std::localtime(
            &nowC);  // Note: localtime is not thread-safe but okay here

        char filenameBuf[128], timestampBuf[64], prettyTimestampBuf[64];
        std::strftime(filenameBuf, sizeof(filenameBuf),
                      "motion_%Y%m%d_%H%M%S.avi", &nowTm);
        std::strftime(timestampBuf, sizeof(timestampBuf), "%Y-%m-%dT%H:%M:%SZ",
                      &nowTm);  // ISO 8601 like
        std::strftime(prettyTimestampBuf, sizeof(prettyTimestampBuf),
                      "%Y-%m-%d %H:%M:%S", &nowTm);

        std::string videoFilename = RECORDINGS_DIR + "/" + filenameBuf;
        std::cout << "[MotionDetector] Motion detected! Recording "
                  << videoFilename << std::endl;

        cv::VideoWriter videoWriter;
        int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        cv::Size frameSize(
            static_cast<int>(gCap.get(cv::CAP_PROP_FRAME_WIDTH)),
            static_cast<int>(gCap.get(cv::CAP_PROP_FRAME_HEIGHT)));

        // Use the actual FPS from the camera if available and reasonable,
        // otherwise fallback
        double actualFps = gCap.get(cv::CAP_PROP_FPS);
        if (actualFps <= 0 || actualFps > 60)
          actualFps = CAP_FPS;  // Sanity check

        videoWriter.open(videoFilename, fourcc, actualFps, frameSize, true);

        if (!videoWriter.isOpened()) {
          std::cerr << "[MotionDetector] Error: Could not open VideoWriter for "
                    << videoFilename << std::endl;
        } else {
          auto recordingStartTime = std::chrono::steady_clock::now();
          int framesWritten = 0;
          while (std::chrono::steady_clock::now() - recordingStartTime <
                 std::chrono::seconds(VIDEO_RECORD_DURATION_SECONDS)) {
            cv::Mat videoFrame;
            {  // Scope for camera lock
              std::lock_guard<std::mutex> lock(gCameraMutex);
              if (!gCap.isOpened() ||
                  gIsLiveStreamingActive.load(
                      std::memory_order_relaxed)) {  // Check live stream status
                                                     // during recording
                std::cout << "[MotionDetector] Live stream started during "
                             "recording, aborting video save."
                          << std::endl;
                break;
              }
              gCap >> videoFrame;
            }
            if (videoFrame.empty()) break;
            videoWriter.write(videoFrame.clone());  // Write a clone
            framesWritten++;
            // Small delay to roughly match desired FPS if camera is faster
            // This is a simple way; a more precise frame timing mechanism could
            // be used.
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<long long>(1000.0 / actualFps / 2.0)));
          }
          videoWriter.release();
          std::cout << "[MotionDetector] Finished recording " << videoFilename
                    << ", " << framesWritten << " frames." << std::endl;

          if (framesWritten > 0) {  // Only add if some frames were written
            std::lock_guard<std::mutex> lock(gDetectionMutex);
            gRecentDetections.push_front(
                {filenameBuf, timestampBuf,
                 prettyTimestampBuf});  // Store only base filename
            if (gRecentDetections.size() > MAX_RECENT_DETECTIONS) {
              // Optional: Delete the oldest video file from disk
              // std::string oldFile = RECORDINGS_DIR + "/" +
              // g_recentDetections.back().videoFilename;
              // remove(oldFile.c_str());
              gRecentDetections.pop_back();
            }
          } else if (videoWriter.isOpened()) {  // If it was opened but no
                                                // frames, means it was aborted
            // videoWriter.release() was called. If the file was created empty,
            // delete it.
            remove(videoFilename.c_str());
            std::cout
                << "[MotionDetector] Recording aborted, deleted empty file: "
                << videoFilename << std::endl;
          }
        }
      }
    }
    prevGray = gray.clone();
    std::this_thread::sleep_for(std::chrono::milliseconds(
        static_cast<long long>(1000.0 / CAP_FPS)));  // Control processing rate
  }
  std::cout << "[MotionDetector] Exiting motion detection loop." << std::endl;
}
