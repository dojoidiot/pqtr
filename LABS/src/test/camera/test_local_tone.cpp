// test_local_tone.cpp
// Test local tone mapping (Iridix-style) on DRO-heavy scenes
//
// Hypothesis: DSC01531's high error (~15%) is due to Sony's DRO
// applying local tone mapping that global transforms can't capture.
// This test applies our Iridix-based local tone mapper to see if
// it reduces the error.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include "mods/mods.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

float measure_error(const cv::Mat& a, const cv::Mat& b)
{
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    cv::Scalar mean = cv::mean(diff);
    return (mean[0] + mean[1] + mean[2]) / 3.0f * 100.0f;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Local Tone Mapping Test (Iridix-style) ===" << std::endl;
    std::cout << "File: " << raw_path << std::endl;

    // Decode RAW
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head)
    {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    // Get scene-linear and camera JPEG
    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    cv::Mat camera_jpeg;
    head->view().view().copyTo(camera_jpeg);

    // Resize scene-linear to match preview
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    // Apply gamma
    cv::Mat scene_gamma;
    cv::max(scene_resized, 0.0f, scene_gamma);
    cv::min(scene_gamma, 1.0f, scene_gamma);
    cv::pow(scene_gamma, 1.0f / 2.2f, scene_gamma);

    // Target in float
    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

    // Baseline error
    float error_baseline = measure_error(scene_gamma, target_f);
    std::cout << "\nBaseline error (gamma only): " << error_baseline << "%" << std::endl;

    // Test local tone mapping at different strengths
    std::cout << "\n=== Testing Local Tone Mapping ===" << std::endl;

    float best_error = error_baseline;
    float best_strength = 0.0f;
    float best_delta = 0.02f;
    float best_window = 0.1f;

    for (float strength : {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f})
    {
        for (float delta : {0.01f, 0.02f, 0.05f, 0.1f})
        {
            cv::UMat input_umat, output_umat;
            scene_gamma.copyTo(input_umat);

            if (pipe::mods::local_tone(input_umat, output_umat, strength, delta, 0.1f))
            {
                cv::Mat result;
                output_umat.copyTo(result);

                float error = measure_error(result, target_f);

                if (error < best_error)
                {
                    best_error = error;
                    best_strength = strength;
                    best_delta = delta;
                    std::cout << "  strength=" << strength << " delta=" << delta
                              << " -> error=" << error << "% (new best)" << std::endl;
                }
            }
        }
    }

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Baseline error: " << error_baseline << "%" << std::endl;
    std::cout << "Best local TM error: " << best_error << "%" << std::endl;
    std::cout << "  strength=" << best_strength << ", delta=" << best_delta << std::endl;
    std::cout << "Improvement: " << (error_baseline - best_error) << " percentage points" << std::endl;

    // Now try polynomial + local tone combined
    std::cout << "\n=== Testing Polynomial + Local Tone Combined ===" << std::endl;

    // Estimate polynomial coefficients
    float poly_coeffs[30];
    cv::UMat scene_umat, target_umat;
    scene_linear.copyTo(scene_umat);
    camera_jpeg.copyTo(target_umat);

    if (pipe::mods::estimate_poly_color(scene_umat, target_umat, poly_coeffs, 50000))
    {
        // Apply polynomial transform
        cv::UMat poly_input, poly_output;
        scene_gamma.copyTo(poly_input);

        if (pipe::mods::poly_color(poly_input, poly_output, poly_coeffs))
        {
            cv::Mat poly_result;
            poly_output.copyTo(poly_result);

            float error_poly = measure_error(poly_result, target_f);
            std::cout << "Polynomial only error: " << error_poly << "%" << std::endl;

            // Now apply local tone on top of polynomial
            cv::UMat lt_input, lt_output;
            poly_result.copyTo(lt_input);

            float combined_best = error_poly;
            float combined_strength = 0.0f;

            for (float strength : {0.1f, 0.2f, 0.3f, 0.4f, 0.5f})
            {
                if (pipe::mods::local_tone(lt_input, lt_output, strength, best_delta, 0.1f))
                {
                    cv::Mat combined_result;
                    lt_output.copyTo(combined_result);

                    float error = measure_error(combined_result, target_f);

                    if (error < combined_best)
                    {
                        combined_best = error;
                        combined_strength = strength;
                        std::cout << "  Poly+LocalTM strength=" << strength
                                  << " -> error=" << error << "%" << std::endl;
                    }
                }
            }

            std::cout << "\nFinal Results:" << std::endl;
            std::cout << "  Baseline (gamma): " << error_baseline << "%" << std::endl;
            std::cout << "  LocalTM alone: " << best_error << "%" << std::endl;
            std::cout << "  Polynomial alone: " << error_poly << "%" << std::endl;
            std::cout << "  Poly + LocalTM: " << combined_best << "%" << std::endl;

            // Save comparison image
            cv::UMat final_lt_output;
            if (pipe::mods::local_tone(lt_input, final_lt_output, combined_strength, best_delta, 0.1f))
            {
                cv::Mat final_result;
                final_lt_output.copyTo(final_result);

                cv::Mat result_8u;
                final_result.convertTo(result_8u, CV_8UC3, 255.0);

                cv::Mat comparison;
                cv::hconcat(camera_jpeg, result_8u, comparison);
                cv::imwrite("tmp/var/tune/local_tone_test.png", comparison);
                std::cout << "\nSaved: tmp/var/tune/local_tone_test.png" << std::endl;
            }
        }
    }

    return 0;
}
