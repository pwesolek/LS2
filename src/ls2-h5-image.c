/*
  
  This file is part of LS² - the Localization Simulation Engine of FU Berlin.

  Copyright 2011-2013   Heiko Will, Marcel Kyas, Thomas Hillebrandt,
  Stefan Adler, Malte Rohde, Jonathan Gunthermann

  LS² is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  LS² is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with LS².  If not, see <http://www.gnu.org/licenses/>.

 */

#if HAVE_CONFIG_H
#  include "ls2/ls2-config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_POPT_H
#  include <popt.h>
#endif

#include "ls2/library.h"
#include "ls2/ls2.h"
#include "ls2/backend.h"
#include "ls2/util.h"

static char const *output_format;           /* Format of the output files. */ 
static char const *output[NUM_VARIANTS];    /* Names of output files.      */
static char const *input_hdf5;              /* Name of raw input files.    */
static char const *inverted;      /* Name of inverted density output file. */

static int stride = 10;
static long runs;

double ls2_backend_steps = 0.0;

int main(int argc, const char* argv[])
{
    poptContext opt_con;       /* context for parsing command-line options */
    int rc;

    /* Command line arguments. */
    static struct poptOption cli_options[] = {
        { "gradation", 'G',
          POPT_ARG_DOUBLE | POPT_ARGFLAG_SHOW_DEFAULT,
          &ls2_backend_steps, 0,
          "number of gradation steps, 0 is unlimited", "steps" },
        { "output-average", 'o',
          POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
          &(output[AVERAGE_ERROR]), 0,
          "name of the average error output image file", "file name" },
        { "output-maximum", 'M',
          POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
          &(output[MAXIMUM_ERROR]), 0,
          "name of the maximum error output image file", "file name" },
        { "output-minimum", 'm',
          POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
          &(output[MINIMUM_ERROR]), 0,
          "name of the minimum error output image file", "file name" },
        { "output-variance", 's',
          POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &(output[STANDARD_DEVIATION]), 0,
          "name of the output image file of the standard deviation", "file name" },
        { "output-rmse", 'p',
          POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
          &(output[ROOT_MEAN_SQUARED_ERROR]), 0,
          "name of the root mean squared error output image", "file name" },
        { "output-success", 'S',
          POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
          &(output[FAILURES]), 0,
          "name of the failure rate output image file", "file name" },
        { "output-phase", 'z',
          POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT,
          &(output[AVERAGE_X_ERROR]), 0,
          "name of the phase portrait output image", "file name" },
        { "stride", 'S',
          POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT,
          &stride, 0,
          "stride of the phase portrait", "steps" },
        { "inverted", 'i', POPT_ARG_STRING, &inverted, 0,
          "name of the inverted density file", "file name" },
        { "maximum", 0,
          POPT_ARG_LONG | POPT_ARGFLAG_SHOW_DEFAULT,
          &runs, 0,
          "maximum frequency in inverted image", "runs" },
        POPT_AUTOHELP
        POPT_TABLEEND
    };

    output_format = "png";

    opt_con = poptGetContext(NULL, argc, argv, cli_options, 0);
    poptSetOtherOptionHelp(opt_con, "[OPTIONS] file.h5");

    // Check for sufficient number of command line arguments
    if (argc < 2) {
        poptPrintUsage(opt_con, stderr, 0);
        poptFreeContext(opt_con);
        exit(EXIT_FAILURE);
    }

    // Parse the command line arguments.
    while ((rc = poptGetNextOpt(opt_con)) >= 0) {
        switch (rc) {
        default:
            break;
        }
    }

    if (rc < -1) {
        /* an error occurred during option processing */
        fprintf(stderr, "%s: %s\n",
                poptBadOption(opt_con, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
        poptFreeContext(opt_con);
        exit(EXIT_FAILURE);
    }

    // Handle the left-over arguments.
    if (poptPeekArg(opt_con) != NULL) {
        input_hdf5 = poptGetArg(opt_con);
    }

    if (poptPeekArg(opt_con) != NULL) {
        fprintf(stderr, "warning: too many arguments, some were ignored.\n");
    }

    ls2_output_format_t format = get_output_format(output_format);

    vector2 *anchors;
    size_t no_anchors;
    uint16_t height, width;

    // calculate average
    if (inverted == NULL) {
        for (ls2_output_variant var = 0; var < NUM_VARIANTS; var++) {
            if (output[var] != NULL && *(output[var]) != '\0') {
                if (var == AVERAGE_Y_ERROR)
                    continue;
                if (var == AVERAGE_X_ERROR) {
                    float *dx, *dy;
                    ls2_hdf5_read_locbased(input_hdf5, AVERAGE_X_ERROR,
                                           &anchors, &no_anchors, &dx,
                                           &width, &height);
                    ls2_hdf5_read_locbased(input_hdf5, AVERAGE_Y_ERROR,
                                           NULL, NULL, &dy,
                                           &width, &height);
                    ls2_cairo_write_pdf_phase_portrait(output[var], anchors,
						       no_anchors, dx, dy, width,
						       height, (uint16_t) stride);
                    free(dx);
                    free(dy);
                } else {
                    float *results;

	            ls2_hdf5_read_locbased(input_hdf5, var, &anchors, &no_anchors,
				           &results, &width, &height);
	            float mu, sigma, min, max;
	            ls2_statistics(results, (size_t) width * height,
			           &mu, &sigma, &min, &max);
	            fprintf(stdout, "MAE = %f, sdev = %f, min = %f, max = %f\n",
		            mu, sigma, min, max);
		    if (var != FAILURES)
			ls2_write_locbased(format, output[var], anchors, no_anchors,
					   results, width, height);
 		    else
			ls2_write_density(format, output[var], anchors, no_anchors,
					  results, width, height);		      
                    free(results);
                }
                // clean-ups.
                free(anchors);
            }
        }
    } else if (*inverted != '\0') { //Reading an inverted image
        float tag_x, tag_y;
        double center_x, center_y;
        uint64_t *result;
        ls2_hdf5_read_inverted(input_hdf5, &tag_x, &tag_y, &anchors, &no_anchors,
			       &result, &width, &height, &center_x, &center_y);
        uint64_t maximum = 0;
        for (int i = 0; i < width * height; i++)
            maximum = MAX(result[i], maximum);
        fprintf(stdout, "Actual maximum: %" PRIu64 ", used maximum: %" PRIu64 "\n",
                maximum, ((runs == 0) ? maximum : (uint64_t) runs));
	ls2_write_inverted(format, inverted, (uint64_t) runs, tag_x, tag_y, anchors, no_anchors,
			   result, width, height, (float) center_x, (float) center_y);
        free(result);
        free(anchors);
    }
    poptFreeContext(opt_con);
    exit(EXIT_SUCCESS);
}
