
/*
#define DEBUG

#include "camshiftkalman.h"
#include "util.h"

#include <opencv2/core/core.hpp>
#include <Windows.h>

Rect selection;
Point origin;
Mat frame;

bool trackObject = false;
bool selectObject;

void onMouse( int event, int x, int y, int, void* )
{
if( selectObject )
{
selection.x = MIN(x, origin.x);
selection.y = MIN(y, origin.y);
selection.width = std::abs(x - origin.x);
selection.height = std::abs(y - origin.y);

selection &= Rect(0, 0, frame.cols, frame.rows);
}

switch( event )
{
case CV_EVENT_LBUTTONDOWN:
origin = Point(x,y);
selection = Rect(x,y,0,0);
selectObject = true;
break;
case CV_EVENT_LBUTTONUP:
selectObject = false;
if( selection.width > 0 && selection.height > 0 )
trackObject = true;
break;
default :
break;
}

}

int main(int argc, const char* argv[])
{
const char* args = {
"{ m | useMouse  | true | how to choose object to track}"
"{ v | videoName |      | the video to track}"
"{ t | featureType | 0  | 0 -- HUE 1 -- SATURATION_HUE 2 -- LBP_HUE 3 -- LBP_SATURATION_HUE}"
};

//CommandLineParser parser(argc, argv, args);
//bool isUseMouse = parser.get<bool>("useMouse");
bool isUseMouse = true;

Rect trackWindow;

string videoName;
//if(isUseMouse)
//    videoName = parser.get<string>("videoName");
if(1)
{ }
else
{

//   * read config parameters from file

string configFile = "config.yaml";
readXML(configFile, videoName, trackWindow);
}

featureType type;
//    int num = parser.get<int>("featureType");
int num = 0;
switch(num){
case 0 :
type = HUE;
break;
case 1 :
type = SATURATION_HUE;
break;
case 2 :
type = LBP_HUE;
break;
case 3 :
type = LBP_SATURATION_HUE;
break;
default :
cerr << "ERROR : feature type can only be chosed among 0,1,2,3" << endl;
return -1;
}

VideoCapture video(1);
//video.open(videoName);
Sleep(2000);
if(!video.isOpened()){
cerr << "open " << videoName << " error" << endl;
cerr << "current parameters : " << endl;
//        parser.printParams();
return -1;
}

Mat image;
if(isUseMouse){
string winName = "choose_windows";
namedWindow(winName, WINDOW_AUTOSIZE);

setMouseCallback(winName, onMouse, 0);

double interval = 1.0/video.get(CV_CAP_PROP_FPS);
while(true)
{
if(trackObject)
break;

video.read(frame);
if(frame.empty())
return -1;

frame.copyTo(image);
if( selectObject && selection.width > 0 && selection.height > 0 )
{
Mat roi(frame, selection);
bitwise_not(roi, roi);
}

imshow(winName, frame);
int key = waitKey(int(interval*1000));
if(key == 27)
return -1;
}

trackWindow = selection;

destroyWindow(winName);
}
else
{
video.read(image);
}

//
do tracking
//
double n = video.get(CV_CAP_PROP_POS_FRAMES);
camShiftKalman tracker(videoName, n, image, trackWindow, type);

tracker.extractTargetModel();
tracker.track();

return 0;
}

*/
#include <cv.h>
#include <highgui.h>
#include <cxcore.h>
#include <stdlib.h>
#include <stdio.h>



#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <Windows.h>
#include "SerialPort.h"

using namespace std;
using namespace cv;

#define X_RATIO 1.75
#define Y_RATIO 1.7
#define X_OFFSET 0
#define Y_OFFSET 60
#define FILTER_VAL 0.95
#define FILTER_SIZE 10

bool is_filter_enabled = false;
int ** taglist;
int ** datalist;
int * queuepointer;

int frame_counter = 0;
int tag_size = 0;

string A_Names[3] = { "desert spoon", "butter knife", "desert fork" };
string B_Names[2] = { "dinner fork", "salad fork" };
string C_Names[4] = { "soup spoon", "dinner spoon", "knife", "salad knife" };

bool is_A_on[3] = { true, true, true };
bool is_B_on[2] = { true, true };
bool is_C_on[4] = { true, true, true, true };

int level = 0;

void changelevel()
{
	//Set all element false
	for (int i = 0; i < 3; ++i)
	{
		is_A_on[i] = false;
	}
	for (int i = 0; i < 2; ++i)
	{
		is_B_on[i] = false;
	}
	for (int i = 0; i < 4; ++i)
	{
		is_C_on[i] = false;
	}

	if(level == 0)
		return;
	else if(level == 1)
	{
		// soup spoon
		is_C_on[0] = true;
	}
	else if(level == 2)
	{
		// butter knife
		is_A_on[1] = true;
	}
	else if (level == 3)
	{
		// salad fork
		is_B_on[1] = true;
		// salad knife
		is_C_on[3] = true;
	}
	else if (level == 4)
	{
		// dinner fork
		is_B_on[0] = true;
		// knife
		is_C_on[2] = true;

	}
	else if (level == 5)
	{
		// desert spoon
		is_A_on[0] = true;
		// desert fork
		is_A_on[2] = true;
	}
	else
	{
		// set all true
		for (int i = 0; i < 3; ++i)
		{
			is_A_on[i] = true;
		}
		for (int i = 0; i < 2; ++i)
		{
			is_B_on[i] = true;
		}
		for (int i = 0; i < 4; ++i)
		{
			is_C_on[i] = true;
		}
	}
}


int calX(int x) {
	return (int)(x * X_RATIO + X_OFFSET);
}
int calY(int y) {
	return (int)(y * Y_RATIO + Y_OFFSET);
}

int compare(const void * a, const void * b)
{
	return (*(int*)a - *(int*)b);
}


int median(int array[])
{
	qsort(array, FILTER_SIZE, sizeof(int), compare);
	return array[FILTER_SIZE / 2];
}

int mean(int array[])
{
	int sum = 0;
	for (int i = 0; i < FILTER_SIZE; i++)
	{
		sum += array[i];
	}
	return sum / FILTER_SIZE;
}

int filter(int * input_tag, int value)
{
	if (!is_filter_enabled)
	{
		//Init filter
		taglist = (int **)malloc(4 * 20);
		datalist = (int **)malloc(4 * 20);
		queuepointer = (int *)malloc(4 * 20);
		taglist[0] = input_tag;
		queuepointer[0] = 0;
		datalist[0] = (int *)malloc(4 * FILTER_SIZE);
		for (int i = 0; i<FILTER_SIZE; i++)
		{
			datalist[0][i] = value;
		}
		tag_size++;
		queuepointer[0]++;
		is_filter_enabled = true;
		return value;
	}
	else
	{
		int i = 0;
		//Find tag
		for (i = 0; i<tag_size; i++)
		{
			//Tag found
			if (taglist[i] == input_tag)
			{
				datalist[i][queuepointer[i]] = value;
				queuepointer[i] = (queuepointer[i] + 1) % FILTER_SIZE;
				//return median(datalist[i]);
				return mean(datalist[i]);
			}
		}

		//no tag found
		if (i == tag_size)
		{
			taglist[tag_size] = input_tag;
			queuepointer[tag_size] = 0;
			datalist[tag_size] = (int *)malloc(4 * FILTER_SIZE);
			for (int j = 0; j<FILTER_SIZE; j++)
			{
				datalist[tag_size][j] = value;
			}

			queuepointer[tag_size]++;
			tag_size++;
			return value;
		}
	}
	return 0;
}

int main()
{
	//Detect Sitting
	char buffer;
	BYTE b;
	CSerialPort cp;

	cp.OpenPort("COM9");
	cp.ConfigurePort(CBR_9600, 8, FALSE, NOPARITY, ONESTOPBIT); //포트 기본값을 설정한다.
	cp.SetCommunicationTimeouts(0, 0, 0, 0, 0); //Timeout값 설정
	while (0)
	{
		cp.ReadByte(b);
		buffer = b;
		printf("%c", buffer);
		if (buffer == 's')
			break;
	}

	IplImage * frame = NULL; // 현재 카메라 영상 저장
	IplImage * img2 = NULL;
	//frame->width = 800;
	//frame->height = 600;
	CvCapture *capture = cvCreateCameraCapture(0);// 캡쳐 장치 설정+


	cv::Mat resized;

	//  VideoCapture capture(0);
	cvNamedWindow("Rtracker", 1); // 윈도우 생성 
	cvNamedWindow("MessageShowing", 1);
	cvResizeWindow("MessageShowing", 1024, 768); //  또 다른 윈도우 생성
	Mat windowImage;
	windowImage = imread("image.png");
	if (!windowImage.data) {
		cout << "image input error" << endl;
	}

	Mat mBox = Mat::zeros(400, 400, CV_8SC3);

	//rectangle(windowImage, Point(10, 10), Point(20, 20), Scalar(0, 55, 255), 5, 5);
	//rectangle(windowImage, Point(50, 100), Point(70, 150), Scalar(0, 55, 255), 5, 5);
	//rectangle(mBox3, Point(10, 10), Point(20, 20), Scalar(0, 55, 255), 1, 4);
	//rectangle(mBox4, Point(10, 10), Point(20, 20), Scalar(0, 55, 255), 1, 4);
	//rectangle(mBox5, Point(10, 10), Point(20, 20), Scalar(0, 55, 255), 1, 4);
	//rectangle(mBox6, Point(10, 10), Point(20, 20), Scalar(0, 55, 255), 1, 4);
	//rectangle(mBox7, Point(10, 10), Point(20, 20), Scalar(0, 55, 255), 1, 4);
	//rectangle(mBox8, Point(10, 10), Point(20, 20), Scalar(0, 55, 255), 1, 4);
	//rectangle(mBox9, Point(10, 10), Point(20, 20), Scalar(0, 55, 255), 1, 4);

	imshow("MessageShowing", windowImage);


	char c;
	int x, y, B, G, R;

	int a_center_x[3] = { 0,0,0 };
	int a_center_y[3] = { 0,0,0 };
	int a_count[3] = { 1,1,1 };
	int b_center_x[2] = { 0,0 };
	int b_center_y[2] = { 0,0 };
	int b_count[2] = { 1,1 };
	int c_center_x[4] = { 0,0,0,0 };
	int c_center_y[4] = { 0,0,0,0 };
	int c_count[4] = { 1,1,1,1 };

	int prev_a_center_x[3] = { 0,0,0 };
	int prev_a_center_y[3] = { 0,0,0 };
	int prev_b_center_x[2] = { 0,0 };
	int prev_b_center_y[2] = { 0,0 };
	int prev_c_center_x[4] = { 0,0,0,0 };
	int prev_c_center_y[4] = { 0,0,0,0 };
	bool entry = false;
	//int c_center
	//int count = 0;
	changelevel();

	while (1)
	{
		if(level == 0)
			windowImage = imread("proceed.jpg");
		else if(level == 1)
			windowImage = imread("step1.jpg");
		else if(level == 2)
			windowImage = imread("step2.jpg");
		else if (level == 3)
			windowImage = imread("step3.jpg");
		else if (level == 4)
			windowImage = imread("step4.jpg");
		else if (level == 5)
			windowImage = imread("step5.jpg");
		else
			windowImage = imread("image.png");


		cvResizeWindow("Rtracker", 1024, 768);


		frame = cvQueryFrame(capture); // 카메라에게서 영상 획득
									   //cvSetImageROI(frame, cvRect(10, 15, 150, 250));
									   //img2 = cvCreateImage(cvGetSize(frame), frame->depth, frame->nChannels);
									   //cvCopy(frame, img2, NULL);

									   //cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, 1920);

									   //cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, 1080);

		if (frame == NULL) break;

		//A section
		for (int i = 0; i < frame->height / 2; i = i + 2)
			for (int j = 0; j < frame->width; j = j + 2)
			{


				CvScalar v = cvGet2D(frame, i, j);
				B = v.val[0];// 이미지에 j,i 좌표의 Blue 성분 추출
				G = v.val[1]; // 이미지에 j,i 좌표의 Green 성분 추출
				R = v.val[2]; // 이미지에 j,i 좌표의 Red 성분 추출


				if (R > 40 && G < 12 && B < 15) // 임의의 빨간색 조건 
				{
					a_count[0]++;
					x = j;
					y = i;

					a_center_x[0] = a_center_x[0] + x;
					a_center_y[0] = a_center_y[0] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(255, 0, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}

				/*
				if ( G > 80 && R< 80 && B < 30  ) // green detection
				{
				//count++;
				x = j;
				y = i;

				//  center_x = center_x + x;
				//  center_y = center_y + y;

				cvRectangle(frame, cvPoint(x, y),
				cvPoint(x + 2, y + 2), cvScalar(255, 0, 0), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}
				*/

				if (R <15 && G< 15 && B< 15)// black
				{
					a_count[1]++;
					x = j;
					y = i;

					a_center_x[1] = a_center_x[1] + x;
					a_center_y[1] = a_center_y[1] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(255, 255, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}


				if (R>60 && G<90 && G>50 && B<20)// yellow
				{
					a_count[2]++;
					x = j;
					y = i;

					a_center_x[2] = a_center_x[2] + x;
					a_center_y[2] = a_center_y[2] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(100, 0, 100), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}


			}




		for (int i = 0; i < 3; i++) {
			a_center_x[i] = a_center_x[i] / a_count[i];
			a_center_y[i] = a_center_y[i] / a_count[i];
			cvRectangle(frame, cvPoint(a_center_x[i], a_center_y[i]),
				cvPoint(a_center_x[i] + 20, a_center_y[i] + 20), cvScalar(255, 255, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .


		// if (!entry) {
		//   prev_a_center_x[i] = a_center_x[i];
		//   prev_a_center_y[i] = a_center_y[i];

		// }
		// else {
		//   prev_a_center_x[i] = ((1 - FILTER_VAL) * a_center_x[i] + (FILTER_VAL)* prev_a_center_x[i]);
		//   prev_a_center_y[i] = ((1 - FILTER_VAL) * a_center_y[i] + (FILTER_VAL)* prev_a_center_y[i]);
		// }

			prev_a_center_x[i] = filter(&a_center_x[i], a_center_x[i]);
			prev_a_center_y[i] = filter(&a_center_y[i], a_center_y[i]);

			if (is_A_on[i])
			{
				rectangle(windowImage, Point(calX(prev_a_center_x[i]), calY(prev_a_center_y[i])),
					Point(calX(prev_a_center_x[i]) + 20, calY(prev_a_center_y[i]) + 20), Scalar(0, 55, 255), 5, 5);

				putText(windowImage, A_Names[i], Point(calX(prev_a_center_x[i]), calY(prev_a_center_y[i])), CV_FONT_HERSHEY_SIMPLEX, 1, Scalar::all(255), 3, 8);
			}

			a_center_x[i] = 0;
			a_center_y[i] = 0;
			a_count[i] = 1;

		}

		//B section
		for (int i = frame->height / 2; i < frame->height; i = i + 2)
			for (int j = 0; j < frame->width / 3; j = j + 2)
			{


				CvScalar v = cvGet2D(frame, i, j);
				B = v.val[0];// 이미지에 j,i 좌표의 Blue 성분 추출
				G = v.val[1]; // 이미지에 j,i 좌표의 Green 성분 추출
				R = v.val[2]; // 이미지에 j,i 좌표의 Red 성분 추출


				if (R > 40 && G < 12 && B < 15) // 임의의 빨간색 조건 
				{
					b_count[0]++;
					x = j;
					y = i;

					b_center_x[0] = b_center_x[0] + x;
					b_center_y[0] = b_center_y[0] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(255, 0, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}


				/*
				if (R <15 && G< 15 && B< 15)// black
				{
				b_count[1]++;
				x = j;
				y = i;

				b_center_x[1] = b_center_x[1] + x;
				b_center_y[1] = b_center_y[1] + y;

				cvRectangle(frame, cvPoint(x, y),
				cvPoint(x + 2, y + 2), cvScalar(255, 255, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}


				*/
				if (R>60 && G<90 && G>50 && B<20)// yellow
				{
					b_count[1]++;
					x = j;
					y = i;

					b_center_x[1] = b_center_x[1] + x;
					b_center_y[1] = b_center_y[1] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(100, 0, 100), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}
				/*

				if (G > 80 && R< 80 && B < 30) // green detection
				{
				b_count[3]++;
				x = j;
				y = i;

				b_center_x[3] = b_center_x[3] + x;
				b_center_y[3] = b_center_y[3] + y;

				cvRectangle(frame, cvPoint(x, y),
				cvPoint(x + 2, y + 2), cvScalar(100, 0, 100), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}
				*/


			}




		for (int i = 0; i < 2; i++) {
			b_center_x[i] = b_center_x[i] / b_count[i];
			b_center_y[i] = b_center_y[i] / b_count[i];
			cvRectangle(frame, cvPoint(b_center_x[i], b_center_y[i]),
				cvPoint(b_center_x[i] + 20, b_center_y[i] + 20), cvScalar(255, 255, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .

		// if (!entry) {
		//   prev_b_center_x[i] = b_center_x[i];
		//   prev_b_center_y[i] = b_center_y[i];

		// }
		// else {
		//   prev_b_center_x[i] = ((1 - FILTER_VAL) * b_center_x[i] + (FILTER_VAL)* prev_b_center_x[i]);
		//   prev_b_center_y[i] = ((1 - FILTER_VAL) * b_center_y[i] + (FILTER_VAL)* prev_b_center_y[i]);
		// }

			prev_b_center_x[i] = filter(&b_center_x[i], b_center_x[i]);
			prev_b_center_y[i] = filter(&b_center_y[i], b_center_y[i]);

			if (is_B_on[i])
			{
				rectangle(windowImage, Point(calX(prev_b_center_x[i]), calY(prev_b_center_y[i])),
					Point(calX(prev_b_center_x[i]) + 20, calY(prev_b_center_y[i]) + 20), Scalar(0, 55, 255), 5, 5);
				putText(windowImage, B_Names[i], Point(calX(prev_b_center_x[i]), calY(prev_b_center_y[i])), CV_FONT_HERSHEY_SIMPLEX, 1, Scalar::all(255), 3, 8);

			}


			b_center_x[i] = 0;
			b_center_y[i] = 0;
			b_count[i] = 1;

		}


		//B section
		for (int i = frame->height / 2; i < frame->height; i = i + 2)
			for (int j = ((frame->width)* 2 / 3); j < frame->width; j = j + 2)
			{


				CvScalar v = cvGet2D(frame, i, j);
				B = v.val[0];// 이미지에 j,i 좌표의 Blue 성분 추출
				G = v.val[1]; // 이미지에 j,i 좌표의 Green 성분 추출
				R = v.val[2]; // 이미지에 j,i 좌표의 Red 성분 추출


				if (R > 40 && G < 12 && B < 15) // 임의의 빨간색 조건 
				{
					c_count[0]++;
					x = j;
					y = i;

					c_center_x[0] = c_center_x[0] + x;
					c_center_y[0] = c_center_y[0] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(255, 0, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}



				if (R <15 && G< 15 && B< 15)// black
				{
					c_count[1]++;
					x = j;
					y = i;

					c_center_x[1] = c_center_x[1] + x;
					c_center_y[1] = c_center_y[1] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(255, 255, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}



				if (R>60 && G<90 && G>50 && B<20)// yellow
				{
					c_count[2]++;
					x = j;
					y = i;

					c_center_x[2] = c_center_x[2] + x;
					c_center_y[2] = c_center_y[2] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(100, 0, 100), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}


				if (G > 80 && R< 80 && B < 30) // green detection
				{
					c_count[3]++;
					x = j;
					y = i;

					c_center_x[3] = c_center_x[3] + x;
					c_center_y[3] = c_center_y[3] + y;

					cvRectangle(frame, cvPoint(x, y),
						cvPoint(x + 2, y + 2), cvScalar(100, 0, 100), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .
				}



			}




		for (int i = 0; i < 4; i++) {
			c_center_x[i] = c_center_x[i] / c_count[i];
			c_center_y[i] = c_center_y[i] / c_count[i];
			cvRectangle(frame, cvPoint(c_center_x[i], c_center_y[i]),
				cvPoint(c_center_x[i] + 20, c_center_y[i] + 20), cvScalar(255, 255, 255), 2, 8, 0);// 만족하는 경우 사각형을 그려주게 됨 .

																								   // if (!entry) {
																								   //   prev_c_center_x[i] = c_center_x[i];
																								   //   prev_c_center_y[i] = c_center_y[i];

																								   // }
																								   // else {
																								   //   prev_c_center_x[i] = ((1 - FILTER_VAL) * c_center_x[i] + (FILTER_VAL)* prev_c_center_x[i]);
																								   //   prev_c_center_y[i] = ((1 - FILTER_VAL) * c_center_y[i] + (FILTER_VAL)* prev_c_center_y[i]);
																								   // }

			prev_c_center_x[i] = filter(&c_center_x[i], c_center_x[i]);
			prev_c_center_y[i] = filter(&c_center_y[i], c_center_y[i]);

			if (is_C_on[i])
			{
				rectangle(windowImage, Point(calX(prev_c_center_x[i]), calY(prev_c_center_y[i])),
					Point(calX(prev_c_center_x[i]) + 20, calY(prev_c_center_y[i]) + 20), Scalar(0, 55, 255), 5, 5);
				putText(windowImage, C_Names[i], Point(calX(prev_c_center_x[i]), calY(prev_c_center_y[i])), CV_FONT_HERSHEY_SIMPLEX, 1, Scalar::all(255), 3, 8);
			}
			c_center_x[i] = 0;
			c_center_y[i] = 0;
			c_count[i] = 1;

		}


		cvShowImage("Rtracker", frame);
		imshow("MessageShowing", windowImage);
		c = cvWaitKey(10);
		//ESC Key
		if (c == 27) break;
		//press n to proceed
		if (c == 'n')
		{
			level++;
			changelevel();
		}

		//Filter option
		if (!entry)
		{
			entry = true;
		}

		//Sleep(5);
	}
	cvReleaseCapture(&capture);
	cvDestroyWindow("Canny");

	return 0;

}


