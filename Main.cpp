#include "Capture.h"
#include <thread>

using namespace std;
using namespace cv;

map<milliseconds, Frame> frames;
vector<map<milliseconds, vector<Point>>> allTracks;
mutex mutex_frames, mutex_tracks;

const int cameraNumber = 0;
const string fileName = R"(/home/klm/work/td_marco/images/video/video_test.mp4)";
// const string fileName = R"(/home/klm/work/gitwork/opencv2_cookbook_code/3241OS_images/images/bike.avi)";
//const string fileName = R"(/home/drew/ClionProjects/detection/TB.mp4)";

int main()
{
#ifdef USE_CAMERA
    // 使用摄像头视频流
    Capture capture(cameraNumber);
    if (!capture.isOpened())
    {
        cout << "Error: Camera #" << cameraNumber << " is not available." << endl;
        return -1;
    }
#else
    // 使用视频文件检测    
    const char* file = fileName.c_str();
    FILE* f = fopen(file, "r");
    if (f == NULL) {
        cout << "Error: File "<< file << " is not found." << endl;
        return -1;
    }
    fclose(f);
    
    Capture capture(fileName);
#endif

    /// 创建检测线程和显示线程
    thread capturing( &Capture::find, 
                      &capture, 
                      ref(frames), ref(mutex_frames), 
                      ref(allTracks), ref(mutex_tracks)
                    );
    thread display( &Capture::display, 
                    &capture, 
                    ref(frames), ref(mutex_frames), 
                    ref(allTracks), ref(mutex_tracks)
                  );

    // 等待线程结束
    if (capturing.joinable()) {
        capturing.join();
    }
    if (display.joinable()) {
        display.join();
    }

    return 0;
}

