#pragma once 

#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include <opencv2/opencv.hpp>

using namespace std;

class FishPipelineException : public std::exception
{
public:
    FishPipelineException(const std::string &message) : _message(message) {}
    virtual const char *what() const noexcept override {
        return _message.c_str();
    }
private:
    std::string _message;
};

class FishPipeline
{
const string DEFAULT_IP_ADDRESS = "192.168.0.235";
const string DEFAULT_MULTICAST_ADDRESS = "239.255.0.1";
const unsigned short DEFAULT_MULTICAST_PORT = 1234;

public:
    FishPipeline();
    ~FishPipeline();
    
    void init(string ip_address_str="", string multicast_address_str="", unsigned short multicast_port=0);
    void write(cv::Mat &frame);
    void close();

private:
    boost::asio::io_context _ioContext;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _multicastEndpoint;

    boost::asio::ip::address _multicast_address;
    boost::asio::ip::address _ip_address;
    unsigned short _multicast_port;
    
    void handleSend(const boost::system::error_code &error, size_t bytesSent);
};