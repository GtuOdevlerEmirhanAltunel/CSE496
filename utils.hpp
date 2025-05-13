#ifndef UTILS
#define UTILS

#include <atomic>
#include <deque>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>

// --- Global Shared Resources ---
struct MotionArtifact {
  std::string video_filename;
  std::string timestamp;
  std::string pretty_timestamp;  // For display
};

std::string sanitizeFilename(const std::string& filename);
void startHttpServer();
void handleHttpClient(int clientFd);
void motionDetectionLoop();

extern std::deque<MotionArtifact> gRecentDetections;
extern std::mutex gDetectionMutex;  // Protects g_recentDetections
extern cv::VideoCapture gCap;
extern std::mutex gCameraMutex;  // Protects g_cap
extern std::atomic<bool> gIsLiveStreamingActive;
extern std::atomic<int>
    gLiveStreamClientCount;  // Number of active live stream clients

#endif /* UTILS */
