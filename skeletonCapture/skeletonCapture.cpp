#include "stdafx.h"
#include <kinect.h>
#include <iostream>
#include <iomanip>
#include <fstream>  
#include <vector>
#include <opencv2\imgproc.hpp>
#include <opencv2\calib3d.hpp>
#include "rapidjson\rapidjson.h"
#include "rapidjson\document.h"
#include "rapidjson\writer.h"
#include "rapidjson\stringbuffer.h"
#include <pthread.h>

using namespace std;
using namespace cv;
using namespace rapidjson;

void draw(Mat &img, Joint & r_1, Joint & r_2, ICoordinateMapper * myMapper);
void mydraw(Mat & img, CameraSpacePoint & position_1, CameraSpacePoint & position_2);


void *curl(void *args) {
	const string add = "https://gateway-a.watsonplatform.net/visual-recognition/api/v3/detect_faces?api_key=11f6badd6a3924e3f872efb44519b5932e2311b1^&version=2016-05-20";
	string cmd = "curl -X POST --form \"images_file=@fruit.jpg\" " + add + " > here.json";
	char *p = &cmd[0];
	system(p);
	return NULL;
}

string read_json(string path) {
	ifstream fin;
	fin.open(path);
	string str = "";
	string str_in = "";
	while (getline(fin, str))   
	{
		str_in += str + '\n';
	}
	return str_in;
}

Joint myJointArr[JointType_Count];
CameraSpacePoint position[18];
int imagecnt = 9000;

int main() 
{ 
	Mat frame;
	string filename = "hiphop.mp4";
	VideoCapture capture(filename);
	Document document;

	vector<string> paths;
	char path[60];
	for (int i = 0; i < imagecnt; i++) {
		sprintf_s(path, "hiphop\\hiphop_0000000%05d_keypoints.json", i);
		paths.push_back(string(path));
	}

	IKinectSensor *m_pKinect = nullptr;
	// 查找当前默认Kinect  
	HRESULT hr = GetDefaultKinectSensor(&m_pKinect);
	m_pKinect->Open();

	IColorFrameSource * myColorSource = nullptr;
	m_pKinect->get_ColorFrameSource(&myColorSource);
	IColorFrameReader * myColorReader = nullptr;
	myColorSource->OpenReader(&myColorReader);
	int colorHeight = 0;
	int colorWidth = 0;
	IFrameDescription * myDescription = nullptr;
	myColorSource->get_FrameDescription(&myDescription);
	myDescription->get_Height(&colorHeight);
	myDescription->get_Width(&colorWidth);
	IColorFrame * myColorFrame = nullptr;
	IBodyFrameSource * myBodySource = nullptr;

	m_pKinect->get_BodyFrameSource(&myBodySource);
	IBodyFrameReader * myBodyReader = nullptr;
	myBodySource->OpenReader(&myBodyReader);
	int myBodyCount = 0;
	myBodySource->get_BodyCount(&myBodyCount);
	IBodyFrame * myBodyFrame = nullptr;
	ICoordinateMapper * myMapper = nullptr;
	m_pKinect->get_CoordinateMapper(&myMapper);

	Mat original(colorHeight, colorWidth, CV_8UC4);

	int cnt = 0;
	BOOLEAN firsttime = true;
	//double pos = 17500;
	//capture.set(CV_CAP_PROP_POS_FRAMES, pos);
	capture >> frame;

	Mat bg = imread("bg.jpg");

	while (true)
	{
		while (myColorReader->AcquireLatestFrame(&myColorFrame) != S_OK);
		myColorFrame->CopyConvertedFrameDataToArray(colorHeight * colorWidth * 4, original.data, ColorImageFormat_Bgra);
		if (firsttime) {
			firsttime = false;
			filename = "fruit.jpg";
			Mat dst(480, 270, CV_8UC4);
			cv::resize(original, dst, Size(480, 270));
			cv::imwrite(filename, dst);
			pthread_t tid;
			pthread_create(&tid, NULL, curl, NULL);
			//curl(filename);
		}
		capture >> frame;
		
		Mat copy(colorHeight, colorWidth, CV_8UC3);
		bg.copyTo(copy(Rect(0, 0, bg.cols, bg.rows)));
		frame.copyTo(copy(Rect(160, 250, frame.cols, frame.rows)));

		while (myBodyReader->AcquireLatestFrame(&myBodyFrame) != S_OK);

		IBody ** myBodyArr = new IBody *[myBodyCount];
		for (int i = 0; i < myBodyCount; i++)
		{
			myBodyArr[i] = nullptr;
		}

		if (myBodyFrame->GetAndRefreshBodyData(myBodyCount, myBodyArr) == S_OK)
		{
			document.Parse<0>(read_json(paths[(cnt++)%imagecnt]).c_str());
			Value &people = document["people"];
			Value myarray = people.GetArray();
			Value &data = myarray[0];
			Value pose_keypoints = data["pose_keypoints"].GetArray();
			for (size_t i = 0; i < 18; i++) {
				position[i].X = pose_keypoints[i * 3].GetFloat() * 4;
				position[i].Y = pose_keypoints[i * 3 + 1].GetFloat() * 3 - 200;
				position[i].Z = pose_keypoints[i * 3 + 2].GetFloat() * 3;
			}
			cout << cnt << endl;
			if (cnt + 1 == 9000) {
				capture.set(CV_CAP_PROP_POS_FRAMES, 1);
				cnt = 0;
				continue;
			}

			// mydraw from here
			mydraw(copy, position[14], position[16]);
			mydraw(copy, position[15], position[17]);
			mydraw(copy, position[0], position[14]);
			mydraw(copy, position[0], position[15]);
			mydraw(copy, position[0], position[1]);
			mydraw(copy, position[1], position[2]);
			mydraw(copy, position[1], position[5]);
			mydraw(copy, position[1], position[8]);
			mydraw(copy, position[1], position[11]);
			mydraw(copy, position[2], position[3]);
			mydraw(copy, position[5], position[6]);
			mydraw(copy, position[3], position[4]);
			mydraw(copy, position[6], position[7]);
			mydraw(copy, position[8], position[9]);
			mydraw(copy, position[11], position[12]);
			mydraw(copy, position[9], position[10]);
			mydraw(copy, position[12], position[13]);


			for (int i = 0; i < myBodyCount; i++)
			{
				BOOLEAN result = false;
				if (myBodyArr[i]->get_IsTracked(&result) == S_OK && result)
				{
					if (myBodyArr[i]->GetJoints(JointType_Count, myJointArr) == S_OK)
					{
						draw(copy, myJointArr[JointType_Head], myJointArr[JointType_Neck], myMapper);
						draw(copy, myJointArr[JointType_Neck], myJointArr[JointType_SpineShoulder], myMapper);

						draw(copy, myJointArr[JointType_SpineShoulder], myJointArr[JointType_ShoulderLeft], myMapper);
						draw(copy, myJointArr[JointType_SpineShoulder], myJointArr[JointType_SpineMid], myMapper);
						draw(copy, myJointArr[JointType_SpineShoulder], myJointArr[JointType_ShoulderRight], myMapper);

						draw(copy, myJointArr[JointType_ShoulderLeft], myJointArr[JointType_ElbowLeft], myMapper);
						draw(copy, myJointArr[JointType_SpineMid], myJointArr[JointType_SpineBase], myMapper);
						draw(copy, myJointArr[JointType_ShoulderRight], myJointArr[JointType_ElbowRight], myMapper);

						draw(copy, myJointArr[JointType_ElbowLeft], myJointArr[JointType_WristLeft], myMapper);
						draw(copy, myJointArr[JointType_SpineBase], myJointArr[JointType_HipLeft], myMapper);
						draw(copy, myJointArr[JointType_SpineBase], myJointArr[JointType_HipRight], myMapper);
						draw(copy, myJointArr[JointType_ElbowRight], myJointArr[JointType_WristRight], myMapper);

						draw(copy, myJointArr[JointType_WristLeft], myJointArr[JointType_ThumbLeft], myMapper);
						draw(copy, myJointArr[JointType_WristLeft], myJointArr[JointType_HandLeft], myMapper);
						draw(copy, myJointArr[JointType_HipLeft], myJointArr[JointType_KneeLeft], myMapper);
						draw(copy, myJointArr[JointType_HipRight], myJointArr[JointType_KneeRight], myMapper);
						draw(copy, myJointArr[JointType_WristRight], myJointArr[JointType_ThumbRight], myMapper);
						draw(copy, myJointArr[JointType_WristRight], myJointArr[JointType_HandRight], myMapper);

						draw(copy, myJointArr[JointType_HandLeft], myJointArr[JointType_HandTipLeft], myMapper);
						draw(copy, myJointArr[JointType_KneeLeft], myJointArr[JointType_FootLeft], myMapper);
						draw(copy, myJointArr[JointType_KneeRight], myJointArr[JointType_FootRight], myMapper);
						draw(copy, myJointArr[JointType_HandRight], myJointArr[JointType_HandTipRight], myMapper);
					}
				}
			}
		}

		delete[] myBodyArr;
		myBodyFrame->Release();
		myColorFrame->Release();

		cv::imshow("HipHop", copy);
		if (waitKey(30) == VK_ESCAPE)
			break;
	}

	myMapper->Release();

	myDescription->Release();
	myColorReader->Release();
	myColorSource->Release();

	myBodyReader->Release();
	myBodySource->Release();
	m_pKinect->Close();
	m_pKinect->Release();
	//system("pause");
	return 0;
}

void mydraw(Mat & img, CameraSpacePoint & position_1, CameraSpacePoint & position_2) 
{
	if (position_1.Z <= 0 || position_2.Z <= 0)
		return;

	Point p_1(position_1.X, position_1.Y);
	Point p_2(position_2.X, position_2.Y);

	line(img, p_1, p_2, Scalar(0, 255, 0), 5);
	circle(img, p_1, 10, Scalar(255, 0, 0), -1);
	circle(img, p_2, 10, Scalar(255, 0, 0), -1);
}

void draw(Mat & img, Joint & r_1, Joint & r_2, ICoordinateMapper * myMapper)
{
	//用两个关节点来做线段的两端，并且进行状态过滤  
	if (r_1.TrackingState == TrackingState_Tracked && r_2.TrackingState == TrackingState_Tracked)
	{
		ColorSpacePoint t_point;    //要把关节点用的摄像机坐标下的点转换成彩色空间的点 
		myMapper->MapCameraPointToColorSpace(r_1.Position, &t_point);
		Point p_1(t_point.X, t_point.Y);
		myMapper->MapCameraPointToColorSpace(r_2.Position, &t_point);
		Point p_2(t_point.X, t_point.Y);

		line(img, p_1, p_2, Scalar(0, 0, 255), 5);
		circle(img, p_1, 10, Scalar(255, 255, 255), -1);
		circle(img, p_2, 10, Scalar(255, 255, 255), -1);
	}
}