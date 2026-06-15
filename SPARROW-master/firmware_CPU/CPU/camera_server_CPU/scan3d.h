#pragma once
#include "LookupTableFunction.h"
#include "camera_galaxy.h"
#include "camera_param.h"
#include "vector"
#include "system_config_settings.h"
#include "lightcrafter3010.h"
#include <mutex>

const int  GENERATE_BRIGHTNESS_MODEL_SINGLE_ = 1;
const int  GENERATE_BRIGHTNESS_MODEL_HDR_ = 2;

class Scan3D
{
public:
	Scan3D();
	~Scan3D();

	int init();

    bool cameraIsValid();

    bool triggerLineIsValid();

    bool setParamHdr(int num,std::vector<int> led_list,std::vector<int> exposure_list);
    /**
     * @brief [26-03-20] 重启恢复HDR参数
     * @return true: 成功; false: 失败
     */
    bool recoverParamHdr();

    bool setParamExposure(float exposure);
    /**
     * @brief [26-03-20] 重启恢复曝光参数
     * @return true: 成功; false: 失败
     */
    bool recoverParamExposure();

    bool setParamGain(float gain);
    /**
     * @brief [26-03-21] 重启恢复增益参数
     * @return true: 成功; false: 失败
     */
    bool recoverParamGain();

    bool setParamLedCurrent(int current);
    /**
     * @brief [26-03-21] 重启恢复LED电流参数
     * @return true: 成功; false: 失败
     */
    bool recoverParamLedCurrent();

    bool setParamConfidence(float confidence);
    
    bool setParamGenerateBrightness(int model,int exposure);

    bool setProjectorVersion(int version);

    void getProjectorVersion(int &version);
    
    void setParamSystemConfig(SystemConfigDataStruct param);

    void setParamFisherConfidence(float confidence);
    /************************************************************************/
    
    bool captureTextureImage(int model,float exposure,unsigned char* buff);

    bool captureRaw01(unsigned char* buff);

    bool captureRaw02(unsigned char* buff);
    
    bool captureRaw03(unsigned char* buff);
    
    bool captureRaw04(unsigned char* buff); 
    
    int captureRaw05(unsigned char* buff); 
    
    int captureRaw06(unsigned char* buff); 

    int captureRaw08(unsigned char* buff); 
    
    bool captureRaw04Repetition01(int repetition_count,unsigned char* buff);

    void mergeBrightness();

    int captureHdrBrightness(unsigned char* buff);

    int captureFrame06_cpu();

    int captureFrame06Hdr_cpu();

    int captureFrame06Repetition_cpu(int repetition_count);

    int captureFrame06Mono12_cpu();

    int captureFrame06HdrMono12_cpu();

    int captureFrame06RepetitionMono12_cpu(int repetition_count);

    bool readCalibParam();

    bool loadCalibData();

    void copyBrightnessData(unsigned char*& ptr);

    void copyDepthData(float*& ptr);

    void copyPointcloudData(float*& ptr);

    void getCameraResolution(int& width, int& height);


    int initCache();

    int reopenCamera();

    void getCameraPixelType(XemaPixelType& type);

    int setPixelFormat(int bit);

    int getPixelFormat(int& bit);

    int bayerToRgb(unsigned short* src, unsigned short* dst);

    CameraGalaxy* getCameraDevice();
    //bool capturePhase02Repetition02(int repetition_count,float* phase_x,float* phase_y,unsigned char* brightness);

    // bool captureFrame04();

    //int captureFrame04BaseConfidence(); 

    // bool captureFrame04Hdr();
    
   // int captureFrame04HdrBaseConfidence(); 
    


    //bool captureFrame04Repetition01(int repetition_count); 

    // bool captureFrame04Repetition02(int repetition_count);
    
    //int captureFrame04Repetition02BaseConfidence(int repetition_count);

   // bool captureFrame05();
    
    //int captureFrame06();
    
    //int captureFrame06Hdr();
  

    //int captureFrame06Repetition(int repetition_count);


    
    //int captureFrame06Mono12();
   

    //int captureFrame06HdrMono12();


    //int captureFrame06RepetitionMono12(int repetition_count); 

    
    //int captureFrame06Color();
    int captureFrame06Color_cpu();

    //int captureFrame06HdrColor();
    int captureFrame06HdrColor_cpu();

    //int captureFrame06RepetitionColor(int repetition_count);
    int captureFrame06RepetitionColor_cpu(int repetition_count);
    
   // int captureFrame06RepetitionColorMono12(int repetition_count);
    
    //bool captureFrame03();
    
    //bool captureFrame01();
    
    //bool testCaptureFrame01(unsigned char* buffer);
    /************************************************************************/
 


    /************************************************************************/
 


    //void removeOutlierBaseRadiusFilter();

    //void removeOutlierBaseDepthFilter();

    int set_calib_param(float* param);

    /**
     * @brief [26-03-21] 重置缓存
     */
    void bufferMemset();
    /**
     * @brief [26-03-21] 重启恢复参数
     */
    void recoverParam();

private:
 
    CameraGalaxy* camera_;
    LightCrafter3010* lc3010_;

    int patterns_sets_num_;

    int projector_version_;
    bool camera_opened_flag_;
    

    struct CameraCalibParam calib_param_;
 
 
    int hdr_num_; 
    std::vector<int> led_current_list_; 
    std::vector<int> camera_exposure_list_; 

    int max_camera_exposure_;
    int min_camera_exposure_;

    int min_camera_exposure_mono8_;
    int min_camera_exposure_mono12_; 

    int led_current_;
    int camera_exposure_;
    float camera_gain_;

    int generate_brightness_model_;
    int generate_brightness_exposure_;

    int image_width_;
    int image_height_;

    unsigned char* buff_brightness_;
    float* buff_depth_;
    float* buff_pointcloud_;

    float fisher_confidence_val_;

    std::mutex hdr_is_shooting_;
    SystemConfigDataStruct system_config_settings_machine_;

};
