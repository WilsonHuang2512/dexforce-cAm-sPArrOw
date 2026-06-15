#pragma once

void project_version();
bool calibrate_stereo(std::string patterns_path, std::string calib_path);
bool correct_stereo(std::string patterns_path, std::string in_path, std::string out_path);
bool correct_stereo_color(std::string patterns_path, std::string in_path, std::string out_path);
bool calibrate_stereo_base_phase(std::string phase_path, std::string calib_path);
bool calibrate_stereo_color(std::string patterns_path, std::string calib_path);

bool correct_stereo_monO(int board_size, std::string patterns_path, std::string in_path, std::string out_path, float* c_in, float* c_dis, float* p_in, float* p_dis, float* r, float* t, double& error);

bool correct_stereo_coloR(int board_size, std::string patterns_path, std::string in_path, std::string out_path, float* c_in, float* c_dis, float* p_in, float* p_dis, float* r, float* t, double& error);
