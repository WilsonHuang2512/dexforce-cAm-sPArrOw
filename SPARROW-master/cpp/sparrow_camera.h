#pragma once 
#ifndef __SPARROW_CAMERA_H__
#define __SPARROW_CAMERA_H__

#include "scamera.h"
#include <mutex>
#include <thread>
#include "../sdk/socket_tcp.h" 
#include "../sdk/enums.h"
#include "camera_param.h" 
#include "system_config_settings.h"
#include"../sdk/enums.h"

#include <windows.h>  
#include <tlhelp32.h>  
#include <iostream>  
#include <wchar.h> 

extern "C" {
#ifdef NEUTRAL_MODE
	namespace CAMERA {
#else
	namespace SPARROW {
#endif
		class SparrowCamera : public SCamera
		{
		public:
			SparrowCamera();
			~SparrowCamera();
			SparrowCamera(const SparrowCamera&) = delete;
			SparrowCamera& operator=(const SparrowCamera&) = delete;

#ifdef USE_OPENCV
			//功能： 过曝显示
			//输入参数： 像素类型，亮度图，过曝图
			//输出参数： 无
			//返回值： 类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int over_exposure( cv::Mat bright_map, cv::Mat& over_bright_map);

			//功能： 精度检测
			//输入参数： 标定板尺寸，内参结构体，亮度图，深度图，精度
			//输出参数： 无
			//返回值： 类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int accuracy_single(int board_size, struct CalibrationParam* calibration_param, cv::Mat bright, cv::Mat depth, double& accuracy, cv::Mat& draw_map);

			//功能： 获得像素类型
			//输入参数： type
			//输出参数： 无
			//返回值： 类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int dfGetCameraPixelType(int& type);

			//功能：mono图转过曝图
			//输入参数： mono图，过曝图
			//输出参数： 无
			//返回值： 类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int renderBrightnessImage(cv::Mat bright_map_, cv::Mat& over_bright_map_);

			//功能： color图转过曝图
			//输入参数： color图，过曝图
			//输出参数： 无
			//返回值： 类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int renderColorBrightnessImage(cv::Mat bright_map, cv::Mat& over_bright_map);

			//功能： 服务端是否启动
			//输入参数： 无
			//输出参数： 无
			//返回值： 类型（int）:返回true表示连接成功;返回false表示连接失败.
			bool search_camera();

			//函数名： get_cameraSN
			//功能： 获取设备唯一标识
			//输入参数：无
			//输出参数：SN码
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			bool get_cameraSN(char SN[64])override;


			//函数名：get_calib_param
			//功能：获取相机光机内外参数
			//输入参数：文件名
			//输出参数：无
			//返回值：  类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int get_calib_param(const char* calib_param_path)override;

			//函数名：get_onecalibrationdata
			//功能：获取一组光机条纹图
			//输入参数：文件名
			//输出参数：无
			//返回值：  类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int get_onecalibrationdata(const char* calib_param_path)override;

			//函数名：correct_stereo_Mono
			//功能：灰度相机外参校正
			//输入参数：标定板型号、DLP类型、数据文件名，原参数、校正参数
			//输出参数：无
			//返回值：  类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int correct_stereo_Mono(int board_size, std::string patterns_path, std::string in_path, std::string out_path, float* c_in, float* c_dis, float* p_in, float* p_dis, float* r, float* t, double& error)override;

			// 函数名：correct_stereo_Color
			//功能：	彩色相机外参校正
			//输入参数：标定板型号、DLP类型、数据文件名，原参数、校正参数
			//输出参数：无
			//返回值：  类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int correct_stereo_Color(int board_size, std::string patterns_path, std::string in_path, std::string out_path, float* c_in, float* c_dis, float* p_in, float* p_dis, float* r, float* t, double& error) override;

			//函数名：set_calib_looktable
			//功能：	写入标定数据
			//输入参数：校正参数
			//输出参数：无
			//返回值：  类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int set_calib_looktable(float* c_in, float* c_dis, float* p_in, float* p_dis, float* r, float* t)override;
#endif



			//功能： 连接相机
			//输入参数： camera_id（相机ip地址）
			//输出参数： 无
			//返回值： 类型（int）:返回0表示连接成功;返回-1表示连接失败.
			int connect(const char* camera_id)override;

			//功能： 断开相机连接
			//输入参数： camera_id（相机ip地址）
			//输出参数： 无
			//返回值： 类型（int）:返回0表示断开成功;返回-1表示断开失败.
			int disconnect(const char* camera_id)override;

			//功能： 获取相机分辨率
			//输入参数： 无
			//输出参数： width(图像宽)、height(图像高)
			//返回值： 类型（int）:返回0表示获取参数成功;返回-1表示获取参数失败.
			int getCameraResolution(int* width, int* height)override;

			//函数名： DfGetCameraChannels
			//功能： 获取相机图像通道数
			//输入参数： 无
			//输出参数： channels(通道数)
			//返回值： 类型（int）:返回0表示获取参数成功;返回-1表示获取参数失败.
			int getCameraChannels(int* channels)override;

			//函数名： DfSetCaptureEngine
			//功能： 设置采集引擎
			//输入参数：engine
			//输出参数：  
			//返回值： 类型（int）:返回0表示设置参数成功;返回-1表示设置参数失败。
			int setCaptureEngine(SparrowEngine engine)override;

			//函数名： DfGetCaptureEngine
			//功能： 设置采集引擎
			//输入参数：
			//输出参数：engine
			//返回值： 类型（int）:返回0表示设置参数成功;返回-1表示设置参数失败。
			int getCaptureEngine(SparrowEngine& engine)override;

			//功能： 采集一帧数据并阻塞至返回状态
			//输入参数： exposure_num（曝光次数）：设置值为1为单曝光，大于1为多曝光模式（具体参数在相机gui中设置）.
			//输出参数： timestamp(时间戳)
			//返回值： 类型（int）:返回0表示获取采集数据成功;返回-1表示采集数据失败.
			int captureData(int exposure_num, char* timestamp)override;

			//功能： 获取去畸变深度图
			//输入参数：无
			//输出参数： undistort_depth(深度图)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getUndistortDepthData(float* undistort_depth)override;

			//功能： 获取深度图
			//输入参数：无
			//输出参数： depth(深度图)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败. 
			int getDepthData(float* depth)override;
			
			//功能： 获取点云
			//输入参数：无
			//输出参数： point_cloud(点云)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getPointcloudData(float* point_cloud)override;

			//功能： 获取去畸变亮度图
			//输入参数：无
			//输出参数： undistort_brightness(亮度图)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getUndistortBrightnessData(unsigned char* undistort_brightness)override;

			//功能： 获取亮度图
			//输入参数：无
			//输出参数： brightness(亮度图)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getBrightnessData(unsigned char* brightness)override;

			//函数名： getColorBrightnessData
			//功能： 获取亮度图
			//输入参数：无
			//输出参数： brightness(亮度图),color(亮度图颜色类型)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getColorBrightnessData(unsigned char* brightness, SparrowColor color)override;

			//函数名： getUndistortColorBrightnessData
			//功能： 获取去畸变后的彩色亮度图
			//输入参数：无
			//输出参数： brightness(亮度图)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getUndistortColorBrightnessData(unsigned char* brightness, SparrowColor color)override;

			//功能： 获取校正到基准平面的高度映射图
			//输入参数：无  
			//输出参数： height_map(高度映射图)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getHeightMapData(float* height_map)override;

			//功能： 获取基准平面参数
			//输入参数：无
			//输出参数： R(旋转矩阵：3*3)、T(平移矩阵：3*1)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getStandardPlaneParam(float* R, float* T)override;

			//功能： 获取校正到基准平面的高度映射图
			//输入参数：R(旋转矩阵)、T(平移矩阵)
			//输出参数： height_map(高度映射图)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int getHeightMapDataBaseParam(float* R, float* T, float* height_map)override;


			 
			//功能： 获取相机标定参数
			//输入参数： 无
			//输出参数： calibration_param（相机标定参数结构体）
			//返回值： 类型（int）:返回0表示获取标定参数成功;返回-1表示获取标定参数失败.
			int getCalibrationParam(struct CalibrationParam* calibration_param)override;
 
			/***************************************************************************************************************************************************************/
			//参数设置

			//功能： 设置LED电流
			//输入参数： led（电流值）
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamLedCurrent(int led)override;

			//功能： 设置LED电流
			//输入参数： 无
			//输出参数： led（电流值）
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamLedCurrent(int& led)override;
			 
			//功能： 设置基准平面的外参
			//输入参数：R(旋转矩阵：3*3)、T(平移矩阵：3*1)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamStandardPlaneExternal(float* R, float* T)override;

			//功能： 获取基准平面的外参
			//输入参数：无
			//输出参数： R(旋转矩阵：3*3)、T(平移矩阵：3*1)
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamStandardPlaneExternal(float* R, float* T)override;

			//功能： 设置生成亮度图参数
			//输入参数：model(1:与条纹图同步连续曝光、2：单独发光曝光、3：不发光单独曝光)、exposure(亮度图曝光时间)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamGenerateBrightness(int model, float exposure)override;

			//功能： 获取生成亮度图参数
			//输入参数： 无
			//输出参数：model(1:与条纹图同步连续曝光、2：单独发光曝光、3：不发光单独曝光)、exposure(亮度图曝光时间)
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamGenerateBrightness(int& model, float& exposure)override;

			//功能： 设置相机曝光时间
			//输入参数：exposure(相机曝光时间)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamCameraExposure(float exposure)override;

			//功能： 获取相机曝光时间
			//输入参数： 无
			//输出参数：exposure(相机曝光时间)
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamCameraExposure(float& exposure)override;

			//功能： 设置混合多曝光参数（最大曝光次数为6次）
			//输入参数： num（曝光次数）、exposure_param[6]（6个曝光参数、前num个有效）、led_param[6]（6个led亮度参数、前num个有效）
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamMixedHdr(int num, int exposure_param[6], int led_param[6])override;

			//功能： 获取混合多曝光参数（最大曝光次数为6次）
			//输入参数： 无
			//输出参数： num（曝光次数）、exposure_param[6]（6个曝光参数、前num个有效）、led_param[6]（6个led亮度参数、前num个有效）
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamMixedHdr(int& num, int exposure_param[6], int led_param[6])override;

			//功能： 设置相机曝光时间
			//输入参数：confidence(相机置信度)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamCameraConfidence(float confidence)override;

			//功能： 获取相机曝光时间
			//输入参数： 无
			//输出参数：confidence(相机置信度)
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamCameraConfidence(float& confidence)override;

			//功能： 设置相机增益
			//输入参数：gain(相机增益)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamCameraGain(float gain)override;

			//功能： 获取相机增益
			//输入参数： 无
			//输出参数：gain(相机增益)
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamCameraGain(float& gain)override;

			//功能： 设置点云平滑参数
			//输入参数：smoothing(0:关、1-5:平滑程度由低到高)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamSmoothing(int smoothing)override;

			//功能： 设置点云平滑参数
			//输入参数：无
			//输出参数：smoothing(0:关、1-5:平滑程度由低到高)
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamSmoothing(int& smoothing)override;

			//功能： 设置点云半径滤波参数
			//输入参数：use(开关：1开、0关)、radius(半径）、num（有效点）
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamRadiusFilter(int use, float radius, int num)override;

			//功能： 获取点云半径滤波参数
			//输入参数：无
			//输出参数：use(开关：1开、0关)、radius(半径）、num（有效点）
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamRadiusFilter(int& use, float& radius, int& num)override;

			//函数名： setParamDepthFilter
			//功能： 设置深度图滤波参数
			//输入参数：use(开关：1开、0关)、depth_filterthreshold(深度图在1000mm距离过滤的噪声阈值)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamDepthFilter(int use, float depth_filter_threshold)override;

			//函数名： getParamDepthFilter
			//功能： 设置深度图滤波参数
			//输入参数：use(开关：1开、0关)、depth_filterthreshold(阈值0-100)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamDepthFilter(int& use, float& depth_filter_threshold)override;

			//功能： 设置外点过滤阈值
			//输入参数：threshold(阈值0-100)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamOutlierFilter(float threshold)override;

			//功能： 获取外点过滤阈值
			//输入参数： 无
			//输出参数：threshold(阈值0-100)
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamOutlierFilter(float& threshold)override;

			//功能： 设置多曝光模式
			//输入参数： model(1：HDR(默认值)、2：重复曝光)
			//输出参数：无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamMultipleExposureModel(int model)override;

			//功能： 设置重复曝光数
			//输入参数： num(2-10)
			//输出参数：无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamRepetitionExposureNum(int num)override;


			//函数名： setParamGrayRectify
			//功能： 设置点云灰度补偿参数
			//输入参数：use(开关：1开、0关)、radius(半径：3、5、7、9）、sigma（补偿强度，范围0-100）
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamGrayRectify(int use, int radius, float sigma)override;

			//函数名： getParamGrayRectify
			//功能： 获取点云灰度补偿参数
			//输入参数：无
			//输出参数：use(开关：1开、0关)、radius(半径：3、5、7、9）、sigma（补偿强度，范围0-100）
			//返回值： 类型（int）:返回0表示获取参数成功;否则失败。
			int getParamGrayRectify(int& use, int& radius, float& sigma)override;

			//函数名： setParamBrightnessHdrExposure
			//功能： 设置亮度图多曝光参数（最大曝光次数为10次）
			//输入参数： num（曝光次数）、exposure_param[6]（6个曝光参数、前num个有效））
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamBrightnessHdrExposure(int num, int exposure_param[10])override;

			//函数名：getParamBrightnessHdrExposure
			//功能： 设置亮度图多曝光参数（最大曝光次数为10次）
			//输入参数：无 
			//输出参数：num（曝光次数）、exposure_param[10]（10个曝光参数、前num个有效））
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getParamBrightnessHdrExposure(int& num, int exposure_param[10])override;

			//函数名： setParamBrightnessExposureModel
			//功能： 设置亮度图曝光模式
			//输入参数： model（1：单曝光、2：曝光融合）
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamBrightnessExposureModel(int model)override;

			//函数名： getParamBrightnessExposureModel
			//功能： 获取亮度图曝光模式
			//输入参数： 无
			//输出参数： model（1：单曝光、2：曝光融合）
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getParamBrightnessExposureModel(int& model)override;

			//函数名： setParamBrightnessGain
			//功能： 设置亮度图增益
			//输入参数：gain(亮度图增益)
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamBrightnessGain(float gain)override;

			//函数名： getParamBrightnessGain
			//功能： 获取亮度图增益
			//输入参数：无
			//输出参数：gain(亮度图增益)
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getParamBrightnessGain(float& gain)override;

			//函数名： getSdkVersion
			//功能： 获取sdk版本
			//输入参数：无
			//输出参数：version(版本)
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getSdkVersion(char version[64])override;

			//函数名： getFirmwareVersion
			//功能： 获取固件版本
			//输入参数：无
			//输出参数：version(版本)
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getFirmwareVersion(char version[64])override;

			//函数名： DfCaptureBrightnessData
			//功能： 获取亮度图
			//输入参数： color(图像颜色类型)
			//输出参数： brightness(亮度图)
			//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
			int captureBrightnessData(unsigned char* brightness, SparrowColor color)override;

			//函数名： setParamReflectFilter
			//功能： 设置亮度图增益
			//输入参数：use(开关：1开、0关)、param_b（过滤系数：范围0-100）
			//输出参数： 无
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamReflectFilter(int use, float param_b) override;

			//函数名： getParamReflectFilter
			//功能： 获取亮度图增益
			//输入参数：无
			//输出参数：use(开关：1开、0关)、param_b（过滤系数：范围0-100）
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getParamReflectFilter(int& use, float& param_b) override;


			//函数名： DfGetParamRepetitionExposureNum
			//功能： 获取重复曝光数
			//输入参数： 无
			//输出参数：num(2-10)
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getParamRepetitionExposureNum(int& num)override;

			//函数名： DfGetParamMultipleExposureModel
			//功能： 获取多曝光模式
			//输入参数： 无
			//输出参数：model(1：HDR(默认值)、2：重复曝光)
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getParamMultipleExposureModel(int& model)override;

			//函数名： DfGetParamJson
			//功能： 获取Json配置文件
			//输入参数：无
			//输出参数： config_json（配置文件字符）、config_size（配置文本输出大小）、
			//status_json（输出设置状态）、status_size（状态文本大小）
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int getParamJson(char* config_json, char* status_json)override;

			//函数名： DfSaveJson
			//功能： 保存Json配置文件
			//输入参数：config_json（配置文件字符）
			//输出参数： 无 
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int saveJson(const char* config_json, const char* path)override;

			//函数名： DfReadJson
			//功能： 读取Json配置文件
			//输入参数：config_json（配置文件字符）、config_size（配置文本大小）、
			//输出参数： 无 
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int readJson(char* config_json, const char* path)override;

			//函数名： DfSetParamJson
			//功能： 设置Json配置文件里的参数
			//输入参数：config_json（输入配置文件字符）、config_size（输入文本出大小）
			//输出参数：out_status_json（输出设置状态）、out_size（输出文本大小）、maxnum(填入采集captureData接口）
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int setParamJson(char* config_json, char* status_json, int& maxnum)override;

			//函数名： savePointcloudToPcd
			//功能： 保存pcd点云
			//输入参数：pointcloud(点云)、brightness（亮度图）、channels（亮度图通道数）、path(路径)
			//输出参数：gain(亮度图增益)
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int savePointcloudToPcd(float* pointcloud, unsigned char* brightness, int channels, const char* path)override;

			//函数名： savePointcloudToPly
			//功能： 保存ply点云
			//输入参数：pointcloud(点云)、brightness（亮度图）、channels（亮度图通道数）、path(路径)
			//输出参数：gain(亮度图增益)
			//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
			int savePointcloudToPly(float* pointcloud, unsigned char* brightness, int channels, const char* path)override;

		public:

			int DfGetCameraRawDataTest(unsigned char* raw, int raw_buf_size);

			void save_images(const char* raw_image_dir, unsigned char* buffer, int width, int height, int image_num);

			int rgbToGray(unsigned char* src, unsigned char* dst);

			int bayerToRgb(unsigned char* src, unsigned char* dst);

			int undistortColorBrightnessMap(unsigned char* brightness_map);

			int undistortBrightnessMap(unsigned char* brightness_map);

			int undistortDepthMap(float* depth_map);

			bool transformPointcloud(float* org_point_cloud_map, float* transform_point_cloud_map, float* rotate, float* translation);

			int depthTransformPointcloud(float* depth_map, float* point_cloud_map);

			int getBrightness(unsigned char* brightness, int brightness_buf_size);

			int getFrame04(float* depth, int depth_buf_size,unsigned char* brightness, int brightness_buf_size);

			int getRepetitionFrame04(int count, float* depth, int depth_buf_size, unsigned char* brightness, int brightness_buf_size);

			int getFrame04Hdr(float* depth, int depth_buf_size, unsigned char* brightness, int brightness_buf_size);
 
			int getFrame06(float* depth, int depth_buf_size,unsigned char* brightness, int brightness_buf_size);

			int getFrame06Hdr(float* depth, int depth_buf_size,unsigned char* brightness, int brightness_buf_size);

			int getRepetitionFrame06(int count, float* depth, int depth_buf_size,unsigned char* brightness, int brightness_buf_size);
 
			int getFrame06Mono12(float* depth, int depth_buf_size,
				unsigned char* brightness, int brightness_buf_size);

			int getFrame06HdrMono12(float* depth, int depth_buf_size,
				unsigned char* brightness, int brightness_buf_size);

			int getRepetitionFrame06Mono12(int count, float* depth, int depth_buf_size,
				unsigned char* brightness, int brightness_buf_size); 

			int getSystemConfigParam(struct SystemConfigParam& config_param);
 
			int setParamBilateralFilter(int use, int param_d);

			int getParamBilateralFilter(int& use, int& param_d);

			int getCalibrationParam(struct CameraCalibParam& calibration_param);
 
			int getFrameStatus(int& status);
 
			int getCameraPixelType(int& type);

			std::string get_timestamp();

			std::time_t getTimeStamp(long long& msec);

			std::tm* gettm(long long timestamp);
		public:

			int registerOnDropped(std::function<int(void*)> p_function);

			int connectNet(const char* ip);

			int disconnectNet();
			 
			int HeartBeat();

			int HeartBeat_loop();

		private:
			std::function<int(void*)> p_OnDropped_ = 0;

			SparrowEngine engine_ = SparrowEngine::Normal;

			SparrowPixelType pixel_type_ = SparrowPixelType::Mono;

			std::string camera_ip_;
			int multiple_exposure_model_ = 1;
			int repetition_exposure_model_ = 2;


			std::timed_mutex command_mutex_;
			std::timed_mutex undistort_mutex_;

			std::thread heartbeat_thread;
			int heartbeat_error_count_ = 0;

			SOCKET g_sock_heartbeat;
			SOCKET g_sock;
			bool connected = false;
			long long token = 0;

			int camera_width_ = 1920;
			int camera_height_ = 1200;

			struct CameraCalibParam calibration_param_;
			bool connected_flag_ = false;

			int depth_buf_size_ = 0;
			int pointcloud_buf_size_ = 0;
			int brightness_bug_size_ = 0;
			float* point_cloud_buf_ = NULL;
			float* trans_point_cloud_buf_ = NULL;
			bool transform_pointcloud_flag_ = false;
			float* depth_buf_ = NULL;
			unsigned char* brightness_buf_ = NULL;
			float* undistort_map_x_ = NULL;
			float* undistort_map_y_ = NULL;
			float* distorted_map_x_ = NULL;
			float* distorted_map_y_ = NULL;

			unsigned char* rgb_buf_ = NULL;
			bool bayer_to_rgb_flag_ = false;
		};
#ifdef NEUTRAL_MODE
#define SPARROW_API CAMERA_API
#endif
		SPARROW_API int connectSparrow(SCamera* camera, const char* camera_id) {
			if (camera) {
				return camera->connect(camera_id);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getCameraResolutionSparrow(SCamera* camera, int* width, int* height) {
			if (camera) {
				return camera->getCameraResolution(width, height);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getCameraChannelsSparrow(SCamera* camera, int* channels) {
			if (camera) {
				return camera->getCameraChannels(channels);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setCaptureEngineSparrow(SCamera* camera, SparrowEngine engine) {
			if (camera) {
				return camera->setCaptureEngine(engine);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int capturedataSparrow(SCamera* camera, int exposure_num, char* timestamp) {
			if (camera) {
				return camera->captureData(exposure_num, timestamp);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getDepthDataFloatSparrow(SCamera* camera, float* depth) {
			if (camera) {
				return camera->getDepthData(depth);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getUndistortDepthDataSparrow(SCamera* camera, float* depth) {
			if (camera) {
				return camera->getUndistortDepthData(depth);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getPointcloudDataSparrow(SCamera* camera, float* point_cloud) {
			if (camera) {
				return camera->getPointcloudData(point_cloud);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}
        
		SPARROW_API int getBrightnessSparrow(SCamera* camera, unsigned char* brightness) {
			if (camera) {
				return camera->getBrightnessData(brightness);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getUndistortBrightnessDataSparrow(SCamera* camera, unsigned char* brightness) {
			if (camera) {
				return camera->getUndistortBrightnessData(brightness);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getColorBrightnessDataSparrow(SCamera* camera, unsigned char* brightness,SparrowColor color) {
			if (camera) {
				return camera->getColorBrightnessData(brightness, color);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getUndistortColorBrightnessDataSparrow(SCamera* camera, unsigned char* brightness, SparrowColor color) {
			if (camera) {
				return camera->getUndistortColorBrightnessData(brightness, color);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getHeightMapDataBaseParamSparrow(SCamera* camera, float* R, float* T, float* height_map) {
			if (camera) {
				return camera->getHeightMapDataBaseParam(R, T, height_map);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int disconnectSparrow(SCamera* camera, const char* camera_id) {
			if (camera) {
				return camera->disconnect(camera_id);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int getCalibrationParamSparrow(SCamera* camera, struct CalibrationParam* calibration_param) {
			if (camera) {
				return camera->getCalibrationParam(calibration_param);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamLedCurrentSparrow(SCamera* camera, int led) {
			if (camera) {
				return camera->setParamLedCurrent(led);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamGenerateBrightnessSparrow(SCamera* camera, int model, float exposure) {
			if (camera) {
				return camera->setParamGenerateBrightness(model, exposure);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamCameraExposureSparrow(SCamera* camera, float exposure) {
			if (camera) {
				return camera->setParamCameraExposure(exposure);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamMixedHdrSparrow(SCamera* camera, int num, int exposure_param[5], int led_param[5]) {
			if (camera) {
				return camera->setParamMixedHdr(num, exposure_param, led_param);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamCameraConfidenceSparrow(SCamera* camera, float confidence) {
			if (camera) {
				return camera->setParamCameraConfidence(confidence);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamCameraGainSparrow(SCamera* camera, float gain) {
			if (camera) {
				return camera->setParamCameraGain(gain);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamSmoothingSparrow(SCamera* camera, int smoothing) {
			if (camera) {
				return camera->setParamSmoothing(smoothing);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamRadiusFilterSparrow(SCamera* camera, int use, float radius, int num) {
			if (camera) {
				return camera->setParamRadiusFilter(use, radius, num);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamDepthFilterSparrow(SCamera* camera, int use, float depth_filter_threshold) {
			if (camera) {
				return camera->setParamDepthFilter(use,depth_filter_threshold);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamOutlierFilterSparrow(SCamera* camera, float threshold) {
			if (camera) {
				return camera->setParamOutlierFilter(threshold);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamMultipleExposureModelSparrow(SCamera* camera, int model) {
			if (camera) {
				return camera->setParamMultipleExposureModel(model);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamRepetitionExposureNumSparrow(SCamera* camera, int num) {
			if (camera) {
				return camera->setParamRepetitionExposureNum(num);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamGrayRectifySparrow(SCamera* camera, int use, int radius, float sigma) {
			if (camera) {
				return camera->setParamGrayRectify(use,radius,sigma);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamBrightnessHdrExposureSparrow(SCamera* camera, int num, int exposure_param[10]) {
			if (camera) {
				return camera->setParamBrightnessHdrExposure(num, exposure_param);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamBrightnessExposureModelSparrow(SCamera* camera, int model) {
			if (camera) {
				return camera->setParamBrightnessExposureModel(model);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamBrightnessGainSparrow(SCamera* camera, float gain) {
			if (camera) {
				return camera->setParamBrightnessGain(gain);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int captureBrightnessDataSparrow(SCamera* camera, unsigned char* brightness, SparrowColor color) {
			if (camera) {
				return camera->captureBrightnessData(brightness, color);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API int setParamReflectFilterSparrow(SCamera* camera, int use, float param_b) {
			if (camera) {
				return camera->setParamReflectFilter(use,param_b);  // 调用虚函数
			}
			return -1;  // 如果 camera 无效，则返回失败
		}

		SPARROW_API SparrowCamera* createSparrowCamera() {
			return new SparrowCamera();
		}

		SPARROW_API void destroySparrowCamera(SparrowCamera* camera) {
			delete camera;
		}
	}
}
#endif
