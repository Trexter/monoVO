/*
 * Point.cpp
 *
 *  Created on: Jan 30, 2017
 *      Author: pauvsi
 */


#include "../invio/Point.h"

Point::Point()
{
	deleted = false;

	this->Sigma.setIdentity();

	this->Sigma(0, 0) = DEFAULT_POINT_HOMOGENOUS_VARIANCE;
	this->Sigma(1, 1) = DEFAULT_POINT_HOMOGENOUS_VARIANCE;
	this->Sigma(2, 2) = DEFAULT_POINT_DEPTH_VARIANCE;

	this->theMap = NULL;
	this->immature = true;
	this->guessed = false;
	this->moba_candidate = false;
	this->frames_since_depth_update = 0;
	//this->thisPoint = 0;

}

Point::Point(Feature* ft){
	this->addObservation(ft); // add this observation to the deque
	deleted = false;

	this->Sigma.setIdentity();

	this->Sigma(0, 0) = DEFAULT_POINT_HOMOGENOUS_VARIANCE;
	this->Sigma(1, 1) = DEFAULT_POINT_HOMOGENOUS_VARIANCE;
	this->Sigma(2, 2) = DEFAULT_POINT_DEPTH_VARIANCE;

	this->theMap = NULL;
	this->immature = true;
	this->guessed = false;
	this->moba_candidate = false;
	this->frames_since_depth_update = 0;
	//this->thisPoint = 0;
}

Point::Point(Feature* ft, std::list<Point>::iterator _thisPoint, std::list<Point>* _map)
{
	this->addObservation(ft); // add this observation to the deque
	deleted = false;

	ROS_ASSERT(_map != NULL);

	this->immature = true;
	this->guessed = false;
	this->moba_candidate = false;

	this->Sigma.setIdentity();

	this->Sigma(0, 0) = DEFAULT_POINT_HOMOGENOUS_VARIANCE;
	this->Sigma(1, 1) = DEFAULT_POINT_HOMOGENOUS_VARIANCE;
	this->Sigma(2, 2) = DEFAULT_POINT_DEPTH_VARIANCE;

	this->theMap = _map;
	this->thisPoint = _thisPoint;

	this->frames_since_depth_update = 0;
}

void Point::addObservation(Feature* ft)
{
	/*
	if(observations.size() >= 1)
	{
		ROS_DEBUG_STREAM("this feature frame " << ft->frame << " last frame: " << observations.at(0)->frame);
	}*/

	//set up the initial params if this is the first observation
	if(_observations.size() == 0)
	{
		this->initial_camera_pose = ft->getParentFrame()->getPose();
		this->initial_camera_pose_inv = this->initial_camera_pose.inverse();
		this->last_update_pose = this->initial_camera_pose; // set last update pose

		this->mu.head(2) = ft->getMetricPixel(); // set the first two values of mu

	}

	_observations.push_front(ft);

	/*
	if(observations.size() > 1)
	{
		ROS_DEBUG_STREAM("*most recent obs frame: " << observations.front()->frame << " last obs frame: " << observations.back()->frame);
	}*/
}


void Point::safelyDeletePoint()
{
	ROS_DEBUG_STREAM("deleting point with " << this->observations().size() << " obs");
	//ROS_ASSERT(this->observations().size() <= 100);


	/*for(auto& e : this->observations())
	{
		ROS_DEBUG_STREAM("check feature (" << e << ") pos for deleted memory" << e->px);
		e->setPoint(NULL); // null the point reference
		e->obsolete = true;


		if(e->frame->finalFrame) // this is the last frame in the buffer do not search past this point due to potential dangling pointer
		{
			ROS_DEBUG("breaking out of point deletion dereferencer because the final frame has been passed");
			break;
		}
	}*/

	ROS_DEBUG_STREAM("point deleting itself");

	deleted = true;

	//peace out delete my self
	// we had a good run
	this->getMap()->erase(this->getPointIterator());

	ROS_DEBUG("point deleted");
}


/*
 * bayesian update step for the depth of the first observed feature
 */
void Point::update(Eigen::Vector3d measurement, Eigen::Vector3d sigmas)
{
	ROS_ASSERT(sigmas(0)*sigmas(1)*sigmas(2) != 0.0);

	//u
	double K_0 = this->Sigma(0, 0) / (this->Sigma(0, 0) + sigmas(0));
	this->mu(0) = this->mu(0) + K_0*(measurement(0) - this->mu(0));
	this->Sigma(0, 0) = (1 - K_0)*this->Sigma(0, 0);

	//v
	double K_1 = this->Sigma(1, 1) / (this->Sigma(1, 1) + sigmas(1));
	this->mu(1) = this->mu(1) + K_1*(measurement(1) - this->mu(1));
	this->Sigma(1, 1) = (1 - K_1)*this->Sigma(1, 1);

	//d
	double K_2 = this->Sigma(2, 2) / (this->Sigma(2, 2) + sigmas(2));
	this->mu(2) = this->getDepth() + K_2*(measurement(2) - this->getDepth());
	this->Sigma(2, 2) = (1 - K_2)*this->Sigma(2, 2);



	if(guessed)
	{
		// set the min/max to the current point
		this->min_depth = this->max_depth = measurement(2);
		this->guessed = false; // we got a measurement so it is nolonger a guessed point
	}
	else
	{
		if(measurement(2) > max_depth)
			max_depth = measurement(2);

		if(measurement(2) < min_depth)
			min_depth = measurement(2);
	}
}


/*
 * adapted from SVO, Forster et al
 * this performs structure only bundle adjustment on this point using all keyframes still in the buffer
 *
 * This function uses the gauss newton optimization method to find the optimal 3d point for a set of keyframes
 *
 * searches for best keyframe(s) to determine the depth of the point and update it
 */
/*bool Point::SBA(int iterations)
{
	Eigen::Vector3d new_point = this->pos; // this will the optimum point for each iteration
	Eigen::Vector3d old_point = this->pos;
	Eigen::Vector3d original = this->pos;
	double chi2 = 0.0;
	Eigen::Matrix3d A;
	Eigen::Vector3d b;

	bool failed = false;
	int failIter = 0;

	//this bock assumes that there are no observations which point to an out of buffer frame
	std::vector<Feature*> vertices;

	for(auto e : this->_observations)
	{
		if(e->getParentFrame()->isKeyframe())
		{
			vertices.push_back(e);
			ROS_DEBUG_STREAM("added a keyframe to the SBA problem");
			if(vertices.size() >= MAXIMUM_KEYFRAME_COUNT_FOR_OPTIMIZATION)
			{
				ROS_DEBUG_STREAM("got enough keyframes to attempt optimization");
				break;
			}
		}
		if(e->frame->finalFrame) // this is the last frame in the buffer do not search past this point due to potential dangling pointer
		{
			ROS_DEBUG("breaking out of keyframe search because the final frame has been passed");
			break;
		}
	}

	if(vertices.size() < MINIMUM_KEYFRAME_COUNT_FOR_OPTIMIZATION){
		ROS_DEBUG_STREAM("NOT ENOUGH KEYFRAMES FOR SBA PROBLEM TO SOLVE");
		return false;
	}

	ROS_DEBUG_STREAM("point before: " << this->pos);


	for(size_t i = 0; i < iterations; i++)
	{
		A.setZero();
		b.setZero();
		double new_chi2 = 0.0;

		// compute residuals
		for(auto it = vertices.begin(); it != vertices.end(); ++it)
		{
			Matrix23d J;
			const Eigen::Vector3d p_in_f((*it)->getParentFrame()->getPose_inv() * new_point); // the 3d point projected into this keyframe's coordinate system

			Point::jacobian_xyz2uv(p_in_f, (*it)->getParentFrame()->getPose_inv().rotationMatrix(), J); // this gets the jacobian of the projection function

			const Eigen::Vector2d e((*it)->getMetricPixel() - Point::toMetricPixel(p_in_f)); // this is the metric pixel error of this point

			new_chi2 += e.squaredNorm(); // add the pixel error to chi

#if SUPER_DEBUG
			ROS_DEBUG_STREAM("error at vertex " << e.squaredNorm());
#endif

			A.noalias() += J.transpose() * J; //add this observation to this iteration's problem
			b.noalias() -= J.transpose() * e;
		}


		// solve linear system
		const Eigen::Vector3d dp(A.ldlt().solve(b));

		// check if error increased
		if((i > 0 && new_chi2 > chi2) || (bool) std::isnan((double)dp[0]))
		{
			failed = true;
			failIter = i;
			ROS_DEBUG_STREAM("it " << i << " FAILURE  new_chi2 = " << new_chi2);

			new_point = old_point; // roll-back

			break;
		}

		// update the model
		new_point = this->pos + dp;
		old_point = new_point;
		chi2 = new_chi2;

		//ROS_DEBUG_STREAM("point between " << new_point);

		ROS_DEBUG_STREAM("it " << i << " Success  new_chi2 = " << new_chi2 << " norm(b) = " << b.norm());

		// stop when converged
		if(dp.norm() <= EPS_SBA)
			break;
	}

	//compute the depth to the current frame
	double depth = (vertices.back()->getParentFrame()->getPose_inv() * new_point).z();

	//compute the variance
	double measured_variance = (chi2 / vertices.size());

	if(!(failed && failIter < 2) && measured_variance < MAXIMUM_FEATURE_DEPTH_VARIANCE && depth > MIN_POINT_Z && depth < MAX_POINT_Z && chi2 != 0.0)
	{
		ROS_DEBUG_STREAM("GOOD POINT DEPTH: "  << depth);

		//update the points position and variance
		this->updatePoint(new_point);

		// set this point to mature
		this->setImmature(false);

		//important set guessed false
		this->guessed = false;


		ROS_DEBUG_STREAM("new variance: " << this->variance);
	}
	else
	{
		ROS_WARN_STREAM("DELETING! Point failed to converge. depth: " << depth << " and variance: " << measured_variance);

		// due to a !null assertion I must flag the front feature as obsolete so that this point is deleted in the future instead of now
		this->observations().front()->obsolete = true; // now this point will be deleted later

	}


	ROS_DEBUG_STREAM("point after: " << this->pos);

	return true;

}*/



