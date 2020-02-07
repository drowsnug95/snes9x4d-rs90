#include "scaler.h"

#define AVERAGE(z, x) ((((z) & 0xF7DEF7DE) >> 1) + (((x) & 0xF7DEF7DE) >> 1))
#define AVERAGEHI(AB) ((((AB) & 0xF7DE0000) >> 1) + (((AB) & 0xF7DE) << 15))
#define AVERAGELO(CD) ((((CD) & 0xF7DE) >> 1) + (((CD) & 0xF7DE0000) >> 17))

#define RSHIFT(X) (((X) & 0xF7DEF7DE) >>1)

/*convert 208px to 160px by drowsnug */
void downscale_208to160(uint32_t* __restrict__ dst, uint32_t* __restrict__ src,int width)
{
    uint16_t y=8;
    uint32_t* __restrict__ buffer_mem;
    
    const uint16_t ix=1, iy=4;

    for(int H=0;H<53;H+=1)
    {
	    buffer_mem = &src[y<<7];
        uint16_t x = 4;
        for(int W=0;W<120;W++) 
        {
            uint32_t a,b,c,d;
            a = RSHIFT(buffer_mem[x]);
            b = RSHIFT(buffer_mem[x + 128]);
            c = RSHIFT(buffer_mem[x + 128 * 2]);
            d = RSHIFT(buffer_mem[x + 128 * 3]);
            
            *dst = a +  RSHIFT(a + b);
	        *(dst+120) = b + c;
	        *(dst+120*2) = d + RSHIFT(c + d);
 	        dst++;
            x += ix;
        }
        dst += 120*2;
        y += iy;
    }
}


/*convert 224px to 160px by drowsnug */
void downscale_224to160(uint32_t* __restrict__ dst,uint32_t* __restrict__ src, int width)
{
    uint16_t y=4;
    uint32_t* __restrict__ buffer_mem;
    
    const uint16_t ix=1, iy=7;
    
    for(int H=0;H<32;H+=1)
    {
	    buffer_mem = &src[y<<7];
        uint16_t x = 4;
        for(int W=0;W<120;W++) 
        {
            uint32_t a,b,c,d,e,f,g;
            a = RSHIFT(buffer_mem[x]);
            b = RSHIFT(buffer_mem[x+128]);
            c = RSHIFT(buffer_mem[x+128*2]);
            d = RSHIFT(buffer_mem[x+128*3]);
            e = RSHIFT(buffer_mem[x+128*4]);
            f = RSHIFT(buffer_mem[x+128*5]);
            g = RSHIFT(buffer_mem[x+128*6]);          

            *dst =  a +  RSHIFT(a + b);
	        *(dst+120) = b + c;
	        *(dst+120*2) = d + RSHIFT(d + RSHIFT(c + e));
	        *(dst+120*3) = e + f;
	        *(dst+120*4) = g + RSHIFT(f + g);
            dst++;
            x += ix;
        }
        dst += 120*4;
        y += iy;
    }
}



/*
    Upscale 256x224 -> 320x240

    Horizontal upscale:
        320/256=1.25  --  do some horizontal interpolation
        8p -> 10p
        4dw -> 5dw

        coarse interpolation:
        [ab][cd][ef][gh] -> [ab][(bc)c][de][f(fg)][gh]

        fine interpolation
        [ab][cd][ef][gh] -> [a(0.25a+0.75b)][(0.5b+0.5c)(0.75c+0.25d)][de][(0.25e+0.75f)(0.5f+0.5g)][(0.75g+0.25h)h]

    Vertical upscale:
        Bresenham algo with simple interpolation

    Parameters:
        uint32_t *dst - pointer to 320x240x16bpp buffer
        uint32_t *src - pointer to 256x192x16bpp buffer
*/

void upscale_256x224_to_320x240(uint32_t *dst, uint32_t *src, int width)
{
    int midh = 240 / 2;
    int Eh = 0;
    int source = 0;
    int dh = 0;
    int y, x;

    for (y = 0; y < 240; y++)
    {
        source = dh * width / 2;

        for (x = 0; x < 320/10; x++)
        {
            register uint32_t ab, cd, ef, gh;

            __builtin_prefetch(dst + 4, 1);
            __builtin_prefetch(src + source + 4, 0);

            ab = src[source] & 0xF7DEF7DE;
            cd = src[source + 1] & 0xF7DEF7DE;
            ef = src[source + 2] & 0xF7DEF7DE;
            gh = src[source + 3] & 0xF7DEF7DE;

            if(Eh >= midh) {
                ab = AVERAGE(ab, src[source + width/2]) & 0xF7DEF7DE; // to prevent overflow
                cd = AVERAGE(cd, src[source + width/2 + 1]) & 0xF7DEF7DE; // to prevent overflow
                ef = AVERAGE(ef, src[source + width/2 + 2]) & 0xF7DEF7DE; // to prevent overflow
                gh = AVERAGE(gh, src[source + width/2 + 3]) & 0xF7DEF7DE; // to prevent overflow
            }

            *dst++ = ab;
            *dst++  = ((ab >> 17) + ((cd & 0xFFFF) >> 1)) + (cd << 16);
            *dst++  = (cd >> 16) + (ef << 16);
            *dst++  = (ef >> 16) + (((ef & 0xFFFF0000) >> 1) + ((gh & 0xFFFF) << 15));
            *dst++  = gh;

            source += 4;

        }
        Eh += 224; if(Eh >= 240) { Eh -= 240; dh++; }
    }
}

/*
    Upscale 256x224 -> 384x240 (for 400x240)

    Horizontal interpolation
        384/256=1.5
        4p -> 6p
        2dw -> 3dw

        for each line: 4 pixels => 6 pixels (*1.5) (64 blocks)
        [ab][cd] => [a(ab)][bc][(cd)d]

    Vertical upscale:
        Bresenham algo with simple interpolation

    Parameters:
        uint32_t *dst - pointer to 400x240x16bpp buffer
        uint32_t *src - pointer to 256x192x16bpp buffer
        pitch correction is made
*/

void upscale_256x224_to_384x240_for_400x240(uint32_t *dst, uint32_t *src, int width)
{
    int midh = 240 / 2;
    int Eh = 0;
    int source = 0;
    int dh = 0;
    int y, x;

    dst += (400 - 384) / 4;

    for (y = 0; y < 240; y++)
    {
        source = dh * width / 2;

        for (x = 0; x < 384/6; x++)
        {
            register uint32_t ab, cd;

            __builtin_prefetch(dst + 4, 1);
            __builtin_prefetch(src + source + 4, 0);

            ab = src[source] & 0xF7DEF7DE;
            cd = src[source + 1] & 0xF7DEF7DE;

            if(Eh >= midh) {
                ab = AVERAGE(ab, src[source + width/2]) & 0xF7DEF7DE; // to prevent overflow
                cd = AVERAGE(cd, src[source + width/2 + 1]) & 0xF7DEF7DE; // to prevent overflow
            }

            *dst++ = (ab & 0xFFFF) + AVERAGEHI(ab);
            *dst++ = (ab >> 16) + ((cd & 0xFFFF) << 16);
            *dst++ = (cd & 0xFFFF0000) + AVERAGELO(cd);

            source += 2;

        }
        dst += (400 - 384) / 2; 
        Eh += 224; if(Eh >= 240) { Eh -= 240; dh++; }
    }
}

/*
    Upscale 256x224 -> 384x272 (for 480x240)

    Horizontal interpolation
        384/256=1.5
        4p -> 6p
        2dw -> 3dw

        for each line: 4 pixels => 6 pixels (*1.5) (64 blocks)
        [ab][cd] => [a(ab)][bc][(cd)d]

    Vertical upscale:
        Bresenham algo with simple interpolation

    Parameters:
        uint32_t *dst - pointer to 480x272x16bpp buffer
        uint32_t *src - pointer to 256x192x16bpp buffer
        pitch correction is made
*/

void upscale_256x224_to_384x272_for_480x272(uint32_t *dst, uint32_t *src, int width)
{
    int midh = 272 / 2;
    int Eh = 0;
    int source = 0;
    int dh = 0;
    int y, x;

    dst += (480 - 384) / 4;

    for (y = 0; y < 272; y++)
    {
        source = dh * width / 2;

        for (x = 0; x < 384/6; x++)
        {
            register uint32_t ab, cd;

            __builtin_prefetch(dst + 4, 1);
            __builtin_prefetch(src + source + 4, 0);

            ab = src[source] & 0xF7DEF7DE;
            cd = src[source + 1] & 0xF7DEF7DE;

            if(Eh >= midh) {
                ab = AVERAGE(ab, src[source + width/2]) & 0xF7DEF7DE; // to prevent overflow
                cd = AVERAGE(cd, src[source + width/2 + 1]) & 0xF7DEF7DE; // to prevent overflow
            }

            *dst++ = (ab & 0xFFFF) + AVERAGEHI(ab);
            *dst++ = (ab >> 16) + ((cd & 0xFFFF) << 16);
            *dst++ = (cd & 0xFFFF0000) + AVERAGELO(cd);

            source += 2;

        }
        dst += (480 - 384) / 2; 
        Eh += 224; if(Eh >= 272) { Eh -= 272; dh++; }
    }
}
 