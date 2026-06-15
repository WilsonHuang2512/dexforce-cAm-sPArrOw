#pragma once
#include "GxIAPI.h"
#include "camera.h"
#include "lightcrafter3010.h"
#include <chrono>         // std::chrono::milliseconds
#include <thread>         // std::thread
#include <mutex>          // std::timed_mutex
#include <string>
#include <map>

// 以太网配置信息
class EthernetConfigure
{
public:
	std::string friendlyName;      /*!< 网卡友好名称 */
	std::string adapterName;       /*!< 网卡ID */
	std::string macAddress;        /*!< MAC地址 */
	std::string ipv4Address;       /*!< IPv4地址 */
	std::string gateway;           /*!< 网关 */
	std::string subnetMask;        /*!< 子网掩码 */
	std::string transmitLinkSpeed; /*!< 传输链路速度 */

	std::map<std::string, std::string> advancedProps; /*!< 高级属性 */
};

// 网络适配器管理器
class NetworkAdapterManager
{
public:
	NetworkAdapterManager();
	~NetworkAdapterManager();
	/**
	 * @brief 枚举网络适配器
	 * @return
	 */
	bool enumerateEthernetAdapters();
	/**
	 * @brief 获取和相机在同一网段下的网络适配器配置信息
	 * @param [in] ipv4Address IP地址
	 * @return 适配器配置信息
	 */
	EthernetConfigure getAdapter(const std::string& ipv4Address) const;
	/**
	 * @brief 获取所有网络适配器配置信息
	 * @return 适配器配置信息列表
	 */
	std::vector<EthernetConfigure> getAdapters() const;
	/**
	 * @brief 清空网络适配器配置信息
	 */
	void clearAdapters();

private:
	/**
	 * @brief 初始化 WSA
	 * @return true: 初始化成功; false: 初始化失败
	 */
	bool initializeWSA();
	/**
	 * @brief 清理 WSA
	 */
	void cleanupWSA();
	/**
	 * @brief 获取指定适配器注册表路径
	 * @param [in]  adapterId 适配器ID
	 * @param [out] outPath   注册表路径
	 * @return true: 获取成功; false: 获取失败
	 */
	bool findAdapterRegistryPath(const std::wstring& adapterId, std::wstring& outPath);
	/**
	 * @brief 加载指定适配器的高级设置
	 * @param [in]  classPath 注册表路径
	 * @param [out] config    适配器配置信息
	 */
	void loadAdvancedProps(const std::wstring& classPath, EthernetConfigure& config);

	std::vector<EthernetConfigure> m_ethernetConfigList; /*!< 网络适配器配置列表 */
	bool m_initialized; /*!< 是否初始化 */
};


class CameraGalaxy : public Camera, public LightCrafter3010
{
public:
	CameraGalaxy();
	~CameraGalaxy();
	/**
	 * @brief 打开相机.
	 * @param [in] loadParam 是否从文件读取相机参数.
	 * @return true: 打开相机成功; false: 打开相机失败.
	 */
	bool openCamera(bool loadParam = true);

	bool resopenCamera();

	bool closeCamera();
	/**
	 * @brief 从txt文件读取相机参数并生成LUT.
	 * @return true: 读取相机参数成功; false: 读取相机参数失败.
	 */
	bool loadCameraParamGenerateLUT();

	bool switchToInternalTriggerMode();
	bool switchToExternalTriggerMode();

	bool getExposure(double& val);
	bool setExposure(double val);

	bool getGain(double& value);
	bool setGain(double value);

	bool trigger_software();
	bool grap(unsigned char* buf);
	bool grap(unsigned short* buf);

	bool setPixelFormat(int val);

	bool getPixelFormat(int& val);

	bool getPixelType(XemaPixelType& type);

	bool getMinExposure(float& val);


	bool streamOn();
	bool streamOff();

	void LINE0_IN();
	void LINE1_OUT(bool xOn);
	void LINE1_OUT_HDR(bool xOn);
	void SDA_OUTPUT();
	void SDA_ON();
	void SDA_OFF();
	void SDA_INPUT();
	bool SDA_READ();
	void SCL_OUTPUT();
	void SCL_ON_OFF();
	void SCL_ON();
	void SCL_OFF();

	bool set_calib_param(float* c_in);
	/**
	 * @brief 重置 `IIC`.
	 */
	void IICReset();
	/**
	 * @brief 检查 `IIC` 通讯状态.
	 * @return true: ACK; false: NACK.
	 */
	bool checkAck();
	/**
	 * @brief 获取 `IIC` NACK 状态.
	 * @return true: NACK; false: ACK.
	 */
	bool getNACKStatus();
	/**
	 * @brief 读取 MCU 版本号.
	 * @param [out] version 版本号.
	 * @return true: 获取成功; false: 获取失败.
	 */
	bool getMCUVersion(std::string& version);
	/**
	 * @brief 读取 DLP 启动状态
	 * @param [out] status 启动状态.
	 * @return true: 获取成功; false: 获取失败.
	 */
	bool getDLPStatus(bool& status);
	/**
	 * @brief 检查网络适配器设置
	 * @param [in] cameraIPv4Address 相机IP地址.
	 * @return true: 读取成功; false: 读取失败.
	 */
	bool checkNIC(const std::string& cameraIPv4Address);
private:
	//void streamOffThread();
	void trigger_one_sequence();

	void trigger_hdr_list_flush();

	/**
	 * @brief 相机从 MCU 读取数据, IIC通信.
	 * @note 相比于read(), 这个版本会向 MCU 发送buffer_size.
	 * @param [in] inner_reg   内部寄存器地址.
	 * @param [in] buffer      接收的数据.
	 * @param [in] buffer_size 接收的数据大小.
	 * @return 
	 */
	size_t read2(char inner_reg, void* buffer, size_t buffer_size);

	size_t read(char inner_reg, void* buffer, size_t buffer_size);
	/**
	 * @brief 相机向 MCU 发送数据, IIC通信.
	 * @param [in] inner_reg   内部寄存器地址.
	 * @param [in] buffer      发送的数据.
	 * @param [in] buffer_size 发送的数据大小.
	 * @return UNKOWN; 1000: MCU NACK.
	 */
	size_t write(char inner_reg, void* buffer, size_t buffer_size);

private:
	float min_camera_exposure_;
	float camera_exposure;
	GX_DEV_HANDLE hDevice_;
	GX_FRAME_DATA  pFrameData;
	std::mutex operate_mutex_;
	bool line_1_inited_;
	int color;
	int pixel_format_;
	bool m_addNACK;     /*!< 记录地址码NACK */
	bool m_regNACK;     /*!< 记录寄存器NACK */
	bool m_bufNACK;     /*!< 记录数据NACK */
	int m_bufNACKCount; /*!< 记录数据NACK次数 */

	NetworkAdapterManager m_networkAdapterMgr; /*!< 网络适配器管理器 */
};

