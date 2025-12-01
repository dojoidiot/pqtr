// color_check.cpp
// Check if the color matrix and white balance are correct
// Compare scene-linear color channels to camera JPEG

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Color Check ===" << std::endl;

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    cv::Mat camera_jpeg;
    head->view().view().copyTo(camera_jpeg);

    // Resize
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    // Convert to gamma for comparison
    cv::Mat scene_gamma;
    cv::max(scene_resized, 0.0f, scene_gamma);
    cv::min(scene_gamma, 1.0f, scene_gamma);
    cv::pow(scene_gamma, 1.0f/2.2f, scene_gamma);

    // Analyze channel means
    cv::Scalar scene_mean = cv::mean(scene_gamma);
    cv::Mat camera_f;
    camera_jpeg.convertTo(camera_f, CV_32FC3, 1.0/255.0);
    cv::Scalar camera_mean = cv::mean(camera_f);

    std::cout << "\nChannel means (BGR order):" << std::endl;
    std::cout << "  Scene:  B=" << scene_mean[0] << " G=" << scene_mean[1] << " R=" << scene_mean[2] << std::endl;
    std::cout << "  Camera: B=" << camera_mean[0] << " G=" << camera_mean[1] << " R=" << camera_mean[2] << std::endl;

    // Compute ratios
    std::cout << "\nCamera/Scene ratios:" << std::endl;
    std::cout << "  B: " << (camera_mean[0] / scene_mean[0]) << std::endl;
    std::cout << "  G: " << (camera_mean[1] / scene_mean[1]) << std::endl;
    std::cout << "  R: " << (camera_mean[2] / scene_mean[2]) << std::endl;

    // Check for cross-channel correlation
    // Sample 1000 random pixels
    std::cout << "\n--- Sampling 1000 random pixels ---" << std::endl;

    double corr_rr = 0, corr_rg = 0, corr_rb = 0;
    double corr_gr = 0, corr_gg = 0, corr_gb = 0;
    double corr_br = 0, corr_bg = 0, corr_bb = 0;
    int samples = 0;

    srand(42);
    for (int i = 0; i < 1000; i++) {
        int y = rand() % scene_gamma.rows;
        int x = rand() % scene_gamma.cols;

        const float* s = scene_gamma.ptr<float>(y) + x * 3;
        const float* c = camera_f.ptr<float>(y) + x * 3;

        // Accumulate products for cross-channel correlation
        corr_rr += s[2] * c[2];  // scene R vs camera R
        corr_rg += s[2] * c[1];  // scene R vs camera G
        corr_rb += s[2] * c[0];  // scene R vs camera B
        corr_gr += s[1] * c[2];  // scene G vs camera R
        corr_gg += s[1] * c[1];  // scene G vs camera G
        corr_gb += s[1] * c[0];  // scene G vs camera B
        corr_br += s[0] * c[2];  // scene B vs camera R
        corr_bg += s[0] * c[1];  // scene B vs camera G
        corr_bb += s[0] * c[0];  // scene B vs camera B

        samples++;
    }

    std::cout << "\nCross-channel correlation (scene × camera):" << std::endl;
    printf("        CamR    CamG    CamB\n");
    printf("SceR: %6.3f  %6.3f  %6.3f\n", corr_rr/samples, corr_rg/samples, corr_rb/samples);
    printf("SceG: %6.3f  %6.3f  %6.3f\n", corr_gr/samples, corr_gg/samples, corr_gb/samples);
    printf("SceB: %6.3f  %6.3f  %6.3f\n", corr_br/samples, corr_bg/samples, corr_bb/samples);

    // Check if there's a simple 3x3 matrix that could help
    // Estimate: Camera = M × Scene
    // Using least squares on the samples
    std::cout << "\n--- Estimating 3x3 Color Matrix ---" << std::endl;

    // Simplified: just compute channel-wise scaling
    cv::Mat scene_split[3], camera_split[3];
    cv::split(scene_gamma, scene_split);
    cv::split(camera_f, camera_split);

    // For each camera channel, find best linear combination of scene channels
    for (int c = 0; c < 3; c++) {
        double best_err = 1e9;
        float best_r = 1, best_g = 0, best_b = 0;

        // Grid search for weights (simplified)
        for (float wr = -0.5f; wr <= 1.5f; wr += 0.1f) {
            for (float wg = -0.5f; wg <= 1.5f; wg += 0.1f) {
                for (float wb = -0.5f; wb <= 1.5f; wb += 0.1f) {
                    cv::Mat combined = wr * scene_split[2] + wg * scene_split[1] + wb * scene_split[0];
                    cv::Mat diff;
                    cv::absdiff(combined, camera_split[c], diff);
                    double err = cv::mean(diff)[0];

                    if (err < best_err) {
                        best_err = err;
                        if (c == 0) { best_b = wb; best_g = wg; best_r = wr; }
                        else if (c == 1) { best_b = wb; best_g = wg; best_r = wr; }
                        else { best_b = wb; best_g = wg; best_r = wr; }
                    }
                }
            }
        }

        const char* names[] = {"Blue", "Green", "Red"};
        std::cout << "Camera " << names[c] << " = " << best_r << "*SceneR + "
                  << best_g << "*SceneG + " << best_b << "*SceneB  (err=" << best_err << ")" << std::endl;
    }

    // Save a simple comparison: just apply gamma and compare
    cv::Mat scene_8u;
    scene_gamma.convertTo(scene_8u, CV_8UC3, 255.0);

    cv::Mat comparison;
    cv::hconcat(camera_jpeg, scene_8u, comparison);
    cv::imwrite("tmp/var/tune/color_check.png", comparison);
    std::cout << "\nSaved: tmp/var/tune/color_check.png (left: camera, right: our scene-linear with gamma)" << std::endl;

    return 0;
}
