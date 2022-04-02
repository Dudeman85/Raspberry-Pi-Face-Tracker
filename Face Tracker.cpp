#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/objdetect.hpp" 
#include <wiringPi.h>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <sys/timeb.h>
#include <thread>
#include <string>
#include <chrono>
#include <stdlib.h>
#include <bits/stdc++.h>

using namespace cv;
using namespace std;

Mat FRAME; //Frame from the pi camera for interaction between threads
Point center; //Center point of face for interaction between threads
string classifier = "face";

//Thread for OpenCV to capture frames from the pi camera
void CaptureFrames()
{
	//Open first camera
	VideoCapture cam(0);
	if (cam.isOpened() == false)
		cout << "Cannot open camera\n";

	//Resolution of the camera
	cam.set(CV_CAP_PROP_FRAME_WIDTH, 352);
	cam.set(CV_CAP_PROP_FRAME_HEIGHT, 288);

	//Capture frame to global Matrix
	while (true)
	{
		cam.read(FRAME);
	}
}

//Thread for object detection and real time preview using haar cascade
void DrawFrames(int draw)
{
	//Timer
	chrono::high_resolution_clock::time_point t1;

	//load classifier
	CascadeClassifier cascade;
	cascade.load("./cascades/" + classifier + ".xml");

	//Draw window for preview if desired
	if (draw == 1)
	{
		namedWindow("Test", WINDOW_NORMAL);
		resizeWindow("Test", 704, 576);
	}

	//Loop untill escape is pressed
	while (true)
	{
		if (!FRAME.empty())
		{
			//Timer
			t1 = chrono::high_resolution_clock::now();

			//Varibles for OpenCV
			vector<Rect> faces;
			Mat gray;
			cvtColor(FRAME, gray, COLOR_BGR2GRAY);

			//Detect Faces in image
			cascade.detectMultiScale(gray, faces, 1.1, 7, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));

			//Get center coordinates of first face
			if (faces.size() != 0)
			{
				center.x = cvRound((faces[0].x + faces[0].width*0.5));
				center.y = cvRound((faces[0].y + faces[0].height*0.5));
			}
			else
			{
				center.x = 176;
				center.y = 144;
			}

			//Draw Frame and circle faces if desired
			if (draw == 1)
			{
				for (size_t i = 0; i < faces.size(); i++)
				{
					Rect r = faces[i];
					Mat smallImgROI;
					Point center2;
					vector<Rect> nestedObjects;
					Scalar color = Scalar(255, 0, 0);
					int radius;

					//Get center of faces
					center2.x = cvRound((r.x + r.width*0.5));
					center2.y = cvRound((r.y + r.height*0.5));

					//Draw circles around faces
					radius = cvRound((r.width + r.height)*0.25);
					circle(FRAME, center2, 0.1, color, 3, 8, 0);
					circle(FRAME, center2, radius, color, 3, 8, 0);

				}
				//Display frame
				imshow("Test", FRAME);
			}

			//Exit when escape is pressed
			if (waitKey(10) == 27)
				break;

			//Timer
			chrono::duration<double, milli> timespan = (chrono::high_resolution_clock::now() - t1);
			cout << center << " Took " << timespan.count() << "ms, " << 1000 / timespan.count() << "fps" << endl;
		}
	}
}

//Thread for servo control
void MoveTurret()
{
	int posx = 150;
	int posy = 150;
	int facex = 0;
	int facey = 0;
	int neutralTics = 0;

	center.x = 176;
	center.y = 144;

	//Setup PWM on pins 18 and 19
	wiringPiSetupGpio();
	pinMode(18, PWM_OUTPUT);
	pinMode(19, PWM_OUTPUT);
	pwmSetMode(PWM_MODE_MS);
	pwmSetRange(2000);
	pwmSetClock(192);

	while (true) {
		//change the length of pwm to control the servos
		pwmWrite(19, posx);
		pwmWrite(18, posy);

		facex = center.x - 176;
		facey = center.y - 144;

		//Get amount to move servos based on the distance of the face to the center
		if (abs(facex) > 10)
		{
			posx += facex / 30;
			posx = max(80, min(posx, 210));
		}
		if (abs(facey) > 20)
		{
			posy -= facey / 30;
			posy = max(80, min(posy, 210));
		}

		//If no object is detected for about 1 second return to neutral
		if (facex == 0 && facey == 0)
		{
			neutralTics++;
			if (neutralTics > 30)
			{
				posx = 150;
				posy = 150;
			}
		}
		else
			neutralTics = 0;

		//Wait for about half a frame before continuing
		this_thread::sleep_for(chrono::milliseconds(32));
	}
}

int main(int argc, char** argv)
{
	//Start Frame Capture
	thread t1(CaptureFrames);

	//Start Servo Control
	thread t3(MoveTurret);

	//Start object detection with or without display
	if (argc > 1)
	{
		if (argc > 2)
			classifier = argv[2];

		thread t2(DrawFrames, atoi(argv[1]));
		t2.join();
	}
	else
	{
		thread t2(DrawFrames, 0);
		t2.join();
	}

	pwmWrite(19, 150);
	pwmWrite(18, 150);
}
