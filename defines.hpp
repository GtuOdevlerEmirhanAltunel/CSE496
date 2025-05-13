#ifndef DEFINES
#define DEFINES

#include <string>

// --- Configuration ---
const int CAMERA_INDEX = 0;     // Adjust if your camera is not /dev/video0
const double CAP_WIDTH = 1280;  // Frame width for processing
const double CAP_HEIGHT = 720;  // Frame height for processing
const double CAP_FPS = 30.0;    // Desired camera FPS
const int HTTP_PORT = 8080;
const std::string RECORDINGS_DIR = "recordings";  // Directory to save videos

// Motion detection parameters
const int GAUSSIAN_BLUR_SIZE =
    15;  // Kernel size for Gaussian blur (odd number)
const double THRESHOLD_VALUE = 25;  // Threshold for detecting changes
const int MIN_NON_ZERO_COUNT =
    1000;  // Minimum non-zero pixels to trigger motion (adjust based on
           // resolution and sensitivity)
const int VIDEO_RECORD_DURATION_SECONDS =
    5;  // Duration of recorded video clips
const int MAX_RECENT_DETECTIONS =
    20;  // Max number of video clips to keep track of

#endif /* DEFINES */
