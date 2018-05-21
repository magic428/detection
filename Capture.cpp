#include "Capture.h"

using namespace std;
using namespace chrono;


Capture::Capture(int cameraNumber)
{
    capture.open(cameraNumber);
}

Capture::Capture(string fileName)
{
    capture.open(fileName);
}

Capture::~Capture()
{
    capture.release();

}

bool Capture::isOpened()
{
    return capture.isOpened();
}


Mat Capture::getFrame()
{
    Mat m;
    return m;
}


vector<Rect> Capture::uniteRect(vector<vector<Point>> contours)
{
    vector<Rect> rects;
    for (auto i = contours.begin(); i != contours.end(); i++)
    {
        rects.push_back(boundingRect(*i));
    }
    bool isCrossed = true;
//	Rect stub(1, 1, 1, 1);
    while (isCrossed)
    {
        isCrossed = false;
        for (auto i = rects.begin(); i != rects.end();)
        {
            if ((i->height + i->width) < MINRECTPERIMETR)
            {
                i = rects.erase(i);
            }
            else
            {
                for (auto j = i; j != rects.end(); j++)
                {
                    if (i == j) continue;
                    if ((*i & *j).width != 0)
                    {
                        *i = *i | *j;
                        *j = Rect(1, 1, 1, 1);
                        isCrossed = true;
                    }
                }
                i++;
            }
        }
    }
    return rects;
}

/***
 * \@brief  合并轮廓点.    
 *       1. 对于小于 30 个点的轮廓, 直接删除; 
 *       2. 对于两个轮廓的外接矩形相交的情形, 将新的轮廓直接添加到旧的轮廓末端; 
*/
vector<vector<Point>> Capture::uniteContours(vector<vector<Point>> contours)
{
    // vector<vector<Point>> contours = contours;
    bool isCrossed = true;
    vector<Point> stub = { Point(1, 1) };  // 指示这个轮廓已经被合并, 变为无效轮廓.       

    // 直到所有的轮廓都没有相交
    while (isCrossed)
    {
        isCrossed = false;
        // 第三个表达式为空. vector 的一种特殊处理.   
        for (auto i = contours.begin(); i != contours.end();)
        {
            if (i->size() < CONTOUR_MIN_LENGTH)
            {
                i = contours.erase(i);
            }
            else
            {
                for (auto j = i; j != contours.end(); j++)
                {
                    if (i == j) continue;
                    // 对于两个轮廓的外接矩形相交的情况, 将新的轮廓直接添加到旧的轮廓末端.   
                    if ((boundingRect(*i) & boundingRect(*j)).width != 0)
                    {
                        i->insert(i->end(), j->begin(), j->end());
                        *j = stub;
                        isCrossed = true;  // 轮廓合并之后可能会引起新的轮廓相交  
                    }
                }
                i++;
            }
        }
    }
    return contours;
}

/***
 * \@brief 稀疏化取一系列特征点.   
 * 
 *   本次使用为间隔十个点提取一个特征点. 这是为了保证帧率在可接受的范围内   
*/
vector <Point2f> Capture::getFeaturePoints(vector<Point> contours)
{
    std::cout << contours.size() << std::endl;
    vector<Point2f> features;
    const int qty = 10;
    long step = (long)contours.size() / qty;

    for (auto i = contours.begin(); i < contours.end(); i+=step) {
        features.push_back( Point2f(i->x, i->y) );
    }

    return features;
}


void Capture::cut(map<milliseconds, Frame>& framesFlow, mutex& mutex_frames, vector<map<milliseconds, vector<Point>>>& allTracks, mutex& mutex_tracks)
{
    bool endFlag = true;
    while (endFlag)
    {
        milliseconds thisTime = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
        mutex_frames.lock();
        mutex_tracks.lock();
        for (auto frameIt = framesFlow.begin(); frameIt != framesFlow.end();)
        {
            milliseconds frameTime = frameIt->first;
            if ((thisTime.count() - frameTime.count()) > timeRange)
            {
                frameIt = framesFlow.erase(frameIt);
                for (auto trackIt = allTracks.begin(); trackIt != allTracks.end(); trackIt++)
                {
                    auto mapIt = trackIt->find(frameTime);
                    if (mapIt != trackIt->end())
                    {
                        mapIt = trackIt->erase(mapIt);
                    }
                }
            }
            else frameIt++;
        }
        mutex_frames.unlock();
        mutex_tracks.unlock();
    }
}

/***
 * \@brief 核心的检测实现;
 *    1. 使用基于自适应混合高斯背景建模的背景减除法(BackgroundSubtractorMOG2);  
 * 
 * \@param framesFlow
 * \@param mutex_frames
 * \@param allTracks:  所有用于追踪的点, 如果这些点很长时间没有更新, 说明是一些固定的点
 *	 
 * \@param mutex_tracks
 * 
*/
void Capture::find(map<milliseconds, Frame>& framesFlow, mutex& mutex_frames, vector<map<milliseconds, vector<Point>>>& allTracks, mutex& mutex_tracks)
{
    BackgroundSubtractorMOG2 backgroundSubtractor(10, 25, false);
    vector<Vec4i> hierarchy;   // findContours() 函数参数  
    TermCriteria termcrit(CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 20, 0.03);  // 结束准则

    Size subPixWinSize(10, 10), winSize(31, 31);
    bool firstTime = true;
    Mat savemask, gray, prevGray;  // prevGray 用于稀疏光流法检测.   
    bool endFlag = true;

    while (endFlag)
    {
        Mat frame, mask, fgimg;
        currentTime = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
        capture >> frame;
        backgroundSubtractor(frame, mask, -1);
        mask.copyTo(savemask);
        Frame frametoMap(frame, savemask);
        mutex_frames.lock();
        mutex_tracks.lock();
        for (auto frameIt = framesFlow.begin(); frameIt != framesFlow.end();)
        {
            milliseconds frameTime = frameIt->first;

            // 时效性: 处理的不能太过频繁? 
            // 移除那些很长时间没有更新过的点, 说明这些点在这段时间内是不变的.  
            if ((currentTime.count() - frameTime.count()) > timeRange)
            {
                frameIt = framesFlow.erase(frameIt);
                for (auto trackIt = allTracks.begin(); trackIt != allTracks.end(); trackIt++)
                {
                    // 查找键值, 如果存在说明很长时间内没有更新这个点了,删除它 
                    auto mapIt = trackIt->find(frameTime);  
                    if (mapIt != trackIt->end())
                    {
                        mapIt = trackIt->erase(mapIt);
                    }
                }
            }
            else frameIt++;
        }
        framesFlow.emplace(currentTime, frametoMap);
        mutex_frames.unlock();
        mutex_tracks.unlock();

        // 查找轮廓点.  
        // fgimg 的作用是什么? 
        // fgimg = Scalar::all(0);
        // frame.copyTo(fgimg, mask);
        vector<vector<Point>> allContours;
        findContours(mask, allContours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_NONE);
        if (allContours.size() > 0 && allContours.size() < 1000)
        {
            // frame.copyTo(frame);  // 防止修改之前的图片内存空间???
            cvtColor(frame, gray, COLOR_BGR2GRAY);
            vector<vector<Point>> contours = uniteContours(allContours);  //合并轮廓点
            if (prevGray.empty()) {
                gray.copyTo(prevGray);
                lastTime = currentTime;
            }

            mutex_tracks.lock();
            if (allTracks.empty()) {
                for (auto contIt = contours.begin(); contIt != contours.end(); contIt++)
                {
                    map<milliseconds, vector<Point>> oneTrack;
                    oneTrack.emplace(currentTime, *contIt);
                    allTracks.push_back(oneTrack);
                }
            }
            else
            {
                vector<uchar> status;
                vector<float> err;
                multimap<int, int> pointsNum;
                vector<Point2f> pointsPrev, pointsNow;
                
                for (auto allTrackIt = allTracks.begin(); allTrackIt != allTracks.end(); allTrackIt++)
                {
                    long trackNumber = allTrackIt - allTracks.begin();
                    if (allTrackIt->size() > 0)
                    {
                        vector<Point2f> tmpVec = getFeaturePoints(allTrackIt->rbegin()->second);
                        for (auto i = pointsPrev.size(); i < pointsPrev.size() + tmpVec.size(); i++)
                        {
                            pointsNum.emplace(trackNumber, i);
                        }
                        pointsPrev.insert(pointsPrev.end(), tmpVec.begin(), tmpVec.end());
                    }
                }

                // LK 金字塔光流确定最后的运动目标     
                calcOpticalFlowPyrLK(prevGray, gray, pointsPrev, pointsNow, status, err, winSize, 3, termcrit, 0, 0.001);
                for (auto allTrackIt = allTracks.begin(); allTrackIt != allTracks.end(); allTrackIt++)
                {
                    long trackNumber = allTrackIt - allTracks.begin();
                    if (allTrackIt->size() > 0)
                    {
                        auto pointsNumIt = pointsNum.equal_range(trackNumber);
                        vector<Point2f> tmpVecPoints;
                        for (auto it = pointsNumIt.first; it != pointsNumIt.second; it++)
                        {
                            tmpVecPoints.push_back(pointsNow[it->second]);
                        }
                        Rect tmpRect = boundingRect(tmpVecPoints);
                        for (auto contIt = contours.begin(); contIt != contours.end();)
                        {
                            Rect rect = boundingRect(*contIt);
                            if ((tmpRect&rect).width > 0)
                            {
                                allTrackIt->emplace(currentTime, *contIt);
                                contIt = contours.erase(contIt);
                                break;
                            }
                            else contIt++;
                        }
                    }
                }
                for (auto contIt = contours.begin(); contIt != contours.end(); contIt++)
                {
                    map<milliseconds, vector<Point>> oneTrack;
                    oneTrack.emplace(currentTime, *contIt);
                    allTracks.push_back(oneTrack);
                }
            }
            mutex_tracks.unlock();
        }
        
        int delay = 1;
        waitKey(delay);
        milliseconds endtime = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch());
        fps = (int) 1000 / (endtime - currentTime).count();
        
        swap(prevGray, gray);  // 更新用于下一次迭代.   
        lastTime = currentTime;
    }
}


void Capture::display(map<milliseconds, Frame>& framesFlow, mutex& mutex_frames, vector<map<milliseconds, vector<Point>>>& allTracks, mutex& mutex_tracks)
{
    bool endFlag = true;
    while (endFlag)
    {
        mutex_frames.lock();
        if (framesFlow.size() == 0)
        {
            mutex_frames.unlock();
            continue;
        }
        auto frameIt = framesFlow.rbegin();
        milliseconds time = frameIt->first;
        Mat outFrame;
        outFrame = frameIt->second.getImg();
        mutex_tracks.lock();
        for (auto trackIt = allTracks.begin(); trackIt != allTracks.end(); trackIt++)
        {
            if (trackIt->size() > 1)
            {
                auto mapIt = trackIt->find(time);
                if (mapIt==trackIt->end()) continue;
                long number = trackIt - allTracks.begin();
                Rect r = boundingRect(mapIt->second);
                rectangle(outFrame, r, Scalar(255, 0, 0), 2, 8, 0);
                stringstream ss;
                ss << number;
                string stringNumber = ss.str();
                putText(outFrame, stringNumber, Point(r.x + 5, r.y + 5),
                    FONT_HERSHEY_COMPLEX_SMALL, 1, Scalar(0,0,255), 1, 8);
            }
        }
        mutex_frames.unlock();
        mutex_tracks.unlock();

        stringstream sst;
        sst << fps;
        string fpsString = "FPS = " + sst.str();
        putText(outFrame, fpsString, Point(20, outFrame.rows - 20),
                FONT_HERSHEY_COMPLEX_SMALL, 0.8, Scalar(255,0,255), 1, 8);
        displayTime(outFrame);
        imshow("Track Results", outFrame);
        
        waitKey(10);
    }
}

void Capture::displayTime(Mat img)
{
    seconds sec = duration_cast<seconds>(currentTime);
    time_t timefordisp = sec.count();
    putText(img, ctime(&timefordisp), Point(20, img.rows - 40), FONT_HERSHEY_COMPLEX, 0.5, Scalar::all(0), 1, 8);
}