//
// Copyright (c) 2004 K. Wilkins
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from
// the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//

//////////////////////////////////////////////////////////////////////////////
//                       Handy - An Atari Lynx Emulator                     //
//                          Copyright (c) 1996,1997                         //
//                                 K. Wilkins                               //
//////////////////////////////////////////////////////////////////////////////
// Suzy emulation class                                                     //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
// This class emulates the Suzy chip within the lynx. This provides math    //
// and sprite painting facilities. SpritePaint() is called from within      //
// the Mikey POKE functions when SPRGO is set and is called via the system  //
// object to keep the interface clean.                                      //
//                                                                          //
//    K. Wilkins                                                            //
// August 1997                                                              //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
// Revision History:                                                        //
// -----------------                                                        //
//                                                                          //
// 01Aug1997 KW Document header added & class documented.                   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#define SUSIE_CPP

#include "system.h"
#include "susie.h"
#include "lynxdef.h"

//
// As the Susie sprite engine only ever sees system RAM
// wa can access this directly without the hassle of
// going through the system object, much faster
//
#define RAM_PEEK(m)				(mRamPointer[(uint16)(m)])
#define RAM_PEEKW(m)			(mRamPointer[(uint16)(m)]+(mRamPointer[(uint16)((m)+1)]<<8))
#define RAM_POKE(m1,m2)			{mRamPointer[(uint16)(m1)]=(m2);}

uint32 cycles_used=0;

CSusie::CSusie(CSystem& parent)
	:mSystem(parent)
{
	Reset();
}

CSusie::~CSusie()
{
}

void CSusie::Reset(void)
{
	// Fetch pointer to system RAM, faster than object access
	// and seeing as Susie only ever sees RAM.

	mRamPointer=mSystem.GetRamPointer();

	// Reset ALL variables

	mTMPADR.Val16=0;
	mTILTACUM.Val16=0;
	mHOFF.Val16=0;
	mVOFF.Val16=0;
	mVIDBAS.Val16=0;
	mCOLLBAS.Val16=0;
	mVIDADR.Val16=0;
	mCOLLADR.Val16=0;
	mSCBNEXT.Val16=0;
	mSPRDLINE.Val16=0;
	mHPOSSTRT.Val16=0;
	mVPOSSTRT.Val16=0;
	mSPRHSIZ.Val16=0;
	mSPRVSIZ.Val16=0;
	mSTRETCH.Val16=0;
	mTILT.Val16=0;
	mSPRDOFF.Val16=0;
	mSPRVPOS.Val16=0;
	mCOLLOFF.Val16=0;
	mVSIZACUM.Val16=0;
	mHSIZACUM.Val16=0;
	mHSIZOFF.Val16=0x007f;
	mVSIZOFF.Val16=0x007f;
	mSCBADR.Val16=0;
	mPROCADR.Val16=0;

	// Must be initialised to this due to
	// stun runner math initialisation bug
	// see whatsnew for 0.7
	mMATHABCD.Long=0xffffffff;
	mMATHEFGH.Long=0xffffffff;
	mMATHJKLM.Long=0xffffffff;
	mMATHNP.Long=0xffff;

	mMATHAB_sign=1;
	mMATHCD_sign=1;
	mMATHEFGH_sign=1;

	mSPRCTL0_Type=0;
	mSPRCTL0_Vflip=0;
	mSPRCTL0_Hflip=0;
	mSPRCTL0_PixelBits=0;

	mSPRCTL1_StartLeft=0;
	mSPRCTL1_StartUp=0;
	mSPRCTL1_SkipSprite=0;
	mSPRCTL1_ReloadPalette=0;
	mSPRCTL1_ReloadDepth=0;
	mSPRCTL1_Sizing=0;
	mSPRCTL1_Literal=0;

	mSPRCOLL_Number=0;
	mSPRCOLL_Collide=0;

	mSPRSYS_StopOnCurrent=0;
	mSPRSYS_LeftHand=0;
	mSPRSYS_VStretch=0;
	mSPRSYS_NoCollide=0;
	mSPRSYS_Accumulate=0;
	mSPRSYS_SignedMath=0;
	mSPRSYS_Status=0;
	mSPRSYS_UnsafeAccess=0;
	mSPRSYS_LastCarry=0;
	mSPRSYS_Mathbit=0;
	mSPRSYS_MathInProgress=0;

	mSUZYBUSEN=false;

	mSPRINIT.Byte=0;

	mSPRGO=false;
	mEVERON=false;

	for(int loop=0;loop<16;loop++) mPenIndex[loop]=loop;

	hquadoff = vquadoff = 0;

	mJOYSTICK.Byte=0;
	mSWITCHES.Byte=0;
}


void CSusie::DoMathMultiply(void)
{
	mSPRSYS_Mathbit=false;

	// Multiplies with out sign or accumulate take 44 ticks to complete.
	// Multiplies with sign and accumulate take 54 ticks to complete. 
	//
	//    AB                                    EFGH
	//  * CD                                  /   NP
	// -------                            -----------
	//  EFGH                                    ABCD
	// Accumulate in JKLM         Remainder in (JK)LM
	//


	uint32 result;

	// Basic multiply is ALWAYS unsigned, sign conversion is done later
	result=(uint32)mMATHABCD.Words.AB*(uint32)mMATHABCD.Words.CD;
	mMATHEFGH.Long=result;

	if(mSPRSYS_SignedMath)
	{
		// Add the sign bits, only >0 is +ve result
		mMATHEFGH_sign=mMATHAB_sign+mMATHCD_sign;
		if(!mMATHEFGH_sign)
		{
			mMATHEFGH.Long^=0xffffffff;
			mMATHEFGH.Long++;
		}
	}

	// Check overflow, if B31 has changed from 1->0 then its overflow time
	if(mSPRSYS_Accumulate)
	{
		uint32 tmp=mMATHJKLM.Long+mMATHEFGH.Long;
		// Save accumulated result
		mMATHJKLM.Long=tmp;
	}
}

void CSusie::DoMathDivide(void)
{
	mSPRSYS_Mathbit=false;

	//
	// Divides take 176 + 14*N ticks
	// (N is the number of most significant zeros in the divisor.)
	//
	//    AB                                    EFGH
	//  * CD                                  /   NP
	// -------                            -----------
	//  EFGH                                    ABCD
	// Accumulate in JKLM         Remainder in (JK)LM
	//

	// Divide is ALWAYS unsigned arithmetic...
	if(mMATHNP.Long)
	{
		mMATHABCD.Long=mMATHEFGH.Long/mMATHNP.Long;
		mMATHJKLM.Long=mMATHEFGH.Long%mMATHNP.Long;
	}
	else
	{
		mMATHABCD.Long=0xffffffff;
		mMATHJKLM.Long=0;
		mSPRSYS_Mathbit=true;
	}
}


uint32 CSusie::PaintSprites(void)
{
	int	sprcount=0;
	int data=0;
	int everonscreen=0;

	if(!mSUZYBUSEN || !mSPRGO)
		return 0;

	cycles_used=0;

	do
	{
		everonscreen = 0;

		// Step 1 load up the SCB params into Susie

		// And thus it is documented that only the top byte of SCBNEXT is used.
		// Its mentioned under the bits that are broke section in the bluebook
		if(!(mSCBNEXT.Val16&0xff00))
		{
			mSPRSYS_Status=0;	// Engine has finished
			mSPRGO=false;
			break;
		}
		else
		{
			mSPRSYS_Status=1;
		}

		mTMPADR.Val16=mSCBNEXT.Val16;	// Copy SCB pointer
		mSCBADR.Val16=mSCBNEXT.Val16;	// Copy SCB pointer

		data=RAM_PEEK(mTMPADR.Val16);			// Fetch control 0
		mSPRCTL0_Type=data&0x0007;
		mSPRCTL0_Vflip=data&0x0010;
		mSPRCTL0_Hflip=data&0x0020;
		mSPRCTL0_PixelBits=((data&0x00c0)>>6)+1;
		mTMPADR.Val16+=1;

		data=RAM_PEEK(mTMPADR.Val16);			// Fetch control 1
		mSPRCTL1_StartLeft=data&0x0001;
		mSPRCTL1_StartUp=data&0x0002;
		mSPRCTL1_SkipSprite=data&0x0004;
		mSPRCTL1_ReloadPalette=data&0x0008;
		mSPRCTL1_ReloadDepth=(data&0x0030)>>4;
		mSPRCTL1_Sizing=data&0x0040;	
		mSPRCTL1_Literal=data&0x0080;
		mTMPADR.Val16+=1;

		data=RAM_PEEK(mTMPADR.Val16);			// Collision num
		mSPRCOLL_Number=data&0x000f;
		mSPRCOLL_Collide=data&0x0020;
		mTMPADR.Val16+=1;

		mSCBNEXT.Val16=RAM_PEEKW(mTMPADR.Val16);	// Next SCB
		mTMPADR.Val16+=2;

		cycles_used+=5*SPR_RDWR_CYC;

		// Initialise the collision depositary

// Although Tom Schenck says this is correct, it doesnt appear to be
//		if(!mSPRCOLL_Collide && !mSPRSYS_NoCollide)
//		{
//			mCollision=RAM_PEEK((mSCBADR.Val16+mCOLLOFF.Val16)&0xffff);
//			mCollision&=0x0f;
//		}
		mCollision=0;

		// Check if this is a skip sprite

		if(!mSPRCTL1_SkipSprite)
		{

			mSPRDLINE.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite pack data
			mTMPADR.Val16+=2;

			mHPOSSTRT.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite horizontal start position
			mTMPADR.Val16+=2;

			mVPOSSTRT.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite vertical start position
			mTMPADR.Val16+=2;

			cycles_used+=6*SPR_RDWR_CYC;

			bool enable_sizing=false;
			bool enable_stretch=false;
			bool enable_tilt=false;
		
			// Optional section defined by reload type in Control 1

			switch(mSPRCTL1_ReloadDepth)
			{
				case 1:
					enable_sizing=true;

					mSPRHSIZ.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite Horizontal size
					mTMPADR.Val16+=2;

					mSPRVSIZ.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite Verticalal size
					mTMPADR.Val16+=2;

					cycles_used+=4*SPR_RDWR_CYC;
					break;

				case 2:
					enable_sizing=true;
					enable_stretch=true;

					mSPRHSIZ.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite Horizontal size
					mTMPADR.Val16+=2;

					mSPRVSIZ.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite Verticalal size
					mTMPADR.Val16+=2;

					mSTRETCH.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite stretch
					mTMPADR.Val16+=2;

					cycles_used+=6*SPR_RDWR_CYC;
					break;

				case 3:
					enable_sizing=true;
					enable_stretch=true;
					enable_tilt=true;

					mSPRHSIZ.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite Horizontal size
					mTMPADR.Val16+=2;

					mSPRVSIZ.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite Verticalal size
					mTMPADR.Val16+=2;

					mSTRETCH.Val16=RAM_PEEKW(mTMPADR.Val16);	// Sprite stretch
					mTMPADR.Val16+=2;

					mTILT.Val16=RAM_PEEKW(mTMPADR.Val16);		// Sprite tilt
					mTMPADR.Val16+=2;

					cycles_used+=8*SPR_RDWR_CYC;
					break;

				default:
					break;
			}

			// Optional Palette reload

			if(!mSPRCTL1_ReloadPalette)
			{
				for(int loop=0;loop<8;loop++)
				{
					uint8 data_tmp = RAM_PEEK(mTMPADR.Val16++);
					mPenIndex[loop*2]=(data_tmp>>4)&0x0f;
					mPenIndex[(loop*2)+1]=data_tmp&0x0f;
				}
				// Increment cycle count for the reads
				cycles_used+=8*SPR_RDWR_CYC;
			}

			// Now we can start painting
		
			// Quadrant drawing order is: SE,NE,NW,SW
			// start quadrant is given by sprite_control1:0 & 1

			// Setup screen start end variables

			int screen_h_start=(int16)mHOFF.Val16;
			int screen_h_end=(int16)mHOFF.Val16+SCREEN_WIDTH;
			int screen_v_start=(int16)mVOFF.Val16;
			int screen_v_end=(int16)mVOFF.Val16+SCREEN_HEIGHT;

			int world_h_mid=screen_h_start+0x8000+(SCREEN_WIDTH/2);
			int world_v_mid=screen_v_start+0x8000+(SCREEN_HEIGHT/2);

			bool superclip=false;
			int quadrant=0;
			int hsign,vsign;

			if(mSPRCTL1_StartLeft)
			{
				if(mSPRCTL1_StartUp) quadrant=2; else quadrant=3;
			}
			else
			{
				if(mSPRCTL1_StartUp) quadrant=1; else quadrant=0;
			}

			// Check ref is inside screen area

			//if((int16)mHPOSSTRT.Val16<screen_h_start || (int16)mHPOSSTRT.Val16>=screen_h_end ||
			//	(int16)mVPOSSTRT.Val16<screen_v_start || (int16)mVPOSSTRT.Val16>=screen_v_end) superclip=true;


			// Quadrant mapping is:	SE	NE	NW	SW
			//						0	1	2	3
			// hsign				+1	+1	-1	-1
			// vsign				+1	-1	-1	+1
			//
			//
			//		2 | 1
			//     -------
			//      3 | 0
			//

			// Loop for 4 quadrants

			for(int loop=0;loop<4;loop++)	
			{
				int sprite_v=mVPOSSTRT.Val16;
				int sprite_h=mHPOSSTRT.Val16;

				bool render=false;

				// Set quadrand multipliers
				hsign=(quadrant==0 || quadrant==1)?1:-1;
				vsign=(quadrant==0 || quadrant==3)?1:-1;

				//Use h/v flip to invert v/hsign

				if(mSPRCTL0_Vflip) vsign=-vsign;
				if(mSPRCTL0_Hflip) hsign=-hsign;

				// Two different rendering algorithms used, on-screen & superclip
				// when on screen we draw in x until off screen then skip to next
				// line, BUT on superclip we draw all the way to the end of any
				// given line checking each pixel is on screen.

				if(superclip)
				{
					// Check on the basis of each quad, we only render the quad
					// IF the screen is in the quad, relative to the centre of
					// the screen which is calculated below.

					// Quadrant mapping is:	SE	NE	NW	SW
					//						0	1	2	3
					// hsign				+1	+1	-1	-1
					// vsign				+1	-1	-1	+1
					//
					//
					//	2 | 1
					//     -------
					//      3 | 0
					//
					// Quadrant mapping for superclipping must also take into account
					// the hflip, vflip bits & negative tilt to be able to work correctly
					//
					int	modquad=quadrant;
					static const int vquadflip[4]={1,0,3,2};
					static const int hquadflip[4]={3,2,1,0};

					if(mSPRCTL0_Vflip) modquad=vquadflip[modquad];
					if(mSPRCTL0_Hflip) modquad=hquadflip[modquad];

					// This is causing Eurosoccer to fail!!
					//if(enable_tilt && mTILT.Val16&0x8000) modquad=hquadflip[modquad];
					//if(quadrant == 0 && sprite_v == 219 && sprite_h == 890)
					//printf("%d:%d %d %d\n", quadrant, modquad, sprite_h, sprite_v);

					switch(modquad)
					{
						case 3:
							if((sprite_h>=screen_h_start || sprite_h<world_h_mid) && (sprite_v<screen_v_end   || sprite_v>world_v_mid)) render=true;
							break;
						case 2:
							if((sprite_h>=screen_h_start || sprite_h<world_h_mid) && (sprite_v>=screen_v_start || sprite_v<world_v_mid)) render=true;
							break;
						case 1:
							if((sprite_h<screen_h_end   || sprite_h>world_h_mid) && (sprite_v>=screen_v_start || sprite_v<world_v_mid)) render=true;
							break;
						default:
							if((sprite_h<screen_h_end   || sprite_h>world_h_mid) && (sprite_v<screen_v_end   || sprite_v>world_v_mid)) render=true;
							break;
					}
				}
				else
				{
					render=true;
				}

				// Is this quad to be rendered ??

				int pixel_height;
				int pixel_width;
				int pixel;
				int hoff,voff;
				int hloop,vloop;
				bool onscreen;

				if(render)
				{
					// Set the vertical position & offset
					voff=(int16)mVPOSSTRT.Val16-screen_v_start;

					// Zero the stretch,tilt & acum values
					mTILTACUM.Val16=0;

					// Perform the SIZOFF
					if(vsign==1) mVSIZACUM.Val16=mVSIZOFF.Val16; else mVSIZACUM.Val16=0;

					// Take the sign of the first quad (0) as the basic
					// sign, all other quads drawing in the other direction
					// get offset by 1 pixel in the other direction, this
					// fixes the squashed look on the multi-quad sprites.
//					if(vsign==-1 && loop>0) voff+=vsign;
					if(loop==0)	vquadoff=vsign;
					if(vsign!=vquadoff) voff+=vsign;
					
					for(;;)
					{
						// Vertical scaling is done here
						mVSIZACUM.Val16+=mSPRVSIZ.Val16;
						pixel_height=mVSIZACUM.Union8.High;
						mVSIZACUM.Union8.High=0;

						// Update the next data line pointer and initialise our line
						mSPRDOFF.Val16=(uint16)LineInit(0);

						// If 1 == next quad, ==0 end of sprite, anyways its END OF LINE
						if(mSPRDOFF.Val16==1)		// End of quad
						{
							mSPRDLINE.Val16+=mSPRDOFF.Val16;
							break;
						}

						if(mSPRDOFF.Val16==0)		// End of sprite
						{
							loop=4;		// Halt the quad loop
							break;
						}

						// Draw one horizontal line of the sprite 
						for(vloop=0;vloop<pixel_height;vloop++)
						{
							// Early bailout if the sprite has moved off screen, terminate quad
							if(vsign==1 && voff>=SCREEN_HEIGHT)	break;
							if(vsign==-1 && voff<0)	break;

							// Only allow the draw to take place if the line is visible
							if(voff>=0 && voff<SCREEN_HEIGHT)
							{
								// Work out the horizontal pixel start position, start + tilt
								mHPOSSTRT.Val16+=((int16)mTILTACUM.Val16>>8);
								mTILTACUM.Union8.High=0;
								hoff=(int)((int16)mHPOSSTRT.Val16)-screen_h_start;

					 			// Zero/Force the horizontal scaling accumulator
								if(hsign==1) mHSIZACUM.Val16=mHSIZOFF.Val16; else mHSIZACUM.Val16=0;

								// Take the sign of the first quad (0) as the basic
								// sign, all other quads drawing in the other direction
								// get offset by 1 pixel in the other direction, this
								// fixes the squashed look on the multi-quad sprites.
//								if(hsign==-1 && loop>0) hoff+=hsign;
								if(loop==0)	hquadoff=hsign;
								if(hsign!=hquadoff) hoff+=hsign;

								// Initialise our line
								LineInit(voff);
								onscreen=false;

								// Now render an individual destination line
								while((pixel=LineGetPixel())!=LINE_END)
								{
									// This is allowed to update every pixel
									mHSIZACUM.Val16+=mSPRHSIZ.Val16;
									pixel_width=mHSIZACUM.Union8.High;
									mHSIZACUM.Union8.High=0;

									for(hloop=0;hloop<pixel_width;hloop++)
									{
										// Draw if onscreen but break loop on transition to offscreen
										if(hoff>=0 && hoff<SCREEN_WIDTH)
										{
											ProcessPixel(hoff,pixel);
											onscreen = true;
											everonscreen = true;
										}
										else
										{
											if(onscreen) break;
										}
										hoff+=hsign;
									}
								}
							}
							voff+=vsign;

							// For every destination line we can modify SPRHSIZ & SPRVSIZ & TILTACUM
							if(enable_stretch)
							{
								mSPRHSIZ.Val16+=mSTRETCH.Val16;
//								if(mSPRSYS_VStretch) mSPRVSIZ.Val16+=mSTRETCH.Val16;
							}
							if(enable_tilt)
							{
								// Manipulate the tilt stuff
								mTILTACUM.Val16+=mTILT.Val16;
							}
						}
						// According to the docs this increments per dest line 
						// but only gets set when the source line is read
						if(mSPRSYS_VStretch) mSPRVSIZ.Val16+=mSTRETCH.Val16*pixel_height;

						// Update the line start for our next run thru the loop
						mSPRDLINE.Val16+=mSPRDOFF.Val16;
					}
				}
				else
				{
					// Skip thru data to next quad
					for(;;)
					{
						// Read the start of line offset

						mSPRDOFF.Val16=(uint16)LineInit(0);

						// We dont want to process data so mSPRDLINE is useless to us
						mSPRDLINE.Val16+=mSPRDOFF.Val16;

						// If 1 == next quad, ==0 end of sprite, anyways its END OF LINE

						if(mSPRDOFF.Val16==1) break;	// End of quad
						if(mSPRDOFF.Val16==0)		// End of sprite
						{
							loop=4;		// Halt the quad loop
							break;
						}

					}
				}

				// Increment quadrant and mask to 2 bit value (0-3)
				quadrant++;
				quadrant&=0x03;
			}

			// Write the collision depositary if required

			if(!mSPRCOLL_Collide && !mSPRSYS_NoCollide)
			{
				switch(mSPRCTL0_Type)
				{
					case sprite_xor_shadow:
					case sprite_boundary:
					case sprite_normal:
					case sprite_boundary_shadow:
					case sprite_shadow:
						{
							uint16 coldep=mSCBADR.Val16+mCOLLOFF.Val16;
							RAM_POKE(coldep,(uint8)mCollision);
						}
						break;
					default:
						break;
				}
			}

			if(mEVERON)
			{
				uint16 coldep=mSCBADR.Val16+mCOLLOFF.Val16;
				uint8 coldat=RAM_PEEK(coldep);
				if(!everonscreen) coldat|=0x80; else coldat&=0x7f;
				RAM_POKE(coldep,coldat);
			}
		}

		// Increase sprite number
		sprcount++;

		// Check if we abort after 1st sprite is complete

//		if(mSPRSYS.Read.StopOnCurrent) 
//		{
//			mSPRSYS.Read.Status=0;	// Engine has finished
//			mSPRGO=false;
//			break;
//		}

		// Check sprcount for looping SCB, random large number chosen
		if(sprcount>4096)
		{
			// Stop the system, otherwise we may just come straight back in.....
			gSystemHalt=true;
			// Display warning message
			//gError->Warning("CSusie:PaintSprites(): Single draw sprite limit exceeded (>4096). The SCB is most likely looped back on itself. Reset/Exit is recommended");
			// Signal error to the caller
			return 0;
		}
	}
	while(1);

	// Fudge factor to fix many flickering issues, also the keypress
	// problem with Hard Drivin and the strange pause in Dirty Larry.
	//cycles_used>>=2;
	return cycles_used;
}


INLINE void CSusie::WritePixel(uint32 hoff,uint32 pixel)
{
        const uint16 scr_addr=mLineBaseAddress+(hoff/2);

        uint8 dest=RAM_PEEK(scr_addr);
        if(!(hoff&0x01))
        {
                // Upper nibble screen write
                dest&=0x0f;
                dest|=pixel<<4;
        }
        else
        {
                // Lower nibble screen write
                dest&=0xf0;
                dest|=pixel;
        }
        RAM_POKE(scr_addr,dest);

        // Increment cycle count for the read/modify/write
        cycles_used+=2*SPR_RDWR_CYC;
}

INLINE uint32 CSusie::ReadPixel(uint32 hoff)
{
        const uint16 scr_addr=mLineBaseAddress+(hoff/2);

        uint32 data=RAM_PEEK(scr_addr);
        if(!(hoff&0x01))
        {
                // Upper nibble read
                data>>=4;
        }
        else
        {
                // Lower nibble read
                data&=0x0f;
        }

        // Increment cycle count for the read/modify/write
        cycles_used+=SPR_RDWR_CYC;

        return data;
}

INLINE void CSusie::WriteCollision(uint32 hoff,uint32 pixel)
{
        const uint16 col_addr=mLineCollisionAddress+(hoff/2);

        uint8 dest=RAM_PEEK(col_addr);
        if(!(hoff&0x01))
        {
                // Upper nibble screen write
                dest&=0x0f;
                dest|=pixel<<4;
        }
        else
        {
                // Lower nibble screen write
                dest&=0xf0;
                dest|=pixel;
        }
        RAM_POKE(col_addr,dest);

        // Increment cycle count for the read/modify/write
        cycles_used+=2*SPR_RDWR_CYC;
}

INLINE uint32 CSusie::ReadCollision(uint32 hoff)
{
        const uint16 col_addr=mLineCollisionAddress+(hoff/2);

        uint32 data=RAM_PEEK(col_addr);
        if(!(hoff&0x01))
        {
                // Upper nibble read
                data>>=4;
        }
        else
        {
                // Lower nibble read
                data&=0x0f;
        }

        // Increment cycle count for the read/modify/write
        cycles_used+=SPR_RDWR_CYC;

        return data;
}


INLINE uint32 CSusie::LineGetBits(uint32 bits)
{
        uint32 retval;

        // Sanity, not really needed
        // if(bits>32) return 0;

        // Only return data IF there is enought bits left in the packet

        //if(mLinePacketBitsLeft<bits) return 0;
	if(mLinePacketBitsLeft<=bits) return 0;	// Hardware bug(<= instead of <), apparently

        // Make sure shift reg has enough bits to fulfil the request

        if(mLineShiftRegCount<bits)
        {
                // This assumes data comes into LSB and out of MSB
//              mLineShiftReg&=0x000000ff;      // Has no effect
                mLineShiftReg<<=24;
                mLineShiftReg|=RAM_PEEK(mTMPADR.Val16++)<<16;
                mLineShiftReg|=RAM_PEEK(mTMPADR.Val16++)<<8;
                mLineShiftReg|=RAM_PEEK(mTMPADR.Val16++);

                mLineShiftRegCount+=24;

                // Increment cycle count for the read
                cycles_used+=3*SPR_RDWR_CYC;
        }

        // Extract the return value
        retval=mLineShiftReg>>(mLineShiftRegCount-bits);
        retval&=(1<<bits)-1;

        // Update internal vars;
        mLineShiftRegCount-=bits;
        mLinePacketBitsLeft-=bits;

        return retval;
}



//
// Collision code modified by KW 22/11/98
// Collision buffer cler added if there is no
// apparent collision, I have a gut feeling this
// is the wrong solution to the inv07.com bug but
// it seems to work OK.
//
// Shadow-------------------------------|
// Boundary-Shadow--------------------| |
// Normal---------------------------| | |
// Boundary-----------------------| | | |
// Background-Shadow------------| | | | |
// Background-No Collision----| | | | | |
// Non-Collideable----------| | | | | | |
// Exclusive-or-Shadow----| | | | | | | |
//                        | | | | | | | |
//                        1 1 1 1 0 1 0 1   F is opaque 
//                        0 0 0 0 1 1 0 0   E is collideable 
//                        0 0 1 1 0 0 0 0   0 is opaque and collideable 
//                        1 0 0 0 1 1 1 1   allow collision detect 
//                        1 0 0 1 1 1 1 1   allow coll. buffer access 
//                        1 0 0 0 0 0 0 0   exclusive-or the data 
//

//inline 
void CSusie::ProcessPixel(uint32 hoff,uint32 pixel)
{
	switch(mSPRCTL0_Type)
	{
		// BACKGROUND SHADOW
		// 1   F is opaque 
		// 0   E is collideable 
		// 1   0 is opaque and collideable 
		// 0   allow collision detect 
		// 1   allow coll. buffer access 
		// 0   exclusive-or the data 
		case sprite_background_shadow:
			WritePixel(hoff,pixel);
			if(!mSPRCOLL_Collide && !mSPRSYS_NoCollide && pixel!=0x0e)
			{
				WriteCollision(hoff,mSPRCOLL_Number);
			}
			break;

		// BACKGROUND NOCOLLIDE
		// 1   F is opaque 
		// 0   E is collideable 
		// 1   0 is opaque and collideable 
		// 0   allow collision detect 
		// 0   allow coll. buffer access 
		// 0   exclusive-or the data 
		case sprite_background_noncollide:
			WritePixel(hoff,pixel);
			break;

		// NOCOLLIDE
		// 1   F is opaque 
		// 0   E is collideable 
		// 0   0 is opaque and collideable 
		// 0   allow collision detect 
		// 0   allow coll. buffer access 
		// 0   exclusive-or the data 
		case sprite_noncollide:
			if(pixel!=0x00) WritePixel(hoff,pixel);
			break;

		// BOUNDARY
		// 0   F is opaque 
		// 1   E is collideable 
		// 0   0 is opaque and collideable 
		// 1   allow collision detect 
		// 1   allow coll. buffer access 
		// 0   exclusive-or the data 
		case sprite_boundary:
			if(pixel!=0x00 && pixel!=0x0f)
			{
				WritePixel(hoff,pixel);
			}
			if(pixel!=0x00)
			{
				if(!mSPRCOLL_Collide && !mSPRSYS_NoCollide)
				{
					int collision=ReadCollision(hoff);
					if(collision>mCollision)
					{
						mCollision=collision;
					}
// 01/05/00 V0.7	if(mSPRCOLL_Number>collision)
					{
						WriteCollision(hoff,mSPRCOLL_Number);
					}
				}
			}
			break;

		// NORMAL
		// 1   F is opaque 
		// 1   E is collideable 
		// 0   0 is opaque and collideable 
		// 1   allow collision detect 
		// 1   allow coll. buffer access 
		// 0   exclusive-or the data 
		case sprite_normal:
			if(pixel!=0x00)
			{
				WritePixel(hoff,pixel);
				if(!mSPRCOLL_Collide && !mSPRSYS_NoCollide)
				{
					int collision=ReadCollision(hoff);
					if(collision>mCollision)
					{
						mCollision=collision;
					}
// 01/05/00 V0.7	if(mSPRCOLL_Number>collision)
					{
						WriteCollision(hoff,mSPRCOLL_Number);
					}
				}
			}
			break;

		// BOUNDARY_SHADOW
		// 0   F is opaque 
		// 0   E is collideable 
		// 0   0 is opaque and collideable 
		// 1   allow collision detect 
		// 1   allow coll. buffer access 
		// 0   exclusive-or the data 
		case sprite_boundary_shadow:
			if(pixel!=0x00 && pixel!=0x0e && pixel!=0x0f)
			{
				WritePixel(hoff,pixel);
			}
			if(pixel!=0x00 && pixel!=0x0e)
			{
				if(!mSPRCOLL_Collide && !mSPRSYS_NoCollide)
				{
					int collision=ReadCollision(hoff);
					if(collision>mCollision)
					{
						mCollision=collision;
					}
// 01/05/00 V0.7	if(mSPRCOLL_Number>collision)
					{
						WriteCollision(hoff,mSPRCOLL_Number);
					}
				}
			}
			break;

		// SHADOW
		// 1   F is opaque 
		// 0   E is collideable 
		// 0   0 is opaque and collideable 
		// 1   allow collision detect 
		// 1   allow coll. buffer access 
		// 0   exclusive-or the data 
		case sprite_shadow:
			if(pixel!=0x00)
			{
				WritePixel(hoff,pixel);
			}
			if(pixel!=0x00 && pixel!=0x0e)
			{
				if(!mSPRCOLL_Collide && !mSPRSYS_NoCollide)
				{
					int collision=ReadCollision(hoff);
					if(collision>mCollision)
					{
						mCollision=collision;
					}
// 01/05/00 V0.7	if(mSPRCOLL_Number>collision)
					{
						WriteCollision(hoff,mSPRCOLL_Number);
					}
				}
			}
			break;

		// XOR SHADOW
		// 1   F is opaque 
		// 0   E is collideable 
		// 0   0 is opaque and collideable 
		// 1   allow collision detect 
		// 1   allow coll. buffer access 
		// 1   exclusive-or the data 
		case sprite_xor_shadow:
			if(pixel!=0x00)
			{
				WritePixel(hoff,ReadPixel(hoff)^pixel);
			}
			if(pixel!=0x00 && pixel!=0x0e)
			{
				if(!mSPRCOLL_Collide && !mSPRSYS_NoCollide && pixel!=0x0e)
				{
					int collision=ReadCollision(hoff);
					if(collision>mCollision)
					{
						mCollision=collision;
					}
// 01/05/00 V0.7	if(mSPRCOLL_Number>collision)
					{
						WriteCollision(hoff,mSPRCOLL_Number);
					}
				}
			}
			break;
		default:
//			_asm int 3;
			break;
	}
}

uint32 CSusie::LineInit(uint32 voff)
{

	mLineShiftReg=0;
	mLineShiftRegCount=0;
	mLineRepeatCount=0;
	mLinePixel=0;
	mLineType=line_error;
	mLinePacketBitsLeft=0xffff;

	// Initialise the temporary pointer

	mTMPADR=mSPRDLINE;

	// First read the Offset to the next line

	uint32 offset=LineGetBits(8);

	// Specify the MAXIMUM number of bits in this packet, it
	// can terminate early but can never use more than this
	// without ending the current packet, we count down in LineGetBits()

	mLinePacketBitsLeft=(offset-1)*8;

	// Literals are a special case and get their count set on a line basis
	
	if(mSPRCTL1_Literal)
	{
		mLineType=line_abs_literal;
		mLineRepeatCount=((offset-1)*8)/mSPRCTL0_PixelBits;
		// Why is this necessary, is this compensating for the 1,1 offset bug
//		mLineRepeatCount--;
	}

	// Set the line base address for use in the calls to pixel painting

	if(voff>101)
	{
		//gError->Warning("CSusie::LineInit() Out of bounds (voff)");
		voff=0;
	}

	mLineBaseAddress=mVIDBAS.Val16+(voff*(SCREEN_WIDTH/2));
	mLineCollisionAddress=mCOLLBAS.Val16+(voff*(SCREEN_WIDTH/2));

	// Return the offset to the next line

	return offset;
}

uint32 CSusie::LineGetPixel()
{
	if(!mLineRepeatCount)
	{
		// Normal sprites fetch their counts on a packet basis
		if(mLineType!=line_abs_literal)
		{
			uint32 literal=LineGetBits(1);
			if(literal) mLineType=line_literal; else mLineType=line_packed;
		}

		// Pixel store is empty what should we do
		switch(mLineType)
		{
			case line_abs_literal:
				// This means end of line for us
				mLinePixel=LINE_END;
				return mLinePixel;		// SPEEDUP
				break;
			case line_literal:
				mLineRepeatCount=LineGetBits(4);
				mLineRepeatCount++;
				break;
			case line_packed:
				//
				// From reading in between the lines only a packed line with
				// a zero size i.e 0b00000 as a header is allowable as a packet end
				//
				mLineRepeatCount=LineGetBits(4);
				if(!mLineRepeatCount)
				{
					mLinePixel=LINE_END;
				}
				else
				{
					mLinePixel=mPenIndex[LineGetBits(mSPRCTL0_PixelBits)];
				}
				mLineRepeatCount++;
				break;
			default:
				return 0;
		}
	}

	if(mLinePixel!=LINE_END)
	{
		mLineRepeatCount--;

		switch(mLineType)
		{
			case line_abs_literal:
				mLinePixel=LineGetBits(mSPRCTL0_PixelBits);
				// Check the special case of a zero in the last pixel
				if(!mLineRepeatCount && !mLinePixel)
					mLinePixel=LINE_END;
				else
					mLinePixel=mPenIndex[mLinePixel];
				break;
			case line_literal:
				mLinePixel=mPenIndex[LineGetBits(mSPRCTL0_PixelBits)];
				break;
			case line_packed:
				break;
			default:
				return 0;
		}
	}

	return mLinePixel;
}


void CSusie::Poke(uint32 addr,uint8 data)
{
	switch(addr&0xff)
	{
		case (TMPADRL&0xff):
			mTMPADR.Union8.Low=data;
			mTMPADR.Union8.High=0;
			break;
		case (TMPADRH&0xff):
			mTMPADR.Union8.High=data;
			break;
		case (TILTACUML&0xff):
			mTILTACUM.Union8.Low=data;
			mTILTACUM.Union8.High=0;
			break;
		case (TILTACUMH&0xff):
			mTILTACUM.Union8.High=data;
			break;
		case (HOFFL&0xff):
			mHOFF.Union8.Low=data;
			mHOFF.Union8.High=0;
			break;
		case (HOFFH&0xff):
			mHOFF.Union8.High=data;
			break;
		case (VOFFL&0xff):
			mVOFF.Union8.Low=data;
			mVOFF.Union8.High=0;
			break;
		case (VOFFH&0xff):
			mVOFF.Union8.High=data;
			break;
		case (VIDBASL&0xff):
			mVIDBAS.Union8.Low=data;
			mVIDBAS.Union8.High=0;
			break;
		case (VIDBASH&0xff):
			mVIDBAS.Union8.High=data;
			break;
		case (COLLBASL&0xff):
			mCOLLBAS.Union8.Low=data;
			mCOLLBAS.Union8.High=0;
			break;
		case (COLLBASH&0xff):
			mCOLLBAS.Union8.High=data;
			break;
		case (VIDADRL&0xff):
			mVIDADR.Union8.Low=data;
			mVIDADR.Union8.High=0;
			break;
		case (VIDADRH&0xff):
			mVIDADR.Union8.High=data;
			break;
		case (COLLADRL&0xff):
			mCOLLADR.Union8.Low=data;
			mCOLLADR.Union8.High=0;
			break;
		case (COLLADRH&0xff):
			mCOLLADR.Union8.High=data;
			break;
		case (SCBNEXTL&0xff):
			mSCBNEXT.Union8.Low=data;
			mSCBNEXT.Union8.High=0;
			break;
		case (SCBNEXTH&0xff):
			mSCBNEXT.Union8.High=data;
			break;
		case (SPRDLINEL&0xff):
			mSPRDLINE.Union8.Low=data;
			mSPRDLINE.Union8.High=0;
			break;
		case (SPRDLINEH&0xff):
			mSPRDLINE.Union8.High=data;
			break;
		case (HPOSSTRTL&0xff):
			mHPOSSTRT.Union8.Low=data;
			mHPOSSTRT.Union8.High=0;
			break;
		case (HPOSSTRTH&0xff):
			mHPOSSTRT.Union8.High=data;
			break;
		case (VPOSSTRTL&0xff):
			mVPOSSTRT.Union8.Low=data;
			mVPOSSTRT.Union8.High=0;
			break;
		case (VPOSSTRTH&0xff):
			mVPOSSTRT.Union8.High=data;
			break;
		case (SPRHSIZL&0xff):
			mSPRHSIZ.Union8.Low=data;
			mSPRHSIZ.Union8.High=0;
			break;
		case (SPRHSIZH&0xff):
			mSPRHSIZ.Union8.High=data;
			break;
		case (SPRVSIZL&0xff):
			mSPRVSIZ.Union8.Low=data;
			mSPRVSIZ.Union8.High=0;
			break;
		case (SPRVSIZH&0xff):
			mSPRVSIZ.Union8.High=data;
			break;
		case (STRETCHL&0xff):
			mSTRETCH.Union8.Low=data;
			mSTRETCH.Union8.High=0;
			break;
		case (STRETCHH&0xff):
			mSTRETCH.Union8.High=data;
			break;
		case (TILTL&0xff):
			mTILT.Union8.Low=data;
			mTILT.Union8.High=0;
			break;
		case (TILTH&0xff):
			mTILT.Union8.High=data;
			break;
		case (SPRDOFFL&0xff):
			mSPRDOFF.Union8.Low=data;
			mSPRDOFF.Union8.High=0;
			break;
		case (SPRDOFFH&0xff):
			mSPRDOFF.Union8.High=data;
			break;
		case (SPRVPOSL&0xff):
			mSPRVPOS.Union8.Low=data;
			mSPRVPOS.Union8.High=0;
			break;
		case (SPRVPOSH&0xff):
			mSPRVPOS.Union8.High=data;
			break;
		case (COLLOFFL&0xff):
			mCOLLOFF.Union8.Low=data;
			mCOLLOFF.Union8.High=0;
			break;
		case (COLLOFFH&0xff):
			mCOLLOFF.Union8.High=data;
			break;
		case (VSIZACUML&0xff):
			mVSIZACUM.Union8.Low=data;
			mVSIZACUM.Union8.High=0;
			break;
		case (VSIZACUMH&0xff):
			mVSIZACUM.Union8.High=data;
			break;
		case (HSIZOFFL&0xff):
			mHSIZOFF.Union8.Low=data;
			mHSIZOFF.Union8.High=0;
			break;
		case (HSIZOFFH&0xff):
			mHSIZOFF.Union8.High=data;
			break;
		case (VSIZOFFL&0xff):
			mVSIZOFF.Union8.Low=data;
			mVSIZOFF.Union8.High=0;
			break;
		case (VSIZOFFH&0xff):
			mVSIZOFF.Union8.High=data;
			break;
		case (SCBADRL&0xff):
			mSCBADR.Union8.Low=data;
			mSCBADR.Union8.High=0;
			break;
		case (SCBADRH&0xff):
			mSCBADR.Union8.High=data;
			break;
		case (PROCADRL&0xff):
			mPROCADR.Union8.Low=data;
			mPROCADR.Union8.High=0;
			break;
		case (PROCADRH&0xff):
			mPROCADR.Union8.High=data;
			break;

		case (MATHD&0xff):
			mMATHABCD.Bytes.D=data;
//			mMATHABCD.Bytes.C=0;
			// The hardware manual says that the sign shouldnt change
			// but if I dont do this then stun runner will hang as it
			// does the init in the wrong order and if the previous
			// calc left a zero there then we'll get a sign error
			Poke(MATHC,0);
			break;
		case (MATHC&0xff):
			mMATHABCD.Bytes.C=data;
			// Perform sign conversion if required
			if(mSPRSYS_SignedMath)
			{
				// Account for the math bug that 0x8000 is +ve & 0x0000 is -ve by subracting 1
				if((mMATHABCD.Words.CD-1)&0x8000)
				{
					uint16 conv;
					conv=mMATHABCD.Words.CD^0xffff;
					conv++;
					mMATHCD_sign=-1;
					mMATHABCD.Words.CD=conv;
				}
				else
				{
					mMATHCD_sign=1;
				}
			}
			break;
		case (MATHB&0xff):
			mMATHABCD.Bytes.B=data;
			mMATHABCD.Bytes.A=0;
			break;
		case (MATHA&0xff):
			mMATHABCD.Bytes.A=data;
			// Perform sign conversion if required
			if(mSPRSYS_SignedMath)
			{
				// Account for the math bug that 0x8000 is +ve & 0x0000 is -ve by subracting 1
				if((mMATHABCD.Words.AB-1)&0x8000)
				{
					uint16 conv;
					conv=mMATHABCD.Words.AB^0xffff;
					conv++;
					mMATHAB_sign=-1;
					mMATHABCD.Words.AB=conv;
				}
				else
				{
					mMATHAB_sign=1;
				}
			}
			DoMathMultiply();
			break;

		case (MATHP&0xff):
			mMATHNP.Bytes.P=data;
			mMATHNP.Bytes.N=0;
			break;
		case (MATHN&0xff):
			mMATHNP.Bytes.N=data;
			break;

		case (MATHH&0xff):
			mMATHEFGH.Bytes.H=data;
			mMATHEFGH.Bytes.G=0;
			break;
		case (MATHG&0xff):
			mMATHEFGH.Bytes.G=data;
			break;
		case (MATHF&0xff):
			mMATHEFGH.Bytes.F=data;
			mMATHEFGH.Bytes.E=0;
			break;
		case (MATHE&0xff):
			mMATHEFGH.Bytes.E=data;
			DoMathDivide();
			break;

		case (MATHM&0xff):
			mMATHJKLM.Bytes.M=data;
			mMATHJKLM.Bytes.L=0;
			mSPRSYS_Mathbit=false;
			break;
		case (MATHL&0xff):
			mMATHJKLM.Bytes.L=data;
			break;
		case (MATHK&0xff):
			mMATHJKLM.Bytes.K=data;
			mMATHJKLM.Bytes.J=0;
			break;
		case (MATHJ&0xff):
			mMATHJKLM.Bytes.J=data;
			break;

		case (SPRCTL0&0xff):
			mSPRCTL0_Type=data&0x0007;
			mSPRCTL0_Vflip=data&0x0010;
			mSPRCTL0_Hflip=data&0x0020;
			mSPRCTL0_PixelBits=((data&0x00c0)>>6)+1;
			break;
		case (SPRCTL1&0xff):
			mSPRCTL1_StartLeft=data&0x0001;
			mSPRCTL1_StartUp=data&0x0002;
			mSPRCTL1_SkipSprite=data&0x0004;
			mSPRCTL1_ReloadPalette=data&0x0008;
			mSPRCTL1_ReloadDepth=(data&0x0030)>>4;
			mSPRCTL1_Sizing=data&0x0040;	
			mSPRCTL1_Literal=data&0x0080;
			break;
		case (SPRCOLL&0xff):
			mSPRCOLL_Number=data&0x000f;
			mSPRCOLL_Collide=data&0x0020;
			break;
		case (SPRINIT&0xff):
			mSPRINIT.Byte=data;
			break;
		case (SUZYBUSEN&0xff):
			mSUZYBUSEN=data&0x01;
			break;
		case (SPRGO&0xff):
			mSPRGO=data&0x01;
			mEVERON=data&0x04;
			break;
		case (SPRSYS&0xff):
			mSPRSYS_StopOnCurrent=data&0x0002;
			if(data&0x0004) mSPRSYS_UnsafeAccess=0;
			mSPRSYS_LeftHand=data&0x0008;
			mSPRSYS_VStretch=data&0x0010;
			mSPRSYS_NoCollide=data&0x0020;
			mSPRSYS_Accumulate=data&0x0040;
			mSPRSYS_SignedMath=data&0x0080;
			break;

// Cartridge writing ports

		case (RCART0&0xff):
			mSystem.Poke_CARTB0(data);
			break;
		case (RCART1&0xff):
			mSystem.Poke_CARTB1(data);
			break;
			
// These are not so important, so lets ignore them for the moment

		case (LEDS&0xff):
		case (PPORTSTAT&0xff):
		case (PPORTDATA&0xff):
		case (HOWIE&0xff):
			break;

// Errors on read only register accesses

		case (SUZYHREV&0xff):
		case (JOYSTICK&0xff):
		case (SWITCHES&0xff):
			break;

// Errors on illegal location accesses

		default:
			break;
	}
}

uint8 CSusie::Peek(uint32 addr)
{
	uint8	retval=0;

	switch(addr&0xff)
	{
		case (TMPADRL&0xff):
			retval=mTMPADR.Union8.Low;
			return retval;
			break;
		case (TMPADRH&0xff):
			retval=mTMPADR.Union8.High;
			return retval;
			break;
		case (TILTACUML&0xff):
			retval=mTILTACUM.Union8.Low;
			return retval;
			break;
		case (TILTACUMH&0xff):
			retval=mTILTACUM.Union8.High;
			return retval;
			break;
		case (HOFFL&0xff):
			retval=mHOFF.Union8.Low;
			return retval;
			break;
		case (HOFFH&0xff):
			retval=mHOFF.Union8.High;
			return retval;
			break;
		case (VOFFL&0xff):
			retval=mVOFF.Union8.Low;
			return retval;
			break;
		case (VOFFH&0xff):
			retval=mVOFF.Union8.High;
			return retval;
			break;
		case (VIDBASL&0xff):
			retval=mVIDBAS.Union8.Low;
			return retval;
			break;
		case (VIDBASH&0xff):
			retval=mVIDBAS.Union8.High;
			return retval;
			break;
		case (COLLBASL&0xff):
			retval=mCOLLBAS.Union8.Low;
			return retval;
			break;
		case (COLLBASH&0xff):
			retval=mCOLLBAS.Union8.High;
			return retval;
			break;
		case (VIDADRL&0xff):
			retval=mVIDADR.Union8.Low;
			return retval;
			break;
		case (VIDADRH&0xff):
			retval=mVIDADR.Union8.High;
			return retval;
			break;
		case (COLLADRL&0xff):
			retval=mCOLLADR.Union8.Low;
			return retval;
			break;
		case (COLLADRH&0xff):
			retval=mCOLLADR.Union8.High;
			return retval;
			break;
		case (SCBNEXTL&0xff):
			retval=mSCBNEXT.Union8.Low;
			return retval;
			break;
		case (SCBNEXTH&0xff):
			retval=mSCBNEXT.Union8.High;
			return retval;
			break;
		case (SPRDLINEL&0xff):
			retval=mSPRDLINE.Union8.Low;
			return retval;
			break;
		case (SPRDLINEH&0xff):
			retval=mSPRDLINE.Union8.High;
			return retval;
			break;
		case (HPOSSTRTL&0xff):
			retval=mHPOSSTRT.Union8.Low;
			return retval;
			break;
		case (HPOSSTRTH&0xff):
			retval=mHPOSSTRT.Union8.High;
			return retval;
			break;
		case (VPOSSTRTL&0xff):
			retval=mVPOSSTRT.Union8.Low;
			return retval;
			break;
		case (VPOSSTRTH&0xff):
			retval=mVPOSSTRT.Union8.High;
			return retval;
			break;
		case (SPRHSIZL&0xff):
			retval=mSPRHSIZ.Union8.Low;
			return retval;
			break;
		case (SPRHSIZH&0xff):
			retval=mSPRHSIZ.Union8.High;
			return retval;
			break;
		case (SPRVSIZL&0xff):
			retval=mSPRVSIZ.Union8.Low;
			return retval;
			break;
		case (SPRVSIZH&0xff):
			retval=mSPRVSIZ.Union8.High;
			return retval;
			break;
		case (STRETCHL&0xff):
			retval=mSTRETCH.Union8.Low;
			return retval;
			break;
		case (STRETCHH&0xff):
			retval=mSTRETCH.Union8.High;
			return retval;
			break;
		case (TILTL&0xff):
			retval=mTILT.Union8.Low;
			return retval;
			break;
		case (TILTH&0xff):
			retval=mTILT.Union8.High;
			return retval;
			break;
		case (SPRDOFFL&0xff):
			retval=mSPRDOFF.Union8.Low;
			return retval;
			break;
		case (SPRDOFFH&0xff):
			retval=mSPRDOFF.Union8.High;
			return retval;
			break;
		case (SPRVPOSL&0xff):
			retval=mSPRVPOS.Union8.Low;
			return retval;
			break;
		case (SPRVPOSH&0xff):
			retval=mSPRVPOS.Union8.High;
			return retval;
			break;
		case (COLLOFFL&0xff):
			retval=mCOLLOFF.Union8.Low;
			return retval;
			break;
		case (COLLOFFH&0xff):
			retval=mCOLLOFF.Union8.High;
			return retval;
			break;
		case (VSIZACUML&0xff):
			retval=mVSIZACUM.Union8.Low;
			return retval;
			break;
		case (VSIZACUMH&0xff):
			retval=mVSIZACUM.Union8.High;
			return retval;
			break;
		case (HSIZOFFL&0xff):
			retval=mHSIZOFF.Union8.Low;
			return retval;
			break;
		case (HSIZOFFH&0xff):
			retval=mHSIZOFF.Union8.High;
			return retval;
			break;
		case (VSIZOFFL&0xff):
			retval=mVSIZOFF.Union8.Low;
			return retval;
			break;
		case (VSIZOFFH&0xff):
			retval=mVSIZOFF.Union8.High;
			return retval;
			break;
		case (SCBADRL&0xff):
			retval=mSCBADR.Union8.Low;
			return retval;
			break;
		case (SCBADRH&0xff):
			retval=mSCBADR.Union8.High;
			return retval;
			break;
		case (PROCADRL&0xff):
			retval=mPROCADR.Union8.Low;
			return retval;
			break;
		case (PROCADRH&0xff):
			retval=mPROCADR.Union8.High;
			return retval;
			break;

		case (MATHD&0xff):
			retval=mMATHABCD.Bytes.D;
			return retval;
			break;
		case (MATHC&0xff):
			retval=mMATHABCD.Bytes.C;
			return retval;
			break;
		case (MATHB&0xff):
			retval=mMATHABCD.Bytes.B;
			return retval;
			break;
		case (MATHA&0xff):
			retval=mMATHABCD.Bytes.A;
			return retval;
			break;

		case (MATHP&0xff):
			retval=mMATHNP.Bytes.P;
			return retval;
			break;
		case (MATHN&0xff):
			retval=mMATHNP.Bytes.N;
			return retval;
			break;

		case (MATHH&0xff):
			retval=mMATHEFGH.Bytes.H;
			return retval;
			break;
		case (MATHG&0xff):
			retval=mMATHEFGH.Bytes.G;
			return retval;
			break;
		case (MATHF&0xff):
			retval=mMATHEFGH.Bytes.F;
			return retval;
			break;
		case (MATHE&0xff):
			retval=mMATHEFGH.Bytes.E;
			return retval;
			break;

		case (MATHM&0xff):
			retval=mMATHJKLM.Bytes.M;
			return retval;
			break;
		case (MATHL&0xff):
			retval=mMATHJKLM.Bytes.L;
			return retval;
			break;
		case (MATHK&0xff):
			retval=mMATHJKLM.Bytes.K;
			return retval;
			break;
		case (MATHJ&0xff):
			retval=mMATHJKLM.Bytes.J;
			return retval;
			break;

		case (SUZYHREV&0xff):
			retval=0x01;
			return retval;

		case (SPRSYS&0xff):
			retval=0x0000;
			//	retval+=(mSPRSYS_Status)?0x0001:0x0000;
			retval+= (gSuzieDoneTime)?0x0001:0x0000;
			retval+=(mSPRSYS_StopOnCurrent)?0x0002:0x0000;
			retval+=(mSPRSYS_UnsafeAccess)?0x0004:0x0000;
			retval+=(mSPRSYS_LeftHand)?0x0008:0x0000;
			retval+=(mSPRSYS_VStretch)?0x0010:0x0000;
			retval+=(mSPRSYS_LastCarry)?0x0020:0x0000;
			retval+=(mSPRSYS_Mathbit)?0x0040:0x0000;
			retval+=(mSPRSYS_MathInProgress)?0x0080:0x0000;
			return retval;

		case (JOYSTICK&0xff):
			if(mSPRSYS_LeftHand)
			{
				retval= mJOYSTICK.Byte;
			}
			else
			{
				TJOYSTICK Modified=mJOYSTICK;
				Modified.Bits.Left=mJOYSTICK.Bits.Right;
				Modified.Bits.Right=mJOYSTICK.Bits.Left;
				Modified.Bits.Down=mJOYSTICK.Bits.Up;
				Modified.Bits.Up=mJOYSTICK.Bits.Down;
				retval= Modified.Byte;
			}
			return retval;
			break;


		case (SWITCHES&0xff):
			retval=mSWITCHES.Byte;
			return retval;

// Cartridge reading ports

		case (RCART0&0xff):
			retval=mSystem.Peek_CARTB0();
			return retval;
			break;
		case (RCART1&0xff):
			retval=mSystem.Peek_CARTB1();
			return retval;
			break;

// These are no so important so lets ignore them for the moment

		case (LEDS&0xff):
		case (PPORTSTAT&0xff):
		case (PPORTDATA&0xff):
		case (HOWIE&0xff):
			break;

// Errors on write only register accesses

		case (SPRCTL0&0xff):
		case (SPRCTL1&0xff):
		case (SPRCOLL&0xff):
		case (SPRINIT&0xff):
		case (SUZYBUSEN&0xff):
		case (SPRGO&0xff):
			break;
		
// Errors on illegal location accesses

		default:
			break;
	}

	return 0xff;
}

int CSusie::StateAction(StateMem *sm, int load, int data_only)
{
 SFORMAT SuzieRegs[] =
 {
	SFVARN(mTMPADR.Val16, "mTMPADR"),
        SFVARN(mTILTACUM.Val16, "mTILTACUM"),
        SFVARN(mHOFF.Val16, "mHOFF"),
        SFVARN(mVOFF.Val16, "mVOFF"),
        SFVARN(mVIDBAS.Val16, "mVIDBAS"),
        SFVARN(mCOLLBAS.Val16, "mCOLLBAS"),
        SFVARN(mVIDADR.Val16, "mVIDADR"),
        SFVARN(mCOLLADR.Val16, "mCOLLADR"),
        SFVARN(mSCBNEXT.Val16, "mSCBNEXT"),
        SFVARN(mSPRDLINE.Val16, "mSPRDLINE"),
        SFVARN(mHPOSSTRT.Val16, "mHPOSSTRT"),
        SFVARN(mVPOSSTRT.Val16, "mVPOSSTRT"),
        SFVARN(mSPRHSIZ.Val16, "mSPRHSIZ"),
        SFVARN(mSPRVSIZ.Val16, "mSPRVSIZ"),
        SFVARN(mSTRETCH.Val16, "mSTRETCH"),
        SFVARN(mTILT.Val16, "mTILT"),
        SFVARN(mSPRDOFF.Val16, "mSPRDOFF"),
        SFVARN(mSPRVPOS.Val16, "mSPRVPOS"),
        SFVARN(mCOLLOFF.Val16, "mCOLLOFF"),
        SFVARN(mVSIZACUM.Val16, "mVSIZACUM"),
        SFVARN(mHSIZACUM.Val16, "mHSIZACUM"),
        SFVARN(mHSIZOFF.Val16, "mHSIZOFF"),
        SFVARN(mVSIZOFF.Val16, "mVSIZOFF"),
        SFVARN(mSCBADR.Val16, "mSCBADR"),
        SFVARN(mPROCADR.Val16, "mPROCADR"),

        SFVARN(mMATHABCD.Long, "mMATHABCD"),
        SFVARN(mMATHEFGH.Long, "mMATHEFGH"),
        SFVARN(mMATHJKLM.Long, "mMATHJKLM"),
        SFVARN(mMATHNP.Long, "mMATHNP"),

        SFVAR(mMATHAB_sign),
        SFVAR(mMATHCD_sign),
        SFVAR(mMATHEFGH_sign),

        SFVAR(mSPRCTL0_Type),
        SFVAR(mSPRCTL0_Vflip),
        SFVAR(mSPRCTL0_Hflip),
        SFVAR(mSPRCTL0_PixelBits),
        SFVAR(mSPRCTL1_StartLeft),
        SFVAR(mSPRCTL1_StartUp),
        SFVAR(mSPRCTL1_SkipSprite),
        SFVAR(mSPRCTL1_ReloadPalette),
        SFVAR(mSPRCTL1_ReloadDepth),
        SFVAR(mSPRCTL1_Sizing),
        SFVAR(mSPRCTL1_Literal),

        SFVAR(mSPRCOLL_Number),
        SFVAR(mSPRCOLL_Collide),

        SFVAR(mSPRSYS_StopOnCurrent),
        SFVAR(mSPRSYS_LeftHand),
        SFVAR(mSPRSYS_VStretch),
        SFVAR(mSPRSYS_NoCollide),
        SFVAR(mSPRSYS_Accumulate),
        SFVAR(mSPRSYS_SignedMath),
        SFVAR(mSPRSYS_Status),
        SFVAR(mSPRSYS_UnsafeAccess),
        SFVAR(mSPRSYS_LastCarry),
        SFVAR(mSPRSYS_Mathbit),
        SFVAR(mSPRSYS_MathInProgress),

        SFVAR(mSUZYBUSEN),

        SFVARN(mSPRINIT.Byte, "mSPRINIT"),
        SFVAR(mSPRGO),
        SFVAR(mEVERON),

        SFARRAYN(mPenIndex, 16, "mPenIndex"),

        SFVAR(mLineType),
        SFVAR(mLineShiftRegCount),
        SFVAR(mLineShiftReg),
        SFVAR(mLineRepeatCount),
        SFVAR(mLinePixel),
        SFVAR(mLinePacketBitsLeft),

        SFVAR(mCollision),

        SFVAR(mLineBaseAddress),
        SFVAR(mLineCollisionAddress),

        SFVARN(mJOYSTICK.Byte, "mJOYSTICK"),
        SFVARN(mSWITCHES.Byte, "mSWITCHES"),
	SFVAR(hquadoff),
	SFVAR(vquadoff),
	SFEND
 };

 int ret = MDFNSS_StateAction(sm, load, data_only, SuzieRegs, "SUZY", false);

 return(ret);
}
