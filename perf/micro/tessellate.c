/*
 * Copyright © 2006 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Red Hat, Inc. not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. Red Hat, Inc. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * RED HAT, INC. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL RED HAT, INC. BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author: Carl D. Worth <cworth@cworth.org>
 */

#include "cairo-perf.h"

typedef struct {
    double x;
    double y;
} point_t;

point_t points[300] = {
 {39.4383,84.0188}, {79.844,78.3099}, {19.7551,91.1647}, {76.823,33.5223}, {55.397,27.7775},
 {62.8871,47.7397}, {51.3401,36.4784}, {91.6195,95.223}, {71.7297,63.5712}, {60.6969,14.1603},
 {24.2887,1.63006}, {80.4177,13.7232}, {40.0944,15.6679}, {10.8809,12.979}, {21.8257,99.8925},
 {83.9112,51.2932}, {29.6032,61.264}, {52.4287,63.7552}, {97.2775,49.3583}, {77.1358,29.2517},
 {76.9914,52.6745}, {89.1529,40.0229}, {35.2458,28.3315}, {91.9026,80.7725}, {94.9327,6.97553},
 {8.60558,52.5995}, {66.3227,19.2214}, {34.8893,89.0233}, {2.0023,6.41713}, {6.30958,45.7702},
 {97.0634,23.828}, {85.092,90.2208}, {53.976,26.6666}, {76.0249,37.5207}, {66.7724,51.2535},
 {3.92803,53.1606}, {93.1835,43.7638}, {72.0952,93.081}, {73.8534,28.4293}, {35.4049,63.9979},
 {16.5974,68.7861}, {88.0075,44.0105}, {33.0337,82.9201}, {89.3372,22.8968}, {68.667,35.036},
 {58.864,95.6468}, {85.8676,65.7304}, {92.397,43.956}, {81.4767,39.8437}, {91.0972,68.4219},

 {21.5825,48.2491}, {92.0128,95.0252}, {88.1062,14.766}, {43.1953,64.1081}, {28.1059,61.9596},
 {30.7458,78.6002}, {22.6107,44.7034}, {27.6235,18.7533}, {41.6501,55.6444}, {90.6804,16.9607},
 {12.6075,10.3171}, {76.0475,49.5444}, {93.5004,98.4752}, {38.3188,68.4445}, {36.8664,74.9771},
 {23.2262,29.416}, {24.4413,58.4489}, {73.2149,15.239}, {79.347,12.5475}, {74.5071,16.4102},
 {95.0104,7.45298}, {52.1563,5.25293}, {24.0062,17.6211}, {73.2654,79.7798}, {96.7405,65.6564},
 {75.9735,63.9458}, {13.4902,9.34805}, {7.82321,52.021}, {20.4655,6.99064}, {81.9677,46.142},
 {75.5581,57.3319}, {15.7807,5.19388}, {20.4329,99.9994}, {12.5468,88.9956}, {5.40576,99.7799},
 {7.23288,87.054}, {92.3069,0.416161}, {18.0372,59.3892}, {39.169,16.3131}, {81.9695,91.3027},
 {55.2485,35.9095}, {45.2576,57.943}, {9.96401,68.7387}, {75.7294,53.0808}, {99.2228,30.4295},
 {87.7614,57.6971}, {62.891,74.7809}, {74.7803,3.54209}, {92.5377,83.3239}, {83.1038,87.3271},

 {74.3811,97.9434}, {98.3596,90.3366}, {49.7259,66.688}, {83.0012,16.3968}, {7.69947,88.8949},
 {24.8044,64.9707}, {22.9137,62.948}, {31.6867,70.062}, {23.1428,32.8777}, {63.3072,7.4161},
 {65.1132,22.3656}, {97.1466,51.0686}, {54.6107,28.0042}, {11.3281,71.9269}, {59.254,47.1483},
 {45.0918,94.4318}, {84.7684,33.6351}, {0.323146,43.4513}, {59.8481,34.4943}, {23.3892,83.3243},
 {48.295,67.5476}, {30.4956,48.1936}, {18.2556,71.2087}, {4.08643,62.1823}, {69.5984,41.3984},
 {63.764,67.3936}, {18.4622,34.7116}, {62.7158,60.9106}, {32.8374,73.0729}, {20.2213,74.0438},
 {68.4757,92.0914}, {25.7265,65.313}, {8.76436,53.2441}, {87.7384,26.0497}, {9.37402,68.6125},
 {36.1601,11.1276}, {59.3211,57.6691}, {28.8778,66.6557}, {28.8379,77.5767}, {18.9751,32.9642},
 {0.357857,98.4363}, {33.1479,82.7391}, {43.6497,18.8201}, {91.893,95.8637}, {69.9075,76.4871},
 {68.5786,12.1143}, {77.4273,38.3832}, {91.6273,94.3051}, {20.3548,86.1917}, {54.8042,79.3657},

 {90.4932,29.7288}, {87.3979,90.9643}, {57.62,49.8144}, {27.3911,16.2757}, {49.2399,86.4579},
 {84.8942,46.3662}, {29.1053,49.5977}, {68.4178,18.0421}, {13.9058,72.755}, {49.2422,60.3109},
 {72.4252,83.8134}, {22.1966,17.8208}, {12.1259,49.8525}, {36.0443,13.8238}, {93.1895,32.4807},
 {62.2095,90.8485}, {81.8128,83.6828}, {33.4972,49.6074}, {65.8831,39.4327}, {25.8906,60.8883},
 {7.2545,15.123}, {64.7207,10.7848}, {28.827,36.3598}, {9.11486,33.1386}, {93.4495,42.7328},
 {26.5461,58.357}, {76.1778,65.8747}, {15.7272,48.7427}, {62.5665,88.3037}, {20.7844,51.7715},
 {42.6199,55.7561}, {39.4388,82.9939}, {32.6013,24.4327}, {63.8654,72.936}, {33.8243,98.4845},
 {13.6075,89.756}, {0.540855,41.0788}, {77.4386,78.3282}, {11.4668,29.3678}, {72.1006,86.5535},
 {44.9105,4.91625}, {70.7909,98.6467}, {47.3894,21.0883}, {9.39195,86.5181}, {38.2896,9.95593},
 {65.712,30.1763}, {13.1702,80.9095}, {5.34223,5.15083}, {78.0868,45.7716}, {44.256,69.2076},

 {58.9637,11.9111}, {52.9899,57.8635}, {36.1917,59.5045}, {88.8723,30.4285}, {16.982,47.6585},
 {52.5747,60.9729}, {59.6196,61.8925}, {82.9808,23.3656}, {9.88374,7.00902}, {16.965,92.3728},
 {22.5491,48.1733}, {29.0829,82.6769}, {87.8278,35.7193}, {81.4909,34.4251}, {3.63274,65.9146},
 {77.8257,25.7469}, {83.6104,62.5964}, {22.1009,30.8157}, {61.2442,19.8021}, {67.4605,10.9733},
 {71.9462,78.2262}, {40.1188,20.0352}, {43.4009,31.5658}, {38.5748,23.0996}, {15.4724,53.2846},
 {1.45793,55.5398}, {38.2167,38.0215}, {73.7408,30.5408}, {64.9659,26.0445}, {91.9591,55.2316},
 {80.9785,68.5986}, {31.195,69.7848}, {0.600477,64.5889}, {84.391,53.296}, {64.2693,61.8447},
 {40.0709,51.8515}, {71.8867,36.2154}, {67.7812,80.1897}, {3.28927,15.2876}, {68.5722,6.35606},
 {61.8958,18.7616}, {56.7831,70.0301}, {0.570914,0.112548}, {26.157,30.5239}, {85.7555,65.5368},
 {34.1354,18.1161}, {87.9009,66.7341}, {31.323,65.3305}, {18.6265,88.5014}, {50.3461,15.7139},

 {67.5654,82.8957}, {19.1112,90.417}, {70.6067,39.4521}, {54.7397,86.8924}, {93.2485,73.8959},
 {92.6576,23.3119}, {93.342,55.1443}, {55.2568,49.4407}, {79.9646,93.9129}, {59.4497,81.4139},
 {99.53,65.7201}, {32.4541,93.5852}, {58.9157,87.4309}, {75.9324,63.7771}, {79.491,77.5421},
 {60.4379,26.2785}, {16.6955,47.0564}, {86.5086,79.549}, {66.4414,87.3021}, {61.1981,41.2483},
 {64.5601,59.6899}, {14.8342,53.8557}, {3.29634,57.9022}, {51.8151,70.091}, {51.5049,83.2609},
 {48.981,11.2648}, {4.84997,51.0349}, {38.4658,81.4351}, {45.2122,63.7656}, {41.3078,14.3982},
 {40.6767,24.7033}, {71.7597,1.74566}, {81.2947,57.3721}, {44.6743,58.2682}, {99.5165,47.7361},
 {7.42604,5.87232}, {59.728,64.0766}, {21.9788,22.2602}, {92.3513,63.0243}, {46.2852,73.7939},
 {85.0586,43.8562}, {94.8911,95.2662}, {76.7014,89.9086}, {53.6742,33.3569}, {47.7551,21.9136},
 {46.6169,94.982}, {96.7277,88.4318}, {45.8039,18.3765}, {76.6448,78.0224}, {25.7585,90.4782}
};

static cairo_time_t
do_tessellate (cairo_t *cr, int num_points, int loops)
{
    int i;

    for (i=0; i < num_points; i++)
	cairo_line_to (cr, points[i].x, points[i].y);

    cairo_perf_timer_start ();

    /* We'd like to measure just tessellation without
     * rasterization. For now, we can do that with cairo_in_fill. But
     * we'll have to be careful since cairo_in_fill might eventually
     * be optimized to have an implementation that doesn't necessarily
     * include tessellation. */
    while (loops--)
	cairo_in_fill (cr, 50, 50);

    cairo_perf_timer_stop ();

    cairo_new_path (cr);

    return cairo_perf_timer_elapsed ();
}

static cairo_time_t
tessellate_16 (cairo_t *cr, int width, int height, int loops)
{
    return do_tessellate (cr, 16, loops);
}

static cairo_time_t
tessellate_64 (cairo_t *cr, int width, int height, int loops)
{
    return do_tessellate (cr, 64, loops);
}

static cairo_time_t
tessellate_256 (cairo_t *cr, int width, int height, int loops)
{
    return do_tessellate (cr, 256, loops);
}

void
tessellate (cairo_perf_t *perf, cairo_t *cr, int width, int height)
{
    if (! cairo_perf_can_run (perf, "tessellate", NULL))
	return;

    cairo_perf_run (perf, "tessellate-16", tessellate_16, NULL);
    cairo_perf_run (perf, "tessellate-64", tessellate_64, NULL);
    cairo_perf_run (perf, "tessellate-256", tessellate_256, NULL);
}

#if 0
double
rand_within (double range)
{
    return range * (rand() / (RAND_MAX + 1.0));
}

int
main (void)
{
#define SIZE 100
    int i;

    printf ("point_t points[] = {\n");

    for (i=0; i < 1000; i++) {
	printf (" {%g,%g},", rand_within (SIZE), rand_within (SIZE));
	if (i % 5 == 4)
	    printf ("\n");
    }

    printf ("};\n");
}
#endif
