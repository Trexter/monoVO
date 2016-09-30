/*
 * frame.h
 *
 *  Created on: Sep 20, 2016
 *      Author: kevinsheridan
 */

#ifndef PAUVSI_M7_INCLUDE_PAUVSI_VO_FRAME_H_
#define PAUVSI_M7_INCLUDE_PAUVSI_VO_FRAME_H_

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>
#include "opencv2/core/core.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/xfeatures2d.hpp"
#include "opencv2/video.hpp"
#include <vector>
#include <string>
#include <ros/ros.h>

#include "VIOFeature2D.hpp"



#define DEFAULT_FEATURE_SEARCH_RANGE 5
#define MAXIMUM_ID_NUM 1000000000 //this is the maximum size that a feature ID should be to ensure there are no overflow issues.

class Frame
{

private:
	cv::Ptr<cv::xfeatures2d::BriefDescriptorExtractor> descriptionExtractor;
	bool frameSet;

public:
	ros::Time timeImageCreated;
	cv::Mat image;
	std::vector<VIOFeature2D> features; //the feature vector for this frame
	//this ensures that all features have a unique ID
	int nextFeatureID; // the id of the next feature that is added to this frame

	/*
	 * initilize and set frame
	 * the next feature id will be 0;
	 */
	Frame(cv::Mat img, ros::Time t)
	{
		this->image = img;
		this->timeImageCreated = t;
		descriptionExtractor = cv::xfeatures2d::BriefDescriptorExtractor::create();
		frameSet = true;
		nextFeatureID = 0;
	}

	/*
	 * initilize and set frame
	 * The startingID  should be equal to the lastFrame's last feature's ID
	 * if the startingID is too large it will wrap to 0
	 */
	Frame(cv::Mat img, ros::Time t, int startingID)
	{
		this->image = img;
		this->timeImageCreated = t;
		descriptionExtractor = cv::xfeatures2d::BriefDescriptorExtractor::create();
		frameSet = true;
		if(startingID > MAXIMUM_ID_NUM){
			nextFeatureID = 0;
			ROS_WARN("2D FEATURE ID's ARE OVER FLOWING!");
		}
		else{
			nextFeatureID = startingID;
		}
	}

	/*
	 * initilize frame but don't set it
	 * nextFeatureID will be set to zero
	 */
	Frame()
	{
		descriptionExtractor = cv::xfeatures2d::BriefDescriptorExtractor::create();
		frameSet = false;
		nextFeatureID = 0; // assume that this frame starts at zero featureID
	}

	bool isFrameSet(){
		return frameSet;
	}

	/*
	 * gets the time since this image was captured
	 */
	double getAgeSeconds()
	{
		return ros::Time::now().toSec() - this->timeImageCreated.toSec();
	}

	bool addFeature(cv::KeyPoint _corner){
		//ROS_DEBUG_STREAM("adding feature with ID " << nextFeatureID);
		features.push_back(VIOFeature2D(_corner, nextFeatureID));
		nextFeatureID++; // iterate the nextFeatureID
		return true;
	}

	bool addFeature(VIOFeature2D feat){
		//ROS_DEBUG_STREAM("adding feature with ID " << nextFeatureID);
		//set the feature id
		feat.setFeatureID(nextFeatureID);
		features.push_back(feat); // add it to the features vector
		nextFeatureID++; // iterate the nextFeatureID
	}

	/*
	 * get the fast corners from this image
	 * and add them to the frames features
	 *
	 * This function does not check for identical features
	 */
	bool getFASTCorners(int threshold){
		std::vector<cv::KeyPoint> corners;
		cv::FAST(this->image, corners, threshold, true); // detect with nonmax suppression

		int startingID = features.size(); // the starting ID is the current size of the feature vector

		for(int i=0; i < corners.size(); i++)
		{
			this->addFeature(corners.at(i)); // ensure that id's do not repeat
		}
	}

	/*
	 * gets the a keypoint vector form the feature vector
	 */
	std::vector<cv::KeyPoint> getKeyPointVectorFromFeatures(){
		std::vector<cv::KeyPoint> kp;
		for (int i = 0; i < this->features.size(); i++)
		{
			kp.push_back(this->features.at(i).getFeature());
		}
		return kp;
	}

	/*
	 * gets a point2f vector from the local feature vector
	 */
	std::vector<cv::Point2f> getPoint2fVectorFromFeatures(){
		std::vector<cv::Point2f> points;
		for (int i = 0; i < this->features.size(); i++)
		{
			points.push_back(this->features.at(i).getFeature().pt);
		}
		return points;
	}

	/*
	 * describes an individual feature
	 */
	cv::Mat extractBRIEFDescriptor(VIOFeature2D feature){
		cv::Mat descriptor;
		std::vector<cv::KeyPoint> kp(1); // must be in the form of a vector
		kp.push_back((feature.getFeature()));
		descriptionExtractor->compute(this->image, kp, descriptor);
		return descriptor;
	}

	/*
	 * searches for all features in a the global feature vector that have not been described
	 * and describes them using the BRIEF algorithm
	 */
	bool describeFeaturesWithBRIEF(){
		std::vector<unsigned int> featureIndexes; // the index of the feature keypoints
		std::vector<cv::KeyPoint> featureKPs; // feature keypoints to be described

		// find all features which must be described
		for(int i = 0; i < features.size(); i++)
		{
			if(!features.at(i).isFeatureDescribed())
			{
				featureIndexes.push_back(i);
				featureKPs.push_back(features.at(i).getFeature());
			}
		}

		cv::Mat descriptions;
		descriptionExtractor->compute(this->image, featureKPs, descriptions); //describe all un-described keypoints

		for(int i = 0; i < descriptions.rows; i++)
		{
			cv::Mat desc = descriptions.row(i);
			features.at(featureIndexes.at(i)).setFeatureDescription(desc);
		}

	}

	/*
	 * compares two descriptors
	 * 0 is perfect match
	 * 255 * 32 is the worst possible match
	 * expects two row vectors in cvMats with uchars
	 * assumes that the two description vectors match in size
	 */
	int compareDescriptors(cv::Mat desc1, cv::Mat desc2){
		int error = 0;
		for(int i = 0; i < desc1.cols; i++)
		{
			error += std::abs(desc2.at<uchar>(0, i) - desc1.at<uchar>(0, i));
		}
		return error;
	}

};



#endif /* PAUVSI_M7_INCLUDE_PAUVSI_VO_FRAME_H_ */