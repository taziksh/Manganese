#include <opencv2/opencv.hpp>
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

#define PI 3.1415926535897932384626433

//Creates blob detector
cv::Ptr<cv::SimpleBlobDetector> create_blob_detector(){
        cv::SimpleBlobDetector::Params params;
        params.filterByColor = false;
        params.filterByConvexity = false;
        params.filterByCircularity = false;
        params.filterByInertia = false;
        params.filterByArea = true;
        params.minArea = 5000;
        params.maxArea = 65000;
        params.minDistBetweenBlobs = 100;
        
        return cv::SimpleBlobDetector::create(params);
}

cv::Mat create_ROI(cv::Mat img_usm, cv::Rect bbox){
        cv::Mat ROI = img_usm(bbox);
        cv::Mat ROI_resize;
        cv::resize(ROI, ROI_resize, cv::Size(), 400.0/ROI.cols, 400.0/ROI.rows);
        
        ROI.release();
        return ROI_resize;
}

char* run_tesseract(tesseract::TessBaseAPI *api, cv::Mat ROI_resize){
        api->SetImage((uchar*)ROI_resize.data, ROI_resize.size().width, ROI_resize.size().height, ROI_resize.channels(), ROI_resize.step1());
        return api->GetUTF8Text();
}

void process_image(cv::Mat img, std::string img_name, tesseract::TessBaseAPI *api)
{
        cv::Ptr<cv::SimpleBlobDetector> blob_detector = create_blob_detector();
        std::vector<cv::KeyPoint> keypoints;
        blob_detector->detect( img, keypoints );
        unsigned int num_keypoints = keypoints.size();

        cv::Mat img_blurred, img_usm;
        cv::GaussianBlur(img, img_blurred, cv::Size(9, 9), 0, 0, cv::BORDER_DEFAULT);
        img_usm = 4 * img - 2 * img_blurred;
        

        for (int i = 0; i < num_keypoints; i++){
                float x = keypoints[i].pt.x, y = keypoints[i].pt.y;
                float r = keypoints[i].size;
                
                //Corners of min bounding rect
                float left = x - r/2,  right = x + r/2;
                float top = y - r/2,  bottom = y + r/2;
               
                //Checks bounds
                if(left < 0)
                        left = 0;
                else if(right > img.cols)
                        right = img.cols;
                if(top < 0)
                        top = 0;
                else if(bottom > img.rows)
                        bottom = img.rows;
                
                //Creates image matrix for region-of-interest (ROI)
                cv::Rect bbox = cv::Rect(left, top, right - left, bottom - top);
                cv::Mat ROI_resize = create_ROI(img_usm, bbox);

                //Passes ROI to Tesseract object
                char* ocr_text = run_tesseract(api, ROI_resize);

                //Reallocate memory
                //bbox.release();
                ROI_resize.release();
                delete ocr_text;
                ocr_text = nullptr;
        }
        
        /*
        * Code for testing
        * Should not be in final build
        */
        cv::Mat img_with_keypoints;
        drawKeypoints( img, keypoints, img_with_keypoints, cv::Scalar(50,200,50), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS );
        std::cout << img_name << ".jpg" << std::endl;
        cv::imwrite(img_name+"keypoints_.jpg", img_with_keypoints ); 
        //cv::waitKey(0);
        img_with_keypoints.release();

        //Deletes objects
        img.release();
        img_blurred.release();
        img_usm.release();
        blob_detector.release();
        api->Clear();
}

int main(int argc,char** argv)
{
        //Declares Tesseract API object
        tesseract::TessBaseAPI *api = new tesseract::TessBaseAPI();
        api->Init(NULL, "eng");
        //api->Set(tesseract::OEM_TESSERACT_ONLY);
        //DEFAULT:
        //api->SetPageSegMode(tesseract::PSM_SINGLE_BLOCK); 
         
        
        /*Code for iterating through directory
         *In dire need of rewriting
        */
        std::string series_name = "dragon-ball";
        int num_chapters = 197;

        for(int i=196; i<num_chapters; i++){
                std::string chapter="";
                if (i+1<10)
                       chapter+="0";
                if (i+1<100)
                       chapter+="0";
                chapter+=std::to_string(i+1);
                std::string chapter_path = "manga/"+series_name+"/"+series_name+"-"+chapter;
                std::string chapter_name = chapter_path.substr(chapter_path.find_last_of("/")+1);
                
                for(auto& page: std::filesystem::directory_iterator(chapter_path)){
                        std::string path = page.path();
                        std::string file_name = path.substr(path.find_last_of("/")+1);
                        std::string path_root = path.substr(0, path.find(".jpg"));
                        if (path.find_last_of("jpg") == path.size()-1){ 
                                cv::Mat img = cv::imread(path, cv::IMREAD_GRAYSCALE); 
                                //bug - when recursively going through subdir,
                                //matches 'true' for '.jpg. for dir folder itself.
                                process_image(img,path_root, api);
                                }
                }
        }
        //Deletes Tesseract API object
        api->End();
        return 0;
}

