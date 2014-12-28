#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "general.h"
#include "lpc.h"
#include "vorbis_lpc.h"
#include "utils.h"

#define handle_alloc_error(x) if (!x) die("Could not allocate memory.\n")

/* void hanning(const float *input, const size_t len, float *output) */
/* { */
/* 	unsigned int i; */
/* 	float coef; */

/* 	for (i = 0; i < len; ++i) { */
/* 		coef = 0.5 - 0.5 * cos(2 * M_PI * i / len); */
/* 		*output++ = *input++ * coef; */
/* 	} */
/* } */

float get_pitch_by_amdf(float *input, const size_t len)
{
	/*
	 * Min and max pitch *in sample*. We use 50Hz and 300Hz as arbitrary pitch
	 * limits.
	 */
	const unsigned int min_pitch = SAMPLE_RATE / 300;
	const unsigned int max_pitch = SAMPLE_RATE / 50;
	float pitch = 0;

	unsigned int i, j;

	float *d = calloc(max_pitch - min_pitch, sizeof(*d));
	handle_alloc_error(d);

	/* compute the difference signals */
	for (i = min_pitch; i < max_pitch; ++i) {

		for (j = i; j < len; ++j) {
			d[i] += fabs(input[i] - input[i - j]);
		}

		/* mean */
		d[i] /= len - i + 1;
	}

	/* determine optimal pitch */
	for (i = 0; i < max_pitch - min_pitch; ++i) {
		if (pitch < d[i])
			pitch = d[i];
	}

	free(d);

	return pitch;
}

LpcData *lpc_data_init(const size_t input_len)
{
	LpcData *lpc_data = NULL;
	LpcChunk *chunks = NULL;

	size_t n_chunks = input_len / WINDOW_SIZE;

	/* create a struct to store the lpc analysis results */
	lpc_data = calloc(1, sizeof(*lpc_data));
	handle_alloc_error(lpc_data);

	/* allocate memory for the chunks */
	chunks = calloc(n_chunks, sizeof(*chunks));
	handle_alloc_error(chunks);

	/* fill struct info */
	lpc_data->n_chunks = n_chunks;
	lpc_data->chunk_size = WINDOW_SIZE;
	lpc_data->chunks = chunks;

	return lpc_data;
}

void lpc_data_free(LpcData *lpc_data)
{
	/* free the mallocs... */
	free(lpc_data->chunks);
	free(lpc_data);
}

void lpc_detect_voiced(float *input, LpcData *lpc_data)
{
	unsigned int i, j, c;
	float val1, val2, p, f;

	/* count the number of zero crossings in each chunks */
	for (i = 0; i < lpc_data->n_chunks; ++i) {

		/* zero crossing counter */
		c = 0;

		/* TODO: discard chunk if chunk qualifies as (white) noise */
		for (j = 1; j < lpc_data->chunk_size; ++j) {
			val1 = input[i * lpc_data->chunk_size + j];
			val2 = input[i * lpc_data->chunk_size + j + 1];

			/* yup, it's a zero crossing */
			if ((val1 * val2) < 0) {
				c++;
			}
		}

		/* TODO: skip these steps by using a threshold directly based on the
		 * number of zero crossings (converted from the max frequency). */

		/* compute period */
		p = 2 * lpc_data->chunk_size / c;

		/* compute frequency */
		f = 1 / (2 * p);

		/* set as voiced if criterion is fullfilled */
		if (f > F_THRESHOLD) {
			lpc_data->chunks[i].pitch = 1;
		}
	}
}

LpcData *lpc_encode(float *input, const size_t input_len)
{
	LpcChunk *lpc_chunk = NULL;
	LpcData *lpc_data = NULL;

	float *chunk_ptr = NULL;
	unsigned int i;

	/* create a struct for the results */
	lpc_data = lpc_data_init(input_len);

	/* pre emphasis filter */
	/* TODO */

	/* detect voiced/ non-voiced sound */
	lpc_detect_voiced(input, lpc_data);

	/* TODO: use autocorrelation to compute the pitch (?) */

	/* as AMDF is simpler to implement, let's use it for now */
	for (i = 0; i < lpc_data->n_chunks; ++i) {
		lpc_chunk = &lpc_data->chunks[i];

		/* skip step for non-voiced sounds */
		if (lpc_chunk->pitch == 0)
			continue;

		chunk_ptr = (float *) &input[lpc_data->chunk_size * i];
		lpc_chunk->pitch = get_pitch_by_amdf(chunk_ptr, lpc_data->chunk_size);
	}

	/* compute lpc coefficients */
	for (i = 0; i < lpc_data->n_chunks; ++i) {
		lpc_chunk = &lpc_data->chunks[i];
		chunk_ptr = (float *) &input[lpc_data->chunk_size * i];

		/* Compute LPC thx to libvorbis function. TODO: use the error ? */
		vorbis_lpc_from_data(chunk_ptr, lpc_chunk->coefficients, 
				lpc_data->chunk_size, N_COEFFS);
	}

	return lpc_data;
}

void lpc_decode(LpcData *lpc_data, float *output) 
{
	unsigned int i;
	LpcChunk *lpc_chunk = NULL;

	float *out_ptr;

	for (i = 0; i < lpc_data->n_chunks; ++i) {
		/* compute excitation */

		if (lpc_chunk->pitch > 0) {
			/* voiced sound, use a pulse impulsion train */

		}
		else {
			/* non voiced sound, use a white noise */
		}

		out_ptr = &output[lpc_data->chunk_size * i];

		/* lpc decode */
		vorbis_lpc_predict(lpc_chunk->coefficients, NULL, N_COEFFS, out_ptr,
				lpc_data->chunk_size); 
	}
}
