/**
 * \file stereo_ovr.cpp
 * \brief CStereoOVR
 * \date 2014-08-04 16:21GMT
 * \author Jan Boon (Kaetemi)
 * CStereoOVR
 */

/* 
 * Copyright (C) 2014  by authors
 * 
 * This file is part of NL3D.
 * NL3D is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * NL3D is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General
 * Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public
 * License along with NL3D.  If not, see
 * <http://www.gnu.org/licenses/>.
 * 
 * Linking this library statically or dynamically with other modules
 * is making a combined work based on this library.  Thus, the terms
 * and conditions of the GNU General Public License cover the whole
 * combination.
 * 
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with the Oculus SDK to produce
 * an executable, regardless of the license terms of the Oculus SDK,
 * and distribute linked combinations including the two, provided that
 * you also meet the terms and conditions of the license of the Oculus
 * SDK.  You must obey the GNU General Public License in all respects
 * for all of the code used other than the Oculus SDK.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to do
 * so, delete this exception statement from your version.
 */

#ifdef HAVE_LIBOVR

#include "std3d.h"
#include <nel/3d/stereo_ovr_04.h>

// STL includes
#include <sstream>

// External includes
#define OVR_NO_STDINT
#include <OVR.h>

// NeL includes
// #include <nel/misc/debug.h>
#include <nel/3d/u_camera.h>
#include <nel/3d/u_driver.h>
#include <nel/3d/material.h>
#include <nel/3d/texture_bloom.h>
#include <nel/3d/texture_user.h>
#include <nel/3d/driver_user.h>
#include <nel/3d/u_texture.h>

// Project includes

using namespace std;
// using namespace NLMISC;

namespace NL3D {

namespace {

#include "stereo_ovr_04_program.h"

class CStereoOVRSystem
{
public:
	CStereoOVRSystem() : m_InitOk(false)
	{

	}

	~CStereoOVRSystem()
	{
		if (m_InitOk)
		{
			nlwarning("OVR: Not all resources were released before exit");
			Release();
		}
	}

	bool Init()
	{
		if (!m_InitOk)
		{
			nldebug("OVR: Initialize");
			m_InitOk = ovr_Initialize();
			nlassert(m_InitOk);
		}

		return m_InitOk;
	}

	void Release()
	{
		if (m_InitOk)
		{
			nldebug("OVR: Release");
			ovr_Shutdown();
			m_InitOk = false;
		}
	}

private:
	bool m_InitOk;

};

CStereoOVRSystem s_StereoOVRSystem;

sint s_DeviceCounter = 0;
uint s_DetectId = 0;

}

class CStereoOVRDeviceFactory : public IStereoDeviceFactory
{
public:
	uint DeviceIndex;
	uint DetectId;

	bool DebugDevice;
	ovrHmdType DebugDeviceType;

	IStereoDisplay *createDevice() const
	{
		CStereoOVR *stereo = new CStereoOVR(this);
		if (stereo->isDeviceCreated())
			return stereo;
		delete stereo;
		return NULL;
	}
};

CStereoOVR::CStereoOVR(const CStereoOVRDeviceFactory *factory) : m_DevicePtr(NULL), m_Stage(0), m_SubStage(0), m_OrientationCached(false), m_Driver(NULL), /*m_SceneTexture(NULL),*/ m_GUITexture(NULL), /*m_PixelProgram(NULL),*/ m_EyePosition(0.0f, 0.09f, 0.15f), m_Scale(1.0f)
{
	nlctassert(NL_OVR_EYE_COUNT == ovrEye_Count);

	if (factory->DetectId != s_DetectId)
	{
		nlwarning("OVR: Previous device info structures become invalid after listing devices");
		return;
	}

	if (factory->DebugDevice) m_DevicePtr = ovrHmd_CreateDebug(factory->DebugDeviceType);
	else m_DevicePtr = ovrHmd_Create(factory->DeviceIndex);

	if (!m_DevicePtr)
	{
		nlwarning("OVR: Device not created");
		return;
	}

	++s_DeviceCounter;

	// nldebug("OVR: HScreenSize: %f, VScreenSize: %f", m_DevicePtr->HMDInfo.HScreenSize, m_DevicePtr->HMDInfo.VScreenSize); // No more support for physically non-square pixels?
	// nldebug("OVR: VScreenCenter: %f", m_DevicePtr->HMDInfo.VScreenCenter);
	// nldebug("OVR: EyeToScreenDistance: %f", m_DevicePtr->HMDInfo.EyeToScreenDistance);
	// nldebug("OVR: LensSeparationDistance: %f", m_DevicePtr->HMDInfo.LensSeparationDistance);
	// nldebug("OVR: InterpupillaryDistance: %f", m_DevicePtr->HMDInfo.InterpupillaryDistance);
	nldebug("OVR: Resolution.w: %i, Resolution.h: %i", m_DevicePtr->Resolution.w, m_DevicePtr->Resolution.h);
	// nldebug("OVR: DistortionK[0]: %f, DistortionK[1]: %f", m_DevicePtr->HMDInfo.DistortionK[0], m_DevicePtr->HMDInfo.DistortionK[1]);
	// nldebug("OVR: DistortionK[2]: %f, DistortionK[3]: %f", m_DevicePtr->HMDInfo.DistortionK[2], m_DevicePtr->HMDInfo.DistortionK[3]);

	if (!ovrHmd_ConfigureTracking(m_DevicePtr, 
		ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection, // | ovrTrackingCap_Position
		ovrTrackingCap_Orientation))
	{
		nlwarning("OVR: Cannot configure tracking");
		ovrHmd_Destroy(m_DevicePtr);
		m_DevicePtr = NULL;
		--s_DeviceCounter;
		return;
	}
	
	float nativeWidth = m_DevicePtr->Resolution.w;
	float nativeHeight = m_DevicePtr->Resolution.h;

	// get render descriptions for default fov
	ovrEyeRenderDesc eyeRenderDesc[ovrEye_Count];
	eyeRenderDesc[ovrEye_Left] = ovrHmd_GetRenderDesc(m_DevicePtr, ovrEye_Left, m_DevicePtr->DefaultEyeFov[ovrEye_Left]);
	eyeRenderDesc[ovrEye_Right] = ovrHmd_GetRenderDesc(m_DevicePtr, ovrEye_Right, m_DevicePtr->DefaultEyeFov[ovrEye_Right]);
	nldebug("OVR: LEFT DistortedViewport: x: %i, y: %i, w: %i, h: %i", eyeRenderDesc[0].DistortedViewport.Pos.x, eyeRenderDesc[0].DistortedViewport.Pos.y, eyeRenderDesc[0].DistortedViewport.Size.w, eyeRenderDesc[0].DistortedViewport.Size.h);
	nldebug("OVR: LEFT PixelsPerTanAngleAtCenter: x: %f, y: %f ", eyeRenderDesc[0].PixelsPerTanAngleAtCenter.x, eyeRenderDesc[0].PixelsPerTanAngleAtCenter.y);
	nldebug("OVR: LEFT ViewAdjust: x: %f, y: %f, z: %f ", eyeRenderDesc[0].ViewAdjust.x, eyeRenderDesc[0].ViewAdjust.y, eyeRenderDesc[0].ViewAdjust.z);
	nldebug("OVR: RIGHT DistortedViewport: x: %i, y: %i, w: %i, h: %i", eyeRenderDesc[1].DistortedViewport.Pos.x, eyeRenderDesc[1].DistortedViewport.Pos.y, eyeRenderDesc[1].DistortedViewport.Size.w, eyeRenderDesc[1].DistortedViewport.Size.h);
	nldebug("OVR: RIGHT PixelsPerTanAngleAtCenter: x: %f, y: %f ", eyeRenderDesc[1].PixelsPerTanAngleAtCenter.x, eyeRenderDesc[1].PixelsPerTanAngleAtCenter.y);
	nldebug("OVR: RIGHT ViewAdjust: x: %f, y: %f, z: %f ", eyeRenderDesc[1].ViewAdjust.x, eyeRenderDesc[1].ViewAdjust.y, eyeRenderDesc[1].ViewAdjust.z);

	// 2014/08/04 19:54:25 DBG  a60 snowballs_client.exe stereo_ovr_04.cpp 171 NL3D::CStereoOVR::CStereoOVR : OVR: Resolution.w: 1280, Resolution.h: 800
	// 2014/08/04 19:54:25 DBG  a60 snowballs_client.exe stereo_ovr_04.cpp 189 NL3D::CStereoOVR::CStereoOVR : OVR: LEFT DistortedViewport: x: 0, y: 0, w: 640, h: 800
	// 2014/08/04 19:54:25 DBG  a60 snowballs_client.exe stereo_ovr_04.cpp 190 NL3D::CStereoOVR::CStereoOVR : OVR: LEFT PixelsPerTanAngleAtCenter: x: 363.247864, y: 363.247864 
	// 2014/08/04 19:54:25 DBG  a60 snowballs_client.exe stereo_ovr_04.cpp 191 NL3D::CStereoOVR::CStereoOVR : OVR: LEFT ViewAdjust: x: 0.031800, y: 0.000000, z: 0.000000 
	// 2014/08/04 19:55:46 DBG 2e18 snowballs_client.exe stereo_ovr_04.cpp 192 NL3D::CStereoOVR::CStereoOVR : OVR: RIGHT DistortedViewport: x: 640, y: 0, w: 640, h: 800
	// 2014/08/04 19:55:46 DBG 2e18 snowballs_client.exe stereo_ovr_04.cpp 193 NL3D::CStereoOVR::CStereoOVR : OVR: RIGHT PixelsPerTanAngleAtCenter: x: 363.247864, y: 363.247864 
	// 2014/08/04 19:55:46 DBG 2e18 snowballs_client.exe stereo_ovr_04.cpp 194 NL3D::CStereoOVR::CStereoOVR : OVR: RIGHT ViewAdjust: x: -0.031868, y: 0.000000, z: 0.000000 

	// find out the recommended render target size
	ovrSizei fovTextureSize[ovrEye_Count];
	fovTextureSize[ovrEye_Left] = ovrHmd_GetFovTextureSize(m_DevicePtr, ovrEye_Left, eyeRenderDesc[ovrEye_Left].Fov, 1.0f);
    fovTextureSize[ovrEye_Right] = ovrHmd_GetFovTextureSize(m_DevicePtr, ovrEye_Right, eyeRenderDesc[ovrEye_Right].Fov, 1.0f);
    m_RenderTargetWidth = fovTextureSize[ovrEye_Left].w + fovTextureSize[ovrEye_Right].w;
    m_RenderTargetHeight = max(fovTextureSize[ovrEye_Left].h, fovTextureSize[ovrEye_Right].h);
	nldebug("OVR: RenderTarget: w: %u, h: %u", m_RenderTargetWidth, m_RenderTargetHeight);
	
	// 2014/08/04 20:22:03 DBG 30e4 snowballs_client.exe stereo_ovr_04.cpp 213 NL3D::CStereoOVR::CStereoOVR : OVR: RenderTarget: w: 2414, h: 1870 // That looks a bit excessive...

	for (uint eye = 0; eye < ovrEye_Count; ++eye)
	{
		ovrFovPort &fov = eyeRenderDesc[eye].Fov;

		// store data
		m_EyeViewAdjustX[eye] = -eyeRenderDesc[eye].ViewAdjust.x;
		
		// setup viewport
		m_EyeViewport[eye].init(
			(float)eyeRenderDesc[eye].DistortedViewport.Pos.x / nativeWidth, 
			(float)eyeRenderDesc[eye].DistortedViewport.Pos.y / nativeHeight, 
			(float)eyeRenderDesc[eye].DistortedViewport.Size.w / nativeWidth, 
			(float)eyeRenderDesc[eye].DistortedViewport.Size.h / nativeHeight);
		nldebug("OVR: EyeViewport: x: %f, y: %f, w: %f, h: %f", m_EyeViewport[eye].getX(), m_EyeViewport[eye].getY(), m_EyeViewport[eye].getWidth(), m_EyeViewport[eye].getHeight());
		ovrRecti eyeViewport;
		eyeViewport.Pos.x = (eyeRenderDesc[eye].DistortedViewport.Pos.x * m_RenderTargetWidth) / m_DevicePtr->Resolution.w;
		eyeViewport.Pos.y = (eyeRenderDesc[eye].DistortedViewport.Pos.y * m_RenderTargetHeight) / m_DevicePtr->Resolution.h;
		eyeViewport.Size.w = (eyeRenderDesc[eye].DistortedViewport.Size.w * m_RenderTargetWidth) / m_DevicePtr->Resolution.w;
		eyeViewport.Size.h = (eyeRenderDesc[eye].DistortedViewport.Size.h * m_RenderTargetHeight) / m_DevicePtr->Resolution.h;

		// calculate hfov and ar
		float combinedTanHalfFovHorizontal = max(fov.LeftTan, fov.RightTan);
		float combinedTanHalfFovVertical = max(fov.UpTan, fov.DownTan);
		float horizontalFullFovInRadians = 2.0f * atanf (combinedTanHalfFovHorizontal);
		float aspectRatio = combinedTanHalfFovHorizontal / combinedTanHalfFovVertical;
		m_EyeHFov[eye] = horizontalFullFovInRadians;
		m_EyeAR[eye] = aspectRatio;
		nldebug("OVR: HFOV: %f, AR: %f", horizontalFullFovInRadians, aspectRatio);

		// get distortion mesh
		ovrDistortionMesh meshData;
		ovrHmd_CreateDistortionMesh(m_DevicePtr, (ovrEyeType)eye, fov,
			ovrDistortionCap_Chromatic /*| ovrDistortionCap_TimeWarp*/ | ovrDistortionCap_Vignette, // I believe the timewarp gimmick screws with parallax
			&meshData);
		ovrVector2f uvScaleOffset[2];

		// get parameters for programs
		ovrHmd_GetRenderScaleAndOffset(fov,
			fovTextureSize[eye], eyeViewport,
			(ovrVector2f *)uvScaleOffset);
		m_EyeUVScaleOffset[eye][0] = NLMISC::CVector2f(uvScaleOffset[0].x, uvScaleOffset[0].y);
		m_EyeUVScaleOffset[eye][1] = NLMISC::CVector2f(uvScaleOffset[1].x, uvScaleOffset[1].y);

		// create distortion mesh vertex buffer
		m_VB.setVertexFormat(CVertexBuffer::PositionFlag | CVertexBuffer::TexCoord0Flag | CVertexBuffer::TexCoord1Flag | CVertexBuffer::TexCoord2Flag | CVertexBuffer::PrimaryColorFlag);
		m_VB.setPreferredMemory(CVertexBuffer::StaticPreferred, false);
		m_VB.setNumVertices(meshData.VertexCount);
		{
			CVertexBufferReadWrite vba;
			m_VB.lock(vba);
			for (uint i = 0; i < meshData.VertexCount; ++i)
			{
				ovrDistortionVertex &ov = meshData.pVertexData[i];
				vba.setVertexCoord(i, (ov.ScreenPosNDC.x + 1.0f) * 0.5f, (ov.ScreenPosNDC.y + 1.0f) * 0.5f, 0.5f);
				vba.setTexCoord(i, 0, ov.TanEyeAnglesR.x, ov.TanEyeAnglesR.y);
				vba.setTexCoord(i, 1, ov.TanEyeAnglesG.x, ov.TanEyeAnglesG.y);
				vba.setTexCoord(i, 2, ov.TanEyeAnglesB.x, ov.TanEyeAnglesB.y);
				NLMISC::CRGBA color;
				color.R = color.G = color.B = (uint8)(ov.VignetteFactor * 255.99f);
				color.A = 255; // (uint8)(ov.TimeWarpFactor * 255.99f);
				vba.setColor(i, color);
			}
		}

		// create distortion mesh index buffer
		m_IB.setFormat(NL_DEFAULT_INDEX_BUFFER_FORMAT);
		m_IB.setPreferredMemory(CIndexBuffer::StaticPreferred, false);
		m_IB.setNumIndexes(meshData.IndexCount);
		{
			CIndexBufferReadWrite iba;
			m_IB.lock(iba);
			for (uint i = 0; i + 2 < meshData.IndexCount; i += 3)
			{
				nlassert(meshData.pIndexData[i] < meshData.VertexCount);
				nlassert(meshData.pIndexData[i + 1] < meshData.VertexCount);
				nlassert(meshData.pIndexData[i + 2] < meshData.VertexCount);
				iba.setTri(i, meshData.pIndexData[i], meshData.pIndexData[i + 1], meshData.pIndexData[i + 2]);
			}
		}

		// set tri count
		m_NbTris = meshData.IndexCount / 3;

		// destroy ovr distortion mesh
		ovrHmd_DestroyDistortionMesh(&meshData);
	}
	
	// 2014/08/04 20:22:03 DBG 30e4 snowballs_client.exe stereo_ovr_04.cpp 222 NL3D::CStereoOVR::CStereoOVR : OVR: EyeViewport: x: 0.000000, y: 0.000000, w: 0.500000, h: 1.000000
	// 2014/08/04 22:28:39 DBG 3040 snowballs_client.exe stereo_ovr_04.cpp 235 NL3D::CStereoOVR::CStereoOVR : OVR: HFOV: 2.339905, AR: 0.916641
	// 2014/08/04 20:22:03 DBG 30e4 snowballs_client.exe stereo_ovr_04.cpp 222 NL3D::CStereoOVR::CStereoOVR : OVR: EyeViewport: x: 0.500000, y: 0.000000, w: 0.500000, h: 1.000000
	// 2014/08/04 22:28:39 DBG 3040 snowballs_client.exe stereo_ovr_04.cpp 235 NL3D::CStereoOVR::CStereoOVR : OVR: HFOV: 2.339905, AR: 0.916641

	// DEBUG EARLY EXIT
	nldebug("OVR: Early exit");
	ovrHmd_Destroy(m_DevicePtr);
	m_DevicePtr = NULL;
	--s_DeviceCounter;
}

CStereoOVR::~CStereoOVR()
{
	if (!m_UnlitMat.empty())
	{
		m_Driver->deleteMaterial(m_UnlitMat);
	}

	m_PP.kill();
	m_VP.kill();

	m_Driver = NULL;

	if (m_DevicePtr)
	{
		ovrHmd_Destroy(m_DevicePtr);
		m_DevicePtr = NULL;
		--s_DeviceCounter;
	}
}

class CVertexProgramOVR : public CVertexProgram
{
public:
	struct COVRIndices
	{
		/*uint LensCenter;
		uint ScreenCenter;
		uint Scale;
		uint ScaleIn;
		uint HmdWarpParam;*/
	};

	CVertexProgramOVR()
	{
		/*{
			CSource *source = new CSource();
			source->Profile = glsl330f;
			source->Features.MaterialFlags = CProgramFeatures::TextureStages;
			source->setSourcePtr(g_StereoOVR_glsl330f);
			addSource(source);
		}*/
	}

	virtual ~CVertexProgramOVR()
	{
		
	}

	virtual void buildInfo()
	{
		CVertexProgram::buildInfo();

		/*m_OVRIndices.LensCenter = getUniformIndex("cLensCenter");
		nlassert(m_OVRIndices.LensCenter != ~0);
		m_OVRIndices.ScreenCenter = getUniformIndex("cScreenCenter");
		nlassert(m_OVRIndices.ScreenCenter != ~0);
		m_OVRIndices.Scale = getUniformIndex("cScale");
		nlassert(m_OVRIndices.Scale != ~0);
		m_OVRIndices.ScaleIn = getUniformIndex("cScaleIn");
		nlassert(m_OVRIndices.ScaleIn != ~0);
		m_OVRIndices.HmdWarpParam = getUniformIndex("cHmdWarpParam");
		nlassert(m_OVRIndices.HmdWarpParam != ~0);*/
	}

	inline const COVRIndices &ovrIndices() { return m_OVRIndices; }

private:
	COVRIndices m_OVRIndices;

};

class CPixelProgramOVR : public CPixelProgram
{
public:
	struct COVRIndices
	{
		/*uint LensCenter;
		uint ScreenCenter;
		uint Scale;
		uint ScaleIn;
		uint HmdWarpParam;*/
	};

	CPixelProgramOVR()
	{
		/*
		{
			CSource *source = new CSource();
			source->Profile = arbfp1;
			source->Features.MaterialFlags = CProgramFeatures::TextureStages;
			source->setSourcePtr(g_StereoOVR_arbfp1);
			source->ParamIndices["cLensCenter"] = 0;
			source->ParamIndices["cScreenCenter"] = 1;
			source->ParamIndices["cScale"] = 2;
			source->ParamIndices["cScaleIn"] = 3;
			source->ParamIndices["cHmdWarpParam"] = 4;
			addSource(source);
		}
		{
			CSource *source = new CSource();
			source->Profile = ps_2_0;
			source->Features.MaterialFlags = CProgramFeatures::TextureStages;
			source->setSourcePtr(g_StereoOVR_ps_2_0);
			source->ParamIndices["cLensCenter"] = 0;
			source->ParamIndices["cScreenCenter"] = 1;
			source->ParamIndices["cScale"] = 2;
			source->ParamIndices["cScaleIn"] = 3;
			source->ParamIndices["cHmdWarpParam"] = 4;
			addSource(source);
		}*/
	}

	virtual ~CPixelProgramOVR()
	{
		
	}

	virtual void buildInfo()
	{
		CPixelProgram::buildInfo();

		/*m_OVRIndices.LensCenter = getUniformIndex("cLensCenter");
		nlassert(m_OVRIndices.LensCenter != ~0);
		m_OVRIndices.ScreenCenter = getUniformIndex("cScreenCenter");
		nlassert(m_OVRIndices.ScreenCenter != ~0);
		m_OVRIndices.Scale = getUniformIndex("cScale");
		nlassert(m_OVRIndices.Scale != ~0);
		m_OVRIndices.ScaleIn = getUniformIndex("cScaleIn");
		nlassert(m_OVRIndices.ScaleIn != ~0);
		m_OVRIndices.HmdWarpParam = getUniformIndex("cHmdWarpParam");
		nlassert(m_OVRIndices.HmdWarpParam != ~0);*/
	}

	inline const COVRIndices &ovrIndices() { return m_OVRIndices; }

private:
	COVRIndices m_OVRIndices;

};

void CStereoOVR::setDriver(NL3D::UDriver *driver)
{
	nlassert(!m_PP);
	nlassert(!m_VP);
	m_Driver = driver;

	CDriverUser *dru = static_cast<CDriverUser *>(driver);
	IDriver *drv = dru->getDriver();

	m_UnlitMat = m_Driver->createMaterial();
	m_UnlitMat.initUnlit();
	m_UnlitMat.setColor(CRGBA::White);
	m_UnlitMat.setBlend (false);
	m_UnlitMat.setAlphaTest (false);
	NL3D::CMaterial *unlitMat = m_UnlitMat.getObjectPtr();
	unlitMat->setShader(NL3D::CMaterial::Normal);
	unlitMat->setBlendFunc(CMaterial::one, CMaterial::zero);
	unlitMat->setZWrite(false);
	unlitMat->setZFunc(CMaterial::always);
	unlitMat->setDoubleSided(true);

	if (drv->supportBloomEffect() && drv->supportNonPowerOfTwoTextures())
	{
		m_PP = new CPixelProgramOVR();
		if (!drv->compilePixelProgram(m_PP))
		{
			m_PP.kill();
			nlwarning("OVR: No pixel program support");
			return;
		}
		m_VP = new CVertexProgramOVR();
		if (!drv->compileVertexProgram(m_VP))
		{
			m_VP.kill();
			nlwarning("OVR: No vertex program support");
			m_PP.kill();
			return;
		}
	}
}

bool CStereoOVR::getScreenResolution(uint &width, uint &height)
{
	if (m_DevicePtr)
	{
		width = m_DevicePtr->Resolution.w;
		height = m_DevicePtr->Resolution.h;
		return true;
	}
	return false;
}

void CStereoOVR::initCamera(uint cid, const NL3D::UCamera *camera)
{
	/*m_OriginalFrustum[cid] = camera->getFrustum();

	float ar = (float)m_DevicePtr->HMDInfo.HResolution / ((float)m_DevicePtr->HMDInfo.VResolution * 2.0f);
	float fov = 2.0f * atanf((m_DevicePtr->HMDInfo.HScreenSize * 0.5f * 0.5f) / (m_DevicePtr->HMDInfo.EyeToScreenDistance)); //(float)NLMISC::Pi/2.f; // 2.0f * atanf(m_DevicePtr->HMDInfo.VScreenSize / 2.0f * m_DevicePtr->HMDInfo.EyeToScreenDistance);
	m_LeftFrustum[cid].initPerspective(fov, ar, camera->getFrustum().Near, camera->getFrustum().Far);
	m_RightFrustum[cid] = m_LeftFrustum[cid];
	
	float viewCenter = m_DevicePtr->HMDInfo.HScreenSize * 0.25f;
	float eyeProjectionShift = viewCenter - m_DevicePtr->HMDInfo.LensSeparationDistance * 0.5f; // docs say LensSeparationDistance, why not InterpupillaryDistance? related to how the lenses work?
	float projectionCenterOffset = (eyeProjectionShift / (m_DevicePtr->HMDInfo.HScreenSize * 0.5f)) * (m_LeftFrustum[cid].Right - m_LeftFrustum[cid].Left); // used logic for this one, but it ends up being the same as the one i made up
	nldebug("OVR: projectionCenterOffset = %f", projectionCenterOffset);

	m_LeftFrustum[cid].Left -= projectionCenterOffset;
	m_LeftFrustum[cid].Right -= projectionCenterOffset;
	m_RightFrustum[cid].Left += projectionCenterOffset;
	m_RightFrustum[cid].Right += projectionCenterOffset;

	// TODO: Clipping frustum should also take into account the IPD
	m_ClippingFrustum[cid] = m_LeftFrustum[cid];
	m_ClippingFrustum[cid].Left = min(m_LeftFrustum[cid].Left, m_RightFrustum[cid].Left);
	m_ClippingFrustum[cid].Right = max(m_LeftFrustum[cid].Right, m_RightFrustum[cid].Right);*/
}

/// Get the frustum to use for clipping
void CStereoOVR::getClippingFrustum(uint cid, NL3D::UCamera *camera) const
{
	camera->setFrustum(m_ClippingFrustum[cid]);
}

/// Get the original frustum of the camera
void CStereoOVR::getOriginalFrustum(uint cid, NL3D::UCamera *camera) const
{
	camera->setFrustum(m_OriginalFrustum[cid]);
}

void CStereoOVR::updateCamera(uint cid, const NL3D::UCamera *camera)
{
	if (camera->getFrustum().Near != m_LeftFrustum[cid].Near
		|| camera->getFrustum().Far != m_LeftFrustum[cid].Far)
		CStereoOVR::initCamera(cid, camera);
	m_CameraMatrix[cid] = camera->getMatrix();
}

bool CStereoOVR::nextPass()
{
	if (m_Driver->getPolygonMode() == UDriver::Filled)
	{
		switch (m_Stage) // Previous stage
		{
		case 0:
			m_Stage += 2;
			m_SubStage = 0;
			// stage 2:
			// draw interface 2d (onto render target)
			return true;
		case 2:
			++m_Stage;
			m_SubStage = 0;
			// stage 3:
			// (initBloom)
			// clear buffer
			// draw scene left
			return true;
		case 3:
			++m_Stage;
			m_SubStage = 0;
			// stage 4:
			// draw scene right
			return true;
		case 4:
			++m_Stage;
			m_SubStage = 0;
			// stage 5:
			// (endBloom)
			// draw interface 3d left
			return true;
		case 5:
			++m_Stage;
			m_SubStage = 0;
			// stage 6:
			// draw interface 3d right
			return true;
		/*case 6:
			++m_Stage;
			m_SubStage = 0;
			// stage 7:
			// (endInterfacesDisplayBloom)
			// draw interface 2d left
			return true;
		case 7:
			++m_Stage;
			m_SubStage = 0;
			// stage 8:
			// draw interface 2d right
			return true;*/
		case 6:
			m_Stage = 0;
			m_SubStage = 0;
			// present
			m_OrientationCached = false;
			return false;
		}
	}
	else
	{
		switch (m_Stage)
		{
		case 0:
			++m_Stage;
			m_SubStage = 0;
			return true;
		case 1:
			m_Stage = 0;
			m_SubStage = 0;
			return false;
		}
	}
	nlerror("Invalid stage");
	m_Stage = 0;
	m_SubStage = 0;
	m_OrientationCached = false;
	return false;
}

const NL3D::CViewport &CStereoOVR::getCurrentViewport() const
{
	if (m_Stage == 2) return m_RegularViewport;
	else if (m_Stage % 2) return m_EyeViewport[ovrEye_Left];
	else return m_EyeViewport[ovrEye_Right];
}

const NL3D::CFrustum &CStereoOVR::getCurrentFrustum(uint cid) const
{
	if (m_Stage == 2) return m_OriginalFrustum[cid];
	else if (m_Stage % 2) return m_LeftFrustum[cid];
	else return m_RightFrustum[cid];
}

void CStereoOVR::getCurrentFrustum(uint cid, NL3D::UCamera *camera) const
{
	if (m_Stage == 2) camera->setFrustum(m_OriginalFrustum[cid]);
	else if (m_Stage % 2) camera->setFrustum(m_LeftFrustum[cid]);
	else camera->setFrustum(m_RightFrustum[cid]);
}

void CStereoOVR::getCurrentMatrix(uint cid, NL3D::UCamera *camera) const
{
	CMatrix translate;
	if (m_Stage == 2) { }
	else if (m_Stage % 2) translate.translate(CVector(m_EyeViewAdjustX[ovrEye_Left] * m_Scale, 0.f, 0.f));
	else translate.translate(CVector(m_EyeViewAdjustX[ovrEye_Right] * m_Scale, 0.f, 0.f));
	CMatrix mat = m_CameraMatrix[cid] * translate;
	if (camera->getTransformMode() == NL3D::UTransformable::RotQuat)
	{
		camera->setPos(mat.getPos());
		camera->setRotQuat(mat.getRot());
	}
	else
	{
		// camera->setTransformMode(NL3D::UTransformable::DirectMatrix);
		camera->setMatrix(mat);
	}
}

bool CStereoOVR::wantClear()
{
	switch (m_Stage)
	{
	case 3:
		m_SubStage = 1;
		return true;
	}
	return m_Driver->getPolygonMode() != UDriver::Filled;
}
	
bool CStereoOVR::wantScene()
{
	switch (m_Stage)
	{
	case 3:
	case 4:
		m_SubStage = 2;
		return true;
	}
	return m_Driver->getPolygonMode() != UDriver::Filled;
}

bool CStereoOVR::wantInterface3D()
{
	switch (m_Stage)
	{
	case 5:
	case 6:
		m_SubStage = 3;
		return true;
	}
	return m_Driver->getPolygonMode() != UDriver::Filled;
}

bool CStereoOVR::wantInterface2D()
{
	switch (m_Stage)
	{
	case 2:
		m_SubStage = 4;
		return true;
	}
	return m_Driver->getPolygonMode() != UDriver::Filled;
}

/// Returns non-NULL if a new render target was set
bool CStereoOVR::beginRenderTarget()
{
	// render target always set before driver clear
	// nlassert(m_SubStage <= 1);

	// Set GUI render target
	if (m_Driver && m_Stage == 2 && (m_Driver->getPolygonMode() == UDriver::Filled))
	{
		nlassert(!m_GUITexture);
		uint32 width, height;
		m_Driver->getWindowSize(width, height);
		m_GUITexture = m_Driver->getRenderTargetManager().getRenderTarget(width, height, true, UTexture::RGBA8888);
		static_cast<CDriverUser *>(m_Driver)->setRenderTarget(*m_GUITexture);
		m_Driver->clearBuffers(NLMISC::CRGBA(0, 0, 0, 0));
		return true;
	}

	// Begin 3D scene render target
	if (m_Driver && m_Stage == 3 && (m_Driver->getPolygonMode() == UDriver::Filled))
	{
		nlassert(!m_SceneTexture);
		m_SceneTexture = m_Driver->getRenderTargetManager().getRenderTarget(m_RenderTargetWidth, m_RenderTargetHeight);
		static_cast<CDriverUser *>(m_Driver)->setRenderTarget(*m_SceneTexture);
		return true;
	}

	return false;
}

void CStereoOVR::setInterfaceMatrix(const NL3D::CMatrix &matrix)
{
	m_InterfaceCameraMatrix = matrix;
}

void CStereoOVR::renderGUI()
{
	m_Driver->setModelMatrix(m_InterfaceCameraMatrix);

	{
		NLMISC::CLine line(NLMISC::CVector(0, 5, 2), NLMISC::CVector(0, 5, 3));

		NL3D::UMaterial mat = m_Driver->createMaterial();
		mat.setZWrite(false);
		// mat.setZFunc(UMaterial::always); // Not nice!
		mat.setDoubleSided(true);
		mat.setColor(NLMISC::CRGBA::Red);
		mat.setBlend(false);

		m_Driver->drawLine(line, mat);

		m_Driver->deleteMaterial(mat);
	}
	
	{
		nlassert(m_GUITexture);

		NLMISC::CQuadUV quad;

		NL3D::UMaterial umat = m_Driver->createMaterial();
		umat.initUnlit();
		umat.setColor(NLMISC::CRGBA::White);
		umat.setDoubleSided(true);
		umat.setBlend(true);
		umat.setAlphaTest(false);
		NL3D::CMaterial *mat = umat.getObjectPtr();
		mat->setShader(NL3D::CMaterial::Normal);
		mat->setBlendFunc(CMaterial::one, CMaterial::invsrcalpha);
		mat->setZWrite(false);
		// mat->setZFunc(CMaterial::always); // Not nice
		mat->setDoubleSided(true);
		mat->setTexture(0, m_GUITexture->getITexture());

		// user options
		float scale = 1.0f;
		float distance = 1.5f;
		float offcenter = 0.75f;

		float height = scale * distance * 2.0f;

		uint32 winw, winh;
		m_Driver->getWindowSize(winw, winh);
		float width = height * (float)winw / (float)winh;

		float bottom = -(height * 0.5f);
		float top = (height * 0.5f);

		NLMISC::CQuadUV quadUV;
		quadUV.V0 = CVector(-(width * 0.5f), distance, -(height * 0.5f));
		quadUV.V1 = CVector((width * 0.5f), distance, -(height * 0.5f));
		quadUV.V2 = CVector((width * 0.5f), distance, (height * 0.5f));
		quadUV.V3 = CVector(-(width * 0.5f), distance, (height * 0.5f));
		quadUV.Uv0 = CUV(0.f,  0.f);
		quadUV.Uv1 = CUV(1.f, 0.f);
		quadUV.Uv2 = CUV(1.f, 1.f);
		quadUV.Uv3 = CUV(0.f,  1.f);
		
		const uint nbQuads = 128;
		static CVertexBuffer vb;
		static CIndexBuffer ib;
		
		vb.setVertexFormat(CVertexBuffer::PositionFlag | CVertexBuffer::TexCoord0Flag);
		vb.setPreferredMemory(CVertexBuffer::RAMVolatile, false);
		vb.setNumVertices((nbQuads + 1) * 2);

		{
			CVertexBufferReadWrite vba;
			vb.lock(vba);
			float radius = distance + offcenter;
			float relWidth = width / radius;
			float quadWidth = relWidth / (float)nbQuads;
			for (uint i = 0; i < nbQuads + 1; ++i)
			{
				uint vi0 = i * 2;
				uint vi1 = vi0 + 1;
				float lineH = -(relWidth * 0.5f) + quadWidth * (float)i;
				float lineUV = (float)i / (float)(nbQuads);
				float left = sin(lineH) * radius;
				float forward = cos(lineH) * radius;
				vba.setVertexCoord(vi0, left, forward - offcenter, bottom);
				vba.setTexCoord(vi0, 0, lineUV, 0.0f);
				vba.setVertexCoord(vi1, left, forward - offcenter, top);
				vba.setTexCoord(vi1, 0, lineUV, 1.0f);
			}
		}

		ib.setFormat(NL_DEFAULT_INDEX_BUFFER_FORMAT);
		ib.setPreferredMemory(CIndexBuffer::RAMVolatile, false);
		ib.setNumIndexes(nbQuads * 6);

		{
			CIndexBufferReadWrite iba;
			ib.lock(iba);
			for (uint i = 0; i < nbQuads; ++i)
			{
				uint ti0 = i * 2;
				uint ti1 = ti0 + 1;
				uint bl = ti0;
				uint tl = ti0 + 1;
				uint br = ti0 + 2;
				uint tr = ti0 + 3;
				iba.setTri(ti0 * 3, bl, tl, br);
				iba.setTri(ti1 * 3, br, tl, tr);
			}
		}

		IDriver *driver = static_cast<CDriverUser *>(m_Driver)->getDriver();
		// m_Driver->setPolygonMode(UDriver::Line);
		driver->activeVertexBuffer(vb);
		driver->activeIndexBuffer(ib);
		driver->renderTriangles(*umat.getObjectPtr(), 0, nbQuads * 2); //renderRawQuads(umat, 0, 128);
		// m_Driver->setPolygonMode(UDriver::Filled);

		// m_Driver->drawQuad(quadUV, umat);

		m_Driver->deleteMaterial(umat);
	}
}

/// Returns true if a render target was fully drawn
bool CStereoOVR::endRenderTarget()
{
	// after rendering of course
	// nlassert(m_SubStage > 1);

	// End GUI render target
	if (m_Driver && m_Stage == 2 && (m_Driver->getPolygonMode() == UDriver::Filled))
	{
		// End GUI render target
		nlassert(m_GUITexture);
		CTextureUser texNull;
		(static_cast<CDriverUser *>(m_Driver))->setRenderTarget(texNull);
	}

	// End of 3D Interface pass left
	if (m_Driver && m_Stage == 5 && (m_Driver->getPolygonMode() == UDriver::Filled))
	{
		// Render 2D GUI in 3D space, assume existing camera is OK
		renderGUI();
	}

	// End of 3D Interface pass right
	if (m_Driver && m_Stage == 6 && (m_Driver->getPolygonMode() == UDriver::Filled))
	{
		// Render 2D GUI in 3D space, assume existing camera is OK
		renderGUI();

		// Recycle render target
		m_Driver->getRenderTargetManager().recycleRenderTarget(m_GUITexture);
		m_GUITexture = NULL;
	}

	// End 3D scene render target
	/*if (m_Driver && m_Stage == 6 && (m_Driver->getPolygonMode() == UDriver::Filled)) // set to 4 to turn off distortion of 2d gui
	{
		nlassert(m_SceneTexture);

		CTextureUser texNull;
		(static_cast<CDriverUser *>(m_Driver))->setRenderTarget(texNull);
		bool fogEnabled = m_Driver->fogEnabled();
		m_Driver->enableFog(false);

		m_Driver->setMatrixMode2D11();
		CViewport vp = CViewport();
		m_Driver->setViewport(vp);
		uint32 width, height;
		m_Driver->getWindowSize(width, height);
		NL3D::IDriver *drvInternal = (static_cast<CDriverUser *>(m_Driver))->getDriver();
		NL3D::CMaterial *barrelMat = m_BarrelMat.getObjectPtr();
		barrelMat->setTexture(0, m_SceneTexture->getITexture());

		drvInternal->activePixelProgram(m_PixelProgram);

		float w = float(m_BarrelQuadLeft.V1.x),// / float(width),
			h = float(m_BarrelQuadLeft.V2.y),// / float(height),
			x = float(m_BarrelQuadLeft.V0.x),/// / float(width),
			y = float(m_BarrelQuadLeft.V0.y);// / float(height);

		float lensOffset = m_DevicePtr->HMDInfo.LensSeparationDistance * 0.5f;
		float lensShift = m_DevicePtr->HMDInfo.HScreenSize * 0.25f - lensOffset;
		float lensViewportShift = 4.0f * lensShift / m_DevicePtr->HMDInfo.HScreenSize;

		float lensCenterX = x + (w + lensViewportShift * 0.5f) * 0.5f;
		float lensCenterY = y + h * 0.5f;
		float screenCenterX = x + w * 0.5f;
		float screenCenterY = y + h * 0.5f;
		float scaleX = (w / 2);
		float scaleY = (h / 2);
		float scaleInX = (2 / w);
		float scaleInY = (2 / h);
		

		drvInternal->setUniform2f(IDriver::PixelProgram, 
			m_PixelProgram->ovrIndices().LensCenter, 
			lensCenterX, lensCenterY);

		drvInternal->setUniform2f(IDriver::PixelProgram, 
			m_PixelProgram->ovrIndices().ScreenCenter, 
			screenCenterX, screenCenterY);

		drvInternal->setUniform2f(IDriver::PixelProgram, 
			m_PixelProgram->ovrIndices().Scale, 
			scaleX, scaleY);

		drvInternal->setUniform2f(IDriver::PixelProgram, 
			m_PixelProgram->ovrIndices().ScaleIn, 
			scaleInX, scaleInY);


		drvInternal->setUniform4fv(IDriver::PixelProgram, 
			m_PixelProgram->ovrIndices().HmdWarpParam, 
			1, m_DevicePtr->HMDInfo.DistortionK);

		m_Driver->drawQuad(m_BarrelQuadLeft, m_BarrelMat);

		x = w;
		lensCenterX = x + (w - lensViewportShift * 0.5f) * 0.5f;
		screenCenterX = x + w * 0.5f;


		drvInternal->setUniform2f(IDriver::PixelProgram, 
			m_PixelProgram->ovrIndices().LensCenter, 
			lensCenterX, lensCenterY);

		drvInternal->setUniform2f(IDriver::PixelProgram, 
			m_PixelProgram->ovrIndices().ScreenCenter, 
			screenCenterX, screenCenterY);


		m_Driver->drawQuad(m_BarrelQuadRight, m_BarrelMat);

		drvInternal->activePixelProgram(NULL);
		m_Driver->enableFog(fogEnabled);

		// Recycle render target
		m_Driver->getRenderTargetManager().recycleRenderTarget(m_SceneTexture);
		m_SceneTexture = NULL;

		return true;
	}*/

	return false;
}

NLMISC::CQuat CStereoOVR::getOrientation() const
{
	if (m_OrientationCached)
		return m_OrientationCache;

	ovrTrackingState ts = ovrHmd_GetTrackingState(m_DevicePtr, ovr_GetTimeInSeconds()); // TODO: Predict forward
	if (ts.StatusFlags & ovrStatus_OrientationTracked)
	{
		// get just the orientation
		ovrQuatf quatovr = ts.HeadPose.ThePose.Orientation;
		NLMISC::CMatrix coordsys;
		float csys[] = {
			1.0f, 0.0f, 0.0f, 0.0f, 
			0.0f, 0.0f, -1.0f, 0.0f, 
			0.0f, 1.0f, 0.0f, 0.0f, 
			0.0f, 0.0f, 0.0f, 1.0f, 
		};
		coordsys.set(csys);
		NLMISC::CMatrix matovr;
		matovr.setRot(NLMISC::CQuat(quatovr.x, quatovr.y, quatovr.z, quatovr.w));
		NLMISC::CMatrix matr;
		matr.rotateX(NLMISC::Pi * 0.5f); // fix this properly... :) (note: removing this allows you to use rift while lying down)
		NLMISC::CMatrix matnel = matr * matovr * coordsys;
		NLMISC::CQuat finalquat = matnel.getRot();
		m_OrientationCache = finalquat;
		m_OrientationCached = true;
		return finalquat;
	}
	else
	{
		nlwarning("OVR: No orientation returned");
		// return old orientation
		m_OrientationCached = true;
		return m_OrientationCache;
	}
}

/// Get GUI shift
void CStereoOVR::getInterface2DShift(uint cid, float &x, float &y, float distance) const
{

}

void CStereoOVR::setEyePosition(const NLMISC::CVector &v)
{
	m_EyePosition = v;
}

const NLMISC::CVector &CStereoOVR::getEyePosition() const
{
	return m_EyePosition;
}

void CStereoOVR::setScale(float s)
{
	m_EyePosition = m_EyePosition * (s / m_Scale);
	m_Scale = s;
}



void CStereoOVR::listDevices(std::vector<CStereoDeviceInfo> &devicesOut)
{
	if (!s_StereoOVRSystem.Init())
		return;

	++s_DetectId;
	uint hmdDetect = ovrHmd_Detect();
	nldebug("OVR: Detected %u HMDs", hmdDetect);

	for (uint i = 0; i < hmdDetect; ++i)
	{
		devicesOut.resize(devicesOut.size() + 1);
		CStereoDeviceInfo &deviceInfoOut = devicesOut[devicesOut.size() - 1];
		ovrHmd hmd = ovrHmd_Create(i);
		CStereoOVRDeviceFactory *factory = new CStereoOVRDeviceFactory();
		factory->DetectId = s_DetectId;
		factory->DeviceIndex = i;
		factory->DebugDevice = false;
		deviceInfoOut.Factory = factory;
		deviceInfoOut.Class = CStereoDeviceInfo::StereoHMD;
		deviceInfoOut.Library = CStereoDeviceInfo::OVR;
		deviceInfoOut.Manufacturer = hmd->Manufacturer;
		deviceInfoOut.ProductName = hmd->ProductName;
		deviceInfoOut.AllowAuto = true;
		deviceInfoOut.Serial = hmd->SerialNumber;
		ovrHmd_Destroy(hmd);
	}
	
#if !FINAL_VERSION
	// Debug DK1
	{
		devicesOut.resize(devicesOut.size() + 1);
		CStereoDeviceInfo &deviceInfoOut = devicesOut[devicesOut.size() - 1];
		ovrHmd hmd = ovrHmd_CreateDebug(ovrHmd_DK1);
		CStereoOVRDeviceFactory *factory = new CStereoOVRDeviceFactory();
		factory->DebugDevice = true;
		factory->DebugDeviceType = ovrHmd_DK1;
		deviceInfoOut.Factory = factory;
		deviceInfoOut.Class = CStereoDeviceInfo::StereoHMD;
		deviceInfoOut.Library = CStereoDeviceInfo::OVR;
		deviceInfoOut.Manufacturer = hmd->Manufacturer;
		deviceInfoOut.ProductName = hmd->ProductName;
		deviceInfoOut.AllowAuto = false;
		deviceInfoOut.Serial = "OVR-DK1-DEBUG";
		ovrHmd_Destroy(hmd);
	}
	// Debug DK2
	{
		devicesOut.resize(devicesOut.size() + 1);
		CStereoDeviceInfo &deviceInfoOut = devicesOut[devicesOut.size() - 1];
		ovrHmd hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
		CStereoOVRDeviceFactory *factory = new CStereoOVRDeviceFactory();
		factory->DebugDevice = true;
		factory->DebugDeviceType = ovrHmd_DK2;
		deviceInfoOut.Factory = factory;
		deviceInfoOut.Class = CStereoDeviceInfo::StereoHMD;
		deviceInfoOut.Library = CStereoDeviceInfo::OVR;
		deviceInfoOut.Manufacturer = hmd->Manufacturer;
		deviceInfoOut.ProductName = hmd->ProductName;
		deviceInfoOut.AllowAuto = false;
		deviceInfoOut.Serial = "OVR-DK2-DEBUG";
		ovrHmd_Destroy(hmd);
	}
#endif
}

bool CStereoOVR::isLibraryInUse()
{
	nlassert(s_DeviceCounter >= 0);
	return s_DeviceCounter > 0;
}

void CStereoOVR::releaseLibrary()
{
	nlassert(s_DeviceCounter == 0);
	s_StereoOVRSystem.Release();
}

bool CStereoOVR::isDeviceCreated()
{
	return m_DevicePtr != NULL;
}

} /* namespace NL3D */

#endif /* HAVE_LIBOVR */

/* end of file */
