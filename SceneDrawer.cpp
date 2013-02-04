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
extern XnBool g_bPrintState;

extern XnBool g_bPrintFrameID;
extern XnBool *is_warning;
extern clock_t *time_start_warn;
GLfloat texcoords[8];
const int BOUNDING_BOX_WIDTH = 2;
const int WARNING_BOX_WIDTH = 20;
const XnUInt WARNING_BOTTOM = 1;
const XnUInt WARNING_TOP = 479;
const XnUInt WARNING_LEFT = 1;
const XnUInt WARNING_RIGHT = 639;
const float WARNING_TIME = 1.5;
const float THRESHOLD = 100;
const int SAMPLE_NUM = 20;
const int FAST_SAMPLE_NUM = 5;
const int FAST = 1;
const int MEDIUM = 2;
const int SLOW = 3;

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
				
				
		//printf("User %d : %d , Z: %f", i, aUsers[i], com2.Z);
		//printf("\n");
		g_DepthGenerator.ConvertRealWorldToProjective(1, &com, &com);

		XnUInt32 nDummy = 0;

		xnOSMemSet(strLabel, 0, sizeof(strLabel));
		if (g_bPrintState)
		{
			// Tracking
			xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d, Z: %f", aUsers[i], com2.Z);
		}

		glColor4f(1-Colors[i%nColors][0], 1-Colors[i%nColors][1], 1-Colors[i%nColors][2], 1);

		glRasterPos2i(com.X, com.Y);
		glPrintString(GLUT_BITMAP_HELVETICA_18, strLabel);
		
#endif
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

float average(float COM_tracker[][100],int j,int start, int end)
{
	float sum = 0;
	for(int i = start; i< end; i++)
		sum = sum + COM_tracker[j][i];
	float avg = sum/(end - start);
	return avg;
}

bool moving_with_speed(float COM_tracker[][100], int user_id, int type)
{
	int sample_num;
	if(type == FAST)
	{
		sample_num = FAST_SAMPLE_NUM;
	}
	else
	{
		sample_num = SAMPLE_NUM;
	}
	int check_sample_num = sample_num;
	if(type == SLOW)
	{
		check_sample_num = 50;
	}
	for(int i = 0; i< check_sample_num*2; i++)
	{
		if((int)COM_tracker[user_id][i] == 0)
		{
			return false;
		}
	}
	float avg_now = 0;
	float avg_prev = 0;
	if(type == SLOW)
	{
		avg_now = average(COM_tracker,user_id,0,sample_num);
		avg_prev = average(COM_tracker,user_id,100-sample_num,100);
	}
	else
	{
		avg_now = average(COM_tracker,user_id,0,sample_num);
                avg_prev = average(COM_tracker,user_id,sample_num,sample_num*2);
	}
	if(avg_prev - avg_now >= THRESHOLD)
	{
		return true;
	}
	else
	{
		return false;
	}
}

int* moving_forward(float COM_tracker[][100])
{
	int* direction = new int[15];
	memset(direction,0,15*sizeof(int));
	for(int j = 1; j < 15; j++)
        {
                bool wrong_direction;
                for(int speed = FAST; speed <= SLOW; speed++)
                {
                        wrong_direction = moving_with_speed(COM_tracker,j,speed);
                        if(wrong_direction == true)
                        {
                                direction[j] = 1;
				printf("user %d: %f\n",j,COM_tracker[j][0]);
                                break;
                        }
                }
        }
	return direction;
}

void DrawBox(XnUInt bottom, XnUInt top, XnUInt left, XnUInt right, const int width)
{
	glBindTexture(GL_TEXTURE_2D,0);
	glColor4f(1.0f,0.0f,0.0f,1.0);
        glLineWidth(width);
	glBegin(GL_LINES);
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

void DrawWarningBox(void)
{
	glBindTexture(GL_TEXTURE_2D,0);
        glColor4f(1.0f,0.0f,0.0f,1.0);
	glRectf(0,0,640,WARNING_BOX_WIDTH);
	glRectf(0,0,WARNING_BOX_WIDTH,480);
	glRectf(640,480,640-WARNING_BOX_WIDTH,0);
	glRectf(640,480,0,480-WARNING_BOX_WIDTH);
}

void ShowWarning(XnBool *is_warning_p, clock_t *time_start_warn_p, XnBool direction)
{
	if(direction == FALSE)
	{
		DrawWarningBox();
		*is_warning_p = TRUE;
		*time_start_warn_p = clock();
	}
	else if(*is_warning_p == TRUE)
	{
		clock_t time_now = clock();
		float t = ((float)(time_now - *time_start_warn_p))/CLOCKS_PER_SEC;
		//printf("%f\n",t);
		if( t >= WARNING_TIME)
		{
			*is_warning_p = FALSE;
		}
		else
		{
			DrawWarningBox();
		}
	}
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
	direction = moving_forward(COM_tracker);
	XnBool is_direction_right = TRUE;
	for( int i =1; i< 15; i++)
	{
		//printf("%d\n",direction[i]);
		if( Bounding_Box[i][0] !=-1 && direction[i] == 1)
		{
			bottom = Bounding_Box[i][0];
			top = Bounding_Box[i][1];
			left = Bounding_Box[i][2];
			right = Bounding_Box[i][3];
			DrawBox(bottom,top,left,right,BOUNDING_BOX_WIDTH);
			if( is_direction_right == TRUE)
			{
				is_direction_right = FALSE;
			}
		}
	}
	ShowWarning(is_warning,time_start_warn,is_direction_right);
	//The following is to show the cover area of sensor.
	//DrawBox((XnUInt)mask_bottom,(XnUInt)mask_top,(XnUInt)mask_left,(XnUInt)mask_right,BOUNDING_BOX_WIDTH);
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
				
				
		//printf("User %d : %d , Z: %f", i, aUsers[i], com2.Z);
		//printf("\n");
		g_DepthGenerator.ConvertRealWorldToProjective(1, &com, &com);

		XnUInt32 nDummy = 0;

		xnOSMemSet(strLabel, 0, sizeof(strLabel));
		if (g_bPrintState)
		{
			// Tracking
			xnOSStrFormat(strLabel, sizeof(strLabel), &nDummy, "%d, Z: %f", aUsers[i], com2.Z);
		}

		glColor4f(1-Colors[i%nColors][0], 1-Colors[i%nColors][1], 1-Colors[i%nColors][2], 1);

		glRasterPos2i(com.X, com.Y);
		glPrintString(GLUT_BITMAP_HELVETICA_18, strLabel);
#endif
	}
}
