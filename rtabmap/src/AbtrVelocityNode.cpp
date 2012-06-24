/*
 * CameraNode.cpp
 *
 *      Author: labm2414
 */

#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/TwistStamped.h>
#include <utilite/UMutex.h>
#include <queue>
#include <utilite/ULogger.h>

std::queue<float> commandsA;
std::queue<float> commandsB;
std::vector<float> lastCommandsB;
int commandSize = 0;
int commandIndex = 0;
double commandsHz = 10.0;
bool cmdABuffered = false;
bool cmdBBuffered = true;
ros::Publisher rosPublisher;
bool statsLogged = false;
const char * statsFileName = "AbtrStats.txt";

void velocityAReceivedCallback(const geometry_msgs::TwistConstPtr & msg)
{
	commandsA.push(msg->linear.x);
	commandsA.push(msg->linear.y);
	commandsA.push(msg->linear.z);
	commandsA.push(msg->angular.x);
	commandsA.push(msg->angular.y);
	commandsA.push(msg->angular.z);
}

void velocityBReceivedCallback(const geometry_msgs::TwistConstPtr & msg)
{
	commandsB.push(msg->linear.x);
	commandsB.push(msg->linear.y);
	commandsB.push(msg->linear.z);
	commandsB.push(msg->angular.x);
	commandsB.push(msg->angular.y);
	commandsB.push(msg->angular.z);

	if(!lastCommandsB.size())
	{
		lastCommandsB = std::vector<float>(6);
	}
	lastCommandsB[0] = msg->linear.x;
	lastCommandsB[1] = msg->linear.y;
	lastCommandsB[2] = msg->linear.z;
	lastCommandsB[3] = msg->angular.x;
	lastCommandsB[4] = msg->angular.x;
	lastCommandsB[5] = msg->angular.x;
}

int main(int argc, char** argv)
{
	ros::init(argc, argv, "abtr_velocity");
	ros::NodeHandle nh("~");

	nh.param("commands_hz", commandsHz, commandsHz);
	ROS_INFO("commands_hz=%f", commandsHz);

	nh.param("cmd_vel_a_buffered", cmdABuffered, cmdABuffered);
	ROS_INFO("cmd_vel_a_buffered=%d", cmdABuffered);
	nh.param("cmd_vel_b_buffered", cmdBBuffered, cmdBBuffered);
	ROS_INFO("cmd_vel_b_buffered=%d", cmdBBuffered);

	nh.param("stats_logged", statsLogged, statsLogged);
	ROS_INFO("stats_logged=%d", statsLogged);

	if(statsLogged)
	{
		ULogger::setPrintWhere(false);
		ULogger::setBuffered(true);
		ULogger::setPrintLevel(false);
		ULogger::setPrintTime(false);
		ULogger::setType(ULogger::kTypeFile, statsFileName, false);
		ROS_INFO("stats log file = \"%s\"", statsFileName);
	}
	else
	{
		ULogger::setLevel(ULogger::kError);
	}

	nh = ros::NodeHandle();
	ros::Subscriber velATopic;
	ros::Subscriber velBTopic;
	if(cmdABuffered)
	{
		velATopic = nh.subscribe("cmd_vel_a", 0, velocityAReceivedCallback);
	}
	else
	{
		velATopic = nh.subscribe("cmd_vel_a", 1, velocityAReceivedCallback);
	}
	if(cmdBBuffered)
	{
		velBTopic = nh.subscribe("cmd_vel_b", 0, velocityBReceivedCallback);
	}
	else
	{
		velBTopic = nh.subscribe("cmd_vel_b", 1, velocityBReceivedCallback);
	}

	rosPublisher = nh.advertise<geometry_msgs::TwistStamped>("cmd_vel", 1);

	int index = 1;
	ros::Rate loop_rate(commandsHz); // 10 Hz
	while(ros::ok())
	{
		loop_rate.sleep();
		ros::spinOnce();

		geometry_msgs::TwistStampedPtr vel(new geometry_msgs::TwistStamped());
		vel->header.frame_id = "base_link";
		vel->header.stamp = ros::Time::now();

		// priority for commandsA
		if(commandsA.size())
		{
			vel->twist.linear.x = commandsA.front();
			commandsA.pop();
			vel->twist.linear.y = commandsA.front();
			commandsA.pop();
			vel->twist.linear.z = commandsA.front();
			commandsA.pop();
			vel->twist.angular.x = commandsA.front();
			commandsA.pop();
			vel->twist.angular.y = commandsA.front();
			commandsA.pop();
			vel->twist.angular.z = commandsA.front();
			commandsA.pop();
			rosPublisher.publish(vel);
			UINFO("%d A %f %f %f", index, vel->twist.linear.x, vel->twist.linear.y, vel->twist.angular.z);
			if(!cmdABuffered)
			{
				commandsA = std::queue<float>();
			}
			if(commandsB.size())
			{
				commandsB.pop();
				commandsB.pop();
				commandsB.pop();
				commandsB.pop();
				commandsB.pop();
				commandsB.pop();
			}
		}
		else if(commandsB.size())
		{
			vel->twist.linear.x = commandsB.front();
			commandsB.pop();
			vel->twist.linear.y = commandsB.front();
			commandsB.pop();
			vel->twist.linear.z = commandsB.front();
			commandsB.pop();
			vel->twist.angular.x = commandsB.front();
			commandsB.pop();
			vel->twist.angular.y = commandsB.front();
			commandsB.pop();
			vel->twist.angular.z = commandsB.front();
			commandsB.pop();
			UINFO("%d B %f %f %f", index, vel->twist.linear.x, vel->twist.linear.y, vel->twist.angular.z);
			rosPublisher.publish(vel);
			if(!cmdBBuffered)
			{
				commandsB = std::queue<float>();
			}
		}
		else
		{
			UINFO("%d NULL", index);
			if(lastCommandsB.size())
			{
				// Republish the last command one more time
				// (if the sender cannot reach commandsHz for an iteration)
				vel->twist.linear.x = lastCommandsB[0];
				vel->twist.linear.y = lastCommandsB[1];
				vel->twist.linear.z = lastCommandsB[2];
				vel->twist.angular.x = lastCommandsB[3];
				vel->twist.angular.y = lastCommandsB[4];
				vel->twist.angular.z = lastCommandsB[5];
				rosPublisher.publish(vel);
				lastCommandsB = std::vector<float>();
			}
		}
		++index;
	}
	if(statsLogged)
	{
		ULogger::flush();
	}

	return 0;
}
