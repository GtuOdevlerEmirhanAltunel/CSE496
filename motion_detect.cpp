#include <sys/stat.h>  // For mkdir

#include <thread>

#include "defines.hpp"
#include "utils.hpp"

std::deque<MotionArtifact> gRecentDetections;
std::mutex gDetectionMutex;  // Protects g_recentDetections

cv::VideoCapture gCap;
std::mutex gCameraMutex;  // Protects g_cap

std::atomic<bool> gIsLiveStreamingActive{false};
std::atomic<int> gLiveStreamClientCount{0};

// --- Main Function ---
int main() {
  std::cout << "Application starting..." << std::endl;
  std::cout << "Attempting to open camera index: " << CAMERA_INDEX << std::endl;
  std::cout << "Requested Resolution: " << CAP_WIDTH << "x" << CAP_HEIGHT
            << " @ " << CAP_FPS << "FPS" << std::endl;

  {  // Scope for camera lock during initialization
    std::lock_guard<std::mutex> lock(gCameraMutex);
    gCap.open(CAMERA_INDEX);
    if (!gCap.isOpened()) {
      // Try GStreamer pipeline as a fallback for Raspberry Pi
      std::string gstPipeline =
          "v4l2src device=/dev/video" + std::to_string(CAMERA_INDEX) +
          " ! video/x-raw,width=" +
          std::to_string(static_cast<int>(CAP_WIDTH)) +
          ",height=" + std::to_string(static_cast<int>(CAP_HEIGHT)) +
          ",framerate=" + std::to_string(static_cast<int>(CAP_FPS)) + "/1" +
          " ! videoconvert ! appsink";
      std::cout << "Camera index " << CAMERA_INDEX
                << " failed. Trying GStreamer: " << gstPipeline << std::endl;
      gCap.open(gstPipeline, cv::CAP_GSTREAMER);
    }

    if (!gCap.isOpened()) {
      std::cerr << "Error: Could not open camera using index " << CAMERA_INDEX
                << " or GStreamer. Exiting." << std::endl;
      return -1;
    }

    // Set camera parameters (some might not be settable if using GStreamer
    // pipeline string directly)
    gCap.set(cv::CAP_PROP_FRAME_WIDTH, CAP_WIDTH);
    gCap.set(cv::CAP_PROP_FRAME_HEIGHT, CAP_HEIGHT);
    // gCap.set(cv::CAP_PROP_FPS, CAP_FPS);
  }

  std::cout << "Camera opened successfully." << std::endl;
  std::cout << "Actual Resolution: " << gCap.get(cv::CAP_PROP_FRAME_WIDTH)
            << "x" << gCap.get(cv::CAP_PROP_FRAME_HEIGHT) << std::endl;
  std::cout << "Actual FPS: " << gCap.get(cv::CAP_PROP_FPS) << std::endl;

  // Ensure recordings directory exists before starting threads that might use
  // it
  if (mkdir(RECORDINGS_DIR.c_str(), 0755) != 0 && errno != EEXIST) {
    perror("Failed to create recordings directory");
    // Decide if this is a fatal error
  } else {
    std::cout << "Recordings will be saved to ./" << RECORDINGS_DIR
              << std::endl;
  }

  std::thread camThread(motionDetectionLoop);
  std::thread webThread(startHttpServer);

  std::cout << "Main: Motion detection and HTTP server threads started."
            << std::endl;

  camThread.join();
  webThread.join();

  {  // Scope for camera lock during release
    std::lock_guard<std::mutex> lock(gCameraMutex);
    if (gCap.isOpened()) {
      gCap.release();
    }
  }
  std::cout << "Application terminated." << std::endl;
  return 0;
}
