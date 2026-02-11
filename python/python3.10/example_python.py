import camera
import numpy as np
import cv2

#连接相机
ip='127.0.0.1'
ret=camera.DfConnect(ip)

width=np.zeros(1,dtype=np.int32)
height=np.zeros(1,dtype=np.int32)
cannels=np.zeros(1,dtype=np.int32)
camera.DfGetCameraResolution(width,height)
camera.DfGetCameraChannels(cannels)
print('通道数',cannels[0])
print("height:",height[0])
print("width:", width[0])

width=int(width[0])
height=int(height[0])

cfg = camera.DfReadJson("3.json")

status, maxnum = camera.DfSetParamJson(cfg)

camera.DfCaptureData(maxnum,"time")

#数组创建
bright_data = np.zeros((height, width), dtype=np.uint8)
depth_data=np.zeros((height,width),dtype=np.float32)
pointcloud_data = np.zeros((width * height * 3,), dtype=np.float32)

camera.DfGetBrightnessData(bright_data)

camera.DfGetDepthDataFloat(depth_data)

camera.DfGetPointcloudData(pointcloud_data)

camera.savePointcloudToPcd(pointcloud_data, bright_data, 1, "test.pcd")

camera.savePointcloudToPly(pointcloud_data, bright_data, 1, "test.ply")

#断开相机
finish=camera.DfDisconnect(ip)
if(finish==0):
   print('相机已断开')
