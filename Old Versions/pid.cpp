//All Speeds are rotations per second
double pidVelocity(double targetVelocity, double prevVelocity, double &sumError){
	double error = targetVelocity - currentVelocity;
	sumError += error;
	double prevError = targetVelocity - prevVelocity;

	double kp = 10;
	double ki = 0;
	double kd = 0;


	double proportional = kp * error;
	double intergral = ki * sumError;
	double differential = kd * (error - prevError);

	return proportional + integral + differential;
}

double pidPosition(double targetPosition, double prevPosition, double &sumError){
	double error = targetPosition - currentPosition;
	sumError += error;
	double prevError = targetPosition - prevPosition;

	double kp = 10;
	double ki = 0;
	double kd = 0;


	double proportional = kp * error;
	double intergral = ki * sumError;
	double differential = kd * (error - prevError);

	return proportional + integral + differential;
}