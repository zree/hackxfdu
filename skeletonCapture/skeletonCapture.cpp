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
#include <Windows.h>     
#include <ctime>    
#include <cassert>    
#include <process.h>   
#pragma comment(lib, "ws2_32.lib")  

using namespace std;
using namespace cv;
using namespace rapidjson;

void draw(Mat &img, Joint & r_1, Joint & r_2, ICoordinateMapper * myMapper);
void mydraw(Mat & img, CameraSpacePoint & position_1, CameraSpacePoint & position_2);

#define _DDDEBUG  

static int CTRL = 0;

template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
	if (pInterfaceToRelease != NULL) {
		pInterfaceToRelease->Release();
		pInterfaceToRelease = NULL;
	}
}

// 记录手势当前位置    
enum GesturePos { //   
	NonePos = 0,
	Left,
	Right,
	Neutral
};

// 判断识别状态    
enum DetectionState {
	NoneState = 0,
	Success,
	Failed,
	InProgress
};

// 判断手势需要的数据    
struct DataState {
	GesturePos Pos;     // 每个人的左右手    
	DetectionState State; // 状态  
	int times;
	time_t timestamp;
	void Reset() // 状态的重置  
	{
		Pos = GesturePos::NonePos;
		State = DetectionState::NoneState;
		times = 0;
		timestamp = 0;
	}
};

// 完成手势判断逻辑功能    
class GestureDetection {
public:
	GestureDetection(float neutral_threshold, int times, double difftimes)
		: neutral_threshold(neutral_threshold)
		, times(times)
		, difftimes(difftimes)
		, left_hand(0)
		, right_hand(1)
	{
		for (int i = 0; i < 1; i++)
		{
			wave_datas[i][left_hand].Reset();
			wave_datas[i][right_hand].Reset();
		}
	}
	// 功能：循环接收骨骼数据，如果识别出为挥手动作则输出：success，    
	// 识别失败输出：failed，    
	void Update(IBody * frame)
	{

		if (NULL == frame)
			return;
		for (int i = 0; i < 1; i++)
		{
			JudgeState(frame, wave_datas[i][right_hand], true);
		}
	}
private:
	DataState wave_datas[1][2];        // 记录每个人，每只手的状态    
	const int left_hand;                            // 左手 ID    
	const int right_hand;                           // 右手 ID    
													// 中间位置阀值：在该范围内的都认为手在中间位置（相对于肘部的 x 坐标）    
	const float neutral_threshold;
	// 挥手次数阀值，达到该次数认为是挥手    
	const int times;
	// 时间限制，如果超过该时间差依然识别不出挥手动作则认为识别失败    
	const double difftimes;

	// 判断当前的状态成功输出：success，并生成事件：DetectionEvent     
	// 失败输出 failed，供 UpDate 函数调用    
	void JudgeState(IBody *n_body, DataState& data, bool isLeft = true)
	{

		Joint joints[JointType_Count]; // 定义骨骼信息  

		n_body->GetJoints(JointType::JointType_Count, joints);  // 获取骨骼信息节点  

		int elbow = JointType_ElbowRight;

		int hand = JointType_HandRight;

		if (!IsSkeletonTrackedWell(n_body, isLeft))  //  如果手部的位置在肘部之上  则认为为真  
		{
			if (data.State == InProgress)
			{
#ifdef _DDEBUG  
				cout << "not a well skeleton, detection failed!\n";
#endif    
				data.Reset();
				return;
			}
		}

		float curpos = joints[hand].Position.X;
		float center = joints[elbow].Position.X;  //  得到人手部和肘部的X坐标的位置  都是右手  

		if (!IsNeutral(curpos, center))  //  如果手部不是在中立的位置  
		{
			if (data.Pos == NonePos)
			{
#ifdef _DDEBUG    
				cout << "found!\n";
#endif    

				data.times++;


				if (get_length(curpos, center) == -1)
				{
					data.Pos = Left;
				}
				else if (get_length(curpos, center) == 1)
				{
					data.Pos = Right;
				}
				cout << "times:" << data.times << endl;
				if (data.Pos == Left)
				{
					cout << "left !\n";
				}
				else if (data.Pos == Right)
				{
					cout << "right!\n";
				}
				else
					cout << "you can't see me!\n";

				data.State = InProgress;
				data.timestamp = time(NULL);

			}

			else if (((data.Pos == Left) && get_length(curpos, center) == 1) || ((data.Pos == Right) && get_length(curpos, center) == -1))  // 左摆找右摆  右摆找左摆  
			{

				assert(data.State == InProgress);
				data.times++;
				data.Pos = (data.Pos == Left) ? Right : Left;
#ifdef _DDDEBUG    
				cout << "times:" << data.times << endl;
				if (data.Pos == Left)
				{
					cout << "left !\n";
				}
				else if (data.Pos == Right)
				{
					cout << "right!\n";
				}
				else
					cout << "you can't see me!\n";
#endif    
				if (data.times >= times)
				{
#ifdef _DDDEBUG    
					cout << "success!\n";
#endif    
					CTRL = 1;
					data.Reset();
				}
				else if (difftime(time(NULL), data.timestamp) > difftimes)
				{
#ifdef _DDDEBUG    
					cout << "time out, detection failed!\n";
					cout << "data.times : " << data.times << endl;
#endif    
					data.Reset();
				}
			}
		}

	}

	bool IsLeftSide(float curpos, float center)
	{
		int i = 0;
		i = get_length(curpos, center);
		if (i == -1)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	bool IsRightSide(float curpos, float center)
	{
		int i = 0;
		i = get_length(curpos, center);
		if (i == 1)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	bool IsNeutral(float curpos, float center) // 参数分别为手部的位置和肘部的位置  判断是否是中立的状态  
	{
		int i = 0;
		i = get_length(curpos, center);
		if (i == 0)
		{
			return true;  // 是中立的状态  
		}
		else
		{
			return false;
		}
	}

	int get_length(float shou, float zhou)   //  在这里定义  右边是 1 左边是 -1 中间是 0  
	{
		if (shou >= 0 && zhou >= 0)
		{
			if ((shou - zhou) > neutral_threshold)
			{
				return 1; // 在右边  
			}
			else if ((shou - zhou) < neutral_threshold || (zhou - shou) > -neutral_threshold)
			{
				return 0;  // 中立  
			}
			else
			{
				return -1;   // 左边  
			}
		}
		else if (shou >= 0 && zhou <= 0)
		{
			if ((shou + (-zhou)) > neutral_threshold)
			{
				return 1; // 右边  
			}
			else
			{
				return 0; // 中立  
			}
		}
		else if (shou <= 0 && zhou >= 0)
		{
			if (((-shou) + zhou) > neutral_threshold)
			{
				return -1; // 左边  
			}
			else
			{
				return 0; // 中立  
			}
		}
		else
		{
			if ((-shou) >= (-zhou))
			{
				if (((-shou) + zhou) > neutral_threshold)
				{
					return -1; // 左边  
				}
				else if (((-shou) + zhou) < neutral_threshold)
				{
					return 0; // 中立  
				}
				else
				{
					return 1;  // 右边  
				}
			}
			else
			{
				if (((-zhou) + shou) > neutral_threshold)
				{
					return 1; // 右边  
				}
				else
				{
					return 0; // 中立  
				}
			}
		}

	}

	// 判断骨骼追踪情况：包括骨骼追踪完好且手部位置在肘上面    
	bool IsSkeletonTrackedWell(IBody * n_body, bool isLeft = true)
	{
		Joint joints[JointType_Count];
		n_body->GetJoints(JointType::JointType_Count, joints);

		int elbow = JointType_ElbowRight;

		int hand = JointType_HandRight;


		if (joints[hand].Position.Y > joints[elbow].Position.Y)
		{
			return true;
		}
		else
		{
			return false;
		}


	}
};

int beginBehave()
{
	IKinectSensor *kinect = NULL;
	HRESULT hr = S_OK;
	hr = GetDefaultKinectSensor(&kinect);  //  得到默认的设备  

	if (FAILED(hr) || kinect == NULL)
	{
		cout << "创建 sensor 失败\n";
		return -1;
	}
	if (kinect->Open() != S_OK) // 是否打开成功  
	{
		cout << "Kinect sensor 没准备好\n";
		return -1;
	}


	IBodyFrameSource *bady = nullptr;  // 获取源  
	hr = kinect->get_BodyFrameSource(&bady);

	IBodyFrameReader *pBodyReader;

	hr = bady->OpenReader(&pBodyReader); // 打开获取骨骼信息的  Reader  
	if (FAILED(hr)) {
		std::cerr << "Error : IBodyFrameSource::OpenReader()" << std::endl;
		return -1;
	}
	cout << "开始检测\n";

	GestureDetection gesture_detection(0.05, 3, 5);

	while (1)
	{
		IBodyFrame* pBodyFrame = nullptr;
		hr = pBodyReader->AcquireLatestFrame(&pBodyFrame);

		if (SUCCEEDED(hr)) {
			IBody* pBody[BODY_COUNT] = { 0 }; // 默认的是 6 个骨骼 ，初始化所有的骨骼信息  
											  //更新骨骼数据    
			hr = pBodyFrame->GetAndRefreshBodyData(BODY_COUNT, pBody); // 刷新骨骼信息（6个）  
			if (SUCCEEDED(hr))
			{
				BOOLEAN bTracked = false;

				for (int i = 0; i < 6; i++)
				{
					hr = pBody[i]->get_IsTracked(&bTracked); // 检查是否被追踪  

					if (SUCCEEDED(hr) && bTracked)
					{

						gesture_detection.Update(pBody[i]);
					}
				}
			}
			for (int count = 0; count < BODY_COUNT; count++) {
				SafeRelease(pBody[count]);
			}
		}
		if (CTRL == 1)
		{
			return 1;  //  识别成功以后  跳出识别程序  
		}
		SafeRelease(pBodyFrame);  // 别忘了释放  
	}

	// kinect->Close();  // 关闭设备  
	// system("pause");
	return 0;
}

void *curl(void *args) {
	const string add = "https://gateway-a.watsonplatform.net/visual-recognition/api/v3/detect_faces?api_key=11f6badd6a3924e3f872efb44519b5932e2311b1^&version=2016-05-20";
	string cmd = "curl -X POST --form \"images_file=@fruit.jpg\" " + add + " > here.json";
	char *p = &cmd[0];
	system(p);
	return NULL;
}

void curl_another_server() {
	const string add = "http://10.141.208.28:26789/getjson";
	string cmd = "curl " + add + " > get_poses.json";
	char *p = &cmd[0];
	system(p);
	Sleep(0.5);
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
{/*
	const int BUF_SIZE = 1024;

	WSADATA         wsd;            //WSADATA变量
	SOCKET          sHost;          //服务器套接字
	SOCKADDR_IN     servAddr;       //服务器地址
	char            buf[BUF_SIZE];  //接收数据缓冲区
	char            bufRecv[BUF_SIZE] = { '0' };
	int             retVal;         //返回值

	//初始化套结字动态库
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
	{
		cout << "WSAStartup failed!" << endl;
		return -1;
	}

	//创建套接字
	sHost = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == sHost)
	{
		cout << "socket failed!" << endl;
		WSACleanup();//释放套接字资源
		return  -1;
	}

	//设置服务器地址
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = inet_addr("10.131.245.142");
	servAddr.sin_port = htons(26789);
	int nServAddlen = sizeof(servAddr);

	//连接服务器
	retVal = connect(sHost, (LPSOCKADDR)&servAddr, sizeof(servAddr));
	if (SOCKET_ERROR == retVal)
	{
		cout << "connect failed!" << endl;
		closesocket(sHost); //关闭套接字
		WSACleanup();       //释放套接字资源
		return -1;
	}


	while (1) {
		cout << "erere" << endl;
		recv(sHost, bufRecv, BUF_SIZE, 0);
		cout << bufRecv << endl;
	}
	//*/

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
	
	vector<string> paths_17500;
	for (int i = 0; i < 700; i++) {
		sprintf_s(path, "hiphop\\hiphop_0000000%05d_keypoints.json", i+17500);
		paths_17500.push_back(string(path));
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

	int pose_cnt = 30;

	filename = "1.mp4";
	VideoCapture cap(filename);
	Mat fram;
	while (1) {
		cap >> fram;
		if (fram.data==nullptr) {
			cap.set(CV_CAP_PROP_POS_FRAMES, 1);
			cap >> fram;
		}
		cv::imshow("HipHop", fram);
		if (waitKey(30) == VK_RETURN) {
			break;
		}
	}

	filename = "2.mp4";
	VideoCapture cap2(filename);
	Mat fra2;
	int single_or_multi = 0;
	while (1) {
		cap2 >> fra2;
		if (fra2.data == nullptr) {
			cap2.set(CV_CAP_PROP_POS_FRAMES, 1);
			cap2 >> fra2;
		}
		cv::imshow("HipHop", fra2);
		if (waitKey(30) == VK_RETURN) {
			single_or_multi = 0;
			break;
		}
		else if (waitKey(30) == VK_SPACE) {
			single_or_multi = 1;
			break;
		}
	}

	filename = "3.mp4";
	VideoCapture cap3(filename);
	Mat fra3;
	while (1) {
		cap3 >> fra3;
		if (fra3.data == nullptr) {
			cap3.set(CV_CAP_PROP_POS_FRAMES, 1);
			cap3 >> fra3;
		}
		cv::imshow("HipHop", fra3);
		if (waitKey(30) == VK_RETURN) {
			break;
		}
	}

	if (single_or_multi == 0) {
		while (true)
		{
			while (myColorReader->AcquireLatestFrame(&myColorFrame) != S_OK);
			myColorFrame->CopyConvertedFrameDataToArray(colorHeight * colorWidth * 4, original.data, ColorImageFormat_Bgra);

			cout << cnt << endl;

			if (cnt + 10 == 9000) {
				capture.set(CV_CAP_PROP_POS_FRAMES, 1);
				cnt = 0;
				//continue;
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
				cout << "came here" << endl;
				//recv(sHost, bufRecv, BUF_SIZE, 0);
				//document.Parse<0>(bufRecv);
				//cout << bufRecv << endl;

				document.Parse<0>(read_json(paths[cnt%imagecnt]).c_str());
				if (document.IsNull()) {
					cout << "there must be some error" << endl;
					system("pause");
					return -1;
				}

				cnt++;
				Value &people = document["people"];
				Value myarray = people.GetArray();
				Value &data = myarray[0];
				Value pose_keypoints = data["pose_keypoints"].GetArray();
				for (size_t i = 0; i < 18; i++) {
					position[i].X = pose_keypoints[i * 3].GetFloat() * 4;
					position[i].Y = pose_keypoints[i * 3 + 1].GetFloat() * 3 - 200;
					position[i].Z = pose_keypoints[i * 3 + 2].GetFloat() * 3;
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
	}
	else {
		while (true)
		{
			while (myColorReader->AcquireLatestFrame(&myColorFrame) != S_OK);
			myColorFrame->CopyConvertedFrameDataToArray(colorHeight * colorWidth * 4, original.data, ColorImageFormat_Bgra);
			cout << cnt << endl;

			if (cnt + 10 == 9000) {
				capture.set(CV_CAP_PROP_POS_FRAMES, 1);
				cnt = 0;
				//continue;
			}

			//capture >> frame;

			Mat copy(colorHeight, colorWidth, CV_8UC3);
			bg.copyTo(copy(Rect(0, 0, bg.cols, bg.rows)));
			//frame.copyTo(copy(Rect(160, 250, frame.cols, frame.rows)));

			while (myBodyReader->AcquireLatestFrame(&myBodyFrame) != S_OK);

			IBody ** myBodyArr = new IBody *[myBodyCount];
			for (int i = 0; i < myBodyCount; i++)
			{
				myBodyArr[i] = nullptr;
			}

			if (myBodyFrame->GetAndRefreshBodyData(myBodyCount, myBodyArr) == S_OK)
			{
				cout << "came here" << endl;
				//recv(sHost, bufRecv, BUF_SIZE, 0);
				//document.Parse<0>(bufRecv);
				//cout << bufRecv << endl;
				document.Parse<0>(read_json(paths[cnt%imagecnt]).c_str());
				if (document.IsNull()) {
					cout << "there must be some error" << endl;
					system("pause");
					return -1;
				}

				cnt++;
				Value &people = document["people"];
				Value myarray = people.GetArray();
				Value &data = myarray[0];
				Value pose_keypoints = data["pose_keypoints"].GetArray();
				for (size_t i = 0; i < 18; i++) {
					position[i].X = pose_keypoints[i * 3].GetFloat() * 2;
					position[i].Y = pose_keypoints[i * 3 + 1].GetFloat() * 1;
					position[i].Z = pose_keypoints[i * 3 + 2].GetFloat();
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
	}
	myMapper->Release();

	myDescription->Release();
	myColorReader->Release();
	myColorSource->Release();

	myBodyReader->Release();
	myBodySource->Release();
	m_pKinect->Close();
	m_pKinect->Release();

//	closesocket(sHost); //关闭套接字  
//	WSACleanup();       //释放套接字资源 
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