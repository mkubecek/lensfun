/*
    Image modifier implementation: perspective correction functions
    Copyright (C) 2015 by Torsten Bronger <bronger@physik.rwth-aachen.de>
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>
#include <cmath>
#include <vector>
#include <numeric>
#include <iostream>
#include "windows/mathconstants.h"

fvector normalize (float x, float y)
{
    fvector result (2);
    float norm = sqrt (x * x + y * y);
    result [0] = x / norm;
    result [1] = y / norm;
    return result;
}

void central_projection (fvector coordinates, float plane_distance, float &x, float &y)
{
    float stretch_factor = plane_distance / coordinates [2];
    x = coordinates [0] * stretch_factor;
    y = coordinates [1] * stretch_factor;
}

typedef std::vector<fvector> matrix;

void ellipse_analysis (fvector x, fvector y, float f_normalized, float &x_v, float &y_v,
                       float &center_x, float &center_y)
{
    matrix M (12, fvector (6));
    for (int i = 0; i < 5; i++)
    {
        M [6 * i]     = x [i] * x [i];
        M [6 * i + 1] = x [i] * y [i];
        M [6 * i + 2] = y [i] * y [i];
        M [6 * i + 3] = x [i];
        M [6 * i + 4] = y [i];
        M [6 * i + 5] = 1;
    }
    fvector S2 = svd (M);
}

fvector rotate_rho_delta (float rho, float delta, float x, float y, float z)
{
}

void intersection (fvector x, fvector y, float &x_i, float &y_i)
{
}

float determine_rho_h (float rho, float delta, fvector x_perpendicular_line, fvector y_perpendicular_line, float f_normalized, float center_x, float center_y)
{
}

void calculate_angles(fvector x, fvector y, float f,
                      float normalized_in_millimeters, float &rho, float &delta, float &rho_h,
                      float &f_normalized, float &final_rotation, float &center_of_control_points_x,
                      float &center_of_control_points_y)
{
    const int number_of_control_points = x.size();

    float center_x, center_y;
    if (number_of_control_points == 6)
    {
        center_x = std::accumulate(x.begin(), x.begin() + 4, 0.) / 4;
        center_y = std::accumulate(y.begin(), y.begin() + 4, 0.) / 4;
    }
    else
    {
        center_x = std::accumulate(x.begin(), x.end(), 0.) / number_of_control_points;
        center_y = std::accumulate(y.begin(), y.end(), 0.) / number_of_control_points;
    }

    // FixMe: Is really always an f given (even if it is inaccurate)?
    f_normalized = f / normalized_in_millimeters;
    float x_v, y_v;
    if (number_of_control_points == 5 || number_of_control_points == 7)
        ellipse_analysis (fvector (x.begin(), x.begin() + 5), fvector (y.begin(), y.begin() + 5),
                          f_normalized, x_v, y_v, center_x, center_y);
    else
    {
        intersection (fvector (x.begin(), x.begin() + 4),
                      fvector (y.begin(), y.begin() + 4),
                      x_v, y_v);
        if (number_of_control_points == 8)
        {
            /* The problem is over-determined.  I prefer the fourth line over
               the focal length.  Maybe this is useful in cases where the focal
               length is not known. */
            float x_h, y_h;
            intersection (fvector (x.begin() + 4, x.begin() + 8),
                          fvector (y.begin() + 4, y.begin() + 8),
                          x_h, y_h);
            float radicand = - x_h * x_v - y_h * y_v;
            if (radicand >= 0)
                f_normalized = sqrt (radicand);
        }
    }

    rho = atan (- x_v / f_normalized);
    delta = M_PI_2 - atan (- y_v / sqrt (x_v * x_v + f_normalized * f_normalized));
    if (rotate_rho_delta (rho, delta, center_x, center_y, f_normalized) [2] < 0)
        // We have to move the vertex into the nadir instead of the zenith.
        delta -= M_PI;

    bool swapped_verticals_and_horizontals = false;

    fvector c(2);
    switch (number_of_control_points) {
    case 4:
    case 6:
    case 8:
    {
        fvector a = normalize (x_v - x [0], y_v - y [0]);
        fvector b = normalize (x_v - x [2], y_v - y [2]);
        c [0] = a [0] + b [0];
        c [1] = a [1] + b [1];
        break;
    }
    case 5:
    {
        c [0] = x_v - center_x;
        c [1] = y_v - center_y;
        break;
    }
    default:
    {
        c [0] = x [5] - x [6];
        c [1] = y [5] - y [6];
    }
    }
    float alpha;
    if (number_of_control_points == 7)
    {
        float x5_, y5_;
        central_projection (rotate_rho_delta (rho, delta, x [5], y [5], f_normalized), f_normalized, x5_, y5_);
        float x6_, y6_;
        central_projection (rotate_rho_delta (rho, delta, x [6], y [6], f_normalized), f_normalized, x6_, y6_);
        alpha = - atan2 (y6_ - y5_, x6_ - x5_);
        if (fabs (c [0]) > fabs (c [1]))
            // Find smallest rotation into horizontal
            alpha = - std::fmod (alpha - M_PI_2, M_PI) - M_PI_2;
        else
            // Find smallest rotation into vertical
            alpha = - std::fmod (alpha, M_PI) - M_PI_2;
    }
    else if (fabs (c [0]) > fabs (c [1]))
    {
        swapped_verticals_and_horizontals = true;
        alpha = rho > 0 ? M_PI_2 : - M_PI_2;
    }
    else
        alpha = 0;

    /* Calculate angle of intersection of horizontal great circle with equator,
       after the vertex was moved into the zenith */
    if (number_of_control_points == 4)
    {
        fvector x_perpendicular_line (2), y_perpendicular_line (2);
        if (swapped_verticals_and_horizontals)
        {
            x_perpendicular_line [0] = center_x;
            x_perpendicular_line [1] = center_x;
            y_perpendicular_line [0] = center_y - 1;
            y_perpendicular_line [1] = center_y + 1;
        }
        else
        {
            x_perpendicular_line [0] = center_x - 1;
            x_perpendicular_line [1] = center_x + 1;
            y_perpendicular_line [0] = center_y;
            y_perpendicular_line [1] = center_y;
        }
        rho_h = determine_rho_h (rho, delta, x_perpendicular_line, y_perpendicular_line, f_normalized, center_x, center_y);
        if (isnan (rho_h))
            rho_h = 0;
    }
    else if (number_of_control_points == 5 || number_of_control_points == 7)
        rho_h = 0;
    else
    {
        rho_h = determine_rho_h (rho, delta, fvector(x.begin() + 4, x.begin() + 6),
                                 fvector(y.begin() + 4, y.begin() + 6), f_normalized, center_x, center_y);
        if (isnan (rho_h))
            if (number_of_control_points == 8)
                rho_h = determine_rho_h (rho, delta, fvector(x.begin() + 6, x.begin() + 8),
                                         fvector(y.begin() + 6, y.begin() + 8), f_normalized, center_x, center_y);
            else
                rho_h = 0;
    }
}

bool lfModifier::enable_perspective_correction (fvector x, fvector y, float d)
{
    const int number_of_control_points = x.size();
    if (focal_length <= 0 || number_of_control_points != 4 &&
        number_of_control_points != 6 && number_of_control_points != 8)
        return false;
    if (d < -1)
        d = -1;
    if (d > 1)
        d = 1;
    for (int i = 0; i < number_of_control_points; i++)
    {
        x [i] = x [i] * NormScale - CenterX;
        y [i] = y [i] * NormScale - CenterY;
    }

    float rho, delta, rho_h, f_normalized, final_rotation, center_of_control_points_x,
        center_of_control_points_y;
    calculate_angles(x, y, focal_length, NormalizedInMillimeters,
                     rho, delta, rho_h, f_normalized, final_rotation, center_of_control_points_x,
                     center_of_control_points_y);
    
}

void lfModifier::ModifyCoord_Perspective_Correction (void *data, float *iocoord, int count)
{
    // Rd = Ru * (1 - k1 + k1 * Ru^2)
    const float k1 = *(float *)data;
    const float one_minus_k1 = 1.0 - k1;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float poly2 = one_minus_k1 + k1 * (x * x + y * y);

        iocoord [0] = x * poly2;
        iocoord [1] = y * poly2;
    }
}
