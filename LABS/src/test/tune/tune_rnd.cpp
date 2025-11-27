// tune_rnd.cpp
// Tune R&D test harness
// Loads DSC00202.ARW into a pipe with no edit steps
// Outputs at social media size (1080px):
//   1. head.png  - Camera embedded preview (target)
//   2. body.png  - Body view (no edit steps, scene-referred)
//   3. tail.png  - Final output from tail (should match head after tuning)
//   4. diff.png  - Visual difference between head and body
//   5. tune.json - Loss metrics (spectral + frequency)
//
// Usage: tune_rnd

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <tune.hpp>
#include <data.hpp>
#include <iostream>
#include <iomanip>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

constexpr int SOCIAL_SIZE = 1080;
const std::string OUTPUT_DIR = "tmp/var/tune/";

int main()
{
    const std::string rawPath = "var/pics/DSC00202.ARW";

    std::cout << "=== Tune R&D Test Harness ===" << std::endl;
    std::cout << "Loading: " << rawPath << std::endl;
    std::cout << "Output size: " << SOCIAL_SIZE << "px" << std::endl;

    try
    {
        // Create pipe instance
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();

        // Load RAW file into Sink
        pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(rawPath));
        std::cout << "  Sink size: " << rawSink->size() << " bytes" << std::endl;

        // HEAD: Decode RAW
        std::cout << "\n[HEAD] Decoding..." << std::endl;
        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
        if (!head)
        {
            throw std::runtime_error("Failed to decode RAW file");
        }

        // Get decoded metadata
        pipe::Info info = head->data().info();
        std::cout << "  Decoded: " << info["width"] << "x" << info["height"] << std::endl;
        std::cout << "  Camera: " << info["camera_make"] << " " << info["camera_model"] << std::endl;

        // 1. Save HEAD view (embedded camera preview) - THE TARGET
        std::cout << "\n[1] Saving head view (camera preview - target)..." << std::endl;
        pipe::View headView = head->view().view();
        cv::Mat headMat;
        headView.copyTo(headMat);

        // Resize head to match body size for fair comparison
        cv::Mat headResized;
        int headMaxDim = std::max(headMat.cols, headMat.rows);
        float headScale = static_cast<float>(SOCIAL_SIZE) / headMaxDim;
        cv::resize(headMat, headResized, cv::Size(), headScale, headScale, cv::INTER_AREA);

        std::string headPath = OUTPUT_DIR + "head.png";
        cv::imwrite(headPath, headResized);
        std::cout << "  Saved: " << headPath << " (" << headResized.cols << "x" << headResized.rows << ")" << std::endl;

        // BODY: Create with no edit steps, using social media working size
        std::cout << "\n[BODY] Creating empty body (no edit steps)..." << std::endl;
        pipe::Body& body = head->body(SOCIAL_SIZE);

        // Verify no links
        pipe::Body::Iterator& iter = body.links();
        int linkCount = 0;
        while (iter.next()) linkCount++;
        std::cout << "  Links: " << linkCount << std::endl;

        // 2. Save BODY view (scene-linear processed through body, gamma encoded)
        std::cout << "\n[2] Saving body view (no edit steps)..." << std::endl;
        pipe::View bodyView = body.view();
        cv::Mat bodyMat;
        bodyView.copyTo(bodyMat);
        std::string bodyPath = OUTPUT_DIR + "body.png";
        cv::imwrite(bodyPath, bodyMat);
        std::cout << "  Saved: " << bodyPath << " (" << bodyMat.cols << "x" << bodyMat.rows << ")" << std::endl;

        // 3. Save TAIL view (final output at social size)
        std::cout << "\n[3] Saving tail output (final)..." << std::endl;
        std::string tailPath = OUTPUT_DIR + "tail.png";
        if (!body.tail().save(tailPath, SOCIAL_SIZE))
        {
            throw std::runtime_error("Failed to save tail output");
        }
        std::cout << "  Saved: " << tailPath << std::endl;

        // 4. Compute diff between head (target) and body (candidate)
        std::cout << "\n[4] Computing diff (head vs body)..." << std::endl;

        // Resize head to exactly match body dimensions
        cv::Mat headForDiff;
        cv::resize(headResized, headForDiff, cv::Size(bodyMat.cols, bodyMat.rows), 0, 0, cv::INTER_AREA);

        cv::UMat headUMat, bodyUMat;
        headForDiff.copyTo(headUMat);
        bodyMat.copyTo(bodyUMat);

        // Create tune task with head as target (features cached)
        pqtr::Hold<tune::Task> tuneTask = tune::make(headUMat);

        // Compute metrics
        tune::Data metrics = tuneTask->diff(bodyUMat);
        std::cout << "  Spectral loss:  " << std::fixed << std::setprecision(4) << metrics.spectral
                  << " (" << std::setprecision(2) << (metrics.spectral * 100) << "%)" << std::endl;
        std::cout << "  Frequency loss: " << std::fixed << std::setprecision(4) << metrics.frequency
                  << " (" << std::setprecision(2) << (metrics.frequency * 100) << "%)" << std::endl;

        // 5. Save diff image
        std::cout << "\n[5] Saving diff image..." << std::endl;
        cv::UMat diffUMat = tuneTask->view(bodyUMat);
        cv::Mat diffMat;
        diffUMat.copyTo(diffMat);
        std::string diffImgPath = OUTPUT_DIR + "diff.png";
        cv::imwrite(diffImgPath, diffMat);
        std::cout << "  Saved: " << diffImgPath << std::endl;

        // 6. Save tune.json using data layer
        std::cout << "\n[6] Saving tune.json..." << std::endl;
        std::string tuneJsonPath = OUTPUT_DIR + "tune.json";
        if (!data::tune::save(metrics, tuneJsonPath))
        {
            throw std::runtime_error("Failed to save tune.json");
        }
        std::cout << "  Saved: " << tuneJsonPath << std::endl;

        std::cout << "\n[OK] All outputs saved to " << OUTPUT_DIR << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
