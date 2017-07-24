#include "path.h"
#include "classifier.h"
#include "Eigen-3.3/Eigen/Dense"
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

path::path() {}
path::~path() {}


Vehicle *r_daneel_olivaw = new Vehicle;  // our self driving car
GNB *gnb = new GNB;

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;

void path::init() {
	
	/****************************************
	* 1. Prediction - classifier
	****************************************/
	vector< vector<double> > X_train = gnb->load_state("./train_states.txt");
	vector< string > Y_train = gnb->load_label("./train_labels.txt");
	gnb->train(X_train, Y_train);
	
	

}

void path::sensor_fusion_predict(vector< vector<double>> sensor_fusion) {
/****************************************
*  Turn raw sensor_fusion observations
 into predictions for use by update_state() and trajectory_generation() 
****************************************/

	// 1. Update vehicles list  (create new objects?)
	for (size_t i = 0; i < sensor_fusion.size(); ++i) {
		
		int id = sensor_fusion[i][0];
		if (other_vehicles.find(id) == other_vehicles.end()) {
			
			other_vehicles.insert(make_pair(id, new Vehicle));

		}

		// is there better way to do this?
		Vehicle *vehicle = other_vehicles.find(id)->second;

		// 2. Update sensor readings

		// previous
		vehicle->sf_x_p = vehicle->sf_x;
		vehicle->sf_y_p = vehicle->sf_y;
		vehicle->sf_vx_p = vehicle->sf_vx;
		vehicle->sf_vy_p = vehicle->sf_vy_p;
		vehicle->sf_s_p = vehicle->sf_d;
		vehicle->sf_d_p = vehicle->sf_d;

		// new
		vehicle->sf_x	= sensor_fusion[i][1];
		vehicle->sf_y	= sensor_fusion[i][2];
		vehicle->sf_vx	= sensor_fusion[i][3];
		vehicle->sf_vy	= sensor_fusion[i][4];
		vehicle->sf_s	= sensor_fusion[i][5];
		vehicle->sf_d	= sensor_fusion[i][6];

		double d_dot = vehicle->sf_d_p - vehicle->sf_d;   // is this right?
		
		// run  classifier.predict() to see if changing lanes?
		vehicle->predicted_state = gnb->predict(d_dot);	
	
	}
}


void path::update_state() {
/****************************************
* Behavior planning usine finite state machine
****************************************/
}



void path::trajectory_generation() {
/****************************************
* find best trajectory according to weighted cost function
****************************************/

	r_daneel_olivaw->S = { 0, 0, 0 }; // TODO dynamic get this
	r_daneel_olivaw->D = { 0, 0, 0 };

	// 1. Generate random nearby goals

	// 2. Store jerk minimal trajectories for all goals

	// 3. Find best using weighted cost function

}


/****************************************
* Cost functions
****************************************/

double path::collision_cost(vector< vector<double> > trajectory) {

	
	double a = nearest_approach_to_any_vehicle(trajectory);
	double b = 2 * r_daneel_olivaw->radius;
	if (a < b) { return 1.0;	 }
	else { return 0.0; }
}

double path::nearest_approach_to_any_vehicle(vector< vector<double> > trajectory) {
// returns closest distance to any vehicle

	double a = 1e9;
	for (auto& v : other_vehicles) {
		double b = nearest_approach(trajectory, v);
		if (a < b) { a = b; }
	}
	return a;

}

double path::nearest_approach(vector< vector<double> > trajectory, Vehicle *vehicle) {
	
	double T, s_time, d_time, target_s, target_d, a, b, c, e, t;
	a = 1e9;
	T = trajectory[0][0];

	vector<double> S, D;
	S, D = trajectory[0], trajectory[1];

	for (size_t i = 0; i < 100; ++i) {
		t = double(i) / 100 * T;
		s_time = coefficients_to_time_function(D, t);
		d_time = coefficients_to_time_function(S, t);

		vehicle->update_target_state(t);

		b = pow((s_time - vehicle->s_target), 2);
		c = pow((d_time - vehicle->d_target), 2);
		e = sqrt(b + c);

		if (e < a) { a = e; }
	}
	return a;
}


double path::coefficients_to_time_function(vector<double> coefficients, double t) {
	// Returns a function of time given coefficients
	double total = 0.0;
	for (size_t i = 0; i < coefficients.size(); ++i) {
		total += coefficients[i] * pow(t, i);
	}
	return total;
}


vector<double> path::jerk_minimal_trajectory(vector< double> start, vector <double> end, double T)
{
	/*
	Calculate the Jerk Minimizing Trajectory that connects the initial state
	to the final state in time T.

	INPUTS

	start - the vehicles start location given as a length three array
	corresponding to initial values of [s, s_dot, s_double_dot]

	end   - the desired end state for vehicle. Like "start" this is a
	length three array.

	T     - The duration, in seconds, over which this maneuver should occur.

	OUTPUT
	an array of length 6, each value corresponding to a coefficent in the polynomial
	s(t) = a_0 + a_1 * t + a_2 * t**2 + a_3 * t**3 + a_4 * t**4 + a_5 * t**5

	EXAMPLE

	> JMT( [0, 10, 0], [10, 10, 0], 1)
	[0.0, 10.0, 0.0, 0.0, 0.0, 0.0]
	*/

	const double s_i = start[0];
	const double s_i_dot = start[1];
	const double s_i_dot_dot = start[2] / 2;

	const double s_f = end[0];
	const double s_f_dot = end[1];
	const double s_f_dot_dot = end[2];

	const double s_r_1 = s_f - (s_i + s_i_dot * T + (s_i_dot_dot * T * T) / 2);
	const double s_r_2 = s_f_dot - (s_i_dot + s_i_dot_dot * T);
	const double s_r_3 = s_f_dot_dot - s_i_dot_dot;

	VectorXd t_1(3), t_2(3), t_3(3);
	t_1 << pow(T, 3), pow(T, 4), pow(T, 5);
	t_2 << 3 * pow(T, 2), 4 * pow(T, 3), 5 * pow(T, 4);
	t_3 << 6 * pow(T, 1), 12 * pow(T, 2), 20 * pow(T, 3);

	MatrixXd s_matrix(3, 1), T_matrix(3, 3);
	s_matrix << s_r_1, s_r_2, s_r_3;
	T_matrix.row(0) = t_1;
	T_matrix.row(1) = t_2;
	T_matrix.row(2) = t_3;

	MatrixXd T_inverse = T_matrix.inverse();

	MatrixXd output(3, 1);
	output = T_inverse * s_matrix;

	double a_3 = double(output.data()[0]);
	double a_4 = double(output.data()[1]);
	double a_5 = double(output.data()[2]);

	vector<double> results;
	results = { s_i, s_i_dot, s_i_dot_dot, a_3, a_4, a_5 };

	cout << output << endl;

	return results;

}
