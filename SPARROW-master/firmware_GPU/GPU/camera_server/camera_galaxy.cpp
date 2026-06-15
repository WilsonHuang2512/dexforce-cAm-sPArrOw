#include <winsock2.h> // 必须在windows.h之前包含, critical_zone.h中存在windows.h
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include "camera_galaxy.h"
#include <vector>
#include <fstream>
#include "set_calib_looktable.h"
#include "critical zone.h"
#include "easylogging++.h"
#include <windows.h>
#include <iomanip>

#include "GxIAPI.h"
#define Write_Pattern_Order             0x98
#define Read_Pattern_Order              0x99

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")


void Delay_nMs()
{
}

CameraGalaxy::CameraGalaxy() :
    m_addNACK(false),
    m_regNACK(false),
    m_bufNACK(false),
    m_bufNACKCount(0)
{
}
CameraGalaxy::~CameraGalaxy()
{
}


bool CameraGalaxy::trigger_software()
{
    GX_STATUS status = GX_STATUS_SUCCESS;
    // 发送软触发命令
    status = GXSendCommand(hDevice_, GX_COMMAND_TRIGGER_SOFTWARE);
    if (status != GX_STATUS_SUCCESS)
    {
        return false;
    }
    return true;
}

bool CameraGalaxy::streamOn() {

    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXSendCommand(hDevice_, GX_COMMAND_ACQUISITION_START);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "设置开始采集失败！" << std::endl;

    }

    return true;
}

bool CameraGalaxy::streamOff() {

    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXSendCommand(hDevice_, GX_COMMAND_ACQUISITION_STOP);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "设置停止采集失败！" << std::endl;

    }

    // [26-03-26] 流采集结束后清空缓存区
    status = GXFlushQueue(hDevice_);

    return true;
}


bool CameraGalaxy::grap(unsigned char* buf)
{
    if (pixel_format_ != 8)
    {
        LOG(INFO) << "set pixel format";
   
        return false;
    }



    int64_t nPayLoadSize = 0;
    GX_STATUS status = GX_STATUS_SUCCESS;


    status = GXGetInt(hDevice_, GX_INT_PAYLOAD_SIZE, &nPayLoadSize);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "获取数据大小失败！" << std::endl;
    }

    pFrameData.pImgBuf = malloc((size_t)nPayLoadSize);

    status = GXGetImage(hDevice_, &pFrameData, 1000);
    if (status != GX_STATUS_SUCCESS)
    {
        //std::cout << "get image failed" << std::endl;
        // 增加2D相机采集异常日志记录
        LOG(ERROR) << "GXGetImage: get image failed";
        LOG(ERROR) << "status = " << status;
        return false;
    }


    if (pFrameData.nStatus == GX_FRAME_STATUS_SUCCESS && pFrameData.pImgBuf != nullptr)
    {
        memcpy(buf, pFrameData.pImgBuf, pFrameData.nImgSize);
        free(pFrameData.pImgBuf);
    }
    else
    {
        // 增加2D相机采集残帧日志记录
        LOG(ERROR) << "GXGetImage:  Incomplete frame";
        LOG(ERROR) << "  frame status: " << pFrameData.nStatus;
        free(pFrameData.pImgBuf);
        return false;
    }

    return true;
}

bool CameraGalaxy::grap(unsigned short* buf)
{
    if (pixel_format_ != 12)
    {
        LOG(INFO) << "set pixel format";
        return false;
    }

    int64_t nPayLoadSize = 0;
    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXGetInt(hDevice_, GX_INT_PAYLOAD_SIZE, &nPayLoadSize);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "获取数据大小失败！" << std::endl;
    }

    pFrameData.pImgBuf = malloc((size_t)nPayLoadSize);

    if (GXGetImage(hDevice_, &pFrameData, 1000) != GX_STATUS_SUCCESS)
    {
        // 增加2D相机采集异常日志记录
        LOG(ERROR) << "GXGetImage: get image failed";
        LOG(ERROR) << "status = " << status;
        return false;
    }

    if (pFrameData.nStatus == GX_FRAME_STATUS_SUCCESS && pFrameData.pImgBuf != nullptr)
    {
        memcpy(buf, pFrameData.pImgBuf, pFrameData.nImgSize);
        free(pFrameData.pImgBuf);
    }
    else
    {
        // 增加2D相机采集残帧日志记录
        LOG(ERROR) << "GXGetImage:  Incomplete frame";
        LOG(ERROR) << "  frame status: " << pFrameData.nStatus;
        free(pFrameData.pImgBuf);
        return false;
    }

    return true;
}

bool CameraGalaxy::setPixelFormat(int val)
{
    
    LOG(INFO) << "set pixel format:"<<val;
    GXSendCommand(hDevice_, GX_COMMAND_ACQUISITION_STOP);
    GX_STATUS status = GX_STATUS_SUCCESS;
    switch (val)
    {
    case 8:
        //status = GXSetEnum(hDevice_, GX_ENUM_PIXEL_FORMAT, GX_PIXEL_FORMAT_MONO8);
        status = GXSetEnum(hDevice_, GX_ENUM_PIXEL_FORMAT, GX_PIXEL_FORMAT_BAYER_RG8);
        if (status != GX_STATUS_SUCCESS)
        {
            LOG(INFO) << "set RG8 failed!";
            status = GXSetEnum(hDevice_, GX_ENUM_PIXEL_FORMAT, GX_PIXEL_FORMAT_MONO8);
            if (status != GX_STATUS_SUCCESS) {
                LOG(INFO)  << "set mono8 failed!";
            }
            else
            {
                color = 0;
                LOG(INFO) << "set mono8 success!" ;
            }
        }
        else
        {
            color = 1;
            LOG(INFO) << "set RG8 success!";
        }
        pixel_format_ = val;
        break;
    case 12:
        GXSetEnum(hDevice_, GX_ENUM_PIXEL_FORMAT, GX_PIXEL_FORMAT_MONO12);

        pixel_format_ = val;
        break;
    default:
        break;
    }

    return true;
}

bool CameraGalaxy::getPixelFormat(int& val)
{
    val = pixel_format_;

    return true;
}

bool CameraGalaxy::getPixelType(XemaPixelType& type) {
    if (1 == color) {
        type = XemaPixelType::BayerRG8;
    }
    else
    {
        type = XemaPixelType::Mono;
    }
    return true;
}

bool CameraGalaxy::getMinExposure(float& val)
{
    double dValue = 0;
    if (GXGetFloat(hDevice_, GX_FLOAT_CURRENT_ACQUISITION_FRAME_RATE, &dValue) != GX_STATUS_SUCCESS)
    {

        return false;
    }
    val = (int)(1000000 / dValue);

    return true;
}

bool CameraGalaxy::openCamera(bool loadParam)
{
    std::lock_guard<std::mutex> my_guard(operate_mutex_);

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
            }
        }

        delete[] pBaseinfo;
    }

    GX_OPEN_PARAM stOpenParam;
    stOpenParam.accessMode = GX_ACCESS_EXCLUSIVE;
    stOpenParam.openMode = GX_OPEN_INDEX;
    stOpenParam.pszContent = cam_idx;
    status = GXOpenDevice(&stOpenParam, &hDevice_);

    if (status == GX_STATUS_SUCCESS)
    {

        // 获取设备IP信息, 用于筛选网卡
        GX_DEVICE_IP_INFO ipInfo;
        status = GXGetDeviceIPInfo(std::stoi(cam_idx), &ipInfo);
        // 提示网卡设置
        if (status == GX_STATUS_SUCCESS)
            checkNIC(std::string(ipInfo.szIP));

        GX_STATUS status = GX_STATUS_SUCCESS;
        status = GXSetEnum(hDevice_, GX_ENUM_ACQUISITION_MODE, 2);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to set the continuous collection mode!" << std::endl;

        }

        // 关闭帧率模式  
        status = GXSetEnum(hDevice_, GX_ENUM_ACQUISITION_FRAME_RATE_MODE, GX_ACQUISITION_FRAME_RATE_MODE_OFF);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to turn off automatic frame rate mode!" << std::endl;

        }

        /*int64_t nLinkThroughputLimitVal = 40000000;
        status = GXSetInt(hDevice_, GX_INT_DEVICE_LINK_THROUGHPUT_LIMIT,nLinkThroughputLimitVal);
       if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to set link!" << std::endl;

        }*/

        status = GXSetFloat(hDevice_, GX_FLOAT_ACQUISITION_FRAME_RATE, 75);



        // 先设置一个小曝光，例如1000us，然后查看最大帧率
        double exposure_temp = 1000;
        status = GXSetFloat(hDevice_, GX_FLOAT_EXPOSURE_TIME, exposure_temp);
        double max_frame = 0;
        status = GXGetFloat(hDevice_, GX_FLOAT_CURRENT_ACQUISITION_FRAME_RATE, &max_frame);


        std::cout << "frame:" << max_frame << std::endl;
 
        min_camera_exposure_ = (int)(1000000 / max_frame);



  

        status = GXGetInt(hDevice_, GX_INT_WIDTH, &image_width_);
        status = GXGetInt(hDevice_, GX_INT_HEIGHT, &image_height_);

        // 设置线选择器为LINE1  
        status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE1);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to set line selector." << std::endl;
            return false;
        }

        // 设置线模式为输出模式  
        status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_OUTPUT);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to set line mode." << std::endl;
            return false;
        }


        status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to set line mode." << std::endl;
         
        }

      /*  status = GXSetEnum(hDevice_, GX_ENUM_LINE_SOURCE, GX_ENUM_LINE_SOURCE_USEROUTPUT2);
        status = GXSetEnum(hDevice_, GX_ENUM_USER_OUTPUT_SELECTOR, GX_USER_OUTPUT_SELECTOR_OUTPUT2);
        status = GXSetBool(hDevice_, GX_BOOL_USER_OUTPUT_VALUE, false);*/
        setPixelFormat(8);

        //设置白平衡关闭
        status = GXSetEnum(hDevice_, GX_ENUM_BALANCE_WHITE_AUTO, GX_BALANCE_WHITE_AUTO_OFF);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to GX_ENUM_BALANCE_WHITE_AUTO." << std::endl;
        }

        //设置蓝色通道
        status = GXSetEnumValueByString(hDevice_, "BalanceRatioSelector", "Blue");
        status = GXSetFloatValue(hDevice_, "BalanceRatio", 1.9);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to Blue." << std::endl;
        }
        //设置绿色通道
        status = GXSetEnumValueByString(hDevice_, "BalanceRatioSelector", "Green");
        status = GXSetFloatValue(hDevice_, "BalanceRatio", 1.0000);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to Green." << std::endl;
        }
        //设置红色通道
        status = GXSetEnumValueByString(hDevice_, "BalanceRatioSelector", "Red");
        status = GXSetFloatValue(hDevice_, "BalanceRatio", 2.4);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to Red." << std::endl;
        }

        //设置饱和度
        status = GXSetEnumValueByString(hDevice_, "SaturationMode", "On");
        status = GXSetIntValue(hDevice_, "Saturation", 50);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to Saturation." << std::endl;
        }
     

        status = GXSetBoolValue(hDevice_, "GammaEnable", true);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to GammaEnable." << std::endl;
        }

        status = GXSetEnumValueByString(hDevice_, "GammaMode", "User");
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to GammaMode." << std::endl;
        }
        status = GXSetFloatValue(hDevice_, "Gamma", 1.0000);
        if (status != GX_STATUS_SUCCESS)
        {
            std::cout << "Failed to Gamma." << std::endl;
        }




        status = GXSetAcqusitionBufferNumber(hDevice_,  72);
        if (status != GX_STATUS_SUCCESS) {
            std::cout << "Failed to GXSetAcqusitionBufferNumber" << std::endl;


        }

        // 设置预留带宽
        LOG(INFO) << "Set BandwidthReserve: 10.";
        status = GXSetIntValue(hDevice_, "BandwidthReserve", 10);

        GX_INT_VALUE setIntValue;
        status = GXGetIntValue(hDevice_, "BandwidthReserve", &setIntValue);
        LOG(INFO) << "BandwidthReserve:" << setIntValue.nCurValue;

        // 加载相机参数
        if (loadParam)
        {
            if (loadCameraParamGenerateLUT()) 
                return true;
            else 
                return false;
        }

    }
    return true;
}

bool CameraGalaxy::resopenCamera()
{
    // 重新打开相机但不需生成LUT
    openCamera(false);
    // 需要重新设置为外部触发模式
    switchToExternalTriggerMode();
    return true;
}

bool CameraGalaxy::closeCamera()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::lock_guard<std::mutex> my_guard(operate_mutex_);

    //if (!camera_opened_state_)
    //{
    //    return false;
    //}

    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXCloseDevice(hDevice_);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "关闭设备失败！" << std::endl;

    }
    status = GXCloseLib();
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "关闭设备库失败！" << std::endl;

    }
    camera_opened_state_ = false;
    return true;
}

bool CameraGalaxy::loadCameraParamGenerateLUT()
{
    //在打开相机时读取相机的40个参数，生成calib_param.txt参数以及五个bin文件
    //设置使用区域
    GX_STATUS status = GXSetEnum(hDevice_, GX_ENUM_USER_DATA_FILED_SELECTOR, GX_USER_DATA_FILED_0);
    if (status != GX_STATUS_SUCCESS) 
    {
        LOG(ERROR) << "Failed to set the usage zone!";
        LOG(ERROR) << "Set the use zone return code:" << status;
        return false;
    }

    //申请长度为 4K 的 buffer，该 buffer 需要由用户填充数据
    size_t nLength = 4096;
    uint8_t* pSetBuffer = new uint8_t[nLength];
    //获取用户区内容, 获取失败不应继续执行
    status = GXGetBuffer(hDevice_, GX_BUFFER_USER_DATA_FILED_VALUE, pSetBuffer, &nLength);
    if (status != GX_STATUS_SUCCESS)
    {
        LOG(INFO) << "Failed to get the user area content!";
        delete[] pSetBuffer;
        return false;
    }

    float output_param[40] = {};
    memcpy(output_param, pSetBuffer, sizeof(float) * 40);
    delete[] pSetBuffer;

    //for (int i = 0; i < 40; i++) {
    //    std::cout << output_param[i] << std::endl;
    //}
    
    //判断本地40个参数是否与相机内一致，不一致则重新生成
    float param[40] = {};
    std::ifstream ifile;
    ifile.open("calib_param.txt");
    if (!ifile.is_open())
    {

        LOG(INFO) << "calib param.txt file does not exist, directly generate lookup table!";
        LOG(INFO) << "Lookup tables are being generated";
        LOG(INFO) << "Wait 30 seconds........";
        if (set_calib_looktable(output_param, image_width_, image_height_) < 0)
        {
            LOG(ERROR) << "Failed to generate lookup table!";
            return false;
        }
        LOG(INFO) << "The lookup table generation is complete!";
        return true;
    }

    int j = 0;
    for (int i = 0; i < 40; i++) 
    {
        ifile >> param[i];
        if (output_param[i] == param[i])
        {
            j++;
        }
        else
        {
            break;
        }
    }
    // 防止占用
    ifile.close();

    std::ifstream file1("combine_xL_rotate_x_cam1_iter.bin");
    std::ifstream file2("combine_xL_rotate_y_cam1_iter.bin");
    //std::ifstream file3("calib_param.txt");
    std::ifstream file4("R1.bin");
    std::ifstream file5("single_pattern_mapping.bin");
    std::ifstream file6("single_pattern_minimapping.bin");
    if (file1 && file2 /*&& file3*/ && file4 && file5 && file6 && (j == 40))
    {
        LOG(INFO) << "Bin files already exists,No need to generate lookup tables!";
        return true;
    }

    LOG(INFO) << "The internal parameters are different, directly generate lookup table!";
    LOG(INFO) << "Lookup tables are being generated";
    LOG(INFO) << "Wait 30 seconds........";
    if (set_calib_looktable(output_param, image_width_, image_height_) < 0)
    {
        LOG(ERROR) << "Failed to generate lookup table!";
        return false;
    }
    LOG(INFO) << "The lookup table generation is complete!";

    return true;
}

bool CameraGalaxy::switchToInternalTriggerMode()
{
    std::lock_guard<std::mutex> my_guard(operate_mutex_);
    GX_STATUS status;
    status = GXSetEnum(hDevice_, GX_ENUM_TRIGGER_MODE, GX_TRIGGER_MODE_ON);
    if (GX_STATUS_SUCCESS != status)
    {
        return false;
    }

    status = GXSetEnum(hDevice_, GX_ENUM_TRIGGER_SOURCE, GX_TRIGGER_SOURCE_SOFTWARE);
    if (GX_STATUS_SUCCESS != status)
    {
        return false;
    }

    status = GXSetEnum(hDevice_, GX_ENUM_EVENT_NOTIFICATION, GX_ENUM_EVENT_NOTIFICATION_ON);
    if (GX_STATUS_SUCCESS != status)
    {
        return false;
    }

    trigger_on_flag_ = false;

    return true;
}
bool CameraGalaxy::switchToExternalTriggerMode()
{

    std::lock_guard<std::mutex> my_guard(operate_mutex_);

    // 设置触发模式为开启  
    GX_STATUS status = GX_STATUS_SUCCESS;
    int trigger_mode = 1;
    status = GXSetEnum(hDevice_, GX_ENUM_TRIGGER_MODE, trigger_mode);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "设置触发模式失败！" << std::endl;

    }

    // 设置触发源为LINE0  
    int trigger_source = 1;
    status = GXSetEnum(hDevice_, GX_ENUM_TRIGGER_SOURCE, trigger_source);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "设置触发源失败！" << std::endl;

    }

    // 设置触发选择器为FRAME_START  
    int trigger_selector = 1;
    status = GXSetEnum(hDevice_, GX_ENUM_TRIGGER_SELECTOR, trigger_selector);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "设置触发选择器失败！" << std::endl;

    }

    // 设置触发激活方式为下降沿触发  
    int trigger_activation = 0;
    status = GXSetEnum(hDevice_, GX_ENUM_TRIGGER_ACTIVATION, trigger_activation);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "设置触发激活方式失败！" << std::endl;

    }

    // 设置用户输出选择器为OUTPUT1  
    status = GXSetEnum(hDevice_, GX_ENUM_USER_OUTPUT_SELECTOR, GX_USER_OUTPUT_SELECTOR_OUTPUT1);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "设置用户输出选择器失败！" << std::endl;

    }

    // 设置用户输出值为真  
    bool user_output_value = true;
    status = GXSetBool(hDevice_, GX_BOOL_USER_OUTPUT_VALUE, user_output_value);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "设置用户输出值失败！" << std::endl;
    }
    trigger_on_flag_ = true;
    return true;
}

bool CameraGalaxy::getExposure(double& val)
{
    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXGetFloat(hDevice_, GX_FLOAT_EXPOSURE_TIME, &val);

    if (status != GX_STATUS_SUCCESS)
    {
        return false;
    }

    return true;
}
bool CameraGalaxy::setExposure(double val)
{

    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXSetFloat(hDevice_, GX_FLOAT_EXPOSURE_TIME, val);

    if (status != GX_STATUS_SUCCESS)
    {
        return false;
    }

    return true;
}
bool CameraGalaxy::getGain(double& val)
{
    std::lock_guard<std::mutex> my_guard(operate_mutex_);

    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXGetFloat(hDevice_, GX_FLOAT_GAIN, &val);

    if (status != GX_STATUS_SUCCESS)
    {

        return false;
    }

    return true;
}
bool CameraGalaxy::setGain(double val)
{
    std::lock_guard<std::mutex> my_guard(operate_mutex_);

    GX_STATUS status = GX_STATUS_SUCCESS;
    status = GXSetFloat(hDevice_, GX_FLOAT_GAIN, val);

    if (status != GX_STATUS_SUCCESS)
    {
        return false;
    }
    return true;
}


void CameraGalaxy::LINE0_IN() {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE0  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE0);
    if (status != GX_STATUS_SUCCESS)
    {
        printf("Failed to set line selector.\n");
        return;
    }
    // 设置线模式为输入模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_INPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        printf("Failed to set line mode.\n");
        return;
    }
}

void CameraGalaxy::LINE1_OUT(bool xOn) {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE1  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE1);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector." << std::endl;
        return;
    }

    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE,GX_ENUM_LINE_MODE_OUTPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }

  //status = GXSetEnum(hDevice_, GX_ENUM_LINE_SOURCE, GX_ENUM_LINE_SOURCE_USEROUTPUT2);
  //status = GXSetEnum(hDevice_, GX_ENUM_USER_OUTPUT_SELECTOR, GX_USER_OUTPUT_SELECTOR_OUTPUT2);
  //status = GXSetBool(hDevice_, GX_BOOL_USER_OUTPUT_VALUE, xOn);


    EnterCriticalSection();
   
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, true);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }

    // [26-01-30] 加长 trigger 时间(小延时), 确保MCU能正确收到
    std::this_thread::sleep_for(std::chrono::milliseconds(3));

    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    LeaveCriticalSection();
  /*  status = GXSetEnum(hDevice_, GX_ENUM_LINE_SOURCE, GX_ENUM_LINE_SOURCE_USEROUTPUT2);
    status = GXSetEnum(hDevice_, GX_ENUM_USER_OUTPUT_SELECTOR, GX_USER_OUTPUT_SELECTOR_OUTPUT2);
    status = GXSetBool(hDevice_, GX_BOOL_USER_OUTPUT_VALUE, xOn);*/
}

void CameraGalaxy::LINE1_OUT_HDR(bool xOn) {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE1  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE1);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector." << std::endl;
        return;
    }

    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_OUTPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }

    //status = GXSetEnum(hDevice_, GX_ENUM_LINE_SOURCE, GX_ENUM_LINE_SOURCE_USEROUTPUT2);
    //status = GXSetEnum(hDevice_, GX_ENUM_USER_OUTPUT_SELECTOR, GX_USER_OUTPUT_SELECTOR_OUTPUT2);
    //status = GXSetBool(hDevice_, GX_BOOL_USER_OUTPUT_VALUE, xOn);


    EnterCriticalSection();

    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, true);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    Sleep(100);
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    LeaveCriticalSection();
    /*  status = GXSetEnum(hDevice_, GX_ENUM_LINE_SOURCE, GX_ENUM_LINE_SOURCE_USEROUTPUT2);
      status = GXSetEnum(hDevice_, GX_ENUM_USER_OUTPUT_SELECTOR, GX_USER_OUTPUT_SELECTOR_OUTPUT2);
      status = GXSetBool(hDevice_, GX_BOOL_USER_OUTPUT_VALUE, xOn);*/
}



void CameraGalaxy::SDA_OUTPUT() {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE2);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector." << std::endl;
        return;
    }
    // 设置线模式为输出模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_OUTPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    Delay_nMs();
}

void CameraGalaxy::SDA_ON() {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE2);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector." << std::endl;
        return;
    }
    // 设置线模式为输出模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_OUTPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    Delay_nMs();
}

void CameraGalaxy::SDA_OFF() {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE2);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector." << std::endl;
        return;
    }
    // 设置线模式为输出模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_OUTPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, true);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    Delay_nMs();
}

void CameraGalaxy::SDA_INPUT() {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE2);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector2." << std::endl;
        return;
    }
    // 设置线模式为输出模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_INPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    Delay_nMs();
}

bool CameraGalaxy::SDA_READ() {
    GX_STATUS status = GX_STATUS_SUCCESS;
    bool bLineStatus = true;
    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE2);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector22." << std::endl;
        return 0;
    }
    // 设置线模式为输出模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_INPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return 0;
    }
    status = GXGetBool(hDevice_, GX_BOOL_LINE_STATUS, &bLineStatus);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to get line status." << std::endl;
        return 0;
    }
    return bLineStatus;
}

void CameraGalaxy::SCL_OUTPUT() {

    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE3);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector3." << std::endl;
        return;
    }
    // 设置线模式为输出模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_OUTPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    Delay_nMs();
}
void CameraGalaxy::SCL_ON_OFF() {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE3);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector33." << std::endl;
        return;
    }
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    Delay_nMs();
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, true);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    Delay_nMs();
}
void CameraGalaxy::SCL_ON() {

    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE3);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector33." << std::endl;
        return;
    }
    // 设置线模式为输出模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_OUTPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, false);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    
    Delay_nMs();

}

void CameraGalaxy::SCL_OFF() {
    GX_STATUS status = GX_STATUS_SUCCESS;

    // 设置线选择器为LINE2  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_SELECTOR, GX_ENUM_LINE_SELECTOR_LINE3);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line selector333." << std::endl;
        return;
    }
    // 设置线模式为输出模式  
    status = GXSetEnum(hDevice_, GX_ENUM_LINE_MODE, GX_ENUM_LINE_MODE_OUTPUT);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line mode." << std::endl;
        return;
    }
    // 设置线反转为关闭  
    status = GXSetBool(hDevice_, GX_BOOL_LINE_INVERTER, true);
    if (status != GX_STATUS_SUCCESS)
    {
        std::cout << "Failed to set line inverter." << std::endl;
        return;
    }
    Delay_nMs();
}

bool CameraGalaxy::set_calib_param(float* c_in) {
    GX_STATUS status = GX_STATUS_SUCCESS;
    //设置使用区域
    status = GXSetEnum(hDevice_, GX_ENUM_USER_DATA_FILED_SELECTOR, GX_USER_DATA_FILED_0);
    if (status != GX_STATUS_SUCCESS) {
        std::cout << "Failed to set the usage zone" << std::endl;
        std::cout << "Set the use of the zone return code：" << status << std::endl;

    }
    else
    {
        std::cout << "Setting the user area return code succeeded" << std::endl;
        //std::cout << "设置使用区域0返回码：" << status << std::endl;
    }

    size_t nLength = 40 * sizeof(float);  // 40 个 float 类型元素
    uint8_t* pSetBuffer = new uint8_t[nLength];  // 为 40 个 float 分配空间
    //拷贝数据：将c_in中的float数据逐字节拷贝到pSetBuffer中
    std::memcpy(pSetBuffer, c_in, nLength);

    //设置用户区内容
    status = GXSetBuffer(hDevice_, GX_BUFFER_USER_DATA_FILED_VALUE, pSetBuffer, nLength);
    if (status == GX_STATUS_SUCCESS) {
        std::cout << "Description Writing the user data area succeeded" << std::endl;
    }
    else
    {
        std::cout << "Set error tolerance codes in the user zone：" << status << std::endl;
    }

    //Sleep(1000);
    ////获取用户区内容
    //size_t nLength1 = 40 * sizeof(float);  // 40 个 float 类型元素
    //uint8_t* pSetBuffer1 = new uint8_t[nLength1];  // 为 40 个 float 分配空间
    //status = GXGetBuffer(hDevice_, GX_BUFFER_USER_DATA_FILED_VALUE, pSetBuffer1, &nLength1);
    //if (status == GX_STATUS_SUCCESS) {
    //    std::cout << "Internal data is obtained successfully. The following information is displayed" << std::endl;
    //}
    //else
    //{
    //    std::cout << "Get error tolerant error codes in the user zone：" << status << std::endl;
    //}

    //float output_param[40] = {};
    //memcpy(output_param, pSetBuffer1, sizeof(float) * 40);
    //for (int i = 0; i < 40; i++) {
    //    std::cout << output_param[i] << std::endl;

    //}

    return true;
}

void CameraGalaxy::IICReset()
{
    SDA_OUTPUT();
    SCL_OFF();
    SDA_OFF();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SCL_ON();
    SDA_ON();
}

bool CameraGalaxy::checkAck()
{
    LOG(INFO) << "Checking ACK Status...";
    int retryCount = 0;
    const int maxRetries = 10;
    const std::chrono::seconds retryTime(1);

    while (retryCount < maxRetries)
    {
        LOG(INFO) << "-- ACK First Check --";
        unsigned char working_mode = 0; // Single Exposure
        write(SingleChip_Write_Working_Mode, &working_mode, 0);
        if (getNACKStatus())
        {
            LOG(INFO) << "NACK. Retrying [" << retryCount + 1 << " / " << maxRetries << "]...";
            this->IICReset();
            std::this_thread::sleep_for(retryTime);
            retryCount++;
            continue;
        }

        // Double Check
        LOG(INFO) << "-- ACK Second Check --";
        int exposure_int = 5000;
        write(SingleChip_Write_Exposure_Time, &exposure_int, 4);
        if (getNACKStatus())
        {
            LOG(INFO) << "NACK. Retrying [" << retryCount + 1 << " / " << maxRetries << "]...";
            this->IICReset();
            std::this_thread::sleep_for(retryTime);
            retryCount++;
            continue;
        }
        else
        {
            LOG(INFO) << "Check ACK Done.";
            return true;
        }

    }
    LOG(INFO) << "Check ACK Failed.";
    return false;
}

bool CameraGalaxy::getNACKStatus()
{
    if (this->m_addNACK || this->m_regNACK || this->m_bufNACKCount > 0)
        return true;
    return false;
}

bool CameraGalaxy::getMCUVersion(std::string& version)
{
    // 清空版本号
    version = std::string();

    // 读取 MCU 版本号 VYYMMDD
    size_t nLength = 10;
    char versionStr[10] = { 0 };
    if (read2(SingleChip_Read_MCU_Version, &versionStr, nLength) == 0)
    {
        return false;
    }

    versionStr[9] = '\0';
    // 判断是否为新版本MCU程序
    if (versionStr[0] != 'V' && versionStr[0] != 'v')
        return false;

    version = std::string(versionStr); 
    return true;
}

bool CameraGalaxy::getDLPStatus(bool& status)
{
    status = false;

    // 读取 DLP 启动状态
    size_t nLength = 10;
    char dlpStr[10] = { 0 };
    if (read2(SingleChip_Read_DLP_Status, &dlpStr, nLength) == 0)
    {
        return false;
    }

    dlpStr[9] = '\0';
    // 判断是否为新版本MCU程序
    if (dlpStr[0] != 'L' && dlpStr[0] != 'l') // lcf: on ; lcf: off
        return false;

    // 判断是否启动
    std::string statusStr = std::string(dlpStr);
    if (statusStr.find("on") != std::string::npos || statusStr.find("ON") != std::string::npos)
    {
        status = true;
    }
    else if (statusStr.find("off") != std::string::npos || statusStr.find("OFF") != std::string::npos)
    {
        status = false;
    }

    return true;
}

bool CameraGalaxy::checkNIC(const std::string& cameraIPv4Address)
{
    if (!this->m_networkAdapterMgr.enumerateEthernetAdapters())
    {
        std::cout << "Enumerate Ethernet Adapters Failed." << std::endl;
        return false;
    }
    
    const std::vector<std::string> keywordsEEE = {
    "节能以太网", "EEE", "Energy Efficient Ethernet",
    "绿色以太网", "环保节能", "Green Ethernet"
    };
    const std::vector<std::string> keywordsInterrupt = {
    "中断裁决", "中断调整", "中断节流", "中断仲裁",
    "Interrupt Moderation", "Interrupt Throttle", "Interrupt Arbitration",
    "Rx Moderation", "Tx Moderation"
    };

    auto adapter = this->m_networkAdapterMgr.getAdapter(cameraIPv4Address);
    std::cout << "=========== Network Adapter ===========" << std::endl;
    std::cout << "FriendlyName: " << adapter.friendlyName << std::endl;
    std::cout << "MAC Address: " << adapter.macAddress << std::endl;
    std::cout << "IPv4 Address: " << adapter.ipv4Address << std::endl;
    std::cout << "IPv4 SubnetMask: " << adapter.subnetMask << std::endl;
    std::cout << "IPv4 Gateway: " << adapter.gateway << std::endl;
    std::cout << "TransmitLinkSpeed: " << adapter.transmitLinkSpeed << std::endl;
    if (std::stoi(adapter.transmitLinkSpeed) < 1000)
    {
        LOG(WARNING) << "网卡传输速率低于1000Mbps, 可能导致丢包.";
    }
    std::cout << "============ AdvancedProps ============" << std::endl;
    for (const auto& [key, value] : adapter.advancedProps) 
    {
        std::cout << key << "：" << value << std::endl;
        // 检查节能模式
        for (const auto& kw : keywordsEEE)
        {
            if (key.find(kw) != std::string::npos)
            {
                // 0: 关闭; 1: 启用
                if(value == "1") LOG(WARNING) << "网卡节能模式启用, 可能导致丢包.";
                break;
            }
        }
        // 检查中断裁决
        for (const auto& kw : keywordsInterrupt)
        {
            if (key.find(kw) != std::string::npos)
            {
                // 0: 关闭; 1: 启用
                if (value == "1") LOG(WARNING) << "网卡中断裁决启用, 可能导致丢包.";
                break;
            }
        }
    }
    std::cout << "=========================================" << std::endl;

    return true;
}

size_t CameraGalaxy::read(char inner_reg, void* buffer, size_t buffer_size) {
    //return 0;
    char ack = 0;
    char inner_addr = 0x1b;

    SDA_OUTPUT();
    SDA_ON();
    SCL_ON();

    SDA_OFF();
    SCL_OFF();

    char ch = (inner_addr << 1);

    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        SCL_ON();
        SCL_OFF();
        ch <<= 1;
    }
    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    ack = SDA_READ();
    SCL_OFF();
    if (ack)
    {
        std::cout << "no ack" << std::endl;
    }
    SDA_OUTPUT();

    ch = inner_reg;
    SDA_OUTPUT();
    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        SCL_ON();
        SCL_OFF();
        ch <<= 1;
    }

    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    ack = SDA_READ();
    SCL_OFF();
    if (ack)
    {
        std::cout << "no ack" << std::endl;
    }

    SDA_OUTPUT();
    SCL_OFF();
    SDA_OFF();

    SCL_ON();
    SDA_ON();

    SDA_OFF();
    SCL_OFF();


    ch = (inner_addr << 1) | 0x01;

    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        SCL_ON();
        SCL_OFF();
        ch <<= 1;
    }
    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    ack = SDA_READ();
    SCL_OFF();
    if (ack)
    {
        std::cout << "no ack" << std::endl;
    }
    char* p = (char*)buffer;
    for (int i = 0; i < buffer_size; i++)
    {
        ch = 0;
        SDA_INPUT();
        for (int j = 0; j < 8; j++)
        {
            ch <<= 1;
            if (SDA_READ())
            {
                ch |= 0x01;
                //  std::cout << "1 ";
            }
            else
            {
                //  std::cout << "0 ";
            }
            SCL_ON();
            SCL_OFF();
        }
        std::cout << std::endl;
        SDA_OUTPUT();
        SCL_ON();
        SDA_OFF();
        SCL_OFF();
        *p++ = (ch & 0xff);
        std::cout << "rd:" << (int)ch << std::endl;
    }

    SCL_ON();
    SDA_ON();

    return 0;
}

size_t CameraGalaxy::write(char inner_reg, void* buffer, size_t buffer_size) {
    this->m_addNACK = false;
    this->m_regNACK = false;
    this->m_bufNACK = false;
    this->m_bufNACKCount = 0;

    char inner_addr = 0x1b;

    SDA_OUTPUT();
    SDA_ON();
    SCL_ON();
    SDA_OFF();
    SCL_OFF();

    char ch = (inner_addr << 1);

    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        /*SCL_ON();
        SCL_OFF();*/
        SCL_ON_OFF();
        ch <<= 1;
    }

    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    //Delay_nMs();
    this->m_addNACK = SDA_READ();
    SCL_OFF();
    SDA_OUTPUT();

    ch = inner_reg;
    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        //SCL_ON();
        //SCL_OFF();
        SCL_ON_OFF();
        ch <<= 1;
    }

    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    this->m_regNACK = SDA_READ();
    SCL_OFF();
    SDA_OUTPUT();

    char* p = (char*)buffer;
    for (int i = 0; i < buffer_size; i++)
    {
        char ch = *p++;
        SDA_OUTPUT();
        for (int j = 0; j < 8; j++)
        {
            if (ch & 0x80)
            {
                SDA_ON();
            }
            else
            {
                SDA_OFF();
            }
            //SCL_ON();
            //SCL_OFF();
            SCL_ON_OFF();
            ch <<= 1;
        }

        SDA_ON();
        SDA_INPUT();
        SCL_ON();
        this->m_bufNACK = SDA_READ();
        SCL_OFF();
        if (this->m_bufNACK)
        {
            this->m_bufNACKCount++;
        }

    }

    SDA_OUTPUT();
    SCL_OFF();
    SDA_OFF();

    SCL_ON();
    SDA_ON();

    if (this->m_addNACK)
        LOG(ERROR) << "ADDR NACK!";
    if (this->m_regNACK)
    {
        LOG(ERROR) << "REG NACK!";
        std::stringstream ss;
        ss << "inner_reg: 0x" << std::hex << std::uppercase
            << std::setw(2) << std::setfill('0')
            << static_cast<unsigned int>(static_cast<unsigned char>(inner_reg))
            ;
        LOG(ERROR) << ss.str();
    }
    if (this->m_bufNACKCount > 0)
        LOG(ERROR) << "Buffer NACK! Count = " << this->m_bufNACKCount;
    if (this->m_addNACK || this->m_regNACK || this->m_bufNACKCount > 0)
        return 1000;

    return 0;
}

void CameraGalaxy::trigger_one_sequence()
{
    LINE1_OUT(true);

}

void CameraGalaxy::trigger_hdr_list_flush()
{

    LINE1_OUT_HDR(true);

}

size_t CameraGalaxy::read2(char inner_reg, void* buffer, size_t buffer_size)
{
    this->m_addNACK = false;
    this->m_regNACK = false;
    this->m_bufNACK = false;
    this->m_bufNACKCount = 0;

    char inner_addr = 0x1b;

    SDA_OUTPUT();
    SDA_ON();
    SCL_ON();

    SDA_OFF();
    SCL_OFF();

    char ch = (inner_addr << 1);

    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        SCL_ON();
        SCL_OFF();
        ch <<= 1;
    }
    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    this->m_addNACK = SDA_READ();
    SCL_OFF();
    SDA_OUTPUT();
    if (this->m_addNACK)
    {
        LOG(ERROR) << "ADDR NACK!";
        return 0;
    }
    ch = inner_reg;
    SDA_OUTPUT();
    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        SCL_ON();
        SCL_OFF();
        ch <<= 1;
    }

    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    this->m_regNACK = SDA_READ();
    SCL_OFF();

    SDA_OUTPUT();
    if (this->m_regNACK)
    {
        LOG(ERROR) << "REG NACK!";
        return 0;
    }

    // 发送长度
    ch = static_cast<char>(buffer_size);
    SDA_OUTPUT();
    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        SCL_ON();
        SCL_OFF();
        ch <<= 1;
    }

    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    this->m_bufNACK = SDA_READ();
    SCL_OFF();
    SDA_OUTPUT();
    if (this->m_bufNACK)
    {
        LOG(ERROR) << "Buffer NACK!";
        return 0;
    }
    SCL_OFF();
    SDA_OFF();

    SCL_ON();
    SDA_ON();

    SDA_OFF();
    SCL_OFF();


    ch = (inner_addr << 1) | 0x01;

    for (int j = 0; j < 8; j++)
    {
        if (ch & 0x80)
        {
            SDA_ON();
        }
        else
        {
            SDA_OFF();
        }
        SCL_ON();
        SCL_OFF();
        ch <<= 1;
    }
    SDA_ON();
    SDA_INPUT();
    SCL_ON();
    this->m_bufNACK = SDA_READ();
    SCL_OFF();

    // 读取数据
    char* p = (char*)buffer;
    for (int i = 0; i < buffer_size; i++)
    {
        ch = 0;
        SDA_INPUT();
        for (int j = 0; j < 8; j++)
        {
            ch <<= 1;
            if (SDA_READ())
            {
                ch |= 0x01;
                //  std::cout << "1 ";
            }
            else
            {
                //  std::cout << "0 ";
            }
            SCL_ON();
            SCL_OFF();
        }
        SDA_OUTPUT();
        SCL_ON();
        SDA_OFF();
        SCL_OFF();
        *p++ = (ch & 0xff);
    }

    SCL_ON();
    SDA_ON();
    return buffer_size;
}

/**
 * @brief MAC地址转字符串
 * @param [in] mac MAC地址
 * @param [in] len MAC地址长度
 * @return MAC地址字符串
 */
std::string MacToString(BYTE* mac, ULONG len)
{
    if (len < 6) return "未知";
    char macStr[32] = { 0 };
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return macStr;
}
/**
 * @brief 宽字符转字符串
 * @param [in] wstr 宽字符
 * @return 字符串
 */
std::string WStringToString(const std::wstring& wstr)
{
    if (wstr.empty()) return {};
    int len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string res(len - 1, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &res[0], len, nullptr, nullptr);
    return res;
}
/**
 * @brief 字符串转宽字符
 * @param [in] str 字符串
 * @return 宽字符
 */
std::wstring StringToWString(const std::string& str)
{
    if (str.empty()) return {};
    int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    std::wstring res(len - 1, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &res[0], len);
    return res;
}
/**
 * @brief 子网前缀字符转子网掩码
 * @param [in] prefixLength 子网前缀字符
 * @return 子网掩码 e.g. "255.255.255.0"
 */
std::string PrefixToSubnetMask(int prefixLength)
{
    if (prefixLength < 0 || prefixLength > 32) return "未知";
    uint32_t mask = prefixLength == 0 ? 0 : (0xFFFFFFFFu << (32 - prefixLength));
    mask = htonl(mask);

    char subnet[INET_ADDRSTRLEN] = { 0 };
    inet_ntop(AF_INET, &mask, subnet, INET_ADDRSTRLEN);
    return subnet;
}
/**
 * @brief 读取注册表中的字符串配置
 * @param [in]  root 根键
 * @param [in]  path 路径
 * @param [in]  key  键
 * @param [out] out 配置字符串
 * @return 是否成功
 */
bool RegReadString(HKEY root, const std::wstring& path, const std::wstring& key, std::wstring& out)
{
    HKEY hKey;
    if (RegOpenKeyExW(root, path.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) return false;

    wchar_t buf[512];
    DWORD size = sizeof(buf);
    DWORD type = 0;

    if (RegQueryValueExW(hKey, key.c_str(), 0, &type, (LPBYTE)buf, &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ)) {
        out = buf;
        RegCloseKey(hKey);
        return true;
    }

    RegCloseKey(hKey);
    return false;
}
/**
 * @brief 读取注册表中的DWORD配置
 * @param [in]  root 根键
 * @param [in]  path 路径
 * @param [in]  key  键
 * @param [out] out 配置值
 * @return 是否成功
 */
bool RegReadDWORD(HKEY root, const std::wstring& path, const std::wstring& key, DWORD& out)
{
    HKEY hKey;
    if (RegOpenKeyExW(root, path.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) return false;

    DWORD size = sizeof(DWORD);
    DWORD type = 0;

    if (RegQueryValueExW(hKey, key.c_str(), 0, &type, (LPBYTE)&out, &size) == ERROR_SUCCESS &&
        type == REG_DWORD) {
        RegCloseKey(hKey);
        return true;
    }

    RegCloseKey(hKey);
    return false;
}
/**
 * @brief 读取注册表中的任意配置
 * @param [in]  root 根键
 * @param [in]  path 路径
 * @param [in]  key  键
 * @return 配置字符串
 */
std::string RegReadAny(HKEY root, const std::wstring& path, const std::wstring& key)
{
    std::wstring ws;
    if (RegReadString(root, path, key, ws))
        return WStringToString(ws);

    DWORD dw;
    if (RegReadDWORD(root, path, key, dw))
        return std::to_string(dw);

    return "";
}

NetworkAdapterManager::NetworkAdapterManager() :
    m_initialized(false)
{
    initializeWSA();
}

NetworkAdapterManager::~NetworkAdapterManager()
{
    cleanupWSA();
}

bool NetworkAdapterManager::enumerateEthernetAdapters()
{
    if (!this->m_initialized) 
        return false;

    // 清空旧数据
    clearAdapters();

    // 获取缓冲区大小
    ULONG bufLen = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, nullptr, &bufLen);
    if (bufLen == 0) return false;

    // 分配内存
    PIP_ADAPTER_ADDRESSES pAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufLen);
    if (!pAddresses) return false;

    // 获取网卡信息
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, pAddresses, &bufLen) == ERROR_SUCCESS)
    {
        for (auto p = pAddresses; p; p = p->Next) 
        {
            // 过滤：仅以太网 + 已启用
            if (p->IfType != IF_TYPE_ETHERNET_CSMACD || p->OperStatus != IfOperStatusUp)
                break;

            EthernetConfigure adapter;
            adapter.friendlyName = WStringToString(p->FriendlyName);
            adapter.adapterName = p->AdapterName;
            adapter.macAddress = MacToString(p->PhysicalAddress, p->PhysicalAddressLength);

            // 速率
            char speed[32];
            sprintf_s(speed, "%llu Mbps", p->TransmitLinkSpeed / 1000000);
            adapter.transmitLinkSpeed = speed;

            // IPv4地址+子网掩码
            for (auto u = p->FirstUnicastAddress; u; u = u->Next) {
                if (u->Address.lpSockaddr->sa_family == AF_INET) {
                    char ip[32];
                    auto sa = (sockaddr_in*)u->Address.lpSockaddr;
                    inet_ntop(AF_INET, &sa->sin_addr, ip, 32);
                    adapter.ipv4Address = ip;
                    adapter.subnetMask = PrefixToSubnetMask(u->OnLinkPrefixLength);
                    break;
                }
            }

            // 网关
            if (p->FirstGatewayAddress) {
                auto sa = (sockaddr_in*)p->FirstGatewayAddress->Address.lpSockaddr;
                char gw[32];
                inet_ntop(AF_INET, &sa->sin_addr, gw, 32);
                adapter.gateway = gw;
            }

            // 高级属性
            std::wstring regPath;
            if (findAdapterRegistryPath(StringToWString(adapter.adapterName), regPath)) {
                loadAdvancedProps(regPath, adapter);
            }

            this->m_ethernetConfigList.push_back(adapter);
        }
    }

    // 释放内存
    free(pAddresses);
    return !this->m_ethernetConfigList.empty();
}

EthernetConfigure NetworkAdapterManager::getAdapter(const std::string& ipv4Address) const
{
    std::string subnet = ipv4Address.substr(0, ipv4Address.find_last_of('.'));
    for (auto& adapter : this->m_ethernetConfigList)
    {
        if (adapter.ipv4Address.find(subnet) != std::string::npos)
        {
            return adapter;
        }
    }
    return EthernetConfigure();
}

std::vector<EthernetConfigure> NetworkAdapterManager::getAdapters() const
{
    return this->m_ethernetConfigList;
}

void NetworkAdapterManager::clearAdapters()
{
    std::vector<EthernetConfigure> empty;
    this->m_ethernetConfigList.swap(empty);
}

bool NetworkAdapterManager::initializeWSA()
{
    WSADATA wsa;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsa);
    this->m_initialized = (ret == 0);
    return m_initialized;
}

void NetworkAdapterManager::cleanupWSA()
{
    if (this->m_initialized)
    {
        WSACleanup();
        this->m_initialized = false;
    }
}

bool NetworkAdapterManager::findAdapterRegistryPath(const std::wstring& adapterId, std::wstring& outPath)
{
    std::wstring base = L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}";

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, base.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    for (DWORD i = 0;; i++) {
        wchar_t name[256];
        DWORD size = 256;

        if (RegEnumKeyExW(hKey, i, name, &size, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        std::wstring sub = base + L"\\" + name;

        std::wstring val;
        if (RegReadString(HKEY_LOCAL_MACHINE, sub, L"NetCfgInstanceId", val)) {
            if (_wcsicmp(val.c_str(), adapterId.c_str()) == 0) {
                outPath = sub;
                RegCloseKey(hKey);
                return true;
            }
        }
    }

    RegCloseKey(hKey);
    return false;
}

void NetworkAdapterManager::loadAdvancedProps(const std::wstring& classPath, EthernetConfigure& config)
{
    std::wstring params = classPath + L"\\Ndi\\Params";

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, params.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return;

    for (DWORD i = 0;; i++) 
    {
        wchar_t name[256];
        DWORD size = 256;

        if (RegEnumKeyExW(hKey, i, name, &size, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;

        std::wstring param = name;
        std::wstring full = params + L"\\" + param;

        std::wstring desc;
        RegReadString(HKEY_LOCAL_MACHINE, full, L"ParamDesc", desc);

        std::string value = RegReadAny(HKEY_LOCAL_MACHINE, classPath, param);

        std::string key = desc.empty() ? WStringToString(param) : WStringToString(desc);
        config.advancedProps[key] = value.empty() ? "未设置" : value;
    }

    RegCloseKey(hKey);
}

