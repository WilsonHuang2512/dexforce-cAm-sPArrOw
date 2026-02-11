using CameraCsharp;
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.IO;

namespace example
{
    class Program
    {

        static void Main()
        {

            //string cameraId = args[1];
            CameraCls camera = new CameraCls();

            string cameraId = "127.0.0.1";
            int ret_code;
            ret_code = camera.DfConnect_Csharp(cameraId);

            int width=0 ;
            int height=0;
            int channels=0 ;
            if (ret_code == 0)
            {

                ret_code = camera.GetCameraResolution_Csharp(out width, out height);
                Console.WriteLine("width:{0}", width);
                Console.WriteLine("height:{0}", height);
                ret_code = camera.DfGetCameraChannels_Csharp(out channels);
                Console.WriteLine("channels:{0}", channels);
            }
            else
            {
                Console.WriteLine("Connect camera Error!!");
            }

            CalibrationParam calibrationParam;
            ret_code = camera.DfGetCalibrationParam_Csharp(out calibrationParam);
            if (ret_code == 0)
            {
                Console.WriteLine("内参：");
                for (int i = 0; i < calibrationParam.intrinsic.Length; i++)
                {
                    Console.WriteLine(calibrationParam.intrinsic[i]);
                }
                Console.WriteLine("外参：");
                for (int i = 0; i < calibrationParam.extrinsic.Length; i++)
                {
                    Console.WriteLine(calibrationParam.extrinsic[i]);
                }
                Console.WriteLine("畸变：");
                for (int i = 0; i < calibrationParam.distortion.Length; i++)
                {
                    Console.WriteLine(calibrationParam.distortion[i]);
                }
            }
            else
            {
                Console.WriteLine("Get Calibration Data Error!!");
            }

            //分配内存保存采集结果

            StringBuilder timestamp = new StringBuilder(30);

            byte[] brightnessArray = new byte[width * height];
            IntPtr brightnessPtr = Marshal.AllocHGlobal(brightnessArray.Length);


            float[] depthArray = new float[width * height];
            IntPtr depthPtr = Marshal.AllocHGlobal(depthArray.Length * sizeof(float));

            float[] point_cloud_Array = new float[width * height * 3];
            IntPtr point_cloud_Ptr = Marshal.AllocHGlobal(depthArray.Length * sizeof(float) * 3);

            StringBuilder config_json = new StringBuilder(20480);
            StringBuilder status_json = new StringBuilder(20480);
            int num;
            string path = "3.json";
            camera.DfReadJson_Csharp(config_json, path);
            camera.DfSetParamJson_Csharp(config_json, status_json, out num);

            ret_code = camera.DfCaptureData_Csharp(num, timestamp);
          
            if (ret_code == 0)
            {
                //获取亮度图数据
                ret_code = camera.DfGetBrightnessData_Csharp(brightnessPtr);
                //获取深度图数据
                ret_code = camera.DfGetDepthDataFloat_Csharp(depthPtr);
                //获取点云数据
                ret_code = camera.DfGetPointcloudData_Csharp(point_cloud_Ptr);

                string ply = "1.ply";
                string pcd = "1.pcd";
                camera.savePointcloudToPly_Csharp(point_cloud_Ptr, brightnessPtr, 1, ply);
                camera.savePointcloudToPcd_Csharp(point_cloud_Ptr, brightnessPtr, 1, pcd);

                Marshal.Copy(brightnessPtr, brightnessArray, 0, brightnessArray.Length);
                Marshal.Copy(depthPtr, depthArray, 0, depthArray.Length);
                Marshal.Copy(point_cloud_Ptr, point_cloud_Array, 0, point_cloud_Array.Length);
                Marshal.FreeHGlobal(brightnessPtr);
                Marshal.FreeHGlobal(depthPtr);
                Marshal.FreeHGlobal(point_cloud_Ptr);

            }
            else
            {
                Console.WriteLine("Capture Data Error!!");
            }

            ret_code = camera.DfDisconnect_Csharp(cameraId);
            if (ret_code == 0)
            {
                Console.WriteLine("Camera Disconnect!!");
                Console.ReadKey();
            }


        }

    }
}