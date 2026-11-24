// Internal raster template. Each inclusion defines the span painter
// and the clipping rules before instantiating the shared crossing logic.

static __noinline void POLY_SCAN
{
	bool inside = false;
	word start = 0;
	byte pitch = band->pitch;
	for (byte i = 0; i < pitch; i++)
	{
		byte flags = changes[i];
		word x = (word)(i + band->first_byte) << 3;
		while (flags)
		{
			if (flags & 1)
			{
				if (inside)
					POLY_SPAN(start, x);
				else
					start = x;
				inside = !inside;
			}
			flags >>= 1;
			x++;
		}
	}
	if (inside)
		POLY_SPAN(start, POLY_WIDTH);
}

static __noinline void POLY_FILL
{
	POLY_BITMAP
	int top = points[0].y, bottom = top;
	int left = points[0].x, right = left;
	for (byte i = 1; i < count; i++)
	{
		int y = points[i].y;
		if (y < top)
			top = y;
		if (y > bottom)
			bottom = y;
		int x = points[i].x;
		if (x < left)
			left = x;
		if (x > right)
			right = x;
	}
	if (right < 0 || left >= (int)bm->width)
		return;
	if (left < 0)
		left = 0;
	if (right >= (int)bm->width)
		right = bm->width - 1;
	if (top < 0)
		top = 0;
	if (bottom > (int)bm->height)
		bottom = bm->height;

	POLY_CLIP

	PolygonBand band;
	band.width = bm->width;
	band.first_byte = (word)left >> 3;
	band.pitch = ((word)right >> 3) - band.first_byte + 1;
	while (top < bottom)
	{
		int end = top + BAND_ROWS;
		if (end > bottom)
			end = bottom;
		band.top = top;
		band.bottom = end;
		byte bytes = band.pitch * (end - top);
		for (byte i = 0; i < bytes; i++)
			band.changes[i] = 0;

		const ABitmapPoint * last = points + (count - 1);
		for (byte i = 0; i < count; i++)
		{
			const ABitmapPoint * next = points + i;
			mark_edge(&band, last, next);
			last = next;
		}

		const byte * row = band.changes;
		while (top < end)
		{
			POLY_ROW;
			row += band.pitch;
			top++;
		}
	}
}

#undef POLY_SCAN
#undef POLY_SPAN
#undef POLY_WIDTH
#undef POLY_FILL
#undef POLY_BITMAP
#undef POLY_CLIP
#undef POLY_ROW
