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
const string DEFAULT_IP_ADDRESS = "127.0.0.1";
const unsigned short DEFAULT_UDP_PORT = 1595;

public:
    FishPipeline();
    ~FishPipeline();
    
    void init(string ip_address_str="", unsigned short udp_port=0);
    void write(cv::Mat &frame);
    void close();

private:
    boost::asio::io_context _ioContext;
    boost::asio::ip::udp::socket _socket;
    boost::asio::ip::udp::endpoint _multicastEndpoint;

    boost::asio::ip::address _ip_address;
    unsigned short _udp_port;

    boost::asio::ip::udp::endpoint _senderEndpoint;
    
    void handleSend(const boost::system::error_code &error, size_t bytesSent, std::shared_ptr<std::vector<uint8_t>> framePtr);
};