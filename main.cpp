#include <iostream>
#include <vector>
#include <string>
#include <cmath>

#include "glog/logging.h"
#include "opencv2/opencv.hpp"
#include "ceres/ceres.h"


// 用于存储每个时刻的弹丸位置
struct TrajectoryPoint {
    double t; // 时间 (秒)
    double x; // x 坐标
    double y; // y 坐标
};

// --- Ceres 优化部分 ---

// 1. 定义残差计算结构 (Cost Functor)
//    用于计算模型预测值与观测值之间的差异
struct TrajectoryResidual {
    // 构造函数，传入观测数据点和初始状态
    TrajectoryResidual(double t, double x, double y, double t0, double x0, double y0)
        : t_(t), x_(x), y_(y), t0_(t0), x0_(x0), y0_(y0) {}

    // 重载 () 运算符，Ceres 会调用此函数计算残差
    template <typename T>   //定义模版
    bool operator()(const T* const params, T* residual) const {
        // 待优化的参数:
        // params[0]: 初始 x 方向速度 v_x0
        // params[1]: 初始 y 方向速度 v_y0
        // params[2]: 重力参数 g
        // params[3]: 空气阻力系数 k
        T v_x0 = params[0];     //解包，方便使用
        T v_y0 = params[1];     //T是占位符，计算残差时会变成 double
        T g = params[2];
        T k = params[3];

        T delta_t = T(t_) - T(t0_);

        // 根据任务图片中的弹道模型计算预测位置
        // x(t) = x0 + v_x0/k * (1 - exp(-k*Δt))
        T pred_x = T(x0_) + v_x0 / k * (T(1.0) - exp(-k * delta_t));

        // y(t) = y0 + (v_y0 + g/k)/k * (1 - exp(-k*Δt)) - g/k * Δt
        T pred_y = T(y0_) + (v_y0 + g / k) / k * (T(1.0) - exp(-k * delta_t)) - (g / k) * delta_t;

        // 计算 x 和 y 方向的残差 (观测值 - 预测值)
        residual[0] = T(x_) - pred_x;
        residual[1] = T(y_) - pred_y;

        return true;
    }

private:
    const double t_, x_, y_;     // 观测数据 (t, x, y)
    const double t0_, x0_, y0_; // 初始状态 (t0, x0, y0)
};


int main(int argc, char** argv){
    google::InitGoogleLogging(argv[0]);

    // =================================================================
    // == 步骤 1: 在 main() 函数内部获取实验数据 
    // =================================================================
    

    // 1. 打开视频文件
    std::string video_path = "/home/heish/Documents/Projects/XJTU-RMV-Task03/video.mp4";
    cv::VideoCapture cap;
    cap.open(video_path);

    if (!cap.isOpened()) {
         std::cerr << "错误: 无法打开视频文件: " << video_path << std::endl;
         return -1;
    }

    // 从任务描述中获取FPS
    double fps = 60.0;
    // 你也可以从视频文件中读取FPS，但任务指定了60
    //double fps = cap.get(cv::CAP_PROP_FPS);

    std::vector<TrajectoryPoint> trajectory_data;
    cv::Mat frame;
    int frame_count = 0;//总帧数

    

    // 2. 逐帧处理视频
     while (cap.read(frame)) {
            if (frame.empty()) {
            break;
        }

        // --- 图像处理与目标检测 ---
        cv::Mat gray_frame;
        // 转换为灰度图
        cv::cvtColor(frame, gray_frame, cv::COLOR_BGR2GRAY);

        cv::Mat binary_mask;
        // 二值化处理，选取高亮度区域。
        cv::threshold(gray_frame, binary_mask, 100, 255, cv::THRESH_BINARY);
        
                
        // 查找二值化图像中的轮廓
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(binary_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        // --- 提取坐标 ---
        if (!contours.empty()) {
            // 假设最大的轮廓就是弹丸
            // 你也可以通过面积大小来过滤，确保找到的是弹丸而不是噪点
            double max_area = 0;
            int max_area_idx = -1;
            for (int i = 0; i < contours.size(); i++) {
                double area = cv::contourArea(contours[i]);
                if (area > max_area) {
                    max_area = area;
                    max_area_idx = i; //记下最大轮廓的索引
                }
            }
            
            if (max_area_idx != -1) {
                // 计算轮廓的几何矩
                cv::Moments M = cv::moments(contours[max_area_idx]);

                // 确保M.m00不为0以避免除零错误
                if (M.m00 > 0) {
                    // 计算质心 (x, y)（平均值）
                    double cx = M.m10 / M.m00;  //总长/总数
                    double cy = frame.rows - (M.m01 / M.m00);                     
                    // 计算当前时间
                    double current_time = static_cast<double>(frame_count) / fps;

                    // 存储数据点
                    trajectory_data.push_back({current_time, cx, cy});
                }
            }
        }
        
        frame_count++;//当前帧结束，总帧数加1
    }

    // 3. 释放资源
    cap.release();

    // =================================================================
    // == 步骤 2: Ceres 优化流程
    // =================================================================

    // 初始状态 (t0, x0, y0) 固定为第一个数据点
    const double t0 = trajectory_data[0].t;
    const double x0 = trajectory_data[0].x;
    const double y0 = trajectory_data[0].y;

    // 待优化的参数数组: [v_x0, v_y0, g, k]
    // 设置一个合理的初始猜测值
    double params[4] = {100.0, 100.0, 500.0, 0.5};

    // 创建优化问题对象
    ceres::Problem problem;

    // 向 Problem 添加残差块 (Residual Blocks)
    ceres::LossFunction* loss = new ceres::HuberLoss(1.0);  //采用鲁棒核
    for (const auto& point : trajectory_data) {
        problem.AddResidualBlock(
            new ceres::AutoDiffCostFunction<TrajectoryResidual, 2, 4>(      //二维，四个参数
                new TrajectoryResidual(point.t, point.x, point.y, t0, x0, y0)),
        loss, params);
    }

    // 设置参数的约束范围
    problem.SetParameterLowerBound(params, 2, 100.0);    // g >= 100
    problem.SetParameterUpperBound(params, 2, 1000.0);   // g <= 1000
    problem.SetParameterLowerBound(params, 3, 0.01);     // k >= 0.01
    problem.SetParameterUpperBound(params, 3, 1.0);      // k <= 1.0

    // 配置并运行求解器 (Solver)
    ceres::Solver::Options options;
    options.linear_solver_type = ceres::DENSE_QR;
    options.minimizer_progress_to_stdout = true;

    ceres::Solver::Summary summary;
    ceres::Solve(options, &problem, &summary);

    // 输出结果
    std::cout << "\n" << summary.BriefReport() << "\n";
    std::cout << "------------------------------------------\n";
    std::cout << "拟合完成，最终结果:\n";
    std::cout << "  - 初始速度 (vx, vy): " << params[0] << ", " << params[1] << " (px/s)\n";
    std::cout << "  - 参数 g: " << params[2] << " (px/s^2)\n";
    std::cout << "  - 参数 k: " << params[3] << " (1/s)\n";
    std::cout << "------------------------------------------\n";

    return 0;
}
