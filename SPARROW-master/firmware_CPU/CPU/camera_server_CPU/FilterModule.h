#pragma once
//#include <opencv2/core.hpp>
//#include <opencv2/highgui.hpp> 
//#include <opencv2/imgproc/imgproc.hpp>
#include<opencv2/opencv.hpp>
class FilterModule
{
public:
	FilterModule();
	~FilterModule();

	bool RadiusOutlierRemoval(cv::Mat& point_cloud_map, cv::Mat& mask, double dot_spacing, double radius, int points_num);

	bool RadiusOutlierRemoval_cpu(int height,int width,cv::Mat& point_cloud_map,cv::Mat& depth, cv::Mat& mask, double dot_spacing, double radius, int points_num);

	bool statisticOutlierRemoval(cv::Mat& point_cloud_map, int num_neighbors, double threshold_radio);
	bool statisticOutlierRemoval_cpu(int height,int width,cv::Mat& point_cloud_map,cv::Mat& depth, int num_neighbors, double threshold_radio);
	bool monotonicityFilter_cpu(cv::Mat& unwrapMap, cv::Mat& maskOutput, float monotonicityThresholdVal, float monotonicityFilterVal);

	void depth_filter_step_1(cv::Mat& depth_map, cv::Mat& depth_map_temp, cv::Mat& mask_temp, float depth_threshold);
	void depth_filter_step_2(cv::Mat& depth_map, const cv::Mat& depth_map_temp, cv::Mat& mask_temp, float depth_threshold);


	//bool removeReflectNoise(cv::Mat unwrap_map, cv::Mat confidence_map, cv::Mat& mask);
private:

	double computePointsDistance(cv::Vec3d p0, cv::Vec3d p1);

	double computePointsDistance(cv::Point3d p0, cv::Point3d p1);

	double computePointsZDistance(cv::Point3d p0, cv::Point3d p1);

	double computePointsDistance(cv::Point3f p0, cv::Point3f p1);

	double computePointsDistance(cv::Point2f p0, cv::Point2f p1);

	int findNearPointsnum(cv::Mat& point_cloud_map, cv::Mat& mask, cv::Point pos, double dot_spacing, double radius);

	bool statisticRegion(cv::Mat& point_cloud_map, cv::Point pos, int num_neighbors, double threshold_radio);

	bool computeNormalDistribution(cv::Mat& point_cloud_map, int num_neighbors, double& mean, double& sd);

	bool getDistanctList(cv::Mat point_cloud_map, cv::Point pos, int num_neighbors, double& mean, std::vector<double>& dist_list);

};

