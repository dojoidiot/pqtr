#pragma once
#include <opencv2/core.hpp>
#include <vector>
#include <random>
#include <cmath>
#include <fstream>

namespace vibe {

// Simple MLP for dial prediction
// Architecture: input → hidden layers → output (sigmoid)
class MLP {
public:
    struct Layer {
        cv::Mat weights;  // [in × out]
        cv::Mat biases;   // [1 × out]
        cv::Mat z;        // pre-activation (for backprop)
        cv::Mat a;        // activation output
        cv::Mat dW;       // weight gradients
        cv::Mat db;       // bias gradients
    };

    std::vector<Layer> layers;
    std::vector<int> topology;

    MLP() = default;

    // Create network with given topology, e.g., {23, 128, 64, 45}
    void init(const std::vector<int>& topo) {
        topology = topo;
        layers.clear();

        std::mt19937 rng(42);

        for (size_t i = 0; i < topo.size() - 1; i++) {
            Layer layer;
            int in = topo[i];
            int out = topo[i + 1];

            // Xavier initialization
            float scale = std::sqrt(2.0f / (in + out));
            std::normal_distribution<float> dist(0.0f, scale);

            layer.weights = cv::Mat(in, out, CV_32F);
            layer.biases = cv::Mat(1, out, CV_32F, cv::Scalar(0));

            for (int r = 0; r < in; r++) {
                for (int c = 0; c < out; c++) {
                    layer.weights.at<float>(r, c) = dist(rng);
                }
            }

            layers.push_back(layer);
        }
    }

    // Forward pass: x is [batch × input_dim]
    cv::Mat forward(const cv::Mat& x) {
        cv::Mat current = x;

        for (size_t i = 0; i < layers.size(); i++) {
            Layer& layer = layers[i];

            // z = x @ W + b
            layer.z = current * layer.weights;
            for (int r = 0; r < layer.z.rows; r++) {
                layer.z.row(r) += layer.biases;
            }

            // Activation
            if (i < layers.size() - 1) {
                // Hidden: ReLU
                layer.a = relu(layer.z);
            } else {
                // Output: Sigmoid (dials are 0-1)
                layer.a = sigmoid(layer.z);
            }

            current = layer.a;
        }

        return current;
    }

    // Backward pass: compute gradients
    // target is [batch × output_dim]
    float backward(const cv::Mat& x, const cv::Mat& target, float lr) {
        int batch = x.rows;

        // Forward first
        cv::Mat pred = forward(x);

        // MSE loss
        cv::Mat diff = pred - target;
        float loss = cv::norm(diff, cv::NORM_L2SQR) / (batch * target.cols);

        // Output layer gradient: dL/dz = (pred - target) * sigmoid'(z)
        cv::Mat delta = diff.mul(sigmoidDeriv(layers.back().z));

        // Backprop through layers
        for (int i = layers.size() - 1; i >= 0; i--) {
            Layer& layer = layers[i];
            cv::Mat input = (i == 0) ? x : layers[i-1].a;

            // Gradients
            layer.dW = input.t() * delta / batch;
            cv::reduce(delta, layer.db, 0, cv::REDUCE_SUM);
            layer.db /= batch;

            // Propagate delta to previous layer
            if (i > 0) {
                cv::Mat delta_next = delta * layer.weights.t();
                delta_next = delta_next.mul(reluDeriv(layers[i-1].z));
                delta = delta_next;
            }
        }

        // Update weights
        for (auto& layer : layers) {
            layer.weights -= lr * layer.dW;
            layer.biases -= lr * layer.db;
        }

        return loss;
    }

    // Train on dataset
    void train(const cv::Mat& X, const cv::Mat& Y,
               int epochs, float lr, int batch_size = 32) {
        int n = X.rows;

        for (int epoch = 0; epoch < epochs; epoch++) {
            float total_loss = 0;
            int batches = 0;

            // Shuffle indices
            std::vector<int> idx(n);
            for (int i = 0; i < n; i++) idx[i] = i;
            std::shuffle(idx.begin(), idx.end(), std::mt19937(epoch));

            for (int start = 0; start < n; start += batch_size) {
                int end = std::min(start + batch_size, n);
                int bs = end - start;

                cv::Mat batch_X(bs, X.cols, CV_32F);
                cv::Mat batch_Y(bs, Y.cols, CV_32F);

                for (int i = 0; i < bs; i++) {
                    X.row(idx[start + i]).copyTo(batch_X.row(i));
                    Y.row(idx[start + i]).copyTo(batch_Y.row(i));
                }

                float loss = backward(batch_X, batch_Y, lr);
                total_loss += loss;
                batches++;
            }

            if (epoch % 100 == 0 || epoch == epochs - 1) {
                printf("Epoch %4d: loss = %.6f\n", epoch, total_loss / batches);
            }
        }
    }

    // Predict single sample
    std::vector<float> predict(const std::vector<float>& features) {
        cv::Mat x(1, features.size(), CV_32F);
        for (size_t i = 0; i < features.size(); i++) {
            x.at<float>(0, i) = features[i];
        }

        cv::Mat out = forward(x);

        std::vector<float> result(out.cols);
        for (int i = 0; i < out.cols; i++) {
            result[i] = out.at<float>(0, i);
        }
        return result;
    }

    // Save model
    void save(const std::string& path) {
        cv::FileStorage fs(path, cv::FileStorage::WRITE);
        fs << "topology" << topology;
        fs << "num_layers" << (int)layers.size();

        for (size_t i = 0; i < layers.size(); i++) {
            fs << ("weights_" + std::to_string(i)) << layers[i].weights;
            fs << ("biases_" + std::to_string(i)) << layers[i].biases;
        }
        fs.release();
    }

    // Load model
    bool load(const std::string& path) {
        cv::FileStorage fs(path, cv::FileStorage::READ);
        if (!fs.isOpened()) return false;

        fs["topology"] >> topology;
        int num_layers;
        fs["num_layers"] >> num_layers;

        layers.clear();
        layers.resize(num_layers);

        for (int i = 0; i < num_layers; i++) {
            fs["weights_" + std::to_string(i)] >> layers[i].weights;
            fs["biases_" + std::to_string(i)] >> layers[i].biases;
        }
        fs.release();
        return true;
    }

private:
    cv::Mat relu(const cv::Mat& x) {
        cv::Mat out;
        cv::max(x, 0, out);
        return out;
    }

    cv::Mat reluDeriv(const cv::Mat& x) {
        cv::Mat out = cv::Mat::zeros(x.size(), CV_32F);
        for (int r = 0; r < x.rows; r++) {
            for (int c = 0; c < x.cols; c++) {
                out.at<float>(r, c) = x.at<float>(r, c) > 0 ? 1.0f : 0.0f;
            }
        }
        return out;
    }

    cv::Mat sigmoid(const cv::Mat& x) {
        cv::Mat out(x.size(), CV_32F);
        for (int r = 0; r < x.rows; r++) {
            for (int c = 0; c < x.cols; c++) {
                float v = x.at<float>(r, c);
                out.at<float>(r, c) = 1.0f / (1.0f + std::exp(-v));
            }
        }
        return out;
    }

    cv::Mat sigmoidDeriv(const cv::Mat& x) {
        cv::Mat s = sigmoid(x);
        return s.mul(1.0f - s);
    }
};

} // namespace vibe
