#ifndef GPU_REG_H
#define GPU_REG_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef short		   s16;
typedef unsigned int   u32;
typedef			 int   s32;
typedef unsigned long long int u64;


// SAME AS GPU HW
#define PREC	(12)

/*
#ifdef GPU_HW
#else
	#define PREC	(11)
#endif
*/

struct GPURdrCtx;

struct Vertex {
	Vertex() {}
	Vertex(int x_, int y_):x(x_),y(y_) {}

	int x;
	int y;
	u8  r;
	u8  g;
	u8  b;
	u8  u;
	u8  v;
};

struct Interpolator {
	int rinterp;
	int ginterp;
	int binterp;
	int uinterp;
	int vinterp;
};

struct PrimitiveSetup {
	//
	// Triangle and rect stuff
	//
	int minXTri,maxXTri;
	int minYTri,maxYTri;
	int minTriDAX0;
	int maxTriDAX1;
	int minTriDAY0;
	int maxTriDAY1;
	int sizeW;
	int sizeH;

	//
	// Triangle,line and rect stuff
	//
	int uxR,vyR,uxG,vyG,uxB,vyB;
	int uxU,vyU,uxV,vyV;

	// Triangle Stuff
	int bias[3];
	int a,b,c,d,e,f;
	bool special; // With horizontals in the triangle.

	// Line only stuff.
	bool swapAxis;
	bool isNegXAxis;
	bool isNegYAxis;

	int  DET;
	bool DETPOS;

	int  aDX2;
	int  aDY2;
	int  stepX;
	int  stepY;
	int  DLine;

	int w[3];	// Line Equation Result
	bool h[3];	// Which line goes away
	bool further[3];

//	void SetupFurtherDir	();
	void SetupFurtherBool	(int dir, int* w);

	/* Return TRUE if primitive render, FALSE if skip */
	bool BBox				(GPURdrCtx& psx, Vertex** ppVertex, int vCount);
	bool Setup				(GPURdrCtx& psx, Vertex** ppVtx, bool isLineCommand);
	bool SetupRect			(GPURdrCtx& psx, Vertex** ppVtx);

	void NextLinePixel		();
	void LineEqu			(int x, int y, Vertex** ppVtx, int* equ);
	bool perPixelTriangle	(int x, int y, Vertex** ppVtx);
	void perPixelInterp		(int x, int y, Vertex** ppVtx, Interpolator& interp);
};

typedef void (*postRendercallback)(GPURdrCtx& ctx, void* userContext, u8 commandID, u32 commandNumber);

struct GPURdrCtx {
	GPURdrCtx();

//	int		offsetHack;
	// E1
	u16		pageX4;
	u16		pageY1;
	u8		semiTransp2;
	u8		textFormat2;

	bool	dither;
	bool	displayAreaEnable;
	bool	disableTexture;
	bool	texXFlip;
	bool	texYFlip;

	// E2
	u8		texMaskX5;
	u8		texMaskY5;
	u8		texOffsX5;
	u8		texOffsY5;

	// E3
	u16		drAreaX0_10;
	u16		drAreaY0_9;

	// E4
	u16		drAreaX1_10;
	u16		drAreaY1_9;

	// E5
	s16		offsetX_s11;
	s16		offsetY_s11;

	// E6
	bool	forceMask;
	bool	checkMask;

	// GP1 Stuff
	bool	interlaced;
	bool	currentInterlaceFrameOdd;
	bool	GP1_MasterTexDisable;

	//
	bool	rtUseSemiTransp;
	bool	rtIsTextured;
	bool	rtIsTexModRGB;
	bool	rtIsPerVtx;
	bool	isLine;			// To FORCE dithering in any case.

	u16		rtClutX;
	u16		rtClutY;

	PrimitiveSetup	primitiveSetup;

	u16*	swBuffer;
	u16		palette[256];

	postRendercallback	callback;
	void*	userContext;

	void	commandDecoder(u32* pStream, u64* pStamps, u8* isGP1Stream, u32 size, postRendercallback callback, void* userContext, u64 maxTime);

	void	writeGP0		(u32 word);
	void	writeGP1		(u32 word);

	void	checkSizeLoad	();
	void	execPrimitive	();
	void	loadColor		(u32 operand);

	int		RenderTriangle		(Vertex* pVertex, u8 id0, u8 id1, u8 id2);
	int		RenderTriangleGPU	(Vertex* pVertex, u8 id0, u8 id1, u8 id2);
	int		RenderTriangleNS	(Vertex* pVertex, u8 id0, u8 id1, u8 id2, int refColor);
	int		RenderTriangleFurther(Vertex* pVertex, u8 id0, u8 id1, u8 id2);
	int		RenderTriangleFurtherPair(Vertex* pVertex, u8 id0, u8 id1, u8 id2);
	int		RenderTriangleNSPair(Vertex* pVertex, u8 id0, u8 id1, u8 id2, int refColor);
	void	RenderRect		(Vertex* pVertex);
	void	RenderLine		(Vertex* pVertex, u8 v0, u8 v1);

	u16		sampleTexture	(u8 Upixel, u8 Vpixel);
	void	textureUnit		(int Uinterp, int Vinterp, u8& Upixel, u8& Vpixel);
	void	pixelPipeline	(s16 x, s16 y, Interpolator& interp);

	void	referenceFILL	(int x, int y, int w, int h, bool interlaced, bool renderOddFrame, u32 bgr);
	void	performRefresh	(int command, int commandID);

	u16		blend			(int x, int y, u16 target, bool allowBlend, bool tBit15,int FR,int FG,int FB);
};

bool otherSide(int codeA, int codeB);
void printTotalTimeCycle();

void loadRefRect(const char* fileName);
void loadRefQuadNoTex(const char* fileName);
void loadRefQuadTex(const char* fileName);


#endif
