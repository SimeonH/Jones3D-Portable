#ifndef SITH_SITHRENDER_H
#define SITH_SITHRENDER_H
#include <j3dcore/j3d.h>
#include <rdroid/types.h>
#include <sith/types.h>
#include <sith/Main/sithMain.h>
#include <sith/RTI/addresses.h>
#include <std/types.h>

J3D_EXTERN_C_START
#define SITHRENDER_MAX_VISIBLE_SECTORS     4096 // Changed: Was 128
#define SITHRENDER_MAX_SECTORS_WITH_THINGS SITHRENDER_MAX_VISIBLE_SECTORS * 2 // Changed: Was 256

extern float sithRender_g_fogDensity;

extern size_t sithRender_g_numDrawnThings;

extern size_t sithRender_g_numArchPolys;

extern size_t sithRender_g_numAlphaArchPolys;

extern size_t sithRender_g_numThingPolys;

extern size_t sithRender_g_numAlphaThingPoly;

extern size_t sithRender_g_numVisibleAdjoins;

extern size_t sithRender_g_numVisibleSectors;

extern bool sithRender_bResetCameraAspect;

int sithRender_Startup(void);
int J3DAPI sithRender_Open();
void J3DAPI sithRender_Close();
void sithRender_Shutdown(void);
void J3DAPI sithRender_SetRenderFlags(int flags);
int J3DAPI sithRender_GetRenderFlags();
void J3DAPI sithRender_SetLightingMode(rdLightMode mode);

void sithRender_RenderScene(void); // Added
void sithRender_Draw(void);
void sithRender_TogglePVS(void);// Added

void J3DAPI sithRender_BuildVisibleSectorList(SithSector* pSector, rdClipFrustum* pFrustrum);
void J3DAPI sithRender_BuildVisibleSurface(SithSurface* pSurface);
void J3DAPI sithRender_BuildClipFrustrum(rdClipFrustum* pFrustrum, size_t numVertices, float orthLeft, float orthTop, float orthRight, float orthBottom);
void J3DAPI sithRender_PVSBuildVisibleSectorList(SithSector* pSector, rdClipFrustum* pClipFrustum);
void J3DAPI sithRender_PVSBuildVisibleSector(SithSector* pSector);
void J3DAPI sithRender_BuildVisibleSector(SithSector* pSector, const rdClipFrustum* pFrustrum);
void J3DAPI sithRender_RenderSectors();
void sithRender_BuildVisibleSectorsThingList(void);
void J3DAPI sithRender_BuildSectorThingList(SithSector* pSector, float curDistance, float extraDistance);
void sithRender_BuildDynamicLights(void);
void sithRender_RenderThings(void);
int J3DAPI sithRender_RenderThing(SithThing* pThing);
void sithRender_RenderAlphaAdjoins(void);
int sithRender_MakeScreenShot(void);

// Helper hooking functions
void sithRender_InstallHooks(void);

J3D_EXTERN_C_END
#endif // SITH_SITHRENDER_H
