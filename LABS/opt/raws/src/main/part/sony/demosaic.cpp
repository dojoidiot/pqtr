// demosaic.cpp
// Demosaic Module - Gold
// Custom float32 bilinear interpolation

#include "../sony.h"
#include <opencv2/core.hpp>
#include <iostream>

namespace sony
{
    namespace demosaic
    {

        bool process(
            const cv::UMat &input,
            cv::UMat &output,
            const RawMetadata &metadata)
        {
            if (input.empty())
            {
                std::cerr << "[Demosaic] Error: Input image is empty\n";
                return false;
            }

            if (input.type() != CV_32FC1)
            {
                std::cerr << "[Demosaic] Error: Input must be CV_32FC1\n";
                return false;
            }

            try
            {
                cv::Mat bayer_cpu = input.getMat(cv::ACCESS_READ);
                const int rows = bayer_cpu.rows;
                const int cols = bayer_cpu.cols;
                cv::Mat rgb_cpu(rows, cols, CV_32FC3);

                // Determine channel positions based on Bayer pattern
                int r_row = 0, r_col = 0;
                int b_row = 1, b_col = 1;

                switch (metadata.bayer_pattern)
                {
                case 46:
                    r_row = 0;
                    r_col = 0;
                    b_row = 1;
                    b_col = 1;
                    break; // RGGB
                case 47:
                    r_row = 0;
                    r_col = 1;
                    b_row = 1;
                    b_col = 0;
                    break; // GRBG
                case 48:
                    r_row = 1;
                    r_col = 1;
                    b_row = 0;
                    b_col = 0;
                    break; // BGGR
                case 49:
                    r_row = 1;
                    r_col = 0;
                    b_row = 0;
                    b_col = 1;
                    break; // GBRG
                default:
                    r_row = 0;
                    r_col = 0;
                    b_row = 1;
                    b_col = 1;
                    break;
                }

                // Bilinear interpolation for each pixel
                for (int row = 0; row < rows; ++row)
                {
                    for (int col = 0; col < cols; ++col)
                    {
                        float r = 0.0f, g = 0.0f, b = 0.0f;
                        int row_mod = row % 2;
                        int col_mod = col % 2;
                        bool is_r = (row_mod == r_row && col_mod == r_col);
                        bool is_b = (row_mod == b_row && col_mod == b_col);
                        float center = bayer_cpu.at<float>(row, col);

                        if (is_r)
                        {
                            r = center;
                            float g_sum = 0.0f;
                            int g_count = 0;
                            if (row > 0)
                            {
                                g_sum += bayer_cpu.at<float>(row - 1, col);
                                g_count++;
                            }
                            if (row < rows - 1)
                            {
                                g_sum += bayer_cpu.at<float>(row + 1, col);
                                g_count++;
                            }
                            if (col > 0)
                            {
                                g_sum += bayer_cpu.at<float>(row, col - 1);
                                g_count++;
                            }
                            if (col < cols - 1)
                            {
                                g_sum += bayer_cpu.at<float>(row, col + 1);
                                g_count++;
                            }
                            g = (g_count > 0) ? g_sum / g_count : center;

                            float b_sum = 0.0f;
                            int b_count = 0;
                            if (row > 0 && col > 0)
                            {
                                b_sum += bayer_cpu.at<float>(row - 1, col - 1);
                                b_count++;
                            }
                            if (row > 0 && col < cols - 1)
                            {
                                b_sum += bayer_cpu.at<float>(row - 1, col + 1);
                                b_count++;
                            }
                            if (row < rows - 1 && col > 0)
                            {
                                b_sum += bayer_cpu.at<float>(row + 1, col - 1);
                                b_count++;
                            }
                            if (row < rows - 1 && col < cols - 1)
                            {
                                b_sum += bayer_cpu.at<float>(row + 1, col + 1);
                                b_count++;
                            }
                            b = (b_count > 0) ? b_sum / b_count : center;
                        }
                        else if (is_b)
                        {
                            b = center;
                            float g_sum = 0.0f;
                            int g_count = 0;
                            if (row > 0)
                            {
                                g_sum += bayer_cpu.at<float>(row - 1, col);
                                g_count++;
                            }
                            if (row < rows - 1)
                            {
                                g_sum += bayer_cpu.at<float>(row + 1, col);
                                g_count++;
                            }
                            if (col > 0)
                            {
                                g_sum += bayer_cpu.at<float>(row, col - 1);
                                g_count++;
                            }
                            if (col < cols - 1)
                            {
                                g_sum += bayer_cpu.at<float>(row, col + 1);
                                g_count++;
                            }
                            g = (g_count > 0) ? g_sum / g_count : center;

                            float r_sum = 0.0f;
                            int r_count = 0;
                            if (row > 0 && col > 0)
                            {
                                r_sum += bayer_cpu.at<float>(row - 1, col - 1);
                                r_count++;
                            }
                            if (row > 0 && col < cols - 1)
                            {
                                r_sum += bayer_cpu.at<float>(row - 1, col + 1);
                                r_count++;
                            }
                            if (row < rows - 1 && col > 0)
                            {
                                r_sum += bayer_cpu.at<float>(row + 1, col - 1);
                                r_count++;
                            }
                            if (row < rows - 1 && col < cols - 1)
                            {
                                r_sum += bayer_cpu.at<float>(row + 1, col + 1);
                                r_count++;
                            }
                            r = (r_count > 0) ? r_sum / r_count : center;
                        }
                        else
                        {
                            g = center;
                            bool on_r_row = (row_mod == r_row);
                            if (on_r_row)
                            {
                                float r_sum = 0.0f;
                                int r_count = 0;
                                if (col > 0)
                                {
                                    r_sum += bayer_cpu.at<float>(row, col - 1);
                                    r_count++;
                                }
                                if (col < cols - 1)
                                {
                                    r_sum += bayer_cpu.at<float>(row, col + 1);
                                    r_count++;
                                }
                                r = (r_count > 0) ? r_sum / r_count : center;

                                float b_sum = 0.0f;
                                int b_count = 0;
                                if (row > 0)
                                {
                                    b_sum += bayer_cpu.at<float>(row - 1, col);
                                    b_count++;
                                }
                                if (row < rows - 1)
                                {
                                    b_sum += bayer_cpu.at<float>(row + 1, col);
                                    b_count++;
                                }
                                b = (b_count > 0) ? b_sum / b_count : center;
                            }
                            else
                            {
                                float b_sum = 0.0f;
                                int b_count = 0;
                                if (col > 0)
                                {
                                    b_sum += bayer_cpu.at<float>(row, col - 1);
                                    b_count++;
                                }
                                if (col < cols - 1)
                                {
                                    b_sum += bayer_cpu.at<float>(row, col + 1);
                                    b_count++;
                                }
                                b = (b_count > 0) ? b_sum / b_count : center;

                                float r_sum = 0.0f;
                                int r_count = 0;
                                if (row > 0)
                                {
                                    r_sum += bayer_cpu.at<float>(row - 1, col);
                                    r_count++;
                                }
                                if (row < rows - 1)
                                {
                                    r_sum += bayer_cpu.at<float>(row + 1, col);
                                    r_count++;
                                }
                                r = (r_count > 0) ? r_sum / r_count : center;
                            }
                        }
                        rgb_cpu.at<cv::Vec3f>(row, col) = cv::Vec3f(b, g, r); // BGR order
                    }
                }

                rgb_cpu.copyTo(output);
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[Demosaic] Error: " << e.what() << "\n";
                return false;
            }
        }

    } // namespace demosaic
} // namespace sony
