#include "effects.h"
#include "init.h"
#include "lpd8806.h"
#include "timer.h"

#define DELAY_SINGLECOLOR_KITT_MS 20
#define DELAY_SINGLECOLOR_WAVE_MS 20
#define DELAY_SINGLECOLOR_ALL_MS 100
#define DELAY_COLORCYCLE_RAINBOW_MS 0
#define DELAY_COLORCYCLE_PONG_MS 30
#define DELAY_COLORCYCLE_ALL_MS 30
#define DELAY_COLORCYCLE_FILL_REMOVE_EVERY 15
#define DELAY_COLORCYCLE_WAVE_MS 20

// Precalculated sine-table from 0 to 127, in correct stripe-length 
const uint8_t sinTable[STRIPE_LENGTH] = { 63, 43, 24, 10, 2, 0, 5, 17, 33, 53, 74, 94, 110, 122, 127, 125, 117, 103, 84, 63 }; 
volatile uint8_t wave_offset = 0;


// In part adapted from the LPD8806-Arduino library by adafruit (https://github.com/adafruit/LPD8806)

/* Helper functions */

// Convert separate R,G,B into combined 32-bit GRB color:
uint32_t Color(uint8_t r, uint8_t g, uint8_t b) 
{
  return ((uint32_t)r << 16) |
         ((uint32_t)g <<  8) |
                    b;
}

//Input a value 0 to 384 to get a color value.
//The colours are a transition r - g - b - back to r

uint32_t Wheel(uint16_t WheelPos)
{
  uint8_t r = 0, g = 0, b = 0;
  switch(WheelPos / 128)
  {
    case 0:
      r = 127 - WheelPos % 128; // red down
      g = WheelPos % 128;       // green up
      b = 0;                    // blue off
      break;
    case 1:
      g = 127 - WheelPos % 128; // green down
      b = WheelPos % 128;       // blue up
      r = 0;                    // red off
      break;
    case 2:
      b = 127 - WheelPos % 128; // blue down
      r = WheelPos % 128;       // red up
      g = 0;                    // green off
      break;
  }
  return(Color(r, g, b));
}

void low_battery_lights(uint8_t r, uint8_t g, uint8_t b)
{
	while (true)
	{
		for (uint8_t i = 0; i < STRIPE_LENGTH; ++i)
		{
			lpd8806_set_pixel(i, 0, 0, 0);
		}
		lpd8806_set_pixel(0, r, g, b);
		lpd8806_set_pixel(STRIPE_LENGTH - 1, r, g, b);
		lpd8806_set_pixel(STRIPE_LENGTH >> 1, r, g, b);
		lpd8806_update_strip();
	}
}

void colorcycle_pong(uint16_t duration) {
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	uint16_t colorWheelIndex = 0;
	uint32_t color = 0;
	uint8_t ballIndex = 2;
	int8_t ballDirection = 1;
	
	while (elapsed_seconds() - start < duration) {
		color = Wheel(colorWheelIndex);
		
		for (uint8_t i = 0; i < STRIPE_LENGTH; ++i) {
			if (i < 2) {
				lpd8806_set_pixel_rgb(i, color);
			} else if (i > STRIPE_LENGTH - 3) {
				lpd8806_set_pixel_rgb(i, color);
			} else if (i == ballIndex) {
				lpd8806_set_pixel_rgb(i, Wheel((colorWheelIndex + 60) % 384));
			} else {
				lpd8806_set_pixel(i, 0, 0, 0);
			}
		}
		lpd8806_update_strip();
		ballIndex = ballIndex + ballDirection;
		
		if (ballIndex == 2) {
			ballDirection = 1;
			} else if (ballIndex == STRIPE_LENGTH - 3) {
			ballDirection = -1;
		}
		colorWheelIndex++;
		colorWheelIndex = colorWheelIndex % 384;
		_delay_ms(DELAY_COLORCYCLE_PONG_MS);
	}
}

void colorcycle_rainbow(uint16_t duration) 
{
	uint16_t i, j;

	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	while (elapsed_seconds() - start < duration)
	{
		for (j=0; j < 384 * 5; j++) 
		{     
			// 5 cycles of all 384 colors in the wheel
			for (i = 0; i < STRIPE_LENGTH; i++) 
			{
				// tricky math! we use each pixel as a fraction of the full 384-color
				// wheel (thats the i / strip.numPixels() part)
				// Then add in j which makes the colors go around per pixel
				// the % 384 is to make the wheel cycle around
				lpd8806_set_pixel_rgb(STRIPE_LENGTH - i - 1, Wheel(((i * 384 / STRIPE_LENGTH) + j) % 384));
			}
			lpd8806_update_strip();
			#if DELAY_RAINBOWCYCLE_MS > 0
			_delay_ms(DELAY_RAINBOWCYCLE_MS);
			#endif
			if (elapsed_seconds() - start >= duration) 
			{
				break;
			}
		}
	}		
}

void colorcycle_fillremove(uint16_t duration)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	uint16_t outercyclepos = 0;
	uint16_t barfillcyclepos = 0;
	int8_t curbarfillpos = 0;
	int8_t barsfilled = 0;
	int8_t barfilldir = 1;
	
	while (elapsed_seconds() - start < duration)
	{
		for (uint8_t i = 0; i < STRIPE_LENGTH; ++i) {
			if (i > STRIPE_LENGTH - 3) {
				lpd8806_set_pixel_rgb(i, Wheel(outercyclepos));
			} else if (i == curbarfillpos || i > STRIPE_LENGTH - 3 - barsfilled) {
				lpd8806_set_pixel_rgb(i, Wheel(barfillcyclepos));
			} else {
				lpd8806_set_pixel(i, 0, 0, 0);
			}
		}
		lpd8806_update_strip();
		if (outercyclepos % DELAY_COLORCYCLE_FILL_REMOVE_EVERY == 0) {
			if (barfilldir == 1 && curbarfillpos == STRIPE_LENGTH - 3 - barsfilled) {
				barsfilled += 1;
				curbarfillpos = 0;
				if (barsfilled == STRIPE_LENGTH - 2) {
					barfilldir = -1;
					curbarfillpos = STRIPE_LENGTH - 2 - barsfilled;
				}
			} else if (curbarfillpos == -1) {
				barsfilled -= 1;
				curbarfillpos = STRIPE_LENGTH - 3 - barsfilled;
				if (barsfilled == -1) {
					barfilldir = +1;
					curbarfillpos = 0;
					barsfilled = 0;
					barfillcyclepos += 61; // Prime to guarantee lots of different colors
					barfillcyclepos %= 384;
				}
			} else {
				curbarfillpos += barfilldir;
			}
		}
		outercyclepos += 1;
//		_delay_ms(100);
		outercyclepos %= 384;
	}
}

void colorcycle_all(uint16_t duration)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	uint16_t cyclePos = 0;
	
	while (elapsed_seconds() - start < duration) {
		for (uint8_t i = 0; i < STRIPE_LENGTH; ++i) {
			lpd8806_set_pixel_rgb(i, Wheel(cyclePos));
		}
		cyclePos += 1;
		cyclePos %= 384;
		lpd8806_update_strip();
		_delay_ms(DELAY_COLORCYCLE_ALL_MS);
	}
}

void colorcycle_wave(uint16_t duration)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	uint16_t colorcylePos = 0;
	uint8_t cyclered = 0x7F, cyclegreen = 0x7F, cycleblue = 0x7F;
	
	uint32_t color;
	while (elapsed_seconds() - start < duration) {
		for(uint8_t i = 0; i < STRIPE_LENGTH; ++i)
		{
			uint8_t sinval = sinTable[(wave_offset + i) % STRIPE_LENGTH];
			uint16_t r = cyclered * sinval;
			uint16_t g = cyclegreen * sinval;
			uint16_t b = cycleblue * sinval;
			lpd8806_set_pixel((STRIPE_LENGTH - i - 1), r >> 7, g >> 7, b >> 7);
		}
		lpd8806_update_strip();
		++wave_offset;
		wave_offset = wave_offset % STRIPE_LENGTH;
		colorcylePos += 1;
		colorcylePos %= 384;
		color = Wheel(colorcylePos);
		cyclered = ((color & 0xFF0000) >> 16) & 0x7F;
		cyclegreen = ((color & 0xFF00) >> 8) & 0x7F;
		cycleblue = (color & 0xFF) & 0x7F;
		_delay_ms(DELAY_COLORCYCLE_WAVE_MS);
	}
}

void allpixel_randcolor(uint16_t duration)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	while (elapsed_seconds() - start < duration) {
		for (uint8_t i = 0; i < STRIPE_LENGTH; ++i) {
			lpd8806_set_pixel_rgb(i, Wheel(rand() % 384));
		}
		lpd8806_update_strip();
	}
}

void allpixel_colorwave(uint16_t duration)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	uint8_t rn[STRIPE_LENGTH];
	
	uint8_t count = 0;
	while (elapsed_seconds() - start < duration) {
		if ((count % 30) == 0) {
			for (uint8_t i = 0; i < STRIPE_LENGTH; ++i) {
				rn[i] = rand() % 256;
			}
			count = 1;
		}

		for (uint8_t i = 0; i < STRIPE_LENGTH; ++i) {
			uint8_t sinval = sinTable[(wave_offset + rn[i]) % STRIPE_LENGTH];
			uint32_t c = Wheel((rn[i] * 10) % 384);
			uint8_t r = (c & 0xFF0000) >> 16;
			uint8_t g = (c & 0xFF00) >> 8;
			uint8_t b = (c & 0xFF);
			lpd8806_set_pixel((STRIPE_LENGTH - i - 1), (r * sinval) >> 7, (g * sinval) >> 7, (b * sinval) >> 7);
		}
		lpd8806_update_strip();
		wave_offset += 1;
		wave_offset %= STRIPE_LENGTH;
		if (wave_offset == 0) {
			count += 1;
		}
	}
}

void singlecolor_wave(uint16_t duration, uint8_t r, uint8_t g, uint8_t b)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	while (elapsed_seconds() - start < duration)
	{
		for(uint8_t i = 0; i < STRIPE_LENGTH; ++i) 
		{
			uint8_t sinval = sinTable[(wave_offset + i) % STRIPE_LENGTH];
			lpd8806_set_pixel((STRIPE_LENGTH - i - 1), (r * sinval) >> 7, (g * sinval) >> 7, (b * sinval) >> 7);
		}		
		lpd8806_update_strip();
		++wave_offset;
		wave_offset = wave_offset % STRIPE_LENGTH;
		_delay_ms(DELAY_SINGLECOLOR_WAVE_MS);
	}
}	

void singlecolor_edgemiddle(uint16_t duration, uint32_t outeredge, uint32_t middle, uint32_t inneredge)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	while (elapsed_seconds() - start < duration)
	{
		for (uint8_t i = 0; i < STRIPE_LENGTH; ++i) {
			lpd8806_set_pixel(i, 0, 0, 0);
		}
		// inner
		lpd8806_set_pixel_rgb(0, inneredge);
		lpd8806_set_pixel_rgb(STRIPE_LENGTH >> 1, middle);
		lpd8806_set_pixel_rgb(STRIPE_LENGTH - 2, outeredge);
		lpd8806_set_pixel_rgb(STRIPE_LENGTH - 1, outeredge);
		lpd8806_update_strip();
		_delay_ms(100);
	}		
}

void singlecolor_kitt_paint(uint8_t index, uint8_t r, uint8_t g, uint8_t b)
{
	for (uint8_t i = 0; i < STRIPE_LENGTH; ++i)
	{
		if (i == index)
		{
			lpd8806_set_pixel(i++, r >> 2, g >> 2, b >> 2);
			lpd8806_set_pixel(i++, r >> 1, g >> 1, b >> 1);
			lpd8806_set_pixel(i++, r, g, b);
			lpd8806_set_pixel(i++, r >> 1, g >> 1, b >> 1);
			lpd8806_set_pixel(i++, r >> 2, g >> 2, b >> 2);
		}
		else if (i > index + 4 || i < index)
		{
			lpd8806_set_pixel(i, 0, 0, 0);
		}
	}
	lpd8806_update_strip();
	_delay_ms(DELAY_SINGLECOLOR_KITT_MS);
}

void singlecolor_kitt(uint16_t duration, uint8_t r, uint8_t g, uint8_t b)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	while (elapsed_seconds() - start < duration)
	{
		for (uint8_t index = 0; index < STRIPE_LENGTH - 5; ++index)
		{
			singlecolor_kitt_paint(index, r, g, b);
		}
		
		for (uint8_t index = STRIPE_LENGTH - 5; index > 0; --index)
		{
			singlecolor_kitt_paint(index, r, g, b);
		}
	}
}

void singlecolor_all(uint16_t duration, uint8_t r, uint8_t g, uint8_t b)
{
	uint16_t start = elapsed_seconds();
	
	if (duration == 0) {
		duration = 0xFFFF;
	}
	
	while (elapsed_seconds() - start < duration) {
		for (uint8_t i = 0; i < STRIPE_LENGTH; ++i) {
			lpd8806_set_pixel(i, r, g, b);
		}
		lpd8806_update_strip();
		_delay_ms(DELAY_SINGLECOLOR_ALL_MS);
	}
}