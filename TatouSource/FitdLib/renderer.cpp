///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Software 3D renderer and projection pipeline
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "consoleLog.h"
#include "modelAtlas.h"
#include "hdBackground.h"

extern int numSpheresPrimitives;

void osystem_fillPolyTextured(float* buffer, int numPoint, float* uvs, bgfx::TextureHandle texture);

/* Projection:

 Z += cameraPerspective;
 float transformedX = ((X * cameraFovX) / Z) + cameraCenterX;
 float transformedY = ((Y * cameraFovY) / Z) + cameraCenterY;
 
 {X, Y, Z, 1}
 
 {
    ((X * cameraFovX) / Z) + cameraCenterX
    ((Y * cameraFovY) / Z) + cameraCenterY
    Z + cameraPerspective
    ?
 }
 
 ((X * CameraFovX) + (Z+CameraPersp) * cameraCenterX) / (Z+cameraPersp)
 
 
 
 */

struct rendererPointStruct
{
    float X;
    float Y;
    float Z;
};

#define NUM_MAX_VERTEX_IN_PRIM 64

struct primEntryStruct
{
	u8 material;
	u8 color;
	u16 size;
	u16 numOfVertices;
	primTypeEnum type;
	int originalPrimIndex;
	bool isRampPrim;  // Track if this primitive is ramp-shaded (material 3-6)
	bool nearClipped;  // Flagged by floating polygon detection (suppressed at render time)
	rendererPointStruct vertices[NUM_MAX_VERTEX_IN_PRIM];
};

#define NUM_MAX_PRIM_ENTRY 500

primEntryStruct primTable[NUM_MAX_PRIM_ENTRY];

u32 positionInPrimEntry = 0;

// Current model atlas data for textured rendering (set per AffObjet call)
static ModelAtlasData* s_currentAtlas = nullptr;
static int s_currentPrimIndex = 0;
static bool s_modelHasTexturedPrims = false;  // Track if model has textured primitives (type 9/10)

// Near-camera floating polygon clipping thresholds.
// When a model is close to the camera, small detail polygons (eyes, nostrils)
// that are geometrically offset from the face surface toward the camera can
// appear as floating colored patches. These constants control detection.
static const float NEAR_POLY_CLIP_Z          = 300.f;   // Enhanced near-clip zone (camera-space Z)
static const float NEAR_POLY_MAX_SCREEN_FRAC = 0.50f;   // Max screen-space extent as fraction of viewport
static const float NEAR_POLY_MAX_Z_RANGE     = 200.f;   // Max Z spread across a single polygon's vertices

// Post-processing: detect small detail polygons (eyes, nostrils) overlapping
// larger polygons (face). A tiny polygon whose screen bbox fits inside a
// significantly larger polygon's screen bbox is suppressed.
static const float FLOATING_POLY_AREA_MAX    = 500.f;   // Max rest-pose triangle area to consider "detail"
static const float FLOATING_POLY_AREA_RATIO  = 4.0f;    // Containing polygon must be this many times larger
static const float FLOATING_POLY_MAX_EXTENT  = 30.f;    // Max rest-pose bbox extent (any axis) for "detail"

ModelAtlasData* getCurrentAtlas()
{
    return s_currentAtlas;
}

void clearCurrentAtlas()
{
    s_currentAtlas = nullptr;
}

int BBox3D1=0;
int BBox3D2=0;
int BBox3D3=0;
int BBox3D4=0;

int renderVar1=0;

int numOfPrimitiveToRender=0;

char renderBuffer[3261];

char* renderVar2=NULL;

int modelFlags = 0;

int modelCosAlpha;
int modelSinAlpha;
int modelCosBeta;
int modelSinBeta;
int modelCosGamma;
int modelSinGamma;

bool noModelRotation;

int renderX;
int renderY;
int renderZ;

int numOfPoints;
int numOfBones;

std::array<point3dStruct, NUM_MAX_POINT_IN_POINT_BUFFER> pointBuffer;
s16 cameraSpaceBuffer[NUM_MAX_POINT_IN_POINT_BUFFER*3];
s16 bonesBuffer[NUM_MAX_BONES];

bool boneRotateX;
bool boneRotateY;
bool boneRotateZ;

int boneRotateXCos;
int boneRotateXSin;
int boneRotateYCos;
int boneRotateYSin;
int boneRotateZCos;
int boneRotateZSin;

char primBuffer[30000];

int renderVar3;

#ifndef AITD_UE4
void fillpoly(s16 * datas, int n, char c);
#endif

/*

 
 
*/

void transformPoint(float* ax, float* bx, float* cx)
{
    int X = (int)*ax;
    int Y = (int)*bx;
    int Z = (int)*cx;
    {
        int* ax = &X;
        int* bx = &Y;
        int* cx = &Z;

        {
            int x;
            int y;
            int z;

            if(transformUseY)
            {
                x = (((((*ax) * transformYSin) - ((*cx) * transformYCos))) / 0x10000)<<1;
                z = (((((*ax) * transformYCos) + ((*cx) * transformYSin))) / 0x10000)<<1;
            }
            else
            {
                x = (*ax);
                z = (*cx);
            }

            //si = x
            //ax = z

            if(transformUseX)
            {
                int tempY = (*bx);
                int tempZ = z;
                y = ((((tempY * transformXSin ) - (tempZ * transformXCos))) / 0x10000)<<1;
                z = ((((tempY * transformXCos ) + (tempZ * transformXSin))) / 0x10000)<<1;
            }
            else
            {
                y = (*bx);
            }

            // cx = y
            // bx = z

            if(transformUseZ)
            {
                int tempX = x;
                int tempY = y;
                x = ((((tempX * transformZSin) - ( tempY * transformZCos))) / 0x10000)<<1;
                y = ((((tempX * transformZCos) + ( tempY * transformZSin))) / 0x10000)<<1;
            }

            *ax = x;
            *bx = y;
            *cx = z;
        }
    }

    *ax = (float)X;
    *bx = (float)Y;
    *cx = (float)Z;
}

void InitGroupeRot(int transX,int transY,int transZ)
{
    if(transX)
    {
        boneRotateXCos = cosTable[transX&0x3FF];
        boneRotateXSin = cosTable[(transX+0x100)&0x3FF];

        boneRotateX = true;
    }
    else
    {
        boneRotateX = false;
    }

    if(transY)
    {
        boneRotateYCos = cosTable[transY&0x3FF];
        boneRotateYSin = cosTable[(transY+0x100)&0x3FF];

        boneRotateY = true;
    }
    else
    {
        boneRotateY = false;
    }

    if(transZ)
    {
        boneRotateZCos = cosTable[transZ&0x3FF];
        boneRotateZSin = cosTable[(transZ+0x100)&0x3FF];

        boneRotateZ = true;
    }
    else
    {
        boneRotateZ = false;
    }
}

void RotateList(point3dStruct* pointPtr, int numOfPoint)
{
    for(int i=0;i<numOfPoint;i++)
    {
        point3dStruct& point = pointPtr[i];
        int x = point.x;
        int y = point.y;
        int z = point.z;

        if(boneRotateY)
        {
            int tempX = x;
            int tempZ = z;
            x = ((((tempX * boneRotateYSin) - (tempZ * boneRotateYCos)))>>16)<<1;
            z = ((((tempX * boneRotateYCos) + (tempZ * boneRotateYSin)))>>16)<<1;
        }

        if(boneRotateX)
        {
            int tempY = y;
            int tempZ = z;
            y = ((((tempY * boneRotateXSin ) - (tempZ * boneRotateXCos)))>>16)<<1;
            z = ((((tempY * boneRotateXCos ) + (tempZ * boneRotateXSin)))>>16)<<1;
        }

		if(boneRotateZ)
		{
			int tempX = x;
			int tempY = y;
			x = ((((tempX * boneRotateZSin) - ( tempY * boneRotateZCos)))>>16)<<1;
			y = ((((tempX * boneRotateZCos) + ( tempY * boneRotateZSin)))>>16)<<1;
		}

        point.x = x;
        point.y = y;
        point.z = z;
    }
}

void RotateGroupeOptimise(sGroup* ptr)
{
    if (ptr->m_numGroup) // if group number is 0
    {
        int baseBone = ptr->m_start;
        int numPoints = ptr->m_numVertices;

        RotateList(pointBuffer.data() + baseBone, numPoints);
    }
}

void RotateGroupe(sGroup* ptr)
{
    int baseBone = ptr->m_start;
    int numPoints = ptr->m_numVertices;
    int temp;
    int temp2;

    RotateList(pointBuffer.data() + baseBone, numPoints);

    temp = ptr->m_numGroup; // group number

    temp2 = numOfBones - temp;

    do
    {
        if (ptr->m_orgGroup == temp) // is it on of this group child
        {
            RotateGroupe(ptr); // yes, so apply the transformation to him
        }

        ptr++;
    } while (--temp2);
}

void TranslateGroupe(int transX, int transY, int transZ, sGroup* ptr)
{
    for (int i = 0; i < ptr->m_numVertices; i++)
    {
        point3dStruct& point = pointBuffer[ptr->m_start + i];
        point.x += transX;
        point.y += transY;
        point.z += transZ;
    }
}

void ZoomGroupe(int zoomX, int zoomY, int zoomZ, sGroup* ptr)
{
    for (int i = 0; i < ptr->m_numVertices; i++)
    {
        point3dStruct& point = pointBuffer[ptr->m_start + i];
        point.x = (point.x * (zoomX + 256)) / 256;
        point.y = (point.y * (zoomY + 256)) / 256;
        point.z = (point.z * (zoomZ + 256)) / 256;
    }
}

int AnimNuage(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    renderX = x - translateX;
    renderY = y;
    renderZ = z - translateZ;

    ASSERT(pBody->m_vertices.size()<NUM_MAX_POINT_IN_POINT_BUFFER);

    for (int i = 0; i < pBody->m_vertices.size(); i++)
    {
        pointBuffer[i] = pBody->m_vertices[i];
    }

    numOfPoints = pBody->m_vertices.size();
    numOfBones = pBody->m_groupOrder.size();
    ASSERT(numOfBones<NUM_MAX_BONES);

    if(pBody->m_flags & INFO_OPTIMISE)
    {
        for(int i=0;i<pBody->m_groupOrder.size();i++)
        {
            int boneDataOffset = pBody->m_groupOrder[i];
            sGroup* pGroup = &pBody->m_groups[pBody->m_groupOrder[i]];

            switch(pGroup->m_state.m_type)
            {
            case 1:
                if(pGroup->m_state.m_delta.x || pGroup->m_state.m_delta.y || pGroup->m_state.m_delta.z)
                {
                    TranslateGroupe(pGroup->m_state.m_delta.x, pGroup->m_state.m_delta.y, pGroup->m_state.m_delta.z, pGroup);
                }
                break;
            case 2:
                if (pGroup->m_state.m_delta.x || pGroup->m_state.m_delta.y || pGroup->m_state.m_delta.z)
                {
                    ZoomGroupe(pGroup->m_state.m_delta.x, pGroup->m_state.m_delta.y, pGroup->m_state.m_delta.z, pGroup);
                }
                break;
            }

            InitGroupeRot(pGroup->m_state.m_rotateDelta.value().x, pGroup->m_state.m_rotateDelta.value().y, pGroup->m_state.m_rotateDelta.value().z);
            RotateGroupeOptimise(pGroup);
        }
    }
    else
    {
        pBody->m_groups[0].m_state.m_delta.x = alpha;
        pBody->m_groups[0].m_state.m_delta.y = beta;
        pBody->m_groups[0].m_state.m_delta.z = gamma;

        for(int i=0;i<pBody->m_groups.size();i++)
        {
            int boneDataOffset = pBody->m_groupOrder[i];
            sGroup* pGroup = &pBody->m_groups[pBody->m_groupOrder[i]];

            int transX = pGroup->m_state.m_delta.x;
            int transY = pGroup->m_state.m_delta.y;
            int transZ = pGroup->m_state.m_delta.z;

            if(transX || transY || transZ)
            {
                switch(pGroup->m_state.m_type)
                {
                case 0:
                    { 
                        InitGroupeRot(transX,transY,transZ);
                        RotateGroupe(pGroup);
                        break;
                    }
                case 1:
                    {
                        TranslateGroupe(transX,transY,transZ, pGroup);
                        break;
                    }
                case 2:
                    {
                        ZoomGroupe(transX,transY,transZ, pGroup);
                        break;
                    }
                }
            }
        }
    }

    for(int i=0;i<pBody->m_groups.size();i++)
    {
        sGroup* pGroup = &pBody->m_groups[i];
        for(int j=0;j< pGroup->m_numVertices;j++)
        {
            pointBuffer[pGroup->m_start + j].x += pointBuffer[pGroup->m_baseVertices].x;
            pointBuffer[pGroup->m_start + j].y += pointBuffer[pGroup->m_baseVertices].y;
            pointBuffer[pGroup->m_start + j].z += pointBuffer[pGroup->m_baseVertices].z;
        }
    }

    if(modelFlags & INFO_OPTIMISE)
    {
        InitGroupeRot(alpha,beta,gamma);
        RotateList(pointBuffer.data(),numOfPoints);
    }

    {
        s16* outPtr = cameraSpaceBuffer;
        for(int i=0;i<numOfPoints;i++)
        {
            float X = pointBuffer[i].x;
            float Y = pointBuffer[i].y;
            float Z = pointBuffer[i].z;


            X += renderX;
            Y += renderY;
            Z += renderZ;

#if defined(AITD_UE4)
            *(outPtr++) = (s16)X;
            *(outPtr++) = (s16)Y;
            *(outPtr++) = (s16)Z;
#else
            if(Y>10000) // height clamp
            {
                *(outPtr++) = -10000;
                *(outPtr++) = -10000;
                *(outPtr++) = -10000;
            }
            else
            {
                Y -= translateY;

                transformPoint(&X,&Y,&Z);

                *(outPtr++) = (s16)X;
                *(outPtr++) = (s16)Y;
                *(outPtr++) = (s16)Z;
            }
#endif
        }

        s16* ptr = cameraSpaceBuffer;
        float* outPtr2 = renderPointList;

        int k = numOfPoints;
        do
        {
            float X;
            float Y;
            float Z;

            X = ptr[0];
            Y = ptr[1];
            Z = ptr[2];
            ptr+=3;

#if defined(AITD_UE4)
            *(outPtr2++) = X;
            *(outPtr2++) = Y;
            *(outPtr2++) = Z;
#else
            Z += cameraPerspective;

            if( Z<=50 ) // clipping
            {
                *(outPtr2++) = -10000;
                *(outPtr2++) = -10000;
                *(outPtr2++) = -10000;
            }
            else
            {
                float transformedX = ((X * cameraFovX) / Z) + cameraCenterX;
                float transformedY;

                *(outPtr2++) = transformedX;

                if(transformedX < BBox3D1)
                    BBox3D1 = (int)transformedX;

                if(transformedX > BBox3D3)
                    BBox3D3 = (int)transformedX;

                transformedY = ((Y * cameraFovY) / Z) + cameraCenterY;

                *(outPtr2++) = transformedY;

                if(transformedY < BBox3D2)
                    BBox3D2 = (int)transformedY;

                if(transformedY > BBox3D4)
                    BBox3D4 = (int)transformedY;

                *(outPtr2++) = Z; 
            }
#endif

            k--;
            if(k==0)
            {
                return(1);
            }

        }while(renderVar1 == 0);
    }

    return(0);
}

/*
x - cameraX
y
z - cameraZ
 
*/

int RotateNuage(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    float* outPtr;

    renderX = x - translateX;
    renderY = y;
    renderZ = z - translateZ;

    if(!alpha && !beta && !gamma)
    {
        noModelRotation = true;
    }
    else
    {
        noModelRotation = false;

        modelCosAlpha = cosTable[alpha&0x3FF];
        modelSinAlpha = cosTable[(alpha+0x100)&0x3FF];

        modelCosBeta = cosTable[beta&0x3FF];
        modelSinBeta = cosTable[(beta+0x100)&0x3FF];

        modelCosGamma = cosTable[gamma&0x3FF];
        modelSinGamma = cosTable[(gamma+0x100)&0x3FF];
    }

    outPtr = renderPointList;

    for(int i=0; i<pBody->m_vertices.size(); i++)
    {
        float X = pBody->m_vertices[i].x;
        float Y = pBody->m_vertices[i].y;
        float Z = pBody->m_vertices[i].z;

        if(!noModelRotation)
        {
			// Y rotation
			{
				float tempX = X;
				float tempZ = Z;

				X = (((modelSinBeta * tempX) - (modelCosBeta * tempZ))/65536.f)*2.f;
				Z = (((modelCosBeta * tempX) + (modelSinBeta * tempZ))/65536.f)*2.f;
			}

			// Z rotation
            {
                float tempX = X;
                float tempY = Y;

                X = (((modelSinGamma * tempX) - (modelCosGamma * tempY))/65536.f)*2.f;
                Y = (((modelCosGamma * tempX) + (modelSinGamma * tempY))/65536.f)*2.f;
            }

			// X rotation
            {
                float tempY = Y;
                float tempZ = Z;

                Y = (((modelSinAlpha * tempY) - (modelCosAlpha * tempZ))/65536.f)*2.f;
                Z = (((modelCosAlpha * tempY) + (modelSinAlpha * tempZ))/65536.f)*2.f;
            }
        }

        X += renderX;
        Y += renderY;
        Z += renderZ;

#if defined(AITD_UE4)
        *(outPtr++) = X;
        *(outPtr++) = Y;
        *(outPtr++) = Z;
#else
        if(Y>10000) // height clamp
        {
            *(outPtr++) = -10000;
            *(outPtr++) = -10000;
            *(outPtr++) = -10000;
        }
        else
        {
            float transformedX;
            float transformedY;

            Y -= translateY;

            transformPoint(&X,&Y,&Z);

            Z += cameraPerspective;

            transformedX = ((X * cameraFovX) / Z) + cameraCenterX;

            *(outPtr++) = transformedX;

            if(transformedX < BBox3D1)
                BBox3D1 = (int)transformedX;

            if(transformedX > BBox3D3)
                BBox3D3 = (int)transformedX;

            transformedY = ((Y * cameraFovY) / Z) + cameraCenterY;

            *(outPtr++) = transformedY;

            if(transformedY < BBox3D2)
                BBox3D2 = (int)transformedY;

            if(transformedY > BBox3D4)
                BBox3D4 = (int)transformedY;

            *(outPtr++) = Z; 

            /*  *(outPtr++) = X;
            *(outPtr++) = Y;
            *(outPtr++) = Z; */

        }
#endif
    }
    return(1);
}

char* primVar1;
char* primVar2;

void primFunctionDefault(int primType,char** ptr,char** out)
{
    printf(RNDR_WARN "UnHandled primType %d" CON_RESET "\n",primType);
    assert(0);
}

void processPrim_Line(int primType, sPrimitive* ptr, char** out)
{
    primEntryStruct* pCurrentPrimEntry = &primTable[positionInPrimEntry];

    ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);

    pCurrentPrimEntry->type = primTypeEnum_Line;
    pCurrentPrimEntry->numOfVertices = 2;
    pCurrentPrimEntry->color = ptr->m_color;
    pCurrentPrimEntry->material = ptr->m_material;

    float depth = 32000.f;
    ASSERT(pCurrentPrimEntry->numOfVertices < NUM_MAX_VERTEX_IN_PRIM);
    for (int i = 0; i < pCurrentPrimEntry->numOfVertices; i++)
    {
        u16 pointIndex;

        pointIndex = ptr->m_points[i] * 6;

        ASSERT((pointIndex % 2) == 0);

        pCurrentPrimEntry->vertices[i].X = renderPointList[pointIndex / 2];
        pCurrentPrimEntry->vertices[i].Y = renderPointList[(pointIndex / 2) + 1];
        pCurrentPrimEntry->vertices[i].Z = renderPointList[(pointIndex / 2) + 2];

        if (pCurrentPrimEntry->vertices[i].Z < depth)
            depth = pCurrentPrimEntry->vertices[i].Z;

    }

#if !defined(AITD_UE4)
    if (depth > 100)
#endif
    {
        positionInPrimEntry++;

        numOfPrimitiveToRender++;
        ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
    }
}

void processPrim_Poly(int primType, sPrimitive* ptr, char** out, int originalPrimIndex)
{
    primEntryStruct* pCurrentPrimEntry = &primTable[positionInPrimEntry];

    ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);

    // Preserve original primitive type (important for distinguishing flat vs textured polys)
    pCurrentPrimEntry->type = (primTypeEnum)primType;
    pCurrentPrimEntry->numOfVertices = ptr->m_points.size();
    pCurrentPrimEntry->color = ptr->m_color;
    pCurrentPrimEntry->material = ptr->m_material;
    pCurrentPrimEntry->originalPrimIndex = originalPrimIndex;  // Set directly to avoid indexing issues with depth culling

    // Track if this is a ramp-shaded primitive (material 3-6)
    pCurrentPrimEntry->isRampPrim = (primType == primTypeEnum_Poly && ptr->m_material >= 3 && ptr->m_material <= 6);

    float depth = 32000.f;
    ASSERT(pCurrentPrimEntry->numOfVertices < NUM_MAX_VERTEX_IN_PRIM);
    for (int i = 0; i < pCurrentPrimEntry->numOfVertices; i++)
    {
        u16 pointIndex;

        pointIndex = ptr->m_points[i] * 6;

        ASSERT((pointIndex % 2) == 0);

        pCurrentPrimEntry->vertices[i].X = renderPointList[pointIndex / 2];
        pCurrentPrimEntry->vertices[i].Y = renderPointList[(pointIndex / 2) + 1];
        pCurrentPrimEntry->vertices[i].Z = renderPointList[(pointIndex / 2) + 2];

        if (pCurrentPrimEntry->vertices[i].Z < depth)
            depth = pCurrentPrimEntry->vertices[i].Z;

    }

#if !defined(AITD_UE4)
    if (depth > 100)
#endif
    {
        bool clipPoly = false;

        // Enhanced near-camera polygon clipping:
        // Detect polygons close to the camera that are severely distorted
        // by perspective or span the near-plane region.
        if (depth < NEAR_POLY_CLIP_Z)
        {
            float maxZ = depth;
            float minSX = 1e30f, maxSX = -1e30f;
            float minSY = 1e30f, maxSY = -1e30f;

            for (int j = 0; j < pCurrentPrimEntry->numOfVertices; j++)
            {
                float sx = pCurrentPrimEntry->vertices[j].X;
                float sy = pCurrentPrimEntry->vertices[j].Y;
                float sz = pCurrentPrimEntry->vertices[j].Z;

                // Sentinel vertex from near-plane clipping in AnimNuage
                if (sx <= -9999.f && sy <= -9999.f && sz <= -9999.f)
                {
                    clipPoly = true;
                    break;
                }

                if (sx < minSX) minSX = sx;
                if (sx > maxSX) maxSX = sx;
                if (sy < minSY) minSY = sy;
                if (sy > maxSY) maxSY = sy;
                if (sz > maxZ) maxZ = sz;
            }

            if (!clipPoly)
            {
                // Z range check: large spread means severe perspective distortion
                float zRange = maxZ - depth;
                if (zRange > NEAR_POLY_MAX_Z_RANGE)
                    clipPoly = true;
            }

            if (!clipPoly)
            {
                // Screen extent check: polygon covers too much of the viewport
                float screenW = maxSX - minSX;
                float screenH = maxSY - minSY;
                if (screenW > 320.f * NEAR_POLY_MAX_SCREEN_FRAC ||
                    screenH > 200.f * NEAR_POLY_MAX_SCREEN_FRAC)
                {
                    clipPoly = true;
                }
            }
        }

        if (!clipPoly)
        {
            positionInPrimEntry++;

            numOfPrimitiveToRender++;
            ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
        }
    }
}

void processPrim_Point(primTypeEnum primType, sPrimitive* ptr, char** out)
{
    primEntryStruct* pCurrentPrimEntry = &primTable[positionInPrimEntry];

    ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);

    pCurrentPrimEntry->type = primType;
    pCurrentPrimEntry->numOfVertices = 1;
    pCurrentPrimEntry->color = ptr->m_color;
    pCurrentPrimEntry->material = ptr->m_material;

    float depth = 32000.f;
    {
        u16 pointIndex;
        pointIndex = ptr->m_points[0] * 6;
        ASSERT((pointIndex % 2) == 0);
        pCurrentPrimEntry->vertices[0].X = renderPointList[pointIndex / 2];
        pCurrentPrimEntry->vertices[0].Y = renderPointList[(pointIndex / 2) + 1];
        pCurrentPrimEntry->vertices[0].Z = renderPointList[(pointIndex / 2) + 2];

        depth = pCurrentPrimEntry->vertices[0].Z;
    }

#if !defined(AITD_UE4)
    if (depth > 100)
#endif
    {
        positionInPrimEntry++;

        numOfPrimitiveToRender++;
        ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
    }
}

void processPrim_Sphere(int primType, sPrimitive* ptr, char** out)
{
    primEntryStruct* pCurrentPrimEntry = &primTable[positionInPrimEntry];

    ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);

    pCurrentPrimEntry->type = primTypeEnum_Sphere;
    pCurrentPrimEntry->numOfVertices = 1;
    pCurrentPrimEntry->color = ptr->m_color;
    pCurrentPrimEntry->material = ptr->m_material;
    pCurrentPrimEntry->size = ptr->m_size;

    float depth = 32000.f;
    {
        u16 pointIndex;
        pointIndex = ptr->m_points[0] * 6;
        ASSERT((pointIndex % 2) == 0);
        pCurrentPrimEntry->vertices[0].X = renderPointList[pointIndex / 2];
        pCurrentPrimEntry->vertices[0].Y = renderPointList[(pointIndex / 2) + 1];
        pCurrentPrimEntry->vertices[0].Z = renderPointList[(pointIndex / 2) + 2];

        depth = pCurrentPrimEntry->vertices[0].Z;
    }

#if !defined(AITD_UE4)
    if (depth > 100)
#endif
    {
        positionInPrimEntry++;

        numOfPrimitiveToRender++;
        ASSERT(positionInPrimEntry < NUM_MAX_PRIM_ENTRY);
    }
}

void primType5(int primType, char** ptr, char** out) // draw out of hardClip
{
	printf(RNDR_WARN "ignoring prim type 5" CON_RESET "\n");
	return;

    int pointNumber;
    s16 ax2;

    primVar1 = *out;

    *(s16*)(*out) = *(s16*)(*ptr);
    *out+=2;
    *ptr+=3;

    pointNumber = *(s16*)(*ptr);
    *ptr+=2;

    // here, should check for clip on X Y Z

    *(float*)(*out) = renderPointList[pointNumber/2]; // X
    *out+=sizeof(float);
    *(float*)(*out) = renderPointList[pointNumber/2+1]; // Y
    *out+=sizeof(float);
    *(float*)(*out) = renderPointList[pointNumber/2+2]; // Z
    ax2 = (s16)(*(float*)(*out));
    *out+=sizeof(float);

    primVar2 = *out;

    {
        numOfPrimitiveToRender++;

        *out = renderVar2;

        *(s16*)(*out) = ax2;
        *out+=2;
        *(s16*)(*out) = ax2;
        *out+=2;
        *(s16*)(*out) = primType;
        *out+=2;

        *(char**)(*out) = primVar1;
        *out+=4;

        renderVar2 = *out;
        *out = primVar2;
    }
}

void line(int x1, int y1, int x2, int y2, char c);

// Mirror UV into [0,1] range so ramp textures never sample transparent atlas padding
static float mirrorUV(float v)
{
    if (v < 0.0f) v = -v;
    int i = (int)v;
    float frac = v - (float)i;
    if (i & 1) frac = 1.0f - frac;
    // Keep a small margin away from exact edges to avoid bleeding
    const float eps = 0.005f;
    if (frac < eps) frac = eps;
    if (frac > 1.0f - eps) frac = 1.0f - eps;
    return frac;
}

// Sample CPU-side RGBA pixel data at a UV coordinate and return true if transparent (alpha == 0)
static bool isAtlasUVTransparent(const std::vector<unsigned char>& pixelData, int texW, int texH, float u, float v)
{
    if (pixelData.empty() || texW <= 0 || texH <= 0)
        return true;
    int px = (int)(u * (float)texW);
    int py = (int)(v * (float)texH);
    if (px < 0) px = 0; if (px >= texW) px = texW - 1;
    if (py < 0) py = 0; if (py >= texH) py = texH - 1;
    int idx = (py * texW + px) * 4;
    return pixelData[idx + 3] == 0;
}

void renderLine(primEntryStruct* pEntry) // line
{
    osystem_draw3dLine( pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,
						pEntry->vertices[1].X,pEntry->vertices[1].Y,pEntry->vertices[1].Z,
						pEntry->color);
}

void renderPoly(primEntryStruct* pEntry) // poly
{
    // Multi-layered rendering: always render the flat base color first, then overlay
    // the atlas texture on top. Transparent atlas pixels naturally let the base show through.

    // Always render the base material layer first
    osystem_fillPoly((float*)pEntry->vertices, pEntry->numOfVertices, pEntry->color, pEntry->material);

    if (!s_currentAtlas)
        return;

    // Overlay atlas texture on top of the base layer where UVs are available

    // Textured polygon primitives (type 9/10) - use flat atlas overlay
    if (pEntry->type == processPrim_PolyTexture9 || pEntry->type == processPrim_PolyTexture10)
    {
        if (bgfx::isValid(s_currentAtlas->flatTexture)
            && pEntry->originalPrimIndex < (int)s_currentAtlas->flatPolyUVs.size())
        {
            AtlasPolyUVs& uvs = s_currentAtlas->flatPolyUVs[pEntry->originalPrimIndex];
            if (!uvs.u.empty() && (int)uvs.u.size() >= pEntry->numOfVertices)
            {
                float uvArray[NUM_MAX_VERTEX_IN_PRIM * 2];
                for (int i = 0; i < pEntry->numOfVertices; i++)
                {
                    uvArray[i * 2 + 0] = uvs.u[i];
                    uvArray[i * 2 + 1] = uvs.v[i];
                }
                osystem_fillPolyTextured((float*)pEntry->vertices, pEntry->numOfVertices, uvArray, s_currentAtlas->flatTexture);
            }
        }
        return;
    }

    // Flat-shaded polygons (material 0) - try flat atlas then main atlas overlay
    if (pEntry->type == primTypeEnum_Poly && pEntry->material == 0)
    {
        if (bgfx::isValid(s_currentAtlas->flatTexture)
            && pEntry->originalPrimIndex < (int)s_currentAtlas->flatPolyUVs.size())
        {
            AtlasPolyUVs& uvs = s_currentAtlas->flatPolyUVs[pEntry->originalPrimIndex];
            if (!uvs.u.empty() && (int)uvs.u.size() >= pEntry->numOfVertices)
            {
                float uvArray[NUM_MAX_VERTEX_IN_PRIM * 2];
                for (int i = 0; i < pEntry->numOfVertices; i++)
                {
                    uvArray[i * 2 + 0] = uvs.u[i];
                    uvArray[i * 2 + 1] = uvs.v[i];
                }
                osystem_fillPolyTextured((float*)pEntry->vertices, pEntry->numOfVertices, uvArray, s_currentAtlas->flatTexture);
                return;
            }
        }

        if (bgfx::isValid(s_currentAtlas->texture)
            && pEntry->originalPrimIndex < (int)s_currentAtlas->polyUVs.size())
        {
            AtlasPolyUVs& uvs = s_currentAtlas->polyUVs[pEntry->originalPrimIndex];
            if (!uvs.u.empty() && (int)uvs.u.size() >= pEntry->numOfVertices)
            {
                float uvArray[NUM_MAX_VERTEX_IN_PRIM * 2];
                for (int i = 0; i < pEntry->numOfVertices; i++)
                {
                    uvArray[i * 2 + 0] = uvs.u[i];
                    uvArray[i * 2 + 1] = uvs.v[i];
                }
                osystem_fillPolyTextured((float*)pEntry->vertices, pEntry->numOfVertices, uvArray, s_currentAtlas->texture);
            }
        }
        return;
    }

    // Ramp-shaded polygons (material 3-6) - ramp atlas overlay with mirrored UVs
    if (pEntry->type == primTypeEnum_Poly && pEntry->isRampPrim
        && pEntry->material >= 3 && pEntry->material <= 6)
    {
        if (bgfx::isValid(s_currentAtlas->rampTexture)
            && pEntry->originalPrimIndex < (int)s_currentAtlas->rampPolyUVs.size())
        {
            AtlasPolyUVs& uvs = s_currentAtlas->rampPolyUVs[pEntry->originalPrimIndex];
            if (!uvs.u.empty() && (int)uvs.u.size() >= pEntry->numOfVertices)
            {
                float uvArray[NUM_MAX_VERTEX_IN_PRIM * 2];
                for (int i = 0; i < pEntry->numOfVertices; i++)
                {
                    uvArray[i * 2 + 0] = mirrorUV(uvs.u[i]);
                    uvArray[i * 2 + 1] = mirrorUV(uvs.v[i]);
                }
                osystem_fillPolyTextured((float*)pEntry->vertices, pEntry->numOfVertices, uvArray, s_currentAtlas->rampTexture);
            }
        }
        return;
    }

    // Other material polygons (material 1: dither, material 2: transparent) - other atlas overlay
    if (pEntry->type == primTypeEnum_Poly && (pEntry->material == 1 || pEntry->material == 2))
    {
        if (bgfx::isValid(s_currentAtlas->otherTexture)
            && pEntry->originalPrimIndex < (int)s_currentAtlas->otherPolyUVs.size())
        {
            AtlasPolyUVs& uvs = s_currentAtlas->otherPolyUVs[pEntry->originalPrimIndex];
            if (!uvs.u.empty() && (int)uvs.u.size() >= pEntry->numOfVertices)
            {
                float uvArray[NUM_MAX_VERTEX_IN_PRIM * 2];
                for (int i = 0; i < pEntry->numOfVertices; i++)
                {
                    uvArray[i * 2 + 0] = uvs.u[i];
                    uvArray[i * 2 + 1] = uvs.v[i];
                }
                osystem_fillPolyTextured((float*)pEntry->vertices, pEntry->numOfVertices, uvArray, s_currentAtlas->otherTexture);
            }
        }
        return;
    }
}

void renderZixel(primEntryStruct* pEntry) // point
{
    static float pointSize = 20.f;
    float transformedSize = ((pointSize * (float)cameraFovX) / (float)(pEntry->vertices[0].Z+cameraPerspective));

    osystem_drawPoint(pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,pEntry->color,pEntry->material, transformedSize);
}

void renderPoint(primEntryStruct* pEntry) // point
{
    static float pointSize = 0.3f; // TODO: better way to compute that?
    osystem_drawPoint(pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,pEntry->color, pEntry->material, pointSize);
}

void renderBigPoint(primEntryStruct* pEntry) // point
{
    static float bigPointSize = 1.f; // TODO: better way to compute that?
    osystem_drawPoint(pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,pEntry->color, pEntry->material, bigPointSize);
}

void renderSphere(primEntryStruct* pEntry) // sphere
{
    float transformedSize;

    transformedSize = (((float)pEntry->size * (float)cameraFovX) / (float)(pEntry->vertices[0].Z+cameraPerspective));

    numSpheresPrimitives++;  // Track sphere index for atlas grid lookup

    osystem_drawSphere(pEntry->vertices[0].X,pEntry->vertices[0].Y,pEntry->vertices[0].Z,pEntry->color, pEntry->material, transformedSize);
}


void defaultRenderFunction(primEntryStruct* buffer)
{
    printf(RNDR_WARN "Unsupported renderType" CON_RESET "\n");
}

typedef void (*renderFunction)(primEntryStruct* buffer);

renderFunction renderFunctions[]={
	renderLine, // line (0)
	renderPoly, // poly (1)
	renderPoint, // point (2)
	renderSphere, // sphere (3)
	nullptr, // disk (4)
	nullptr, // cylinder (5)
	renderBigPoint, // bigpoint (6)
	renderZixel, // zixel (7)
	nullptr, // polyTexture8 (8) - unsupported
	renderPoly, // polyTexture9 (9) - textured polygon
	renderPoly, // polyTexture10 (10) - textured polygon
};

void setCurrentBodyNum(int bodyNum, sBody* pBody, const std::string& hqrName)
{
    // Only load texture atlases when HD backgrounds are enabled
    if (isHDBackgroundEnabled())
    {
        s_currentAtlas = loadModelAtlas(bodyNum, pBody, hqrName);
    }
    else
    {
        s_currentAtlas = nullptr;
    }
}

int AffObjet(int x,int y,int z,int alpha,int beta,int gamma, sBody* pBody)
{
    if (!pBody)
        return 2;

    int numPrim;
    int i;
    char* out;

    // reinit the 2 static tables
    positionInPrimEntry = 0;
    s_modelHasTexturedPrims = false;
    // Clear primitive table to ensure fields are initialized
    memset(primTable, 0, sizeof(primTable));
    //

    BBox3D1 = 0x7FFF;
    BBox3D2 = 0x7FFF;

    BBox3D3 = -0x7FFF;
    BBox3D4 = -0x7FFF;

    renderVar1 = 0;

    numOfPrimitiveToRender = 0;

    renderVar2 = renderBuffer;

    modelFlags = pBody->m_flags;

    if(modelFlags&INFO_ANIM)
    {
        if(!AnimNuage(x,y,z,alpha,beta,gamma, pBody))
        {
            BBox3D3 = -32000;
            BBox3D4 = -32000;
            BBox3D1 = 32000;
            BBox3D2 = 32000;
            return(2);
        }
    }
    else
        if(!(modelFlags&INFO_TORTUE))
        {
            if(!RotateNuage(x,y,z,alpha,beta,gamma, pBody))
            {
                BBox3D3 = -32000;
                BBox3D4 = -32000;
                BBox3D1 = 32000;
                BBox3D2 = 32000;
                return(2);
            }
        }
        else
        {
            printf(RNDR_WARN "unsupported model type prerenderFlag4 in renderer !" CON_RESET "\n");

            BBox3D3 = -32000;
            BBox3D4 = -32000;
            BBox3D1 = 32000;
            BBox3D2 = 32000;
            return(2);
        }

        numPrim = pBody->m_primitives.size();

        if(!numPrim)
        {
            BBox3D3 = -32000;
            BBox3D4 = -32000;
            BBox3D1 = 32000;
            BBox3D2 = 32000;
            return(2);
        }

		out = primBuffer;

		// Pre-scan: detect if model has textured primitives (must be done before processing)
		for(i=0;i<numPrim;i++)
		{
			primTypeEnum primType = pBody->m_primitives[i].m_type;
			if (primType == processPrim_PolyTexture9 || primType == processPrim_PolyTexture10)
			{
				s_modelHasTexturedPrims = true;
				break;
			}
		}

		// create the list of all primitives to render
		for(i=0;i<numPrim;i++)
		{
			sPrimitive* pPrimitive = &pBody->m_primitives[i];
			primTypeEnum primType = pPrimitive->m_type;

			u32 prevPos = positionInPrimEntry;

			switch(primType)
			{
			case primTypeEnum_Line:
				processPrim_Line(primType, pPrimitive,&out);
				break;
			case primTypeEnum_Poly:
				processPrim_Poly(primType, pPrimitive, &out, i);
				break;
			case primTypeEnum_Point:
			case primTypeEnum_BigPoint:
			case primTypeEnum_Zixel:
				processPrim_Point(primType, pPrimitive,&out);
				break;
			case primTypeEnum_Sphere:
				processPrim_Sphere(primType, pPrimitive,&out);
				break;
			case processPrim_PolyTexture9:
			case processPrim_PolyTexture10:
				processPrim_Poly(primType, pPrimitive, &out, i);
				break;
			default:
				return 0;
				assert(0);
			}

			// Track which original primitive this entry came from (for atlas UV lookup)
			// NOTE: originalPrimIndex is now set directly in processPrim_Poly to avoid
			// index mismatches when depth culling filters out primitives
			if (positionInPrimEntry > prevPos)
			{
				// Verify the index was set correctly (should be set for poly types)
				ASSERT(primTable[prevPos].originalPrimIndex == i || 
					   (primType != primTypeEnum_Poly && primType != processPrim_PolyTexture9 && primType != processPrim_PolyTexture10));
			}
		}

		if(!numOfPrimitiveToRender)
		{
			BBox3D3 = -32000;
			BBox3D4 = -32000;
			BBox3D1 = 32000;
			BBox3D2 = 32000;
			return(1); // model ok, but out of screen
		}

		// --- Floating detail polygon detection ---
		// Detect small detail polygons (eyes, nostrils) that overlap larger
		// polygons (face). Uses rest-pose model-space area to identify tiny
		// polygons, then checks screen-space bbox containment to confirm they
		// sit within a larger polygon. Works at any distance and camera angle.
		//
		// Key discriminator: vertex isolation. Floating geometry (eyes) uses
		// vertices that no other polygon in the model references. Connected
		// mesh polygons (lips, chin, cheek) share vertices with neighbors.
		{
			// Build per-vertex reference count across ALL model primitives.
			// Connected mesh vertices are referenced by multiple polygons (refcount >= 2).
			// Floating geometry vertices are referenced by only one polygon (refcount == 1).
			u8 vertexRefCount[NUM_MAX_POINT_IN_POINT_BUFFER];
			memset(vertexRefCount, 0, sizeof(vertexRefCount));
			for (int pk = 0; pk < numPrim; pk++)
			{
				sPrimitive& prim = pBody->m_primitives[pk];
				for (int v = 0; v < (int)prim.m_points.size(); v++)
				{
					u16 vi = prim.m_points[v];
					if (vi < NUM_MAX_POINT_IN_POINT_BUFFER && vertexRefCount[vi] < 255)
						vertexRefCount[vi]++;
				}
			}

			float polyAreas[NUM_MAX_PRIM_ENTRY];
			memset(polyAreas, 0, sizeof(polyAreas));

			// Compute rest-pose model-space area for each polygon
			for (int pi = 0; pi < numOfPrimitiveToRender; pi++)
			{
				primEntryStruct& pe = primTable[pi];
				if (pe.numOfVertices < 3) continue;
				if (pe.type != primTypeEnum_Poly &&
					pe.type != processPrim_PolyTexture9 &&
					pe.type != processPrim_PolyTexture10) continue;

				int origIdx = pe.originalPrimIndex;
				if (origIdx < 0 || origIdx >= (int)pBody->m_primitives.size())
					continue;
				sPrimitive& origPrim = pBody->m_primitives[origIdx];
				if ((int)origPrim.m_points.size() < 3)
					continue;

				int vi0 = origPrim.m_points[0];
				int vi1 = origPrim.m_points[1];
				int vi2 = origPrim.m_points[2];
				if (vi0 >= (int)pBody->m_vertices.size() ||
					vi1 >= (int)pBody->m_vertices.size() ||
					vi2 >= (int)pBody->m_vertices.size())
					continue;

				float e1x = (float)(pBody->m_vertices[vi1].x - pBody->m_vertices[vi0].x);
				float e1y = (float)(pBody->m_vertices[vi1].y - pBody->m_vertices[vi0].y);
				float e1z = (float)(pBody->m_vertices[vi1].z - pBody->m_vertices[vi0].z);
				float e2x = (float)(pBody->m_vertices[vi2].x - pBody->m_vertices[vi0].x);
				float e2y = (float)(pBody->m_vertices[vi2].y - pBody->m_vertices[vi0].y);
				float e2z = (float)(pBody->m_vertices[vi2].z - pBody->m_vertices[vi0].z);
				float crx = e1y * e2z - e1z * e2y;
				float cry = e1z * e2x - e1x * e2z;
				float crz = e1x * e2y - e1y * e2x;
				polyAreas[pi] = sqrtf(crx * crx + cry * cry + crz * crz) * 0.5f;
			}

			// Flag small, isolated polygons whose screen bbox is contained
			// within a larger polygon (floating detail geometry)
			for (int pi = 0; pi < numOfPrimitiveToRender; pi++)
			{
				primEntryStruct& pe = primTable[pi];
				if (pe.numOfVertices < 3) continue;
				if (pe.type != primTypeEnum_Poly &&
					pe.type != processPrim_PolyTexture9 &&
					pe.type != processPrim_PolyTexture10) continue;
				if (polyAreas[pi] >= FLOATING_POLY_AREA_MAX || polyAreas[pi] <= 0.f)
					continue;

				int origIdxP = pe.originalPrimIndex;
				if (origIdxP < 0 || origIdxP >= (int)pBody->m_primitives.size())
					continue;
				sPrimitive& origPrimP = pBody->m_primitives[origIdxP];

				// Vertex isolation check: floating geometry uses vertices that
				// no other polygon references. Connected mesh polygons share
				// vertices with neighbors (refcount >= 2). Only consider
				// polygons where ALL vertices are exclusive (refcount == 1).
				{
					bool isIsolated = true;
					for (int v = 0; v < (int)origPrimP.m_points.size(); v++)
					{
						u16 vi = origPrimP.m_points[v];
						if (vi < NUM_MAX_POINT_IN_POINT_BUFFER && vertexRefCount[vi] > 1)
						{
							isIsolated = false;
							break;
						}
					}
					if (!isIsolated) continue;
				}

				// Check rest-pose model-space extent
				{
					float bMinX = 1e30f, bMaxX = -1e30f;
					float bMinY = 1e30f, bMaxY = -1e30f;
					float bMinZ = 1e30f, bMaxZ = -1e30f;
					for (int v = 0; v < (int)origPrimP.m_points.size(); v++)
					{
						int vi = origPrimP.m_points[v];
						if (vi >= (int)pBody->m_vertices.size()) continue;
						float mx = (float)pBody->m_vertices[vi].x;
						float my = (float)pBody->m_vertices[vi].y;
						float mz = (float)pBody->m_vertices[vi].z;
						if (mx < bMinX) bMinX = mx; if (mx > bMaxX) bMaxX = mx;
						if (my < bMinY) bMinY = my; if (my > bMaxY) bMaxY = my;
						if (mz < bMinZ) bMinZ = mz; if (mz > bMaxZ) bMaxZ = mz;
					}
					float maxExt = bMaxX - bMinX;
					if (bMaxY - bMinY > maxExt) maxExt = bMaxY - bMinY;
					if (bMaxZ - bMinZ > maxExt) maxExt = bMaxZ - bMinZ;
					if (maxExt > FLOATING_POLY_MAX_EXTENT)
						continue;
				}

				// Screen bounding box of the small polygon
				float pMinX = 1e30f, pMaxX = -1e30f;
				float pMinY = 1e30f, pMaxY = -1e30f;
				bool pValid = true;
				for (int v = 0; v < pe.numOfVertices; v++)
				{
					if (pe.vertices[v].X <= -9999.f) { pValid = false; break; }
					if (pe.vertices[v].X < pMinX) pMinX = pe.vertices[v].X;
					if (pe.vertices[v].X > pMaxX) pMaxX = pe.vertices[v].X;
					if (pe.vertices[v].Y < pMinY) pMinY = pe.vertices[v].Y;
					if (pe.vertices[v].Y > pMaxY) pMaxY = pe.vertices[v].Y;
				}
				if (!pValid) continue;

				// Check against every larger polygon
				for (int pj = 0; pj < numOfPrimitiveToRender; pj++)
				{
					if (pj == pi) continue;
					if (polyAreas[pj] < polyAreas[pi] * FLOATING_POLY_AREA_RATIO)
						continue;

					primEntryStruct& qe = primTable[pj];
					if (qe.numOfVertices < 3) continue;

					float qMinX = 1e30f, qMaxX = -1e30f;
					float qMinY = 1e30f, qMaxY = -1e30f;
					bool qValid = true;
					for (int v = 0; v < qe.numOfVertices; v++)
					{
						if (qe.vertices[v].X <= -9999.f) { qValid = false; break; }
						if (qe.vertices[v].X < qMinX) qMinX = qe.vertices[v].X;
						if (qe.vertices[v].X > qMaxX) qMaxX = qe.vertices[v].X;
						if (qe.vertices[v].Y < qMinY) qMinY = qe.vertices[v].Y;
						if (qe.vertices[v].Y > qMaxY) qMaxY = qe.vertices[v].Y;
					}
					if (!qValid) continue;

					const float margin = 2.0f;
					if (pMinX >= qMinX - margin && pMaxX <= qMaxX + margin &&
						pMinY >= qMinY - margin && pMaxY <= qMaxY + margin)
					{
						pe.nearClipped = true;
						break;
					}
				}
			}
		}

				// Render primitives in order
		for(i=0;i<numOfPrimitiveToRender;i++)
		{
			if (primTable[i].nearClipped)
				continue;
			int type = primTable[i].type;
			if(type >= 0 && type < (int)(sizeof(renderFunctions)/sizeof(renderFunctions[0])) && renderFunctions[type])
			{
				renderFunctions[type](&primTable[i]);
			}
		}

        osystem_flushPendingPrimitives();
        return(0);
}

void computeScreenBox(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr)
{
    BBox3D1 = 0x7FFF;
    BBox3D2 = 0x7FFF;

    BBox3D3 = -0x7FFF;
    BBox3D4 = -0x7FFF;

    renderVar1 = 0;

    numOfPrimitiveToRender = 0;

    renderVar2 = renderBuffer;

    modelFlags = bodyPtr->m_flags;

    if(modelFlags&INFO_ANIM)
    {
        AnimNuage(x,y,z,alpha,beta,gamma, bodyPtr);
    }
}

#define SHADOW_SEGMENTS 36

void drawBlobShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr)
{
    float radiusX = (float)(bodyPtr->m_zv.ZVX2 - bodyPtr->m_zv.ZVX1) * 0.45f;
    float radiusZ = (float)(bodyPtr->m_zv.ZVZ2 - bodyPtr->m_zv.ZVZ1) * 0.45f;

    if (radiusX < 10.f || radiusZ < 10.f)
        return;

    float screenVertices[SHADOW_SEGMENTS * 3];
    int numValidVertices = 0;

    for (int i = 0; i < SHADOW_SEGMENTS; i++)
    {
        float angle = (float)i * 2.0f * 3.14159265f / (float)SHADOW_SEGMENTS;

        float X = (float)x + radiusX * cosf(angle) - (float)translateX;
        float Y = (float)y - (float)translateY;
        float Z = (float)z + radiusZ * sinf(angle) - (float)translateZ;

        transformPoint(&X, &Y, &Z);

        Z += (float)cameraPerspective;

        if (Z <= 50.0f)
            return;

        float screenX = ((X * (float)cameraFovX) / Z) + (float)cameraCenterX;
        float screenY = ((Y * (float)cameraFovY) / Z) + (float)cameraCenterY;

        if (screenX < -200.f || screenX > 520.f || screenY < -200.f || screenY > 400.f)
            return;

        screenVertices[numValidVertices * 3 + 0] = screenX;
        screenVertices[numValidVertices * 3 + 1] = screenY;
        screenVertices[numValidVertices * 3 + 2] = Z + 1.0f;
        numValidVertices++;
    }

    if (numValidVertices >= 3)
    {
        float signedArea = 0;
        for (int i = 0; i < numValidVertices; i++)
        {
            int next = (i + 1) % numValidVertices;
            signedArea += screenVertices[i * 3] * screenVertices[next * 3 + 1]
                        - screenVertices[next * 3] * screenVertices[i * 3 + 1];
        }
        if (signedArea > 0)
        {
            for (int i = 0; i < numValidVertices / 2; i++)
            {
                int j = numValidVertices - 1 - i;
                float tx = screenVertices[i * 3];     screenVertices[i * 3]     = screenVertices[j * 3];     screenVertices[j * 3]     = tx;
                float ty = screenVertices[i * 3 + 1]; screenVertices[i * 3 + 1] = screenVertices[j * 3 + 1]; screenVertices[j * 3 + 1] = ty;
                float tz = screenVertices[i * 3 + 2]; screenVertices[i * 3 + 2] = screenVertices[j * 3 + 2]; screenVertices[j * 3 + 2] = tz;
            }
        }

        g_submitShadowStencil = true;
        osystem_fillPoly(screenVertices, numValidVertices, 0x01, 2);
        osystem_flushPendingPrimitives();
        g_submitShadowStencil = false;
    }
}

void drawPlanarShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr)
{
    if (!g_planarShadowEnabled)
        return;

    if (g_shadowLightDirY == 0.0f)
        return;

    const bool isAnimated = (bodyPtr->m_flags & INFO_ANIM) != 0;

    // model rotation trig (mirrors RotateNuage setup)
    bool localNoRot;
    int localCosA = 0, localSinA = 0;
    int localCosB = 0, localSinB = 0;
    int localCosG = 0, localSinG = 0;

    if (!alpha && !beta && !gamma)
    {
        localNoRot = true;
    }
    else
    {
        localNoRot = false;
        localCosA = cosTable[alpha & 0x3FF];
        localSinA = cosTable[(alpha + 0x100) & 0x3FF];
        localCosB = cosTable[beta & 0x3FF];
        localSinB = cosTable[(beta + 0x100) & 0x3FF];
        localCosG = cosTable[gamma & 0x3FF];
        localSinG = cosTable[(gamma + 0x100) & 0x3FF];
    }

    const float worldOffsetX = (float)(x - translateX);
    const float worldOffsetY = (float)y;
    const float worldOffsetZ = (float)(z - translateZ);

    for (int pi = 0; pi < (int)bodyPtr->m_primitives.size(); pi++)
    {
        const sPrimitive* pPrim = &bodyPtr->m_primitives[pi];
        if (pPrim->m_type != primTypeEnum_Poly)
            continue;

        const int numVerts = (int)pPrim->m_points.size();
        if (numVerts < 3 || numVerts >= NUM_MAX_VERTEX_IN_PRIM)
            continue;

        float shadowVerts[NUM_MAX_VERTEX_IN_PRIM * 3];
        bool valid = true;

        for (int vi = 0; vi < numVerts; vi++)
        {
            const u16 vertexIdx = pPrim->m_points[vi];

            float localX, localY, localZ;

            if (isAnimated)
            {
                localX = (float)pointBuffer[vertexIdx].x;
                localY = (float)pointBuffer[vertexIdx].y;
                localZ = (float)pointBuffer[vertexIdx].z;
            }
            else
            {
                localX = (float)bodyPtr->m_vertices[vertexIdx].x;
                localY = (float)bodyPtr->m_vertices[vertexIdx].y;
                localZ = (float)bodyPtr->m_vertices[vertexIdx].z;

                if (!localNoRot)
                {
                    // Y rotation
                    {
                        float tx = localX, tz = localZ;
                        localX = (((localSinB * tx) - (localCosB * tz)) / 65536.f) * 2.f;
                        localZ = (((localCosB * tx) + (localSinB * tz)) / 65536.f) * 2.f;
                    }
                    // Z rotation
                    {
                        float tx = localX, ty = localY;
                        localX = (((localSinG * tx) - (localCosG * ty)) / 65536.f) * 2.f;
                        localY = (((localCosG * tx) + (localSinG * ty)) / 65536.f) * 2.f;
                    }
                    // X rotation
                    {
                        float ty = localY, tz = localZ;
                        localY = (((localSinA * ty) - (localCosA * tz)) / 65536.f) * 2.f;
                        localZ = (((localCosA * ty) + (localSinA * tz)) / 65536.f) * 2.f;
                    }
                }
            }

            // Project vertex onto floor (worldY = y, i.e. localY = 0) along light direction.
            // t = -localY / lightDirY  (floor is at local Y = 0)
            const float t = -localY / g_shadowLightDirY;
            const float shadowLocalX = localX + t * g_shadowLightDirX;
            const float shadowLocalZ = localZ + t * g_shadowLightDirZ;

            // Camera-space transform (same pipeline as RotateNuage/drawBlobShadow)
            float X = shadowLocalX + worldOffsetX;
            float Y = (float)y - (float)translateY;   // floor at actor base Y
            float Z = shadowLocalZ + worldOffsetZ;

            transformPoint(&X, &Y, &Z);

            Z += (float)cameraPerspective;

            if (Z <= 50.0f)
            {
                valid = false;
                break;
            }

            float screenX = ((X * (float)cameraFovX) / Z) + (float)cameraCenterX;
            float screenY = ((Y * (float)cameraFovY) / Z) + (float)cameraCenterY;

            if (screenX < -320.f || screenX > 640.f || screenY < -200.f || screenY > 400.f)
            {
                valid = false;
                break;
            }

            shadowVerts[vi * 3 + 0] = screenX;
            shadowVerts[vi * 3 + 1] = screenY;
            shadowVerts[vi * 3 + 2] = Z + 5.0f;  // push slightly behind model polys
        }

        if (valid)
        {
            osystem_fillPoly(shadowVerts, numVerts, 0x00, 2);
        }
    }

    g_submitShadowStencil = true;
    osystem_flushPendingPrimitives();
    g_submitShadowStencil = false;
}

// Maximum distance (in world units) from actor to consider a wall for shadow projection
#define WALL_SHADOW_MAX_DIST 2000.0f

void drawWallPlanarShadow(int x, int y, int z, int alpha, int beta, int gamma, sBody* bodyPtr, int actorRoom)
{
    if (!g_wallShadowEnabled || !g_planarShadowEnabled)
        return;

    if (g_shadowLightDirY == 0.0f)
        return;

    if (actorRoom < 0 || actorRoom >= (int)roomDataTable.size())
        return;

    const roomDataStruct& roomData = roomDataTable[actorRoom];

    // Convert wall coordinates from room-local to current-room world space
    int roomOffX = 0, roomOffY = 0, roomOffZ = 0;
    if (actorRoom != currentRoom)
    {
        roomOffX = (int)((roomDataTable[actorRoom].worldX - roomDataTable[currentRoom].worldX) * 10);
        roomOffY = (int)((roomDataTable[currentRoom].worldY - roomDataTable[actorRoom].worldY) * 10);
        roomOffZ = (int)((roomDataTable[currentRoom].worldZ - roomDataTable[actorRoom].worldZ) * 10);
    }

    const bool isAnimated = (bodyPtr->m_flags & INFO_ANIM) != 0;

    // Model rotation trig (same as drawPlanarShadow)
    bool localNoRot;
    int localCosA = 0, localSinA = 0;
    int localCosB = 0, localSinB = 0;
    int localCosG = 0, localSinG = 0;

    if (!alpha && !beta && !gamma)
    {
        localNoRot = true;
    }
    else
    {
        localNoRot = false;
        localCosA = cosTable[alpha & 0x3FF];
        localSinA = cosTable[(alpha + 0x100) & 0x3FF];
        localCosB = cosTable[beta & 0x3FF];
        localSinB = cosTable[(beta + 0x100) & 0x3FF];
        localCosG = cosTable[gamma & 0x3FF];
        localSinG = cosTable[(gamma + 0x100) & 0x3FF];
    }

    const float actorWorldX = (float)x;
    const float actorWorldY = (float)y;
    const float actorWorldZ = (float)z;

    for (u32 wi = 0; wi < roomData.numHardCol; wi++)
    {
        const hardColStruct& hardCol = roomData.hardColTable[wi];
        if (hardCol.type != 1) // only walls
            continue;

        // Wall AABB in world space (current room coordinates)
        float wallX1 = (float)(hardCol.zv.ZVX1 * 10 - roomOffX);
        float wallX2 = (float)(hardCol.zv.ZVX2 * 10 - roomOffX);
        float wallY1 = (float)(hardCol.zv.ZVY1 * 10 + roomOffY);
        float wallY2 = (float)(hardCol.zv.ZVY2 * 10 + roomOffY);
        float wallZ1 = (float)(hardCol.zv.ZVZ1 * 10 + roomOffZ);
        float wallZ2 = (float)(hardCol.zv.ZVZ2 * 10 + roomOffZ);

        // Skip walls too far from actor
        float closestX = actorWorldX < wallX1 ? wallX1 : (actorWorldX > wallX2 ? wallX2 : actorWorldX);
        float closestZ = actorWorldZ < wallZ1 ? wallZ1 : (actorWorldZ > wallZ2 ? wallZ2 : actorWorldZ);
        float dx = actorWorldX - closestX;
        float dz = actorWorldZ - closestZ;
        if (dx * dx + dz * dz > WALL_SHADOW_MAX_DIST * WALL_SHADOW_MAX_DIST)
            continue;

        // Determine which face(s) of the wall AABB face the actor.
        // We project onto each facing vertical face separately.
        // For each face we define: axis (0=X, 2=Z), plane coordinate,
        // and the perpendicular min/max bounds + vertical bounds.
        struct WallFace
        {
            int axis;       // 0 = X-plane, 2 = Z-plane
            float planeVal; // the X or Z coordinate of this face
            float perpMin;  // min bound on the perpendicular axis
            float perpMax;  // max bound on the perpendicular axis
            float yMin;     // vertical min
            float yMax;     // vertical max
        };

        WallFace faces[4];
        int numFaces = 0;

        // -X face (normal pointing -X): actor must be at X < wallX1
        if (actorWorldX < wallX1)
            faces[numFaces++] = { 0, wallX1, wallZ1, wallZ2, wallY1, wallY2 };
        // +X face (normal pointing +X): actor must be at X > wallX2
        if (actorWorldX > wallX2)
            faces[numFaces++] = { 0, wallX2, wallZ1, wallZ2, wallY1, wallY2 };
        // -Z face (normal pointing -Z): actor must be at Z < wallZ1
        if (actorWorldZ < wallZ1)
            faces[numFaces++] = { 2, wallZ1, wallX1, wallX2, wallY1, wallY2 };
        // +Z face (normal pointing +Z): actor must be at Z > wallZ2
        if (actorWorldZ > wallZ2)
            faces[numFaces++] = { 2, wallZ2, wallX1, wallX2, wallY1, wallY2 };

        for (int fi = 0; fi < numFaces; fi++)
        {
            const WallFace& face = faces[fi];

            // For each polygon in the model, project onto this wall face
            for (int pi = 0; pi < (int)bodyPtr->m_primitives.size(); pi++)
            {
                const sPrimitive* pPrim = &bodyPtr->m_primitives[pi];
                if (pPrim->m_type != primTypeEnum_Poly)
                    continue;

                const int numVerts = (int)pPrim->m_points.size();
                if (numVerts < 3 || numVerts >= NUM_MAX_VERTEX_IN_PRIM)
                    continue;

                float shadowVerts[NUM_MAX_VERTEX_IN_PRIM * 3];
                bool valid = true;
                int clippedCount = 0;

                for (int vi = 0; vi < numVerts; vi++)
                {
                    const u16 vertexIdx = pPrim->m_points[vi];

                    float localX, localY, localZ;

                    if (isAnimated)
                    {
                        localX = (float)pointBuffer[vertexIdx].x;
                        localY = (float)pointBuffer[vertexIdx].y;
                        localZ = (float)pointBuffer[vertexIdx].z;
                    }
                    else
                    {
                        localX = (float)bodyPtr->m_vertices[vertexIdx].x;
                        localY = (float)bodyPtr->m_vertices[vertexIdx].y;
                        localZ = (float)bodyPtr->m_vertices[vertexIdx].z;

                        if (!localNoRot)
                        {
                            // Y rotation
                            {
                                float tx = localX, tz = localZ;
                                localX = (((localSinB * tx) - (localCosB * tz)) / 65536.f) * 2.f;
                                localZ = (((localCosB * tx) + (localSinB * tz)) / 65536.f) * 2.f;
                            }
                            // Z rotation
                            {
                                float tx = localX, ty = localY;
                                localX = (((localSinG * tx) - (localCosG * ty)) / 65536.f) * 2.f;
                                localY = (((localCosG * tx) + (localSinG * ty)) / 65536.f) * 2.f;
                            }
                            // X rotation
                            {
                                float ty = localY, tz = localZ;
                                localY = (((localSinA * ty) - (localCosA * tz)) / 65536.f) * 2.f;
                                localZ = (((localCosA * ty) + (localSinA * tz)) / 65536.f) * 2.f;
                            }
                        }
                    }

                    // Vertex world position (before projection)
                    float vertWorldX = localX + actorWorldX;
                    float vertWorldY = localY + actorWorldY;
                    float vertWorldZ = localZ + actorWorldZ;

                    // Project along light direction onto the wall face plane.
                    // For X-plane: solve vertWorldX + t * lightDirX = planeVal
                    // For Z-plane: solve vertWorldZ + t * lightDirZ = planeVal
                    float t;
                    if (face.axis == 0) // X-plane
                    {
                        if (g_shadowLightDirX == 0.0f) { valid = false; break; }
                        t = (face.planeVal - vertWorldX) / g_shadowLightDirX;
                    }
                    else // Z-plane
                    {
                        if (g_shadowLightDirZ == 0.0f) { valid = false; break; }
                        t = (face.planeVal - vertWorldZ) / g_shadowLightDirZ;
                    }

                    // Shadow must be cast forward along light (t > 0)
                    if (t <= 0.0f) { valid = false; break; }

                    float hitX = vertWorldX + t * g_shadowLightDirX;
                    float hitY = vertWorldY + t * g_shadowLightDirY;
                    float hitZ = vertWorldZ + t * g_shadowLightDirZ;

                    // Clamp to wall face bounds
                    float perpVal = (face.axis == 0) ? hitZ : hitX;
                    if (perpVal < face.perpMin) perpVal = face.perpMin;
                    if (perpVal > face.perpMax) perpVal = face.perpMax;
                    if (face.axis == 0) hitZ = perpVal; else hitX = perpVal;

                    if (hitY < face.yMin) hitY = face.yMin;
                    if (hitY > face.yMax) hitY = face.yMax;

                    // Check if point hit the floor instead (t for floor is shorter)
                    float tFloor = -localY / g_shadowLightDirY;
                    if (tFloor > 0.0f && tFloor < t)
                    {
                        clippedCount++;
                    }

                    // Camera-space transform
                    float X = hitX - (float)translateX;
                    float Y = hitY - (float)translateY;
                    float Z = hitZ - (float)translateZ;

                    transformPoint(&X, &Y, &Z);

                    Z += (float)cameraPerspective;

                    if (Z <= 50.0f)
                    {
                        valid = false;
                        break;
                    }

                    float screenX = ((X * (float)cameraFovX) / Z) + (float)cameraCenterX;
                    float screenY = ((Y * (float)cameraFovY) / Z) + (float)cameraCenterY;

                    if (screenX < -320.f || screenX > 640.f || screenY < -200.f || screenY > 400.f)
                    {
                        valid = false;
                        break;
                    }

                    shadowVerts[vi * 3 + 0] = screenX;
                    shadowVerts[vi * 3 + 1] = screenY;
                    shadowVerts[vi * 3 + 2] = Z + 5.0f;
                }

                // Skip if all vertices would have hit the floor instead
                if (clippedCount == numVerts)
                    valid = false;

                if (valid)
                {
                    osystem_fillPoly(shadowVerts, numVerts, 0x00, 2);
                }
            }

            g_submitShadowStencil = true;
            osystem_flushPendingPrimitives();
            g_submitShadowStencil = false;
        }
    }
}


