#ifndef _SSLNETIOINTERFACE_H
#define _SSLNETIOINTERFACE_H
#include <OS/OpenSpy.h>
#include <OS/Net/IOIfaces/BSDNetIOInterface.h>
#include <vector>

#include <OS/SSL.h>

namespace SSLNetIOIFace {
	class SSLNetIOInterface;
    class SSL_Socket : public INetIOSocket {
		friend class SSLNetIOInterface;
        public:
            SSL_Socket(OS::ESSL_Type ssl_version = OS::ESSL_None);
			~SSL_Socket();
        protected:
            OS::ESSL_Type m_type;
            bool m_ssl_handshake_complete;
            int m_ssl_handshake_attempts;
    };
    class SSLNetIOInterface : public BSDNetIOInterface<SSL_Socket> {
		friend class SSL_Socket;
        public:
            SSLNetIOInterface(OS::ESSL_Type type, std::string privateKey_raw, std::string cert_raw);
            ~SSLNetIOInterface();

            NetIOCommResp streamRecv(INetIOSocket *socket, OS::Buffer &buffer);
            NetIOCommResp streamSend(INetIOSocket *socket, OS::Buffer &buffer);

			std::vector<INetIOSocket *> TCPAccept(INetIOSocket *socket);
        protected:
            bool try_ssl_accept(SSL_Socket *socket);
            OS::ESSL_Type m_ssl_version;
    };
}
#endif //_SSLNETIOINTERFACE_H