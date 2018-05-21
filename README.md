# Motion analysis and object tracking.     

实现了运动分析和目标跟踪. 程序会在一个视频序列中识别运动并基于金字塔 Lukas-Kanade 计算稀疏光流.    

程序的实现是基于 [OpenCV](http://opencv.org/) 库的.    

运行效率测试:   

如果追踪的目标的轮廓在占整张图的比例很小, 背景变化不明显, 且多个目标之间没有交叉遮挡. 效果会很好(因为查找轮廓函数在这种情况下才会起作用).     

如果追踪的目标很大, 且背景变换比较大, 这时效果就很差了. 因为混合高斯背景建模并不适用于背景变化过渡剧烈的场景.     

## Example
[http://youtu.be/tluSr064jPE](http://youtu.be/tluSr064jPE)

[![](http://img.youtube.com/vi/tluSr064jPE/0.jpg)](http://youtu.be/tluSr064jPE)

[http://youtu.be/zS17b80RPvg](http://youtu.be/zS17b80RPvg)

[![](http://img.youtube.com/vi/zS17b80RPvg/0.jpg)](http://youtu.be/zS17b80RPvg)

## License

* [Apache Version 2.0](http://www.apache.org/licenses/LICENSE-2.0.html)
