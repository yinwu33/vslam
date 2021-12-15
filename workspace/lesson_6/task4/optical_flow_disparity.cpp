#include <opencv2/opencv.hpp>
#include <string>
#include <Eigen/Core>
#include <Eigen/Dense>

using namespace std;
using namespace cv;

// this program shows how to use optical flow

// Camera intrinsics
// 内参
double fx = 718.856, fy = 718.856, cx = 607.1928, cy = 185.2157;
// 基线
double baseline = 0.573;

string file_1 = "../left.png";  // first image
string file_2 = "../right.png";  // second image
string file_3 = "../disparity.png";

// TODO implement this funciton
/**
 * single level optical flow`
 * @param [in] img1 the first image
 * @param [in] img2 the second image
 * @param [in] kp1 keypoints in img1
 * @param [in|out] kp2 keypoints in img2, if empty, use initial guess in kp1
 * @param [out] success true if a keypoint is tracked successfully
 * @param [in] inverse use inverse formulation?
 */
void OpticalFlowSingleLevel(
    const Mat& img1,
    const Mat& img2,
    const vector<KeyPoint>& kp1,
    vector<KeyPoint>& kp2,
    vector<bool>& success,
    bool inverse = false
);

// TODO implement this funciton
/**
 * multi level optical flow, scale of pyramid is set to 2 by default
 * the image pyramid will be create inside the function
 * @param [in] img1 the first pyramid
 * @param [in] img2 the second pyramid
 * @param [in] kp1 keypoints in img1
 * @param [out] kp2 keypoints in img2
 * @param [out] success true if a keypoint is tracked successfully
 * @param [in] inverse set true to enable inverse formulation
 */
void OpticalFlowMultiLevel(
    const Mat& img1,
    const Mat& img2,
    const vector<KeyPoint>& kp1,
    vector<KeyPoint>& kp2,
    vector<bool>& success,
    bool inverse = false
);

/**
 * get a gray scale value from reference image (bi-linear interpolated)
 * @param img
 * @param x
 * @param y
 * @return
 */
inline float GetPixelValue(const cv::Mat& img, float x, float y) {
    uchar* data = &img.data[int(y) * img.step + int(x)];
    float xx = x - floor(x);
    float yy = y - floor(y);
    return float(
        (1 - xx) * (1 - yy) * data[0] +
        xx * (1 - yy) * data[1] +
        (1 - xx) * yy * data[img.step] +
        xx * yy * data[img.step + 1]
        );
}


int main(int argc, char** argv) {

    // images, note they are CV_8UC1, not CV_8UC3
    Mat img1 = imread(file_1, CV_8UC1);
    Mat img2 = imread(file_2, CV_8UC1);
    Mat img3 = imread(file_3, CV_8UC1);  // disparity

    // key points, using GFTT here.
    vector<KeyPoint> kp1;
    Ptr<GFTTDetector> detector = GFTTDetector::create(500, 0.01, 20); // maximum 500 keypoints
    detector->detect(img1, kp1);

    // now lets track these key points in the second image
    // first use single level LK in the validation picture
    vector<KeyPoint> kp2_single;
    vector<bool> success_single;
    OpticalFlowSingleLevel(img1, img2, kp1, kp2_single, success_single, true);

    // then test multi-level LK
    vector<KeyPoint> kp2_multi;
    vector<bool> success_multi;
    OpticalFlowMultiLevel(img1, img2, kp1, kp2_multi, success_multi);

    // use opencv's flow for validation
    vector<Point2f> pt1, pt2;
    for (auto& kp : kp1) pt1.push_back(kp.pt);
    vector<uchar> status;
    vector<float> error;
    cv::calcOpticalFlowPyrLK(img1, img2, pt1, pt2, status, error, cv::Size(8, 8));

    // plot the differences of those functions
    Mat img2_single;
    cv::cvtColor(img2, img2_single, cv::COLOR_GRAY2BGR);
    double error_disparity = 0;
    for (int i = 0; i < kp2_single.size(); i++) {
        if (success_single[i]) {
            cv::circle(img2_single, kp2_single[i].pt, 2, cv::Scalar(0, 250, 0), 2);
            cv::line(img2_single, kp1[i].pt, kp2_single[i].pt, cv::Scalar(0, 250, 0));
            // compare optical flow result and disparity
            double estimate = (pt2[i] - pt1[i]).x;
            double groundtruth = img3.at<uchar>(pt1[i].y, pt1[i].x);
            error_disparity += (estimate - groundtruth);

        }
    }
    std::cout << "error by single: " << error_disparity / kp2_multi.size() << std::endl;

    Mat img2_multi;
    cv::cvtColor(img2, img2_multi, cv::COLOR_GRAY2BGR);
    error_disparity = 0;
    for (int i = 0; i < kp2_multi.size(); i++) {
        if (success_multi[i]) {
            cv::circle(img2_multi, kp2_multi[i].pt, 2, cv::Scalar(0, 250, 0), 2);
            cv::line(img2_multi, kp1[i].pt, kp2_multi[i].pt, cv::Scalar(0, 250, 0));
            // compare optical flow result and disparity
            double estimate = (pt2[i] - pt1[i]).x;
            double groundtruth = img3.at<uchar>(pt1[i].y, pt1[i].x);
            error_disparity += (estimate - groundtruth);
        }
    }
    std::cout << "error by multi: " << error_disparity / kp2_multi.size() << std::endl;

    Mat img2_CV;
    cv::cvtColor(img2, img2_CV, cv::COLOR_GRAY2BGR);
    error_disparity = 0;
    for (int i = 0; i < pt2.size(); i++) {
        if (status[i]) {
            cv::circle(img2_CV, pt2[i], 2, cv::Scalar(0, 250, 0), 2);
            cv::line(img2_CV, pt1[i], pt2[i], cv::Scalar(0, 250, 0));

            // compare optical flow result and disparity
            double estimate = (pt2[i] - pt1[i]).x;
            double groundtruth = img3.at<uchar>(pt1[i].y, pt1[i].x);
            error_disparity += (estimate - groundtruth);
        }
    }
    std::cout << "error by opencv: " << error_disparity / kp2_multi.size() << std::endl;


    cv::imshow("tracked single level", img2_single);
    cv::imshow("tracked multi level", img2_multi);
    cv::imshow("tracked by opencv", img2_CV);
    cv::waitKey(0);

    return 0;
}

void OpticalFlowSingleLevel(
    const Mat& img1,
    const Mat& img2,
    const vector<KeyPoint>& kp1,
    vector<KeyPoint>& kp2,
    vector<bool>& success,
    bool inverse
) {

    // parameters
    int half_patch_size = 4;
    int iterations = 10;
    bool have_initial = !kp2.empty();

    for (size_t i = 0; i < kp1.size(); i++) {
        cv::KeyPoint kp = kp1[i];
        double dx = 0;
        if (have_initial) {
            dx = kp2[i].pt.x - kp.pt.x;
        }

        double cost = 0, lastCost = 0;
        bool succ = true; // indicate if this point succeeded

        // Gauss-Newton iterations
        for (int iter = 0; iter < iterations; iter++) {
            double H = 0;
            double b = 0;
            cost = 0;

            if (kp.pt.x + dx <= half_patch_size || kp.pt.x + dx >= img1.cols - half_patch_size ||
                kp.pt.y <= half_patch_size || kp.pt.y >= img1.rows - half_patch_size) {   // go outside
                succ = false;
                break;
            }

            // compute cost and jacobian
            for (int x = -half_patch_size; x < half_patch_size; x++)
                for (int y = -half_patch_size; y < half_patch_size; y++) {

                    // TODO START YOUR CODE HERE (~8 lines)
                    double error = 0;
                    double J;  // Jacobian
                    if (inverse == false) {
                        // Forward Jacobian
                        double x_curr = kp.pt.x + x + dx;
                        double y_curr = kp.pt.y + y;
                        error = GetPixelValue(img1, kp.pt.x + x, kp.pt.y + y) - GetPixelValue(img2, kp.pt.x + x + dx, kp.pt.y + y);
                        J = -0.5 * (GetPixelValue(img2, x_curr + 1, y_curr) - GetPixelValue(img2, x_curr - 1, y_curr));
                    }
                    else {
                        // Inverse Jacobian
                        // NOTE this J does not change when dx, dy is updated, so we can store it and only compute error
                        double x_curr = kp.pt.x + x;
                        double y_curr = kp.pt.y + y;
                        error = GetPixelValue(img2, x_curr, y_curr) - GetPixelValue(img1, x_curr - dx, y_curr);

                        // J[0] = -1 * (GetPixelValue(img1, x_curr + 1, y_curr) - GetPixelValue(img1, x_curr - 1, y_curr)) / 2;
                        // J[1] = -1 * (GetPixelValue(img1, x_curr, y_curr + 1) - GetPixelValue(img1, x_curr, y_curr - 1)) / 2;
                        J = 0.5*(GetPixelValue(img1,kp.pt.x + x + 1 , kp.pt.y + y) - GetPixelValue(img1,kp.pt.x + x - 1 , kp.pt.y + y));
                    }

                    // compute H, b and set cost;
                    H += J * J;
                    b += -J * error;
                    cost += error * error / 2;
                    // TODO END YOUR CODE HERE
                }

            // compute update
            // TODO START YOUR CODE HERE (~1 lines)
            double update;
            update = b / H;
            // TODO END YOUR CODE HERE

            if (isnan(update)) {
                // sometimes occurred when we have a black or white patch and H is irreversible
                cout << "update is nan" << endl;
                succ = false;
                break;
            }
            if (iter > 0 && cost > lastCost) {
                cout << "cost increased: " << cost << ", " << lastCost << endl;
                break;
            }

            // update dx, dy
            dx += update;
            lastCost = cost;
            succ = true;
        }

        success.push_back(succ);

        // set kp2
        if (have_initial) {
            kp2[i].pt = kp.pt + Point2f(dx, 0);
        }
        else {
            KeyPoint tracked = kp;
            tracked.pt += cv::Point2f(dx, 0);
            kp2.push_back(tracked);
        }
    }
}

void OpticalFlowMultiLevel(
    const Mat& img1,
    const Mat& img2,
    const vector<KeyPoint>& kp1,
    vector<KeyPoint>& kp2,
    vector<bool>& success,
    bool inverse) {

    // parameters
    int pyramids = 4;
    double pyramid_scale = 0.5;
    double scales[] = { 1.0, 0.5, 0.25, 0.125 };

    // create pyramids
    vector<Mat> pyr1, pyr2; // image pyramids
    // TODO START YOUR CODE HERE (~8 lines)
    for (int i = 0; i < pyramids; ++i) {
        Mat img1_child, img2_child;
        cv::resize(img1, img1_child, Size(), scales[i], scales[i]);
        cv::resize(img2, img2_child, Size(), scales[i], scales[i]);
        pyr1.push_back(img1_child);
        pyr2.push_back(img2_child);
    }
    // TODO END YOUR CODE HERE

    // coarse-to-fine LK tracking in pyramids
    // TODO START YOUR CODE HERE
    vector<KeyPoint> kp1_scale;
    vector<KeyPoint> kp2_scale;
    for (const auto &kp: kp1) {
        auto kp_temp = kp;
        kp_temp.pt *= scales[pyramids-1];
        kp1_scale.push_back(kp_temp);
    }
    for (int i = pyramids-1; i > -1; --i) {
        success.clear();
        // resize kp1
        OpticalFlowSingleLevel(pyr1[i], pyr2[i], kp1_scale, kp2_scale, success, true);
    
        if (i == 0)
            break;
    
        for (auto &kp : kp1_scale) {
            kp.pt = kp.pt / pyramid_scale;
        }
        for (auto &kp : kp2_scale) {
            kp.pt = kp.pt / pyramid_scale;
        }
    }

    for (auto &kp: kp2_scale) {
        kp2.push_back(kp);
    }
    // TODO END YOUR CODE HERE
    // don't forget to set the results into kp2
}
