/****************************************************************************
*                                                                           *
*  OpenNI 1.x Alpha                                                         *
*  Copyright (C) 2011 PrimeSense Ltd.                                       *
*                                                                           *
*  This file is part of OpenNI.                                             *
*                                                                           *
*  OpenNI is free software: you can redistribute it and/or modify           *
*  it under the terms of the GNU Lesser General Public License as published *
*  by the Free Software Foundation, either version 3 of the License, or     *
*  (at your option) any later version.                                      *
*                                                                           *
*  OpenNI is distributed in the hope that it will be useful,                *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the             *
*  GNU Lesser General Public License for more details.                      *
*                                                                           *
*  You should have received a copy of the GNU Lesser General Public License *
*  along with OpenNI. If not, see <http://www.gnu.org/licenses/>.           *
*                                                                           *
****************************************************************************/
//---------------------------------------------------------------------------
// Includes
//---------------------------------------------------------------------------
#include <math.h>
#include "SceneDrawer.h"

#ifndef USE_GLES
#if (XN_PLATFORM == XN_PLATFORM_MACOSX)
	#include <GLUT/glut.h>
#else
	#include <GL/glut.h>
#endif
#else
	#include "opengles.h"
#endif

extern xn::UserGenerator g_UserGenerator;
extern xn::DepthGenerator g_DepthGenerator;

extern XnBool g_bDrawBackground;
extern XnBool g_bDrawPixels;
extern XnBool g_bDrawSkeleton;
extern XnBool g_bPrintID;
extern XnBool g_bPrintState;

extern XnBool g_bPrintFrameID;
extern XnBool g_bMarkJoints;
GLfloat texcoords[8];

#include <map>
std::map<XnUInt32, std::pair<XnCalibrationStatus, XnPoseDetectionStatus> > m_Errors;
void XN_CALLBACK_TYPE MyCalibrationInProgress(xn::SkeletonCapability& /*capability*/, XnUserID id, XnCalibrationStatus calibrationError, void* /*pCookie*/)
{
	m_Errors[id].first = calibrationError;
}
void XN_CALLBACK_TYPE MyPoseInProgress(xn::PoseDetectionCapability& /*capability*/, const XnChar* /*strPose*/, XnUserID id, XnPoseDetectionStatus poseError, void* /*pCookie*/)
{
	m_Errors[id].second = poseError;
}

unsigned int getClosestPowerOfTwo(unsigned int n)
{
	unsigned int m = 2;
	while(m < n) m<<=1;

	return m;
}
GLuint initTexture(void** buf, int& width, int& height)
{
	GLuint texID = 0;
	glGenTextures(1,&texID);

	width = getClosestPowerOfTwo(width);
	height = getClosestPowerOfTwo(height); 
	*buf = new unsigned char[width*height*4];
	glBindTexture(GL_TEXTURE_2D,texID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return texID;
}

void DrawRectangle(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
{
	GLfloat verts[8] = {	topLeftX, topLeftY,
		topLeftX, bottomRightY,
		bottomRightX, bottomRightY,
		bottomRightX, topLeftY
	};
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	//TODO: Maybe glFinish needed here instead - if there's some bad graphics crap
	glFlush();
}

void DrawTexture(float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
{
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

	DrawRectangle(topLeftX, topLeftY, bottomRightX, bottomRightY);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

XnFloat Colors[][3] =
{
	{0,1,1},
	{0,0,1},
	{0,1,0},
	{1,1,0},
	{1,0,0},
	{1,.5,0},
	{.5,1,0},
	{0,.5,1},
	{.5,0,1},
	{1,1,.5},
	{1,1,1}
};
XnUInt32 nColors = 10;
#ifndef USE_GLES
void glPrintString(void *font, char *str)
{
	int i,l = (int)strlen(str);

	for(i=0; i<l; i++)
	{
		glutBitmapCharacter(font,*str++);
	}
}
#endif
bool DrawLimb(XnUserID player, XnSkeletonJoint eJoint1, XnSkeletonJoint eJoint2)
{
	if (!g_UserGenerator.GetSkeletonCap().IsTracking(player))
	{
		printf("not tracked!\n");
		return true;
	}

	if (!g_UserGenerator.GetSkeletonCap().IsJointActive(eJoint1) ||
		!g_UserGenerator.GetSkeletonCap().IsJointActive(eJoint2))
	{
		return false;
	}

	XnSkeletonJointPosition joint1, joint2;
	g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint1, joint1);
	g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint2, joint2);

	if (joint1.fConfidence < 0.5 || joint2.fConfidence < 0.5)
	{
		return true;
	}

	XnPoint3D pt[2];
	pt[0] = joint1.position;
	pt[1] = joint2.position;

	g_DepthGenerator.ConvertRealWorldToProjective(2, pt, pt);
#ifndef USE_GLES
	glVertex3i(pt[0].X, pt[0].Y, 0);
	glVertex3i(pt[1].X, pt[1].Y, 0);
#else
	GLfloat verts[4] = {pt[0].X, pt[0].Y, pt[1].X, pt[1].Y};
	glVertexPointer(2, GL_FLOAT, 0, verts);
	glDrawArrays(GL_LINES, 0, 2);
	glFlush();
#endif

	return true;
}

static const float DEG2RAD = 3.14159/180;
 
void drawCircle(float x, float y, float radius)
{
   glBegin(GL_TRIANGLE_FAN);
 
   for (int i=0; i < 360; i++)
   {
      float degInRad = i*DEG2RAD;
      glVertex2f(x + cos(degInRad)*radius, y + sin(degInRad)*radius);
   }
 
   glEnd();
}
void DrawJoint(XnUserID player, XnSkeletonJoint eJoint)
{
	if (!g_UserGenerator.GetSkeletonCap().IsTracking(player))
	{
		printf("not tracked!\n");
		return;
	}

	if (!g_UserGenerator.GetSkeletonCap().IsJointActive(eJoint))
	{
		return;
	}

	XnSkeletonJointPosition joint;
	g_UserGenerator.GetSkeletonCap().GetSkeletonJointPosition(player, eJoint, joint);

	if (joint.fConfidence < 0.5)
	{
		return;
	}

	XnPoint3D pt;
	pt = joint.position;

	g_DepthGenerator.ConvertRealWorldToProjective(1, &pt, &pt);

	drawCircle(pt.X, pt.Y, 2);
}

const XnChar* GetCalibrationErrorString(XnCalibrationStatus error)
{
	switch (error)
	{
	case XN_CALIBRATION_STATUS_OK:
		return "OK";
	case XN_CALIBRATION_STATUS_NO_USER:
		return "NoUser";
	case XN_CALIBRATION_STATUS_ARM:
		return "Arm";
	case XN_CALIBRATION_STATUS_LEG:
		return "Leg";
	case XN_CALIBRATION_STATUS_HEAD:
		return "Head";
	case XN_CALIBRATION_STATUS_TORSO:
		return "Torso";
	case XN_CALIBRATION_STATUS_TOP_FOV:
		return "Top FOV";
	case XN_CALIBRATION_STATUS_SIDE_FOV:
		return "Side FOV";
	case XN_CALIBRATION_STATUS_POSE:
		return "Pose";
	default:
		return "Unknown";
	}
}
const XnChar* GetPoseErrorString(XnPoseDetectionStatus error)
{
	switch (error)
	{
	case XN_POSE_DETECTION_STATUS_OK:
		return "OK";
	case XN_POSE_DETECTION_STATUS_NO_USER:
		return "NoUser";
	case XN_POSE_DETECTION_STATUS_TOP_FOV:
		return "Top FOV";
	case XN_POSE_DETECTION_STATUS_SIDE_FOV:
		return "Side FOV";
	case XN_POSE_DETECTION_STATUS_ERROR:
		return "General error";
	default:
		return "Unknown";
	}
}


void DrawDepthMap(const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd, float COM_tracker[][100], int Bounding_Box[][4])
{
	static bool bInitialized = false;	
	static GLuint depthTexID;
	static unsigned char* pDepthTexBuf;
	static int texWidth, texHeight;

	float topLeftX;
	float topLeftY;
	float bottomRightY;
	float bottomRightX;
	float texXpos;
	float texYpos;

	if(!bInitialized)
	{
		texWidth =  getClosestPowerOfTwo(dmd.XRes());
		texHeight = getClosestPowerOfTwo(dmd.YRes());

//		printf("Initializing depth texture: width = %d, height = %d\n", texWidth, texHeight);
		depthTexID = initTexture((void**)&pDepthTexBuf,texWidth, texHeight) ;

//		printf("Initialized depth texture: width = %d, height = %d\n", texWidth, texHeight);
		bInitialized = true;

		topLeftX = dmd.XRes();
		topLeftY = 0;
		bottomRightY = dmd.YRes();
		bottomRightX = 0;
		texXpos =(float)dmd.XRes()/texWidth;
		texYpos  =(float)dmd.YRes()/texHeight;

		memset(texcoords, 0, 8*sizeof(float));
		texcoords[0] = texXpos, texcoords[1] = texYpos, texcoords[2] = texXpos, texcoords[7] = texYpos;
	}

	unsigned int nValue = 0;
	unsigned int nHistValue = 0;
	unsigned int nIndex = 0;
	unsigned int nX = 0;
	unsigned int nY = 0;
	unsigned int nNumberOfPoints = 0;
	XnUInt16 g_nXRes = dmd.XRes();
	XnUInt16 g_nYRes = dmd.YRes();

	unsigned char* pDestImage = pDepthTexBuf;

	const XnDepthPixel* pDepth = dmd.Data();
	const XnLabel* pLabels = smd.Data();

	static unsigned int nZRes = dmd.ZRes();
	static float* pDepthHist = (float*)malloc(nZRes* sizeof(float));

	// Calculate the accumulative histogram
	memset(pDepthHist, 0, nZRes*sizeof(float));
	for (nY=0; nY<g_nYRes; nY++)
	{
		for (nX=0; nX<g_nXRes; nX++)
		{
			nValue = *pDepth;

			if (nValue != 0)
			{
				pDepthHist[nValue]++;
				nNumberOfPoints++;
			}

			pDepth++;
		}
	}

	for (nIndex=1; nIndex<nZRes; nIndex++)
	{
		pDepthHist[nIndex] += pDepthHist[nIndex-1];
	}
	if (nNumberOfPoints)
	{
		for (nIndex=1; nIndex<nZRes; nIndex++)
		{
			pDepthHist[nIndex] = (unsigned int)(256 * (1.0f - (pDepthHist[nIndex] / nNumberOfPoints)));
		}
	}

	pDepth = dmd.Data();
	if (g_bDrawPixels)
	{
		XnUInt32 nIndex = 0;
		// Prepare the texture map
		for (nY=0; nY<g_nYRes; nY++)
		{
			for (nX=0; nX < g_nXRes; nX++, nIndex++)
			{

				pDestImage[0] = 0;
				pDestImage[1] = 0;
				pDestImage[2] = 0;
				if (g_bDrawBackground || *pLabels != 0)
				{
					nValue = *pDepth;
					XnLabel label = *pLabels;
					XnUInt32 nColorID = label % nColors;
					if (label == 0)
					{
						nColorID = nColors;
					}

					if (nValue != 0)
					{
						nHistValue = pDepthHist[nValue];

						pDestImage[0] = nHistValue * Colors[nColorID][0]; 
						pDestImage[1] = nHistValue * Colors[nColorID][1];
						pDestImage[2] = nHistValue * Colors[nColorID][2];
					}
				}

				pDepth++;
				pLabels++;
				pDestImage+=3;
			}

			pDestImage += (texWidth - g_nXRes) *3;
		}
	}
	else
	{
		xnOSMemSet(pDepthTexBuf, 0, 3*2*g_nXRes*g_nYRes);
	}

	glBindTexture(GL_TEXTURE_2D, depthTexID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pDepthTexBuf);

	// Display the OpenGL texture map
	glColor4f(0.75,0.75,0.75,1);

	glEnable(GL_TEXTURE_2D);
	DrawTexture(dmd.XRes(),dmd.YRes(),0,0);	
	glDisable(GL_TEXTURE_2D);

	char strLabel[50] = "";
	XnUserID aUsers[15];
	XnUInt16 nUsers = 15;
	g_UserGenerator.GetUsers(aUsers, nUsers);
	for (int i = 0; i < nUsers; ++i)
	{
#ifndef USE_GLES
		XnPoint3D com;
                g_UserGenerator.GetCoM(aUsers[i], com);
                XnPoint3D com2 = com;
		//generater COM tracker
		if (aUsers[i] < 15)
		{
			int label = aUsers[i];
			for (int j = 99; j >= 1; --j)
			{
				float COM_value = COM_tracker[label][j-1];
				COM_tracker[label][j] = COM_value;
			}
			COM_tracker[label][0] = com2.Z;
		}		
		else
		{
			printf("UserID is larger than 15");
		}
				
				
		if (g_bPrintID)
		{
			//printf("User %d : %d , Z: %f", i, aUsers[i], com2.Z);
			//printf("\n");
			g_DepthGenerator.ConvertRealWorldToProjective(1, &com, &com);

			XnUInt32 nDummy = 0;

			xnOSMemSet(strLabel, 0, sizeof(strLabel));
			if (!g_bPrintState)
			{
				// Tracking
				xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d", aUsers[i]);
			}
			else if (g_UserGenerator.GetSkeletonCap().IsTracking(aUsers[i]))
			{
				// Tracking
				xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d - Tracking - X:%f,Y:%f,Z:%f", aUsers[i], com2.X, com2.Y, com2.Z);
			}
			else if (g_UserGenerator.GetSkeletonCap().IsCalibrating(aUsers[i]))
			{
				// Calibrating
				xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d - Calibrating [%s]", aUsers[i], GetCalibrationErrorString(m_Errors[aUsers[i]].first));
			}
			else
			{
				// Nothing
				xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d - Looking for pose [%s]", aUsers[i], GetPoseErrorString(m_Errors[aUsers[i]].second));
			}


			glColor4f(1-Colors[i%nColors][0], 1-Colors[i%nColors][1], 1-Colors[i%nColors][2], 1);

			glRasterPos2i(com.X, com.Y);
			glPrintString(GLUT_BITMAP_HELVETICA_18, strLabel);
		}
#endif
		if (g_bDrawSkeleton && g_UserGenerator.GetSkeletonCap().IsTracking(aUsers[i]))
		{
			glColor4f(1-Colors[aUsers[i]%nColors][0], 1-Colors[aUsers[i]%nColors][1], 1-Colors[aUsers[i]%nColors][2], 1);

			// Draw Joints
			if (g_bMarkJoints)
			{
				// Try to draw all joints
				DrawJoint(aUsers[i], XN_SKEL_HEAD);
				DrawJoint(aUsers[i], XN_SKEL_NECK);
				DrawJoint(aUsers[i], XN_SKEL_TORSO);
				DrawJoint(aUsers[i], XN_SKEL_WAIST);

				DrawJoint(aUsers[i], XN_SKEL_LEFT_COLLAR);
				DrawJoint(aUsers[i], XN_SKEL_LEFT_SHOULDER);
				DrawJoint(aUsers[i], XN_SKEL_LEFT_ELBOW);
				DrawJoint(aUsers[i], XN_SKEL_LEFT_WRIST);
				DrawJoint(aUsers[i], XN_SKEL_LEFT_HAND);
				DrawJoint(aUsers[i], XN_SKEL_LEFT_FINGERTIP);

				DrawJoint(aUsers[i], XN_SKEL_RIGHT_COLLAR);
				DrawJoint(aUsers[i], XN_SKEL_RIGHT_SHOULDER);
				DrawJoint(aUsers[i], XN_SKEL_RIGHT_ELBOW);
				DrawJoint(aUsers[i], XN_SKEL_RIGHT_WRIST);
				DrawJoint(aUsers[i], XN_SKEL_RIGHT_HAND);
				DrawJoint(aUsers[i], XN_SKEL_RIGHT_FINGERTIP);

				DrawJoint(aUsers[i], XN_SKEL_LEFT_HIP);
				DrawJoint(aUsers[i], XN_SKEL_LEFT_KNEE);
				DrawJoint(aUsers[i], XN_SKEL_LEFT_ANKLE);
				DrawJoint(aUsers[i], XN_SKEL_LEFT_FOOT);

				DrawJoint(aUsers[i], XN_SKEL_RIGHT_HIP);
				DrawJoint(aUsers[i], XN_SKEL_RIGHT_KNEE);
				DrawJoint(aUsers[i], XN_SKEL_RIGHT_ANKLE);
				DrawJoint(aUsers[i], XN_SKEL_RIGHT_FOOT);
			}

#ifndef USE_GLES
			glBegin(GL_LINES);
#endif

			// Draw Limbs
			DrawLimb(aUsers[i], XN_SKEL_HEAD, XN_SKEL_NECK);

			DrawLimb(aUsers[i], XN_SKEL_NECK, XN_SKEL_LEFT_SHOULDER);
			DrawLimb(aUsers[i], XN_SKEL_LEFT_SHOULDER, XN_SKEL_LEFT_ELBOW);
			if (!DrawLimb(aUsers[i], XN_SKEL_LEFT_ELBOW, XN_SKEL_LEFT_WRIST))
			{
				DrawLimb(aUsers[i], XN_SKEL_LEFT_ELBOW, XN_SKEL_LEFT_HAND);
			}
			else
			{
				DrawLimb(aUsers[i], XN_SKEL_LEFT_WRIST, XN_SKEL_LEFT_HAND);
				DrawLimb(aUsers[i], XN_SKEL_LEFT_HAND, XN_SKEL_LEFT_FINGERTIP);
			}


			DrawLimb(aUsers[i], XN_SKEL_NECK, XN_SKEL_RIGHT_SHOULDER);
			DrawLimb(aUsers[i], XN_SKEL_RIGHT_SHOULDER, XN_SKEL_RIGHT_ELBOW);
			if (!DrawLimb(aUsers[i], XN_SKEL_RIGHT_ELBOW, XN_SKEL_RIGHT_WRIST))
			{
				DrawLimb(aUsers[i], XN_SKEL_RIGHT_ELBOW, XN_SKEL_RIGHT_HAND);
			}
			else
			{
				DrawLimb(aUsers[i], XN_SKEL_RIGHT_WRIST, XN_SKEL_RIGHT_HAND);
				DrawLimb(aUsers[i], XN_SKEL_RIGHT_HAND, XN_SKEL_RIGHT_FINGERTIP);
			}

			DrawLimb(aUsers[i], XN_SKEL_LEFT_SHOULDER, XN_SKEL_TORSO);
			DrawLimb(aUsers[i], XN_SKEL_RIGHT_SHOULDER, XN_SKEL_TORSO);

			DrawLimb(aUsers[i], XN_SKEL_TORSO, XN_SKEL_LEFT_HIP);
			DrawLimb(aUsers[i], XN_SKEL_LEFT_HIP, XN_SKEL_LEFT_KNEE);
			DrawLimb(aUsers[i], XN_SKEL_LEFT_KNEE, XN_SKEL_LEFT_FOOT);

			DrawLimb(aUsers[i], XN_SKEL_TORSO, XN_SKEL_RIGHT_HIP);
			DrawLimb(aUsers[i], XN_SKEL_RIGHT_HIP, XN_SKEL_RIGHT_KNEE);
			DrawLimb(aUsers[i], XN_SKEL_RIGHT_KNEE, XN_SKEL_RIGHT_FOOT);

			DrawLimb(aUsers[i], XN_SKEL_LEFT_HIP, XN_SKEL_RIGHT_HIP);
#ifndef USE_GLES
			glEnd();
#endif
		}
	}

	if (g_bPrintFrameID)
	{
		static XnChar strFrameID[80];
		xnOSMemSet(strFrameID, 0, 80);
		XnUInt32 nDummy = 0;
		xnOSStrFormat(strFrameID, sizeof(strFrameID), &nDummy, "%d", dmd.FrameID());

		glColor4f(1, 0, 0, 1);

		glRasterPos2i(10, 10);

		glPrintString(GLUT_BITMAP_HELVETICA_18, strFrameID);
	}
}

int* moving_forward(float COM_tracker[][100], float threshold, int sample_num)
{
	int* direction = new int[15];
	memset(direction,0,15*sizeof(int));
	//printf("dfdfd:%f",COM_tracker[3][0]);
	for(int j = 1; j < 15; j++)
	{
		for(int i = 0; i < sample_num*2; i++)
		{
			if((int)COM_tracker[j][i] == 0)
			{
				direction[j] = -1;
				continue;
			}
		}
		if(direction[j] == 0)
		{
			int bigger = 0;
			for(int i = 0; i< sample_num; i ++)
			{
				if(COM_tracker[j][i] < COM_tracker[j][i+sample_num])
				{
					bigger++;
				}
			}
			if(bigger >= threshold)
			{
				direction[j] = 1;
			}
		}
		//printf("direction:%d",direction[j]);
	}
	return direction;
}

void DrawBox(XnUInt bottom, XnUInt top, XnUInt left, XnUInt right)
{
	//glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	//glEnable(GL_COLOR_MATERIAL);
	glBindTexture(GL_TEXTURE_2D,0);
	glBegin(GL_LINES);
		glColor4f(1.0f,0.0f,0.0f,1.0);
		glLineWidth(2);
        	//draw bottom boarder
        	glVertex2i(left,bottom);
        	glVertex2i(right,bottom);
        	//draw right boarder
        	glVertex2i(right,bottom);
        	glVertex2i(right,top);
        	//draw top boarder
        	glVertex2i(right,top);
        	glVertex2i(left,top);
        	//draw left boarder
        	glVertex2i(left,top);
        	glVertex2i(left,bottom);
        glEnd();
}

void DrawImageMap(const xn::ImageMetaData& imd, const xn::DepthMetaData& dmd, const xn::SceneMetaData& smd, float COM_tracker[][100], int Bounding_Box[][4])
{
	memset(Bounding_Box, -1, 4*15*sizeof(int));
	static bool bInitialized = false;
        static GLuint imageTexID;
        static XnRGB24Pixel* pImageTexBuf;
        static int texWidth, texHeight;

        float topLeftX;
        float topLeftY;
        float bottomRightY;
        float bottomRightX;
        float texXpos;
        float texYpos;

	//compute boarder of depth image on RGB image
	int mask_bottom = -1;
	int mask_top = -1;
	int mask_left = -1;
	int mask_right = -1;
	for(XnUInt16 nY = 0; nY < imd.YRes(); nY++)
	{
		for(XnUInt16 nX = 0; nX < imd.XRes(); nX++)
		{	
			XnInt32 nDepthIndex = 0;
			XnDouble dRealX = (nX +imd.XOffset())/(XnDouble)imd.FullXRes();
			XnDouble dRealY = (nY +imd.YOffset())/(XnDouble)imd.FullYRes();
			XnUInt32 nDepthX = dRealX * dmd.FullXRes() - dmd.XOffset();
			XnUInt32 nDepthY = dRealY * dmd.FullYRes() - dmd.YOffset();
			if (nDepthX >= dmd.XRes() || nDepthY >= dmd.YRes())
			{
				nDepthIndex = -1;
			}
			else
			{
				nDepthIndex = nDepthY*dmd.XRes() + nDepthX;
			}
			if(!(nDepthIndex == -1 || dmd.Data()[nDepthIndex] == 0))
			{
				if( mask_bottom == -1 || nY < mask_bottom)
					mask_bottom = nDepthY;
				if( mask_top == -1 || nY > mask_top)
					mask_top = nDepthY;
				if( mask_left == -1 || nX < mask_left)
					mask_left = nDepthX;
				if( mask_right == -1 || nX > mask_right)
					mask_right = nDepthX;
			}
		}
	}
	//printf("%d,%d\n",imd.XOffset(),dmd.XOffset());

        if(!bInitialized)
        {
                texWidth =  getClosestPowerOfTwo(imd.XRes());
                texHeight = getClosestPowerOfTwo(imd.YRes());

//                printf("Initializing depth texture: width = %d, height = %d\n", texWidth, texHeight);
                imageTexID = initTexture((void**)&pImageTexBuf,texWidth, texHeight) ;

//              printf("Initialized depth texture: width = %d, height = %d\n", texWidth, texHeight);
                bInitialized = true;

                topLeftX = imd.XRes();
                topLeftY = 0;
                bottomRightY = imd.YRes();
                bottomRightX = 0;
                texXpos =(float)imd.XRes()/texWidth;
                texYpos =(float)imd.YRes()/texHeight;

                memset(texcoords, 0, 8*sizeof(float));
                texcoords[0] = texXpos, texcoords[1] = texYpos, texcoords[2] = texXpos, texcoords[7] = texYpos;
        }

	const XnLabel* pLabels = smd.Data();
	XnUInt top = 0;
	XnUInt bottom = 0;
	XnUInt left = 0;
	XnUInt right = 0;

	XnRGB24Pixel* g_pTexMap = NULL;
	XnUInt16 g_nXRes = getClosestPowerOfTwo(imd.XRes());
        XnUInt16 g_nYRes = getClosestPowerOfTwo(imd.YRes());
	//printf("%d\n",imd.XRes());
	//printf("%d",imd.YRes());
	g_pTexMap = (XnRGB24Pixel*)malloc(g_nXRes * g_nYRes * sizeof(XnRGB24Pixel));
	xnOSMemSet(g_pTexMap, 0, g_nXRes * g_nYRes * sizeof(XnRGB24Pixel));

        const XnRGB24Pixel* pImageRow = imd.RGB24Data();
        XnRGB24Pixel* pTexRow = g_pTexMap + imd.YOffset() * g_nXRes;

        for (XnUInt y = 0; y < imd.YRes(); ++y)
        {
                const XnRGB24Pixel* pImage = pImageRow;
                XnRGB24Pixel* pTex = pTexRow + imd.XOffset();
		XnLabel label2;

                for (XnUInt x = 0; x < imd.XRes(); ++x, ++pImage, ++pTex)
                {
                        *pTex = *pImage;
			//decide the position and size of bounding box
			XnLabel label = *pLabels;
			//printf("%d",label);
			if(label < 15 && label > 0)
			{
				left = x;
				right = x;
				top = y;
				bottom = y;
				if( (int)left < Bounding_Box[label][2] || Bounding_Box[label][2] == -1)
					Bounding_Box[label][2] = (int)left;
				if( (int)right > Bounding_Box[label][3])
					Bounding_Box[label][3] = (int)right;
                        	if( (int)Bounding_Box[label][0] == -1)
                                	Bounding_Box[label][0] = (int)bottom;
                        	if( (int)top > Bounding_Box[label][1])
                                	Bounding_Box[label][1] = (int)top;
			}
			else if(label != 0)
				printf("Strange Label : %d\n", label);
			
			pLabels++;
                }
                pImageRow += imd.XRes();
                pTexRow += g_nXRes;
        }	
	pImageTexBuf = g_pTexMap;

	glBindTexture(GL_TEXTURE_2D, imageTexID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, pImageTexBuf);
	free (g_pTexMap);

        // Display the OpenGL texture map
        glColor4f(0.75,0.75,0.75,1);

        glEnable(GL_TEXTURE_2D);
        DrawTexture(imd.XRes(),imd.YRes(),0,0);
	//if direction is 1 --- moving forward
	//change Bounding Box color
	int* direction = NULL;
	direction = moving_forward(COM_tracker,20,20);
	for( int i =1; i< 15; i++)
	{
		//printf("%d\n",direction[i]);
		if( Bounding_Box[i][0] !=-1 && direction[i] == 1)
		{
			bottom = Bounding_Box[i][0];
			top = Bounding_Box[i][1];
			left = Bounding_Box[i][2];
			right = Bounding_Box[i][3];
			DrawBox(bottom,top,left,right);
		}
	}
	DrawBox((XnUInt)mask_bottom,(XnUInt)mask_top,(XnUInt)mask_left,(XnUInt)mask_right);
        glDisable(GL_TEXTURE_2D);

	
	char strLabel[50] = "";
	XnUserID aUsers[15];
	XnUInt16 nUsers = 15;
	g_UserGenerator.GetUsers(aUsers, nUsers);
	for (int i = 0; i < nUsers; ++i)
	{
#ifndef USE_GLES
		XnPoint3D com;
                g_UserGenerator.GetCoM(aUsers[i], com);
                XnPoint3D com2 = com;
		//generater COM tracker
		if (aUsers[i] < 15)
		{
			int label = aUsers[i];
			for (int j = 99; j >= 1; --j)
			{
				float COM_value = COM_tracker[label][j-1];
				COM_tracker[label][j] = COM_value;
			}
			COM_tracker[label][0] = com2.Z;
		}		
		else
		{
			printf("UserID is larger than 15");
		}
				
				
		if (g_bPrintID)
		{
			//printf("User %d : %d , Z: %f", i, aUsers[i], com2.Z);
			//printf("\n");
			g_DepthGenerator.ConvertRealWorldToProjective(1, &com, &com);

			XnUInt32 nDummy = 0;

			xnOSMemSet(strLabel, 0, sizeof(strLabel));
			if (!g_bPrintState)
			{
				// Tracking
				xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d", aUsers[i]);
			}
			else if (g_UserGenerator.GetSkeletonCap().IsTracking(aUsers[i]))
			{
				// Tracking
				xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d - Tracking - X:%f,Y:%f,Z:%f", aUsers[i], com2.X, com2.Y, com2.Z);
			}
			else if (g_UserGenerator.GetSkeletonCap().IsCalibrating(aUsers[i]))
			{
				// Calibrating
				xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d - Calibrating [%s]", aUsers[i], GetCalibrationErrorString(m_Errors[aUsers[i]].first));
			}
			else
			{
				// Nothing
				xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d - Looking for pose [%s]", aUsers[i], GetPoseErrorString(m_Errors[aUsers[i]].second));
			}


			glColor4f(1-Colors[i%nColors][0], 1-Colors[i%nColors][1], 1-Colors[i%nColors][2], 1);

			glRasterPos2i(com.X, com.Y);
			glPrintString(GLUT_BITMAP_HELVETICA_18, strLabel);
		}
#endif
	}
}
