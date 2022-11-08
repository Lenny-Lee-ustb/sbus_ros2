/*
* Copyright 2018 Jens Willy Johannsen <jens@jwrobotics.com>, JW Robotics
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
* SBUS serial node
*
* Publishes:
*	/sbus
*/

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

#include "sbus_bridge/sbus_serial_driver.h"
#include "sbus_interface/msg/sbus.hpp"
#include <algorithm>
#include <boost/algorithm/clamp.hpp>

int main( int argc, char **argv )
{
	rclcpp::init(argc, argv);
	auto nh = rclcpp::Node::make_shared("sbus_serial_node");


	ros::init( argc, argv, "sbus_serial_node" );
	ros::NodeHandle nh;
	ros::NodeHandle param_nh( "~" );

	// Read/set parameters
	std::string port;
	int refresh_rate_hr;
	int rxMinValue;
	int rxMaxValue;
	int outMinValue;
	int outMaxValue;
	bool silentOnFailsafe;
	int enableChannelNum;
	double enableChannelProportionalMin;
	double enableChannelProportionalMax;

	port = nh->declare_parameter("port", "/dev/ttyTHS2");
	refresh_rate_hr = nh->declare_parameter("refresh_rate_hz", 5);
	rxMinValue = nh->declare_parameter("rxMinValue", 172);
	rxMaxValue = nh->declare_parameter("rxMaxValue", 1811);
	outMinValue = nh->declare_parameter("outMinValue", 0);
	outMaxValue = nh->declare_parameter("outMaxValue", 255);
	silentOnFailsafe = nh->declare_parameter("silentOnFailsafe", false);
	// Parameters for "enable channel". If channel number is -1, no enable channel is used.
	enableChannelNum = nh->declare_parameter("enableChannelNum", -1);
	enableChannelProportionalMin = nh->declare_parameter("enableChannelProportionalMin", -1.0);
	enableChannelProportionalMax = nh->declare_parameter("enableChannelProportionalMax", -1.0);

	// Used for mapping raw values
	float rawSpan = static_cast<float>(rxMaxValue-rxMinValue);
	float outSpan = static_cast<float>(outMaxValue-outMinValue);

	ros::Publisher pub = nh.advertise<sbus_serial::Sbus>( "sbus", 100 );
	ros::Rate loop_rate( refresh_rate_hr );

	// Initialize SBUS port (using pointer to have only the initialization in the try-catch block)
	sbus_serial::SBusSerialPort *sbusPort;
	try {
		sbusPort = new sbus_serial::SBusSerialPort( port, true );
	}
	catch( ... ) {
		// TODO: add error message in exception and report
		ROS_ERROR( "Unable to initalize SBUS port" );
		return 1;
	}

	// Create Sbus message instance and set invariant properties. Other properties will be set in the callback lambda
	sbus_serial::Sbus sbus;
	sbus.header.stamp = ros::Time( 0 );

	// Callback (auto-capture by reference)
	auto callback = [&]( const sbus_serial::SBusMsg sbusMsg ) {
		// First check if we should be silent on failsafe and failsafe is set. If so, do nothing
		if( silentOnFailsafe && sbusMsg.failsafe )
			return;

		// Next check if we have an "enable channel" specified. If so, return immediately if the value of the specified channel is outside of the specified min/max
		if( enableChannelNum >= 1 && enableChannelNum <= 16 ) {
			double enableChannelProportionalValue = (sbusMsg.channels[ enableChannelNum-1 ] - rxMinValue) / rawSpan;
			if( enableChannelProportionalValue < enableChannelProportionalMin || enableChannelProportionalValue > enableChannelProportionalMax )
				return;
		}

		sbus.header.stamp = ros::Time::now();
		sbus.frame_lost = sbusMsg.frame_lost;
		sbus.failsafe = sbusMsg.failsafe;

		// Assign raw channels
		std::transform( sbusMsg.channels.begin(), sbusMsg.channels.end(), sbus.rawChannels.begin(), [&]( uint16_t rawChannel ) {
			return boost::algorithm::clamp( rawChannel, rxMinValue, rxMaxValue );   // Clamp to min/max raw values
		} );

		// Map to min/max values
		std::transform( sbusMsg.channels.begin(), sbusMsg.channels.end(), sbus.mappedChannels.begin(), [&]( uint16_t rawChannel ) {
			int16_t mappedValue = (rawChannel - rxMinValue) / rawSpan * outSpan + outMinValue;
			return boost::algorithm::clamp( mappedValue, outMinValue, outMaxValue );        // Clamp to min/max output values
		} );
	};
	sbusPort->setCallback( callback );

	ROS_INFO( "SBUS node started..." );

	ros::Time lastPublishedTimestamp( 0 );
	while( ros::ok())
	{
		// Only publish if we have a new sample
		if( lastPublishedTimestamp != sbus.header.stamp ) {
			pub.publish( sbus );
			lastPublishedTimestamp = sbus.header.stamp;
		}

		ros::spinOnce();
		loop_rate.sleep();
	}

	delete sbusPort;        // Cleanup
	return 0;
}
