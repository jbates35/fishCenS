#include "fishPipe.h"

FishPipeline::FishPipeline() : _socket(_ioContext)
{
}

FishPipeline::~FishPipeline()
{
    if(_socket.is_open())
        close();
}

void FishPipeline::init(string ip_address_str, unsigned short udp_port)
{
    if(ip_address_str == "")
        ip_address_str = DEFAULT_IP_ADDRESS;

    _udp_port = udp_port;
    
    if(udp_port == 0)
        _udp_port = DEFAULT_UDP_PORT;

    cout << "IP address: " << ip_address_str << endl;
    cout << "UDP port: " << _udp_port << endl;


    //set socket options
    _socket.open(boost::asio::ip::udp::v4());
    _socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));

    //Join udp
    _ip_address = boost::asio::ip::address::from_string(ip_address_str);    
    _senderEndpoint = boost::asio::ip::udp::endpoint(_ip_address, _udp_port);
}

void FishPipeline::write(cv::Mat &frame)
{
     if (frame.empty()) {
        throw FishPipelineException("Empty frame");
        return;
    }

    std::vector<uint8_t> buffer;
    std::vector<int> compression_params;
    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression_params.push_back(90);
    cv::imencode(".jpg", frame, buffer, compression_params);

    // Create a shared pointer to hold the frame
    auto framePtr = std::make_shared<std::vector<uint8_t>>(std::move(buffer));

    boost::asio::const_buffer boostBuffer(boost::asio::buffer(*framePtr));

    _socket.async_send_to(boostBuffer, _senderEndpoint,
        [this, framePtr](const boost::system::error_code& error, std::size_t bytes_transferred) {
            handleSend(error, bytes_transferred, framePtr);
        });
}

void FishPipeline::close()
{
    boost::system::error_code error;
    _socket.close(error);
    if (error) {
        std::cout << "Socket close operation failed: " << error.message() << std::endl;
    }
}

void FishPipeline::handleSend(const boost::system::error_code &error, size_t bytesSent, std::shared_ptr<std::vector<uint8_t>> framePtr)
{    
    if (error) {
        // Handle the error
        std::cout << "Send operation failed: " << error.message() << std::endl;
    } else {
        // Handle the successful send
        std::cout << "Sent " << bytesSent << " bytes successfully" << std::endl;
    }    
}