#include "cse_geoarithmetic.h"
#include <math.h>
#include <float.h>

static double sq(double x) { return x * x; }
static unsigned init = 0;
static const int FALSE = 0;
static const int TRUE = 1;
static unsigned digits, maxit1, maxit2;
static double epsilon, doublemin, pi, degree, NaN,
tiny, tol0, tol1, tol2, tolb, xthresh;

/**
 * mask values for the \e caps argument to geod_lineinit().
 **********************************************************************/
enum geod_mask {
    GEOD_NONE = 0U,                    /**< Calculate nothing */
    GEOD_LATITUDE = 1U << 7 | 0U,           /**< Calculate latitude */
    GEOD_LONGITUDE = 1U << 8 | 1U << 3,        /**< Calculate longitude */
    GEOD_AZIMUTH = 1U << 9 | 0U,           /**< Calculate azimuth */
    GEOD_DISTANCE = 1U << 10 | 1U << 0,        /**< Calculate distance */
    GEOD_DISTANCE_IN = 1U << 11 | 1U << 0 | 1U << 1,/**< Allow distance as input  */
    GEOD_REDUCEDLENGTH = 1U << 12 | 1U << 0 | 1U << 2,/**< Calculate reduced length */
    GEOD_GEODESICSCALE = 1U << 13 | 1U << 0 | 1U << 2,/**< Calculate geodesic scale */
    GEOD_AREA = 1U << 14 | 1U << 4,        /**< Calculate reduced length */
    GEOD_ALL = 0x7F80U | 0x1FU         /**< Calculate everything */
};

/**
 * flag values for the \e flags argument to geod_gendirect() and
 * geod_genposition()
 **********************************************************************/
enum geod_flags {
    GEOD_NOFLAGS = 0U,     /**< No flags */
    GEOD_ARCMODE = 1U << 0,  /**< Position given in terms of arc distance */
    GEOD_LONG_UNROLL = 1U << 15  /**< Unroll the longitude */
};


static double LatFix(double x)
{
	return fabs(x) > 90 ? NaN : x;
}


static double minx(double a, double b)
{
	return (b < a) ? b : a;
}

static double maxx(double a, double b)
{
	return (a < b) ? b : a;
}

static void swapx(double* x, double* y)
{
	double t = *x; *x = *y; *y = t;
}


#define GEOGRAPHICLIB_GEODESIC_ORDER 6
#define nA1   GEOGRAPHICLIB_GEODESIC_ORDER
#define nC1   GEOGRAPHICLIB_GEODESIC_ORDER
#define nC1p  GEOGRAPHICLIB_GEODESIC_ORDER
#define nA2   GEOGRAPHICLIB_GEODESIC_ORDER
#define nC2   GEOGRAPHICLIB_GEODESIC_ORDER
#define nA3   GEOGRAPHICLIB_GEODESIC_ORDER
#define nA3x  nA3
#define nC3   GEOGRAPHICLIB_GEODESIC_ORDER
#define nC3x  ((nC3 * (nC3 - 1)) / 2)
#define nC4   GEOGRAPHICLIB_GEODESIC_ORDER
#define nC4x  ((nC4 * (nC4 + 1)) / 2)
#define nC    (GEOGRAPHICLIB_GEODESIC_ORDER + 1)

static void Init(void) {
    if (!init) {
        digits = DBL_MANT_DIG;
        epsilon = DBL_EPSILON;
        doublemin = DBL_MIN;
#if defined(M_PI)
        pi = M_PI;
#else
        pi = atan2(0.0, -1.0);
#endif
        maxit1 = 20;
        maxit2 = maxit1 + digits + 10;
        tiny = sqrt(doublemin);
        tol0 = epsilon;
        /* Increase multiplier in defn of tol1 from 100 to 200 to fix inverse case
         * 52.784459512564 0 -52.784459512563990912 179.634407464943777557
         * which otherwise failed for Visual Studio 10 (Release and Debug) */
        tol1 = 200 * tol0;
        tol2 = sqrt(tol0);
        /* Check on bisection interval */
        tolb = tol0 * tol2;
        xthresh = 1000 * tol2;
        degree = pi / 180;
        NaN = nan("0");
        init = 1;
    }
}

enum captype {
    CAP_NONE = 0U,
    CAP_C1 = 1U << 0,
    CAP_C1p = 1U << 1,
    CAP_C2 = 1U << 2,
    CAP_C3 = 1U << 3,
    CAP_C4 = 1U << 4,
    CAP_ALL = 0x1FU,
    OUT_ALL = 0x7F80U
};

CSE_GeoArithmetic::CSE_GeoArithmetic(void)
{

}

CSE_GeoArithmetic::~CSE_GeoArithmetic()
{

}

void CSE_GeoArithmetic::CalPointByDistanceAndAzimuth(double dSemiMajorAxis, double dSemiMinorAxis, double lat1, double lon1, double azi1, double s12, double* plat2, double* plon2)
{
    
    geod_geodesic g;
    double a = dSemiMajorAxis;
    double f = (dSemiMajorAxis - dSemiMinorAxis) / dSemiMajorAxis;
    geod_init(&g,dSemiMajorAxis, f);
    double* pazi2 = nullptr;
	geod_gendirect(&g, lat1, lon1, azi1, GEOD_NOFLAGS, s12, plat2, plon2, pazi2,
		nullptr, nullptr, nullptr, nullptr, nullptr);
}



void CSE_GeoArithmetic::CalDistanceAndAzimuthByTwoPoints(double dSemiMajorAxis,
    double dSemiMinorAxis,
    double lat1,
    double lon1,
    double lat2,
    double lon2,
    double* s12,
    double* pazi1,
    double* pazi2)
{
    // 初始化大地椭球相关参数
    geod_geodesic g;
    double a = dSemiMajorAxis;
    double f = (dSemiMajorAxis - dSemiMinorAxis) / dSemiMajorAxis;
    geod_init(&g, dSemiMajorAxis, f);

    double salp1 = 0;
    double calp1 = 0;
    double salp2 = 0;
    double calp2 = 0;
    double a12 = geod_geninverse_int(&g, lat1, lon1, lat2, lon2, s12,
            &salp1, &calp1, &salp2, &calp2,
            nullptr, nullptr, nullptr, nullptr);
    if (pazi1) *pazi1 = atan2dx(salp1, calp1);
    if (pazi2) *pazi2 = atan2dx(salp2, calp2);
}

void CSE_GeoArithmetic::geod_direct(const struct geod_geodesic* g,
	double lat1, double lon1, double azi1,
	double s12,
	double* plat2, double* plon2, double* pazi2) {
	geod_gendirect(g, lat1, lon1, azi1, GEOD_NOFLAGS, s12, plat2, plon2, pazi2,
		nullptr, nullptr, nullptr, nullptr, nullptr);
}



double CSE_GeoArithmetic::SinCosSeries(boolx sinp, double sinx, double cosx, const double c[], int n)
{
	/* Evaluate
	 * y = sinp ? sum(c[i] * sin( 2*i    * x), i, 1, n) :
	 *            sum(c[i] * cos((2*i+1) * x), i, 0, n-1)
	 * using Clenshaw summation.  N.B. c[0] is unused for sin series
	 * Approx operation count = (n + 5) mult and (2 * n + 2) add */
	double ar, y0, y1;
	c += (n + sinp);              /* Point to one beyond last element */
	ar = 2 * (cosx - sinx) * (cosx + sinx); /* 2 * cos(2 * x) */
	y0 = (n & 1) ? *--c : 0; y1 = 0;        /* accumulators for sum */
	/* Now n is even */
	n /= 2;
	while (n--) {
		/* Unroll loop x 2, so accumulators return to their original role */
		y1 = ar * y0 - y1 + *--c;
		y0 = ar * y1 - y0 + *--c;
	}
	return sinp
		? 2 * sinx * cosx * y0      /* sin(2 * x) * y0 */
		: cosx * (y0 - y1);         /* cos(x) * (y0 - y1) */
}

double CSE_GeoArithmetic::geod_genposition(const struct geod_geodesicline* l,
    unsigned flags, double s12_a12,
    double* plat2, double* plon2, double* pazi2,
    double* ps12, double* pm12,
    double* pM12, double* pM21,
    double* pS12) {
    double lat2 = 0, lon2 = 0, azi2 = 0, s12 = 0,
        m12 = 0, M12 = 0, M21 = 0, S12 = 0;
    /* Avoid warning about uninitialized B12. */
    double sig12, ssig12, csig12, B12 = 0, AB1 = 0;
    double omg12, lam12, lon12;
    double ssig2, csig2, sbet2, cbet2, somg2, comg2, salp2, calp2, dn2;
    unsigned outmask =
        (plat2 ? GEOD_LATITUDE : GEOD_NONE) |
        (plon2 ? GEOD_LONGITUDE : GEOD_NONE) |
        (pazi2 ? GEOD_AZIMUTH : GEOD_NONE) |
        (ps12 ? GEOD_DISTANCE : GEOD_NONE) |
        (pm12 ? GEOD_REDUCEDLENGTH : GEOD_NONE) |
        (pM12 || pM21 ? GEOD_GEODESICSCALE : GEOD_NONE) |
        (pS12 ? GEOD_AREA : GEOD_NONE);

    outmask &= l->caps & OUT_ALL;
    if (!((flags & GEOD_ARCMODE || (l->caps & (GEOD_DISTANCE_IN & OUT_ALL)))))
        /* Impossible distance calculation requested */
        return NaN;

    if (flags & GEOD_ARCMODE) {
        /* Interpret s12_a12 as spherical arc length */
        sig12 = s12_a12 * degree;
        sincosdx(s12_a12, &ssig12, &csig12);
    }
    else {
        /* Interpret s12_a12 as distance */
        double
            tau12 = s12_a12 / (l->b * (1 + l->A1m1)),
            s = sin(tau12),
            c = cos(tau12);
        /* tau2 = tau1 + tau12 */
        B12 = -SinCosSeries(TRUE,
            l->stau1 * c + l->ctau1 * s,
            l->ctau1 * c - l->stau1 * s,
            l->C1pa, nC1p);
        sig12 = tau12 - (B12 - l->B11);
        ssig12 = sin(sig12); csig12 = cos(sig12);
        if (fabs(l->f) > 0.01) {
            /* Reverted distance series is inaccurate for |f| > 1/100, so correct
             * sig12 with 1 Newton iteration.  The following table shows the
             * approximate maximum error for a = WGS_a() and various f relative to
             * GeodesicExact.
             *     erri = the error in the inverse solution (nm)
             *     errd = the error in the direct solution (series only) (nm)
             *     errda = the error in the direct solution (series + 1 Newton) (nm)
             *
             *       f     erri  errd errda
             *     -1/5    12e6 1.2e9  69e6
             *     -1/10  123e3  12e6 765e3
             *     -1/20   1110 108e3  7155
             *     -1/50  18.63 200.9 27.12
             *     -1/100 18.63 23.78 23.37
             *     -1/150 18.63 21.05 20.26
             *      1/150 22.35 24.73 25.83
             *      1/100 22.35 25.03 25.31
             *      1/50  29.80 231.9 30.44
             *      1/20   5376 146e3  10e3
             *      1/10  829e3  22e6 1.5e6
             *      1/5   157e6 3.8e9 280e6 */
            double serr;
            ssig2 = l->ssig1 * csig12 + l->csig1 * ssig12;
            csig2 = l->csig1 * csig12 - l->ssig1 * ssig12;
            B12 = SinCosSeries(TRUE, ssig2, csig2, l->C1a, nC1);
            serr = (1 + l->A1m1) * (sig12 + (B12 - l->B11)) - s12_a12 / l->b;
            sig12 = sig12 - serr / sqrt(1 + l->k2 * sq(ssig2));
            ssig12 = sin(sig12); csig12 = cos(sig12);
            /* Update B12 below */
        }
    }

    /* sig2 = sig1 + sig12 */
    ssig2 = l->ssig1 * csig12 + l->csig1 * ssig12;
    csig2 = l->csig1 * csig12 - l->ssig1 * ssig12;
    dn2 = sqrt(1 + l->k2 * sq(ssig2));
    if (outmask & (GEOD_DISTANCE | GEOD_REDUCEDLENGTH | GEOD_GEODESICSCALE)) {
        if (flags & GEOD_ARCMODE || fabs(l->f) > 0.01)
            B12 = SinCosSeries(TRUE, ssig2, csig2, l->C1a, nC1);
        AB1 = (1 + l->A1m1) * (B12 - l->B11);
    }
    /* sin(bet2) = cos(alp0) * sin(sig2) */
    sbet2 = l->calp0 * ssig2;
    /* Alt: cbet2 = hypot(csig2, salp0 * ssig2); */
    cbet2 = hypot(l->salp0, l->calp0 * csig2);
    if (cbet2 == 0)
        /* I.e., salp0 = 0, csig2 = 0.  Break the degeneracy in this case */
        cbet2 = csig2 = tiny;
    /* tan(alp0) = cos(sig2)*tan(alp2) */
    salp2 = l->salp0; calp2 = l->calp0 * csig2; /* No need to normalize */

    if (outmask & GEOD_DISTANCE)
        s12 = (flags & GEOD_ARCMODE) ?
        l->b * ((1 + l->A1m1) * sig12 + AB1) :
        s12_a12;

    if (outmask & GEOD_LONGITUDE) {
        double E = copysign(1, l->salp0); /* east or west going? */
        /* tan(omg2) = sin(alp0) * tan(sig2) */
        somg2 = l->salp0 * ssig2; comg2 = csig2;  /* No need to normalize */
        /* omg12 = omg2 - omg1 */
        omg12 = (flags & GEOD_LONG_UNROLL)
            ? E * (sig12
                - (atan2(ssig2, csig2) - atan2(l->ssig1, l->csig1))
                + (atan2(E * somg2, comg2) - atan2(E * l->somg1, l->comg1)))
            : atan2(somg2 * l->comg1 - comg2 * l->somg1,
                comg2 * l->comg1 + somg2 * l->somg1);
        lam12 = omg12 + l->A3c *
            (sig12 + (SinCosSeries(TRUE, ssig2, csig2, l->C3a, nC3 - 1)
                - l->B31));
        lon12 = lam12 / degree;
        lon2 = (flags & GEOD_LONG_UNROLL) ? l->lon1 + lon12 :
            AngNormalize(AngNormalize(l->lon1) + AngNormalize(lon12));
    }

    if (outmask & GEOD_LATITUDE)
        lat2 = atan2dx(sbet2, l->f1 * cbet2);

    if (outmask & GEOD_AZIMUTH)
        azi2 = atan2dx(salp2, calp2);

    if (outmask & (GEOD_REDUCEDLENGTH | GEOD_GEODESICSCALE)) {
        double
            B22 = SinCosSeries(TRUE, ssig2, csig2, l->C2a, nC2),
            AB2 = (1 + l->A2m1) * (B22 - l->B21),
            J12 = (l->A1m1 - l->A2m1) * sig12 + (AB1 - AB2);
        if (outmask & GEOD_REDUCEDLENGTH)
            /* Add parens around (csig1 * ssig2) and (ssig1 * csig2) to ensure
             * accurate cancellation in the case of coincident points. */
            m12 = l->b * ((dn2 * (l->csig1 * ssig2) - l->dn1 * (l->ssig1 * csig2))
                - l->csig1 * csig2 * J12);
        if (outmask & GEOD_GEODESICSCALE) {
            double t = l->k2 * (ssig2 - l->ssig1) * (ssig2 + l->ssig1) /
                (l->dn1 + dn2);
            M12 = csig12 + (t * ssig2 - csig2 * J12) * l->ssig1 / l->dn1;
            M21 = csig12 - (t * l->ssig1 - l->csig1 * J12) * ssig2 / dn2;
        }
    }

    if (outmask & GEOD_AREA) {
        double
            B42 = SinCosSeries(FALSE, ssig2, csig2, l->C4a, nC4);
        double salp12, calp12;
        if (l->calp0 == 0 || l->salp0 == 0) {
            /* alp12 = alp2 - alp1, used in atan2 so no need to normalize */
            salp12 = salp2 * l->calp1 - calp2 * l->salp1;
            calp12 = calp2 * l->calp1 + salp2 * l->salp1;
        }
        else {
            /* tan(alp) = tan(alp0) * sec(sig)
             * tan(alp2-alp1) = (tan(alp2) -tan(alp1)) / (tan(alp2)*tan(alp1)+1)
             * = calp0 * salp0 * (csig1-csig2) / (salp0^2 + calp0^2 * csig1*csig2)
             * If csig12 > 0, write
             *   csig1 - csig2 = ssig12 * (csig1 * ssig12 / (1 + csig12) + ssig1)
             * else
             *   csig1 - csig2 = csig1 * (1 - csig12) + ssig12 * ssig1
             * No need to normalize */
            salp12 = l->calp0 * l->salp0 *
                (csig12 <= 0 ? l->csig1 * (1 - csig12) + ssig12 * l->ssig1 :
                    ssig12 * (l->csig1 * ssig12 / (1 + csig12) + l->ssig1));
            calp12 = sq(l->salp0) + sq(l->calp0) * l->csig1 * csig2;
        }
        S12 = l->c2 * atan2(salp12, calp12) + l->A4 * (B42 - l->B41);
    }

    /* In the pattern
     *
     *   if ((outmask & GEOD_XX) && pYY)
     *     *pYY = YY;
     *
     * the second check "&& pYY" is redundant.  It's there to make the CLang
     * static analyzer happy.
     */
    if ((outmask & GEOD_LATITUDE) && plat2)
        *plat2 = lat2;
    if ((outmask & GEOD_LONGITUDE) && plon2)
        *plon2 = lon2;
    if ((outmask & GEOD_AZIMUTH) && pazi2)
        *pazi2 = azi2;
    if ((outmask & GEOD_DISTANCE) && ps12)
        *ps12 = s12;
    if ((outmask & GEOD_REDUCEDLENGTH) && pm12)
        *pm12 = m12;
    if (outmask & GEOD_GEODESICSCALE) {
        if (pM12) *pM12 = M12;
        if (pM21) *pM21 = M21;
    }
    if ((outmask & GEOD_AREA) && pS12)
        *pS12 = S12;

    return (flags & GEOD_ARCMODE) ? s12_a12 : sig12 / degree;
}






double CSE_GeoArithmetic::geod_gendirect(const struct geod_geodesic* g,
	double lat1, double lon1, double azi1,
	unsigned flags, double s12_a12,
	double* plat2, double* plon2, double* pazi2,
	double* ps12, double* pm12, double* pM12, double* pM21,
	double* pS12) {
	struct geod_geodesicline l;
	unsigned outmask =
		(plat2 ? GEOD_LATITUDE : GEOD_NONE) |
		(plon2 ? GEOD_LONGITUDE : GEOD_NONE) |
		(pazi2 ? GEOD_AZIMUTH : GEOD_NONE) |
		(ps12 ? GEOD_DISTANCE : GEOD_NONE) |
		(pm12 ? GEOD_REDUCEDLENGTH : GEOD_NONE) |
		(pM12 || pM21 ? GEOD_GEODESICSCALE : GEOD_NONE) |
		(pS12 ? GEOD_AREA : GEOD_NONE);

	geod_lineinit(&l, g, lat1, lon1, azi1,
		/* Automatically supply GEOD_DISTANCE_IN if necessary */
		outmask |
		((flags & GEOD_ARCMODE) ? GEOD_NONE : GEOD_DISTANCE_IN));
	return geod_genposition(&l, flags, s12_a12,
		plat2, plon2, pazi2, ps12, pm12, pM12, pM21, pS12);
}



void CSE_GeoArithmetic::geod_lineinit(struct geod_geodesicline* l,
	const struct geod_geodesic* g,
	double lat1, double lon1, double azi1, unsigned caps) {
	double salp1, calp1;
	azi1 = AngNormalize(azi1);
	/* Guard against underflow in salp0 */
	sincosdx(AngRound(azi1), &salp1, &calp1);
	geod_lineinit_int(l, g, lat1, lon1, azi1, salp1, calp1, caps);
}

double CSE_GeoArithmetic::AngNormalize(double x) {
	x = remainder(x, (double)(360));
	return x != -180 ? x : 180;
}



void CSE_GeoArithmetic::sincosdx(double x, double* sinx, double* cosx) {
    /* In order to minimize round-off errors, this function exactly reduces
     * the argument to the range [-45, 45] before converting it to radians. */
    double r, s, c; int q = 0;
    r = remquo(x, (double)(90), &q);
    /* now abs(r) <= 45 */
    r *= degree;
    /* Possibly could call the gnu extension sincos */
    s = sin(r); c = cos(r);
    switch ((unsigned)q & 3U) {
    case 0U: *sinx = s; *cosx = c; break;
    case 1U: *sinx = c; *cosx = -s; break;
    case 2U: *sinx = -s; *cosx = -c; break;
    default: *sinx = -c; *cosx = s; break; /* case 3U */
    }
    if (x != 0) { *sinx += (double)(0); *cosx += (double)(0); }
}


double CSE_GeoArithmetic::atan2dx(double y, double x) {
    /* In order to minimize round-off errors, this function rearranges the
     * arguments so that result of atan2 is in the range [-pi/4, pi/4] before
     * converting it to degrees and mapping the result to the correct
     * quadrant. */
    int q = 0; double ang;
    if (fabs(y) > fabs(x)) { swapx(&x, &y); q = 2; }
    if (x < 0) { x = -x; ++q; }
    /* here x >= 0 and x >= abs(y), so angle is in [-pi/4, pi/4] */
    ang = atan2(y, x) / degree;
    switch (q) {
        /* Note that atan2d(-0.0, 1.0) will return -0.  However, we expect that
         * atan2d will not be called with y = -0.  If need be, include
         *
         *   case 0: ang = 0 + ang; break;
         */
    case 1: ang = (y >= 0 ? 180 : -180) - ang; break;
    case 2: ang = 90 - ang; break;
    case 3: ang = -90 + ang; break;
    default: break;
    }
    return ang;
}

void CSE_GeoArithmetic::geod_lineinit_int(struct geod_geodesicline* l,
    const struct geod_geodesic* g,
    double lat1, double lon1,
    double azi1, double salp1, double calp1,
    unsigned caps) {
    double cbet1, sbet1, eps;
    l->a = g->a;
    l->f = g->f;
    l->b = g->b;
    l->c2 = g->c2;
    l->f1 = g->f1;
    /* If caps is 0 assume the standard direct calculation */
    l->caps = (caps ? caps : GEOD_DISTANCE_IN | GEOD_LONGITUDE) |
        /* always allow latitude and azimuth and unrolling of longitude */
        GEOD_LATITUDE | GEOD_AZIMUTH | GEOD_LONG_UNROLL;

    l->lat1 = LatFix(lat1);
    l->lon1 = lon1;
    l->azi1 = azi1;
    l->salp1 = salp1;
    l->calp1 = calp1;

    sincosdx(AngRound(l->lat1), &sbet1, &cbet1); sbet1 *= l->f1;
    /* Ensure cbet1 = +epsilon at poles */
    norm2(&sbet1, &cbet1); cbet1 = maxx(tiny, cbet1);
    l->dn1 = sqrt(1 + g->ep2 * sq(sbet1));

    /* Evaluate alp0 from sin(alp1) * cos(bet1) = sin(alp0), */
    l->salp0 = l->salp1 * cbet1; /* alp0 in [0, pi/2 - |bet1|] */
    /* Alt: calp0 = hypot(sbet1, calp1 * cbet1).  The following
     * is slightly better (consider the case salp1 = 0). */
    l->calp0 = hypot(l->calp1, l->salp1 * sbet1);
    /* Evaluate sig with tan(bet1) = tan(sig1) * cos(alp1).
     * sig = 0 is nearest northward crossing of equator.
     * With bet1 = 0, alp1 = pi/2, we have sig1 = 0 (equatorial line).
     * With bet1 =  pi/2, alp1 = -pi, sig1 =  pi/2
     * With bet1 = -pi/2, alp1 =  0 , sig1 = -pi/2
     * Evaluate omg1 with tan(omg1) = sin(alp0) * tan(sig1).
     * With alp0 in (0, pi/2], quadrants for sig and omg coincide.
     * No atan2(0,0) ambiguity at poles since cbet1 = +epsilon.
     * With alp0 = 0, omg1 = 0 for alp1 = 0, omg1 = pi for alp1 = pi. */
    l->ssig1 = sbet1; l->somg1 = l->salp0 * sbet1;
    l->csig1 = l->comg1 = sbet1 != 0 || l->calp1 != 0 ? cbet1 * l->calp1 : 1;
    norm2(&l->ssig1, &l->csig1); /* sig1 in (-pi, pi] */
    /* norm2(somg1, comg1); -- don't need to normalize! */

    l->k2 = sq(l->calp0) * g->ep2;
    eps = l->k2 / (2 * (1 + sqrt(1 + l->k2)) + l->k2);

    if (l->caps & CAP_C1) {
        double s, c;
        l->A1m1 = A1m1f(eps);
        C1f(eps, l->C1a);
        l->B11 = SinCosSeries(TRUE, l->ssig1, l->csig1, l->C1a, nC1);
        s = sin(l->B11); c = cos(l->B11);
        /* tau1 = sig1 + B11 */
        l->stau1 = l->ssig1 * c + l->csig1 * s;
        l->ctau1 = l->csig1 * c - l->ssig1 * s;
        /* Not necessary because C1pa reverts C1a
         *    B11 = -SinCosSeries(TRUE, stau1, ctau1, C1pa, nC1p); */
    }

    if (l->caps & CAP_C1p)
        C1pf(eps, l->C1pa);

    if (l->caps & CAP_C2) {
        l->A2m1 = A2m1f(eps);
        C2f(eps, l->C2a);
        l->B21 = SinCosSeries(TRUE, l->ssig1, l->csig1, l->C2a, nC2);
    }

    if (l->caps & CAP_C3) {
        C3f(g, eps, l->C3a);
        l->A3c = -l->f * l->salp0 * A3f(g, eps);
        l->B31 = SinCosSeries(TRUE, l->ssig1, l->csig1, l->C3a, nC3 - 1);
    }

    if (l->caps & CAP_C4) {
        C4f(g, eps, l->C4a);
        /* Multiplier = a^2 * e^2 * cos(alpha0) * sin(alpha0) */
        l->A4 = sq(l->a) * l->calp0 * l->salp0 * g->e2;
        l->B41 = SinCosSeries(FALSE, l->ssig1, l->csig1, l->C4a, nC4);
    }

    l->a13 = l->s13 = NaN;
}


/* The coefficients C2[l] in the Fourier expansion of B2 */
void CSE_GeoArithmetic::C2f(double eps, double c[]) {
	static const double coeff[] = {
		/* C2[1]/eps^1, polynomial in eps2 of order 2 */
		1, 2, 16, 32,
		/* C2[2]/eps^2, polynomial in eps2 of order 2 */
		35, 64, 384, 2048,
		/* C2[3]/eps^3, polynomial in eps2 of order 1 */
		15, 80, 768,
		/* C2[4]/eps^4, polynomial in eps2 of order 1 */
		7, 35, 512,
		/* C2[5]/eps^5, polynomial in eps2 of order 0 */
		63, 1280,
		/* C2[6]/eps^6, polynomial in eps2 of order 0 */
		77, 2048,
	};
	double
		eps2 = sq(eps),
		d = eps;
	int o = 0, l;
	for (l = 1; l <= nC2; ++l) { /* l is index of C2[l] */
		int m = (nC2 - l) / 2;     /* order of polynomial in eps^2 */
		c[l] = d * polyval(m, coeff + o, eps2) / coeff[o + m + 1];
		o += m + 2;
		d *= eps;
	}
}


void CSE_GeoArithmetic::C4f(const struct geod_geodesic* g, double eps, double c[]) {
    /* Evaluate C4 coeffs
     * Elements c[0] through c[nC4 - 1] are set */
    double mult = 1;
    int o = 0, l;
    for (l = 0; l < nC4; ++l) {   /* l is index of C4[l] */
        int m = nC4 - l - 1;        /* order of polynomial in eps */
        c[l] = mult * polyval(m, g->C4x + o, eps);
        o += m + 1;
        mult *= eps;
    }
}

double CSE_GeoArithmetic::polyval(int N, const double p[], double x) {
	double y = N < 0 ? 0 : *p++;
	while (--N >= 0) y = y * x + *p++;
	return y;
}


double CSE_GeoArithmetic::A2m1f(double eps) {
	static const double coeff[] = {
		/* (eps+1)*A2-1, polynomial in eps2 of order 3 */
		-11, -28, -192, 0, 256,
	};
	int m = nA2 / 2;
	double t = polyval(m, coeff, sq(eps)) / coeff[m + 1];
	return (t - eps) / (1 + eps);
}


void CSE_GeoArithmetic::C3f(const struct geod_geodesic* g, double eps, double c[]) {
    /* Evaluate C3 coeffs
     * Elements c[1] through c[nC3 - 1] are set */
    double mult = 1;
    int o = 0, l;
    for (l = 1; l < nC3; ++l) {   /* l is index of C3[l] */
        int m = nC3 - l - 1;        /* order of polynomial in eps */
        mult *= eps;
        c[l] = mult * polyval(m, g->C3x + o, eps);
        o += m + 1;
    }
}


double CSE_GeoArithmetic::A3f(const struct geod_geodesic* g, double eps) {
	/* Evaluate A3 */
	return polyval(nA3 - 1, g->A3x, eps);
}

double CSE_GeoArithmetic::AngRound(double x) {
	const double z = 1 / (double)(16);
	volatile double y;
	if (x == 0) return 0;
	y = fabs(x);
	/* The compiler mustn't "simplify" z - (z - y) to y */
	y = y < z ? z - (z - y) : y;
	return x < 0 ? -y : y;
}

double CSE_GeoArithmetic::A1m1f(double eps) {
	static const double coeff[] = {
		/* (1-eps)*A1-1, polynomial in eps2 of order 3 */
		1, 4, 64, 0, 256,
	};
	int m = nA1 / 2;
	double t = polyval(m, coeff, sq(eps)) / coeff[m + 1];
	return (t + eps) / (1 - eps);
}


void CSE_GeoArithmetic::norm2(double* sinx, double* cosx) {
#if defined(_MSC_VER) && defined(_M_IX86)

    double r = sqrt(*sinx * *sinx + *cosx * *cosx);
#else
    double r = hypot(*sinx, *cosx);
#endif
    * sinx /= r;
    *cosx /= r;
}

void CSE_GeoArithmetic::C1f(double eps, double c[]) {
	static const double coeff[] = {
		/* C1[1]/eps^1, polynomial in eps2 of order 2 */
		-1, 6, -16, 32,
		/* C1[2]/eps^2, polynomial in eps2 of order 2 */
		-9, 64, -128, 2048,
		/* C1[3]/eps^3, polynomial in eps2 of order 1 */
		9, -16, 768,
		/* C1[4]/eps^4, polynomial in eps2 of order 1 */
		3, -5, 512,
		/* C1[5]/eps^5, polynomial in eps2 of order 0 */
		-7, 1280,
		/* C1[6]/eps^6, polynomial in eps2 of order 0 */
		-7, 2048,
	};
	double
		eps2 = sq(eps),
		d = eps;
	int o = 0, l;
	for (l = 1; l <= nC1; ++l) {  /* l is index of C1p[l] */
		int m = (nC1 - l) / 2;      /* order of polynomial in eps^2 */
		c[l] = d * polyval(m, coeff + o, eps2) / coeff[o + m + 1];
		o += m + 2;
		d *= eps;
	}
}

void CSE_GeoArithmetic::C1pf(double eps, double c[]) {
	static const double coeff[] = {
		/* C1p[1]/eps^1, polynomial in eps2 of order 2 */
		205, -432, 768, 1536,
		/* C1p[2]/eps^2, polynomial in eps2 of order 2 */
		4005, -4736, 3840, 12288,
		/* C1p[3]/eps^3, polynomial in eps2 of order 1 */
		-225, 116, 384,
		/* C1p[4]/eps^4, polynomial in eps2 of order 1 */
		-7173, 2695, 7680,
		/* C1p[5]/eps^5, polynomial in eps2 of order 0 */
		3467, 7680,
		/* C1p[6]/eps^6, polynomial in eps2 of order 0 */
		38081, 61440,
	};
	double
		eps2 = sq(eps),
		d = eps;
	int o = 0, l;
	for (l = 1; l <= nC1p; ++l) { /* l is index of C1p[l] */
		int m = (nC1p - l) / 2;     /* order of polynomial in eps^2 */
		c[l] = d * polyval(m, coeff + o, eps2) / coeff[o + m + 1];
		o += m + 2;
		d *= eps;
	}
}


void CSE_GeoArithmetic::geod_init(struct geod_geodesic* g, double a, double f) {
    if (!init) Init();
    g->a = a;
    g->f = f;
    g->f1 = 1 - g->f;
    g->e2 = g->f * (2 - g->f);
    g->ep2 = g->e2 / sq(g->f1);   /* e2 / (1 - e2) */
    g->n = g->f / (2 - g->f);
    g->b = g->a * g->f1;
    g->c2 = (sq(g->a) + sq(g->b) *
        (g->e2 == 0 ? 1 :
            (g->e2 > 0 ? atanh(sqrt(g->e2)) : atan(sqrt(-g->e2))) /
            sqrt(fabs(g->e2)))) / 2; /* authalic radius squared */
  /* The sig12 threshold for "doublely short".  Using the auxiliary sphere
   * solution with dnm computed at (bet1 + bet2) / 2, the relative error in the
   * azimuth consistency check is sig12^2 * abs(f) * min(1, 1-f/2) / 2.  (Error
   * measured for 1/100 < b/a < 100 and abs(f) >= 1/1000.  For a given f and
   * sig12, the max error occurs for lines near the pole.  If the old rule for
   * computing dnm = (dn1 + dn2)/2 is used, then the error increases by a
   * factor of 2.)  Setting this equal to epsilon gives sig12 = etol2.  Here
   * 0.1 is a safety factor (error decreased by 100) and max(0.001, abs(f))
   * stops etol2 getting too large in the nearly spherical case. */
    g->etol2 = 0.1 * tol2 /
        sqrt(maxx((double)(0.001), fabs(g->f)) * minx((double)(1), 1 - g->f / 2) / 2);

    A3coeff(g);
    C3coeff(g);
    C4coeff(g);
}


void CSE_GeoArithmetic::A3coeff(struct geod_geodesic* g) {
	static const double coeff[] = {
		/* A3, coeff of eps^5, polynomial in n of order 0 */
		-3, 128,
		/* A3, coeff of eps^4, polynomial in n of order 1 */
		-2, -3, 64,
		/* A3, coeff of eps^3, polynomial in n of order 2 */
		-1, -3, -1, 16,
		/* A3, coeff of eps^2, polynomial in n of order 2 */
		3, -1, -2, 8,
		/* A3, coeff of eps^1, polynomial in n of order 1 */
		1, -1, 2,
		/* A3, coeff of eps^0, polynomial in n of order 0 */
		1, 1,
	};
	int o = 0, k = 0, j;
	for (j = nA3 - 1; j >= 0; --j) {             /* coeff of eps^j */
		int m = nA3 - j - 1 < j ? nA3 - j - 1 : j; /* order of polynomial in n */
		g->A3x[k++] = polyval(m, coeff + o, g->n) / coeff[o + m + 1];
		o += m + 2;
	}
}


void CSE_GeoArithmetic::C3coeff(struct geod_geodesic* g) {
	static const double coeff[] = {
		/* C3[1], coeff of eps^5, polynomial in n of order 0 */
		3, 128,
		/* C3[1], coeff of eps^4, polynomial in n of order 1 */
		2, 5, 128,
		/* C3[1], coeff of eps^3, polynomial in n of order 2 */
		-1, 3, 3, 64,
		/* C3[1], coeff of eps^2, polynomial in n of order 2 */
		-1, 0, 1, 8,
		/* C3[1], coeff of eps^1, polynomial in n of order 1 */
		-1, 1, 4,
		/* C3[2], coeff of eps^5, polynomial in n of order 0 */
		5, 256,
		/* C3[2], coeff of eps^4, polynomial in n of order 1 */
		1, 3, 128,
		/* C3[2], coeff of eps^3, polynomial in n of order 2 */
		-3, -2, 3, 64,
		/* C3[2], coeff of eps^2, polynomial in n of order 2 */
		1, -3, 2, 32,
		/* C3[3], coeff of eps^5, polynomial in n of order 0 */
		7, 512,
		/* C3[3], coeff of eps^4, polynomial in n of order 1 */
		-10, 9, 384,
		/* C3[3], coeff of eps^3, polynomial in n of order 2 */
		5, -9, 5, 192,
		/* C3[4], coeff of eps^5, polynomial in n of order 0 */
		7, 512,
		/* C3[4], coeff of eps^4, polynomial in n of order 1 */
		-14, 7, 512,
		/* C3[5], coeff of eps^5, polynomial in n of order 0 */
		21, 2560,
	};
	int o = 0, k = 0, l, j;
	for (l = 1; l < nC3; ++l) {                    /* l is index of C3[l] */
		for (j = nC3 - 1; j >= l; --j) {             /* coeff of eps^j */
			int m = nC3 - j - 1 < j ? nC3 - j - 1 : j; /* order of polynomial in n */
			g->C3x[k++] = polyval(m, coeff + o, g->n) / coeff[o + m + 1];
			o += m + 2;
		}
	}
}



void CSE_GeoArithmetic::C4coeff(struct geod_geodesic* g) {
	static const double coeff[] = {
		/* C4[0], coeff of eps^5, polynomial in n of order 0 */
		97, 15015,
		/* C4[0], coeff of eps^4, polynomial in n of order 1 */
		1088, 156, 45045,
		/* C4[0], coeff of eps^3, polynomial in n of order 2 */
		-224, -4784, 1573, 45045,
		/* C4[0], coeff of eps^2, polynomial in n of order 3 */
		-10656, 14144, -4576, -858, 45045,
		/* C4[0], coeff of eps^1, polynomial in n of order 4 */
		64, 624, -4576, 6864, -3003, 15015,
		/* C4[0], coeff of eps^0, polynomial in n of order 5 */
		100, 208, 572, 3432, -12012, 30030, 45045,
		/* C4[1], coeff of eps^5, polynomial in n of order 0 */
		1, 9009,
		/* C4[1], coeff of eps^4, polynomial in n of order 1 */
		-2944, 468, 135135,
		/* C4[1], coeff of eps^3, polynomial in n of order 2 */
		5792, 1040, -1287, 135135,
		/* C4[1], coeff of eps^2, polynomial in n of order 3 */
		5952, -11648, 9152, -2574, 135135,
		/* C4[1], coeff of eps^1, polynomial in n of order 4 */
		-64, -624, 4576, -6864, 3003, 135135,
		/* C4[2], coeff of eps^5, polynomial in n of order 0 */
		8, 10725,
		/* C4[2], coeff of eps^4, polynomial in n of order 1 */
		1856, -936, 225225,
		/* C4[2], coeff of eps^3, polynomial in n of order 2 */
		-8448, 4992, -1144, 225225,
		/* C4[2], coeff of eps^2, polynomial in n of order 3 */
		-1440, 4160, -4576, 1716, 225225,
		/* C4[3], coeff of eps^5, polynomial in n of order 0 */
		-136, 63063,
		/* C4[3], coeff of eps^4, polynomial in n of order 1 */
		1024, -208, 105105,
		/* C4[3], coeff of eps^3, polynomial in n of order 2 */
		3584, -3328, 1144, 315315,
		/* C4[4], coeff of eps^5, polynomial in n of order 0 */
		-128, 135135,
		/* C4[4], coeff of eps^4, polynomial in n of order 1 */
		-2560, 832, 405405,
		/* C4[5], coeff of eps^5, polynomial in n of order 0 */
		128, 99099,
	};
	int o = 0, k = 0, l, j;
	for (l = 0; l < nC4; ++l) {        /* l is index of C4[l] */
		for (j = nC4 - 1; j >= l; --j) { /* coeff of eps^j */
			int m = nC4 - j - 1;           /* order of polynomial in n */
			g->C4x[k++] = polyval(m, coeff + o, g->n) / coeff[o + m + 1];
			o += m + 2;
		}
	}
}

double CSE_GeoArithmetic::sumx(double u, double v, double* t) {
    volatile double s = u + v;
    volatile double up = s - v;
    volatile double vpp = s - up;
    up -= u;
    vpp -= v;
    if (t) *t = -(up + vpp);
    return s;
}

double CSE_GeoArithmetic::AngDiff(double x, double y, double* e) 
{
    double t = 0;
    double d = AngNormalize(sumx(AngNormalize(-x), AngNormalize(y), &t));
    
    return sumx(d == 180 && t > 0 ? -180 : d, t, e);
}

void CSE_GeoArithmetic::Lengths(const struct geod_geodesic* g,
    double eps, double sig12,
    double ssig1, double csig1, double dn1,
    double ssig2, double csig2, double dn2,
    double cbet1, double cbet2,
    double* ps12b, double* pm12b, double* pm0,
    double* pM12, double* pM21,
    /* Scratch area of the right size */
    double Ca[]) {
    double m0 = 0, J12 = 0, A1 = 0, A2 = 0;
    double Cb[nC];

    /* Return m12b = (reduced length)/b; also calculate s12b = distance/b,
     * and m0 = coefficient of secular term in expression for reduced length. */
    boolx redlp = pm12b || pm0 || pM12 || pM21;
    if (ps12b || redlp) {
        A1 = A1m1f(eps);
        C1f(eps, Ca);
        if (redlp) {
            A2 = A2m1f(eps);
            C2f(eps, Cb);
            m0 = A1 - A2;
            A2 = 1 + A2;
        }
        A1 = 1 + A1;
    }
    if (ps12b) {
        double B1 = SinCosSeries(TRUE, ssig2, csig2, Ca, nC1) -
            SinCosSeries(TRUE, ssig1, csig1, Ca, nC1);
        /* Missing a factor of b */
        *ps12b = A1 * (sig12 + B1);
        if (redlp) {
            double B2 = SinCosSeries(TRUE, ssig2, csig2, Cb, nC2) -
                SinCosSeries(TRUE, ssig1, csig1, Cb, nC2);
            J12 = m0 * sig12 + (A1 * B1 - A2 * B2);
        }
    }
    else if (redlp) {
        /* Assume here that nC1 >= nC2 */
        int l;
        for (l = 1; l <= nC2; ++l)
            Cb[l] = A1 * Ca[l] - A2 * Cb[l];
        J12 = m0 * sig12 + (SinCosSeries(TRUE, ssig2, csig2, Cb, nC2) -
            SinCosSeries(TRUE, ssig1, csig1, Cb, nC2));
    }
    if (pm0) *pm0 = m0;
    if (pm12b)
        /* Missing a factor of b.
         * Add parens around (csig1 * ssig2) and (ssig1 * csig2) to ensure
         * accurate cancellation in the case of coincident points. */
        *pm12b = dn2 * (csig1 * ssig2) - dn1 * (ssig1 * csig2) -
        csig1 * csig2 * J12;
    if (pM12 || pM21) {
        double csig12 = csig1 * csig2 + ssig1 * ssig2;
        double t = g->ep2 * (cbet1 - cbet2) * (cbet1 + cbet2) / (dn1 + dn2);
        if (pM12)
            *pM12 = csig12 + (t * ssig2 - csig2 * J12) * ssig1 / dn1;
        if (pM21)
            *pM21 = csig12 - (t * ssig1 - csig1 * J12) * ssig2 / dn2;
    }
}


double CSE_GeoArithmetic::InverseStart(const struct geod_geodesic* g,
    double sbet1, double cbet1, double dn1,
    double sbet2, double cbet2, double dn2,
    double lam12, double slam12, double clam12,
    double* psalp1, double* pcalp1,
    /* Only updated if return val >= 0 */
    double* psalp2, double* pcalp2,
    /* Only updated for short lines */
    double* pdnm,
    /* Scratch area of the right size */
    double Ca[]) {
    double salp1 = 0, calp1 = 0, salp2 = 0, calp2 = 0, dnm = 0;

    /* Return a starting point for Newton's method in salp1 and calp1 (function
     * value is -1).  If Newton's method doesn't need to be used, return also
     * salp2 and calp2 and function value is sig12. */
    double
        sig12 = -1,               /* Return value */
        /* bet12 = bet2 - bet1 in [0, pi); bet12a = bet2 + bet1 in (-pi, 0] */
        sbet12 = sbet2 * cbet1 - cbet2 * sbet1,
        cbet12 = cbet2 * cbet1 + sbet2 * sbet1;
    double sbet12a;
    boolx shortline = cbet12 >= 0 && sbet12 < (double)(0.5) &&
        cbet2* lam12 < (double)(0.5);
    double somg12, comg12, ssig12, csig12;
    sbet12a = sbet2 * cbet1 + cbet2 * sbet1;
    if (shortline) {
        double sbetm2 = sq(sbet1 + sbet2), omg12;
        /* sin((bet1+bet2)/2)^2
         * =  (sbet1 + sbet2)^2 / ((sbet1 + sbet2)^2 + (cbet1 + cbet2)^2) */
        sbetm2 /= sbetm2 + sq(cbet1 + cbet2);
        dnm = sqrt(1 + g->ep2 * sbetm2);
        omg12 = lam12 / (g->f1 * dnm);
        somg12 = sin(omg12); comg12 = cos(omg12);
    }
    else {
        somg12 = slam12; comg12 = clam12;
    }

    salp1 = cbet2 * somg12;
    calp1 = comg12 >= 0 ?
        sbet12 + cbet2 * sbet1 * sq(somg12) / (1 + comg12) :
        sbet12a - cbet2 * sbet1 * sq(somg12) / (1 - comg12);

    ssig12 = hypot(salp1, calp1);
    csig12 = sbet1 * sbet2 + cbet1 * cbet2 * comg12;

    if (shortline && ssig12 < g->etol2) {
        /* really short lines */
        salp2 = cbet1 * somg12;
        calp2 = sbet12 - cbet1 * sbet2 *
            (comg12 >= 0 ? sq(somg12) / (1 + comg12) : 1 - comg12);
        norm2(&salp2, &calp2);
        /* Set return value */
        sig12 = atan2(ssig12, csig12);
    }
    else if (fabs(g->n) > (double)(0.1) || /* No astroid calc if too eccentric */
        csig12 >= 0 ||
        ssig12 >= 6 * fabs(g->n) * pi * sq(cbet1)) {
        /* Nothing to do, zeroth order spherical approximation is OK */
    }
    else {
        /* Scale lam12 and bet2 to x, y coordinate system where antipodal point
         * is at origin and singular point is at y = 0, x = -1. */
        double y, lamscale, betscale;
        /* Volatile declaration needed to fix inverse case
         * 56.320923501171 0 -56.320923501171 179.664747671772880215
         * which otherwise fails with g++ 4.4.4 x86 -O3 */
        volatile double x;
        double lam12x = atan2(-slam12, -clam12); /* lam12 - pi */
        if (g->f >= 0) {            /* In fact f == 0 does not get here */
          /* x = dlong, y = dlat */
            {
                double
                    k2 = sq(sbet1) * g->ep2,
                    eps = k2 / (2 * (1 + sqrt(1 + k2)) + k2);
                lamscale = g->f * cbet1 * A3f(g, eps) * pi;
            }
            betscale = lamscale * cbet1;

            x = lam12x / lamscale;
            y = sbet12a / betscale;
        }
        else {                    /* f < 0 */
       /* x = dlat, y = dlong */
            double
                cbet12a = cbet2 * cbet1 - sbet2 * sbet1,
                bet12a = atan2(sbet12a, cbet12a);
            double m12b, m0;
            /* In the case of lon12 = 180, this repeats a calculation made in
             * Inverse. */
            Lengths(g, g->n, pi + bet12a,
                sbet1, -cbet1, dn1, sbet2, cbet2, dn2,
                cbet1, cbet2, nullptr, &m12b, &m0, nullptr, nullptr, Ca);
            x = -1 + m12b / (cbet1 * cbet2 * m0 * pi);
            betscale = x < -(double)(0.01) ? sbet12a / x :
                -g->f * sq(cbet1) * pi;
            lamscale = betscale / cbet1;
            y = lam12x / lamscale;
        }

        if (y > -tol1 && x > -1 - xthresh) {
            /* strip near cut */
            if (g->f >= 0) {
                salp1 = minx((double)(1), -(double)(x)); calp1 = -sqrt(1 - sq(salp1));
            }
            else {
                calp1 = maxx((double)(x > -tol1 ? 0 : -1), (double)(x));
                salp1 = sqrt(1 - sq(calp1));
            }
        }
        else {
            /* Estimate alp1, by solving the astroid problem.
             *
             * Could estimate alpha1 = theta + pi/2, directly, i.e.,
             *   calp1 = y/k; salp1 = -x/(1+k);  for f >= 0
             *   calp1 = x/(1+k); salp1 = -y/k;  for f < 0 (need to check)
             *
             * However, it's better to estimate omg12 from astroid and use
             * spherical formula to compute alp1.  This reduces the mean number of
             * Newton iterations for astroid cases from 2.24 (min 0, max 6) to 2.12
             * (min 0 max 5).  The changes in the number of iterations are as
             * follows:
             *
             * change percent
             *    1       5
             *    0      78
             *   -1      16
             *   -2       0.6
             *   -3       0.04
             *   -4       0.002
             *
             * The histogram of iterations is (m = number of iterations estimating
             * alp1 directly, n = number of iterations estimating via omg12, total
             * number of trials = 148605):
             *
             *  iter    m      n
             *    0   148    186
             *    1 13046  13845
             *    2 93315 102225
             *    3 36189  32341
             *    4  5396      7
             *    5   455      1
             *    6    56      0
             *
             * Because omg12 is near pi, estimate work with omg12a = pi - omg12 */
            double k = Astroid(x, y);
            double
                omg12a = lamscale * (g->f >= 0 ? -x * k / (1 + k) : -y * (1 + k) / k);
            somg12 = sin(omg12a); comg12 = -cos(omg12a);
            /* Update spherical estimate of alp1 using omg12 instead of lam12 */
            salp1 = cbet2 * somg12;
            calp1 = sbet12a - cbet2 * sbet1 * sq(somg12) / (1 - comg12);
        }
    }
    /* Sanity check on starting guess.  Backwards check allows NaN through. */
    if (!(salp1 <= 0))
        norm2(&salp1, &calp1);
    else {
        salp1 = 1; calp1 = 0;
    }

    *psalp1 = salp1;
    *pcalp1 = calp1;
    if (shortline)
        *pdnm = dnm;
    if (sig12 >= 0) {
        *psalp2 = salp2;
        *pcalp2 = calp2;
    }
    return sig12;
}

double CSE_GeoArithmetic::Astroid(double x, double y) {
    /* Solve k^4+2*k^3-(x^2+y^2-1)*k^2-2*y^2*k-y^2 = 0 for positive root k.
     * This solution is adapted from Geocentric::Reverse. */
    double k;
    double
        p = sq(x),
        q = sq(y),
        r = (p + q - 1) / 6;
    if (!(q == 0 && r <= 0)) {
        double
            /* Avoid possible division by zero when r = 0 by multiplying equations
             * for s and t by r^3 and r, resp. */
            S = p * q / 4,            /* S = r^3 * s */
            r2 = sq(r),
            r3 = r * r2,
            /* The discriminant of the quadratic equation for T3.  This is zero on
             * the evolute curve p^(1/3)+q^(1/3) = 1 */
            disc = S * (S + 2 * r3);
        double u = r;
        double v, uv, w;
        if (disc >= 0) {
            double T3 = S + r3, T;
            /* Pick the sign on the sqrt to maximize abs(T3).  This minimizes loss
             * of precision due to cancellation.  The result is unchanged because
             * of the way the T is used in definition of u. */
            T3 += T3 < 0 ? -sqrt(disc) : sqrt(disc); /* T3 = (r * t)^3 */
            /* N.B. cbrt always returns the double root.  cbrt(-8) = -2. */
            T = cbrt(T3);            /* T = r * t */
            /* T can be zero; but then r2 / T -> 0. */
            u += T + (T != 0 ? r2 / T : 0);
        }
        else {
            /* T is complex, but the way u is defined the result is double. */
            double ang = atan2(sqrt(-disc), -(S + r3));
            /* There are three possible cube roots.  We choose the root which
             * avoids cancellation.  Note that disc < 0 implies that r < 0. */
            u += 2 * r * cos(ang / 3);
        }
        v = sqrt(sq(u) + q);              /* guaranteed positive */
        /* Avoid loss of accuracy when u < 0. */
        uv = u < 0 ? q / (v - u) : u + v; /* u+v, guaranteed positive */
        w = (uv - q) / (2 * v);           /* positive? */
        /* Rearrange expression for k to avoid loss of accuracy due to
         * subtraction.  Division by 0 not possible because uv > 0, w >= 0. */
        k = uv / (sqrt(uv + sq(w)) + w);   /* guaranteed positive */
    }
    else {               /* q == 0 && r <= 0 */
   /* y = 0 with |x| <= 1.  Handle this case directly.
    * for y small, positive root is k = abs(y)/sqrt(1-x^2) */
        k = 0;
    }
    return k;
}

double CSE_GeoArithmetic::geod_geninverse_int(const struct geod_geodesic* g,
    double lat1, double lon1, double lat2, double lon2,
    double* ps12,
    double* psalp1, double* pcalp1,
    double* psalp2, double* pcalp2,
    double* pm12, double* pM12, double* pM21,
    double* pS12) {
    double s12 = 0, m12 = 0, M12 = 0, M21 = 0, S12 = 0;
    double lon12, lon12s;
    int latsign, lonsign, swapp;
    double sbet1, cbet1, sbet2, cbet2, s12x = 0, m12x = 0;
    double dn1, dn2, lam12, slam12, clam12;
    double a12 = 0, sig12, calp1 = 0, salp1 = 0, calp2 = 0, salp2 = 0;
    double Ca[nC];
    boolx meridian;
    /* somg12 > 1 marks that it needs to be calculated */
    double omg12 = 0, somg12 = 2, comg12 = 0;

    unsigned outmask =
        (ps12 ? GEOD_DISTANCE : GEOD_NONE) |
        (pm12 ? GEOD_REDUCEDLENGTH : GEOD_NONE) |
        (pM12 || pM21 ? GEOD_GEODESICSCALE : GEOD_NONE) |
        (pS12 ? GEOD_AREA : GEOD_NONE);

    outmask &= OUT_ALL;
    lon12 = AngDiff(lon1, lon2, &lon12s);
    lonsign = lon12 >= 0 ? 1 : -1;
    lon12 = lonsign * AngRound(lon12);
    lon12s = AngRound((180 - lon12) - lonsign * lon12s);
    lam12 = lon12 * degree;
    if (lon12 > 90) {
        sincosdx(lon12s, &slam12, &clam12);
        clam12 = -clam12;
    }
    else
        sincosdx(lon12, &slam12, &clam12);

    lat1 = AngRound(LatFix(lat1));
    lat2 = AngRound(LatFix(lat2));

    swapp = fabs(lat1) < fabs(lat2) ? -1 : 1;
    if (swapp < 0) {
        lonsign *= -1;
        swapx(&lat1, &lat2);
    }

    latsign = lat1 < 0 ? 1 : -1;
    lat1 *= latsign;
    lat2 *= latsign;
   

    sincosdx(lat1, &sbet1, &cbet1); sbet1 *= g->f1;
    /* Ensure cbet1 = +epsilon at poles */
    norm2(&sbet1, &cbet1); cbet1 = maxx(tiny, cbet1);

    sincosdx(lat2, &sbet2, &cbet2); sbet2 *= g->f1;

    norm2(&sbet2, &cbet2); cbet2 = maxx(tiny, cbet2);

    if (cbet1 < -sbet1) {
        if (cbet2 == cbet1)
            sbet2 = sbet2 < 0 ? sbet1 : -sbet1;
    }
    else {
        if (fabs(sbet2) == -sbet1)
            cbet2 = cbet1;
    }

    dn1 = sqrt(1 + g->ep2 * sq(sbet1));
    dn2 = sqrt(1 + g->ep2 * sq(sbet2));

    meridian = lat1 == -90 || slam12 == 0;

    if (meridian) {

        double ssig1, csig1, ssig2, csig2;
        calp1 = clam12; salp1 = slam12; /* Head to the target longitude */
        calp2 = 1; salp2 = 0;           /* At the target we're heading north */

        /* tan(bet) = tan(sig) * cos(alp) */
        ssig1 = sbet1; csig1 = calp1 * cbet1;
        ssig2 = sbet2; csig2 = calp2 * cbet2;

        sig12 = atan2(maxx((double)(0), csig1 * ssig2 - ssig1 * csig2),
            csig1 * csig2 + ssig1 * ssig2);
        Lengths(g, g->n, sig12, ssig1, csig1, dn1, ssig2, csig2, dn2,
            cbet1, cbet2, &s12x, &m12x, nullptr,
            (outmask & GEOD_GEODESICSCALE) ? &M12 : nullptr,
            (outmask & GEOD_GEODESICSCALE) ? &M21 : nullptr,
            Ca);

        if (sig12 < 1 || m12x >= 0) {
            /* Need at least 2, to handle 90 0 90 180 */
            if (sig12 < 3 * tiny ||
                // Prevent negative s12 or m12 for short lines
                (sig12 < tol0 && (s12x < 0 || m12x < 0)))
                sig12 = m12x = s12x = 0;
            m12x *= g->b;
            s12x *= g->b;
            a12 = sig12 / degree;
        }
        else
            meridian = FALSE;
    }

    if (!meridian &&
        sbet1 == 0 &&          
        (g->f <= 0 || lon12s >= g->f * 180)) {
        calp1 = calp2 = 0; salp1 = salp2 = 1;
        s12x = g->a * lam12;
        sig12 = omg12 = lam12 / g->f1;
        m12x = g->b * sin(sig12);
        if (outmask & GEOD_GEODESICSCALE)
            M12 = M21 = cos(sig12);
        a12 = lon12 / g->f1;

    }
    else if (!meridian) {
        double dnm = 0;
        sig12 = InverseStart(g, sbet1, cbet1, dn1, sbet2, cbet2, dn2,
            lam12, slam12, clam12,
            &salp1, &calp1, &salp2, &calp2, &dnm,
            Ca);

        if (sig12 >= 0) {
            /* Short lines (InverseStart sets salp2, calp2, dnm) */
            s12x = sig12 * g->b * dnm;
            m12x = sq(dnm) * g->b * sin(sig12 / dnm);
            if (outmask & GEOD_GEODESICSCALE)
                M12 = M21 = cos(sig12 / dnm);
            a12 = sig12 / degree;
            omg12 = lam12 / (g->f1 * dnm);
        }
        else {
            double ssig1 = 0, csig1 = 0, ssig2 = 0, csig2 = 0, eps = 0, domg12 = 0;
            unsigned numit = 0;
            /* Bracketing range */
            double salp1a = tiny, calp1a = 1, salp1b = tiny, calp1b = -1;
            boolx tripn = FALSE;
            boolx tripb = FALSE;
            for (; numit < maxit2; ++numit) {
                double dv = 0,
                    v = Lambda12(g, sbet1, cbet1, dn1, sbet2, cbet2, dn2, salp1, calp1,
                        slam12, clam12,
                        &salp2, &calp2, &sig12, &ssig1, &csig1, &ssig2, &csig2,
                        &eps, &domg12, numit < maxit1, &dv, Ca);
                if (tripb || !(fabs(v) >= (tripn ? 8 : 1) * tol0)) break;
                if (v > 0 && (numit > maxit1 || calp1 / salp1 > calp1b / salp1b))
                {
                    salp1b = salp1; calp1b = calp1;
                }
                else if (v < 0 && (numit > maxit1 || calp1 / salp1 < calp1a / salp1a))
                {
                    salp1a = salp1; calp1a = calp1;
                }
                if (numit < maxit1 && dv > 0) {
                    double
                        dalp1 = -v / dv;
                    double
                        sdalp1 = sin(dalp1), cdalp1 = cos(dalp1),
                        nsalp1 = salp1 * cdalp1 + calp1 * sdalp1;
                    if (nsalp1 > 0 && fabs(dalp1) < pi) {
                        calp1 = calp1 * cdalp1 - salp1 * sdalp1;
                        salp1 = nsalp1;
                        norm2(&salp1, &calp1);
                        tripn = fabs(v) <= 16 * tol0;
                        continue;
                    }
                }
                salp1 = (salp1a + salp1b) / 2;
                calp1 = (calp1a + calp1b) / 2;
                norm2(&salp1, &calp1);
                tripn = FALSE;
                tripb = (fabs(salp1a - salp1) + (calp1a - calp1) < tolb ||
                    fabs(salp1 - salp1b) + (calp1 - calp1b) < tolb);
            }
            Lengths(g, eps, sig12, ssig1, csig1, dn1, ssig2, csig2, dn2,
                cbet1, cbet2, &s12x, &m12x, nullptr,
                (outmask & GEOD_GEODESICSCALE) ? &M12 : nullptr,
                (outmask & GEOD_GEODESICSCALE) ? &M21 : nullptr, Ca);
            m12x *= g->b;
            s12x *= g->b;
            a12 = sig12 / degree;
            if (outmask & GEOD_AREA) {
                /* omg12 = lam12 - domg12 */
                double sdomg12 = sin(domg12), cdomg12 = cos(domg12);
                somg12 = slam12 * cdomg12 - clam12 * sdomg12;
                comg12 = clam12 * cdomg12 + slam12 * sdomg12;
            }
        }
    }

    if (outmask & GEOD_DISTANCE)
        s12 = 0 + s12x;            

    if (outmask & GEOD_REDUCEDLENGTH)
        m12 = 0 + m12x;           

    if (outmask & GEOD_AREA) {
        double
            salp0 = salp1 * cbet1,
            calp0 = hypot(calp1, salp1 * sbet1); 
        double alp12;
        if (calp0 != 0 && salp0 != 0) {
            double
                /* From Lambda12: tan(bet) = tan(sig) * cos(alp) */
                ssig1 = sbet1, csig1 = calp1 * cbet1,
                ssig2 = sbet2, csig2 = calp2 * cbet2,
                k2 = sq(calp0) * g->ep2,
                eps = k2 / (2 * (1 + sqrt(1 + k2)) + k2),
                /* Multiplier = a^2 * e^2 * cos(alpha0) * sin(alpha0). */
                A4 = sq(g->a) * calp0 * salp0 * g->e2;
            double B41, B42;
            norm2(&ssig1, &csig1);
            norm2(&ssig2, &csig2);
            C4f(g, eps, Ca);
            B41 = SinCosSeries(FALSE, ssig1, csig1, Ca, nC4);
            B42 = SinCosSeries(FALSE, ssig2, csig2, Ca, nC4);
            S12 = A4 * (B42 - B41);
        }
        else
            /* Avoid problems with indeterminate sig1, sig2 on equator */
            S12 = 0;

        if (!meridian && somg12 > 1) {
            somg12 = sin(omg12); comg12 = cos(omg12);
        }

        if (!meridian &&
            /* omg12 < 3/4 * pi */
            comg12 > -(double)(0.7071) &&     
            sbet2 - sbet1 < (double)(1.75)) 
        { 
            double
                domg12 = 1 + comg12, dbet1 = 1 + cbet1, dbet2 = 1 + cbet2;
            alp12 = 2 * atan2(somg12 * (sbet1 * dbet2 + sbet2 * dbet1),
                domg12 * (sbet1 * sbet2 + dbet1 * dbet2));
        }
        else {
            
            double
                salp12 = salp2 * calp1 - calp2 * salp1,
                calp12 = calp2 * calp1 + salp2 * salp1;

            if (salp12 == 0 && calp12 < 0) {
                salp12 = tiny * calp1;
                calp12 = -1;
            }
            alp12 = atan2(salp12, calp12);
        }
        S12 += g->c2 * alp12;
        S12 *= swapp * lonsign * latsign;
        /* Convert -0 to 0 */
        S12 += 0;
    }

    if (swapp < 0) {
        swapx(&salp1, &salp2);
        swapx(&calp1, &calp2);
        if (outmask & GEOD_GEODESICSCALE)
            swapx(&M12, &M21);
    }

    salp1 *= swapp * lonsign; calp1 *= swapp * latsign;
    salp2 *= swapp * lonsign; calp2 *= swapp * latsign;

    if (psalp1) *psalp1 = salp1;
    if (pcalp1) *pcalp1 = calp1;
    if (psalp2) *psalp2 = salp2;
    if (pcalp2) *pcalp2 = calp2;

    if (outmask & GEOD_DISTANCE)
        *ps12 = s12;
    if (outmask & GEOD_REDUCEDLENGTH)
        *pm12 = m12;
    if (outmask & GEOD_GEODESICSCALE) {
        if (pM12) *pM12 = M12;
        if (pM21) *pM21 = M21;
    }
    if (outmask & GEOD_AREA)
        *pS12 = S12;

    return a12;
}


double CSE_GeoArithmetic::Lambda12(const struct geod_geodesic* g,
    double sbet1, double cbet1, double dn1,
    double sbet2, double cbet2, double dn2,
    double salp1, double calp1,
    double slam120, double clam120,
    double* psalp2, double* pcalp2,
    double* psig12,
    double* pssig1, double* pcsig1,
    double* pssig2, double* pcsig2,
    double* peps,
    double* pdomg12,
    boolx diffp, double* pdlam12,
    /* Scratch area of the right size */
    double Ca[]) {
    double salp2 = 0, calp2 = 0, sig12 = 0,
        ssig1 = 0, csig1 = 0, ssig2 = 0, csig2 = 0, eps = 0,
        domg12 = 0, dlam12 = 0;
    double salp0, calp0;
    double somg1, comg1, somg2, comg2, somg12, comg12, lam12;
    double B312, eta, k2;

    if (sbet1 == 0 && calp1 == 0)
        /* Break degeneracy of equatorial line.  This case has already been
         * handled. */
        calp1 = -tiny;

    /* sin(alp1) * cos(bet1) = sin(alp0) */
    salp0 = salp1 * cbet1;
    calp0 = hypot(calp1, salp1 * sbet1); /* calp0 > 0 */

    /* tan(bet1) = tan(sig1) * cos(alp1)
     * tan(omg1) = sin(alp0) * tan(sig1) = tan(omg1)=tan(alp1)*sin(bet1) */
    ssig1 = sbet1; somg1 = salp0 * sbet1;
    csig1 = comg1 = calp1 * cbet1;
    norm2(&ssig1, &csig1);
    /* norm2(&somg1, &comg1); -- don't need to normalize! */

    /* Enforce symmetries in the case abs(bet2) = -bet1.  Need to be careful
     * about this case, since this can yield singularities in the Newton
     * iteration.
     * sin(alp2) * cos(bet2) = sin(alp0) */
    salp2 = cbet2 != cbet1 ? salp0 / cbet2 : salp1;
    /* calp2 = sqrt(1 - sq(salp2))
     *       = sqrt(sq(calp0) - sq(sbet2)) / cbet2
     * and subst for calp0 and rearrange to give (choose positive sqrt
     * to give alp2 in [0, pi/2]). */
    calp2 = cbet2 != cbet1 || fabs(sbet2) != -sbet1 ?
        sqrt(sq(calp1 * cbet1) +
            (cbet1 < -sbet1 ?
                (cbet2 - cbet1) * (cbet1 + cbet2) :
                (sbet1 - sbet2) * (sbet1 + sbet2))) / cbet2 :
        fabs(calp1);
    /* tan(bet2) = tan(sig2) * cos(alp2)
     * tan(omg2) = sin(alp0) * tan(sig2). */
    ssig2 = sbet2; somg2 = salp0 * sbet2;
    csig2 = comg2 = calp2 * cbet2;
    norm2(&ssig2, &csig2);
    /* norm2(&somg2, &comg2); -- don't need to normalize! */

    /* sig12 = sig2 - sig1, limit to [0, pi] */
    sig12 = atan2(maxx((double)(0), csig1 * ssig2 - ssig1 * csig2),
        csig1 * csig2 + ssig1 * ssig2);

    /* omg12 = omg2 - omg1, limit to [0, pi] */
    somg12 = maxx((double)(0), comg1 * somg2 - somg1 * comg2);
    comg12 = comg1 * comg2 + somg1 * somg2;
    /* eta = omg12 - lam120 */
    eta = atan2(somg12 * clam120 - comg12 * slam120,
        comg12 * clam120 + somg12 * slam120);
    k2 = sq(calp0) * g->ep2;
    eps = k2 / (2 * (1 + sqrt(1 + k2)) + k2);
    C3f(g, eps, Ca);
    B312 = (SinCosSeries(TRUE, ssig2, csig2, Ca, nC3 - 1) -
        SinCosSeries(TRUE, ssig1, csig1, Ca, nC3 - 1));
    domg12 = -g->f * A3f(g, eps) * salp0 * (sig12 + B312);
    lam12 = eta + domg12;

    if (diffp) {
        if (calp2 == 0)
            dlam12 = -2 * g->f1 * dn1 / sbet1;
        else {
            Lengths(g, eps, sig12, ssig1, csig1, dn1, ssig2, csig2, dn2,
                cbet1, cbet2, nullptr, &dlam12, nullptr, nullptr, nullptr, Ca);
            dlam12 *= g->f1 / (calp2 * cbet2);
        }
    }

    *psalp2 = salp2;
    *pcalp2 = calp2;
    *psig12 = sig12;
    *pssig1 = ssig1;
    *pcsig1 = csig1;
    *pssig2 = ssig2;
    *pcsig2 = csig2;
    *peps = eps;
    *pdomg12 = domg12;
    if (diffp)
        *pdlam12 = dlam12;

    return lam12;
}