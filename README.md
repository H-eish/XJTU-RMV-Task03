# XJTU-RMV-Task03
##  - 初始速度 (vx, vy): 252.262, 346.56 (px/s)
##  - 参数 g: 497.561 (px/s^2)
##  - 参数 k: 0.0641256 (1/s)  
##  完成思路：依旧是ai先写，后逐行学习，并修正错误（比如调整二值化参数的低值，使其能读取到轮廓）
## 遇到的困难
### 1.安装完ceres后检查，在#inlcude "ceres/ceres.h"报错，提示Eigen not found
解决：在配置opencv时遇到过类似问题，本质上是路径指向问题，如法炮制，设置软链接解决
### 2.视频无法读入问题，虽然编译成功，但提示"错误: 无法打开视频文件"
硬控大量时间，找ai和找学长的效果不理想。但是还是通过ai找到问题解决的思路。  
问题本质应是opencv不知道为什么没有配置上视频解码的FFmpeg,最后通过朴素的浏览器找到一个帖子得以解决。
![alt text](<picture/Screenshot from 2025-09-27 15-15-21.png>)  
解决后：（原本这里全是NO）  
![alt text](<picture/Screenshot from 2025-09-27 11-12-09.png>)
### 3.代码编译报错问题
代码关于cap的部分全部报错，发现ceres没接入到CMakeList中（气笑了）  
但是加上了以后仍然报错  
![alt text](<picture/Screenshot from 2025-09-28 19-11-04.png>)  
报错点很奇怪，直接问ai解决无果，最终找学长得到解决。实际上就是CMakeList写错了，只需要把ceres::ceres（这也是ai给的）改成${CERES_LIBRARIES}就能解决
### 4.学习各行代码的过程中遇到很多看不懂的，包括对数据处理的办法（）如T占位符）、质心坐标的计算等等。耗了一部分时间理解和克服。
### 5.数据的检验
最开始的数据是这样的，没加鲁棒核  
![alt text](<picture/Screenshot from 2025-09-28 20-26-43.png>)  
加了鲁棒核再试了一次  
![alt text](<picture/Screenshot from 2025-09-28 20-26-57.png>)  
发现数据差距很大。后者vy是负值，貌似更合理（那时没有翻转y轴）
但是从终端输出看，二者的cost值都很大  
![alt text](<picture/Screenshot from 2025-09-28 20-55-52.png>)  
![alt text](<picture/Screenshot from 2025-09-28 20-58-40.png>)  
说明代码本身出了问题。进行排查。  
把参数范围的限制尝试去掉：  
![alt text](<picture/Screenshot from 2025-09-28 21-21-46.png>)  
可以看到cost的值小了很多，但g变成负几百，同时注意到之前的g都是最小值100  
于是明白是方向出了问题，g和vy应是同向的，将y轴翻转解决（double cy = frame.rows - (M.m01 / M.m00);）                     
采用鲁棒核的cost值小很多，故用了鲁棒核
### ps：由于不清楚结果与答案有多少差距，故现将作业提交，若后面有优化方案，或者给出了参考值，可能会再交一次