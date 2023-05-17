#include "fishPipe.h"

FishPipeline::FishPipeline() : _socket(_ioContext)
{
}

FishPipeline::~FishPipeline()
{
    if(_socket.is_open())
        _socket.close();
}

void FishPipeline::init(string ip_address_str, string multicast_address_str, unsigned short multicast_port)
{
    if(ip_address_str == "")
        ip_address_str = DEFAULT_IP_ADDRESS;

    if(multicast_address_str == "")
        multicast_address_str = DEFAULT_MULTICAST_ADDRESS;

    _multicast_port = multicast_port;
    
    if(multicast_port == 0)
        _multicast_port = DEFAULT_MULTICAST_PORT;

    //initialize socket
    _socket = boost::asio::ip::udp::socket(_ioContext);

    //set socket options
    _socket.open(boost::asio::ip::udp::v4());
    _socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    _socket.set_option(boost::asio::ip::multicast::enable_loopback(true));

    //Join multicast group
    _multicast_address = boost::asio::ip::address::from_string(multicast_address_str);
    _ip_address = boost::asio::ip::address::from_string(ip_address_str);    
    _socket.set_option(boost::asio::ip::multicast::join_group(_multicast_address));

    if(!_socket.is_open())
        throw FishPipelineException("Socket not open");
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

    boost::asio::const_buffer boostBuffer(boost::asio::buffer(buffer));

    _socket.async_send_to(boostBuffer, boost::asio::ip::udp::endpoint(_multicast_address, _multicast_port),
        [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
            handleSend(error, bytes_transferred);
        });
}

void FishPipeline::close()
{
    _socket.close();
}

void FishPipeline::handleSend(const boost::system::error_code &error, size_t bytesSent)
{    
    if (error) {
        // Handle the error
        std::cout << "Send operation failed: " << error.message() << std::endl;
    } else {
        // Handle the successful send
        std::cout << "Sent " << bytesSent << " bytes successfully" << std::endl;
    }    
}