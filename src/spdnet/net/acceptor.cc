#include <spdnet/net/acceptor.h>
#include <unordered_map>
#include <chrono>
#include <set> 
#include <cassert>
#include <spdnet/base/socket_api.h>
#include <iostream>
#include <algorithm>
#include <spdnet/net/exception.h>
#include <spdnet/net/event_service.h>

namespace spdnet
{
namespace net
{
		TcpAcceptor::TcpAcceptor(EventService& service)
			:service_(service) , 
			 epoll_fd_(epoll_create(1))
		{

		}

		TcpAcceptor::~TcpAcceptor()
		{
			*run_listen_ = false; 
			if (listen_thread_->joinable())
				listen_thread_->join();
		}

		void TcpAcceptor::start(const std::string& ip , int port , TcpSession::TcpEnterCallback&& cb) 
		{
			ip_ = ip ; 
        	port_ = port ; 

			const int listen_fd = createListenSocket();
			if (listen_fd == -1) {
				 throw SpdnetException(std::string("listen error : ")  + std::to_string(errno));
			}
			
			struct  epoll_event ev ; 
			ev.events = EPOLLIN ; 
			ev.data.ptr = nullptr ; 
			if(epoll_ctl(epoll_fd_ , EPOLL_CTL_ADD , listen_fd , &ev) != 0){
				throw SpdnetException(std::string("epoll_ctl error : ")  + std::to_string(errno));
			}

			auto listen_socket = std::make_shared<ListenSocket>(listen_fd);
        	listen_socket->setNonblock();

			run_listen_  = std::make_shared<bool>(true);
			listen_thread_ = std::make_shared<std::thread>([ listen_socket , enter_callback = std::move(cb), this ]() mutable
				{
					while(*(this->run_listen_))
					{
						struct epoll_event ev ; 
						int ret = epoll_wait(this->epoll_fd_ , &ev , 1 , 1000);
						if(ret == 1 && ev.events & EPOLLIN) {
							auto tcp_socket = listen_socket->accept() ; 
							if(nullptr != tcp_socket) {
								this->service_.addTcpSession(std::move(tcp_socket), enter_callback);
							}
						}

					}
				}
			) ; 
		}


		int TcpAcceptor::createListenSocket()
		{
			base::initSocketEnv();

			struct sockaddr_in addr{0};
			int fd = socket(AF_INET , SOCK_STREAM , 0) ;
			if(fd == -1){
				return  -1 ; 
			}
			
			addr.sin_family = AF_INET ; 
			addr.sin_port = htons(port_) ; 
			addr.sin_addr.s_addr = INADDR_ANY; 
			if(inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) == 0){
				base::closeSocket(fd) ; 
				return  -1;
			}
			int reuse_on = 1 ; 
			if(setsockopt(fd , SOL_SOCKET , SO_REUSEADDR , (const char*)&reuse_on  , sizeof(int)) < 0){
				base::closeSocket(fd) ;
				return  -1;
			}
			
			int ret = bind(fd , (struct sockaddr*)&addr , sizeof(addr)) ;
			if(ret == -1 || listen(fd , 512) == -1){
				base::closeSocket(fd) ;
				return -1 ; 
			}

			return fd ; 

		}
}
}