void process_fragment(in DolphinFragmentInput frag_input, out DolphinFragmentOutput frag_output)
{
	float layer = frag_input.tex0.z;
	float r = efb_scale * SPREAD_MULTIPLIER;
	float coord;
	float length;
	float2 initial_coords = frag_input.tex0.xy;;
	float brightness = 1.0;

	if (HORIZONTAL)
	{
		length = source_resolution.x;
		coord = initial_coords.x;
		brightness = BRIGHTNESS_MULTIPLIER * brightness;
	}
	else
	{
		length = source_resolution.y;
		coord = initial_coords.y;
	}

	float offset;
	float count = 0.0;
	float4 col = float4(0.0,0.0,0.0,0.0);
	for (offset = -r; offset <= r; offset += 2.0) 
	{
		float between_pixels = -0.5 * sign(offset)/length;
		float pos = coord + offset/length + between_pixels;

		if (pos <= 1.0 && pos >= 0.0)
		{
			float3 sample_coords;
			if (HORIZONTAL)
			{
				sample_coords = float3(pos, initial_coords.y, layer);
			}
			else
			{
				sample_coords = float3(initial_coords.x, pos, layer);
			}
			
			col += texture(samp0, sample_coords);
			count += 1.0;
		}
	}
	frag_output.main = col / count * brightness;

}
