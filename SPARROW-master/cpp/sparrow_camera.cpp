#include "sparrow_camera.h"
#include "version.h"
#include "easylogging++.h"
#include "../sdk/camera_status.h"
#include "protocol.h"
#include "../test/triangulation.h"
#include <chrono>
#include <ctime>
#include <time.h>
#include <stddef.h> 
#include <assert.h>
#include <iostream>
#include <thread>   
#include <stddef.h> 
#include<xstring>
#ifdef USE_OPENCV
#include"PrecisionTest.h"
#include"calibrate_function.h"
#include"DetectCCTag.h"
#include "GxIAPI.h"
#include"calibration.h"
#endif
#include <iostream>
#include <filesystem>
#include "json.hpp" 
#include <iomanip>
#include <filesystem>
#ifdef _WIN32

#elif __linux
#include <dirent.h>
#include <unistd.h>

#define INVALID_SOCKET (~0)
#define SOCKET_ERROR -1
#endif

INITIALIZE_EASYLOGGINGPP
using namespace std;
using namespace std::chrono;
#ifdef NEUTRAL_MODE
namespace CAMERA {
#else
namespace SPARROW {
#endif
	SparrowCamera::SparrowCamera()
	{
		g_sock = INVALID_SOCKET;
		g_sock_heartbeat = INVALID_SOCKET;
	}

	SparrowCamera::~SparrowCamera()
	{

	}

	void rolloutHandler(const char* filename, std::size_t size)
	{
#ifdef _WIN32 
		/// 备份日志
		system("mkdir sparrowLog");
		system("DIR .\\sparrowLog\\ .log / B > LIST.TXT");
		ifstream name_in("LIST.txt", ios_base::in);//文件流

		int num = 0;
		std::vector<std::string> name_list;
		char buf[1024] = { 0 };
		while (name_in.getline(buf, sizeof(buf)))
		{
			//std::cout << "name: " << buf << std::endl;
			num++;
			name_list.push_back(std::string(buf));
		}

		if (num < 5)
		{
			num++;
		}
		else
		{
			num = 5;
			name_list.pop_back();
		}


		for (int i = num; i > 0 && !name_list.empty(); i--)
		{
			std::stringstream ss;
			std::string path = ".\\sparrowLog\\" + name_list.back();
			name_list.pop_back();
			ss << "move " << path << " sparrowLog\\log_" << i - 1 << ".log";
			std::cout << ss.str() << std::endl;
			system(ss.str().c_str());
		}

		std::stringstream ss;
		ss << "move " << filename << " sparrowLog\\log_0" << ".log";
		system(ss.str().c_str());
#elif __linux 

		/// 备份日志
		if (access("xemaLog", F_OK) != 0)
		{
			system("mkdir xemaLog");
		}

		std::vector<std::string> name_list;
		std::string suffix = "log";
		DIR* dir;
		struct dirent* ent;
		if ((dir = opendir("xemaLog")) != NULL)
		{
			while ((ent = readdir(dir)) != NULL)
			{
				/* print all the files and directories within directory */
				// printf("%s\n", ent->d_name);

				std::string name = ent->d_name;

				if (name.size() < 3)
				{
					continue;
				}

				std::string curSuffix = name.substr(name.size() - 3);

				if (suffix == curSuffix)
				{
					name_list.push_back(name);
				}
			}
			closedir(dir);
		}

		sort(name_list.begin(), name_list.end());

		int num = name_list.size();
		if (num < 5)
		{
			num++;
		}
		else
		{
			num = 5;
			name_list.pop_back();
		}


		for (int i = num; i > 0 && !name_list.empty(); i--)
		{
			std::stringstream ss;
			std::string path = "./xemaLog/" + name_list.back();
			name_list.pop_back();
			ss << "mv " << path << " xemaLog/log_" << i - 1 << ".log";
			std::cout << ss.str() << std::endl;
			system(ss.str().c_str());
		}

		std::stringstream ss;
		ss << "mv " << filename << " xemaLog/log_0" << ".log";
		system(ss.str().c_str());

#endif 

	}


	//网格掉线
	int on_dropped(void* param)
	{
		LOG(INFO) << "Network dropped!" << std::endl;
		return 0;
	}

	int SparrowCamera::registerOnDropped(std::function<int(void*)> p_function)
	{
		p_OnDropped_ = p_function;
		return 0;
	}


	int SparrowCamera::HeartBeat()
	{
		//std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		//while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		//{
		//	LOG(INFO) << "--";
		//}

		LOG(TRACE) << "heart beat: ";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock_heartbeat);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock_heartbeat);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_HEARTBEAT, g_sock_heartbeat);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to send disconnection cmd";
			close_socket(g_sock_heartbeat);
			return DF_FAILED;
		}
		ret = send_buffer((char*)&token, sizeof(token), g_sock_heartbeat);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to send token";
			close_socket(g_sock_heartbeat);
			return DF_FAILED;
		}

		int command;
		ret = recv_command(&command, g_sock_heartbeat);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock_heartbeat);
			return DF_FAILED;
		}
		if (command == DF_CMD_OK)
		{
			ret = DF_SUCCESS;
		}
		else if (command == DF_CMD_REJECT)
		{
			ret = DF_BUSY;
		}
		else
		{
			LOG(ERROR) << "Failed recv heart beat command";
		}
		close_socket(g_sock_heartbeat);
		return ret;
	}


	int SparrowCamera::HeartBeat_loop()
	{
		heartbeat_error_count_ = 0;
		while (connected)
		{
			//int ret = HeartBeat();

			char SN[64] = "";
			bool status = true;
#ifdef USE_OPENCV
			status=get_cameraSN(SN);
#endif
			if (!status)
			{
				heartbeat_error_count_++;
				LOG(ERROR) << "heartbeat error count: " << heartbeat_error_count_;

				if (heartbeat_error_count_ > 2)
				{
					LOG(ERROR) << "heart stopped!";
					LOG(ERROR) << "close connect";
					connected = false;
					//disconnect("127.0.0.1");
					close_socket(g_sock);
					p_OnDropped_(0);
					
					
				}

			}
			else
			{
				LOG(ERROR) << "heart ok";
				heartbeat_error_count_ = 0;
			}


			for (int i = 0; i < 100; i++)
			{
				if (!connected)
				{
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
		return 0;
	}

	 

	int SparrowCamera::connectNet(const char* ip)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfConnectNet: ";
		/*******************************************************************************************************************/
		//关闭log输出
		//el::Configurations conf;
		//conf.setToDefault();
		//conf.setGlobally(el::ConfigurationType::Format, "[%datetime{%H:%m:%s} | %level] %msg");
		//conf.setGlobally(el::ConfigurationType::Filename, "log\\log_%datetime{%Y%M%d}.log");
		//conf.setGlobally(el::ConfigurationType::Enabled, "true");
		//conf.setGlobally(el::ConfigurationType::ToFile, "true");
		//el::Loggers::reconfigureAllLoggers(conf);
		//el::Loggers::reconfigureAllLoggers(el::ConfigurationType::ToStandardOutput, "false");


		//DfRegisterOnDropped(on_dropped);
		/*******************************************************************************************************************/


		camera_ip_ = ip;
		LOG(INFO) << "start connection: " << ip;
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}


		LOG(INFO) << "sending connection cmd";
		ret = send_command(DF_CMD_CONNECT, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(INFO) << "Failed to send connection cmd";
			close_socket(g_sock);
			return DF_FAILED;
		}
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}
		if (ret == DF_SUCCESS)
		{
			if (command == DF_CMD_OK)
			{
				LOG(INFO) << "Recieved connection ok";
				ret = recv_buffer((char*)&token, sizeof(token), g_sock);
				if (ret == DF_SUCCESS)
				{
					connected = true;
					LOG(INFO) << "token: " << token;
					close_socket(g_sock);
					if (heartbeat_thread.joinable())
					{
						heartbeat_thread.join();
					}
					heartbeat_thread = std::thread(&SparrowCamera::HeartBeat_loop,this);
					return DF_SUCCESS;
				}
			}
			else if (command == DF_CMD_REJECT)
			{
				LOG(INFO) << "connection rejected";
				close_socket(g_sock);
				return DF_BUSY;
			}
		}
		else
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		return DF_FAILED;
	}


	int SparrowCamera::disconnectNet()
	{

		LOG(INFO) << "token " << token << " try to disconnection";
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";

		}


		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(INFO) << "Failed to setup_socket";
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_DISCONNECT, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(INFO) << "Failed to send disconnection cmd";
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		if (ret == DF_FAILED)
		{
			LOG(INFO) << "Failed to send token";
			close_socket(g_sock);
			return DF_FAILED;
		}
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}
		connected = false;
		token = 0;

		if (heartbeat_thread.joinable())
		{
			heartbeat_thread.join();
		}

		LOG(INFO) << "Camera disconnected";
		return close_socket(g_sock);
	}


	bool SparrowCamera::transformPointcloud(float* org_point_cloud_map, float* transform_point_cloud_map, float* rotate, float* translation)
	{


		int point_num = camera_height_ * camera_width_;

		int nr = camera_height_;
		int nc = camera_width_;

#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{

			for (int c = 0; c < nc; c++)
			{

				int offset = r * camera_width_ + c;

				float x = org_point_cloud_map[3 * offset + 0];
				float y = org_point_cloud_map[3 * offset + 1];
				float z = org_point_cloud_map[3 * offset + 2];

				//if (z > 0)
				//{
				transform_point_cloud_map[3 * offset + 0] = rotate[0] * x + rotate[1] * y + rotate[2] * z + translation[0];
				transform_point_cloud_map[3 * offset + 1] = rotate[3] * x + rotate[4] * y + rotate[5] * z + translation[1];
				transform_point_cloud_map[3 * offset + 2] = rotate[6] * x + rotate[7] * y + rotate[8] * z + translation[2];

				//}
				//else
				//{
				   // point_cloud_map[3 * offset + 0] = 0;
				   // point_cloud_map[3 * offset + 1] = 0;
				   // point_cloud_map[3 * offset + 2] = 0;
				//}


			}

		}


		return true;
	}

	int SparrowCamera::depthTransformPointcloud(float* depth_map, float* point_cloud_map)
	{

		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}

		float camera_fx = calibration_param_.camera_intrinsic[0];
		float camera_fy = calibration_param_.camera_intrinsic[4];

		float camera_cx = calibration_param_.camera_intrinsic[2];
		float camera_cy = calibration_param_.camera_intrinsic[5];


		float k1 = calibration_param_.camera_distortion[0];
		float k2 = calibration_param_.camera_distortion[1];
		float p1 = calibration_param_.camera_distortion[2];
		float p2 = calibration_param_.camera_distortion[3];
		float k3 = calibration_param_.camera_distortion[4];


		int nr = camera_height_;
		int nc = camera_width_;

#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{

			for (int c = 0; c < nc; c++)
			{



				int offset = r * camera_width_ + c;
				if (depth_map[offset] > 0)
				{
					//double undistort_x = c;
					//double undistort_y = r;
					//undistortPoint(c, r, camera_fx, camera_fy,
					//	camera_cx, camera_cy, k1, k2, k3, p1, p2, undistort_x, undistort_y);

					float undistort_x = undistort_map_x_[offset];
					float undistort_y = undistort_map_y_[offset];

					point_cloud_map[3 * offset + 0] = (undistort_x - camera_cx) * depth_map[offset] / camera_fx;
					point_cloud_map[3 * offset + 1] = (undistort_y - camera_cy) * depth_map[offset] / camera_fy;
					point_cloud_map[3 * offset + 2] = depth_map[offset];


				}
				else
				{
					point_cloud_map[3 * offset + 0] = 0;
					point_cloud_map[3 * offset + 1] = 0;
					point_cloud_map[3 * offset + 2] = 0;
				}


			}

		}


		return DF_SUCCESS;
	}


	int SparrowCamera::getCameraPixelType(int& type)
	{

		int get_type = 0;

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_ERROR_NETWORK;
		}
		ret = send_command(DF_CMD_GET_CAMERA_PIXEL_TYPE, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_ERROR_NETWORK;
		}

		if (command == DF_CMD_OK)
		{
			ret = recv_buffer((char*)(&get_type), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_ERROR_NETWORK;
			}

			LOG(INFO) << "Frame Status: " << get_type;
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		type = get_type;
		return DF_SUCCESS;
	}

	int SparrowCamera::getFrameStatus(int& status)
	{

		int get_status = 0; 
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_ERROR_NETWORK;
		}
		ret = send_command(DF_CMD_GET_FRAME_STATUS, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_ERROR_NETWORK;
		}

		if (command == DF_CMD_OK)
		{
			ret = recv_buffer((char*)(&get_status), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_ERROR_NETWORK;
			}

			LOG(INFO) << "Frame Status: " << status;
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		status = get_status;
		return DF_SUCCESS;
	}

	int SparrowCamera::getCalibrationParam(struct CameraCalibParam& calibration_param)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_CAMERA_PARAMETERS, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = recv_buffer((char*)(&calibration_param), sizeof(calibration_param), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
#ifdef USE_OPENCV
	int SparrowCamera::accuracy_single(int board_size, struct CalibrationParam* calibration_param, cv::Mat bright, cv::Mat depth, double& accuracy, cv::Mat& draw_map)
	{
		cv::Mat copy_bright = bright.clone();
		struct BoardMessage boardmessage;
		switch (board_size)
		{
		case 4:
		{
			boardmessage.cols = 7;
			boardmessage.rows = 11;
			boardmessage.width = 4;
			boardmessage.height = 2;
		}
		break;
		case 12:
		{
			boardmessage.cols = 7;
			boardmessage.rows = 11;
			boardmessage.width = 12;
			boardmessage.height = 6;
		}
		break;
		case 20:
		{
			boardmessage.cols = 7;
			boardmessage.rows = 11;
			boardmessage.width = 20;
			boardmessage.height = 10;
		}
		break;
		case 40:
		{
			boardmessage.cols = 7;
			boardmessage.rows = 11;
			boardmessage.width = 40;
			boardmessage.height = 20;
		}
		break;
		case 80:
		{
			boardmessage.cols = 9;
			boardmessage.rows = 13;
			boardmessage.width = 80;
			boardmessage.height = 40;
		}
		break;

		case 45:
		{
			boardmessage.cols = 2;
			boardmessage.rows = 3;
			boardmessage.width = 45;
			boardmessage.height = 22.5;
		}
		break;
		case 90:
		{
			boardmessage.cols = 2;
			boardmessage.rows = 3;
			boardmessage.width = 90;
			boardmessage.height = 45;
		}
		break;
		case 130:
		{
			boardmessage.cols = 2;
			boardmessage.rows = 3;
			boardmessage.width = 130;
			boardmessage.height = 65;
		}
		break;
		case 180:
		{
			boardmessage.cols = 2;
			boardmessage.rows = 3;
			boardmessage.width = 180;
			boardmessage.height = 90;
		}
		break;
		default:
			break;
		}

		Calibrate_Function calib_function;
		calib_function.setBoardMessage(boardmessage);


		if (bright.type() == CV_8UC3)
		{
			std::vector<cv::Mat> channels;
			cv::split(bright, channels);
			bright = channels[2].clone();

		}
		//cv::Mat cameraMatrix(3, 3, CV_32FC1, calibration_param->intrinsic);
		//cv::Mat distCoeffs = cv::Mat(5, 1, CV_32F, calibration_param->distortion);


		//cv::Mat img = bright.clone();
		//cv::Mat undist_img;
		//cv::undistort(bright, undist_img, cameraMatrix, distCoeffs);

		if (true)
		{
			std::vector<cv::Point2f> circle_points;
			std::vector<int>id_list;
			std::vector<int>status_list;
			int hai = 0;
			bool found = -1;
			if ((board_size == 45) || (board_size == 90) || (board_size == 130) || (board_size == 180)) {
				found = detectCCTag(bright, circle_points, id_list, status_list, hai);
			}
			else
			{
				found = calib_function.findCircleBoardFeature(bright, circle_points);
			}

			if (!found)
			{
				return -1;
			}

			//显示
			else
			{
				cv::Mat color_img;
				cv::Size board_size_ = calib_function.getBoardSize();
				cv::cvtColor(bright, color_img, cv::COLOR_GRAY2BGR);
				cv::drawChessboardCorners(color_img, board_size_, circle_points, found);
				cv::drawChessboardCorners(copy_bright, board_size_, circle_points, found);
				if (copy_bright.type() == CV_8UC3) {
					draw_map = copy_bright.clone();
				}
				else
				{
					draw_map = color_img.clone();
				}

				cv::medianBlur(depth, depth, 3);
				cv::Mat points_map(bright.size(), CV_32FC3, cv::Scalar(0., 0., 0.));
				depthTransformPointcloud((float*)depth.data, (float*)points_map.data);

				/*******************************************************************************************/
				std::vector<cv::Point3f> point_3d;
				Calibrate_Function calibrate;
				calibrate.bilinearInterpolationFeaturePoints(circle_points, point_3d, points_map);

				PrecisionTest precision_machine;
				cv::Mat pc1(point_3d.size(), 3, CV_64F, cv::Scalar(0));
				cv::Mat pc2(point_3d.size(), 3, CV_64F, cv::Scalar(0));

				std::vector<cv::Point3f> world_points;
				if (board_size == 45) {
					world_points.push_back(cv::Point3f(0, 0, 0));
					world_points.push_back(cv::Point3f(45, 0, 0));
					world_points.push_back(cv::Point3f(22.5, 22.5, 0));
					world_points.push_back(cv::Point3f(67.5, 22.5, 0));
					world_points.push_back(cv::Point3f(0, 45, 0));
					world_points.push_back(cv::Point3f(45, 45, 0));
				}
				else
				{
					world_points = calib_function.generateAsymmetricWorldFeature(boardmessage);
				}

				for (int r = 0; r < point_3d.size(); r++)
				{
					pc2.at<double>(r, 0) = point_3d[r].x;
					pc2.at<double>(r, 1) = point_3d[r].y;
					pc2.at<double>(r, 2) = point_3d[r].z;
				}
				for (int r = 0; r < world_points.size(); r++)
				{
					pc1.at<double>(r, 0) = world_points[r].x;
					pc1.at<double>(r, 1) = world_points[r].y;
					pc1.at<double>(r, 2) = world_points[r].z;
				}

				cv::Mat r(3, 3, CV_64F, cv::Scalar(0));
				cv::Mat t(3, 3, CV_64F, cv::Scalar(0));

				precision_machine.svdIcp(pc1, pc2, r, t);

				std::vector<cv::Point3f> transform_points;

				precision_machine.transformPoints(point_3d, transform_points, r, t);

				double diff = precision_machine.computeTwoPointSetDistance(world_points, transform_points);
				accuracy = diff;

			}
		}
		return 0;
	}

	int SparrowCamera::dfGetCameraPixelType(int& type)
	{

		int get_type = 0;

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_ERROR_NETWORK;
		}
		ret = send_command(DF_CMD_GET_CAMERA_PIXEL_TYPE, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_ERROR_NETWORK;
		}

		if (command == DF_CMD_OK)
		{
			ret = recv_buffer((char*)(&get_type), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_ERROR_NETWORK;
			}

			LOG(INFO) << "Frame Status: " << get_type;
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		type = get_type;
		return DF_SUCCESS;
	}

	int SparrowCamera::over_exposure(cv::Mat bright_map, cv::Mat& over_bright_map) {
		if (bright_map.type() == CV_8UC3) {
			renderColorBrightnessImage(bright_map, over_bright_map);
		}
		else if (bright_map.type() == CV_8UC1) {
			renderBrightnessImage(bright_map, over_bright_map);
		}
		else
		{
			return -1;
		}

		
		return 0;
	}

	int SparrowCamera::renderBrightnessImage(cv::Mat bright_map, cv::Mat& over_bright_map) {

		cv::Mat color_map(bright_map.size(), CV_8UC3, cv::Scalar(0, 0, 0));

		int nr = color_map.rows;
		int nc = color_map.cols;

		for (int r = 0; r < nr; r++)
		{
			uchar* ptr_b = bright_map.ptr<uchar>(r);
			cv::Vec3b* ptr_cb = color_map.ptr<cv::Vec3b>(r);
			for (int c = 0; c < nc; c++)
			{
				if (ptr_b[c] == 255)
				{
					ptr_cb[c][0] = 255;
					ptr_cb[c][1] = 0;
					ptr_cb[c][2] = 0;
				}
				else
				{
					ptr_cb[c][0] = ptr_b[c];
					ptr_cb[c][1] = ptr_b[c];
					ptr_cb[c][2] = ptr_b[c];
				}
			}

		}
		cv::Mat Rgb;
		cv::cvtColor(color_map, Rgb, cv::COLOR_BGR2RGB);
		over_bright_map = Rgb.clone();
		return 0;
	}

	int SparrowCamera::renderColorBrightnessImage(cv::Mat bright_map, cv::Mat& over_bright_map) {

		cv::Mat color_map(bright_map.size(), CV_8UC3, cv::Scalar(0, 0, 0));

		int nr = color_map.rows;
		int nc = color_map.cols;

		for (int r = 0; r < nr; r++)
		{
			cv::Vec3b* ptr_b = bright_map.ptr<cv::Vec3b>(r);
			cv::Vec3b* ptr_cb = color_map.ptr<cv::Vec3b>(r);
			for (int c = 0; c < nc; c++)
			{
				if (ptr_b[c][2] == 255)
				{
					ptr_cb[c][0] = 0;
					ptr_cb[c][1] = 0;
					ptr_cb[c][2] = 255;
				}
				else
				{
					ptr_cb[c][0] = ptr_b[c][0];
					ptr_cb[c][1] = ptr_b[c][1];
					ptr_cb[c][2] = ptr_b[c][2];
				}
			}

		}
		over_bright_map = color_map.clone();
		return 0;
	}

	bool SparrowCamera::search_camera() {

		const wchar_t* processName = L"camera_server.exe";
		bool exists = false;
		PROCESSENTRY32W entry;
		entry.dwSize = sizeof(PROCESSENTRY32W);

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot == INVALID_HANDLE_VALUE) {
			return false;
		}
		if (Process32FirstW(snapshot, &entry)) {
			do {
				if (wcscmp(entry.szExeFile, processName) == 0) {
					exists = true;
					break;
				}
			} while (Process32NextW(snapshot, &entry));
		}

		CloseHandle(snapshot);
		return exists;
	}

	bool SparrowCamera::get_cameraSN(char SN[64]) {

		std::string Sn;

		GX_STATUS status = GX_STATUS_SUCCESS;
		uint32_t nDeviceNum = 0;

		status = GXInitLib();
		if (status != GX_STATUS_SUCCESS)
		{
			return false;
		}

		status = GXUpdateDeviceList(&nDeviceNum, 1000);
		if ((status != GX_STATUS_SUCCESS) || (nDeviceNum <= 0))
		{
			return false;
		}

		char cam_idx[8] = "0";
		if (status == GX_STATUS_SUCCESS && nDeviceNum > 0)
		{
			GX_DEVICE_BASE_INFO* pBaseinfo = new GX_DEVICE_BASE_INFO[nDeviceNum];
			size_t nSize = nDeviceNum * sizeof(GX_DEVICE_BASE_INFO);

			status = GXGetAllDeviceBaseInfo(pBaseinfo, &nSize);
			for (int i = 0; i < nDeviceNum; i++)
			{
				if (GX_DEVICE_CLASS_GEV == pBaseinfo[i].deviceClass)
				{
					snprintf(cam_idx, 8, "%d", i + 1);
					//std::cout << "SN: " << pBaseinfo[i].szSN << std::endl;
					Sn = std::string(pBaseinfo[i].szSN);
				}
			}

			delete[] pBaseinfo;
		}
		std::memcpy(SN, Sn.c_str(), Sn.size());
		return true;
	}

	int SparrowCamera::get_calib_param(const char* calib_param_path)
	{

		getCalibrationParam(calibration_param_);
		std::ofstream ofile;
		ofile.open(calib_param_path);
		for (int i = 0; i < 9; i++)
		{
			ofile << calibration_param_.camera_intrinsic[i] << std::endl;
		}
		for (int i = 0; i < 5; i++)
		{
			ofile << calibration_param_.camera_distortion[i] << std::endl;
		}

		for (int i = 0; i < 9; i++)
		{
			ofile << calibration_param_.projector_intrinsic[i] << std::endl;
		}
		for (int i = 0; i < 5; i++)
		{
			ofile << calibration_param_.projector_distortion[i] << std::endl;
		}
		for (int i = 0; i < 9; i++)
		{
			ofile << calibration_param_.rotation_matrix[i] << std::endl;
		}
		for (int i = 0; i < 3; i++)
		{
			ofile << calibration_param_.translation_matrix[i] << std::endl;
		}
		ofile.close();

		return DF_SUCCESS;
	}

	int SparrowCamera::get_onecalibrationdata(const char* calib_param_path) {
		int width, height;
		getCameraResolution(&width, &height);

		int capture_num = 37;

		int image_size = width * height;

		unsigned char* raw_buf = new unsigned char[(long)(image_size * capture_num)];

		int ret = DfGetCameraRawDataTest(raw_buf, image_size * capture_num);

		save_images(calib_param_path, raw_buf, width, height, capture_num);

		delete[] raw_buf;

		return DF_SUCCESS;

	}

	void SparrowCamera::save_images(const char* raw_image_dir, unsigned char* buffer, int width, int height, int image_num) {
		std::string folderPath1 = raw_image_dir;
		std::filesystem::path folderPath = folderPath1;
		if (!std::filesystem::exists(folderPath)) {
			std::filesystem::create_directories(folderPath);
		}

		int image_size = width * height;

		for (int i = 0; i < image_num; i++)
		{
			std::stringstream ss;
			cv::Mat image(height, width, CV_8UC1, buffer + (long)(image_size * i));
			ss << std::setw(2) << std::setfill('0') << i;
			std::filesystem::path filename = folderPath / ("phase" + ss.str() + ".bmp");
			cv::imwrite(filename.string(), image);
		}
	}

	int SparrowCamera::correct_stereo_Mono(int board_size, std::string patterns_path, std::string in_path, std::string out_path, float* c_in, float* c_dis, float* p_in, float* p_dis, float* r, float* t, double& error) {

		bool ret = correct_stereo_monO(board_size, patterns_path, in_path, out_path, c_in, c_dis, p_in, p_dis, r, t, error);
		if (ret == DF_SUCCESS) {
			return DF_SUCCESS;
		}
		else
		{
			return DF_FAILED;
		}

	}

	int SparrowCamera::correct_stereo_Color(int board_size, std::string patterns_path, std::string in_path, std::string out_path, float* c_in, float* c_dis, float* p_in, float* p_dis, float* r, float* t, double& error) {

		bool ret = correct_stereo_coloR(board_size, patterns_path, in_path, out_path, c_in, c_dis, p_in, p_dis, r, t, error);
		if (ret == DF_SUCCESS) {
			return DF_SUCCESS;
		}
		else
		{
			return DF_FAILED;
		}

	}

	int SparrowCamera::set_calib_looktable(float* c_in, float* c_dis, float* p_in, float* p_dis, float* r, float* t) {

		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "set_calib_looktable: ";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_CALIB_PARAM, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			float plane_param[40];

			memcpy(plane_param, c_in, 9 * sizeof(float));
			memcpy(plane_param + 9, c_dis, 5 * sizeof(float));
			memcpy(plane_param + 14, p_in, 9 * sizeof(float));
			memcpy(plane_param + 23, p_dis, 5 * sizeof(float));
			memcpy(plane_param + 28, r, 9 * sizeof(float));
			memcpy(plane_param + 37, t, 3 * sizeof(float));

			ret = send_buffer((char*)(plane_param), sizeof(float) * 40, g_sock);


			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
#endif

	int SparrowCamera::connect(const char* camera_id)
	{

		LOG(INFO) << "DfConnect: ";
		/*******************************************************************************************************************/
		//关闭log输出
		el::Configurations conf;
		conf.setToDefault();
		//conf.setGlobally(el::ConfigurationType::Format, "[%datetime{%H:%m:%s} | %level] %msg"); 
#ifdef _WIN32 
	//conf.setGlobally(el::ConfigurationType::Filename, "log\\log_%datetime{%Y%M%d}.log");
		conf.setGlobally(el::ConfigurationType::Filename, "Sparrow_log.log");
#elif __linux 
	//conf.setGlobally(el::ConfigurationType::Filename, "log/log_%datetime{%Y%M%d}.log");
		conf.setGlobally(el::ConfigurationType::Filename, "xema_log.log");
#endif 
		conf.setGlobally(el::ConfigurationType::Enabled, "true");
		conf.setGlobally(el::ConfigurationType::ToFile, "true");
		//conf.setGlobally(el::ConfigurationType::MaxLogFileSize, "204800");//1024*1024*1024=1073741824 
		el::Loggers::reconfigureAllLoggers(conf);
		el::Loggers::reconfigureAllLoggers(el::ConfigurationType::ToStandardOutput, "false");

		/*******************************************************************************************************************/

		registerOnDropped(on_dropped);


		int ret = connectNet(camera_id);
		if (ret != DF_SUCCESS)
		{
			return ret;
		}

		ret = getCalibrationParam(calibration_param_);

		if (ret != DF_SUCCESS)
		{
			disconnectNet();
			return ret;
		}

		int width, height;
		ret = getCameraResolution(&width, &height);
		if (ret != DF_SUCCESS)
		{
			disconnectNet();
			return ret;
		}

		if (width <= 0 || height <= 0)
		{
			disconnectNet();
			return DF_ERROR_2D_CAMERA;
		}

		int pixel_type = 0;

		ret = getCameraPixelType(pixel_type);
		if (ret == DF_SUCCESS)
		{
			pixel_type_ = SparrowPixelType(pixel_type);
		}
		else if (ret == DF_UNKNOWN)
		{
			pixel_type_ = SparrowPixelType::Mono;
		}

		camera_width_ = width;
		camera_height_ = height;

		camera_ip_ = camera_id;
		connected_flag_ = true;

		int image_size_ = camera_width_ * camera_height_;

		depth_buf_size_ = image_size_ * 1 * 4;
		depth_buf_ = (float*)(new char[depth_buf_size_]);

		pointcloud_buf_size_ = depth_buf_size_ * 3;
		point_cloud_buf_ = (float*)(new char[pointcloud_buf_size_]);

		trans_point_cloud_buf_ = (float*)(new char[pointcloud_buf_size_]);

		brightness_bug_size_ = image_size_;
		brightness_buf_ = new unsigned char[brightness_bug_size_];


		rgb_buf_ = new unsigned char[3 * brightness_bug_size_];
		/******************************************************************************************************/
		//产生畸变校正表
		undistort_map_x_ = (float*)(new char[depth_buf_size_]);
		undistort_map_y_ = (float*)(new char[depth_buf_size_]);
		distorted_map_x_ = (float*)(new char[depth_buf_size_]);
		distorted_map_y_ = (float*)(new char[depth_buf_size_]);

		float camera_fx = calibration_param_.camera_intrinsic[0];
		float camera_fy = calibration_param_.camera_intrinsic[4];

		float camera_cx = calibration_param_.camera_intrinsic[2];
		float camera_cy = calibration_param_.camera_intrinsic[5];


		float k1 = calibration_param_.camera_distortion[0];
		float k2 = calibration_param_.camera_distortion[1];
		float p1 = calibration_param_.camera_distortion[2];
		float p2 = calibration_param_.camera_distortion[3];
		float k3 = calibration_param_.camera_distortion[4];


		int nr = camera_height_;
		int nc = camera_width_;

#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{

			for (int c = 0; c < nc; c++)
			{
				double undistort_x = c;
				double undistort_y = r;


				int offset = r * camera_width_ + c;

				undistortPoint(c, r, camera_fx, camera_fy,
					camera_cx, camera_cy, k1, k2, k3, p1, p2, undistort_x, undistort_y);

				undistort_map_x_[offset] = (float)undistort_x;
				undistort_map_y_[offset] = (float)undistort_y;
			}

		}
		/********************************************************************************************************************/
			// 生成畸变矫正的畸变表
#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{

			for (int c = 0; c < nc; c++)
			{
				float distorted_x, distorted_y;


				int offset = r * camera_width_ + c;

				distortPoint(camera_fx, camera_fy,camera_cx, camera_cy, k1, k2, k3, p1, p2,
					c, r, distorted_x, distorted_y);

				distorted_map_x_[offset] = (float)distorted_x;
				distorted_map_y_[offset] = (float)distorted_y;
			}

		}

		/*****************************************************************************************************************/

		el::Loggers::addFlag(el::LoggingFlag::StrictLogFileSizeCheck);
		el::Loggers::reconfigureAllLoggers(el::ConfigurationType::MaxLogFileSize, "104857600");//100MB 104857600

		/// 注册回调函数
		el::Helpers::installPreRollOutCallback(rolloutHandler);


		/********************************************************************************************************/



		return 0;
	}


	//函数名： setParamReflectFilter
	//功能： 设置亮度图增益
	//输入参数：use(开关：1开、0关)、param_b（过滤系数：范围0-100）
	//输出参数： 无
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::setParamReflectFilter(int use, float param_b)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "setParamReflectFilter:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_GLOBAL_LIGHT_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = send_buffer((char*)(&param_b), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

	//函数名： getParamReflectFilter
	//功能： 获取亮度图增益
	//输入参数：无
	//输出参数：use(开关：1开、0关)、param_b（过滤系数：范围0-100）
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::getParamReflectFilter(int& use, float& param_b)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "getParamReflectFilter:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_GLOBAL_LIGHT_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(&param_b), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}



	int SparrowCamera::DfGetCameraRawDataTest(unsigned char* raw, int raw_buf_size) {
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		if (raw)
		{
			LOG(INFO) << "GetRawTest";
			assert(raw_buf_size >= image_size_ * sizeof(unsigned char) * 37);
			int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
			ret = send_command(DF_CMD_GET_RAW_TEST, g_sock);
			ret = send_buffer((char*)&token, sizeof(token), g_sock);
			int command;
			ret = recv_command(&command, g_sock);
			if (ret == DF_FAILED)
			{
				LOG(ERROR) << "Failed to recv command";
				close_socket(g_sock);
				return DF_FAILED;
			}

			if (command == DF_CMD_OK)
			{
				LOG(INFO) << "token checked ok";
				LOG(INFO) << "receiving buffer, raw_buf_size=" << raw_buf_size;
				ret = recv_buffer((char*)raw, raw_buf_size, g_sock);
				LOG(INFO) << "images received";
				if (ret == DF_FAILED)
				{
					close_socket(g_sock);
					return DF_FAILED;
				}
			}
			else if (command == DF_CMD_REJECT)
			{
				LOG(INFO) << "Get raw rejected";
				close_socket(g_sock);
				return DF_BUSY;
			}

			LOG(INFO) << "Get raw success";
			close_socket(g_sock);
			return DF_SUCCESS;
		}
		return DF_FAILED;
	}



	//函数名： DfCaptureBrightnessData
	//功能： 获取亮度图
	//输入参数： color(图像颜色类型)
	//输出参数： brightness(亮度图)
	//返回值： 类型（int）:返回0表示获取数据成功;返回-1表示采集数据失败.
	int SparrowCamera::captureBrightnessData(unsigned char* brightness, SparrowColor color)
	{
		LOG(INFO) << "brightness_bug_size: " << brightness_bug_size_;
		int ret = DF_SUCCESS;

		ret = getBrightness(brightness_buf_, brightness_bug_size_);

		if (DF_SUCCESS != ret)
		{
			return ret;
		}
		 
		if (pixel_type_ == SparrowPixelType::BayerRG8)
		{
			bayerToRgb(brightness_buf_, rgb_buf_);

			switch (color)
			{
			case SparrowColor::Rgb:
			{
				memcpy(brightness, rgb_buf_, 3 * brightness_bug_size_);
			}
			break;
			case SparrowColor::Bgr:
			{
				for (int i = 0; i < brightness_bug_size_; i++)
				{
					brightness[3 * i + 0] = rgb_buf_[3 * i + 2];
					brightness[3 * i + 1] = rgb_buf_[3 * i + 1];
					brightness[3 * i + 2] = rgb_buf_[3 * i + 0];
				}
			}
			break;
			case SparrowColor::Bayer:
			{
				memcpy(brightness, brightness_buf_, brightness_bug_size_);
			}
			break;
			case SparrowColor::Gray:
			{
				rgbToGray(rgb_buf_, brightness);
			}
			break;
			default:
				break;
			}

		}
		else
		{
			switch (color)
			{
			case SparrowColor::Rgb:
			{
				for (int i = 0; i < brightness_bug_size_; i++)
				{
					brightness[3 * i + 0] = brightness_buf_[i];
					brightness[3 * i + 1] = brightness_buf_[i];
					brightness[3 * i + 2] = brightness_buf_[i];
				}
			}
			break;
			case SparrowColor::Bgr:
			{
				for (int i = 0; i < brightness_bug_size_; i++)
				{
					brightness[3 * i + 0] = brightness_buf_[i];
					brightness[3 * i + 1] = brightness_buf_[i];
					brightness[3 * i + 2] = brightness_buf_[i];
				}
			}
			break;
			case SparrowColor::Bayer:
			{
				memcpy(brightness, brightness_buf_, brightness_bug_size_);
			}
			break;
			case SparrowColor::Gray:
			{
				memcpy(brightness, brightness_buf_, brightness_bug_size_);
			}
			break;
			default:
				break;
			}

		}


		return ret;

	}
	 
	int SparrowCamera::disconnect(const char* camera_id)
	{ 
		LOG(INFO) << "disconnect:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}


		int ret = disconnectNet();

		if (DF_FAILED == ret)
		{
			return DF_FAILED;
		}

		delete[] depth_buf_;
		delete[] brightness_buf_;
		delete[] point_cloud_buf_;
		delete[] trans_point_cloud_buf_;
		delete[] undistort_map_x_;
		delete[] undistort_map_y_;
		delete[] distorted_map_x_;
		delete[] distorted_map_y_;
		delete[] rgb_buf_;

		depth_buf_ = NULL;
		brightness_buf_ = NULL;
		point_cloud_buf_ = NULL;
		trans_point_cloud_buf_ = NULL;
		undistort_map_x_ = NULL;
		undistort_map_y_ = NULL;
		distorted_map_x_ = NULL;
		distorted_map_y_ = NULL;
		rgb_buf_ = NULL;

		connected_flag_ = false;

		/// 注销回调函数
		el::Helpers::uninstallPreRollOutCallback();

		return DF_SUCCESS;
	}
	 
	int SparrowCamera::getCameraChannels(int* channels)
	{
		int type = 0;
		int ret = getCameraPixelType(type);

		if (ret != DF_SUCCESS)
		{
			return ret;
		}

		SparrowPixelType pixel = (SparrowPixelType)type;

		switch (pixel)
		{
		case SparrowPixelType::Mono:
		{
			*channels = 1;
		}
		break;
		case SparrowPixelType::BayerRG8:
		{
			*channels = 3;
		}
		break;

		default:
			*channels = 1;
		}



		return DF_SUCCESS;
	}
	 
	int SparrowCamera::getCameraResolution(int* width, int* height)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetCameraResolution:";
		*width = camera_width_;
		*height = camera_height_;

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_CAMERA_RESOLUTION, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(width), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(height), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{

			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);

		LOG(INFO) << "width: " << *width;
		LOG(INFO) << "height: " << *height;
		return DF_SUCCESS;

	}
 
	int SparrowCamera::setCaptureEngine(SparrowEngine engine)
	{
		engine_ = engine;
		 
		//std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		//while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		//{
		//	LOG(INFO) << "--";
		//}

		//int val = (int)engine;

		//LOG(INFO) << "DfSetCaptureEngine: " << val;
		//int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		//if (ret == DF_FAILED)
		//{
		//	close_socket(g_sock);
		//	return DF_FAILED;
		//}
		//ret = send_command(DF_CMD_SET_PARAM_CAPTURE_ENGINE, g_sock);
		//ret = send_buffer((char*)&token, sizeof(token), g_sock);
		//int command;
		//ret = recv_command(&command, g_sock);
		//if (ret == DF_FAILED)
		//{
		//	LOG(ERROR) << "Failed to recv command";
		//	close_socket(g_sock);
		//	return DF_FAILED;
		//}

		//if (command == DF_CMD_OK)
		//{
		//	ret = send_buffer((char*)(&val), sizeof(val), g_sock);
		//	if (ret == DF_FAILED)
		//	{
		//		close_socket(g_sock);
		//		return DF_FAILED;
		//	}
		//}
		//else if (command == DF_CMD_REJECT)
		//{
		//	close_socket(g_sock);
		//	return DF_BUSY;
		//}
		//else if (command == DF_CMD_UNKNOWN)
		//{
		//	close_socket(g_sock);
		//	return DF_UNKNOWN;
		//}

		//close_socket(g_sock);


		return DF_SUCCESS;
	}
	 
	int SparrowCamera::getCaptureEngine(SparrowEngine& engine)
	{
		engine = engine_;
		return 0;
	}

	int SparrowCamera::captureData(int exposure_num, char* timestamp)
	{

		LOG(INFO) << "captureData: " << exposure_num;
		int ret = -1;

		if (exposure_num > 1)
		{
			switch (multiple_exposure_model_)
			{
			case 1:
			{


				switch (engine_)
				{
				case SparrowEngine::Normal:
				{
					ret = getFrame04Hdr(depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
					if (DF_SUCCESS != ret)
					{
						return ret;
					}
				}
				break;
				case SparrowEngine::Reflect:
				{
					ret = getFrame06Hdr(depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
					if (DF_SUCCESS != ret)
					{
						return ret;
					}
				}
				break;
				case SparrowEngine::Black:
				{
					ret = getFrame06HdrMono12(depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
					if (DF_SUCCESS != ret)
					{
						return ret;
					}
				}
				break;
				default:
					break;
				}
			}
			break;
			case 2:
			{


				switch (engine_)
				{
				case SparrowEngine::Normal:
				{
					ret = getRepetitionFrame04(repetition_exposure_model_, depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
					if (DF_SUCCESS != ret)
					{
						return ret;
					}
				}
				break;
				case SparrowEngine::Reflect:
				{
					ret = getRepetitionFrame06(repetition_exposure_model_, depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
					if (DF_SUCCESS != ret)
					{
						return ret;
					}
				}
				break;
				case SparrowEngine::Black:
				{
					ret = getRepetitionFrame06Mono12(repetition_exposure_model_, depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
					if (DF_SUCCESS != ret)
					{
						return ret;
					}
				}
				break;
				default:
					break;
				}

			}
			default:
				break;
			}

		}
		else
		{

			switch (engine_)
			{
			case SparrowEngine::Normal:
			{
				ret = getFrame04(depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
				if (DF_SUCCESS != ret)
				{
					return ret;
				}
			}
			break;
			case SparrowEngine::Reflect:
			{
				ret = getFrame06(depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
				if (DF_SUCCESS != ret)
				{
					return ret;
				}
			}
			break;
			case SparrowEngine::Black:
			{
				ret = getFrame06Mono12(depth_buf_, depth_buf_size_, brightness_buf_, brightness_bug_size_);
				if (DF_SUCCESS != ret)
				{
					return ret;
				}
			}
			break;
			default:
				break;
			}


		}

  
		std::string time = get_timestamp();
		for (int i = 0; i < time.length(); i++)
		{
			timestamp[i] = time[i];
		}

		transform_pointcloud_flag_ = false;
		bayer_to_rgb_flag_ = false;


		int status = DF_SUCCESS;
		ret = getFrameStatus(status);
		if (DF_SUCCESS != ret && DF_UNKNOWN != ret)
		{
			LOG(INFO) << "getFrameStatus Failed!";
			//return ret;
		}

		LOG(INFO) << "Frame Status: " << status;
		if (DF_SUCCESS != status)
		{
			return status;
		}

		return DF_SUCCESS;
	}
 
	int SparrowCamera::getUndistortDepthData(float* undistort_depth)
	{

		LOG(INFO) << "DfGetUndistortDepthDataFloat:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}


		LOG(INFO) << "Trans Depth:";
		int point_num = camera_height_ * camera_width_;

		int nr = camera_height_;
		int nc = camera_width_;

#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{
			for (int c = 0; c < nc; c++)
			{
				int offset = r * camera_width_ + c;
				undistort_depth[offset] = depth_buf_[offset];

			}

		}

		undistortDepthMap(undistort_depth);

		LOG(INFO) << "Get Undistort Depth!";

		return DF_SUCCESS;
	}
	 
	int SparrowCamera::getDepthData(float* depth)
	{
		LOG(INFO) << "getDepthData:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}


		LOG(INFO) << "Trans Depth:";
		int point_num = camera_height_ * camera_width_;

		int nr = camera_height_;
		int nc = camera_width_;

#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{
			for (int c = 0; c < nc; c++)
			{
				int offset = r * camera_width_ + c;
				depth[offset] = depth_buf_[offset];

			}

		}

		LOG(INFO) << "Get Depth!";

		return 0;
	}


	int SparrowCamera::getPointcloudData(float* point_cloud)
	{
		LOG(INFO) << "DfGetPointcloudData:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}


		if (!transform_pointcloud_flag_)
		{
			depthTransformPointcloud(depth_buf_, point_cloud_buf_);
			transform_pointcloud_flag_ = true;
		}

		memcpy(point_cloud, point_cloud_buf_, pointcloud_buf_size_);


		LOG(INFO) << "Get Pointcloud!";

		return 0;
	}

	int SparrowCamera::getColorBrightnessData(unsigned char* brightness, SparrowColor color)
	{
		LOG(INFO) << "getColorBrightnessData:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}

		if (pixel_type_ == SparrowPixelType::BayerRG8)
		{

			if (!bayer_to_rgb_flag_)
			{
				bayerToRgb(brightness_buf_, rgb_buf_);
				bayer_to_rgb_flag_ = true;
			}

			switch (color)
			{
			case SparrowColor::Rgb:
			{
				memcpy(brightness, rgb_buf_, 3 * brightness_bug_size_);
			}
			break;
			case SparrowColor::Bgr:
			{
				for (int i = 0; i < brightness_bug_size_; i++)
				{
					brightness[3 * i + 0] = rgb_buf_[3 * i + 2];
					brightness[3 * i + 1] = rgb_buf_[3 * i + 1];
					brightness[3 * i + 2] = rgb_buf_[3 * i + 0];
				}
			}
			break;
			case SparrowColor::Bayer:
			{
				memcpy(brightness, brightness_buf_, brightness_bug_size_);
			}
			break;
			default:
				break;
			}

		}


		return DF_SUCCESS;
	}

	int SparrowCamera::getUndistortColorBrightnessData(unsigned char* brightness, SparrowColor color)
	{

		std::unique_lock<std::timed_mutex> lck(undistort_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "getUndistortColorBrightnessData:"<< (int)pixel_type_;
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}


		if (pixel_type_ == SparrowPixelType::BayerRG8)
		{

			if (!bayer_to_rgb_flag_)
			{
				bayerToRgb(brightness_buf_, rgb_buf_);
				bayer_to_rgb_flag_ = true;
			}
			else
			{

				LOG(ERROR) << "bayer_to_rgb_flag_";
				return DF_FAILED;
			}

			if (NULL == rgb_buf_)
			{
				return DF_FAILED;
			}

			switch (color)
			{
			case SparrowColor::Rgb:
			{ 
				memcpy(brightness, rgb_buf_, 3 * brightness_bug_size_);
				if (DF_SUCCESS != undistortColorBrightnessMap(brightness))
				{
					return DF_FAILED;
				}

			}
			break;
			case SparrowColor::Bgr:
			{ 

				for (int i = 0; i < brightness_bug_size_; i++)
				{
					brightness[3 * i + 0] = rgb_buf_[3 * i + 2];
					brightness[3 * i + 1] = rgb_buf_[3 * i + 1];
					brightness[3 * i + 2] = rgb_buf_[3 * i + 0];
				}

				if (DF_SUCCESS != undistortColorBrightnessMap(brightness))
				{
					LOG(ERROR) << "undistortColorBrightnessMap";
					return DF_FAILED;
				}
			}
			break;
			case SparrowColor::Bayer:
			{ 
				memcpy(brightness, brightness_buf_, brightness_bug_size_);

				if (DF_SUCCESS != undistortBrightnessMap(brightness))
				{
					LOG(ERROR) << "undistortColorBrightnessMap";
					return DF_FAILED;
				}
			}
			break;
			default:
				break;
			}

		}


		return DF_SUCCESS;
	}

	int SparrowCamera::getUndistortBrightnessData(unsigned char* undistort_brightness)
	{

		LOG(INFO) << "DfGetBrightnessData:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}


		LOG(INFO) << "Trans Brightness:";

		memcpy(undistort_brightness, brightness_buf_, brightness_bug_size_);

		undistortBrightnessMap(undistort_brightness);

		//brightness = brightness_buf_;

		LOG(INFO) << "Get Undistort Brightness!";

		return 0;
	}

	int SparrowCamera::getBrightnessData(unsigned char* brightness)
	{
		LOG(INFO) << "DfGetBrightnessData:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}


		LOG(INFO) << "Trans Brightness:";

		//memcpy(brightness, brightness_buf_, brightness_bug_size_);

		if (pixel_type_ == SparrowPixelType::Mono)
		{
			memcpy(brightness, brightness_buf_, brightness_bug_size_);
		}
		else if (pixel_type_ == SparrowPixelType::BayerRG8)
		{
			if (!bayer_to_rgb_flag_)
			{
				bayerToRgb(brightness_buf_, rgb_buf_);
				bayer_to_rgb_flag_ = true;
			}

			rgbToGray(rgb_buf_, brightness);
		}
		else
		{
			return DF_FAILED;
		}

		//brightness = brightness_buf_;

		LOG(INFO) << "Get Brightness!";

		return 0;
	}


	int SparrowCamera::getStandardPlaneParam(float* R, float* T)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetStandardPlaneParam: ";

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}

		int param_buf_size = 12 * 4;
		float* plane_param = new float[param_buf_size];

		ret = send_command(DF_CMD_GET_STANDARD_PLANE_PARAM, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, param_buf_size=" << param_buf_size;
			ret = recv_buffer((char*)plane_param, param_buf_size, g_sock);
			LOG(INFO) << "plane param received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}

		close_socket(g_sock);


		memcpy(R, plane_param, 9 * 4);
		memcpy(T, plane_param + 9, 3 * 4);

		delete[] plane_param;

		LOG(INFO) << "Get plane param success";
		return DF_SUCCESS;

	}


	int SparrowCamera::getHeightMapDataBaseParam(float* R, float* T, float* height_map)
	{
		LOG(INFO) << "DfGetHeightMapDataBaseParam:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}



		if (!transform_pointcloud_flag_)
		{
			depthTransformPointcloud((float*)depth_buf_, (float*)point_cloud_buf_);
			transform_pointcloud_flag_ = true;
		}

		transformPointcloud((float*)point_cloud_buf_, (float*)trans_point_cloud_buf_, R, T);


		int nr = camera_height_;
		int nc = camera_width_;
#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{
			for (int c = 0; c < nc; c++)
			{
				int offset = r * camera_width_ + c;
				if (depth_buf_[offset] > 0)
				{
					height_map[offset] = trans_point_cloud_buf_[offset * 3 + 2];
				}
				else
				{
					height_map[offset] = NULL;
				}

			}


		}


		LOG(INFO) << "Get Height Map!";

		return 0;
	}
	 
	int SparrowCamera::getCalibrationParam(struct CalibrationParam* calibration_param)
	{
		LOG(INFO) << "DfGetCalibrationParam:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}

		//calibration_param = &calibration_param_;

		for (int i = 0; i < 9; i++)
		{
			calibration_param->intrinsic[i] = calibration_param_.camera_intrinsic[i];
		}

		for (int i = 0; i < 5; i++)
		{
			calibration_param->distortion[i] = calibration_param_.camera_distortion[i];
		}

		for (int i = 5; i < 12; i++)
		{
			calibration_param->distortion[i] = 0;
		}

		float extrinsic[4 * 4] = { 1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1 };

		for (int i = 0; i < 16; i++)
		{
			calibration_param->extrinsic[i] = extrinsic[i];
		}


		return 0;
	}


	int SparrowCamera::getHeightMapData(float* height_map)
	{

		LOG(INFO) << "DfGetHeightMapData:";
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}


		struct SystemConfigParam system_config_param;
		int ret_code = getSystemConfigParam(system_config_param);
		if (0 != ret_code)
		{
			std::cout << "Get Param Error;";
			return -1;
		}

		LOG(INFO) << "Transform Pointcloud:";

		if (!transform_pointcloud_flag_)
		{
			depthTransformPointcloud((float*)depth_buf_, (float*)point_cloud_buf_);
			transform_pointcloud_flag_ = true;
		}

		//memcpy(trans_point_cloud_buf_, point_cloud_buf_, pointcloud_buf_size_);
		transformPointcloud((float*)point_cloud_buf_, (float*)trans_point_cloud_buf_, system_config_param.standard_plane_external_param, &system_config_param.standard_plane_external_param[9]);


		int nr = camera_height_;
		int nc = camera_width_;
#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{
			for (int c = 0; c < nc; c++)
			{
				int offset = r * camera_width_ + c;
				if (depth_buf_[offset] > 0)
				{
					height_map[offset] = trans_point_cloud_buf_[offset * 3 + 2];
				}
				else
				{
					height_map[offset] = NULL;
				}

			}


		}


		LOG(INFO) << "Get Height Map!";

		return 0;
	}

	/************************************************************************************************/
	 

	int SparrowCamera::setParamLedCurrent(int led)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamLedCurrent: " << led;
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_LED_CURRENT, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = send_buffer((char*)(&led), sizeof(led), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::getParamLedCurrent(int& led)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamLedCurrent: ";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_CAMERA_PARAMETERS, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = recv_buffer((char*)(&led), sizeof(led), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		LOG(INFO) << "led: " << led;
		return DF_SUCCESS;
	}
	

	int SparrowCamera::setParamStandardPlaneExternal(float* R, float* T)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamStandardPlaneExternal: ";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_STANDARD_PLANE_EXTERNAL_PARAM, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			float plane_param[12];

			memcpy(plane_param, R, 9 * sizeof(float));
			memcpy(plane_param + 9, T, 3 * sizeof(float));

			ret = send_buffer((char*)(plane_param), sizeof(float) * 12, g_sock);


			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;


	}
	 
	int SparrowCamera::getParamStandardPlaneExternal(float* R, float* T)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamStandardPlaneExternal: ";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_STANDARD_PLANE_EXTERNAL_PARAM, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			//int param_buf_size = 12 * sizeof(float);
			//float* plane_param = new float[param_buf_size];
			float plane_param[12];

			ret = recv_buffer((char*)(plane_param), sizeof(float) * 12, g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			memcpy(R, plane_param, 9 * sizeof(float));
			memcpy(T, plane_param + 9, 3 * sizeof(float));

		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::setParamGenerateBrightness(int model, float exposure)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamGenerateBrightness: ";
		if (exposure < 20 || exposure> 1000000)
		{
			std::cout << "exposure param out of range!" << std::endl;
			return DF_FAILED;
		}

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_GENERATE_BRIGHTNESS, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = send_buffer((char*)(&model), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = send_buffer((char*)(&exposure), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
	 
	int SparrowCamera::getParamGenerateBrightness(int& model, float& exposure)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamGenerateBrightness: ";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_GENERATE_BRIGHTNESS, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&model), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(&exposure), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::setParamCameraExposure(float exposure)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamCameraExposure:";

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_CAMERA_EXPOSURE_TIME, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&exposure), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
 
	int SparrowCamera::getParamCameraExposure(float& exposure)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamCameraExposure:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_CAMERA_EXPOSURE_TIME, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&exposure), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::setParamMixedHdr(int num, int exposure_param[6], int led_param[6])
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamMixedHdr:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_MIXED_HDR, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			int param[13];
			param[0] = num;

			memcpy(param + 1, exposure_param, sizeof(int) * 6);
			memcpy(param + 7, led_param, sizeof(int) * 6);

			ret = send_buffer((char*)(param), sizeof(int) * 13, g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

 
	int SparrowCamera::getParamMixedHdr(int& num, int exposure_param[6], int led_param[6])
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamMixedHdr:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_MIXED_HDR, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			int param[13];

			ret = recv_buffer((char*)(param), sizeof(int) * 13, g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}


			memcpy(exposure_param, param + 1, sizeof(int) * 6);
			memcpy(led_param, param + 7, sizeof(int) * 6);
			num = param[0];

		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::setParamCameraConfidence(float confidence)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamCameraConfidence:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_CAMERA_CONFIDENCE, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = send_buffer((char*)(&confidence), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
 
	int SparrowCamera::getParamCameraConfidence(float& confidence)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamCameraConfidence:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_CAMERA_CONFIDENCE, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&confidence), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::setParamCameraGain(float gain)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamCameraGain:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_CAMERA_GAIN, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&gain), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
 
	int SparrowCamera::getParamCameraGain(float& gain)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}
		LOG(INFO) << "DfGetParamCameraGain:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_CAMERA_GAIN, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&gain), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::setParamSmoothing(int smoothing)
	{

		LOG(INFO) << "DfSetParamSmoothing:";
		int ret = -1;
		if (0 == smoothing)
		{
			ret = setParamBilateralFilter(0, 5);
		}
		else
		{
			ret = setParamBilateralFilter(1, 2 * smoothing + 1);
		}

		return ret;
	}

	int SparrowCamera::getParamSmoothing(int& smoothing)
	{

		LOG(INFO) << "DfGetParamSmoothing:";
		int use = 0;
		int d = 0;

		int ret = getParamBilateralFilter(use, d);

		if (DF_FAILED == ret)
		{
			return DF_FAILED;
		}

		if (0 == use)
		{
			smoothing = 0;
		}
		else if (1 == use)
		{
			smoothing = d / 2;
		}
		return DF_SUCCESS;
	}


	int SparrowCamera::setParamRadiusFilter(int use, float radius, int num)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamRadiusFilter:";
		if (use != 1 && use != 0)
		{
			std::cout << "use param should be 1 or 0:  " << use << std::endl;
			return DF_FAILED;
		}

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_RADIUS_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = send_buffer((char*)(&radius), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = send_buffer((char*)(&num), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

	int SparrowCamera::getParamRadiusFilter(int& use, float& radius, int& num)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamRadiusFilter:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_RADIUS_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(&radius), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(&num), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

 
	int SparrowCamera::setParamDepthFilter(int use, float depth_filter_threshold)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamDepthFilter:";
		if (use != 1 && use != 0)
		{
			std::cout << "use param should be 1 or 0:  " << use << std::endl;
			return DF_FAILED;
		}

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}

		ret = send_command(DF_CMD_SET_PARAM_DEPTH_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = send_buffer((char*)(&depth_filter_threshold), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::getParamDepthFilter(int& use, float& depth_filter_threshold)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamDepthFilter:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_DEPTH_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(&depth_filter_threshold), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

	int SparrowCamera::setParamOutlierFilter(float threshold)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfSetParamOutlierFilter:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_FISHER_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = send_buffer((char*)(&threshold), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::getParamOutlierFilter(float& threshold)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetParamOutlierFilter:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_FISHER_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&threshold), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

	int SparrowCamera::setParamMultipleExposureModel(int model)
	{
		if (model != 1 && model != 2)
		{
			return DF_ERROR_INVALID_PARAM;
		}
		multiple_exposure_model_ = model;

		return DF_SUCCESS;
	}
	 
	int SparrowCamera::setParamRepetitionExposureNum(int num)
	{
		if (num < 2 || num >10)
		{
			return DF_ERROR_INVALID_PARAM;
		}

		repetition_exposure_model_ = num;

		return DF_SUCCESS;
	}

 
	int SparrowCamera::setParamGrayRectify(int use, int radius, float sigma)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		if (radius % 2 != 1 || radius > 9 || radius < 3 || sigma < 1 || sigma > 100)
		{
			LOG(INFO) << "error param range";
			return DF_FAILED;
		}

		LOG(INFO) << "DfSetParamGrayRectify:";
		if (use != 1 && use != 0)
		{
			std::cout << "use param should be 1 or 0:  " << use << std::endl;
			return DF_FAILED;
		}

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_GRAY_RECTIFY, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = send_buffer((char*)(&radius), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = send_buffer((char*)(&sigma), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

 
	int SparrowCamera::getParamGrayRectify(int& use, int& radius, float& sigma)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "getParamGrayRectify:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_GRAY_RECTIFY, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(&radius), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(&sigma), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

	//函数名： setParamBrightnessHdrExposure
	//功能： 设置亮度图多曝光参数（最大曝光次数为10次）
	//输入参数： num（曝光次数）、exposure_param[6]（6个曝光参数、前num个有效））
	//输出参数： 无
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::setParamBrightnessHdrExposure(int num, int exposure_param[10])
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		if (num < 2 || num>10)
		{
			LOG(INFO) << "Error num:" << num;
			return DF_FAILED;
		}

		LOG(INFO) << "setParamBrightnessHdrExposure:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_BRIGHTNESS_HDR_EXPOSURE, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			int param[11] = { 2,10000,20000,30000,40000,50000,60000,70000,80000,90000,100000 };

			param[0] = num;

			memcpy(param + 1, exposure_param, sizeof(int) * num);

			ret = send_buffer((char*)(param), sizeof(int) * 11, g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
 
	int SparrowCamera::getParamBrightnessHdrExposure(int& num, int exposure_param[10]) 
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "getParamBrightnessHdrExposure:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_BRIGHTNESS_HDR_EXPOSURE, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			int param[11];

			ret = recv_buffer((char*)(param), sizeof(int) * 11, g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			memcpy(exposure_param, param + 1, sizeof(int) * 10);
			num = param[0];

		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
	 
	int SparrowCamera::setParamBrightnessExposureModel(int model)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "setParamBrightnessExposureModel:";

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_BRIGHTNESS_EXPOSURE_MODEL, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&model), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
	 
	int SparrowCamera::getParamBrightnessExposureModel(int& model)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "getParamBrightnessExposureModel:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_BRIGHTNESS_EXPOSURE_MODEL, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&model), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}
	 
	int SparrowCamera::setParamBrightnessGain(float gain) 
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}
		
		LOG(INFO) << "setParamBrightnessGain:";

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_BRIGHTNESS_GAIN, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&gain), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

	//函数名： getSdkVersion
	//功能： 获取sdk版本
	//输入参数：无
	//输出参数：version(版本)
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::getSdkVersion(char version[64])
	{
		std::strcpy(version, _VERSION_NUMBER_);
		return DF_SUCCESS;
	}

	//函数名： getFirmwareVersion
	//功能： 获取固件版本
	//输入参数：无
	//输出参数：version(版本)
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。 
	int SparrowCamera::getFirmwareVersion(char version[64])
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		char pVersion[_VERSION_LENGTH_] = { 0 };

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_FIRMWARE_VERSION, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = recv_buffer(pVersion, _VERSION_LENGTH_, g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}

		close_socket(g_sock);

		pVersion[64 - 1] = '\0';
		std::memcpy(version, pVersion, 64);
		LOG(INFO) << "Firmware version: " << version;
		return DF_SUCCESS;
	}
 
	int SparrowCamera::getParamBrightnessGain(float& gain)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "getParamBrightnessGain:";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_BRIGHTNESS_GAIN, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = recv_buffer((char*)(&gain), sizeof(float), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}


	/************************************************************************************************/



	/************************************************************************/
	std::tm* SparrowCamera::gettm(long long timestamp)
	{
		auto milli = timestamp + (long long)8 * 60 * 60 * 1000; //此处转化为东八区北京时间，如果是其它时区需要按需求修改
		auto mTime = std::chrono::milliseconds(milli);
		auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(mTime);
		auto tt = std::chrono::system_clock::to_time_t(tp);
		std::tm* now = std::gmtime(&tt);
		//printf("%4d年%02d月%02d日 %02d:%02d:%02d\n", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec);
		return now;
	}

	std::time_t SparrowCamera::getTimeStamp(long long& msec)
	{
		std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
		auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
		seconds sec = duration_cast<seconds>(tp.time_since_epoch());


		std::time_t timestamp = tmp.count();

		msec = tmp.count() - sec.count() * 1000;
		//std::time_t timestamp = std::chrono::system_clock::to_time_t(tp);
		return timestamp;
	}

	std::string SparrowCamera::get_timestamp()
	{

		long long msec = 0;
		char time_str[7][16];
		auto t = getTimeStamp(msec);
		//std::cout << "Millisecond timestamp is: " << t << std::endl;
		auto time_ptr = gettm(t);
		sprintf(time_str[0], "%02d", time_ptr->tm_year + 1900); //月份要加1
		sprintf(time_str[1], "%02d", time_ptr->tm_mon + 1); //月份要加1
		sprintf(time_str[2], "%02d", time_ptr->tm_mday);//天
		sprintf(time_str[3], "%02d", time_ptr->tm_hour);//时
		sprintf(time_str[4], "%02d", time_ptr->tm_min);// 分
		sprintf(time_str[5], "%02d", time_ptr->tm_sec);//时
		sprintf(time_str[6], "%02lld", msec);// 分
		//for (int i = 0; i < 7; i++)
		//{
		//	std::cout << "time_str[" << i << "] is: " << time_str[i] << std::endl;
		//}

		std::string timestamp = "";

		timestamp += time_str[0];
		timestamp += "-";
		timestamp += time_str[1];
		timestamp += "-";
		timestamp += time_str[2];
		timestamp += " ";
		timestamp += time_str[3];
		timestamp += ":";
		timestamp += time_str[4];
		timestamp += ":";
		timestamp += time_str[5];
		timestamp += ",";
		timestamp += time_str[6];

		//std::cout << timestamp << std::endl;

		return timestamp;
	}


	int SparrowCamera::getBrightness(unsigned char* brightness, int brightness_buf_size)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "GetBrightness";
		//assert(brightness_buf_size >= image_size_ * sizeof(unsigned char));
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		//std::cout << "1" << std::endl;
		ret = send_command(DF_CMD_GET_BRIGHTNESS, g_sock);
		//std::cout << "send token " << token<< std::endl;
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}
		if (command == DF_CMD_OK)
		{
			LOG(INFO) << "token checked ok";
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get brightness rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}

		LOG(INFO) << "Get brightness success";
		close_socket(g_sock);
		return DF_SUCCESS;
	}

	int SparrowCamera::getFrame04(float* depth, int depth_buf_size,unsigned char* brightness, int brightness_buf_size)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}
		LOG(INFO) << "GetFrame04"; 
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}


		ret = send_command(DF_CMD_GET_FRAME_04, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}

		LOG(INFO) << "Get frame04 success";
		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::getRepetitionFrame04(int count, float* depth, int depth_buf_size, unsigned char* brightness, int brightness_buf_size)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "GetRepetition01Frame04";
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}


		ret = send_command(DF_CMD_GET_REPETITION_FRAME_04, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = send_buffer((char*)(&count), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}


			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		LOG(INFO) << "Get frame success";
		close_socket(g_sock);
		return DF_SUCCESS;
	}
	 
	int SparrowCamera::getFrame04Hdr(float* depth, int depth_buf_size,
		unsigned char* brightness, int brightness_buf_size)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "GetFrameHdr";
	//	assert(depth_buf_size == image_size_ * sizeof(float) * 1);
	//	assert(brightness_buf_size == image_size_ * sizeof(char) * 1);
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_FRAME_HDR, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}



			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}

		LOG(INFO) << "Get frame success";
		close_socket(g_sock);
		return DF_SUCCESS;
	}


	int SparrowCamera::getFrame06(float* depth, int depth_buf_size, unsigned char* brightness, int brightness_buf_size)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}
		LOG(INFO) << "GetFrame06";
                //assert(depth_buf_size == image_size_ * sizeof(float) * 1);
                //assert(brightness_buf_size == image_size_ * sizeof(char) * 1);
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}


		ret = send_command(DF_CMD_GET_FRAME_06, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			LOG(INFO) << "Get frame DF_CMD_UNKNOWN";
			return DF_UNKNOWN;
		}

		close_socket(g_sock);



		LOG(INFO) << "Get frame06 success";
		return DF_SUCCESS;
	}

	int SparrowCamera::getFrame06Hdr(float* depth, int depth_buf_size, unsigned char* brightness, int brightness_buf_size)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}
		LOG(INFO) << "GetFrame06Hdr";
                //assert(depth_buf_size == image_size_ * sizeof(float) * 1);
                //assert(brightness_buf_size == image_size_ * sizeof(char) * 1);
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}

		ret = send_command(DF_CMD_GET_FRAME_06_HDR, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);


		LOG(INFO) << "Get FRAME 06 HDR success";
		return DF_SUCCESS;
	}

	int SparrowCamera::getRepetitionFrame06(int count, float* depth, int depth_buf_size, unsigned char* brightness, int brightness_buf_size)
	{

		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "GetRepetitionFrame06";
                //assert(depth_buf_size == image_size_ * sizeof(float) * 1);
                //assert(brightness_buf_size == image_size_ * sizeof(char) * 1);
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}


		ret = send_command(DF_CMD_GET_REPETITION_FRAME_06, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = send_buffer((char*)(&count), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}


			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		LOG(INFO) << "Get frame success";
		close_socket(g_sock);
		return DF_SUCCESS;
	}

	int SparrowCamera::getFrame06Mono12(float* depth, int depth_buf_size,
		unsigned char* brightness, int brightness_buf_size)
	{

		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}
		LOG(INFO) << "GetFrame06";
		//assert(depth_buf_size == image_size_ * sizeof(float) * 1);
		//assert(brightness_buf_size == image_size_ * sizeof(char) * 1);
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}

		ret = send_command(DF_CMD_GET_FRAME_06_MONO12, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			LOG(INFO) << "Get frame DF_CMD_UNKNOWN";
			return DF_UNKNOWN;
		}

		close_socket(g_sock);



		LOG(INFO) << "Get frame06 success";
		return DF_SUCCESS;
	}

	int SparrowCamera::getFrame06HdrMono12(float* depth, int depth_buf_size,
		unsigned char* brightness, int brightness_buf_size)
	{

		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}
		LOG(INFO) << "DfGetFrame06HdrMono12";
		//assert(depth_buf_size == image_size_ * sizeof(float) * 1);
		//assert(brightness_buf_size == image_size_ * sizeof(char) * 1);
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}

		ret = send_command(DF_CMD_GET_FRAME_06_HDR_MONO12, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			LOG(INFO) << "Get frame DF_CMD_UNKNOWN";
			return DF_UNKNOWN;
		}

		close_socket(g_sock);



		LOG(INFO) << "Get frame06 success";
		return DF_SUCCESS;
	}

	int SparrowCamera::getRepetitionFrame06Mono12(int count, float* depth, int depth_buf_size,
		unsigned char* brightness, int brightness_buf_size)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		LOG(INFO) << "DfGetRepetitionFrame06Mono12";
		//assert(depth_buf_size == image_size_ * sizeof(float) * 1);
		//assert(brightness_buf_size == image_size_ * sizeof(char) * 1);
		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}


		ret = send_command(DF_CMD_GET_REPETITION_FRAME_06_MONO12, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = send_buffer((char*)(&count), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "token checked ok";
			LOG(INFO) << "receiving buffer, depth_buf_size=" << depth_buf_size;
			ret = recv_buffer((char*)depth, depth_buf_size, g_sock);
			LOG(INFO) << "depth received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			LOG(INFO) << "receiving buffer, brightness_buf_size=" << brightness_buf_size;
			ret = recv_buffer((char*)brightness, brightness_buf_size, g_sock);
			LOG(INFO) << "brightness received";
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}


			//brightness = (unsigned char*)depth + depth_buf_size;
		}
		else if (command == DF_CMD_REJECT)
		{
			LOG(INFO) << "Get frame rejected";
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		LOG(INFO) << "Get frame success";
		close_socket(g_sock);
		return DF_SUCCESS;
	}

	int SparrowCamera::getSystemConfigParam(struct SystemConfigParam& config_param)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_SYSTEM_CONFIG_PARAMETERS, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{
			ret = recv_buffer((char*)(&config_param), sizeof(config_param), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

	 
	//功能： 设置双边滤波参数
	//输入参数： use（开关：1为开、0为关）、param_d（平滑系数：3、5、7、9、11）
	//输出参数： 无
	//返回值： 类型（int）:返回0表示获取标定参数成功;返回-1表示获取标定参数失败.
	int SparrowCamera::setParamBilateralFilter(int use, int param_d)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		if (use != 1 && use != 0)
		{
			std::cout << "use param should be 1 or 0:  " << use << std::endl;
			return DF_FAILED;
		}

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_SET_PARAM_BILATERAL_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = send_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = send_buffer((char*)(&param_d), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	}

	//函数名： DfGetParamBilateralFilter
	//功能： 获取混合多曝光参数（最大曝光次数为6次）
	//输入参数： 无
	//输出参数： use（开关：1为开、0为关）、param_d（平滑系数：3、5、7、9、11）
	//返回值： 类型（int）:返回0表示获取标定参数成功;返回-1表示获取标定参数失败.
	int SparrowCamera::getParamBilateralFilter(int& use, int& param_d)
	{
		std::unique_lock<std::timed_mutex> lck(command_mutex_, std::defer_lock);
		while (!lck.try_lock_for(std::chrono::milliseconds(1)))
		{
			LOG(INFO) << "--";
		}

		int ret = setup_socket(camera_ip_.c_str(), DF_PORT, g_sock);
		if (ret == DF_FAILED)
		{
			close_socket(g_sock);
			return DF_FAILED;
		}
		ret = send_command(DF_CMD_GET_PARAM_BILATERAL_FILTER, g_sock);
		ret = send_buffer((char*)&token, sizeof(token), g_sock);
		int command;
		ret = recv_command(&command, g_sock);
		if (ret == DF_FAILED)
		{
			LOG(ERROR) << "Failed to recv command";
			close_socket(g_sock);
			return DF_FAILED;
		}

		if (command == DF_CMD_OK)
		{

			ret = recv_buffer((char*)(&use), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}

			ret = recv_buffer((char*)(&param_d), sizeof(int), g_sock);
			if (ret == DF_FAILED)
			{
				close_socket(g_sock);
				return DF_FAILED;
			}
		}
		else if (command == DF_CMD_REJECT)
		{
			close_socket(g_sock);
			return DF_BUSY;
		}
		else if (command == DF_CMD_UNKNOWN)
		{
			close_socket(g_sock);
			return DF_UNKNOWN;
		}

		close_socket(g_sock);
		return DF_SUCCESS;
	} 

	int SparrowCamera::rgbToGray(unsigned char* src, unsigned char* dst)
	{
		int height = camera_height_;
		int width = camera_width_;

		for (int r = 0; r < height; r++)
		{

			for (int c = 0; c < width; c++)
			{
				//0.299 R  + 0.587 G + B
				dst[r * width + c] = src[3 * (r * width + c) + 0] * 0.299 +
					0.587 * src[3 * (r * width + c) + 1] + 0.114 * src[3 * (r * width + c) + 2];
			}

		}


		return 0;
	}

	int SparrowCamera::bayerToRgb(unsigned char* src, unsigned char* dst)
	{

		int height = camera_height_;
		int width = camera_width_;

		//按3×3的邻域处理，边缘需填充至少1个像素的宽度
		int nBorder = 1;
		unsigned char* bayer = (unsigned char*)malloc(sizeof(unsigned char) * (width + 2 * nBorder) * (height + 2 * nBorder));
		memset(bayer, 0, sizeof(unsigned char) * (width + 2 * nBorder) * (height + 2 * nBorder));

		for (int r = 0; r < height; r++)
		{
			for (int c = 0; c < width; c++)
			{
				bayer[(r + nBorder) * (width + 2 * nBorder) + (c + nBorder)] = src[r * width + c];
			}
		}

		for (int b = 0; b < nBorder; b++)
		{
			for (int r = 0; r < height; r++)
			{
				bayer[(r + nBorder) * (width + 2 * nBorder) + b] = src[r * width];
				bayer[(r + nBorder) * (width + 2 * nBorder) + b + 2 * nBorder + width - 1] = src[r * width + width - 1];
			}
		}

		for (int b = 0; b < nBorder; b++)
		{
			for (int c = 0; c < width + 2 * nBorder; c++)
			{
				bayer[b * (width + 2 * nBorder) + c] = bayer[nBorder * (width + 2 * nBorder) + c];
				bayer[(nBorder + height + b) * (width + 2 * nBorder) + c] = bayer[(nBorder + height - 1) * (width + 2 * nBorder) + c];
			}
		}

		unsigned char* p_rgb = (unsigned char*)malloc(sizeof(unsigned char) * 3 * (width + 2 * nBorder) * (height + 2 * nBorder));
		memset(p_rgb, 0, sizeof(unsigned char) * 3 * (width + 2 * nBorder) * (height + 2 * nBorder));


		unsigned char* pBayer = bayer;
		unsigned char* pRGB = p_rgb;
		int nW = width + 2 * nBorder;
		int nH = height + 2 * nBorder;


		for (int i = nBorder; i < nH - nBorder; i++)
		{
			for (int j = nBorder; j < nW - nBorder; j++)
			{
				//3×3邻域像素定义
				/*
				 * |M00 M01 M02|
				 * |M10 M11 M12|
				 * |M20 M21 M22|
				*/
				int nM00 = (i - 1) * nW + (j - 1); int nM01 = (i - 1) * nW + (j + 0);  int nM02 = (i - 1) * nW + (j + 1);
				int nM10 = (i - 0) * nW + (j - 1); int nM11 = (i - 0) * nW + (j + 0);  int nM12 = (i - 0) * nW + (j + 1);
				int nM20 = (i + 1) * nW + (j - 1); int nM21 = (i + 1) * nW + (j + 0);  int nM22 = (i + 1) * nW + (j + 1);

				if (i % 2 == 0)
				{
					if (j % 2 == 0)     //偶数行偶数列
					{
						pRGB[i * nW * 3 + j * 3 + 0] = pBayer[nM11];//b
						pRGB[i * nW * 3 + j * 3 + 2] = ((pBayer[nM00] + pBayer[nM02] + pBayer[nM20] + pBayer[nM22]) >> 2);//r
						pRGB[i * nW * 3 + j * 3 + 1] = ((pBayer[nM01] + pBayer[nM10] + pBayer[nM12] + pBayer[nM21]) >> 2);//g
					}
					else             //偶数行奇数列
					{
						pRGB[i * nW * 3 + j * 3 + 1] = pBayer[nM11];//g
						pRGB[i * nW * 3 + j * 3 + 2] = (pBayer[nM01] + pBayer[nM21]) >> 1;//r
						pRGB[i * nW * 3 + j * 3 + 0] = (pBayer[nM10] + pBayer[nM12]) >> 1;//b
					}
				}
				else
				{
					if (j % 2 == 0)     //奇数行偶数列
					{
						pRGB[i * nW * 3 + j * 3 + 1] = pBayer[nM11];//g
						pRGB[i * nW * 3 + j * 3 + 2] = (pBayer[nM10] + pBayer[nM12]) >> 1;//r
						pRGB[i * nW * 3 + j * 3 + 0] = (pBayer[nM01] + pBayer[nM21]) >> 1;//b
					}
					else             //奇数行奇数列
					{
						pRGB[i * nW * 3 + j * 3 + 2] = pBayer[nM11];//r
						pRGB[i * nW * 3 + j * 3 + 1] = (pBayer[nM01] + pBayer[nM21] + pBayer[nM10] + pBayer[nM12]) >> 2;//g
						pRGB[i * nW * 3 + j * 3 + 0] = (pBayer[nM00] + pBayer[nM02] + pBayer[nM20] + pBayer[nM22]) >> 2;//b
					}
				}
			}
		}


		for (int r = 0; r < height; r++)
		{
			for (int c = 0; c < width; c++)
			{
				dst[3 * (r * width + c) + 0] = pRGB[3 * ((r + nBorder) * (width + 2 * nBorder) + c + nBorder) + 0];
				dst[3 * (r * width + c) + 1] = pRGB[3 * ((r + nBorder) * (width + 2 * nBorder) + c + nBorder) + 1];
				dst[3 * (r * width + c) + 2] = pRGB[3 * ((r + nBorder) * (width + 2 * nBorder) + c + nBorder) + 2];
			}
		}


		free(bayer);
		free(p_rgb);

		return 0;
	}


	int SparrowCamera::undistortColorBrightnessMap(unsigned char* brightness_map)  
	{


		int nr = camera_height_;
		int nc = camera_width_;

		unsigned char* b_map = new unsigned char[nr * nc];
		memset(b_map, 0, sizeof(unsigned char) * nr * nc);

		unsigned char* g_map = new unsigned char[nr * nc];
		memset(g_map, 0, sizeof(unsigned char) * nr * nc);

		unsigned char* r_map = new unsigned char[nr * nc];
		memset(r_map, 0, sizeof(unsigned char) * nr * nc);

		for (int r = 0; r < nr; r++)
		{
			for (int c = 0; c < nc; c++)
			{
				b_map[r * nc + c] = brightness_map[3 * (r * nc + c) + 0];
				g_map[r * nc + c] = brightness_map[3 * (r * nc + c) + 1];
				r_map[r * nc + c] = brightness_map[3 * (r * nc + c) + 2];

			}

		}

		if (DF_SUCCESS != undistortBrightnessMap(b_map))
		{
			delete[] b_map;
			delete[] g_map;
			delete[] r_map;
			return DF_FAILED;
		}

		if (DF_SUCCESS != undistortBrightnessMap(g_map))
		{
			delete[] b_map;
			delete[] g_map;
			delete[] r_map;
			return DF_FAILED;
		}

		if (DF_SUCCESS != undistortBrightnessMap(r_map))
		{
			delete[] b_map;
			delete[] g_map;
			delete[] r_map;
			return DF_FAILED;
		}


		for (int r = 0; r < nr; r++)
		{
			for (int c = 0; c < nc; c++)
			{
				brightness_map[3 * (r * nc + c) + 0] = b_map[r * nc + c];
				brightness_map[3 * (r * nc + c) + 1] = g_map[r * nc + c];
				brightness_map[3 * (r * nc + c) + 2] = r_map[r * nc + c];
			}

		}

		delete[] b_map;
		delete[] g_map;
		delete[] r_map;
		return DF_SUCCESS;
	}



	int SparrowCamera::undistortBrightnessMap(unsigned char* brightness_map) //最近邻
	{

		int nr = camera_height_;
		int nc = camera_width_;

		unsigned char* brightness_map_temp = new unsigned char[nr * nc];
		memset(brightness_map_temp, 0, sizeof(unsigned char) * nr * nc);

		#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{

			for (int c = 0; c < nc; c++)
			{
				int offset = r * camera_width_ + c;
				float distort_x, distort_y;
				//distortPoint(c, r, distort_x, distort_y);
				distort_x = distorted_map_x_[offset];
				distort_y = distorted_map_y_[offset];
				// 进行双线性差值
				if (distort_x > 0 && distort_x < nc - 1 && distort_y > 0 && distort_y < nr - 1)
				{
					float l_t_brightness = brightness_map[(int)distort_y * camera_width_ + (int)distort_x];
					float l_b_brightness = brightness_map[(int)(distort_y + 1) * camera_width_ + (int)distort_x];
					float r_t_brightness = brightness_map[(int)distort_y * camera_width_ + (int)(distort_x + 1)];
					float r_b_brightness = brightness_map[(int)(distort_y + 1) * camera_width_ + (int)(distort_x + 1)];

					float rate_y = distort_y - (int)distort_y;
					float rate_x = distort_x - (int)distort_x;

					brightness_map_temp[offset] = (unsigned char)(l_t_brightness * (1 - rate_x) * (1 - rate_y) + l_b_brightness * (1 - rate_x) * rate_y + r_t_brightness * rate_x * (1 - rate_y) + r_b_brightness * rate_x * rate_y + 0.5);
				}
			}

		}

		memcpy(brightness_map, brightness_map_temp, sizeof(unsigned char) * nr * nc);
		delete[] brightness_map_temp;
		brightness_map_temp = NULL;
		return DF_SUCCESS;
	}


	int SparrowCamera::undistortDepthMap(float* depth_map)
	{
		// 使用双线性差值
		if (!connected_flag_)
		{
			return DF_NOT_CONNECT;
		}

		int nr = camera_height_;
		int nc = camera_width_;

		float* depth_map_temp = new float[nr * nc];
		memset(depth_map_temp, -1, sizeof(float) * nr * nc);

#pragma omp parallel for
		for (int r = 0; r < nr; r++)
		{

			for (int c = 0; c < nc; c++)
			{
				int offset = r * camera_width_ + c;
				float distort_x, distort_y;
				//distortPoint(c, r, distort_x, distort_y);
				distort_x = distorted_map_x_[offset];
				distort_y = distorted_map_y_[offset];
				// 进行双线性差值
				if (distort_x > 0 && distort_x < nc - 1 && distort_y > 0 && distort_y < nr - 1)
				{
					float l_t_depth = depth_map[(int)distort_y * camera_width_ + (int)distort_x];
					float l_b_depth = depth_map[(int)(distort_y + 1) * camera_width_ + (int)distort_x];
					float r_t_depth = depth_map[(int)distort_y * camera_width_ + (int)(distort_x + 1)];
					float r_b_depth = depth_map[(int)(distort_y + 1) * camera_width_ + (int)(distort_x + 1)];

					if (l_t_depth <= 0 || l_b_depth <= 0 || r_t_depth <= 0 || r_b_depth <= 0)
					{
						depth_map_temp[offset] = depth_map[(int)(distort_y + 0.5) * camera_width_ + (int)(distort_x + 0.5)];
						continue;
					}

					float rate_y = distort_y - (int)distort_y;
					float rate_x = distort_x - (int)distort_x;

					depth_map_temp[offset] = l_t_depth * (1 - rate_x) * (1 - rate_y) + l_b_depth * (1 - rate_x) * rate_y + r_t_depth * rate_x * (1 - rate_y) + r_b_depth * rate_x * rate_y;
				}
			}

		}

		memcpy(depth_map, depth_map_temp, sizeof(float) * nr * nc);
		delete[] depth_map_temp;
		depth_map_temp = NULL;
		return DF_SUCCESS;
	}



	int SparrowCamera::getParamRepetitionExposureNum(int& num)
	{
		num = repetition_exposure_model_;

		return DF_SUCCESS;

	}


	int SparrowCamera::getParamMultipleExposureModel(int& model)
	{
		model = multiple_exposure_model_;

		return DF_SUCCESS;

	}


	//函数名： DfGetParamJson
	//功能： 获取Json配置文件
	//输入参数：无
	//输出参数： config_json（配置文件字符）、config_size（配置文本出大小）、
	//status_json（输出设置状态）、status_size（状态文本大小）
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::getParamJson(char* config_json, char* status_json)
	{
		nlohmann::json config_all;

		nlohmann::json status_config;

		nlohmann::json config_firmware;
		nlohmann::json config_sdk;
		//nlohmann::json config_gui;

		/**********************************************************************************************************************/


		if (true)
		{
			SparrowEngine engine;
			int ret = getCaptureEngine(engine);

			status_config["engine"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_sdk["engine"] = engine;
			}
			LOG(INFO) << "get engine:" << status_config["engine"];
		}


		if (true)
		{
			int exposure_model = 0;
			int ret = getParamMultipleExposureModel(exposure_model);

			status_config["exposure_model"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_sdk["exposure_model"] = exposure_model;
			}
			LOG(INFO) << "set exposure_model:" << status_config["exposure_model"];
		}

		if (true)
		{
			int repetition_count = 0;
			int ret = getParamRepetitionExposureNum(repetition_count);

			status_config["repetition_count"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_sdk["repetition_count"] = repetition_count;
			}
			LOG(INFO) << "set repetition_count:" << status_config["repetition_count"];
		}


		if (true)
		{
			char c_version[64];
			int ret = getSdkVersion(c_version);

			std::string version(c_version);

			status_config["version"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_sdk["version"] = "V1.5.3";
			}
			LOG(INFO) << "set version:" << status_config["version"];
		}




		/****************************************************************************************************************************/

		if (true)
		{
			int led = 0;
			int ret = getParamLedCurrent(led);

			status_config["led_current"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["led_current"] = led;
			}
			LOG(INFO) << "get led_current:" << status_config["led_current"];
		}

		/**************************************************************************************/
		if (true)
		{
			float gain = 0;
			int ret = getParamCameraGain(gain);

			status_config["camera_gain"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["camera_gain"] = gain;
			}
			LOG(INFO) << "get camera_gain:" << status_config["camera_gain"];
		}

		/**************************************************************************************/

		if (true)
		{
			float confidence = 0;
			int ret = getParamCameraConfidence(confidence);

			status_config["confidence"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["confidence"] = confidence;
			}
			LOG(INFO) << "get confidence:" << status_config["confidence"];
		}

		/**************************************************************************************/

		if (true)
		{
			float camera_exposure_time = 0;
			int ret = getParamCameraExposure(camera_exposure_time);

			status_config["camera_exposure_time"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["camera_exposure_time"] = camera_exposure_time;
			}
			LOG(INFO) << "get camera_exposure_time:" << status_config["camera_exposure_time"];
		}

		/**************************************************************************************/

		if (true)
		{
			int bilateral_filter_param_d = 0;
			int use_bilateral_filter = 0;
			int ret = getParamBilateralFilter(use_bilateral_filter, bilateral_filter_param_d);

			status_config["use_bilateral_filter"] = ret;
			status_config["bilateral_filter_param_d"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["use_bilateral_filter"] = use_bilateral_filter;
				config_firmware["bilateral_filter_param_d"] = bilateral_filter_param_d;
			}
			LOG(INFO) << "get use_bilateral_filter:" << status_config["use_bilateral_filter"];
			LOG(INFO) << "get bilateral_filter_param_d:" << status_config["bilateral_filter_param_d"];
		}

		/**************************************************************************************/

		if (true)
		{
			float threshold = 0;
			int ret = getParamOutlierFilter(threshold);

			status_config["fisher_confidence"] = ret;

			if (DF_SUCCESS == ret)
			{
				float fisher_confidence = threshold * 2 - 50.0;
				config_firmware["fisher_confidence"] = fisher_confidence;
			}
			LOG(INFO) << "get fisher_confidence:" << status_config["fisher_confidence"];
		}

		/**************************************************************************************/

		if (true)
		{
			int mixed_exposure_num = 0;
			int mixed_exposure_param_list[6];
			int mixed_led_param_list[6];


			int ret = getParamMixedHdr(mixed_exposure_num, mixed_exposure_param_list, mixed_led_param_list);


			status_config["mixed_exposure_num"] = ret;
			status_config["mixed_exposure_param_list"] = ret;
			status_config["mixed_led_param_list"] = ret;

			if (DF_SUCCESS == ret)
			{
				std::vector<int> exposure_list;
				std::vector<int> led_list;

				for (int i = 0; i < 6; i++)
				{
					exposure_list.push_back(mixed_exposure_param_list[i]);
					led_list.push_back(mixed_led_param_list[i]);

				}

				config_firmware["mixed_exposure_num"] = mixed_exposure_num;
				config_firmware["mixed_exposure_param_list"] = exposure_list;
				config_firmware["mixed_led_param_list"] = led_list;
			}
			LOG(INFO) << "get mixed_exposure_num:" << status_config["mixed_exposure_num"];
			LOG(INFO) << "get mixed_exposure_param_list:" << status_config["mixed_exposure_param_list"];
			LOG(INFO) << "get mixed_led_param_list:" << status_config["mixed_led_param_list"];
		}

		/**************************************************************************************/

		if (true)
		{
			float radius_filter_r = 0;
			int use_radius_filter = 0;
			int radius_filter_threshold_num = 0;
			int ret = getParamRadiusFilter(use_radius_filter, radius_filter_r, radius_filter_threshold_num);

			status_config["use_radius_filter"] = ret;
			status_config["radius_filter_r"] = ret;
			status_config["radius_filter_threshold_num"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["use_radius_filter"] = use_radius_filter;
				config_firmware["radius_filter_r"] = radius_filter_r;
				config_firmware["radius_filter_threshold_num"] = radius_filter_threshold_num;
			}
			LOG(INFO) << "get use_radius_filter:" << status_config["use_radius_filter"];
			LOG(INFO) << "get radius_filter_r:" << status_config["radius_filter_r"];
			LOG(INFO) << "get radius_filter_threshold_num:" << status_config["radius_filter_threshold_num"];
		}

		/**************************************************************************************/


		if (true)
		{
			float depth_filter_threshold = 0;
			int use_depth_filter = 0;
			int ret = getParamDepthFilter(use_depth_filter, depth_filter_threshold);

			status_config["use_depth_filter"] = ret;
			status_config["depth_filter_threshold"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["use_depth_filter"] = use_depth_filter;
				config_firmware["depth_filter_threshold"] = depth_filter_threshold;
			}
			LOG(INFO) << "get use_depth_filter:" << status_config["use_depth_filter"];
			LOG(INFO) << "get depth_filter_threshold:" << status_config["depth_filter_threshold"];
		}

		/**************************************************************************************/

		if (true)
		{
			float gray_rectify_sigma = 0;
			int use_gray_rectify = 0;
			int gray_rectify_r = 0;
			int ret = getParamGrayRectify(use_gray_rectify, gray_rectify_r, gray_rectify_sigma);

			status_config["use_gray_rectify"] = ret;
			status_config["gray_rectify_r"] = ret;
			status_config["gray_rectify_sigma"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["use_gray_rectify"] = use_gray_rectify;
				config_firmware["gray_rectify_r"] = gray_rectify_r;
				config_firmware["gray_rectify_sigma"] = gray_rectify_sigma;
			}
			LOG(INFO) << "get use_gray_rectify:" << status_config["use_gray_rectify"];
			LOG(INFO) << "get gray_rectify_r:" << status_config["gray_rectify_r"];
			LOG(INFO) << "get gray_rectify_sigma:" << status_config["gray_rectify_sigma"];
		}

		/**************************************************************************************/


		if (true)
		{
			float generate_brightness_exposure = 0;
			int generate_brightness_model = 0;
			int ret = getParamGenerateBrightness(generate_brightness_model, generate_brightness_exposure);

			status_config["generate_brightness_model"] = ret;
			status_config["generate_brightness_exposure"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["generate_brightness_model"] = generate_brightness_model;
				config_firmware["generate_brightness_exposure"] = generate_brightness_exposure;
			}
			LOG(INFO) << "get generate_brightness_model:" << status_config["generate_brightness_model"];
			LOG(INFO) << "get generate_brightness_exposure:" << status_config["generate_brightness_exposure"];
		}

		/**************************************************************************************/
		if (true)
		{
			float brightness_gain = 0;
			int ret = getParamBrightnessGain(brightness_gain);

			status_config["brightness_gain"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["brightness_gain"] = brightness_gain;
			}
			LOG(INFO) << "get brightness_gain:" << status_config["brightness_gain"];
		}

		/**************************************************************************************/

		if (true)
		{
			int brightness_hdr_exposure_num = 0;
			int brightness_hdr_exposure_param_list[10];

			int ret = getParamBrightnessHdrExposure(brightness_hdr_exposure_num, brightness_hdr_exposure_param_list);


			status_config["brightness_hdr_exposure_num"] = ret;
			status_config["brightness_hdr_exposure_param_list"] = ret;

			if (DF_SUCCESS == ret)
			{
				std::vector<int> exposure_list;

				for (int i = 0; i < 10; i++)
				{
					exposure_list.push_back(brightness_hdr_exposure_param_list[i]);

				}

				config_firmware["brightness_hdr_exposure_num"] = brightness_hdr_exposure_num;
				config_firmware["brightness_hdr_exposure_param_list"] = exposure_list;
			}
			LOG(INFO) << "get brightness_hdr_exposure_num:" << status_config["brightness_hdr_exposure_num"];
			LOG(INFO) << "get brightness_hdr_exposure_param_list:" << status_config["brightness_hdr_exposure_param_list"];
		}

		/**************************************************************************************/
		if (true)
		{
			int generate_brightness_exposure_model = 0;
			int ret = getParamBrightnessExposureModel(generate_brightness_exposure_model);

			status_config["generate_brightness_exposure_model"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["generate_brightness_exposure_model"] = generate_brightness_exposure_model;
			}
			LOG(INFO) << "get generate_brightness_exposure_model:" << status_config["generate_brightness_exposure_model"];
		}

		/**************************************************************************************/

		if (true)
		{
			float reflect_filter_b = 0;
			int use_reflect_filter = 0;
			int ret = getParamReflectFilter(use_reflect_filter, reflect_filter_b);

			status_config["use_reflect_filter"] = ret;
			status_config["reflect_filter_b"] = ret;

			if (DF_SUCCESS == ret)
			{
				config_firmware["use_reflect_filter"] = use_reflect_filter;
				config_firmware["reflect_filter_b"] = reflect_filter_b;
			}
			LOG(INFO) << "get use_reflect_filter:" << status_config["use_reflect_filter"];
			LOG(INFO) << "get reflect_filter_b:" << status_config["reflect_filter_b"];
		}

		/**************************************************************************************/


		if (true)
		{
			float standard_plane_external_param[12];

			int ret = getParamStandardPlaneExternal(&standard_plane_external_param[0], &standard_plane_external_param[9]);


			status_config["standard_plane_external_param"] = ret;

			if (DF_SUCCESS == ret)
			{
				std::vector<float> param_list;

				for (int i = 0; i < 12; i++)
				{
					param_list.push_back(standard_plane_external_param[i]);

				}

				config_firmware["standard_plane_external_param"] = param_list;
			}
			LOG(INFO) << "get standard_plane_external_param:" << status_config["standard_plane_external_param"];
		}

		/**************************************************************************************/


	/****************************************************************************************************************************/


		config_all["firmware"] = config_firmware;
		config_all["sdk"] = config_sdk;

		std::cout << status_config << std::endl;
		std::cout << config_all << std::endl;


		try
		{
			std::string return_config_json = config_all.dump();
			std::memcpy(config_json, (char*)return_config_json.data(), return_config_json.size());
			config_json[return_config_json.size()] = '\0';

			std::string return_status_json = status_config.dump();
			std::memcpy(status_json, (char*)return_status_json.data(), return_status_json.size());
			status_json[return_status_json.size()] = '\0';
		}
		catch (const std::exception&)
		{
			LOG(ERROR) << "json error";
			return DF_FAILED;
		}




		return DF_SUCCESS;
	}


	//函数名： DfSetParamJson
	//功能： 设置Json配置文件里的
	//输入参数：in_config_json（输入配置文件字符）、in_size（输入文本出大小）、maxnum(曝光数，填入DfCaptureData接口)
	// out_status_json（输出设置状态）、out_size（输出文本大小）
	//输出参数： 无
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::setParamJson(char* config_json, char* status_json, int& maxnum)
	{

		nlohmann::json status_config;
		nlohmann::json config_all = nlohmann::json::parse(config_json);

		nlohmann::json config_firmware;
		nlohmann::json config_gui = config_all["gui"];
		nlohmann::json config_sdk = config_all["sdk"];
		int true_model = 0;
		if (!config_all["sdk"].is_null())
		{
			config_sdk = config_all["sdk"];


			if (!config_sdk["engine"].is_null())
			{
				int engine = config_sdk["engine"].get<int>();

				int ret = setCaptureEngine(SparrowEngine(engine));
				status_config["engine"] = ret;
			}
			else
			{
				status_config["engine"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set engine:" << status_config["engine"];

			if (!config_sdk["exposure_model"].is_null())
			{
				int exposure_model = config_sdk["exposure_model"].get<int>();

				if (exposure_model == 0) {
					maxnum = 1;
				}
				else
				{
					true_model = exposure_model;
				}

				int ret = setParamMultipleExposureModel(exposure_model);
				status_config["exposure_model"] = ret;
			}
			else
			{
				status_config["exposure_model"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set exposure_model:" << status_config["exposure_model"];


			if (!config_sdk["repetition_count"].is_null())
			{
				int repetition_count = config_sdk["repetition_count"].get<int>();
				if (true_model == 2) {
					maxnum = repetition_count;
				}
				int ret = setParamRepetitionExposureNum(repetition_count);
				status_config["repetition_count"] = ret;
			}
			else
			{
				status_config["repetition_count"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set repetition_count:" << status_config["repetition_count"];

			if (!config_sdk["version"].is_null())
			{
				std::string version = config_sdk["version"].get<string>();

				LOG(INFO) << "version: " << version;
				status_config["version"] = DF_SUCCESS;
			}
			else
			{
				status_config["version"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set version:" << status_config["version"];

		}
		else
		{
			status_config["engine"] = DF_ERROR_LOST_PARAM;
			status_config["exposure_model"] = DF_ERROR_LOST_PARAM;
			status_config["repetition_count"] = DF_ERROR_LOST_PARAM;
		}


		if (!config_all["firmware"].is_null())
		{
			config_firmware = config_all["firmware"];

			std::string feature_name = "";

			feature_name = "led_current";
			if (!config_firmware[feature_name].is_null())
			{
				int led = config_firmware[feature_name].get<int>();

				int ret =setParamLedCurrent(led);
				status_config[feature_name] = ret;
			}
			else
			{
				status_config[feature_name] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set led_current:" << status_config[feature_name];
			/**************************************************************************************/

			feature_name = "camera_gain";
			if (!config_firmware[feature_name].is_null())
			{
				float gain = config_firmware[feature_name].get<float>();

				int ret = setParamCameraGain(gain);
				status_config[feature_name] = ret;
			}
			else
			{
				status_config[feature_name] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set camera_gain:" << status_config[feature_name];
			/**************************************************************************************/
			feature_name = "confidence";
			if (!config_firmware[feature_name].is_null())
			{
				float gain = config_firmware[feature_name].get<float>();

				int ret = setParamCameraConfidence(gain);
				status_config[feature_name] = ret;
			}
			else
			{
				status_config[feature_name] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set confidence:" << status_config[feature_name];
			/**************************************************************************************/

			feature_name = "camera_exposure_time";
			if (!config_firmware[feature_name].is_null())
			{
				int exposure = config_firmware[feature_name].get<int>();

				int ret = setParamCameraExposure(exposure);
				status_config[feature_name] = ret;
			}
			else
			{
				status_config[feature_name] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set camera_exposure_time:" << status_config[feature_name];
			/**************************************************************************************/

			if (!config_firmware["use_bilateral_filter"].is_null() && !config_firmware["bilateral_filter_param_d"].is_null())
			{
				int d = config_firmware["bilateral_filter_param_d"].get<int>();
				int use = config_firmware["use_bilateral_filter"].get<int>();
				int smooth = 0;
				if (0 == use)
				{
					smooth = 0;
				}
				else
				{
					smooth = d / 2;
				}

				int ret = setParamSmoothing(smooth);
				status_config["use_bilateral_filter"] = ret;
				status_config["bilateral_filter_param_d"] = ret;
			}
			else
			{
				status_config["use_bilateral_filter"] = DF_ERROR_LOST_PARAM;
				status_config["bilateral_filter_param_d"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set use_bilateral_filter:" << status_config["use_bilateral_filter"];
			LOG(INFO) << "set bilateral_filter_param_d:" << status_config["bilateral_filter_param_d"];
			/**************************************************************************************/

			feature_name = "fisher_confidence";
			if (!config_firmware[feature_name].is_null())
			{
				float fisher = config_firmware[feature_name].get<float>();
				float threshold = (50 + fisher) / 2.;

				int ret = setParamOutlierFilter(threshold);
				status_config[feature_name] = ret;
			}
			else
			{
				status_config[feature_name] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set fisher_confidence:" << status_config[feature_name];
			/**************************************************************************************/

			if (!config_firmware["mixed_exposure_num"].is_null() && !config_firmware["mixed_exposure_param_list"].is_null() && !config_firmware["mixed_led_param_list"].is_null())
			{

				int num = config_firmware["mixed_exposure_num"].get<int>();

				if (true_model == 1) {
					maxnum = num;
				}
				std::vector<int> exposure_list = config_firmware["mixed_exposure_param_list"].get<std::vector<int>>();
				std::vector<int> led_list = config_firmware["mixed_led_param_list"].get<std::vector<int>>();

				if (6 == exposure_list.size() && 6 == led_list.size() && 2 <= num && num <= 6)
				{
					int exposures[6];
					int leds[6];

					for (int i = 0; i < 6; i++)
					{
						exposures[i] = exposure_list[i];
						leds[i] = led_list[i];
					}
					int ret = setParamMixedHdr(num, exposures, leds);
					status_config["mixed_exposure_num"] = ret;
					status_config["mixed_exposure_param_list"] = ret;
					status_config["mixed_led_param_list"] = ret;
				}
				else
				{
					status_config["mixed_exposure_num"] = DF_ERROR_INVALID_PARAM;
					status_config["mixed_exposure_param_list"] = DF_ERROR_INVALID_PARAM;
					status_config["mixed_led_param_list"] = DF_ERROR_INVALID_PARAM;
				}


			}
			else
			{
				status_config["mixed_exposure_num"] = DF_ERROR_LOST_PARAM;
				status_config["mixed_exposure_param_list"] = DF_ERROR_LOST_PARAM;
				status_config["mixed_led_param_list"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set mixed_exposure_num:" << status_config["mixed_exposure_num"];
			LOG(INFO) << "set mixed_exposure_param_list:" << status_config["mixed_exposure_param_list"];
			LOG(INFO) << "set mixed_led_param_list:" << status_config["mixed_led_param_list"];

			/**************************************************************************************/

			if (!config_firmware["use_radius_filter"].is_null() && !config_firmware["radius_filter_r"].is_null() && !config_firmware["radius_filter_threshold_num"].is_null())
			{

				float r = config_firmware["radius_filter_r"].get<float>();
				int num = config_firmware["radius_filter_threshold_num"].get<int>();
				int use = config_firmware["use_radius_filter"].get<int>();



				int ret = setParamRadiusFilter(use, r, num);
				status_config["use_radius_filter"] = ret;
				status_config["radius_filter_r"] = ret;
				status_config["radius_filter_threshold_num"] = ret;
			}
			else
			{
				status_config["use_radius_filter"] = DF_ERROR_LOST_PARAM;
				status_config["radius_filter_r"] = DF_ERROR_LOST_PARAM;
				status_config["radius_filter_threshold_num"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set use_radius_filter:" << status_config["use_bilateral_filter"];
			LOG(INFO) << "set radius_filter_r:" << status_config["radius_filter_r"];
			LOG(INFO) << "set radius_filter_threshold_num:" << status_config["radius_filter_threshold_num"];
			/**************************************************************************************/

			if (!config_firmware["use_depth_filter"].is_null() && !config_firmware["depth_filter_threshold"].is_null())
			{
				float threshold = config_firmware["depth_filter_threshold"].get<float>();
				int use = config_firmware["use_depth_filter"].get<int>();


				int ret = setParamDepthFilter(use, threshold);
				status_config["use_depth_filter"] = ret;
				status_config["depth_filter_threshold"] = ret;
			}
			else
			{
				status_config["use_depth_filter"] = DF_ERROR_LOST_PARAM;
				status_config["depth_filter_threshold"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set use_depth_filter:" << status_config["use_depth_filter"];
			LOG(INFO) << "set depth_filter_threshold:" << status_config["depth_filter_threshold"];
			/**************************************************************************************/

			if (!config_firmware["use_gray_rectify"].is_null() && !config_firmware["gray_rectify_r"].is_null() && !config_firmware["gray_rectify_sigma"].is_null())
			{

				int r = config_firmware["gray_rectify_r"].get<int>();
				float sigma = config_firmware["gray_rectify_sigma"].get<float>();
				int use = config_firmware["use_gray_rectify"].get<int>();



				int ret = setParamGrayRectify(use, r, sigma);
				status_config["use_gray_rectify"] = ret;
				status_config["gray_rectify_r"] = ret;
				status_config["gray_rectify_sigma"] = ret;
			}
			else
			{
				status_config["use_gray_rectify"] = DF_ERROR_LOST_PARAM;
				status_config["gray_rectify_r"] = DF_ERROR_LOST_PARAM;
				status_config["gray_rectify_sigma"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set use_gray_rectify:" << status_config["use_gray_rectify"];
			LOG(INFO) << "set gray_rectify_r:" << status_config["gray_rectify_r"];
			LOG(INFO) << "set gray_rectify_sigma:" << status_config["gray_rectify_sigma"];

			/**************************************************************************************/

			if (!config_firmware["generate_brightness_model"].is_null() && !config_firmware["generate_brightness_exposure"].is_null())
			{
				int model = config_firmware["generate_brightness_model"].get<int>();
				int exposure = config_firmware["generate_brightness_exposure"].get<int>();


				int ret = setParamGenerateBrightness(model, exposure);
				status_config["generate_brightness_exposure"] = ret;
				status_config["generate_brightness_model"] = ret;
			}
			else
			{
				status_config["generate_brightness_exposure"] = DF_ERROR_LOST_PARAM;
				status_config["generate_brightness_model"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set generate_brightness_exposure:" << status_config["generate_brightness_exposure"];
			LOG(INFO) << "set generate_brightness_model:" << status_config["generate_brightness_model"];
			/**************************************************************************************/
			feature_name = "brightness_gain";
			if (!config_firmware[feature_name].is_null())
			{
				float gain = config_firmware[feature_name].get<float>();

				int ret = setParamBrightnessGain(gain);
				status_config[feature_name] = ret;
			}
			else
			{
				status_config[feature_name] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set brightness_gain:" << status_config[feature_name];
			/**************************************************************************************/


			if (!config_firmware["brightness_hdr_exposure_num"].is_null() && !config_firmware["brightness_hdr_exposure_param_list"].is_null())
			{

				int num = config_firmware["brightness_hdr_exposure_num"].get<int>();
				std::vector<int> exposure_list = config_firmware["brightness_hdr_exposure_param_list"].get<std::vector<int>>();

				if (10 == exposure_list.size() && 2 <= num && num <= 10)
				{
					int exposures[10];

					for (int i = 0; i < 10; i++)
					{
						exposures[i] = exposure_list[i];
					}
					int ret = setParamBrightnessHdrExposure(num, exposures);
					status_config["brightness_hdr_exposure_num"] = ret;
					status_config["brightness_hdr_exposure_param_list"] = ret;
				}
				else
				{
					status_config["brightness_hdr_exposure_num"] = DF_ERROR_INVALID_PARAM;
					status_config["brightness_hdr_exposure_param_list"] = DF_ERROR_INVALID_PARAM;
				}

			}
			else
			{
				status_config["brightness_hdr_exposure_num"] = DF_ERROR_LOST_PARAM;
				status_config["brightness_hdr_exposure_param_list"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set brightness_hdr_exposure_num:" << status_config["brightness_hdr_exposure_num"];
			LOG(INFO) << "set brightness_hdr_exposure_param_list:" << status_config["brightness_hdr_exposure_param_list"];

			/**************************************************************************************/

			feature_name = "generate_brightness_exposure_model";
			if (!config_firmware[feature_name].is_null())
			{
				int model = config_firmware[feature_name].get<int>();

				int ret = setParamBrightnessExposureModel(model);
				status_config[feature_name] = ret;
			}
			else
			{
				status_config[feature_name] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set generate_brightness_exposure_model:" << status_config[feature_name];


			/**************************************************************************************/

			if (!config_firmware["use_reflect_filter"].is_null() && !config_firmware["reflect_filter_b"].is_null())
			{
				float threshold = config_firmware["reflect_filter_b"].get<float>();
				int use = config_firmware["use_reflect_filter"].get<int>();


				int ret = setParamReflectFilter(use, threshold);
				status_config["use_reflect_filter"] = ret;
				status_config["reflect_filter_b"] = ret;
			}
			else
			{
				status_config["use_reflect_filter"] = DF_ERROR_LOST_PARAM;
				status_config["reflect_filter_b"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set use_reflect_filter:" << status_config["use_reflect_filter"];
			LOG(INFO) << "set reflect_filter_b:" << status_config["reflect_filter_b"];
			/**************************************************************************************/

			if (!config_firmware["standard_plane_external_param"].is_null())
			{

				std::vector<float> param_list = config_firmware["standard_plane_external_param"].get<std::vector<float>>();

				if (12 == param_list.size())
				{
					float param[12];

					for (int i = 0; i < 12; i++)
					{
						param[i] = param_list[i];
					}



					int ret = setParamStandardPlaneExternal(&param[0], &param[9]);
					status_config["standard_plane_external_param"] = ret;
				}
				else
				{
					status_config["standard_plane_external_param"] = DF_ERROR_INVALID_PARAM;
				}

			}
			else
			{
				status_config["standard_plane_external_param"] = DF_ERROR_LOST_PARAM;
			}
			LOG(INFO) << "set standard_plane_external_param:" << status_config["standard_plane_external_param"];

			/**************************************************************************************/
		}
		else
		{
			status_config["led_current"] = DF_ERROR_LOST_PARAM;
			status_config["camera_gain"] = DF_ERROR_LOST_PARAM;
			status_config["confidence"] = DF_ERROR_LOST_PARAM;
			status_config["camera_exposure_time"] = DF_ERROR_LOST_PARAM;
			status_config["use_bilateral_filter"] = DF_ERROR_LOST_PARAM;
			status_config["bilateral_filter_param_d"] = DF_ERROR_LOST_PARAM;
			status_config["fisher_confidence"] = DF_ERROR_LOST_PARAM;
			status_config["mixed_exposure_num"] = DF_ERROR_LOST_PARAM;
			status_config["mixed_exposure_param_list"] = DF_ERROR_LOST_PARAM;
			status_config["mixed_led_param_list"] = DF_ERROR_LOST_PARAM;
			status_config["use_radius_filter"] = DF_ERROR_LOST_PARAM;
			status_config["radius_filter_r"] = DF_ERROR_LOST_PARAM;
			status_config["radius_filter_threshold_num"] = DF_ERROR_LOST_PARAM;
			status_config["use_depth_filter"] = DF_ERROR_LOST_PARAM;
			status_config["depth_filter_threshold"] = DF_ERROR_LOST_PARAM;
			status_config["use_gray_rectify"] = DF_ERROR_LOST_PARAM;
			status_config["gray_rectify_r"] = DF_ERROR_LOST_PARAM;
			status_config["gray_rectify_sigma"] = DF_ERROR_LOST_PARAM;
			status_config["generate_brightness_exposure"] = DF_ERROR_LOST_PARAM;
			status_config["generate_brightness_model"] = DF_ERROR_LOST_PARAM;
			status_config["brightness_gain"] = DF_ERROR_LOST_PARAM;
			status_config["brightness_hdr_exposure_num"] = DF_ERROR_INVALID_PARAM;
			status_config["brightness_hdr_exposure_param_list"] = DF_ERROR_INVALID_PARAM;
			status_config["generate_brightness_exposure_model"] = DF_ERROR_INVALID_PARAM;
			status_config["use_reflect_filter"] = DF_ERROR_LOST_PARAM;
			status_config["reflect_filter_b"] = DF_ERROR_LOST_PARAM;
			status_config["standard_plane_external_param"] = DF_ERROR_LOST_PARAM;

		}

		if (!config_all["gui"].is_null())
		{
			config_gui = config_all["gui"];
		}


		//std::cout << config_firmware <<std::endl;
		//std::cout << config_gui << std::endl;
		//std::cout << config_sdk << std::endl;



		std::string return_json = status_config.dump();
		std::memcpy(status_json, (char*)return_json.data(), return_json.size() + 1);

		return DF_SUCCESS;
	}

	//函数名： DfSaveJson
	//功能： 保存Json配置文件
	//输入参数：config_json（配置文件字符）、config_size（配置文本大小）、
	//输出参数： 无 
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::saveJson(const char* config_json, const char* path)
	{
		try
		{
			nlohmann::json json = nlohmann::json::parse(config_json);

			std::ofstream file(path);
			if (file.is_open()) {
				file << std::setw(4) << json << std::endl;
				file.close();
				LOG(INFO) << "JSON data has been written to file: " << config_json;
			}
			else {
				LOG(ERROR) << "Failed to open file for writing.";
			}


			return DF_SUCCESS;
		}
		catch (const std::exception&)
		{

			LOG(ERROR) << "Failed to parse json.";

			return DF_FAILED;
		}


	}

	//函数名： DfReadJson
	//功能： 读取Json配置文件
	//输入参数：config_json（配置文件字符）、config_size（配置文本大小）、
	//输出参数： 无 
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::readJson(char* config_json, const char* path)
	{
		std::ifstream ifile;
		ifile.open(path);
		if (!ifile.is_open())
		{
			std::cout << "读取文件失败" << std::endl;
			return DF_FAILED;
		}

		std::string json_str;
		std::string buf;
		while (std::getline(ifile, buf))
		{
			json_str += buf;
			json_str += '\n';
			//std::cout << buf << std::endl;
		}
		ifile.close();


		try
		{
			nlohmann::json json = nlohmann::json::parse(json_str);
			std::string return_json = json.dump();
			std::memcpy(config_json, (char*)return_json.data(), return_json.size());
			config_json[return_json.size()] = '\0';
			std::cout << return_json << std::endl;
		}
		catch (const std::exception&)
		{
			LOG(ERROR) << "Failed to parse json.";
			return DF_FAILED;
		}

		return DF_SUCCESS;
	}


	inline void write_fbin(std::ofstream& out, float val) {
		out.write(reinterpret_cast<char*>(&val), sizeof(float));
	}

	inline void write_fbin(std::ofstream& out, unsigned char val) {
		out.write(reinterpret_cast<char*>(&val), sizeof(unsigned char));
	}


	//函数名： savePointcloudToPcd
	//功能： 保存pcd点云
	//输入参数：pointcloud(点云)、brightness（亮度图）、channels（亮度图通道数）、path(路径)
	//输出参数：gain(亮度图增益)
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::savePointcloudToPcd(float* pointcloud, unsigned char* brightness, int channels, const char* path) {
		std::ofstream file(path, std::ios::out | std::ios::binary);
		if (!file.is_open()) {
			std::cout << "Save points Error";
			return -1;
		}

		int pointcloud_number = camera_height_ * camera_width_;

		if (channels == 1) {
			// 写入PCD文件头  
			file << "VERSION .7\n";
			file << "FIELDS x y z rgb\n";
			file << "SIZE 4 4 4 4\n";
			file << "TYPE F F F U\n";
			file << "COUNT 1 1 1 1\n";
			file << "WIDTH " << pointcloud_number << "\n";
			file << "HEIGHT 1\n";
			file << "VIEWPOINT 0 0 0 1 0 0 0\n";
			file << "POINTS " << pointcloud_number << "\n";
			file << "DATA binary\n"; // 指定为二进制数据  
			file.close(); // 关闭文件，准备写入二进制数据  
		}
		else
		{
			file << "VERSION .7\n";
			file << "FIELDS x y z";
			file << " rgb";
			file << "\n";
			file << "SIZE 4 4 4";
			file << " 4";
			file << "\n";
			file << "TYPE F F F";
			file << " U";
			file << "\n";
			file << "COUNT 1 1 1";
			file << " 1";
			file << "\n";
			file << "WIDTH " << pointcloud_number << "\n";
			file << "HEIGHT 1\n";
			file << "VIEWPOINT 0 0 0 1 0 0 0\n";
			file << "POINTS " << pointcloud_number << "\n";
			file << "DATA binary\n";
			file.close();
		}

		std::ofstream outFile(path, std::ios::app | std::ios::binary);

		for (int r = 0; r < camera_height_; ++r) {
			for (int c = 0; c < camera_width_; ++c) {
				int offset = r * camera_width_ + c;

				write_fbin(outFile, pointcloud[offset * 3 + 0]);
				write_fbin(outFile, pointcloud[offset * 3 + 1]);
				write_fbin(outFile, pointcloud[offset * 3 + 2]);
				if (channels == 1) {

					uint32_t gray = static_cast<uint32_t>(brightness[offset]);
					uint32_t rgb = (gray << 16) | (gray << 8) | gray;
					float rgb_float;
					memcpy(&rgb_float, &rgb, sizeof(float));
					write_fbin(outFile, rgb_float);
				}
				else {

					uint32_t rgb = (static_cast<uint32_t>(brightness[offset * 3 + 0]) << 16) |
						(static_cast<uint32_t>(brightness[offset * 3 + 1]) << 8) |
						static_cast<uint32_t>(brightness[offset * 3 + 2]);
					float rgb_float;
					memcpy(&rgb_float, &rgb, sizeof(float));
					write_fbin(outFile, rgb_float);
				}
			}
		}
		outFile.close();
		return 0;
	}

	//函数名： savePointcloudToPly
	//功能： 保存ply点云
	//输入参数：pointcloud(点云)、brightness（亮度图）、channels（亮度图通道数）、path(路径)
	//输出参数：gain(亮度图增益)
	//返回值： 类型（int）:返回0表示设置参数成功;否则失败。
	int SparrowCamera::savePointcloudToPly(float* pointcloud, unsigned char* brightness, int channels, const char* path) {
		std::ofstream file;
		file.open(path);
		if (!file.is_open())
		{
			std::cout << "Save points Error";
			return false;
		}
		else
		{

			int pointcloud_number = camera_height_ * camera_width_;

			if (0 != channels)
			{

				if (1 == channels)
				{

					/****************************************************************************************************/

					//Header 
					file << "ply" << "\n";
					file << "format binary_little_endian 1.0" << "\n";
					file << "element vertex " << pointcloud_number << "\n";
					file << "property float x" << "\n";
					file << "property float y" << "\n";
					file << "property float z" << "\n";
					file << "property uchar red" << "\n";
					file << "property uchar green" << "\n";
					file << "property uchar blue" << "\n";
					file << "end_header" << "\n";

					file.close();


					/*********************************************************************************************************/
					//以二进行制保存
					std::ofstream outFile(path, std::ios::app | std::ios::binary);

					for (int r = 0; r < camera_height_; r++)
					{

						for (int c = 0; c < camera_width_; c++)
						{

							int offset = r * camera_width_ + c;


							write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 0]));
							write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 1]));
							write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 2]));
							write_fbin(outFile, static_cast<unsigned char>(brightness[offset]));
							write_fbin(outFile, static_cast<unsigned char>(brightness[offset]));
							write_fbin(outFile, static_cast<unsigned char>(brightness[offset]));
						}

					}


					outFile.close();


					/*****************************************************************************************************/
				}
				else if (3 == channels)
				{
					/****************************************************************************************************/

					//Header 
					file << "ply" << "\n";
					file << "format binary_little_endian 1.0" << "\n";
					file << "element vertex " << pointcloud_number << "\n";
					file << "property float x" << "\n";
					file << "property float y" << "\n";
					file << "property float z" << "\n";
					file << "property uchar red" << "\n";
					file << "property uchar green" << "\n";
					file << "property uchar blue" << "\n";
					file << "end_header" << "\n";

					file.close();


					/*********************************************************************************************************/
					//以二进行制保存
					std::ofstream outFile(path, std::ios::app | std::ios::binary);

					for (int r = 0; r < camera_height_; r++)
					{

						for (int c = 0; c < camera_width_; c++)
						{

							int offset = r * camera_width_ + c;


							write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 0]));
							write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 1]));
							write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 2]));
							write_fbin(outFile, static_cast<unsigned char>(brightness[offset * 3 + 0]));
							write_fbin(outFile, static_cast<unsigned char>(brightness[offset * 3 + 1]));
							write_fbin(outFile, static_cast<unsigned char>(brightness[offset * 3 + 2]));
						}

					}


					outFile.close();

					/*****************************************************************************************************/
				}


				/**********************************************************************************************************/

			}
			else
			{

				//Header 
				file << "ply" << "\n";
				file << "format binary_little_endian 1.0" << "\n";
				file << "element vertex " << pointcloud_number << "\n";
				file << "property float x" << "\n";
				file << "property float y" << "\n";
				file << "property float z" << "\n";
				file << "end_header" << "\n";

				file.close();


				/*********************************************************************************************************/
				//以二进行制保存
				std::ofstream outFile(path, std::ios::app | std::ios::binary);

				for (int r = 0; r < camera_height_; r++)
				{

					for (int c = 0; c < camera_width_; c++)
					{

						int offset = r * camera_width_ + c;

						write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 0]));
						write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 1]));
						write_fbin(outFile, static_cast<float>(pointcloud[offset * 3 + 2]));
					}

				}


				outFile.close();

				/**********************************************************************************************************/
			}

		}


		return 0;
	}

	/*********************************************************************************************************/

	/************************************************************************/
	void* createSCamera() {
		return new SparrowCamera;
	}

	void destroySCamera(void* pCamera) {
		delete reinterpret_cast<SparrowCamera*>(pCamera);
	}

}
