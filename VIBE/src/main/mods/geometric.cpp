// geometric.cpp - VIBE
// Geometric Module - Crop, Zoom, Rotation (6 dials)

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace vibe
{
namespace mods
{

bool geometric(const View& in, View& out,
    Dial crop_top, Dial crop_right, Dial crop_bottom, Dial crop_left,
    Dial zoom_dial, Dial tilt_dial)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::geometric] invalid input\n";
        return false;
    }

    crop_top = std::clamp(crop_top, 0.0f, 1.0f);
    crop_right = std::clamp(crop_right, 0.0f, 1.0f);
    crop_bottom = std::clamp(crop_bottom, 0.0f, 1.0f);
    crop_left = std::clamp(crop_left, 0.0f, 1.0f);
    zoom_dial = std::clamp(zoom_dial, 0.0f, 1.0f);
    tilt_dial = std::clamp(tilt_dial, 0.0f, 1.0f);

    // Convert to working values
    float ct = crop_top * 0.5f;
    float cr = crop_right * 0.5f;
    float cb = crop_bottom * 0.5f;
    float cl = crop_left * 0.5f;
    float zoom = std::pow(4.0f, zoom_dial - 0.5f);
    float tilt = (tilt_dial - 0.5f) * 90.0f;

    bool needs_crop = (ct > 0.001f || cr > 0.001f || cb > 0.001f || cl > 0.001f);
    bool needs_zoom = (std::abs(zoom - 1.0f) > 0.001f);
    bool needs_tilt = (std::abs(tilt) > 0.01f);

    if (!needs_crop && !needs_zoom && !needs_tilt)
    {
        in.copyTo(out);
        return true;
    }

    View current;
    in.copyTo(current);
    int w = current.cols;
    int h = current.rows;

    // Crop
    if (needs_crop)
    {
        int x = static_cast<int>(w * cl);
        int y = static_cast<int>(h * ct);
        int nw = w - static_cast<int>(w * (cl + cr));
        int nh = h - static_cast<int>(h * (ct + cb));

        nw = std::max(1, nw);
        nh = std::max(1, nh);
        x = std::min(x, w - 1);
        y = std::min(y, h - 1);
        if (x + nw > w) nw = w - x;
        if (y + nh > h) nh = h - y;

        View cropped;
        current(cv::Rect(x, y, nw, nh)).copyTo(cropped);
        current = cropped;
        w = current.cols;
        h = current.rows;
    }

    // Zoom
    if (needs_zoom)
    {
        if (zoom > 1.0f)
        {
            int nw = std::max(1, static_cast<int>(w / zoom));
            int nh = std::max(1, static_cast<int>(h / zoom));
            int x = (w - nw) / 2;
            int y = (h - nh) / 2;

            View cropped;
            current(cv::Rect(x, y, nw, nh)).copyTo(cropped);
            cv::resize(cropped, current, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
        }
        else
        {
            int nw = std::max(1, static_cast<int>(w * zoom));
            int nh = std::max(1, static_cast<int>(h * zoom));

            View scaled;
            cv::resize(current, scaled, cv::Size(nw, nh), 0, 0, cv::INTER_AREA);

            View canvas(h, w, current.type(), cv::Scalar(0, 0, 0));
            int x = (w - nw) / 2;
            int y = (h - nh) / 2;
            scaled.copyTo(canvas(cv::Rect(x, y, nw, nh)));
            current = canvas;
        }
    }

    // Rotation
    if (needs_tilt)
    {
        cv::Point2f center(w / 2.0f, h / 2.0f);
        cv::Mat rot = cv::getRotationMatrix2D(center, tilt, 1.0);
        View rotated;
        cv::warpAffine(current, rotated, rot, cv::Size(w, h),
                      cv::INTER_LINEAR, cv::BORDER_REPLICATE);
        current = rotated;
    }

    current.copyTo(out);
    return true;
}

} // namespace mods
} // namespace vibe
