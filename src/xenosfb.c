/*
 * Xenosfb - Framebuffer Device Driver for Xbox 360 Xenos graphics chipset
 * 
 * Authors: Felix Domke, <tmbinc@elitedvb.net>
 *          Axel Auweter, <auweter@informatik.tu-muenchen.de>
 *          http://www.free60.org/
 *
 * based on the X.org fbdev generic framebuffer device driver by
 *          Alan Hourihane, <alanh@fairlite.demon.co.uk>
 *          Michel Dänzer, <michel@tungstengraphics.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <byteswap.h>

/* all driver need this */
#include "xf86.h"
#include "xf86_OSproc.h"

#include "mipointer.h"
#include "mibstore.h"
#include "micmap.h"
#include "colormapst.h"
#include "xf86cmap.h"
#include "shadow.h"
#include "dgaproc.h"

/* for visuals */
#include "fb.h"

#if GET_ABI_MAJOR(ABI_VIDEODRV_VERSION) < 6
#include "xf86Resources.h"
#include "xf86RAC.h"
#endif

#include "fbdevhw.h"

#include "xf86xv.h"

#ifdef XSERVER_LIBPCIACCESS
#include <pciaccess.h>
#endif

#include <xe.h>
#include <edram.h>

static Bool debug = 0;

uint32_t xenos_id = 0x5831;

static unsigned char content_datapspsu[] = {
0x10, 0x2a, 0x11, 0x00, 0x00, 0x00, 0x00, 0xa4, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x4b,
0xff, 0xff, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x30, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x73, 0x00, 0xab, 0xab, 0x00, 0x04, 0x00, 0x0c,
0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40,
0x40, 0x40, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0xab, 0xab,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x21, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
0x00, 0x00, 0xf0, 0x50, 0x00, 0x01, 0x10, 0x02, 0x00, 0x00, 0x12, 0x00, 0xc4, 0x00, 0x00, 0x00,
0x00, 0x00, 0x10, 0x03, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x08, 0x00, 0x01,
0x1f, 0x1f, 0xf6, 0x88, 0x00, 0x00, 0x40, 0x00, 0xc8, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
0xe2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static unsigned char content_datavsvsu[] = {
0x10, 0x2a, 0x11, 0x01, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x23,
0xff, 0xfe, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x1c, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40,
0x40, 0x40, 0x40, 0x40, 0x40, 0x00, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60,
0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x21,
0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02, 0x90,
0x00, 0x10, 0x00, 0x03, 0x00, 0x30, 0x50, 0x04, 0x00, 0x00, 0xf0, 0x50, 0x00, 0x00, 0x10, 0x06,
0x30, 0x05, 0x20, 0x03, 0x00, 0x00, 0x12, 0x00, 0xc2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x05,
0x00, 0x00, 0x12, 0x00, 0xc4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x06, 0x00, 0x00, 0x22, 0x00,
0x00, 0x00, 0x00, 0x00, 0x05, 0xf8, 0x10, 0x00, 0x00, 0x00, 0x06, 0x88, 0x00, 0x00, 0x00, 0x00,
0x05, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x06, 0x88, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x0f, 0x80, 0x3e,
0x00, 0x00, 0x00, 0x00, 0xe2, 0x01, 0x01, 0x00, 0xc8, 0x0f, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
0xe2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


#define TRACE_ENTER(str) \
    do { if (debug) ErrorF("fbdev: " str " %d\n",pScrn->scrnIndex); } while (0)
#define TRACE_EXIT(str) \
    do { if (debug) ErrorF("fbdev: " str " done\n"); } while (0)
#define TRACE(str) \
    do { if (debug) ErrorF("fbdev trace: " str "\n"); } while (0)

/* -------------------------------------------------------------------- */
/* prototypes                                                           */

static const OptionInfoRec * FBDevAvailableOptions(int chipid, int busid);
static void	FBDevIdentify(int flags);
static Bool	FBDevProbe(DriverPtr drv, int flags);
#ifdef XSERVER_LIBPCIACCESS
static Bool	FBDevPciProbe(DriverPtr drv, int entity_num,
     struct pci_device *dev, intptr_t match_data);
#endif
static Bool	FBDevPreInit(ScrnInfoPtr pScrn, int flags);
static Bool	FBDevScreenInit(int Index, ScreenPtr pScreen, int argc,
				char **argv);
static Bool	FBDevCloseScreen(int scrnIndex, ScreenPtr pScreen);
static Bool	FBDevDGAInit(ScrnInfoPtr pScrn, ScreenPtr pScreen);
static Bool	FBDevDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op,
				pointer ptr);


enum { FBDEV_ROTATE_NONE=0, FBDEV_ROTATE_CW=270, FBDEV_ROTATE_UD=180, FBDEV_ROTATE_CCW=90 };


/* -------------------------------------------------------------------- */

/*
 * This is intentionally screen-independent.  It indicates the binding
 * choice made in the first PreInit.
 */

static int pix24bpp = 0;

#define XENOSFB_VERSION		4000
#define XENOSFB_NAME		"XENOSFB"
#define XENOSFB_DRIVER_NAME	"xenosfb"

#ifdef XSERVER_LIBPCIACCESS
static const struct pci_id_match fbdev_device_match[] = {
    {
	PCI_MATCH_ANY, PCI_MATCH_ANY, PCI_MATCH_ANY, PCI_MATCH_ANY,
	0x00030000, 0x00ffffff, 0
    },

    { 0, 0, 0 },
};
#endif

_X_EXPORT DriverRec XENOSFB = {
	XENOSFB_VERSION,
	XENOSFB_DRIVER_NAME,
#if 0
	"driver for linux framebuffer devices",
#endif
	FBDevIdentify,
	FBDevProbe,
	FBDevAvailableOptions,
	NULL,
	0,
	FBDevDriverFunc,

#ifdef XSERVER_LIBPCIACCESS
    fbdev_device_match,
    FBDevPciProbe
#endif
};

/* Supported "chipsets" */
static SymTabRec FBDevChipsets[] = {
    { 0, "fbdev" },
    {-1, NULL }
};

/* Supported options */
typedef enum {
	OPTION_ROTATE,
	OPTION_DEBUG
} FBDevOpts;

static const OptionInfoRec FBDevOptions[] = {
	{ OPTION_ROTATE,	"Rotate",	OPTV_STRING,	{0},	FALSE },
	{ OPTION_DEBUG,		"debug",	OPTV_BOOLEAN,	{0},	FALSE },
	{ -1,			NULL,		OPTV_NONE,	{0},	FALSE }
};

/* -------------------------------------------------------------------- */

#ifdef XFree86LOADER

MODULESETUPPROTO(XENOSFBDevSetup);

static XF86ModuleVersionInfo XENOSFBVersRec =
{
	"xenosfb",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
	ABI_CLASS_VIDEODRV,
	ABI_VIDEODRV_VERSION,
	NULL,
	{0,0,0,0}
};

_X_EXPORT XF86ModuleData xenosfbModuleData = { &XENOSFBVersRec, XENOSFBDevSetup, NULL };

pointer
XENOSFBDevSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
	static Bool setupDone = FALSE;

	if (!setupDone) {
		setupDone = TRUE;
		xf86AddDriver(&XENOSFB, module, HaveDriverFuncs);
		return (pointer)1;
	} else {
		if (errmaj) *errmaj = LDR_ONCEONLY;
		return NULL;
	}
}

#endif /* XFree86LOADER */

/* -------------------------------------------------------------------- */
/* our private data, and two functions to allocate/free this            */

typedef struct {
	struct XenosDevice		xe_,*xe;
	struct XenosSurface *gfxplane;
	struct XenosShader *sh_ps, *sh_vs;
	struct XenosVertexBuffer *vb;

	unsigned char*			fbstart;
	unsigned char*			fbmem;
	int				fboff;
	int				lineLength;
	void				*shadow;
	CloseScreenProcPtr		CloseScreen;
	CreateScreenResourcesProcPtr	CreateScreenResources;
	void				(*PointerMoved)(int index, int x, int y);
	EntityInfoPtr			pEnt;
	/* DGA info */
	DGAModePtr			pDGAMode;
	int				nDGAMode;
	OptionInfoPtr			Options;
} FBDevRec, *FBDevPtr;

#define FBDEVPTR(p) ((FBDevPtr)((p)->driverPrivate))

static Bool
FBDevGetRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate != NULL)
		return TRUE;
	
	pScrn->driverPrivate = xnfcalloc(sizeof(FBDevRec), 1);
	return TRUE;
}

static void
FBDevFreeRec(ScrnInfoPtr pScrn)
{
	if (pScrn->driverPrivate == NULL)
		return;
	xfree(pScrn->driverPrivate);
	pScrn->driverPrivate = NULL;
}

/* -------------------------------------------------------------------- */

void XenosRefreshArea (ScrnInfoPtr pScrn, int nbox, BoxPtr pBox)
{
	FBDevPtr fPtr = FBDEVPTR(pScrn);

		/* wait for render finish */
 	Xe_Sync(fPtr->xe);

		/* flush cache */
	Xe_Surface_LockRect(fPtr->xe, fPtr->gfxplane, 0, 0, 0, 0, XE_LOCK_WRITE);
	Xe_Surface_Unlock(fPtr->xe, fPtr->gfxplane);

		/* begin a new frame, i.e. reset all renderstates to the default */
	Xe_InvalidateState(fPtr->xe);


	Xe_SetZEnable(fPtr->xe,0);
	Xe_SetCullMode(fPtr->xe,XE_CULL_NONE);
	
		/* draw rect */
	Xe_SetShader(fPtr->xe, SHADER_TYPE_PIXEL, fPtr->sh_ps, 0);
	Xe_SetShader(fPtr->xe, SHADER_TYPE_VERTEX, fPtr->sh_vs, 0);
	Xe_SetStreamSource(fPtr->xe, 0, fPtr->vb, 0, 4); /* using this vertex buffer */
	Xe_SetTexture(fPtr->xe, 0, fPtr->gfxplane); /* ... and this texture */
	Xe_DrawPrimitive(fPtr->xe, XE_PRIMTYPE_RECTLIST, 0, 1);

		/* resolve */
	Xe_ResolveInto(fPtr->xe,Xe_GetFramebufferSurface(fPtr->xe),XE_SOURCE_COLOR,0);

		/* start rendering */
	Xe_Execute(fPtr->xe);
}

static const OptionInfoRec *
FBDevAvailableOptions(int chipid, int busid)
{
	return FBDevOptions;
}

static void
FBDevIdentify(int flags)
{
	xf86PrintChipsets(XENOSFB_NAME, "driver for framebuffer of Xenon gaming console", FBDevChipsets);
}


#ifdef XSERVER_LIBPCIACCESS
static Bool FBDevPciProbe(DriverPtr drv, int entity_num,
			  struct pci_device *dev, intptr_t match_data)
{
    ScrnInfoPtr pScrn = NULL;

    if (!xf86LoadDrvSubModule(drv, "fbdevhw"))
	return FALSE;
	    
    pScrn = xf86ConfigPciEntity(NULL, 0, entity_num, NULL, NULL,
				NULL, NULL, NULL, NULL);
    if (pScrn) {
	char *device;
	GDevPtr devSection = xf86GetDevFromEntity(pScrn->entityList[0],
						  pScrn->entityInstanceList[0]);

	device = xf86FindOptionValue(devSection->options, "fbdev");
	if (fbdevHWProbe(NULL, device, NULL)) {
	    pScrn->driverVersion = XENOSFB_VERSION;
	    pScrn->driverName    = XENOSFB_DRIVER_NAME;
	    pScrn->name          = XENOSFB_NAME;
	    pScrn->Probe         = FBDevProbe;
	    pScrn->PreInit       = FBDevPreInit;
	    pScrn->ScreenInit    = FBDevScreenInit;
	    pScrn->SwitchMode    = fbdevHWSwitchModeWeak();
	    pScrn->AdjustFrame   = fbdevHWAdjustFrameWeak();
	    pScrn->EnterVT       = fbdevHWEnterVTWeak();
	    pScrn->LeaveVT       = fbdevHWLeaveVTWeak();
	    pScrn->ValidMode     = fbdevHWValidModeWeak();

	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		       "claimed PCI slot %d@%d:%d:%d\n", 
		       dev->bus, dev->domain, dev->dev, dev->func);
	    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		       "using %s\n", device ? device : "default device");
	}
	else {
	    pScrn = NULL;
	}
    }

    return (pScrn != NULL);
}
#endif


static Bool
FBDevProbe(DriverPtr drv, int flags)
{
	int i;
	ScrnInfoPtr pScrn;
       	GDevPtr *devSections;
	int numDevSections;
	int bus,device,func;
	char *dev;
	Bool foundScreen = FALSE;

	TRACE("probe start");

	/* For now, just bail out for PROBE_DETECT. */
	if (flags & PROBE_DETECT)
		return FALSE;

	if ((numDevSections = xf86MatchDevice(XENOSFB_DRIVER_NAME, &devSections)) <= 0) 
	    return FALSE;
	
	if (!xf86LoadDrvSubModule(drv, "fbdevhw"))
	    return FALSE;
	    
	for (i = 0; i < numDevSections; i++) {
	    Bool isIsa = FALSE;
	    Bool isPci = FALSE;

	    dev = xf86FindOptionValue(devSections[i]->options,"xenosfb");
	    if (devSections[i]->busID) {
#ifndef XSERVER_LIBPCIACCESS
	        if (xf86ParsePciBusString(devSections[i]->busID,&bus,&device,
					  &func)) {
		    if (!xf86CheckPciSlot(bus,device,func))
		        continue;
		    isPci = TRUE;
		} else
#endif
#ifdef HAVE_ISA
		if (xf86ParseIsaBusString(devSections[i]->busID))
		    isIsa = TRUE;
		else
#endif
		    0;
		  
	    }
	    if (fbdevHWProbe(NULL,dev,NULL)) {
		pScrn = NULL;
		if (isPci) {
#ifndef XSERVER_LIBPCIACCESS
		    /* XXX what about when there's no busID set? */
		    int entity;
		    
		    entity = xf86ClaimPciSlot(bus,device,func,drv,
					      0,devSections[i],
					      TRUE);
		    pScrn = xf86ConfigPciEntity(pScrn,0,entity,
						      NULL,RES_SHARED_VGA,
						      NULL,NULL,NULL,NULL);
		    /* xf86DrvMsg() can't be called without setting these */
		    pScrn->driverName    = XENOSFB_DRIVER_NAME;
		    pScrn->name          = XENOSFB_NAME;
		    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
			       "claimed PCI slot %d:%d:%d\n",bus,device,func);

#endif
		} else if (isIsa) {
#ifdef HAVE_ISA
		    int entity;
		    
		    entity = xf86ClaimIsaSlot(drv, 0,
					      devSections[i], TRUE);
		    pScrn = xf86ConfigIsaEntity(pScrn,0,entity,
						      NULL,RES_SHARED_VGA,
						      NULL,NULL,NULL,NULL);
#endif
		} else {
		   int entity;

		    entity = xf86ClaimFbSlot(drv, 0,
					      devSections[i], TRUE);
		    pScrn = xf86ConfigFbEntity(pScrn,0,entity,
					       NULL,NULL,NULL,NULL);
		   
		}
		if (pScrn) {
		    foundScreen = TRUE;
		    
		    pScrn->driverVersion = XENOSFB_VERSION;
		    pScrn->driverName    = XENOSFB_DRIVER_NAME;
		    pScrn->name          = XENOSFB_NAME;
		    pScrn->Probe         = FBDevProbe;
		    pScrn->PreInit       = FBDevPreInit;
		    pScrn->ScreenInit    = FBDevScreenInit;
		    pScrn->SwitchMode    = fbdevHWSwitchModeWeak();
		    pScrn->AdjustFrame   = fbdevHWAdjustFrameWeak();
		    pScrn->EnterVT       = fbdevHWEnterVTWeak();
		    pScrn->LeaveVT       = fbdevHWLeaveVTWeak();
		    pScrn->ValidMode     = fbdevHWValidModeWeak();
		    
		    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			       "using %s\n", dev ? dev : "default device");
		}
	    }
	}
	xfree(devSections);
	TRACE("probe done");
	return foundScreen;
}

static Bool
FBDevPreInit(ScrnInfoPtr pScrn, int flags)
{
	FBDevPtr fPtr;
	int default_depth, fbbpp;
	const char *s;
	int type;

	if (flags & PROBE_DETECT) return FALSE;

	TRACE_ENTER("PreInit");

	/* Check the number of entities, and fail if it isn't one. */
	if (pScrn->numEntities != 1)
		return FALSE;

	pScrn->monitor = pScrn->confScreen->monitor;

	FBDevGetRec(pScrn);
	fPtr = FBDEVPTR(pScrn);

	fPtr->pEnt = xf86GetEntityInfo(pScrn->entityList[0]);

#ifndef XSERVER_LIBPCIACCESS
	pScrn->racMemFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;
	/* XXX Is this right?  Can probably remove RAC_FB */
	pScrn->racIoFlags = RAC_FB | RAC_COLORMAP | RAC_CURSOR | RAC_VIEWPORT;

	if (fPtr->pEnt->location.type == BUS_PCI &&
	    xf86RegisterResources(fPtr->pEnt->index,NULL,ResExclusive)) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
		   "xf86RegisterResources() found resource conflicts\n");
		return FALSE;
	}
#endif
	/* open device */
	if (!fbdevHWInit(pScrn,NULL,xf86FindOptionValue(fPtr->pEnt->device->options,"fbdev")))
		return FALSE;
	default_depth = fbdevHWGetDepth(pScrn,&fbbpp);
	
	if (!xf86SetDepthBpp(pScrn, default_depth, default_depth, fbbpp,
			     Support32bppFb | Support24bppFb | SupportConvert32to24 | SupportConvert24to32))
		return FALSE;
	
	xf86PrintDepthBpp(pScrn);

	/* Get the depth24 pixmap format */
	if (pScrn->depth == 24 && pix24bpp == 0)
		pix24bpp = xf86GetBppFromDepth(pScrn, 24);

	/* color weight */
	if (pScrn->depth > 8) {
		rgb zero = { 0, 0, 0 };
		if (!xf86SetWeight(pScrn, zero, zero))
			return FALSE;
		
		
		// ARGB
		
		pScrn->mask.red=0xff0000;
		pScrn->mask.green=0xff00;
		pScrn->mask.blue=0xff;

		pScrn->offset.red=16;
		pScrn->offset.green=8;
		pScrn->offset.blue=0;
	}
	
	/* visual init */
	if (!xf86SetDefaultVisual(pScrn, -1))
		return FALSE;

	/* We don't currently support DirectColor at > 8bpp */
	if (pScrn->depth > 8 && pScrn->defaultVisual != TrueColor) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "requested default visual"
			   " (%s) is not supported at depth %d\n",
			   xf86GetVisualName(pScrn->defaultVisual), pScrn->depth);
		return FALSE;
	}

	{
		Gamma zeros = {0.0, 0.0, 0.0};

		if (!xf86SetGamma(pScrn,zeros)) {
			return FALSE;
		}
	}

	
	pScrn->progClock = TRUE;
	pScrn->rgbBits   = 8;
	pScrn->chipset   = "fbdev";
	pScrn->videoRam  = fbdevHWGetVidmem(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "hardware: %s (video memory:"
		   " %dkB)\n", fbdevHWGetName(pScrn), pScrn->videoRam/1024);

	/* handle options */
	xf86CollectOptions(pScrn, NULL);
	if (!(fPtr->Options = xalloc(sizeof(FBDevOptions))))
		return FALSE;
	memcpy(fPtr->Options, FBDevOptions, sizeof(FBDevOptions));
	xf86ProcessOptions(pScrn->scrnIndex, fPtr->pEnt->device->options, fPtr->Options);

	debug = xf86ReturnOptValBool(fPtr->Options, OPTION_DEBUG, FALSE);

	/* select video modes */

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "checking modes against framebuffer device...\n");
	fbdevHWSetVideoModes(pScrn);

	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "checking modes against monitor...\n");
	{
		DisplayModePtr mode, first = mode = pScrn->modes;
		
		if (mode != NULL) do {
			mode->status = xf86CheckModeForMonitor(mode, pScrn->monitor);
			mode = mode->next;
		} while (mode != NULL && mode != first);

		xf86PruneDriverModes(pScrn);
	}

	if (NULL == pScrn->modes)
		fbdevHWUseBuildinMode(pScrn);
	pScrn->currentMode = pScrn->modes;

	pScrn->displayWidth = pScrn->virtualX;	/* ShadowFB handles this correctly */

	xf86PrintModes(pScrn);

	/* Set display resolution */
	xf86SetDpi(pScrn, 0, 0);

	/* Load bpp-specific modules */
	switch ((type = fbdevHWGetType(pScrn)))
	{
	case FBDEVHW_PACKED_PIXELS:
		switch (pScrn->bitsPerPixel)
		{
		case 8:
		case 16:
		case 24:
		case 32:
			break;
		default:
			xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"unsupported number of bits per pixel: %d",
			pScrn->bitsPerPixel);
			return FALSE;
		}
		break;
       default:
               xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                          "unrecognised fbdev hardware type (%d)\n", type);
               return FALSE;
	}
	if (xf86LoadSubModule(pScrn, "fb") == NULL) {
		FBDevFreeRec(pScrn);
		return FALSE;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "loading shadowfb");
	if (!xf86LoadSubModule(pScrn, "shadowfb")) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Couldn't load shadowfb module:\n");
		return FALSE;
	}
		xf86DrvMsg(pScrn->scrnIndex, X_CONFIG, "using shadow"
		   " framebuffer\n");

		/* initialize the GPU */
	fPtr->xe=&fPtr->xe_;
	Xe_Init(fPtr->xe);

	TRACE_EXIT("PreInit");
	return TRUE;
}


static Bool
FBDevShadowInit(ScreenPtr pScreen)
{
    ShadowFBInit(pScreen, XenosRefreshArea);

    return TRUE;
}


static Bool
FBDevScreenInit(int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
	ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
	FBDevPtr fPtr = FBDEVPTR(pScrn);
	VisualPtr visual;
	int init_picture = 0;
	int ret, flags;
	int type;

	TRACE_ENTER("FBDevScreenInit");

#if 0
	ErrorF("\tbitsPerPixel=%d, depth=%d, defaultVisual=%s\n"
	       "\tmask: %x,%x,%x, offset: %d,%d,%d\n",
	       pScrn->bitsPerPixel,
	       pScrn->depth,
	       xf86GetVisualName(pScrn->defaultVisual),
	       pScrn->mask.red,pScrn->mask.green,pScrn->mask.blue,
	       pScrn->offset.red,pScrn->offset.green,pScrn->offset.blue);
#endif

	if (NULL == (fPtr->fbmem = fbdevHWMapVidmem(pScrn))) {
	        xf86DrvMsg(scrnIndex,X_ERROR,"mapping of video memory"
			   " failed\n");
		return FALSE;
	}
	fPtr->fboff = fbdevHWLinearOffset(pScrn);

	fbdevHWSave(pScrn);

/*	if (!fbdevHWModeInit(pScrn, pScrn->currentMode)) {
		xf86DrvMsg(scrnIndex,X_ERROR,"mode initialization failed\n");
		return FALSE;
	}*/
	fbdevHWSaveScreen(pScreen, SCREEN_SAVER_ON);
	fbdevHWAdjustFrame(scrnIndex,0,0,0);

	/* mi layer */
	miClearVisualTypes();
	if (pScrn->bitsPerPixel > 8) {
		if (!miSetVisualTypes(pScrn->depth, TrueColorMask, pScrn->rgbBits, TrueColor)) {
			xf86DrvMsg(scrnIndex,X_ERROR,"visual type setup failed"
				   " for %d bits per pixel [1]\n",
				   pScrn->bitsPerPixel);
			return FALSE;
		}
	} else {
		if (!miSetVisualTypes(pScrn->depth,
				      miGetDefaultVisualMask(pScrn->depth),
				      pScrn->rgbBits, pScrn->defaultVisual)) {
			xf86DrvMsg(scrnIndex,X_ERROR,"visual type setup failed"
				   " for %d bits per pixel [2]\n",
				   pScrn->bitsPerPixel);
			return FALSE;
		}
	}
	if (!miSetPixmapDepths()) {
	  xf86DrvMsg(scrnIndex,X_ERROR,"pixmap depth setup failed\n");
	  return FALSE;
	}

	fPtr->fbstart = fPtr->fbmem + fPtr->fboff;
	
		/* create a render target (the framebuffer) */
	struct XenosSurface *fb = Xe_GetFramebufferSurface(fPtr->xe);
	Xe_SetRenderTarget(fPtr->xe, fb);

		/* let's define a vertex buffer format */
	static const struct XenosVBFFormat vbf =
	{
		2, {
		  {XE_USAGE_POSITION, 0, XE_TYPE_FLOAT2},
		  {XE_USAGE_TEXCOORD, 0, XE_TYPE_FLOAT2},
		}
	};
	
	float xo=2.0f/pScrn->virtualX;
	float yo=2.0f/pScrn->virtualY;
	
	float rect[] = {
		-1.0f,  1.0f, 0.0f, 0.0f,
		 1.0f-xo,  1.0f, 1.0f, 0.0f,
		-1.0f, -1.0f+yo, 0.0f, 1.0f,
	};
	
		/* load pixel shader */
	fPtr->sh_ps = Xe_LoadShaderFromMemory(fPtr->xe, content_datapspsu);
	Xe_InstantiateShader(fPtr->xe, fPtr->sh_ps, 0);

		/* load vertex shader */
	fPtr->sh_vs = Xe_LoadShaderFromMemory(fPtr->xe, content_datavsvsu);
	Xe_InstantiateShader(fPtr->xe, fPtr->sh_vs, 0);
	Xe_ShaderApplyVFetchPatches(fPtr->xe, fPtr->sh_vs, 0, &vbf);

		/* create and fill vertex buffer */
	fPtr->vb = Xe_CreateVertexBuffer(fPtr->xe, sizeof(rect));
	void *v = Xe_VB_Lock(fPtr->xe, fPtr->vb, 0, sizeof(rect), XE_LOCK_WRITE);
	memcpy(v, rect, sizeof(rect));
	Xe_VB_Unlock(fPtr->xe, fPtr->vb);

        fPtr->gfxplane = Xe_CreateTexture(fPtr->xe, pScrn->virtualX, pScrn->virtualY, 1, XE_FMT_8888 | XE_FMT_ARGB, 0);
	fPtr->gfxplane->use_filtering = 0;
	fPtr->gfxplane->u_addressing = XE_TEXADDR_CLAMP;
	fPtr->gfxplane->v_addressing = XE_TEXADDR_CLAMP;
	
	fPtr->shadow = Xe_Surface_LockRect(fPtr->xe, fPtr->gfxplane, 0, 0, 0, 0, XE_LOCK_WRITE);
	Xe_Surface_Unlock(fPtr->xe, fPtr->gfxplane);

	edram_init(fPtr->xe);

        if (!fPtr->shadow) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			   "Failed to allocate shadow framebuffer\n");
		return FALSE;
	}
	xf86DrvMsg(scrnIndex,X_INFO,"Real shadow set up at %p, X : %d Y %d Bits %d\n", fPtr->shadow, 
			pScrn->virtualX, pScrn->virtualY, pScrn->bitsPerPixel);

	switch ((type = fbdevHWGetType(pScrn)))
	{
	case FBDEVHW_PACKED_PIXELS:
		switch (pScrn->bitsPerPixel) {
		case 8:
		case 16:
		case 24:
		case 32:
			ret = fbScreenInit(pScreen, fPtr->shadow, pScrn->virtualX,
					   pScrn->virtualY, pScrn->xDpi,
					   pScrn->yDpi, pScrn->displayWidth,
					   pScrn->bitsPerPixel);
			init_picture = 1;
			break;
	 	default:
			xf86DrvMsg(scrnIndex, X_ERROR,
				   "internal error: invalid number of bits per"
				   " pixel (%d) encountered in"
				   " FBDevScreenInit()\n", pScrn->bitsPerPixel);
			ret = FALSE;
			break;
		}
		break;
	case FBDEVHW_INTERLEAVED_PLANES:
		/* This should never happen ...
		* we should check for this much much earlier ... */
		xf86DrvMsg(scrnIndex, X_ERROR,
		           "internal error: interleaved planes are not yet "
			   "supported by the fbdev driver\n");
		ret = FALSE;
		break;
	case FBDEVHW_TEXT:
		/* This should never happen ...
		* we should check for this much much earlier ... */
		xf86DrvMsg(scrnIndex, X_ERROR,
		           "internal error: text mode is not supported by the "
			   "fbdev driver\n");
		ret = FALSE;
		break;
	case FBDEVHW_VGA_PLANES:
		/* Not supported yet */
		xf86DrvMsg(scrnIndex, X_ERROR,
		           "internal error: EGA/VGA Planes are not yet "
			   "supported by the fbdev driver\n");
		ret = FALSE;
		break;
	default:
		xf86DrvMsg(scrnIndex, X_ERROR,
		           "internal error: unrecognised hardware type (%d) "
			   "encountered in FBDevScreenInit()\n", type);
		ret = FALSE;
		break;
	}
	if (!ret)
		return FALSE;

	if (pScrn->bitsPerPixel > 8) {
		/* Fixup RGB ordering */
		visual = pScreen->visuals + pScreen->numVisuals;
		while (--visual >= pScreen->visuals) {
			if ((visual->class | DynamicClass) == DirectColor) {
				visual->offsetRed   = pScrn->offset.red;
				visual->offsetGreen = pScrn->offset.green;
				visual->offsetBlue  = pScrn->offset.blue;
				visual->redMask     = pScrn->mask.red;
				visual->greenMask   = pScrn->mask.green;
				visual->blueMask    = pScrn->mask.blue;
			}
		}
	}

	/* must be after RGB ordering fixed */
	if (init_picture && !fbPictureInit(pScreen, NULL, 0))
		xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
			   "Render extension initialisation failed\n");

	if (!FBDevShadowInit(pScreen)) {
	    xf86DrvMsg(scrnIndex, X_ERROR,
		       "shadow framebuffer initialization failed\n");
	    return FALSE;
	}

//        FBDevDGAInit(pScrn, pScreen);

	xf86SetBlackWhitePixels(pScreen);
	miInitializeBackingStore(pScreen);
	xf86SetBackingStore(pScreen);

	/* software cursor */
	miDCInitialize(pScreen, xf86GetPointerScreenFuncs());

	/* colormap */
	switch ((type = fbdevHWGetType(pScrn)))
	{
	/* XXX It would be simpler to use miCreateDefColormap() in all cases. */
	case FBDEVHW_PACKED_PIXELS:
		if (!miCreateDefColormap(pScreen)) {
			xf86DrvMsg(scrnIndex, X_ERROR,
                                   "internal error: miCreateDefColormap failed "
				   "in FBDevScreenInit()\n");
			return FALSE;
		}
		break;
	default:
		xf86DrvMsg(scrnIndex, X_ERROR,
		           "internal error: unrecognised fbdev hardware type "
			   "(%d) encountered in FBDevScreenInit()\n", type);
		return FALSE;
	}

	flags = CMAP_PALETTED_TRUECOLOR;
	if(!xf86HandleColormaps(pScreen, 256, 8, fbdevHWLoadPaletteWeak(), 
				NULL, flags))
		return FALSE;

	xf86DPMSInit(pScreen, fbdevHWDPMSSetWeak(), 0);

	pScreen->SaveScreen = fbdevHWSaveScreenWeak();

	/* Wrap the current CloseScreen function */
	fPtr->CloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = FBDevCloseScreen;

	{
	    XF86VideoAdaptorPtr *ptr;

	    int n = xf86XVListGenericAdaptors(pScrn,&ptr);
	    if (n) {
		xf86XVScreenInit(pScreen,ptr,n);
	    }
	}

	TRACE_EXIT("FBDevScreenInit");

	return TRUE;
}

static Bool
FBDevCloseScreen(int scrnIndex, ScreenPtr pScreen)
{
	ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
	FBDevPtr fPtr = FBDEVPTR(pScrn);
	
	fbdevHWRestore(pScrn);
	fbdevHWUnmapVidmem(pScrn);
/*	if (fPtr->shadow) {
//	    shadowRemove(pScreen, pScreen->GetScreenPixmap(pScreen));
	    xfree(fPtr->shadow);
	    fPtr->shadow = NULL;
	}*/
	if (fPtr->pDGAMode) {
	  xfree(fPtr->pDGAMode);
	  fPtr->pDGAMode = NULL;
	  fPtr->nDGAMode = 0;
	}
	pScrn->vtSema = FALSE;

	pScreen->CreateScreenResources = fPtr->CreateScreenResources;
	pScreen->CloseScreen = fPtr->CloseScreen;
	return (*pScreen->CloseScreen)(scrnIndex, pScreen);
}

/***********************************************************************
 * DGA stuff
 ***********************************************************************/
static Bool FBDevDGAOpenFramebuffer(ScrnInfoPtr pScrn, char **DeviceName,
				   unsigned char **ApertureBase,
				   int *ApertureSize, int *ApertureOffset,
				   int *flags);
static Bool FBDevDGASetMode(ScrnInfoPtr pScrn, DGAModePtr pDGAMode);
static void FBDevDGASetViewport(ScrnInfoPtr pScrn, int x, int y, int flags);

static Bool
FBDevDGAOpenFramebuffer(ScrnInfoPtr pScrn, char **DeviceName,
		       unsigned char **ApertureBase, int *ApertureSize,
		       int *ApertureOffset, int *flags)
{
    *DeviceName = NULL;		/* No special device */
    *ApertureBase = (unsigned char *)(pScrn->memPhysBase);
    *ApertureSize = pScrn->videoRam;
    *ApertureOffset = pScrn->fbOffset;
    *flags = 0;

    return TRUE;
}

static Bool
FBDevDGASetMode(ScrnInfoPtr pScrn, DGAModePtr pDGAMode)
{
    DisplayModePtr pMode;
    int scrnIdx = pScrn->pScreen->myNum;
    int frameX0, frameY0;

    if (pDGAMode) {
	pMode = pDGAMode->mode;
	frameX0 = frameY0 = 0;
    }
    else {
	if (!(pMode = pScrn->currentMode))
	    return TRUE;

	frameX0 = pScrn->frameX0;
	frameY0 = pScrn->frameY0;
    }

    if (!(*pScrn->SwitchMode)(scrnIdx, pMode, 0))
	return FALSE;
    (*pScrn->AdjustFrame)(scrnIdx, frameX0, frameY0, 0);

    return TRUE;
}

static void
FBDevDGASetViewport(ScrnInfoPtr pScrn, int x, int y, int flags)
{
    (*pScrn->AdjustFrame)(pScrn->pScreen->myNum, x, y, flags);
}

static int
FBDevDGAGetViewport(ScrnInfoPtr pScrn)
{
    return (0);
}

static DGAFunctionRec FBDevDGAFunctions =
{
    FBDevDGAOpenFramebuffer,
    NULL,       /* CloseFramebuffer */
    FBDevDGASetMode,
    FBDevDGASetViewport,
    FBDevDGAGetViewport,
    NULL,       /* Sync */
    NULL,       /* FillRect */
    NULL,       /* BlitRect */
    NULL,       /* BlitTransRect */
};

static void
FBDevDGAAddModes(ScrnInfoPtr pScrn)
{
    FBDevPtr fPtr = FBDEVPTR(pScrn);
    DisplayModePtr pMode = pScrn->modes;
    DGAModePtr pDGAMode;

    do {
	pDGAMode = xrealloc(fPtr->pDGAMode,
			    (fPtr->nDGAMode + 1) * sizeof(DGAModeRec));
	if (!pDGAMode)
	    break;

	fPtr->pDGAMode = pDGAMode;
	pDGAMode += fPtr->nDGAMode;
	(void)memset(pDGAMode, 0, sizeof(DGAModeRec));

	++fPtr->nDGAMode;
	pDGAMode->mode = pMode;
	pDGAMode->flags = DGA_CONCURRENT_ACCESS | DGA_PIXMAP_AVAILABLE;
	pDGAMode->byteOrder = pScrn->imageByteOrder;
	pDGAMode->depth = pScrn->depth;
	pDGAMode->bitsPerPixel = pScrn->bitsPerPixel;
	pDGAMode->red_mask = pScrn->mask.red;
	pDGAMode->green_mask = pScrn->mask.green;
	pDGAMode->blue_mask = pScrn->mask.blue;
	pDGAMode->visualClass = pScrn->bitsPerPixel > 8 ?
	    TrueColor : PseudoColor;
	pDGAMode->xViewportStep = 1;
	pDGAMode->yViewportStep = 1;
	pDGAMode->viewportWidth = pMode->HDisplay;
	pDGAMode->viewportHeight = pMode->VDisplay;

	if (fPtr->lineLength)
	  pDGAMode->bytesPerScanline = fPtr->lineLength;
	else
	  pDGAMode->bytesPerScanline = fPtr->lineLength = fbdevHWGetLineLength(pScrn);

	pDGAMode->imageWidth = pMode->HDisplay;
	pDGAMode->imageHeight =  pMode->VDisplay;
	pDGAMode->pixmapWidth = pDGAMode->imageWidth;
	pDGAMode->pixmapHeight = pDGAMode->imageHeight;
	pDGAMode->maxViewportX = pScrn->virtualX -
				    pDGAMode->viewportWidth;
	pDGAMode->maxViewportY = pScrn->virtualY -
				    pDGAMode->viewportHeight;

	pDGAMode->address = fPtr->fbstart;

	pMode = pMode->next;
    } while (pMode != pScrn->modes);
}

static Bool
FBDevDGAInit(ScrnInfoPtr pScrn, ScreenPtr pScreen)
{
#ifdef XFreeXDGA
    FBDevPtr fPtr = FBDEVPTR(pScrn);

    if (pScrn->depth < 8)
	return FALSE;

    if (!fPtr->nDGAMode)
	FBDevDGAAddModes(pScrn);

    return (DGAInit(pScreen, &FBDevDGAFunctions,
	    fPtr->pDGAMode, fPtr->nDGAMode));
#else
    return TRUE;
#endif
}

static Bool
FBDevDriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer ptr)
{
    xorgHWFlags *flag;
    
    switch (op) {
	case GET_REQUIRED_HW_INTERFACES:
	    flag = (CARD32*)ptr;
	    (*flag) = 0;
	    return TRUE;
	default:
	    return FALSE;
    }
}